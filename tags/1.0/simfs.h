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

#ifndef _SIM_FS_
#define _SIM_FS_


/** @file simfs.h
 *
 * The SimFS module is inteded for simulating FS system calls such as access/mkdir/unlink/creat and maybe others in future.
 * The purpose of it is tracking down inconsistencies with current file system and file system used by straced process - it
 * should warn you about files/directories that are not present and are needed by straced process (e.g. it tries to create
 * file in directory that doesn't exist). It also warns about files/directories that exist but should not (i.e. system call
 * in previous process failed, but wouldn't fail now).
 *
 * It works by dynamic scanning of actual file system when a call to access it is needed. That said, it didn't create whole
 * file-system structure, but only parts that are actualy needed. 
 *
 * If a system call fails, (e.g. mkdir in directory that doesn't exist yet), the SimFS returns error and creates appropriate 
 * records as it would exist. This way, one error doesn't cause chain of other related problems.
 *
 */

#include "adt/fs_trie.h"
#include "common.h"

#define SIMFS_OK 0
#define SIMFS_PHYS -1
#define SIMFS_VIRT -1
#define SIMFS_ENOENT 1
#define SIMFS_EENT 2


typedef struct simfs {
	char type; ///< whether I am directory or file
	char physical; ///< whether it exists on disk
	char created; ///< whether this file was created by replicating (ie by creat/mkdir call)
	uint64_t phys_size; //physical size on the disk (+ something, in case of writes), or -1 if virtual only
	uint64_t virt_size;
	trie_node_t node; ///< I am part of the node	
} simfs_t;

void simfs_init();
void simfs_finish();
void simfs_dump();
int simfs_stat(stat_op_t * stat_op);
int simfs_access(access_op_t * access_op);
int simfs_mkdir(mkdir_op_t * mkdir_op);
int simfs_rmdir(rmdir_op_t * rmdir_op);
int simfs_unlink(unlink_op_t * unlink_op);
int simfs_creat(open_op_t * open_op);
int simfs_has_file(const char * name);
simfs_t * simfs_find(const char * name);
void simfs_apply(void (* function)(simfs_t * item));
void simfs_apply_full_name(void (* function)(simfs_t * item, char * full_name));
int simfs_is_file(simfs_t * simfs);

#endif
