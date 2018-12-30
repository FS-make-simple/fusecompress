/*
    FuseCompress
    Copyright (C) 2005 Milan Svoboda <milan.svoboda@centrum.cz>
*/

#include <assert.h>
#include <bzlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "file.h"
#include "structs.h"
#include "globals.h"
#include "log.h"
#include "compress.h"

#define BUF_SIZE 4096

/**
 * Close compressed stream.
 *
 * @return  0 - Success
 * @return -1 - File system error (not compression error), errno contains error code
 */
static int bz2Close(BZFILE *fd_bz2)
{
	(void) BZ2_bzclose(fd_bz2);

	return 0;
}

/**
 * Compress data from fd_source into fd_dest.
 *
 * @param fd_source	Source file descriptor
 * @param fd_dest	Destination file descriptor
 * @return 		Number of bytes readed from fd_source or -1 on error
 */
static off_t bz2Compress(void *cancel_cookie, int fd_source, int fd_dest)
{
	char buf[BUF_SIZE];
	int wr;
	int rd;
	BZFILE *fd_bz2;
	off_t size = 0;
	int dup_fd;
	
	dup_fd = dup(fd_dest);
	if (dup_fd == -1)
	{
		return (off_t) FAIL;
	}

	// Try to open file with bzdopen (open file descriptor)
	//
	fd_bz2 = BZ2_bzdopen(dup_fd, COMPRESSLEVEL_BACKGROUND);
	if (fd_bz2 == NULL)
	{
		file_close(&dup_fd);
		return (off_t) FAIL;
	}

	while ((rd = read(fd_source, buf, sizeof(buf))) > 0)
	{
		size += rd;

		wr = BZ2_bzwrite(fd_bz2, buf, rd);
		if(wr == -1)
		{
			BZ2_bzclose(fd_bz2);
			return (off_t) FAIL;
		}

		if (compress_testcancel(cancel_cookie))
		{
			break;
		}
	}

	if (rd < 0)
	{
		BZ2_bzclose(fd_bz2);
		return (off_t) FAIL;
	}

	BZ2_bzclose(fd_bz2);
	return size;
}

static off_t bz2Decompress(int fd_source, int fd_dest)
{
	int wr;
	int rd;
	char buf[BUF_SIZE];
	off_t size = 0;
	BZFILE *fd_bz2;
	int dup_fd;
	
	dup_fd = dup(fd_source);
	if (dup_fd == -1)
	{
DEBUG_("dup");
		return (off_t) -1;
	}

	fd_bz2 = BZ2_bzdopen(dup_fd, "rb");
	if (fd_bz2 == NULL)
	{
DEBUG_("BZ2_bzdopen");
		file_close(&dup_fd);
		return (off_t) -1;
	}

	while ((rd = BZ2_bzread(fd_bz2, buf, BUF_SIZE)) > 0)
	{
		wr = write(fd_dest, buf, rd);
		if(wr == -1)
		{
DEBUG_("write");
			BZ2_bzclose(fd_bz2);
			return (off_t) -1;
		}
		size += wr;
	}

	if (rd == -1)
	{
DEBUG_("BZ2_bzread");
		BZ2_bzclose(fd_bz2);
		return (off_t) -1;
	}

	BZ2_bzclose(fd_bz2);
	return size;
}

compressor_t module_bz2 = {
	.type = 0x01,
	.name = "bz2",
	.compress = bz2Compress,
	.decompress = bz2Decompress,
	.open = (void *(*) (int fd, const char *mode)) BZ2_bzdopen,
	.write = (int (*)(void *file, void *, unsigned int len)) BZ2_bzwrite,
	.read = (int (*)(void *file, void *, unsigned int len)) BZ2_bzread,
	.close = (int (*)(void *file)) bz2Close,
};
