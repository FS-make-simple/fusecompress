/*
    FuseCompress
    Copyright (C) 2005 Milan Svoboda <milan.svoboda@centrum.cz>
*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include "structs.h"
#include "globals.h"
#include "compress.h"
#include "file.h"
#include "log.h"

#define BUF_SIZE 4096

/**
 * Close compressed stream.
 *
 * @return  0 - Success
 * @return -1 - File system error (not compression error), errno contains error code
 */
static int gzClose(gzFile fd_gz)
{
    int res;

    res = gzclose(fd_gz);
    if (res != Z_OK)
    {
        if (res == Z_ERRNO)
        {
            ERR_("Failed to close fd_gz!");
            return -1;
        }
        CRIT_("Failed to close fd_gz (gz error): %s", gzerror(fd_gz, &res));
        //exit(EXIT_FAILURE);
        return -1;
    }
    return 0;
}

/**
 * Compress data from fd_source into fd_dest.
 *
 * @param fd_source    Source file descriptor
 * @param fd_dest    Destination file descriptor
 * @return         Number of bytes readed from fd_source or -1 on error
 */
static off_t gzCompress(void *cancel_cookie, int fd_source, int fd_dest)
{
    char buf[BUF_SIZE];
    int wr;
    int rd;
    gzFile fd_gz = NULL;
    off_t size = 0;
    int dup_fd;

    dup_fd = dup(fd_dest);
    if (dup_fd == -1)
    {
        return (off_t) FAIL;
    }

    fd_gz = gzdopen(dup_fd, COMPRESSLEVEL_BACKGROUND);
    if (fd_gz == NULL)
    {
        file_close(&dup_fd);
        return (off_t) FAIL;
    }

    while ((rd = read(fd_source, buf, sizeof(buf))) > 0)
    {
        size += rd;

        wr = gzwrite(fd_gz, buf, rd);
        if(wr == -1)
        {
            gzClose(fd_gz);
            return (off_t) FAIL;
        }

        if (compress_testcancel(cancel_cookie))
        {
            break;
        }
    }

    if (rd < 0)
    {
        gzClose(fd_gz);
        return (off_t) FAIL;
    }

    gzClose(fd_gz);

    return size;
}

/**
 * Decompress data from fd_source into fd_dest.
 *
 * @param fd_source    Source file descriptor
 * @param fd_dest    Destination file descriptor
 * @return         Number of bytes written to fd_dest or (off_t)-1 on error
 */
static off_t gzDecompress(int fd_source, int fd_dest)
{
    int wr;
    int rd;
    char buf[BUF_SIZE];
    off_t size = 0;
    gzFile fd_gz;
    int dup_fd;

    dup_fd = dup(fd_source);
    if (dup_fd == -1)
    {
        return (off_t) FAIL;
    }

    fd_gz = gzdopen(dup_fd, "rb");
    if (fd_gz == NULL)
    {
        file_close(&dup_fd);
        return (off_t) FAIL;
    }

    while ((rd = gzread(fd_gz, buf, BUF_SIZE)) > 0)
    {
        wr = write(fd_dest, buf, rd);
        if(wr == -1)
        {
            gzClose(fd_gz);
            return (off_t) FAIL;
        }
        size += wr;
    }

    if (rd == -1)
    {
        gzClose(fd_gz);
        return (off_t) FAIL;
    }

    gzClose(fd_gz);
    return size;
}

compressor_t module_gzip =
{
    .type = 0x02,
    .name = "gz",
    .compress = gzCompress,
    .decompress = gzDecompress,
    .open = (void *(*) (int fd, const char *mode)) gzdopen,
    .write = (int (*)(void *file, void *buf, unsigned int len)) gzwrite,
    .read = (int (*)(void *file, void *buf, unsigned int len)) gzread,
    .close = (int (*)(void *file)) gzClose,
};

