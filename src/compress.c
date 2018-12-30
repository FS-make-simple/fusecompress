/** @compress.c
 *
 * Copyright (C) 2005 Milan Svoboda <milan.svoboda@centrum.cz>
 * Copyright (C) 2006 Milan Svoboda <milan.svoboda@centrum.cz>
 * Copyright (C) 2011 Ulrich Hecht <uli@suse.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <assert.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>
#include <stdio.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "structs.h"
#include "globals.h"
#include "log.h"
#include "file.h"
#include "direct_compress.h"
#include "dedup.h"

int compress_testcancel(void *cancel_cookie)
{
	int     r = FALSE;
	file_t *file;

	assert(cancel_cookie);

	file = (file_t *) cancel_cookie;

	LOCK(&file->lock);
	if (file->status & CANCEL)
		r = TRUE;
	UNLOCK(&file->lock);

	return r;
}

compressor_t *choose_compressor(const file_t *file)
{
	/* don't compress already compressed file formats */
	if (!is_compressible(file->filename))
	        return NULL;

	/* ignore our temporary files */
	if (strstr(file->filename, TEMP))
		return NULL;

	/* ignore FUSE temporary files */
	if (strstr(file->filename, FUSE))
		return NULL;

        /* ignore explicitly excluded paths */
        if (is_excluded(file->filename))
                return NULL;

	/* ignore binaries and shared objects when mounted at / or /usr */
	if (root_fs)
	{
	        char **ext;
		for (ext = mmapped_dirs; *ext != NULL; ext++)
		{
			DEBUG_("file->filename %s, ext %s", file->filename, *ext);
			if (strncmp(file->filename, *ext, strlen(*ext)) == 0)
				return NULL;
		}
		char *r;
		if ((r = rindex(file->filename, '.')))
		{
			if (!strcmp(r, ".so")) /* name ends with .so */
				return NULL;
			if (strstr(file->filename, ".so.")) /* name contains .so. */
				return NULL;
		}
	}
	/* TODO: decide about compressor and it's compression level from size */
	return compressor_default;
}


/**
 * Decompress file.
 *
 * @param file Specifies file which will be decompressed.
 */
int do_decompress(file_t *file)
{
	descriptor_t *descriptor = NULL;
	compressor_t *header_compressor = NULL;
	int res;
	int fd_temp;
	int fd_source;
	char *temp;
	off_t size;
	off_t header_size;
	struct stat stbuf;
	struct utimbuf buf;

	assert(file);

	NEED_LOCK(&file->lock);

	DEBUG_("('%s')", file->filename);
	STAT_(STAT_DECOMPRESS);

	DEBUG_("file size %zd",file->size);
	struct statvfs stat;
	if(statvfs(file->filename, &stat) < 0)
		return FALSE;
	if(stat.f_bsize * stat.f_bavail < file->size) {
		if(!(geteuid() == 0 && stat.f_bsize * stat.f_bfree >= file->size)) {
			errno = ENOSPC;
			return FALSE;
		}
	}

	flush_file_cache(file);
	
	// Open file
	//
	fd_source = file_open(file->filename, O_RDWR);
	if (fd_source == FAIL)
	{
		CRIT_("open failed on '%s'", file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
	}

	// Try to read header.
	//
	res = file_read_header_fd(fd_source, &header_compressor, &header_size);
	if (res < 0) {
		CRIT_("I/O error reading header on '%s': %s", file->filename, strerror(errno));
		return FALSE;
	}
	if (!header_compressor) {
		/* This is not a compressed file (most likely it's empty)
		   we only have to reset the compressor */
		file->compressor = NULL;
		file->size = -1; /* is this safe? */
		file_close(&fd_source);
		return TRUE;
	}

	// Set compressor (it'll be unset if we're called from
	// truncate for example)
	//
	if (!file->compressor)
	{
		file->compressor = header_compressor;
	}
	assert(file->compressor == header_compressor);

	// close compressor data
	//
	list_for_each_entry(descriptor, &file->head, list)
	{
		direct_close(file, descriptor);
		DEBUG_("file_closing %s (fd %d)",file->filename,descriptor->fd);
		file_close(&descriptor->fd);
	}
	
	// pull fstat info after compressor data is flushed
	//
	res = fstat(fd_source, &stbuf);
	if (res == -1)
	{
		file_close(&fd_source);
		WARN_("fstat failed on '%s'", file->filename);
		return FALSE;
	}

	// Create temp file
	//
	temp = file_create_temp(&fd_temp);
	if (fd_temp == -1)
	{
		CRIT_("can't create tempfile for '%s'", file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
	}
	
	// do actual decompression of the file
	//

	file->status |= DECOMPRESSING;

	/* We cannot release file->lock here: we have closed
	   all file descriptors on a file that is in use! Any
	   accesses would bomb horribly. */
	size = file->compressor->decompress(fd_source, fd_temp);

	file->status &= ~DECOMPRESSING;

	if (file->status & CANCEL)
	{
		file->status &= ~CANCEL;

		pthread_cond_broadcast(&file->cond);
	}

	if ((size == (off_t) FAIL) || (size != header_size))
	{
		// Clean up temporary file
		//
		file_close(&fd_temp);
		unlink(temp);
		CRIT_("decompression of '%s' has failed!", file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
	}

	// reopen all fd's (BEFORE fchmod, so we dont get any
	// permission denied problems)
	//
	list_for_each_entry(descriptor, &file->head, list) {
		descriptor->fd = open(temp, O_RDWR);
	}

	// Chmod file, rename file and close fd to tempfile and source
	//
	res = fchown(fd_temp, stbuf.st_uid, stbuf.st_gid);
	res = fchmod(fd_temp, stbuf.st_mode);
	if (res == -1)
	{
#if 0	// some backing filesystems (vfat, for instance) do not support permissions
		CRIT_("fchmod failed on '%s'!", file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
#endif
	}

	// Rename tmpfile to real file
	//
	res = rename(temp, file->filename);
	if (res == -1)
	{
		CRIT_("Rename failed on '%s' -> '%s'!", temp, file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
	}
	free(temp);

	// Close source file
	//
	file_close(&fd_source);
	
	// Close temp file (now main file)
	//
	file_close(&fd_temp);

	// access and modification time can be only changed
	// after the descriptor is closed 
	//
	buf.actime = stbuf.st_atime;
	buf.modtime = stbuf.st_mtime;
	res = utime(file->filename, &buf);
	if (res == -1)
	{
		CRIT_("utime failed on '%s'!", file->filename);
		//exit(EXIT_FAILURE);
		return FALSE;
	}

	file->compressor = NULL;
	file->size = (off_t) size;

	return TRUE;
}

/**
 * Compress file.
 *
 * @param file Specifies file which will be compressed
 */
void do_compress(file_t *file)
{
	int fd      = FAIL;
	int fd_temp = FAIL;
	int res;
	char *temp = NULL;
	off_t filesize;
	compressor_t *compressor;
	struct stat statbuf;
	struct utimbuf timebuf;

	NEED_LOCK(&file->lock);

	DEBUG_("('%s')", file->filename);
	STAT_(STAT_COMPRESS);

	fd = file_open(file->filename, O_RDWR);
	if (fd == FAIL) {
		FILEERR_(file, "\tfailed to open file %s: %d",file->filename,errno);

		// Something wrong happend, mark file as deleted to break the
		// loop in _direct_open_purge when this function is called by it.
		//
		direct_delete(file);
		goto out;
	}

	res = fstat(fd, &statbuf);
	if (res == FAIL) {
		FILEERR_(file, "\tfailed to fstat file");

		// Something wrong happend, mark file as delete to break the
		// loop in _direct_open_purge when this function is called by it.
		//
		direct_delete(file);
		goto out;
	}

	// Check if file is too small to be compressed.
	//
	if (statbuf.st_size < min_filesize_background)
		goto out;

	// This could be compressed file if this is a result of direct_rename.
	//
	if (statbuf.st_size >= sizeof(header_t))
	{
		res = file_read_header_fd(fd, &file->compressor, &file->size);
		if ((res == 0) && (file->compressor))
		{
			// This file is compressed.
			// Compressor was updated by file_read_header_fd, this prevents
			// additon of this file to the background compress queue again.
			//
			goto out;
		}

		// This is not compressed file.
		//
		lseek(fd, SEEK_SET, 0);
	}

	// Choose compressor
	//
	compressor = choose_compressor(file);
	if (!compressor)
		goto out;

	// Create temp file
	//
	temp = file_create_temp(&fd_temp);
	if (fd_temp == FAIL) {
		CRIT_("\tcan't create tempfile");
		//exit(EXIT_FAILURE);
		return;
	}
	DEBUG_("\tusing tempfile %s", temp);

	// Write header
	//
	res = file_write_header(fd_temp, compressor, statbuf.st_size);
	if (res == FAIL) {
		FILEERR_(file, "\tfailed to write header");
		goto out;
	}

	// Do actual compression. This may take a long time...
	//

	// Mark file as beeing compressing. This allows us to unlock the lock.
	//
	file->status |= COMPRESSING;

	/* Unlock to allow interruption. This is safe because do_compress()
	   is only called by the background compression thread, which
	   only runs on files that are not in use. */
	UNLOCK(&file->lock);
	filesize = compressor->compress(file, fd, fd_temp);
	LOCK(&file->lock);

	file->status &= ~COMPRESSING;

	if ((filesize     == (off_t) FAIL) ||
	    (filesize     != statbuf.st_size) ||
	    (file->status &  CANCEL))
	{
		DEBUG_("\tfile->compressor->compress(file, fd, fd_temp) failed");
		DEBUG_("\tfilesize: %zi, file->size: %zi, file->status & CANCEL: %d",
				filesize, file->size, (file->status & CANCEL));

		if (file->status & CANCEL)
		{
			file->status &= ~CANCEL;

			pthread_cond_broadcast(&file->cond);
		}
		else
		{
			WARN_("\tfailed to compress file");
		}
		goto out;
	}

	res = fchown(fd_temp, statbuf.st_uid, statbuf.st_gid);
	if (res == FAIL) {
#if 0 // some backing filesystems (vfat, for instance) do not support users
		ERR_("\tfailed to chown tempfile");
		goto out;
#endif
	}

	res = fchmod(fd_temp, statbuf.st_mode);
	if (res == FAIL) {
#if 0 // some backing filesystems (vfat, for instance) do not support permissions
		ERR_("\tfailed to chmod tempfile");
		goto out;
#endif
	}

	res = close(fd_temp);
	if (res == FAIL) {
		CRIT_("\tclose failed on tempfile");
		//exit(EXIT_FAILURE);
		return;
	}
	fd_temp = FAIL;

	res = close(fd);
	if (res == FAIL) {
		CRIT_("\tclose failed");
		//exit(EXIT_FAILURE);
		return;
	}
	fd = FAIL;

	res = rename(temp, file->filename);
	if (res == FAIL) {
		FILEERR_(file, "\tfailed to rename %s to %s",temp,file->filename);
		goto out;
	}
	free(temp);
	temp = NULL;

	// File is compressed - update data in file
	//
	file->compressor = compressor;
	file->size = filesize;

#ifdef WITH_DEDUP
	/* no longer present in uncompressed form, so we need to make
	   sure it is removed from the deduplication database */
	if (dedup_enabled)
		dedup_discard(file);
#endif
	
	// Access and modification time can be only changed
	// after the descriptor is closed.
	//
	timebuf.actime = statbuf.st_atime;
	timebuf.modtime = statbuf.st_mtime;
	res = utime(file->filename, &timebuf);
	if (res == FAIL) {
		WARN_("\tutime failed!");
		goto out;
	}
out:
	if (fd != FAIL) {
		res = close(fd);
		if (res == FAIL) {
			CRIT_("\tclose failed");
			//exit(EXIT_FAILURE);
			return;
		}
		fd = FAIL;
	}

	if (fd_temp != FAIL) {
		res = close(fd_temp);
		if (res == FAIL) {
			CRIT_("\tclose failed on tempfile");
			//exit(EXIT_FAILURE);
			return;
		}
		fd_temp = FAIL;
	}

	if (temp) {
		res = unlink(temp);
		if (res == FAIL) {
			CRIT_("\tunlink failed on tempfile");
			//exit(EXIT_FAILURE);
			return;
		}
		free(temp);
		temp = NULL;
	}
}
