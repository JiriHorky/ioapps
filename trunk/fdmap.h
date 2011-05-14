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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _FDMAP_H_
#define _FDMAP_H_

#include "adt/hash_table.h"
#include "common.h"

#define FT_SPEC -1 ///< special file type for std in/ou/err (they are in fact reg. files)


/** This structure serves for mapping among file descriptors of the original process and my filedescriptors,
 * as they may differ.
 */

typedef struct fd_map {
	int32_t my_fd;
	mode_t type;
	uint64_t cur_pos; ///< current position in the file. Usefull when simulating.
	struct int32timeval time_open; ///< when this file was opened
	char name[MAX_STRING]; ///< name of the file
	int created; ///< was it newly created or not?
	int32_t parent_fds[MAX_PARENT_IDS]; ///< array of parent fd numbers - usefull when deleting duplicated fd
	int32_t last_par_index;
} fd_map_t;

typedef struct fd_item {
	item_t item;
	int32_t old_fd; ///< key
	fd_map_t * fd_map;
} fd_item_t;

typedef struct fd_usage {
	item_t item;
	int32_t my_fd; ///< key
	int32_t usage; ///< how many processes has this fd in use
} fd_usage_t;


/** Structure used in the list of hashmaps of fd mappings for each process.
 */

typedef struct process_hash_item {
	item_t item; // I am part of the list
	hash_table_t * ht; // file descriptor table
	int32_t pid; 
} process_hash_item_t;

hash_table_t * get_process_ht(hash_table_t * fd_mappings, int32_t pid);
item_t * new_process_ht(int32_t pid);
hash_table_t * duplicate_process_ht(hash_table_t * h, hash_table_t * usage_map);
void delete_process_ht(hash_table_t * fd_mappings, int32_t pid);

inline void increase_fd_usage(hash_table_t * h, int32_t fd);
inline int decrease_fd_usage(hash_table_t * h, int32_t fd);
inline fd_usage_t * new_fd_usage();
void delete_fd_usage(fd_usage_t * fd_usage);

inline fd_item_t * new_fd_item();
void fd_item_remove_fd_map(item_t * item);
void delete_fd_item(fd_item_t * item);
void dump_fd_item(fd_item_t * fd_item);
void dump_fd_list_item(item_t * it);
void dump_process_hash_list_item(item_t * it);
void insert_parent_fd(fd_item_t * fd_item, int32_t fd);
int delete_parent_fd(fd_item_t * fd_item, int32_t fd);

#endif

