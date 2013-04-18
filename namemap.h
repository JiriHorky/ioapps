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

#ifndef _NAMEMAP_H_
#define _NAMEMAP_H_

/** @file namemap.h
 *
 * Takes care of mapping file names to null string (when given file shoud be ignores) or to another filename.
 *
 * It uses hash_table, where the key is an original filename and data new filenames.
 */

#include <time.h>
#include "adt/hash_table.h"
#include "adt/list.h"

typedef struct namemap_item {
	item_t item;
	char old_name[MAX_STRING];
	char new_name[MAX_STRING];
} namemap_item_t;

int namemap_init(char *ifilename, char *mfilename);
char * namemap_get_name(char * name);
void namemap_finish();
#endif
