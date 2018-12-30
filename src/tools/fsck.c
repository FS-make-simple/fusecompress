/*
    fsck.fusecompress
    (C) 2008 Ulrich Hecht <uli@suse.de>

    This program can be distributed under the terms of the GNU GPL v2.
    See the file COPYING.
 */

#include "../structs.h"
#include "../compress.h"
#include "../compress_lzo.h"
#include "../compress_lzma.h"
#include "../compress_bz2.h"
#include "../compress_gz.h"
#include "../file.h"
#include "../globals.h"
#ifdef WITH_DEDUP
#include "../dedup.h"
#endif

#include <ftw.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#ifdef WITH_DEDUP
#include <mhash.h>
#endif

#define BUFSIZE 131072	/* read buffer size */
#define MAXOPENFD 400	/* max number of dirs ftw() keeps open */

/* error codes */
#define OK 0
#define BROKEN_HEADER 1
#define FAIL_OPEN 2
#define FAIL_READ 3
#define FAIL_OPEN_DECOMP 4
#define FAIL_READ_DECOMP 5
#define SHORT_READ_DECOMP 6
#define FAIL_CLOSE_DECOMP 7
#define STALE_TEMP 8

/* FIXME: not very clean */
char compresslevel[] = "wbx";
int compress_testcancel(void *x)
{
	return 0;
}

/* FuseCompress file header magic bytes */
const unsigned char magic[] = { 037, 0135, 0211 };

int verbose = 0;	/* verbose output */
int unlink_enabled = 0; /* delete files with errors */
int fix_fixables = 0; /* repair files if possible */
int errors_found = 0;
int errors_fixed = 0;
int warnings = 0;
#ifdef WITH_DEDUP
int rebuild_dedup_db = 0;
int dedup_now = 0;
FILE *dedup_db_fp;
#endif
size_t total_size = 0;

/* remove file if unlink_enabled is true */
void do_unlink(const char *fpath)
{
	if (unlink_enabled)
	{
		fprintf(stderr, "%s: removing file\n", fpath);
		unlink(fpath);
		errors_fixed++;
	}
	else if (verbose)
		fprintf(stderr, "%s: not removing file (disabled)\n", fpath);
}

/* try to fix error on file fpath */
int fix(int fd, const char *fpath, int error)
{
	errors_found++;
	switch (error)
	{
		case BROKEN_HEADER:
			fprintf(stderr, "%s: broken header\n", fpath);
			do_unlink(fpath);
			break;

		case FAIL_READ_DECOMP:
		case SHORT_READ_DECOMP:
			fprintf(stderr, "%s: read error while decompressing\n", fpath);
			do_unlink(fpath);
			break;

		case STALE_TEMP:
			fprintf(stderr, "%s: stale temporary file\n", fpath);
			do_unlink(fpath);
			break;
			
		default:
			fprintf(stderr, "unknown error %d, ignored\n", error);
			break;
	}
	close(fd);
	return 0;
}

void warn(const char* fpath, int warning)
{
	warnings++;
	switch(warning)
	{
		case FAIL_OPEN:
			fprintf(stderr,"%s: unable to open file\n", fpath);
			break;
		default:
			fprintf(stderr, "unknown warning %d\n", warning);
			break;
	}
}

/* check file for errors in compressed data
   everything that is not a FuseCompress-compressed regular file is ignored */
int checkfile(const char *fpath, const struct stat *sb, int typeflag, struct FTW* ftwbuf)
{
	int fd, res;
	unsigned char m[3];
	compressor_t *compr;
	off_t size;
	void *handle;
	char buf[BUFSIZE];

	if (!S_ISREG(sb->st_mode))
		return 0;	/* no regular file */

	if (verbose >= 2)
		fprintf(stderr, "%zd MB ", total_size / 1048576);
	if (verbose)
		fprintf(stderr, "checking file %s: ", fpath);

	fd = file_open(fpath, O_RDONLY);
	if (fd < 0) {
		warn(fpath, FAIL_OPEN);
		return 0;
	}

	if (strncmp(&fpath[ftwbuf->base], TEMP, sizeof(TEMP) - 1) == 0  ||
	    strncmp(&fpath[ftwbuf->base], FUSE, sizeof(FUSE) - 1) == 0)
	    	return fix(fd, fpath, STALE_TEMP);

	/* other internal file (attribute file, dedup DB) */
	if (strncmp(&fpath[ftwbuf->base], FUSECOMPRESS_PREFIX, sizeof(FUSECOMPRESS_PREFIX) - 1) == 0) {
		close(fd);
		return 0;
	}

	m[0] = m[1] = m[2] = 0;
	if (read(fd, m, 3) < 0)
		return fix(fd, fpath, FAIL_READ);

#ifdef WITH_DEDUP
	int hashed = FALSE;
	unsigned char md5[16];
#endif
	if (!memcmp(m, magic, 3))
	{
		/* compressed file */
		lseek(fd, 0, SEEK_SET);	/* is this actually necessary? */

		res = file_read_header_fd(fd, &compr, &size);
		if (res == FAIL)
			return fix(fd, fpath, BROKEN_HEADER);

		handle = compr->open(fd, "r");
		if (!handle)
			return fix(fd, fpath, FAIL_OPEN_DECOMP);

#ifdef WITH_DEDUP
		MHASH mh = NULL;
		if (rebuild_dedup_db) {
			mh = mhash_init(MHASH_MD5);
		}
#endif
		total_size += size;
		while (size)
		{
			res = compr->read(handle, buf, size > BUFSIZE ? BUFSIZE : size);
			if (res < 0)
				return fix(fd, fpath, FAIL_READ_DECOMP);
			if (res == 0 && size)
				return fix(fd, fpath, SHORT_READ_DECOMP);
#ifdef WITH_DEDUP
			if (rebuild_dedup_db)
				mhash(mh, buf, res);
#endif
			size -= res;
		}
#ifdef WITH_DEDUP
		if (rebuild_dedup_db) {
			mhash_deinit(mh, md5);
			hashed = TRUE;
		}
#endif
		if (compr->close(handle) < 0)
			return fix(fd, fpath, FAIL_CLOSE_DECOMP);
		
		if (verbose)
			fprintf(stderr, "ok");
	}
	else {
		if (verbose)
			fprintf(stderr, "uncompressed");
		if (verbose >= 2)
			total_size += lseek(fd, 0, SEEK_END);
		close(fd);
	}
	
#ifdef WITH_DEDUP
	if (rebuild_dedup_db) {
		if (!hashed)
			dedup_hash_file(fpath, md5);
		if (dedup_now) {
			if (hardlink_file(md5, fpath + 2))
				fprintf(stderr, " deduped");
		}
		else if (!dedup_db_has_md5(md5)) {
			dedup_add(md5, fpath + 2);
		}
	}
#endif
	if (verbose)
		fprintf(stderr, "\n");

	return 0;
}

void usage(char *n)
{
	fprintf(stderr, "Usage: %s [-dv] directory\n\n", n);
	fprintf(stderr, " -d\tRemove unfixable or superfluous broken files\n");
	fprintf(stderr, " -p\tFix fixable files\n");
#ifdef WITH_DEDUP
	fprintf(stderr, " -r\tRebuild deduplication database\n");
	fprintf(stderr, " -l\tDeduplicate files while building database\n");
#endif
	fprintf(stderr, " -v\tBe verbose\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int next_option;

	do
	{
		next_option = getopt(argc, argv, "dpv"
#ifdef WITH_DEDUP
		"rl"
#endif
		);
		switch (next_option)
		{
			case 'd':
				unlink_enabled = 1;
				break;
			case 'p':
				fix_fixables = 1;
				break;
#ifdef WITH_DEDUP
			case 'r':
				rebuild_dedup_db = 1;
				break;
			case 'l':
				dedup_now = 1;
				break;
#endif
			case 'v':
				verbose++;
				break;
			case -1:
				break;
			default:
				usage(argv[0]);
				break;
		}
	} while (next_option != -1);

	if (optind >= argc)
		usage(argv[0]);

	chdir(argv[optind]);
#ifdef WITH_DEDUP
	if (rebuild_dedup_db) {
		unlink(DEDUP_DB_FILE);
		dedup_init_db();
	}
#endif
	if (nftw(".", checkfile, MAXOPENFD, FTW_MOUNT|FTW_PHYS) < 0)
	{
		perror("nftw");
		exit(1);
	}
#ifdef WITH_DEDUP
	if (rebuild_dedup_db) {
		dedup_save();
	}
#endif

	if (warnings) fprintf(stderr, "%d warnings\n", warnings);
	if (!errors_found) {
		fprintf(stderr, "no errors found\n");
		return 0;
	}
	if (errors_found > errors_fixed) {
		fprintf(stderr, "%d errors fixed, %d unfixed errors remain\n", errors_fixed, errors_found - errors_fixed);
		return 4;
	}
	fprintf(stderr, "%d errors fixed\n", errors_fixed);
	return 1;		/* errors found and fixed */
}
