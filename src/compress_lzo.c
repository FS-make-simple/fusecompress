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
#include "minilzo/lzo.h"
#include "compress.h"

#define BUF_SIZE 4096

/**
 * Close compressed stream.
 *
 * @return  0 - Success
 * @return -1 - File system error (not compression error), errno contains error code
 */
static int lzoClose(lzoFile *fd_lzo)
{
    int r;

    r = lzoclose(fd_lzo);
    if (r == -1)
    {
        ERR_("Failed to close fd_lzo!");
    }
    return r;
}

/**
 * Compress data from fd_source into fd_dest.
 *
 * @param fd_source    Source file descriptor
 * @param fd_dest    Destination file descriptor
 * @return         Number of bytes readed from fd_source or (off_t)-1 on error
 */
static off_t lzoCompress(void *cancel_cookie, int fd_source, int fd_dest)
{
    char buf[BUF_SIZE];
    unsigned wr;
    int rd;
    int dup_fd;
    off_t size = 0;
    lzoFile *fd_lzo;

    dup_fd = dup(fd_dest);
    if (dup_fd == -1)
    {
        return (off_t) FAIL;
    }

    fd_lzo = lzodopen(dup_fd, COMPRESSLEVEL_BACKGROUND);
    if (fd_lzo == NULL)
    {
        ERR_("lzodopen failed");
        file_close(&dup_fd);
        return (off_t) FAIL;
    }

    while ((rd = read(fd_source, buf, sizeof(buf))) > 0)
    {
        size += rd;

        wr = lzowrite(fd_lzo, buf, rd);
        if (wr == 0)
        {
            ERR_("lzowrite failed");
            lzoClose(fd_lzo);
            return (off_t) FAIL;
        }

        if (compress_testcancel(cancel_cookie))
        {
            break;
        }
    }

    if (rd < 0)
    {
        ERR_("read failed");
        lzoClose(fd_lzo);
        return (off_t) FAIL;
    }

    lzoClose(fd_lzo);
    return size;
}

/**
 * Decompress data from fd_source into fd_dest.
 *
 * @param fd_source    Source file descriptor
 * @param fd_dest    Destination file descriptor
 * @return         Number of bytes written to fd_dest or (off_t)-1 on error
 */
static off_t lzoDecompress(int fd_source, int fd_dest)
{
    char buf[BUF_SIZE];
    int wr;
    int rd;
    int dup_fd;
    off_t size = 0;
    lzoFile *fd_lzo;

    dup_fd = dup(fd_source);
    if (dup_fd == -1)
    {
        return (off_t) FAIL;
    }

    fd_lzo = lzodopen(dup_fd, "rb");
    if (fd_lzo == NULL)
    {
        ERR_("lzodopen failed");
        file_close(&dup_fd);
        return (off_t) FAIL;
    }

    while ((rd = lzoread(fd_lzo, buf, BUF_SIZE)) > 0)
    {
        wr = write(fd_dest, buf, rd);
        if (wr == -1)
        {
            lzoClose(fd_lzo);
            return (off_t) FAIL;
        }
        size += wr;
    }

    if (rd == -1)
    {
        lzoClose(fd_lzo);
        return (off_t) FAIL;
    }

    lzoClose(fd_lzo);
    return size;
}

compressor_t module_lzo =
{
    .type = 0x03,
    .name = "lzo",
    .compress = lzoCompress,
    .decompress = lzoDecompress,
    .open = (void *(*) (int fd, const char *mode)) lzodopen,
    .write = (int (*)(void *file, void *buf, unsigned int len)) lzowrite,
    .read = (int (*)(void *file, void *buf, unsigned int len)) lzoread,
    .close = (int (*)(void *file)) lzoClose,
};
