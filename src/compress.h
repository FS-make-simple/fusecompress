/*
 * Copyright (C) 2006 Milan Svoboda <milan.svoboda@centrum.cz>
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

#include "compress_gz.h"
#include "compress_bz2.h"
#include "compress_lzo.h"
#include "compress_null.h"
#include "compress_lzma.h"

compressor_t *choose_compressor(const file_t *file);

/**
 * Decompress file.
 *
 * @param entry Specifies file which will be decompressed
 */
int do_decompress(file_t *file);

/**
 * Compress file.
 *
 * This function is called do_compress because the zlib exports
 * compress name. I think that it is stupid export so common name
 * in library interface.
 *
 * @param entry Specifies file which will be compressed
 */
void do_compress(file_t *file);

/**
 * Test cancel
 *
 * @return This function returns TRUE if compression should be canceled.
 *         Otherwise FALSE is returned.
 */
int compress_testcancel(void *cancel_cookie);
