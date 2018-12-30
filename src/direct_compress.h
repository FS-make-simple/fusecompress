/* direct_compress for fusecompress.
 *
 * Copyright (C) 2005 Anders Aagaard <aagaande@gmail.com>
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
 
#ifndef DIRECT_COMPRESS_H
#define DIRECT_COMPRESS_H

#include <errno.h>

#include "structs.h"
#include "compress.h"

int direct_close(file_t *file, descriptor_t *descriptor);
file_t *direct_open(const char *filename, int stabile);
void direct_open_purge(void);
void direct_open_purge_force(void);
int direct_decompress(file_t *file, descriptor_t *descriptor,void *buffer, size_t size, off_t offset);
int direct_compress(file_t *file, descriptor_t *descriptor, const void *buffer, size_t size, off_t offset);
void direct_delete(file_t *file);
file_t *direct_rename(file_t *file_from, file_t *file_to);

void flush_file_cache(file_t* file);

#endif
