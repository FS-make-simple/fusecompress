/*
    fusecompress_offline
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

#include <ftw.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <utime.h>
#include <signal.h>

/* FIXME: dirty */
char compresslevel[4] = "wbx";
int compress_testcancel(void *x)
{
	return 0;
}
const unsigned char magic[] = { 037, 0135, 0211 };

compressor_t *comp = NULL;
compressor_t *recomp = NULL;
int verbose = 0;
int errors = 0;

#define MAXOPENFD 400

int is_compressed(const char *fpath)
{
	int fd;
	char buf[4] = { 0, 0, 0, 0 };
	fd = open(fpath, O_RDONLY);
	if (fd < 0)
		return -2;
	if (read(fd, buf, 4) < 0) {
		close(fd);
		return -2;
	}
	close(fd);
	if (memcmp(magic, buf, 3) == 0)
		return buf[3];
	else
		return -1;
}

const char *current_tmp_file = NULL;
void cleanup_handler(int sig)
{
	if (current_tmp_file)
		unlink(current_tmp_file);
	signal(sig, SIG_DFL);
	kill(0, sig);
}

int transform(const char *fpath, const struct stat *sb, int typeflag, struct FTW* ftwbuf)
{
	int compress_type;
	int fd_s, fd_t;
	struct stat stbuf_s;
	struct utimbuf utime_s;
	char *tmpname;
	off_t size;
	compressor_t *decomp;

	if (typeflag != FTW_F || !S_ISREG(sb->st_mode))
		return 0;

	/* internal file (attribute file, dedup DB) */
	if (strncmp(&fpath[ftwbuf->base], FUSECOMPRESS_PREFIX, sizeof(FUSECOMPRESS_PREFIX) - 1) == 0)
		return 0;

	if (verbose)
		fprintf(stderr, "%s: ", fpath);

	if (comp && !is_compressible(&fpath[ftwbuf->base])) {
		if (verbose)
			fprintf(stderr, "incompressible\n");
		return 0;
	}

	compress_type = is_compressed(fpath);
	if (compress_type < -1)
	{
		fprintf(stderr, "failed to check %s for compression status\n", fpath);
		errors++;
		return 0;
	}

	/* if this is the type we are supposed to recompress, decompress
	   it first */
	if (comp && recomp && compress_type == recomp->type) {
		compressor_t *temp_c = comp;
		int temp_v = verbose;
		comp = NULL;
		if (verbose)
			fprintf(stderr, "recompress ");
		verbose = 0;
		transform(fpath, sb, typeflag, ftwbuf);
		verbose = temp_v;
		comp = temp_c;
		compress_type = -1;
	}

	if (comp && compress_type >= 0)
	{
		if (verbose)
			fprintf(stderr, "compressed already\n");
		return 0;
	}
	if (!comp && compress_type < 0)
	{
		if (verbose)
			fprintf(stderr, "uncompressed already\n");
		return 0;
	}

	fd_s = file_open(fpath, O_RDONLY);
	if (fstat(fd_s, &stbuf_s) < 0)
	{
		perror("unable to fstat");
		return -1;
	}
	tmpname = file_create_temp(&fd_t);
	current_tmp_file = tmpname;

	if (!tmpname)
	{
		fprintf(stderr, "unable to create temporary file for compression\n");
		return -1;
	}

	if (comp)
	{
		if (lseek(fd_t, sizeof(header_t), SEEK_SET) < 0)
		{
			perror("failed to seek beyond header");
			goto out;
		}
		size = comp->compress(NULL, fd_s, fd_t);
		if (lseek(fd_t, 0, SEEK_SET) < 0)
		{
			perror("failed to seek to beginning");
			goto out;
		}
		if (file_write_header(fd_t, comp, stbuf_s.st_size) < 0)
		{
			perror("failed to write header");
			goto out;
		}
	}
	else
	{
		if (file_read_header_fd(fd_s, &decomp, &size) < 0)
		{
			perror("unable to read header");
			goto out;
		}
		size = decomp->decompress(fd_s, fd_t);
	}

	if (size == (off_t) - 1)
	{
		fprintf(stderr, "compression failed\n");
		goto out;
	}
	close(fd_s);
	close(fd_t);
	if (rename(tmpname, fpath) < 0)
	{
		perror("failed to rename tempfile");
		goto out;
	}

	current_tmp_file = NULL;

	if (lchown(fpath, stbuf_s.st_uid, stbuf_s.st_gid) < 0)
	{
		perror("unable to set owner/group");
		/* do not abort; the filesystem may not support this */
	}
	if (chmod(fpath, stbuf_s.st_mode) < 0)
	{
		perror("unable to set permissions");
		/* do not abort; the filesystem may not support this */
	}
	utime_s.actime = stbuf_s.st_atime;
	utime_s.modtime = stbuf_s.st_mtime;
	if (utime(fpath, &utime_s) < 0)
	{
		perror("failed to set timestamps");
		goto out;
	}

	if (verbose)
		fprintf(stderr, "ok\n");

	return 0;

out:
	close(fd_s);
	close(fd_t);
	unlink(tmpname);
	return -1;
}

void usage(char *n)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [path...]\n\n", n);
	fprintf(stderr, " -c lzo/gz/bz2/lzma/null\tCompress file using the given method\n");
	fprintf(stderr, " -r lzo/gz/bz2/lzma/null\tRecompress file in given format\n");
	fprintf(stderr, " -l LEVEL\t\t\tSpecifies compression level\n");
	fprintf(stderr, " -v\t\t\t\tReport progress\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int next_option;

	compresslevel[2] = 'x';

	do
	{
		next_option = getopt(argc, argv, "c:l:vr:");
		switch (next_option)
		{
			case 'c':
				comp = find_compressor_name(optarg);
				if (!comp)
					usage(argv[0]);

				if (compresslevel[2] == 'x') {
					if (comp->type == 4)	/* LZMA */
						compresslevel[2] = '4';
					else
						compresslevel[2] = '6';
				}

				break;
			case 'r':
				recomp = find_compressor_name(optarg);
				if (!recomp)
					usage(argv[0]);
				break;
			case 'l':
				if (strlen(optarg) != 1 || optarg[0] < '1' || optarg[0] > '9')
					usage(argv[0]);
				compresslevel[2] = optarg[0];
				break;
			case 'v':
				verbose = 1;
				break;
			case -1:
				break;
			default:
				usage(argv[0]);
				break;
		}
	} while (next_option != -1);

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(argv[-optind]);

	signal(SIGINT, cleanup_handler);
	signal(SIGTERM, cleanup_handler);
	signal(SIGHUP, cleanup_handler);
	while (argc)
	{
		if (nftw(argv[0], transform, MAXOPENFD, FTW_PHYS) < 0)
		{
			exit(1);
		}
		argc--;
		argv++;
	}

	return 0;
}
