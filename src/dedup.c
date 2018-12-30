/* Data deduplication for fusecompress.
 *
 * Copyright (C) 2011 Ulrich Hecht <uli@suse.de>
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

#include "utils.h"
#include "dedup.h"
#include "log.h"
#include "globals.h"
#include "file.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <utime.h>
#include <mhash.h>
#include <fcntl.h>
#include <libgen.h>

inline DATABASE_HASH_QUEUE_T md5_to_hash(unsigned char *md5)
{
  return *((DATABASE_HASH_QUEUE_T *)md5);
}

/** Add a filename/MD5 pair to the dedup database.
 * @param md5 MD5 hash
 * @param filename File name.
 */
void dedup_add(unsigned char *md5, const char *filename)
{
  dedup_t *dp = (dedup_t *)malloc(sizeof(dedup_t));
  memcpy(dp->md5, md5, 16);
  dp->filename = strdup(filename);
  int len;
  dp->filename_hash = gethash(filename, &len);
  list_add_tail(&dp->list_filename_hash, &dedup_database.head_filename[dp->filename_hash & DATABASE_HASH_MASK]);
  list_add_tail(&dp->list_md5, &dedup_database.head_md5[md5_to_hash(md5)]);
  dedup_database.entries++;
}

/** Get attribute file name.
 * @param full Full path to file.
 */
static char *fuseattr_name(const char *full)
{
  char *full_attr = malloc(strlen(full) + sizeof(DEDUP_ATTR) + 1);
  char *slash = rindex(full, '/');
  if (!slash) {
    sprintf(full_attr, DEDUP_ATTR "%s", full);
  }
  else {
    memcpy(full_attr, full, slash - full + 1);
    full_attr[slash - full + 1] = 0;
    strcat(full_attr, DEDUP_ATTR);
    strcat(full_attr, slash + 1);
  }
  return full_attr;
}

/** Create an attribute file with given statistics.
 * @param full_attr Full path to attribute file as returned by fuseattr_name().
 * @param st File stats.
 */
static int create_attr(const char *full_attr, struct stat *st)
{
  int fd = open(full_attr, O_CREAT | O_WRONLY, st->st_mode);
  if (fd < 0)
    return fd;
  fchmod(fd, st->st_mode);
  fchown(fd, st->st_uid, st->st_gid);
  struct timespec ts[2];
  ts[0] = st->st_atim;
  ts[1] = st->st_mtim;
  futimens(fd, ts);
  close(fd);
  return 0;
}

/** Hard-link filename to a file from the dedup database with the same MD5
 * hash.
 * @param md5 file content's 128-bit MD5 hash
 * @param filename file name
 * @return target file name if file was a duplicate and could be deduped, NULL otherwise
 */
const char *hardlink_file(unsigned char *md5, const char *filename)
{
  DEBUG_("looking for '%s' in md5 database", filename);
  /* search for entry with matching MD5 hash */
  LOCK(&dedup_database.lock);
  dedup_t* dp;

  list_for_each_entry(dp, &dedup_database.head_md5[md5_to_hash(md5)], list_md5) {
    if (memcmp(md5, dp->md5, 16) == 0) {
      /* Check if this entry points to the file itself. */
      /* XXX: This is something that should not actually happen, although
         should at worst be a performance problem. */
      if(strcmp(filename, dp->filename) == 0) {
        DEBUG_("second run for '%s', ignoring", filename);
        UNLOCK(&dedup_database.lock);
        return NULL;
      }

      DEBUG_("duping it up with the '%s' man", dp->filename);

      /* We cannot just unlink the duplicate file because some filesystems
         (Btrfs, NTFS) have severe limits on the number of hardlinks per
         directory or per inode; we therefore create a differently-named
         link first, and if that succeeds, we move (rename()) it over the
         existing file. */
      char *tmpname = malloc(strlen(filename) + sizeof(FUSECOMPRESS_PREFIX) + 17);
      char *dn = strdup(filename);
      char *dirn = dirname(dn);
      char *bn = strdup(filename);
      char *basen = basename(bn);
      sprintf(tmpname, "%s/" FUSECOMPRESS_PREFIX "t_%s.%d", dirn, basen, getpid());
      free(bn);
      free(dn);

      if (link(dp->filename, tmpname)) {
        DEBUG_("linking '%s' to '%s' failed", dp->filename, tmpname);
        free(tmpname);
        UNLOCK(&dedup_database.lock);
        return NULL;
      }
      else {
        /* Check if we need an attribute file. */
        struct stat st_src;
        struct stat st_target;
        char *full_attr = fuseattr_name(filename);
        /* try any existing attribute file first */
        /* no need to merge the stats of the real and the attr file here
           because all attributes we look at here are in the attribute
           file; we don't look at size and stuff */
        if (lstat(full_attr, &st_src) < 0) {
          if (lstat(filename, &st_src) < 0) {
            ERR_("failed to stat '%s'", filename);
          }
        }
        if (lstat(tmpname, &st_target) < 0) {
          ERR_("failed to stat '%s'", filename);
        }
        DEBUG_("'%s // %s' mtime %zd/%zd uid %d gid %d mode %d, '%s' mtime %zd/%zd uid %d gid %d mode %d", filename, full_attr,
               st_src.st_mtim.tv_sec, st_src.st_mtim.tv_nsec, st_src.st_uid, st_src.st_gid, st_src.st_mode,
               tmpname, st_target.st_mtim.tv_sec, st_target.st_mtim.tv_nsec, st_target.st_uid, st_target.st_gid, st_target.st_mode);
        if (st_src.st_uid != st_target.st_uid ||
            st_src.st_gid != st_target.st_gid ||
            st_src.st_mode != st_target.st_mode ||
#ifdef EXACT_ATIME
            st_src.st_atim.tv_sec != st_target.st_atim.tv_sec ||
            st_src.st_atim.tv_nsec != st_target.st_atim.tv_nsec ||
#endif
            st_src.st_mtim.tv_sec != st_target.st_mtim.tv_sec ||
            st_src.st_mtim.tv_nsec != st_target.st_mtim.tv_nsec) {
          create_attr(full_attr, &st_src);
        }
        else {
          /* no attribute file needed, remove it if present */
          unlink(full_attr);
        }
        free(full_attr);

        /* Try to move the link over the original file. */
        if (rename(tmpname, filename)) {
          DEBUG_("renaming hardlink from '%s' to '%s' failed", tmpname, filename);
          if (unlink(tmpname)) {
            ERR_("failed to delete link");
          }
          free(tmpname);
          UNLOCK(&dedup_database.lock);
          return NULL;
        }
        /* If filename and tmpname are the same inode already, rename() will
           succeed without removing tmpname, so we better do it ourselves.
           This happens frequently if redup is enabled. */
        unlink(tmpname);
        free(tmpname);

        UNLOCK(&dedup_database.lock);
        return dp->filename;
      }

    }
  }
  
  /* If we reach this point, we haven't found any duplicate for this file,
     so we add it as a new entry into the dedup database. */
  DEBUG_("unique file '%s', adding to dedup DB", filename);
  dedup_add(md5, filename);
  UNLOCK(&dedup_database.lock);
  return NULL;
}

/** Checks if an entry matching the given MD5 hash is in the database.
 * @param md5 MD5 hash.
 */
int dedup_db_has_md5(unsigned char *md5)
{
  dedup_t *dp;
  list_for_each_entry(dp, &dedup_database.head_md5[md5_to_hash(md5)], list_md5) {
    if (memcmp(md5, dp->md5, 16) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

/** Checks if an entry matching the given file name hash is in the database.
 * @param filename_hash File name hash.
 */
int dedup_db_has_filehash(unsigned int filename_hash)
{
  dedup_t *dp;
  list_for_each_entry(dp, &dedup_database.head_filename[filename_hash & DATABASE_HASH_MASK], list_filename_hash) {
    if (dp->filename_hash == filename_hash) {
      return TRUE;
    }
  }
  return FALSE;
}

/** Calculate the MD5 hash of the decompressed data in a given file.
 * @param name File name to be hashed.
 * @param md5 16-byte buffer the MD5 hash will be written to.
 */
int dedup_hash_file(const char *name, unsigned char *md5)
{
  MHASH mh = mhash_init(MHASH_MD5);
  int fd = open(name, O_RDONLY);
  if (fd < 0)
    return 1;

  /* check if this is a compressed file */
  int compressed;
  compressor_t *compr;
  void *handle = NULL;
  off_t size;
  const unsigned char magic[] = { 037, 0135, 0211 };
  unsigned char m[3];
  if (read(fd, m, 3) != 3 || memcmp(magic, m, 3)) {
    /* no magic bytes, this is an uncompressed file */
    lseek(fd, 0, SEEK_SET);
    compressed = FALSE;
  }
  else {
    lseek(fd, 0, SEEK_SET);
    compressed = TRUE;
    if (file_read_header_fd(fd, &compr, &size) == FAIL) {
      close(fd);
      return 1;
    }
    handle = compr->open(fd, "r");
    if (!handle) {
      close(fd);
      return 1;
    }
  }

  /* hash file contents */
  int count;
  int failed = 0;
  char buf[4096];
  for(;;) {
    if (!compressed)
      count = read(fd, buf, 4096);
    else {
      count = compr->read(handle, buf, 4096);
    }
    if (count < 0) {
      DEBUG_("read failed on '%s' while deduping", name);
      failed = 1;
      break;
    }
    if (count == 0)
      break;
    mhash(mh, buf, count);
    /* XXX: It would be good for performance to occasionally check
       file->status & CANCEL. */
  }

  mhash_deinit(mh, md5);
  DEBUG_("hashed %s to %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
         name, md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9], md5[10], md5[11], md5[12], md5[13], md5[14], md5[15]);

  if (compressed)
    compr->close(handle);
  else
    close(fd);

  return failed;
}

/** Attempt deduplication of file.
 * @param file File to be deduplicated.
 */
void do_dedup(file_t *file)
{
  NEED_LOCK(&file->lock);
  STAT_(STAT_DO_DEDUP);
  
  file->status |= DEDUPING;
  /* No need to keep the file under lock while calculating the hash; should
     it change in the meantime, we will be informed through file->status. */
  UNLOCK(&file->lock);
  
  /* Calculate MD5 hash. */
  unsigned char md5[16];
  int failed = dedup_hash_file(file->filename, md5);
  
  /* See if everything went fine. */
  LOCK(&file->lock);
  if (failed) {
    DEBUG_("deduping '%s' failed", file->filename);
    /* While this looks suspicious, it is not a deduplication error, so we
       don't have to do anything special. We do, however, have to mark the
       file as deduplicated because it may be re-added to the database again
       otherwise, causing an endless loop when unmounting. */
    file->deduped = TRUE;
    return;
  }
  if (!(file->status & CANCEL)) {
    /* No failure, no cancellation; let's link to identical file from dedup DB. */
    DEBUG_("MD5 for '%s': %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
           file->filename, md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8],
           md5[9], md5[10], md5[11], md5[12], md5[13], md5[14], md5[15]);
    const char *target = hardlink_file(md5, file->filename);
    if (target) {
        /* file linked to may have a different compressor */
        compressor_t *c = NULL;
        off_t s;
        if (file_read_header_name(target, &c, &s) < 0) {
          ERR_("failed to read '%s' header", target);
        }
        else {
          file->compressor = c;
        }
    }
    file->deduped = TRUE;
  }
  else {
    /* Acknowledge the cancellation. */
    DEBUG_("deduping '%s' cancelled", file->filename);
    file->status &= ~CANCEL;
    pthread_cond_broadcast(&file->cond);
  }
  file->status &= ~DEDUPING;
}

/** Reverse deduplication in case a hardlinked file is written to.
 * @param file File to be undeduplicated.
 */
int do_undedup(file_t *file)
{
  NEED_LOCK(&file->lock);
  DEBUG_("undeduping '%s'", file->filename);
  
  /* First, check if there is actually something to be done. */
  struct stat st;
  if (lstat(file->filename, &st) < 0)
    return 0;
  if (st.st_nlink < 2)
    return 0;
  
  struct stat attr_st;
  char *filename_attr = fuseattr_name(file->filename);
  if (lstat(filename_attr, &attr_st) < 0) {
    attr_st = st;
  }

  STAT_(STAT_DO_UNDEDUP);
  
  /* Check if we have enough space on the backing store to undedup. */
  struct statvfs stat;
  if(statvfs(file->filename, &stat) < 0) {
    goto out_eio;
  }
  if(stat.f_bsize * stat.f_bavail < st.st_size) {
          if(!(geteuid() == 0 && stat.f_bsize * stat.f_bfree >= st.st_size)) {
                  errno = ENOSPC;
                  free(filename_attr);
                  return FAIL;
          }
  }
  
  /* XXX: Is this actually necessary? After all, we create an identical
     copy. */
  dedup_discard(file);
  
  /* Behold the shitload of code it takes to safely copy a file on Unix... */
  int fd_out;
  char *temp = file_create_temp(&fd_out);
  if (fd_out == -1) {
    ERR_("undedup: failed to create temp file");
    goto out_eio;
  }
  int fd_in = open(file->filename, O_RDONLY);
  if (fd_in == -1) {
    ERR_("undedup: failed to open input file '%s'", file->filename);
    close(fd_out);
    unlink(temp);
    free(temp);
    goto out_eio;
  }
  char buf[4096];
  for(;;) {
    int count = read(fd_in, buf, 4096);
    if (count < 0) {
      ERR_("undedup: read() failed on input file '%s'", file->filename);
      close(fd_in);
      close(fd_out);
      unlink(temp);
      free(temp);
      goto out_eio;
    }
    else if (count == 0) /* EOF */
      break;
    if (write(fd_out, buf, count) != count) {
      ERR_("undedup: write() failed on output file '%s'", temp);
      close(fd_in);
      close(fd_out);
      unlink(temp);
      free(temp);
      goto out_eio;
    }
  }
  close(fd_in);
  /* fix owner and mode of the new file */
  fchown(fd_out, attr_st.st_uid, attr_st.st_gid);
  fchmod(fd_out, attr_st.st_mode);
  close(fd_out);
  
  /* Remove deduplicated link... */
  if (unlink(file->filename) < 0) {
    ERR_("undedup: failed to unlink '%s'", file->filename);
    unlink(temp);
    free(temp);
    goto out_eio;
  }
  /* ...and replace it with the new copy. */
  DEBUG_("renaming '%s' to '%s'", temp, file->filename);
  if (rename(temp, file->filename) < 0) {
    ERR_("undedup: failed to rename temp file '%s' to '%s'", temp, file->filename);
    free(temp);
    goto out_eio;
  }
  /* fix timestamps */
  struct utimbuf utbuf;
  utbuf.actime = attr_st.st_atime;
  utbuf.modtime = attr_st.st_mtime;
  if (utime(file->filename, &utbuf) < 0) {
    CRIT_("utime failed on '%s'!", file->filename);
    goto out_eio;
  }

  /* remove attribute file, if any */
  unlink(filename_attr);
  free(filename_attr);
  
  errno = 0;
  return 0;

out_eio:
  errno = EIO;
  free(filename_attr);
  return FAIL;
}

/** Remove an entry from the deduplication DB.
 * This function must be called whenever a file's contents are modified.
 * @param file File to be removed.
 */
void dedup_discard(file_t *file)
{
  NEED_LOCK(&file->lock);

  if (file->deduped == FALSE) {
    /* This entry is not in the DB anyway. */
    return;
  }

  file->deduped = FALSE;
  DEBUG_("dedup_discard file '%s'", file->filename);
  STAT_(STAT_DEDUP_DISCARD);
  dedup_t* dp;
  int len;
  unsigned int hash = gethash(file->filename, &len);
  list_for_each_entry(dp, &dedup_database.head_filename[hash & DATABASE_HASH_MASK], list_filename_hash) {
    if (dp->filename_hash == hash && !strcmp(dp->filename, file->filename)) {
      DEBUG_("found file '%s', discarding", file->filename);
      list_del(&dp->list_filename_hash);
      list_del(&dp->list_md5);
      dedup_database.entries--;
      free(dp->filename);
      free(dp);
      return;
    }
  }
}

#define DEDUP_MAGIC "DEDUP"
#define DEDUP_MAGIC_SIZE (sizeof(DEDUP_MAGIC) - 1)
#define DEDUP_VERSION 2

/** Initialize the deduplication database.
 */
void dedup_init_db(void)
{
  int i;
  for (i = 0; i < DATABASE_HASH_SIZE; i++) {
    dedup_database.head_filename[i].prev = dedup_database.head_filename[i].next = &(dedup_database.head_filename[i]);
    dedup_database.head_md5[i].prev = dedup_database.head_md5[i].next = &(dedup_database.head_md5[i]);
  }
}

/** Load the dedup DB saved when last mounted.
 * @param root Path to the backing filesystem's root.
 */
void dedup_load(const char *root)
{
  dedup_init_db();
  
  dedup_t *dp;

  /* we're not in the backing FS root yet, so we need to compose an
     absolute path */
  char fn[strlen(root) + 1 + strlen(DEDUP_DB_FILE) + 1];
  sprintf(fn, "%s/%s", root, DEDUP_DB_FILE);
  FILE *db_fp = fopen(fn, "r");
  if (!db_fp) {
    if (errno == ENOENT) {
      DEBUG_("no dedup DB found");
    }
    else {
      ERR_("failed to open dedup DB for reading: %s", strerror(errno));
    }
    return;
  }
  
  /* check header */
  char header[DEDUP_MAGIC_SIZE];
  uint16_t version;
  if (fread(header, DEDUP_MAGIC_SIZE, 1, db_fp) != 1)
    goto out;
  if (memcmp(header, DEDUP_MAGIC, DEDUP_MAGIC_SIZE)) {
    ERR_("dedup DB magic not found");
    goto out;
  }
  if (fread(&version, 2, 1, db_fp) != 1)
    goto out;
  if (version != DEDUP_VERSION) {
    DEBUG_("verion mismatch, ignoring dedup DB");
    goto out;
  }
  
  /* load data */
  uint32_t filename_length;
  /* every entry starts with the length of the filename */
  while (fread(&filename_length, 4, 1, db_fp) == 1) {
    /* allocate DB entry */
    dp = (dedup_t *)malloc(sizeof(dedup_t));
    /* allocate filename */
    dp->filename = (char *)malloc(filename_length + 1);
    /* read filename */
    if (fread(dp->filename, 1, filename_length, db_fp) != filename_length) {
      ERR_("failed to load filename of %d characters", filename_length);
      free(dp->filename);
      free(dp);
      goto out;
    }
    dp->filename[filename_length] = 0;	/* string termination */

    /* read MD5 hash */
    if (fread(dp->md5, 16, 1, db_fp) != 1) {
      ERR_("failed to load MD5 for %s", dp->filename);
      free(dp->filename);
      free(dp);
      goto out;
    }

    /* ignore files in exluded paths */
    if (is_excluded(dp->filename)) {
      free(dp->filename);
      free(dp);
      continue;
    }

    int len;
    dp->filename_hash = gethash(dp->filename, &len);

    /* add to in-core dedup DB */
    list_add_tail(&dp->list_filename_hash, &dedup_database.head_filename[dp->filename_hash & DATABASE_HASH_MASK]);
    list_add_tail(&dp->list_md5, &dedup_database.head_md5[md5_to_hash(dp->md5)]);
    dedup_database.entries++;
  }
  
  fclose(db_fp);
  /* Very soon, we will change this filesystem without updating the dedup DB,
     and it would be dangerous to keep an out-of-date database on disk. */
  unlink(fn);
  return;
out:
  fclose(db_fp);
  unlink(fn); /* it's broken */
  ERR_("failed to load dedup DB");
}

static int dedup_db_write_header(FILE *db_fp)
{
  if (fwrite("DEDUP", DEDUP_MAGIC_SIZE, 1, db_fp) == EOF)
    return 1;
  uint16_t version = DEDUP_VERSION;
  if (fwrite(&version, 1, 2, db_fp) == EOF)
    return 1;
  return 0;
}

static int dedup_db_write_entry(FILE *db_fp, const char *filename, unsigned char *md5)
{
  /* length of filename */
  uint32_t len = strlen(filename);
  if (fwrite(&len, 1, 4, db_fp) == EOF)
    return 1;
  /* filename */
  if (fputs(filename, db_fp) == EOF)
    return 1;
  /* MD 5 hash */
  if (fwrite(md5, 1, 16, db_fp) == EOF)
    return 1;
  /* no need to save filename hash, it can be easily regenerated
     when loading */
  return 0;
}

/** Save the current dedup DB.
 */
void dedup_save(void)
{
  dedup_t *dp;
  /* assuming that cwd is the backing filesystem's root */
  FILE* db_fp = fopen(DEDUP_DB_FILE, "w");
  if (!db_fp) {
    ERR_("failed to open dedup DB for writing");
    return;
  }

  /* write header */
  if (dedup_db_write_header(db_fp))
    goto out;
  
  /* write data */
  int i;
  for (i = 0; i < DATABASE_HASH_SIZE; i++) {
    list_for_each_entry(dp, &dedup_database.head_filename[i], list_filename_hash) {
      if (dedup_db_write_entry(db_fp, dp->filename, dp->md5))
        goto out;
    }
  }

  fclose(db_fp);
  return;
out:
  fclose(db_fp);
  unlink(DEDUP_DB_FILE);
  ERR_("failed to write dedup DB");
}

/** Rename a file in the dedup database.
 * @param from Original file.
 * @param to Target file.
 */
void dedup_rename(file_t *from, file_t *to)
{
  /* propagate the dedup status to the new file */
  NEED_LOCK(&to->lock);

  /* after the rename, the "to" file will no longer exist, so we have to
     remove it from the dedup DB */
  dedup_discard(to);

  DEBUG_("to->deduped %d, from->deduped %d", to->deduped, from->deduped);
  to->deduped = from->deduped;

  /* search for from file in the dedup DB and remove it from there */
  dedup_t *dp;
  int found = 0;
  DEBUG_("dedup renaming '%s'/%08x to '%s'/%08x", from->filename, from->filename_hash, to->filename, to->filename_hash);
  LOCK(&dedup_database.lock);
  list_for_each_entry(dp, &dedup_database.head_filename[from->filename_hash & DATABASE_HASH_MASK], list_filename_hash) {
    if (dp->filename_hash == from->filename_hash && !strcmp(dp->filename, from->filename)) {
      DEBUG_("found file '%s' to rename", from->filename);
      list_del(&dp->list_filename_hash);
      list_del(&dp->list_md5);
      found = 1;
      break;
    }
  }

  /* it's quite possible that an entry is not in the dedup DB yet, it
     may not have been released yet */
  if (found) {
    /* update with the new file name and add it back to the database */
    /* new filename */
    free(dp->filename);
    dp->filename = strdup(to->filename);
    dp->filename_hash = to->filename_hash;
    /* add to DB again */
    list_add_tail(&dp->list_filename_hash, &dedup_database.head_filename[dp->filename_hash & DATABASE_HASH_MASK]);
    list_add_tail(&dp->list_md5, &dedup_database.head_md5[md5_to_hash(dp->md5)]);
  }
  UNLOCK(&dedup_database.lock);
}

/** Stat a file, overriding some stats with those of an attribute file if any
 * @param full Full path to file.
 * @param stbuf stat() buffer
 */
int dedup_sys_getattr(const char *full, struct stat *stbuf)
{
  int res;
  res = lstat(full, stbuf);
  if (res < 0)
    return FAIL;
  DEBUG_("'%s' mtime %zd", full, stbuf->st_mtime);
  /* check if we have an attribute file */
  char *full_attr = fuseattr_name(full);
  struct stat st_attr;
  if (lstat(full_attr, &st_attr) == 0) {
    DEBUG_("'%s' mtime %zd", full_attr, st_attr.st_mtime);
    /* override the stats from the actual file with those
       from the attribute file */
    stbuf->st_uid = st_attr.st_uid;
    stbuf->st_gid = st_attr.st_gid;
    stbuf->st_atime = st_attr.st_atime;
    stbuf->st_atim = st_attr.st_atim;
    stbuf->st_mtime = st_attr.st_mtime;
    stbuf->st_mtim = st_attr.st_mtim;
    stbuf->st_mode = st_attr.st_mode;
  }
  free(full_attr);
  return res;
}

/** chown() a file, making sure linked files are unchanged by creating
 * and/or using an attribute file if necessary.
 * @param full Full path to file.
 * @param uid user ID
 * @param gid group ID
 */
int dedup_sys_chown(const char *full, uid_t uid, gid_t gid)
{
  struct stat st_attr;
  struct stat st_file;

  if (lstat(full, &st_file) < 0)
    return FAIL;
  DEBUG_("'%s' mtime %zd", full, st_file.st_mtime);
  if (!S_ISREG(st_file.st_mode))
    return lchown(full, uid, gid);

  int res;
  char *full_attr = fuseattr_name(full);
  if (lstat(full_attr, &st_attr) == 0) {
     DEBUG_("'%s' mtime %zd", full_attr, st_attr.st_mtime);
    /* there is an attribute file */
    if (st_file.st_uid == uid &&
        st_file.st_gid == gid &&
#ifdef EXACT_ATIME
        st_file.st_atim.tv_sec == st_attr.st_atim.tv_sec &&
        st_file.st_atim.tv_nsec == st_attr.st_atim.tv_nsec &&
#endif
        st_file.st_mtim.tv_sec == st_attr.st_mtim.tv_sec &&
        st_file.st_mtim.tv_nsec == st_attr.st_mtim.tv_nsec &&
        st_file.st_mode == st_attr.st_mode) {
      /* this attr file is no longer necessary */
      unlink(full_attr);
      res = 0;
      /* XXX: what about ctime? */
    }
    else {
      /* this attr file is required, update it */
      res = lchown(full_attr, uid, gid);
    }
  }
  else {
    /* no existing attribute file, check if we need one */
    if (st_file.st_nlink > 1 && (st_file.st_uid != uid || st_file.st_gid != gid)) {
      /* we do */
      if (create_attr(full_attr, &st_file) < 0)
        return FAIL;
      res = lchown(full_attr, uid, gid);
    }
    else {
      /* we don't */
      res = lchown(full, uid, gid);
    }
  }
  free(full_attr);
  return res;
}

/** chmod() a file, making sure linked files are unchanged by creating
 * and/or using an attribute file if necessary.
 * @param full Full path to file.
 * @param mode File mode.
 */
int dedup_sys_chmod(const char *full, mode_t mode)
{
  struct stat st_attr;
  struct stat st_file;

  if (lstat(full, &st_file) < 0)
    return FAIL;
  DEBUG_("'%s' mtime %zd", full, st_file.st_mtime);
  if (!S_ISREG(st_file.st_mode))
    return chmod(full, mode);

  int res;
  char *full_attr = fuseattr_name(full);
  if (lstat(full_attr, &st_attr) == 0) {
    DEBUG_("'%s' mtime %zd", full_attr, st_attr.st_mtime);
    /* there is an attribute file */
    if (st_file.st_uid == st_attr.st_uid &&
        st_file.st_gid == st_attr.st_gid &&
#ifdef EXACT_ATIME
        st_file.st_atim.tv_sec == st_attr.st_atim.tv_sec &&
        st_file.st_atim.tv_nsec == st_attr.st_atim.tv_nsec &&
#endif
        st_file.st_mtim.tv_sec == st_attr.st_mtim.tv_sec &&
        st_file.st_mtim.tv_nsec == st_attr.st_mtim.tv_nsec &&
        st_file.st_mode == mode) {
      /* this attr file is no longer necessary */
      unlink(full_attr);
      res = 0;
      /* XXX: what about ctime? */
    }
    else {
      /* this attr file is required, update it */
      res = chmod(full_attr, mode);
    }
  }
  else {
    /* no existing attribute file, check if we need one */
    if (st_file.st_nlink > 1 && st_file.st_mode != mode) {
      /* we do */
      if (create_attr(full_attr, &st_file) < 0)
        return FAIL;
      res = chmod(full_attr, mode);
    }
    else {
      /* we don't */
      res = chmod(full, mode);
    }
  }
  free(full_attr);
  return res;
}

/** Change file atime/mtime, making sure linked files remain unchanged by
 * creating and/or using an attribute file if necessary.
 * @param full Full path to file.
 * @param tv atime/mtime array
 */
int dedup_sys_utime(const char *full, struct timeval *tv)
{
  struct stat st_attr;
  struct stat st_file;

  DEBUG_("supposed to set '%s' to mtime %zd", full, tv[1].tv_sec);
  if (lstat(full, &st_file) < 0)
    return FAIL;
  DEBUG_("'%s' mtime %zd", full, st_file.st_mtime);
  if (!S_ISREG(st_file.st_mode))
    return lutimes(full, tv);

  int res;
  char *full_attr = fuseattr_name(full);
  if (lstat(full_attr, &st_attr) == 0) {
    DEBUG_("'%s' mtime %zd", full_attr, st_attr.st_mtime);
    /* there is an attribute file */
    if (st_file.st_uid == st_attr.st_uid &&
        st_file.st_gid == st_attr.st_gid &&
#ifdef EXACT_ATIME
        st_file.st_atim.tv_sec == tv[0].tv_sec &&
        st_file.st_atim.tv_nsec == tv[0].tv_usec * 1000 &&
#endif
        st_file.st_mtim.tv_sec == tv[1].tv_sec &&
        st_file.st_mtim.tv_nsec == tv[1].tv_usec * 1000 &&
        st_file.st_mode == st_attr.st_mode) {
      /* this attr file is no longer necessary */
      unlink(full_attr);
      res = 0;
      /* XXX: what about ctime? */
    }
    else {
      /* this attr file is required, update it */
      res = lutimes(full_attr, tv);
    }
  }
  else {
    /* no existing attribute file, check if we need one */
    if (st_file.st_nlink > 1 && (
#ifdef EXACT_ATIME
        st_file.st_atim.tv_sec != tv[0].tv_sec ||
        st_file.st_atim.tv_nsec != tv[0].tv_usec * 1000 ||
#endif
        st_file.st_mtim.tv_sec != tv[1].tv_sec ||
        st_file.st_mtim.tv_nsec != tv[1].tv_usec * 1000
       )) {
      /* we do */
      if (create_attr(full_attr, &st_file) < 0)
        return FAIL;
      res = lutimes(full_attr, tv);
    }
    else {
      /* we don't */
      res = lutimes(full, tv);
    }
  }
  free(full_attr);
  return res;
}

/** Unlink a file, making sure associated attribute files are removed as well.
 * @param full Full path to file.
 */
int dedup_sys_unlink(const char *full)
{
  char *full_attr = fuseattr_name(full);
  unlink(full_attr);
  free(full_attr);
  return unlink(full);
}

/** Rename a file, working around silly rename() semantics with regard to
 * hard links and making sure any attribute files are renamed as well.
 * @param full_from Full path to file to be renamed.
 * @param full_to Full path to destination.
 */
int dedup_sys_rename(const char *full_from, const char *full_to)
{
  int res;
  /* Work around pants-on-head retarded rename() semantics:
     "If oldpath and newpath are existing hard links referring
     to the same file, then rename() does nothing, and returns
     a success status." */
  struct stat st_from;
  /* Do we have a source file? */
  if (lstat(full_from, &st_from) == 0) {
    DEBUG_("'%s' mtime %zd", full_from, st_from.st_mtime);
    /* Only regular files get special treatment. */
    if (!S_ISREG(st_from.st_mode))
      return rename(full_from, full_to);

    /* Does it have more than one link? */
    if (st_from.st_nlink > 1) {
      /* Do we have a destination file? */
      struct stat st_to;
      if (lstat(full_to, &st_to) == 0) {
        DEBUG_("'%s' mtime %zd", full_to, st_to.st_mtime);
        /* Are the files pointing to the same inode? */
        if (st_from.st_ino == st_to.st_ino) {
          /* Unlink the source file instead of calling rename(). */
          res = unlink(full_from);
          if (res < 0)
            return res;
          /* Any attribute files will have to be renamed, though. */
          char *full_from_attr = fuseattr_name(full_from);
          char *full_to_attr = fuseattr_name(full_to);
          /* These files may or may not exist, so we don't check for errors. */
          rename(full_from_attr, full_to_attr);
          free(full_from_attr);
          free(full_to_attr);
          return res;
        }
      }
    }
  }
  /* No hard link, normal procedure. */
  res = rename(full_from, full_to);
  if (res < 0)
    return res;
  char *full_from_attr = fuseattr_name(full_from);
  char *full_to_attr = fuseattr_name(full_to);
  /* These files may or may not exist, so we don't check for errors. */
  rename(full_from_attr, full_to_attr);
  free(full_from_attr);
  free(full_to_attr);
  return res;
}
