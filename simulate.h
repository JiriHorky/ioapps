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

#ifndef _SIMULATE_H_
#define _SIMULATE_H_

/** @file simulate.h
 *
 * Takes care of noting of every read and write operation, based on the filename.
 *
 * It uses hash_table, where the key is a string and data are sim_item_t strutcs, which for every filename contain
 * list of operations (struct rw_op).
 */

#include <time.h>
#include "fdmap.h"
#include "in_common.h"
#include "adt/hash_table.h"

typedef struct rw_op {
	item_t item;
	int64_t offset;
	uint64_t size;
	struct int32timeval start; ///< start of operation
	int32_t dur; ///< duration of operation
} rw_op_t;


typedef struct sim_item_t {
	item_t item;
	char name[MAX_STRING];
	int created;
	struct int32timeval time_open;
	list_t list;
} sim_item_t;

hash_table_t * simulate_get_map_read();
hash_table_t * simulate_get_map_write();
inline int simulate_get_open_fd();
inline void simulate_sendfile(fd_item_t * in_fd_item, fd_item_t * out_fd_item, sendfile_item_t * op_it);
inline void simulate_read(fd_item_t * fd_item, read_item_t * op_it);
inline void simulate_write(fd_item_t * fd_item, write_item_t * op_it);
inline void simulate_pread(fd_item_t * fd_item, pread_item_t * op_it);
inline void simulate_pwrite(fd_item_t * fd_item, pwrite_item_t * op_it);
void simulate_access(access_op_t * op_it);
void simulate_stat(stat_op_t * op_it);
void simulate_mkdir(mkdir_op_t * op_it);
void simulate_rmdir(rmdir_op_t * op_it);
void simulate_unlink(unlink_op_t * op_it);
void simulate_creat(open_op_t * op_it);
void simulate_init(int mode);
void simulate_finish();
void simulate_list_files();
void simulate_check_files();
void simulate_prepare_files();

#endif
