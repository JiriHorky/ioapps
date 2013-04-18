/* IOapps, IO profiler and IO traces replayer

    Copyright (C) 2010 Jiri Horky <jiri.horky@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _STATS_H_
#define _STATS_H_

/** @file stats.h
 *
 * Counts statistics for every syscall.
 *
 * It uses hash_table, where the key is a string and data are stats_item_t strutcs, which for every syscall contain
 * count of their occurence.
 */

#include <time.h>
#include "in_common.h"
#include "adt/hash_table.h"

#define MAX_OPERATION 30

typedef struct statistic_item {
	char operation[MAX_OPERATION];
	uint64_t count;
	double sumdur; ///< total duration of this type of operation in ms
	item_t item;
} statistic_item_t;

void stats_add_op(const char * line, const char * operation, int32_t duration);
void stats_print();
void stats_init();
#endif
