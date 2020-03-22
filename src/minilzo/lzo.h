//
// C Interface: lzo
//
// Description:
//
//
// Author: Milan Svoboda <milan.svoboda@centrum.cz>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef __LZO_H
#define __LZO_H

#include <lzo/lzo1x.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct
    {
        uint64_t    usize;
        uint64_t    psize;
    } lzoHead;

    typedef struct
    {
        char        *buf;
        uint64_t     usize;    /* For write: size of buffer */
        uint64_t     psize;    /* For write: number of stored bytes */
    } lzoBlock;

    enum lzoMode
    {
        LZO_READ,
        LZO_WRITE,
    };

    typedef struct
    {
        int        fd;
        enum lzoMode    mode;
        lzoBlock    block;
        uint64_t    blockoff;
    } lzoFile;

    lzoFile* lzodopen(int fd, const char *mode);
    int lzoclose(lzoFile *file);
    int lzowrite(lzoFile *file, char *buf, unsigned buf_len);
    int lzoread(lzoFile *file, char *buf, unsigned buf_len);

#ifdef __cplusplus
}
#endif

#endif
