/*
    FuseCompress
    Copyright (C) 2006 Milan Svoboda <milan.svoboda@centrum.cz>
    LZMA module (C) 2008 Ulrich Hecht <uli@suse.de>
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
#include <stdint.h>
#include <lzma.h>

#if LZMA_VERSION <= UINT32_C(49990030)
#define LZMA_EASY_ENCODER(a,b) lzma_easy_encoder_single(a,b)
#elif LZMA_VERSION <= UINT32_C(49990050)
#define LZMA_EASY_ENCODER(a,b) lzma_easy_encoder(a,b)
#else
#define LZMA_EASY_ENCODER(a,b) lzma_easy_encoder(a,b,LZMA_CHECK_CRC32)
#endif

#include "structs.h"
#include "globals.h"
#include "file.h"
#include "log.h"
#include "compress.h"

#define BUF_SIZE 4096

static const lzma_stream lzma_stream_init = LZMA_STREAM_INIT;

/**
 *
 *
 * @param fd_source	Source file descriptor
 * @param fd_dest	Destination file descriptor
 * @return 		Number of bytes read from fd_source or (off_t)-1 on error
 */
static off_t lzmaCompress(void *cancel_cookie, int fd_source, int fd_dest)
{
	DEBUG_("lzmaCompress started");
	unsigned char bufin[BUF_SIZE];	/* data read from file */
	unsigned char bufout[BUF_SIZE];	/* compressed data */
	unsigned int wr;	/* bytes written to compressed file */
	int rd;			/* bytes read from uncompressed file */
	off_t size = 0;		/* total size read (uncompressed) */
	int ret;
	
        int dup_fd = dup(fd_dest);
        if (dup_fd == -1)
        {
        	return (off_t) FAIL;
        }
                                                	/* init LZMA encoder */
	lzma_stream lstr = lzma_stream_init;
	ret = LZMA_EASY_ENCODER(&lstr, compresslevel[2] - '0');
	if(ret != LZMA_OK) {
		ERR_("LZMA_EASY_ENCODER failed: %d",ret);
		close(dup_fd);
		return (off_t)FAIL;
	}
	
	while ((rd = read(fd_source, bufin, sizeof(bufin))) > 0)
	{
		lstr.next_in = (const uint8_t*)bufin;
		lstr.avail_in = rd;
		while(lstr.avail_in) {
			lstr.next_out = (uint8_t*)bufout;
			lstr.avail_out = BUF_SIZE;
			ret = lzma_code(&lstr, LZMA_RUN);
			if(ret != LZMA_OK) {
				lzma_end(&lstr);
				ERR_("lzma_code failed");
				close(dup_fd);
				return (off_t)FAIL;
			}
			//assert(lstr.avail_in == 0);
			wr = write(dup_fd, bufout, BUF_SIZE - lstr.avail_out);
			DEBUG_("lzmaCompress %d bytes written",wr);
			if (wr < BUF_SIZE - lstr.avail_out)
			{	/* unable to write all compressed data */
				lzma_end(&lstr);
				ERR_("write failed");
				close(dup_fd);
				return (off_t) FAIL;
			}
		}

		size += rd;
		if (compress_testcancel(cancel_cookie))
		{	/* we're told to cancel compression */
			break;
		}
	}

	if (rd < 0)
	{
		lzma_end(&lstr);
		ERR_("read error");
		close(dup_fd);
		return (off_t) FAIL;
	}

	/* compress remaining data in input buffer */
	for(;;) {
		lstr.next_out = bufout;
		lstr.avail_out = BUF_SIZE;
		ret = lzma_code(&lstr, LZMA_FINISH);
		DEBUG_("LZMA_FINISHED out'ed %zd bytes",BUF_SIZE - lstr.avail_out);
		if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
			lzma_end(&lstr);
			ERR_("LZMA_FINISH failed: %d",ret);
			close(dup_fd);
			return (off_t)FAIL;
		}
		if(write(dup_fd, bufout, BUF_SIZE - lstr.avail_out) < 0) {
			ERR_("writing tail failed");
			close(dup_fd);
			return ret;
		}
		if(ret == LZMA_STREAM_END) break;
	}
	lzma_end(&lstr);
	DEBUG_("returned size %ld",size);
	close(dup_fd);
	return size;
}

/**
 * Decompress data from fd_source into fd_dest.
 *
 * @param fd_source	Source file descriptor
 * @param fd_dest	Destination file descriptor
 * @return 		Number of bytes written to fd_dest or (off_t)-1 on error
 */
static off_t lzmaDecompress(int fd_source, int fd_dest)
{
	DEBUG_("lzmaDecompress started");
	unsigned char bufin[BUF_SIZE];	/* compressed input buffer */
	unsigned char bufout[BUF_SIZE];	/* uncompressed output buffer */
	int wr;		/* uncompressed bytes written */
	int rd;		/* compressed bytes read */
	off_t size = 0;	/* total uncompressed bytes written */
	int ret;

	int dup_fd = dup(fd_source);
	if(dup_fd == -1) return (off_t)FAIL;
	
	/* init LZMA decoder */
	lzma_stream lstr = lzma_stream_init;
#if LZMA_VERSION <= UINT32_C(49990030)
	ret = lzma_auto_decoder(&lstr, NULL, NULL);
#else
	ret = lzma_auto_decoder(&lstr, -1, 0);
#endif
	if(ret != LZMA_OK) {
		ERR_("lzma_auto_decoder failed");
		close(dup_fd);
		return (off_t)FAIL;
	}
	
	while ((rd = read(dup_fd, bufin, BUF_SIZE)) > 0)
	{
		lstr.next_in = bufin;
		lstr.avail_in = rd;
		
		while(lstr.avail_in) {
			lstr.next_out = bufout;
			lstr.avail_out = BUF_SIZE;
			ret = lzma_code(&lstr, LZMA_RUN);
			if(ret != LZMA_OK && ret != LZMA_STREAM_END) {	/* decompression error */
				lzma_end(&lstr);
				ERR_("lzma_code failed: %d",ret);
				close(dup_fd);
				return (off_t)FAIL;
			}
			wr = write(fd_dest, bufout, BUF_SIZE - lstr.avail_out);
			if (wr == -1) {
				lzma_end(&lstr);
				ERR_("write failed");
				close(dup_fd);
				return (off_t) FAIL;
			}
			size += wr;
			if(ret == LZMA_STREAM_END) break;
		}
		if (ret == LZMA_STREAM_END) break;
	}
	
	lzma_end(&lstr);
	
	close(dup_fd);
	
	if (rd == -1)
	{
		ERR_("read error");
		return (off_t) FAIL;
	}

	return size;
}

struct lzmafile {
	lzma_stream str;	/* codec stream descriptor */
	int fd;			/* backing file descriptor */
	char mode;		/* access mode ('r' or 'w') */
	unsigned char rdbuf[BUF_SIZE];	/* read buffer used by lzmaRead */
};

void* lzmaOpen(int fd, const char* mode)
{
	DEBUG_("lzmaOpen started");
	int ret;

	/* initialize LZMA stream */
	struct lzmafile* lf = malloc(sizeof(struct lzmafile));
	if(!lf) return NULL;
	lf->fd = fd;
	lf->str = lzma_stream_init;
	lf->mode = mode[0];
	if(mode[0] == 'r') {
#if LZMA_VERSION <= UINT32_C(49990030)
		ret = lzma_auto_decoder(&lf->str, NULL, NULL);
#else
		ret = lzma_auto_decoder(&lf->str, -1, 0);
#endif
		lf->str.avail_in = 0;
	}
	else {
		ret = LZMA_EASY_ENCODER(&lf->str, mode[2] - '0');
	}
	if(ret != LZMA_OK) return NULL;
	return (void*)lf;
}

int lzmaClose(struct lzmafile* file)
{
	DEBUG_("lzmaClose started");
	int fd;
	int ret;
	unsigned char buf[BUF_SIZE];	/* buffer used when flushing remaining
					   output data in write mode */
	if(!file) return -1;
	fd = file->fd;
	if(file->mode == 'w') {
		/* flush LZMA output buffer */
		for(;;) {
			file->str.next_out = buf;
			file->str.avail_out = BUF_SIZE;
			ret = lzma_code(&file->str, LZMA_FINISH);
			if(ret != LZMA_STREAM_END && ret != LZMA_OK) {
				lzma_end(&file->str);
				return -1;
			}
			int outsize = BUF_SIZE - file->str.avail_out;
			DEBUG_("LZMA_FINISH out'ed %d bytes",outsize);
			if(write(fd, buf, outsize) != outsize) {
				lzma_end(&file->str);
				return -1;
			}
			if(ret == LZMA_STREAM_END) break;
		}
	}
	DEBUG_("lzmaClose total out %ld bytes",file->str.total_out);
	lzma_end(&file->str);
	free(file);
	return close(fd);
}

int lzmaWrite(struct lzmafile* file, void* buf, unsigned int len)
{
	DEBUG_("lzmaWrite started");
	int ret;
	lzma_stream* lstr = &file->str;
	unsigned char bufout[len];	/* compressed output buffer */
	
	if(file->mode != 'w') return -1;
	lstr->next_in = buf;
	lstr->avail_in = len;
	while(lstr->avail_in) {
		lstr->next_out = bufout;
		lstr->avail_out = len;
		ret = lzma_code(lstr, LZMA_RUN);
		DEBUG_("lzma_code in %d out %ld ret %d",len,len-lstr->avail_out, ret);
		if(ret != LZMA_OK) return -1;
		DEBUG_("writing at %zd",lseek(file->fd,0,SEEK_CUR));
		ret = write(file->fd, bufout, len-lstr->avail_out);
		if(ret != len-lstr->avail_out) return -1;
	}
	return len;
}

int lzmaRead(struct lzmafile* file, void* buf, unsigned int len)
{
	DEBUG_("lzmaRead started at %zd", lseek(file->fd, 0, SEEK_CUR));
	int ret;
	
	if(file->mode != 'r') return -1;
	
	lzma_stream* lstr = &file->str;
	lstr->next_out = buf;
	lstr->avail_out = len;
	
	/* decompress until EOF or output buffer is full */
	while(lstr->avail_out) {
		if(lstr->avail_in == 0) {
			/* refill input buffer */
			if((ret = read(file->fd, file->rdbuf, BUF_SIZE)) < 0)
				return ret;
			DEBUG_("read %d bytes",ret);
			if(ret == 0) break;	/* EOF */
			lstr->next_in = file->rdbuf;
			lstr->avail_in = ret;
		}
		DEBUG_("avail_in %zd avail_out %zd",lstr->avail_in,lstr->avail_out);
		ret = lzma_code(lstr, LZMA_RUN);
		if(ret != LZMA_OK && ret != LZMA_STREAM_END) {
			ERR_("decoding failed: %d",ret);
			return -1;
		}
		if(ret == LZMA_STREAM_END) break;
	}
	DEBUG_("decoded to %zd bytes", len - lstr->avail_out);
	return len - lstr->avail_out;
}

compressor_t module_lzma = {
	.type = 0x04,
	.name = "lzma",
	.compress = lzmaCompress,
	.decompress = lzmaDecompress,
	.open = (void *(*) (int fd, const char *mode)) lzmaOpen,
	.write = (int (*)(void *file, void *buf, unsigned int len)) lzmaWrite,
	.read = (int (*)(void *file, void *buf, unsigned int len)) lzmaRead,
	.close = (int (*)(void *file)) lzmaClose,
};
