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

#ifndef _REPIO_COMMON_H_
#define _REPIO_COMMON_H_

#include <stdio.h>
#include <sched.h>

#include <adt/list.h>
#include <adt/hash_table.h>

#include "common.h"

#define MODE_UNDEF -666

/** The following structure is used to allow insertion of several different types of events/operations into
 * one list. Given a list, one then need to recognize the type of the the structure. If we keep the same offset
 * from the begging of the structure of "item_t item" and "char type", one can retrieve the information using
 * this common item, and then, according to the type, retrieves the true structure.
 *
 * It IS somehow nasty, but other solutions (having two lists, write type information to the item_t itself) seem
 * to me worse.
 */

typedef struct common_op_item {
	item_t item; /* make me item of the list - it MUST be defined first */
	char type;
} common_op_item_t;

/* These are to cover all structures with item_t, so they can be member of the list.
 */

typedef struct read_item {
	item_t item; /* make me item of the list - it MUST be defined first */
	char type; /* and this MUST be defined as second */
	read_op_t o;
} read_item_t;

typedef struct write_item {
	item_t item; /* make me item of the list - it MUST be defined first */
	char type; /* and this MUST be defined as second */
	write_op_t o;
} write_item_t;

typedef struct pipe_item {
	item_t item;
	char type;
	pipe_op_t o;
} pipe_item_t;

typedef struct mkdir_item {
	item_t item;
	char type;
	mkdir_op_t o;
} mkdir_item_t;

typedef struct rmdir_item {
	item_t item;
	char type;
	rmdir_op_t o;
} rmdir_item_t;

typedef struct clone_item {
	item_t item;
	char type;
	clone_op_t o;
} clone_item_t;

typedef struct dup_item {
	item_t item;
	char type;
	dup_op_t o;
} dup_item_t;

typedef struct open_item {	
	item_t item;
	char type;
	open_op_t o;
} open_item_t;

typedef struct close_item {	
	item_t item;
	char type;
	close_op_t o;
} close_item_t;

typedef struct unlink_item {	
	item_t item;
	char type;
	unlink_op_t o;
} unlink_item_t;

typedef struct llseek_item {	
	item_t item;
	char type;
	llseek_op_t o;
} llseek_item_t;

typedef struct lseek_item {	
	item_t item;
	char type;
	lseek_op_t o;
} lseek_item_t;

typedef struct access_item {	
	item_t item;
	char type;
	access_op_t o;
} access_item_t;

typedef struct stat_item {	
	item_t item;
	char type;
	stat_op_t o;
} stat_item_t;

typedef struct socket_item {	
	item_t item;
	char type;
	socket_op_t o;
} socket_item_t;

/* Functions for creating new structures
 */

write_item_t * new_write_item();
read_item_t * new_read_item();
mkdir_item_t * new_mkdir_item();
rmdir_item_t * new_rmdir_item();
dup_item_t * new_dup_item();
clone_item_t * new_clone_item();
pipe_item_t * new_pipe_item();
open_item_t * new_open_item();
close_item_t * new_close_item();
unlink_item_t * new_unlink_item();
llseek_item_t * new_llseek_item();
lseek_item_t * new_lseek_item();
access_item_t * new_access_item();
stat_item_t * new_stat_item();
socket_item_t * new_socket_item();

int remove_items(list_t * list);

int strccount(char * str, char c);


int read_integer(char * str, long long * num);
int read_clone_flags(char * flags);
int read_open_flags(char * flags);
int read_seek_flag(char * flag);
int read_access_flags(char * str);
int read_dup3_flags(char * str);
struct int32timeval read_time(char * timestr);
int32_t read_duration(char * timestr);
#endif

