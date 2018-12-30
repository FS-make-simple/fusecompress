/*
    FuseCompress
    Copyright (C) 2005 Milan Svoboda <milan.svoboda@centrum.cz>
    (C) 2011 Ulrich Hecht <uli@suse.de>
*/

#include "structs.h"

extern int min_filesize_background;
extern int root_fs;
extern int read_only;

extern int cache_decompressed_data;
extern int decomp_cache_size;
extern int max_decomp_cache_size;

extern size_t dont_compress_beyond;

extern int dedup_enabled;
extern int dedup_redup;

#define DC_PAGE_SIZE (4096)

extern pthread_t pt_comp;

extern pthread_mutexattr_t locktype;

extern compressor_t *compressor_default;
extern compressor_t *compressors[5];
extern char *incompressible[];
extern char **user_incompressible;
extern char **user_exclude_paths;
extern char *mmapped_dirs[];

extern database_t database;
extern database_t comp_database;
extern dedup_hash_t dedup_database;

void *thread_compress(void *arg);

#define FUSECOMPRESS_PREFIX "._fC"

#define TEMP FUSECOMPRESS_PREFIX "tmp"		/* Template is: ._.tmpXXXXXX */
#define FUSE ".fuse_hidden"	/* Temporary FUSE file */
#define DEDUP_DB_FILE FUSECOMPRESS_PREFIX "dedup_db"
#define DEDUP_ATTR FUSECOMPRESS_PREFIX "at_"

extern char compresslevel[];
#define COMPRESSLEVEL_BACKGROUND (compresslevel) /* See above, this is for background compress */

// Gcc optimizations
//
#if __GNUC__ >= 3
# define likely(x)	__builtin_expect (!!(x), 1)
# define unlikely(x)	__builtin_expect (!!(x), 0)
#else
# define likely(x)	(x)
# define unlikely(x)	(x)
#endif
