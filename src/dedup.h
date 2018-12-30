#include <stdio.h>
#include <sys/stat.h>
#include "structs.h"

const char *hardlink_file(unsigned char *md5, const char *filename);

void do_dedup(file_t *file);
int do_undedup(file_t *file);
void dedup_discard(file_t *file);
void dedup_rename(file_t *from, file_t *to);

void dedup_load(const char *dir);
void dedup_save();

int dedup_hash_file(const char *name, unsigned char *md5);
void dedup_add(unsigned char *md5, const char *filename);
int dedup_db_has_md5(unsigned char *md5);
int dedup_db_has_filehash(unsigned int filename_hash);
void dedup_init_db(void);

int dedup_sys_getattr(const char *full, struct stat *stbuf);
int dedup_sys_chown(const char *full, uid_t uid, gid_t gid);
int dedup_sys_chmod(const char *full, mode_t mode);
int dedup_sys_utime(const char *full, struct timeval *tv);
int dedup_sys_unlink(const char *full);
int dedup_sys_rename(const char *full_from, const char *full_to);
