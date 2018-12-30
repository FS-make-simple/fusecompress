#ifndef FILE_H
#define FILE_H

#include "structs.h"

compressor_t *find_compressor(const header_t *fh);
compressor_t *find_compressor_name(const char *name);
compressor_t *file_compressor(const header_t *fh);

int file_write_header(int fd, compressor_t *compressor, off_t size);
int file_read_header_fd(int fd, compressor_t **compressor, off_t *size);
int file_read_header_name(const char *name, compressor_t **compressor, off_t *size);

char *file_create_temp(int *fd_temp);
int file_open(const char *filename, int mode);
void file_close(int *fd);

int is_compressible(const char *filename);
int is_excluded(const char *filename);

#endif
