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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <common.h>
#include "in_strace.h"
#include "in_common.h"
#include "adt/list.h"
#include "stats.h"

static int ht_compare_isyscall(key_t *key, item_t *item) {
	isyscall_t * isyscall;

	isyscall = hash_table_entry(item, isyscall_t, item);
	return isyscall->pid == *key;
}

static inline void ht_remove_callback_isyscall(item_t * item) {
	isyscall_t * isyscall = hash_table_entry(item, isyscall_t, item);	
	free(isyscall);
	return;
}

/** hash table operations. */
static hash_table_operations_t ht_ops_isyscall = {
	.hash = ht_hash_int,
	.compare = ht_compare_isyscall,
	.remove_callback = ht_remove_callback_isyscall /* = NULL if not used */
};

/** This function returns position of coma after second quota in the list. This is usefull when parsing
 * file reads and writes, that was aquired with strace -sX, where X is not zero. 
 * There are four possibilities for what line could look like:
 *  1. nothing with dots (if strace was executed with -s0: , ""..., ),
 *  2. nothing without dots (if read/write was really for 0 bytes , "", ),
 *  3. <= than X characters, where X was specified as -sX, "xxxxxx"
 *  4. More than X, resulting string will be "xxxxx"...,
 *  5. NULL string. That one I don't understand, but it seems it happens from time to time: NULL,
 *  
 *  @arg line line to parse
 *  @return pointer to comma after second quote in the line.
 */

inline char * strace_pos_comma(char * line) {
	char *c, *lc;
	int backslash = 0;

	c = line;
	lc = NULL;
	while (*c && *c != '"' ) { // find first quote
		lc = c;
		c++;
	}

	if ( ! *c ) {
		//maybe there was "NULL":
		if ( (c = strstr(line, "NULL,")) != NULL ) {
				return c + strlen("NULL,") -1;
		} else {
			ERRORPRINTF("Unexpected end of line: %s", line);
			return NULL;
		}
	}

	if ( lc == NULL ) {
		ERRORPRINTF("Unexpected end of line: %s", line);
		return NULL;
	}

	c++;
	while ( *c ) { // find the second quote																
			if ( *c == '"' ) {
				if (! backslash) { // make sure we spot something like this "read(3, "blabla\"bla", 10)
							break;
				}
				backslash = 0;
			} else if ( *c == '\\' ) {
				if (backslash) { //two consecutives backslashes - they do not backslash special character but themselves
					backslash = 0;
				} else {
					backslash = 1;
				}
			} else {
				backslash = 0;
			}

			lc = c;
			c++;
	}


	c++;
	if ( *c ) { 
		if ( *c == '.' ) { 
			while ( *c && *c != ',' )
				c++;
			return c;
		} else if ( *c == ',' ) {
			return c;
		} else {
			ERRORPRINTF("Unexpected character after last quote: %c, whole line is %s", *c, line);		
			return NULL;
		}
	} else {
		ERRORPRINTF("Unexpected end of line: %s\n", line);
		return NULL;
	}
}


/** Reads write event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_write(char * line, list_t * list) {
	write_item_t * op_item;
	int retval;
	char start_time[MAX_TIME_STRING];
	char dur[MAX_TIME_STRING];
	char * line2;

	op_item = new_write_item();

	op_item->type = OP_WRITE;
	//first portion
	if ((retval = sscanf(line, " %d %s %*[^(](%d, ", &op_item->o.info.pid, start_time, &op_item->o.fd)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required:%d\n", retval);
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	//second part
	line2 = strace_pos_comma(line);
	if (line2 == NULL || (retval = sscanf(line2, ", %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.size, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required while parsing line2:%d\n", retval);
		ERRORPRINTF("Failing line:%s", line);
		free(op_item);
		return -1;
	}

	op_item->o.info.start = read_time(start_time);
	op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads pwrite(2) syscall from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_pwrite(char * line, list_t * list) {
	pwrite_item_t * op_item;
	int retval;
	char start_time[MAX_TIME_STRING];
	char dur[MAX_TIME_STRING];
	char * line2;

	op_item = new_pwrite_item();

	op_item->type = OP_PWRITE;
	//first portion
	if ((retval = sscanf(line, " %d %s %*[^(](%d, ", &op_item->o.info.pid, start_time, &op_item->o.fd)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required:%d\n", retval);
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	//second part
	line2 = strace_pos_comma(line);
	if (line2 == NULL || (retval = sscanf(line2, ", %"SCNi64", %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.size, &op_item->o.offset, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 4) {
		ERRORPRINTF("Error: It was not able to match all fields required while parsing line2:%d\n", retval);
		ERRORPRINTF("Failing line:%s", line);
		free(op_item);
		return -1;
	}

	op_item->o.info.start = read_time(start_time);
	op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads pread(2) syscall from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_pread(char * line, list_t * list) {
	pread_item_t * op_item;
	int retval;
	char start_time[MAX_TIME_STRING];
	char dur[MAX_TIME_STRING];
	char * line2;

	op_item = new_pread_item();

	op_item->type = OP_PREAD;
	//first portion
	if ((retval = sscanf(line, " %d %s %*[^(](%d, ", &op_item->o.info.pid, start_time, &op_item->o.fd)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required:%d\n", retval);
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	//second part
	line2 = strace_pos_comma(line);
	if (line2 == NULL || (retval = sscanf(line2, ", %"SCNi64", %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.size, &op_item->o.offset, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 4) {
		ERRORPRINTF("Error: It was not able to match all fields required while parsing line2:%d\n", retval);
		ERRORPRINTF("Failing line:%s", line);
		free(op_item);
		return -1;
	}

	op_item->o.info.start = read_time(start_time);
	op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}
/** Reads read event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_read(char * line, list_t * list) {
	read_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];
	char * line2;

	op_item = new_read_item();
	op_item->type = OP_READ;
	
	//first portion
	if ((retval = sscanf(line, " %d %s %*[^(](%d, ", &op_item->o.info.pid, start_time, &op_item->o.fd)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required:%d\n", retval);
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	//second part
	line2 = strace_pos_comma(line);
	if (line2 == NULL || (retval = sscanf(line2, ", %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.size, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of line%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 3) {
		ERRORPRINTF("Error: It was not able to match all fields required while parsing line2:%d\n", retval);
		ERRORPRINTF("Failing line:%s", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads close event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_close(char * line, list_t * list) {
	close_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_close_item();
	op_item->type = OP_CLOSE;
	
	if ((retval = sscanf(line, " %d %s %*[^(](%d) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.fd, &op_item->o.retval, dur)) == EOF) {
		free(op_item);
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		return -1;
	}
//	DEBUGPRINTF("pid:%d, start:%s, fd:%d, retval:%d, dur:%s\n", op_item->o.info.pid, start_time, op_item->o.fd, op_item->o.retval, dur);
//
	if (retval != 5) {
		ERRORPRINTF("Error: Only %d parameters parsed\n", retval);
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	if ( op_item->o.retval == -1 ) {
//		DEBUGPRINTF("Previous close failed...%s", "\n");
	}

	if ( op_item->o.fd == -1 ) {
		DEBUGPRINTF("Closing -1 previously???...%s", "\n");
	}


   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);


	list_append(list, &op_item->item);
	return 0;
}

/** Reads mkdir event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_mkdir(char * line, list_t * list) {
	mkdir_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_mkdir_item();
	op_item->type = OP_MKDIR;
	
	if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %o) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, &op_item->o.mode, &op_item->o.retval, dur)) == EOF) {
		free(op_item);
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		return -1;
	}

	if (retval != 6) {
		ERRORPRINTF("Error: Only %d parameters parsed\n", retval);
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads rmdir event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_rmdir(char * line, list_t * list) {
	rmdir_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_rmdir_item();
	op_item->type = OP_RMDIR;
	
	if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\") = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, &op_item->o.retval, dur)) == EOF) {
		free(op_item);
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		return -1;
	}

	if (retval != 5) {
		ERRORPRINTF("Error: Only %d parameters parsed\n", retval);
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads unlink event from strace file.
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_unlink(char * line, list_t * list) {
	unlink_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_unlink_item();
	op_item->type = OP_UNLINK;
	
	if ((retval = sscanf(line, "%d %s %*[^\"]\"%"QUOTE(MAX_STRING)"[^\"]\") = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, &op_item->o.retval, dur)) == EOF) {
		DEBUGPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	}
	if (retval != 5) {
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}
   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads pipe event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_pipe(char * line, list_t * list) {
	pipe_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_pipe_item();
	op_item->type = OP_PIPE;
	op_item->o.mode = 0;
	
	if ((retval = sscanf(line, " %d %s %*[^[][%d, %d]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.fd1, &op_item->o.fd2, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 6) { //mode flag was not present there
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads open event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_open(char * line, list_t * list) {
	open_item_t * op_item;
	char flags[MAX_STRING];
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_open_item();
	op_item->type = OP_OPEN;
	if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %[^,], %u) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, flags, &op_item->o.mode,
					&op_item->o.retval, dur)) == EOF) {

		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	}

	if (retval != 7) { //mode was probably missing there
		if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, flags,
						&op_item->o.retval, dur)) == EOF) {
			ERRORPRINTF("Error: unexpected end of file%s", "\n");
			free(op_item);
			return -1;
		}
//		DEBUGPRINTF("pid:%d, start:%s, name:%s, flags:%s, retval:%d, dur:%s\n", op_item->o.info.pid, start_time, op_item->o.name, flags, op_item->o.retval, dur);
		if ( retval != 6) {
			ERRORPRINTF("Error: It was not able to match all fields required: %d\n", retval);
			ERRORPRINTF("Failing line: %s", line);
			free(op_item);
			return -1;
		}
		op_item->o.mode = MODE_UNDEF;
	}
	
	op_item->o.flags = read_open_flags(flags);

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads creat event from strace file. It produces open event with correct mode.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_creat(char * line, list_t * list) {
	open_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_open_item();
	op_item->type = OP_OPEN;
	if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %u) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, &op_item->o.mode,
					&op_item->o.retval, dur)) == EOF) {

		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	}

	if ( retval != 6) {
		ERRORPRINTF("Error: It was not able to match all fields required: %d\n", retval);
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}
	
	op_item->o.flags = O_CREAT|O_WRONLY|O_TRUNC;
   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads clone event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_clone(char * line, list_t * list) {
	clone_item_t * op_item;
	char mode[MAX_STRING];
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_clone_item();

	op_item->type = OP_CLONE;

	if ((retval = sscanf(line, "%d %s %*[^\(](%*[^,], flags=%[^,], %*[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, mode, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 5) { //mode flag was not present there
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	op_item->o.mode = read_clone_flags(mode);
	DEBUGPRINTF("pid:%d, start:%s, flags:%s, retval:%d, dur:%s\n", op_item->o.info.pid, start_time, mode, op_item->o.retval, dur);


   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads dup event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_dup(char * line, list_t * list) {
	dup_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_dup_item();
	op_item->type = OP_DUP;
	op_item->o.flags = 0;

	if ((retval = sscanf(line, "%d %s %*[^(](%d) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.old_fd, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 5) { //mode flag was not present there
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	op_item->o.new_fd = op_item->o.retval;

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads dup2 event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_dup2(char * line, list_t * list) {
	dup_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_dup_item();
	op_item->type = OP_DUP2;
	op_item->o.flags = 0;

	if ((retval = sscanf(line, "%d %s %*[^(](%d, %d) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.old_fd, &op_item->o.new_fd, &op_item->o.retval, dur)) == EOF) {
		free(op_item);
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		return -1;
	} 

	if (retval != 6) { //mode flag was not present there
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads dup3 event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_dup3(char * line, list_t * list) {
	dup_item_t * op_item;
	int retval;
	char flags[MAX_STRING];
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_dup_item();
	op_item->type = OP_DUP3;

	if ((retval = sscanf(line, "%d %s %*[^(](%d, %d, %[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.old_fd, &op_item->o.new_fd, flags, &op_item->o.retval, dur)) == EOF) {
		free(op_item);
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		return -1;
	} 

	if (retval != 7) { //mode flag was not present there
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}
	op_item->o.flags = read_dup3_flags(flags);

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads llseek event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_llseek(char * line, list_t * list) {
	llseek_item_t * op_item;
	char flags[MAX_STRING];
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING];

	op_item = new_llseek_item();

	op_item->type = OP_LLSEEK;
	
	if ((retval = sscanf(line, " %d %s %*[^(](%d, %"SCNi64", \[%"SCNi64"], %[^)]) = %"SCNi64"%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.fd, &op_item->o.offset, &op_item->o.f_offset,
					flags, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 8) {
		ERRORPRINTF("Error: It was not able to match all fields required.%s", "\n");
		ERRORPRINTF("Failing line: %s", line);
		free(op_item);
		return -1;
	}

	op_item->o.flag = read_seek_flag(flags);

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads lseek event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_lseek(char * line, list_t * list) {
	lseek_item_t * op_item;
	char flags[MAX_STRING];
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_lseek_item();
	op_item->type = OP_LSEEK;
	if ((retval = sscanf(line, "%d %s %*[^(](%d, %"SCNi64", %[^)]) = %"SCNi64"%*[^<]<%[^>]", &op_item->o.info.pid, start_time,  &op_item->o.fd, &op_item->o.offset, 
					flags, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 
//	DEBUGPRINTF("pid:%d, start:%s, fd:%d, offset: %d, flags:%s, retval:%u, dur:%s\n", op_item->o.info.pid, start_time, op_item->o.fd, op_item->o.offset, flags, op_item->o.retval, dur);

	if (retval != 7) {
		ERRORPRINTF("Error: It was not able to match all fields required :%d\n", retval);
		ERRORPRINTF("Failing line: %s\n", line);
		free(op_item);
		return -1;
	}

	op_item->o.flag = read_seek_flag(flags);

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads sendfile event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_sendfile(char * line, list_t * list) {
	sendfile_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_sendfile_item();
	op_item->type = OP_SENDFILE;

	if ((retval = sscanf(line, "%d %s %*[^(](%d, %d, \[%"SCNi64"], %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.out_fd,
					&op_item->o.in_fd, &op_item->o.offset, &op_item->o.size, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 8) {
		if (retval == 4) { //maybe there is "NULL" string instead of [NUMBER] in fifth argument
			if ((retval = sscanf(line, "%d %s %*[^(](%d, %d, NULL, %"SCNi64") = %"SCNi64"%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.out_fd,
					&op_item->o.in_fd, &op_item->o.size, &op_item->o.retval, dur)) == EOF) {
				ERRORPRINTF("Error: unexpected end of file%s", "\n");
				free(op_item);
				return -1;
			} 
			if (retval != 7) {
				ERRORPRINTF("Error: It was not able to match all fields required :%d\n", retval);
				ERRORPRINTF("Failing line: %s\n", line);
				free(op_item);
				return -1;
			} else {
				op_item->o.offset = OFFSET_INVAL;
			}
		} else {
			ERRORPRINTF("Error: It was not able to match all fields required :%d\n", retval);
			ERRORPRINTF("Failing line: %s\n", line);
			free(op_item);
			return -1;
		}
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads access event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_access(char * line, list_t * list) {
	access_item_t * op_item;
	char mode[MAX_STRING];
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_access_item();
	op_item->type = OP_ACCESS;

   if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, mode, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 6) {
		ERRORPRINTF("Error: It was not able to match all fields required: %d\n", retval);
		ERRORPRINTF("Failing line: %s\n", line);
		free(op_item);
		return -1;
	}

	op_item->o.mode = read_access_flags(mode);
   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads stat event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int strace_read_stat(char * line, list_t * list) {
	stat_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_stat_item();
	op_item->type = OP_STAT;

   if ((retval = sscanf(line, "%d %s %*[^\"]\"%" QUOTE(MAX_STRING) "[^\"]\", %*[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, op_item->o.name, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 5) {
		ERRORPRINTF("Error: It was not able to match all fields required: %d\n", retval);
		ERRORPRINTF("Failing line: %s\n", line);
		free(op_item);
		return -1;
	}

   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads socket event from strace file.
 * 
 *
 * @arg f file from which to read, must be opened
 * @arg list list to which to append new structure
 * @return 0 on success, non-zero otherwise
 */

int read_socket_strace(char * line, list_t * list) {
	socket_item_t * op_item;
	int retval;
   char start_time[MAX_TIME_STRING];
   char dur[MAX_TIME_STRING] = "0";

	op_item = new_socket_item();
	op_item->type = OP_SOCKET;

	if ((retval = sscanf(line, "%d %s %*[^)]) = %d%*[^<]<%[^>]", &op_item->o.info.pid, start_time, &op_item->o.retval, dur)) == EOF) {
		ERRORPRINTF("Error: unexpected end of file%s", "\n");
		free(op_item);
		return -1;
	} 

	if (retval != 4) {
		ERRORPRINTF("Error: It was not able to match all fields required:%d\n",retval);
		ERRORPRINTF("Failing line: %s\n", line);
		free(op_item);
		return -1;
	}
   op_item->o.info.start = read_time(start_time);
   op_item->o.info.dur = read_duration(dur);

	list_append(list, &op_item->item);
	return 0;
}

/** Reads operation identifier from @a line.
 * 
 * @arg line one line from strace output
 * @arg operation output buffer where operation will be written
 * @arg stats whether to count statistics for operations
 */

inline void strace_get_operation(char * line, char * operation,  int stats) {
	char * c = line;
	int i;
	int retval;
	int32_t sec, usec;

	// skip first part containg PID and time of the syscall
	while (*c && (isspace(*c) || isdigit(*c) || *c == '.' || *c == '<' )) {
		c++;
	}

	i = 0;
	while ( *c && ((*c) != '(' && (*c) != ' ') ) { //read only operation - it can end with '(' in normal case or with ' ' in resumed calls
		operation[i] = *c;
		c++;
		i++;
	}

	operation[i] = 0;

	if (stats) {
		if ((retval = sscanf(line, "%*[^=]= %*[^<]<%d.%d>", &sec, &usec)) == EOF) {
			//probably unfinished call, ignore it
			return;
		}
		if ( retval != 2 ) {
			ERRORPRINTF("Error finding duration for statistics on line %s", line);
			return;
		}
		stats_add_op(line, operation, sec*1000000 + usec);
	}
}

/** Returns code of the operation in the given line. Also counts statistics for every operation (even no IO oriented), 
 * if enabled.
 *
 * @arg line line from which to read operation code
 * @arg whether to record statistics or not
 * @return code of the operation on the line
 */

char strace_get_operation_code(char * line, int stats) { 
	char operation[MAX_STRING];

	strace_get_operation(line, operation, stats);

	if (! strcmp(operation, "write")) {
		return OP_WRITE;
	} else if (! strcmp(operation, "read")) {
		return OP_READ;
	} else if (! strcmp(operation, "pwrite")) {
		return OP_PWRITE;
	} else if (! strcmp(operation, "pwrite64")) {
		return OP_PWRITE;
	} else if (! strcmp(operation, "pread")) {
		return OP_PREAD;
	} else if (! strcmp(operation, "pread64")) {
		return OP_PREAD;
	} else if (! strcmp(operation, "close")) {
		return OP_CLOSE;
	} else if (! strcmp(operation, "open")) {
		return OP_OPEN;
	} else if (! strcmp(operation, "creat")) {
		return OP_CREAT;
	} else if (! strcmp(operation, "unlink")) {
		return OP_UNLINK;
	} else if (! strcmp(operation, "lseek")) {
		return OP_LSEEK;
	} else if (! strcmp(operation, "_llseek")) {
		return OP_LLSEEK;
	} else if (! strcmp(operation, "pipe2")) {
		return OP_UNKNOWN; ///< this is on purpose. Change in the future, when pipe2 is supported
	} else if (! strcmp(operation, "pipe")) {
		return OP_PIPE;
	} else if (! strcmp(operation, "dup3")) {
		return OP_DUP3;
	} else if (! strcmp(operation, "dup2")) {
		return OP_DUP2;
	} else if (! strcmp(operation, "dup")) {
		return OP_DUP;
	} else if (! strcmp(operation, "mkdir")) {
		return OP_MKDIR;
	} else if (! strcmp(operation, "clone")) {
		return OP_CLONE;
	} else if (! strcmp(operation, "access")) {
		return OP_ACCESS;
	} else if (! strcmp(operation, "stat64")) {
		return OP_STAT;
	} else if (! strcmp(operation, "stat")) {
		return OP_STAT;
	} else if (! strcmp(operation, "socket")) {
		return OP_SOCKET;
	} else if (! strcmp(operation, "sendfile")) {
		return OP_SENDFILE;
	} else if (! strcmp(operation, "fcntl")) {
		return OP_FCNTL;
	}
	return OP_UNKNOWN;
}

void strace_read_unfinished(char * line, hash_table_t * ht) {
	isyscall_t * isyscall;
	item_t * item;
	int pid;
	
	sscanf(line, "%d", &pid);

	if ( (item = hash_table_find(ht, &pid)) == NULL) {
		isyscall = malloc(sizeof(isyscall_t));
		item_init(&isyscall->item);
		strncpy(isyscall->line, line, MAX_STRING);
		isyscall->pid = pid;
		hash_table_insert(ht, &pid, &isyscall->item);
		//DEBUGPRINTF("Unfinished call for %d inserted:%s", pid, line);
	} else {
		isyscall = hash_table_entry(item, isyscall_t, item);
		ERRORPRINTF("Already have unfinished syscall for pid: %d. %s", pid, isyscall->line);		
	}
}

void strace_read_resumed(char * line, list_t * list, hash_table_t * ht) {
	isyscall_t * isyscall;
	item_t * item;
	char buff[MAX_STRING *2 + 1];
	char *s;
	int pid;

	sscanf(line, "%d", &pid);

	if ( (item = hash_table_find(ht, &pid)) == NULL) {
		ERRORPRINTF("No syscall to resume for pid: %d. %s", pid, line);
	} else {
		isyscall = hash_table_entry(item, isyscall_t, item);
		s = strstr(isyscall->line, " <unfinished ...>");		
		if (s == NULL) {
			ERRORPRINTF("Previously recorded syscall was wrong: %d. %s", pid, isyscall->line);
			return;
		}
		*s = 0;
		strncpy(buff, isyscall->line, 2* MAX_STRING);
		s = strstr(line, "resumed> ");
		if (s == NULL) {
			ERRORPRINTF("Resumed syscall incorrectly formated: %d. %s", pid, isyscall->line);
			return;
		}
		s += strlen("resumed> ");
		strncat(buff, s, 2*MAX_STRING);
		//DEBUGPRINTF("Resulting line is :%s", buff);
		hash_table_remove(ht, &pid);
		strace_process_line(buff, list, ht, 0); //stats were already counted in first part
	}

}

inline int strace_process_line(char * line, list_t * list, hash_table_t * ht, int stats) {
	int retval = 0;
	char c;
	char *s;
	c = strace_get_operation_code(line, stats);

	if (strstr(line, "unfinished") && c != OP_UNKNOWN) {
		strace_read_unfinished(line, ht);
		return 0;
	}

	if ((s = strstr(line, "resumed")) != NULL) {
		if (s !=line) {
			s--;
			*s = '('; //lets hack the line, so it is recognized
			if ( strace_get_operation_code(line, 0) != OP_UNKNOWN) { //stats are disabled here, because it was already counted 5lines above
				strace_read_resumed(line, list, ht);
			}
			return 0;
		}
	}
			//just for now.
			int32_t pid;
			char start_time[20];
			char dur[20];
			int32_t old_fd;
			int32_t new_fd;
			int32_t tmp;


	switch(c) {
		case OP_WRITE:
			if ( (retval = strace_read_write(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_READ:
			if ( (retval = strace_read_read(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_PWRITE:
			if ( (retval = strace_read_pwrite(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_PREAD:
			if ( (retval = strace_read_pread(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_LSEEK:
			if ( (retval = strace_read_lseek(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_LLSEEK:
			if ( (retval = strace_read_llseek(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_OPEN:
			if ( (retval = strace_read_open(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_CREAT:
			if ( (retval = strace_read_creat(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_CLOSE:
			if ( (retval = strace_read_close(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_UNLINK:
			if ( (retval = strace_read_unlink(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_CLONE:
			if ( (retval = strace_read_clone(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_DUP3:
			if ( (retval = strace_read_dup3(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_DUP2:
			if ( (retval = strace_read_dup2(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_DUP:
			if ( (retval = strace_read_dup(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_PIPE:
			if ( (retval = strace_read_pipe(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_MKDIR:
			if ( (retval = strace_read_mkdir(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_RMDIR:
			if ( (retval = strace_read_rmdir(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_ACCESS:
			if ( (retval = strace_read_access(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_STAT:
			if ( (retval = strace_read_stat(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_SOCKET:
			if ( (retval = read_socket_strace(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_SENDFILE:
			if ( (retval = strace_read_sendfile(line, list)) != 0) {
				return retval;
			}
			break;
		case OP_FCNTL:
			//just for now.
			if ( strstr(line, "F_DUPFD")) {
				retval = sscanf(line, "%d %s %*[^(](%d, F_DUPFD, %d) = %d%*[^<]<%[^>]", &pid, start_time, &old_fd, &tmp, &new_fd, dur);
				if (retval != 6 ) {
					ERRORPRINTF("Can not parse line:, %s", line);
					return -1;
				} else {
					sprintf(line, "%d %s dup(%d) = %d <%s>", pid, start_time, old_fd, new_fd, dur);
				}
				if ( (retval = strace_read_dup(line, list)) != 0) {
					return retval;
				}
			}
			break;
		case OP_UNKNOWN: // just skip unknown operations
			break;
		default:
			return -1;
			break;
	}
	return retval;
}

/** Reads file syscalls actions from @a filename which is formatted output of strace program.
 * Detailed format of the input file is described in README under "strace file data structure".
 * All informations are appended to the @a list.
 *
 * @arg filename filename from which to read input
 * @arg list initialized pointer to the the. 
 * @return 0 on success, error code otherwise
 */

int strace_get_items(char * filename, list_t * list, int stats) {
	FILE * f;
	char line[MAX_LINE];
	hash_table_t ht;
	int retval;
	int linenum = 0;

	if ( (f = fopen(filename, "r")) == NULL) {
		DEBUGPRINTF("Error opening file %s: %s\n", filename, strerror(errno));
		return errno;
	}

	hash_table_init(&ht, HASH_TABLE_SIZE, &ht_ops_isyscall);

	if (stats) {
		stats_init();
	}

	while(fgets(line, MAX_LINE, f) != NULL) {
		linenum++;
		retval = strace_process_line(line, list, &ht, stats);
		if (retval != 0) {
			ERRORPRINTF("Error parsing file %s: on line %d, position %ld\n",
					filename, linenum, ftell(f));
		}
	}

	if (stats) {
		stats_print();
	}
	hash_table_destroy(&ht);

	fclose(f);
	return 0;
}
