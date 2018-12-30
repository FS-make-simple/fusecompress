/*
    FuseCompress
    Copyright (C) 2006 Milan Svoboda <milan.svoboda@centrum.cz>
*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "structs.h"
#include "globals.h"
#include "file.h"
#include "log.h"
#include "compress.h"

#define BUF_SIZE 4096

void *nullOpen(int fd, const char *mode)
{
	return (void *)(intptr_t) fd;
}

/**
 *
 *
 * @param fd_source	Source file descriptor
 * @param fd_dest	Destination file descriptor
 * @return 		Number of bytes readed from fd_source or (off_t)-1 on error
 */
static off_t nullCompress(void *cancel_cookie, int fd_source, int fd_dest)
{
	char buf[BUF_SIZE];
	unsigned wr;
	int rd;
	off_t size = 0;

	while ((rd = read(fd_source, buf, sizeof(buf))) > 0)
	{
		size += rd;

		wr = write(fd_dest, buf, rd);
		if (wr == 0)
		{
			return (off_t) FAIL;
		}

		if (compress_testcancel(cancel_cookie))
		{
			break;
		}
	}

	if (rd < 0)
	{
		return (off_t) FAIL;
	}

	return size;
}

/**
 * Decompress data from fd_source into fd_dest.
 *
 * @param fd_source	Source file descriptor
 * @param fd_dest	Destination file descriptor
 * @return 		Number of bytes written to fd_dest or (off_t)-1 on error
 */
static off_t nullDecompress(int fd_source, int fd_dest)
{
	char buf[BUF_SIZE];
	int wr;
	int rd;
	off_t size = 0;

	while ((rd = read(fd_source, buf, BUF_SIZE)) > 0)
	{
		wr = write(fd_dest, buf, rd);
		if (wr == -1)
		{
			return (off_t) FAIL;
		}
		size += wr;
	}
	
	if (rd == -1)
	{
		return (off_t) FAIL;
	}

	return size;
}

compressor_t module_null = {
	.type = 0x00,
	.name = "null",
	.compress = nullCompress,
	.decompress = nullDecompress,
	.open = (void *(*) (int fd, const char *mode)) nullOpen,
	.write = (int (*)(void *file, void *buf, unsigned int len)) write,
	.read = (int (*)(void *file, void *buf, unsigned int len)) read,
	.close = (int (*)(void *file)) close,
};
