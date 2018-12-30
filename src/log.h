/*
 * Copyright (C) 2005 Milan Svoboda <milan.svoboda@centrum.cz>
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

#ifndef LOG_H
#define LOG_H

// ------------------- Logs -------------------------

#include <syslog.h>

#ifndef DEBUG
#define DEBUG_ON
#define DEBUG_OFF
#define DEBUG_(fmt,...)
#else
#include <stdio.h>
extern int _debug_on;
#define DEBUG_ON (_debug_on = 1);
#define DEBUG_OFF (_debug_on = 0);
#define DEBUG_(fmt,...) \
        if(_debug_on) fprintf(stderr, "DEBUG %s %d: " fmt "\n", __PRETTY_FUNCTION__, __LINE__, ## __VA_ARGS__);
#endif

#ifdef DEBUG
#define barf(t,x,...) fprintf(stderr, x "\n", ## __VA_ARGS__)
#else
#define barf(t,x...) syslog(t, x)
#endif

#define WARN_(fmt,...)				\
	barf(LOG_WARNING, "WARN %s %d: " fmt, __PRETTY_FUNCTION__, __LINE__, ## __VA_ARGS__);

#define ERR_(fmt,...)				\
	barf(LOG_ERR, "ERR %s %d: " fmt, __PRETTY_FUNCTION__, __LINE__, ## __VA_ARGS__);

#define MAX_ERRORS_PER_FILE 100
#define FILEERR_(file, fmt, ...)		\
	if (file->errors_reported == MAX_ERRORS_PER_FILE) { file->errors_reported++; ERR_("too many errors for file %s, squelching", file->filename); }	\
	else if (file->errors_reported < MAX_ERRORS_PER_FILE) { file->errors_reported++; ERR_(fmt, ## __VA_ARGS__); }

#define CRIT_(fmt,...)				\
	barf(LOG_CRIT, "CRIT: %s %d: " fmt, __PRETTY_FUNCTION__, __LINE__, ## __VA_ARGS__);
	
#define INFO_(fmt,...)				\
	barf(LOG_INFO, "INFO: %s %d: " fmt, __PRETTY_FUNCTION__, __LINE__, ## __VA_ARGS__);

// ------------------- Statistics -------------------------

#ifdef DEBUG

#include <pthread.h>

#define STAT_GETATTR			1
#define STAT_UNLINK			2
#define STAT_RENAME			3
#define STAT_TRUNCATE			4
#define STAT_OPEN			5
#define STAT_RELEASE			6
#define STAT_READ			7
#define STAT_WRITE			8
#define STAT_DECOMPRESS			9
#define STAT_BACKGROUND_COMPRESS	10
#define STAT_BACKGROUND_COMPRESS_QUEUE	11
#define STAT_DIRECT_OPEN_ALLOC		12
#define STAT_DIRECT_READ		13
#define STAT_DIRECT_WRITE		14
#define STAT_FALLBACK			15
#define STAT_COMPRESS			16
#define STAT_DO_DEDUP			17
#define STAT_DO_UNDEDUP			18
#define STAT_DEDUP_DISCARD		19

#define STAT_MAX			19

#define STAT_(index)	statistics[index-1].count++;

typedef struct
{
	unsigned int count;
	const char *name;
} statistic_t;

// The statistics array is not protected with mutex, this is only
// statistics, it's nothing too important...
//
extern volatile statistic_t statistics[];

extern void statistics_print(void);

#else

#define STAT_(index)

static inline void statistics_print(void) {}

#endif

#endif
