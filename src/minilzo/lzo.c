/*
    FuseCompress
    Copyright (C) 2006 Milan Svoboda <milan.svoboda@centrum.cz>
    Copyright (C) 2009 Ulrich Hecht <uli@suse.de>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <lzo/lzo1x.h>
#include "lzo.h"
#include "../utils.h"
#include "../globals.h"

// #include "../log.h"
#define ERR_(...)
#define DEBUG_(...)

#define HEAP_ALLOC(var,size) \
	lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

#define WRITE_BUFFER_SIZE	512*1024

void __attribute__ ((constructor)) lzoinit(void)
{
	lzo_init();
}

/**
 * @param fd
 * @param mode - The mode parameter is as in fopen ("rb" or "wb")
 */
lzoFile* lzodopen(int fd, const char *mode)
{
	lzoFile *file;

	if (!mode) {
		return NULL;
	}

	file = malloc(sizeof(lzoFile));
	if (!file) {
		return NULL;
	}

	file->fd = fd;
	file->blockoff = 0;

	if (strchr(mode, 'r')) {
		file->mode = LZO_READ;

		file->block.buf = NULL;
		file->block.usize = 0;
		file->block.psize = 0;
	} else {
		file->mode = LZO_WRITE;

		file->block.buf = malloc(WRITE_BUFFER_SIZE);
		if (!file->block.buf) {
			free(file);
			return NULL;
		}
		file->block.usize = WRITE_BUFFER_SIZE;
		file->block.psize = 0;
	}

	return file;
}

static inline void lzoclearblock(lzoBlock *block)
{
	free(block->buf);
	block->buf = NULL;
	block->psize = 0;
	block->usize = 0;
}

static int _lzoreadblock(int fd, lzoHead *head, lzoBlock *block)
{
	int       r;
	unsigned char     *p = NULL;
	lzo_uint  usize;

	if (block->usize != head->usize) {
		DEBUG_("realloc, pointer: %p, new size: %lu", block->buf, (unsigned long) head->usize);
		block->buf = realloc(block->buf, head->usize);
		if (!block->buf) {
			ERR_("realloc");
			goto err;
		}
	}

	if (head->usize == head->psize) {
		//
		// Block is not compressed, read it as is.
		//
		r = read(fd, block->buf, head->psize);
		if (r != head->psize) {
			ERR_("read, head->psize: %lu, r: %d", (unsigned long) head->psize, r);
			goto err;
		}
	} else {
		//
		// Block is compressed, read data into auxiliary buffer and
		// decompress it into the user buffer.
		//
		p = malloc(head->psize);
		if (!p) {
			ERR_("malloc");
			goto err;
		}

		r = read(fd, p, head->psize);
		if (r != head->psize) {
			ERR_("read, head->psize: %lu, r: %d", (unsigned long) head->psize, r);
			free(p);
			goto err;
		}
	
		usize = head->usize;

		r = lzo1x_decompress_safe(p, head->psize, (unsigned char*)block->buf, &usize, NULL);
		if ((r != LZO_E_OK) || (usize != head->usize)) {
			ERR_("decompression, usize: %lu, head->usize: %lu", (unsigned long) usize, (unsigned long) head->usize);
			free(p);
			goto err;
		}

		free(p);
	}

	block->usize = head->usize;
	block->psize = head->psize;

	DEBUG_("ok");
	return 0;
err:
	ERR_("error");
	lzoclearblock(block);
	return -1;
}

static int lzoreadblock(int fd, lzoBlock *block)
{
	int r;
	lzoHead head;

	r = read(fd, &head, sizeof(head));
	if (r == 0) {
		DEBUG_("end of file");
		//
		// This should be end of file
		//
		lzoclearblock(block);
		return 0;
	} else if ((r == -1) || (r != sizeof(head))) {
		ERR_("unrecoverable error");
		//
		// Unrecoverable error
		//
		lzoclearblock(block);
		return -1;
	}
	
	head.usize = from_le64(head.usize);
	head.psize = from_le64(head.psize);
	
	DEBUG_("head.usize %lu, head.psize %lu", (unsigned long) head.usize, (unsigned long) head.psize);

	return _lzoreadblock(fd, &head, block);
}

static int _lzowriteblock(int fd, lzoBlock *block)
{
	int r;
	lzoHead  head;

	head.usize = to_le64(block->usize);
	head.psize = to_le64(block->psize);

	r = write(fd, &head, sizeof(head));
	if ((r == -1) || (r != sizeof(head))) {
		ERR_("write head failed!");
		return -1;
	}

	r = write(fd, block->buf, block->psize);
	if ((r == -1) || (r != block->psize)) {
		ERR_("write buffer failed!");
		return -1;
	}
	return 0;
}

static int lzowriteblock(int fd, lzoBlock *block)
{
	//
	// Work-memory needed for compression. Allocate memory in units
	// of `lzo_align_t' (instead of `char') to make sure it is properly aligned.
	//
	HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

	/* LZO does not initialize its memory and is thus not deterministic,
	   leading to situations where identical data looks different in
	   compressed form, which sabotages deduplication; to prevent this,
	   we initialize the memory here if dedup is enabled */
	if (dedup_enabled)
		memset(wrkmem, 0, LZO1X_1_MEM_COMPRESS);

	int      r;
	lzo_uint out_len;
	lzo_uint inp_len = block->psize;
	unsigned char    *out;
	lzoBlock block_out;

	//
	// allocate auxiliary buffer with the size big enough to be able
	// to compress blocks that are bigger compressed then uncompressed.
	//
	out  = malloc(block->psize + block->psize / 16 + 64 + 3);
	if (!out) {
		ERR_("malloc failed, size: %ld", block->psize + block->psize / 16 + 64 + 3);
		return -1;
	}

	r = lzo1x_1_compress((unsigned char*)block->buf, block->psize, out, &out_len, wrkmem);
	if (r != LZO_E_OK) {
		ERR_("lzo1x_1_compress failed!");
		free(out);
		return -1;
	}

	DEBUG_("usize %lu, psize %lu", (unsigned long) inp_len, (unsigned long) out_len);

	if (inp_len <= out_len) {
		//
		// Uncompressed size is smaller or equal as compressed size.
		// Store uncompressed block to disk.
		//
		DEBUG_("store uncompressed block");
		block_out.buf = block->buf;
		block_out.usize = block->psize;
		block_out.psize = block->psize;
	} else {
		//
		// Compressed size is smaller than uncompressed size.
		// Store compressed block to disk.
		//
		DEBUG_("store compressed block");
		block_out.buf = (char*)out;
		block_out.usize = block->psize;
		block_out.psize = out_len;
	}
	r = _lzowriteblock(fd, &block_out);

	free(out);

	return r;
}

int lzowrite(lzoFile *file, char *buf, unsigned buf_len)
{
	int   r;
	unsigned _buf_len = buf_len;

	while (buf_len > 0) {

		DEBUG_("buf_len: %d, file->blockoff: %ld, file->block.usize: %ld",
			buf_len, file->blockoff, file->block.usize);

		while (file->blockoff < file->block.usize) {
			*(file->block.buf + file->blockoff) = *buf;
			buf++;
			file->blockoff++;
			buf_len--;
			file->block.psize++;

			if (buf_len == 0) {
				goto out;
			}
		}

		// file->block.buf is full, write it to disk
		//
		r = lzowriteblock(file->fd, &file->block);
		if (r == -1) {
			return -1;
		}

		file->blockoff = 0;
		file->block.psize = 0;
	}
out:
	DEBUG_("compressed bytes wrote %d", _buf_len - buf_len);
	return _buf_len - buf_len;
}

int lzoread(lzoFile *file, char *buf, unsigned buf_len)
{
	int   r;
	unsigned _buf_len = buf_len;

	while (buf_len > 0) {

		DEBUG_("buf_len: %d, file->blockoff: %ld, file->block.usize: %ld",
			buf_len, file->blockoff, file->block.usize);

		while (file->blockoff < file->block.usize) {
			*buf = *(file->block.buf + file->blockoff);
			buf++;
			file->blockoff++;
			buf_len--;

			if (buf_len == 0) {
				goto out;
			}
		}

		file->blockoff = 0;
		r = lzoreadblock(file->fd, &file->block);
		if (r == -1) {
			return -1;
		} else if (file->block.buf == NULL) {
			DEBUG_("!file->block.buf");
			/* This is not necessarily an error. */
			goto out;
		}
	}
out:
	DEBUG_("uncompressed bytes read %d", _buf_len - buf_len);
	return _buf_len - buf_len;
}

int lzoclose(lzoFile *file)
{
	int r;

	if (file->mode == LZO_WRITE)
	{
		// Flush buffer to disk...
		//
		lzowriteblock(file->fd, &file->block);
	}

	r = close(file->fd);
	free(file->block.buf);
	free(file);

	return r;
}
