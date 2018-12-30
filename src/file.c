/*
*  C Implementation: file
*
* Description: 
*
*
* Author: Milan Svoboda <milan.svoboda@centrum.cz>, (C) 2005
* (C) 2009, 2011 Ulrich Hecht <uli@suse.de>
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include "direct_compress.h"
#include "globals.h"
#include "log.h"
#include "structs.h"
#include "file.h"
#include "utils.h"

compressor_t *find_compressor(const header_t *fh)
{
	if (fh->type > sizeof(compressors) / sizeof(compressors[0]))
		return NULL;

	return compressors[fh->type];
}

compressor_t *find_compressor_name(const char *name)
{
	int i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); i++)
		if (compressors[i] && !strcmp(name, compressors[i]->name))
			return compressors[i];
	return NULL;
}

compressor_t *file_compressor(const header_t *fh)
{
	compressor_t *compressor;

	if ((fh->id[0] != '\037') || (fh->id[1] != '\135') || (fh->id[2] != '\211'))
		return NULL;
	
	compressor = find_compressor(fh);
	if (!compressor)
		return NULL;

	return compressor;
}

int file_write_header(int fd, compressor_t *compressor, off_t size)
{
	header_t fh;

	assert(fd >= 0);
	assert(compressor);
	assert(size != (off_t) -1);

	fh.id[0] = '\037';
	fh.id[1] = '\135';
	fh.id[2] = '\211';
	fh.type = compressor->type;
	fh.size = to_le64(size);

	DEBUG_("writing header to %d at %zd\n", fd, lseek(fd, 0, SEEK_CUR));
	return write(fd, &fh, sizeof(fh));
}

/**
 * Returns type of compression and real size of the file if file is compressed.
 * If the header is invalid, the size will not be touched, and the compressor
 * will be set to NULL.
 *
 * @param fd
 * @param **compressor
 * @param *size
 *
 * @return  0 - compressor and size assigned or invalid header
 *         -1 - file i/o error
 */
int file_read_header_fd(int fd, compressor_t **compressor, off_t *size)
{
	int           r;
	header_t      fh;

	assert(fd >= 0);
	assert(compressor);
	assert(size);

	DEBUG_("reading header from %d at %zd\n", fd, lseek(fd, 0, SEEK_CUR));
	r = read(fd, &fh, sizeof(fh));
	if (r == -1)
		return r;

	if (r == sizeof(fh))
	{
		fh.size = from_le64(fh.size);
		*compressor = file_compressor(&fh);
		if (*compressor)
			*size       = fh.size;
	}
	return 0;
}

/**
 * Try to acquire compressor type and uncompressed file size from header.
 *
 * @return  0 - compressor and size retrieved or invalid header
 *         -1 - i/o error
 */
int file_read_header_name(const char *name, compressor_t **compressor, off_t *size)
{
	int r;
	int x;
	int fd;
	int err = 0;	// Only to shut-up compiler's warning about uninitialized use

	assert(name);
	assert(compressor);
	assert(size);

	fd = file_open(name, O_RDONLY);
	if (fd == FAIL)
		return FAIL;

	r = file_read_header_fd(fd, compressor, size);
	if (r == FAIL)
		err = errno;

	x = close(fd);
	if (x == FAIL)
		err = errno;

	if ((r == FAIL) || (x == FAIL))
	{
		errno = err;
		return FAIL;
	}

	return 0;
}

char *file_create_temp(int *fd_temp)
{
	int i = 0;
	int fd = -1;
	int temp_len;
	char *temp;
	struct timespec delay;

	temp_len = sizeof(TEMP) + 7;
	temp = (char *)malloc(temp_len);
	if (temp == NULL)
	{
		CRIT_("No memory!");
		//exit(EXIT_FAILURE);
		*fd_temp = -1;
		return 0;
	}

	while (fd == -1)
	{
		memcpy(temp, TEMP "XXXXXX", sizeof(TEMP) + sizeof("XXXXXX") - 1);
		fd = mkstemp(temp);

		// Mkstemp may fail. This protects us before some misterious deadlock
		// that may happen.
		//
		if (i++ > 50)
		{
			CRIT_("Unable to create temp file");
			//exit(EXIT_FAILURE);
			free(temp);
			*fd_temp = -1;
			return 0;
		}
		if (fd == -1)
		{
			delay.tv_sec = 1,
			delay.tv_nsec = 0,
			nanosleep(&delay, NULL);
		}
	}

	*fd_temp = fd;
	return temp;
}

/**
 * "Force" opens a file, ie opens a file even if we dont have access.
 *
 * @param entry 
 * @param mode 
 * @return 
 */
#define ALLMODES S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
int file_open(const char *filename, int mode)
{
	int         fd;
	int         newmode;
	struct stat buf;

	// Try to open file, if we dont have access, force it.
	//
	fd = open(filename, mode);
	if (fd == FAIL && (errno == EACCES || errno == EPERM))
	{
		DEBUG_("fail with EACCES or EPERM");
		// TODO: chmod failing in the start here shouldn't be critical, but size will be invalid if it fails.
		// Grab old file info.
		//
		if (stat(filename,&buf) == FAIL)
		{
			return FAIL;
		}
		newmode = buf.st_mode | ALLMODES;
		if (chmod(filename,newmode) == FAIL)
		{
			return FAIL;
		}
		fd = open(filename, mode);
		if (fd == FAIL)
		{
			return FAIL;
		}
		if (chmod(filename,buf.st_mode) == FAIL)
		{
			return FAIL;
		}
	} else { DEBUG_("fail"); }
	return fd;
}

void file_close(int *fd)
{
	assert(fd);
	assert(*fd > FAIL);

	if (close(*fd) == -1)
	{
		CRIT_("Failed to close fd!");
		//exit(EXIT_FAILURE);
	};

	*fd = -1;
}

int is_compressible(const char *filename)
{
        char **ext;
        for (ext = incompressible; *ext != NULL; ext++)
                if (strcasestr(filename, *ext))
                        return FALSE;
        if (user_incompressible)
                for (ext = user_incompressible; *ext != NULL; ext++)
                        if (strcasestr(filename, *ext))
                                return FALSE;
        return TRUE;
}

int is_excluded(const char *filename)
{
  char **dir;
  if (user_exclude_paths) {
    for (dir = user_exclude_paths; *dir != NULL; dir++) {
      if (strncmp(filename, *dir, strlen(*dir)) == 0)
        return TRUE;
    }
  }
  return FALSE;
}
