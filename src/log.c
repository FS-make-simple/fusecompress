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

#ifdef DEBUG

#include "log.h"

volatile statistic_t statistics[STAT_MAX + 2] = {
	{ 0, "getattr", },
	{ 0, "unlink", },
	{ 0, "rename", },
	{ 0, "truncate", },
	{ 0, "open", },
	{ 0, "release", },
	{ 0, "read", },
	{ 0, "write", },
	{ 0, "decompress", },
	{ 0, "background compress", },
	{ 0, "background compress queue", },
	{ 0, "direct open alloc", },
	{ 0, "direct read", },
	{ 0, "direct write", },
	{ 0, "fallback", },
	{ 0, "compress", },
	{ 0, "do dedup", },
	{ 0, "do undedup", },
	{ 0, "dedup discard", },
	{ 0, "", },
	{ 0, "", },
};

void statistics_print(void)
{
	int i;

	DEBUG_("Statistics:");

	for (i = 0; i < STAT_MAX; i += 3)
	{
		DEBUG_("\t%s: %d, %s: %d, %s: %d",
			statistics[i].name, statistics[i].count,
			statistics[i+1].name, statistics[i+1].count,
			statistics[i+2].name, statistics[i+2].count);
	}
}

#endif
