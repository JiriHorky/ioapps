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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include "in_common.h"

write_item_t * new_write_item() {
	write_item_t * i;

	i = malloc(sizeof(write_item_t));
	item_init(&i->item);
	return i;
}

read_item_t * new_read_item() {
	read_item_t * i;

	i = malloc(sizeof(read_item_t));
	item_init(&i->item);
	return i;
}

mkdir_item_t * new_mkdir_item() {
	mkdir_item_t * i;

	i = malloc(sizeof(mkdir_item_t));
	item_init(&i->item);
	return i;
}

rmdir_item_t * new_rmdir_item() {
	rmdir_item_t * i;

	i = malloc(sizeof(rmdir_item_t));
	item_init(&i->item);
	return i;
}

dup_item_t * new_dup_item() {
	dup_item_t * i;

	i = malloc(sizeof(dup_item_t));
	item_init(&i->item);
#ifndef NDEBUG
	memset(i, 0, sizeof(*i));
#endif
	return i;
}

clone_item_t * new_clone_item() {
	clone_item_t * i;

	i = malloc(sizeof(clone_item_t));
	item_init(&i->item);
	return i;
}

pipe_item_t * new_pipe_item() {
	pipe_item_t * i;

	i = malloc(sizeof(pipe_item_t));
#ifndef NDEBUG
	memset(i, 0, sizeof(*i));
#endif
	item_init(&i->item);
	return i;
}

open_item_t * new_open_item() {
	open_item_t * i;

	i = malloc(sizeof(open_item_t));
	item_init(&i->item);
	return i;
}

close_item_t * new_close_item() {
	close_item_t * i;

	i = malloc(sizeof(close_item_t));
	item_init(&i->item);
	return i;
}

unlink_item_t * new_unlink_item() {
	unlink_item_t * i;

	i = malloc(sizeof(unlink_item_t));
	item_init(&i->item);
	return i;
}

llseek_item_t * new_llseek_item() {
	llseek_item_t * i;

	i = malloc(sizeof(llseek_item_t));
	item_init(&i->item);
	return i;

}

lseek_item_t * new_lseek_item() {
	lseek_item_t * i;

	i = malloc(sizeof(lseek_item_t));
	item_init(&i->item);
	return i;
}

access_item_t * new_access_item() {
	access_item_t * i;

	i = malloc(sizeof(access_item_t));
	item_init(&i->item);
	return i;
}

stat_item_t * new_stat_item() {
	stat_item_t * i;

	i = malloc(sizeof(stat_item_t));
	item_init(&i->item);
	return i;
}

socket_item_t * new_socket_item() {
	socket_item_t * i;

	i = malloc(sizeof(socket_item_t));
	item_init(&i->item);
	return i;
}
/** Removes and unallocates lists of syscalls.
 *
 * @arg list list of syscalls to delete
 * @return zero if ok, non-zero otherwise
 */


int remove_items(list_t * list) {
	long long i = 0;
	item_t * item = list->head;
	common_op_item_t * com_it;
	read_item_t * read_it;
	write_item_t * write_it;
	open_item_t * open_it;
	close_item_t * close_it;
	unlink_item_t * unlink_it;
	lseek_item_t * lseek_it;
	llseek_item_t * llseek_it;
	clone_item_t * clone_it;
	dup_item_t * dup_it;
	mkdir_item_t * mkdir_it;
	rmdir_item_t * rmdir_it;
	pipe_item_t * pipe_it;
	access_item_t * access_it;
	stat_item_t * stat_it;
	socket_item_t * socket_it;

	while (item) { 
		i++;
		com_it = list_entry(item, common_op_item_t, item);
		switch (com_it->type) {
			case OP_WRITE:
				write_it = (write_item_t *) com_it;
				item = write_it->item.next;
				free(write_it);
				break;
			case OP_READ:
				read_it = (read_item_t *) com_it;
				item = read_it->item.next;
				free(read_it);
				break;
			case OP_OPEN:
				open_it = (open_item_t *) com_it;
				item = open_it->item.next;
				free(open_it);
				break;
			case OP_CLOSE:
				close_it = (close_item_t *) com_it;
				item = close_it->item.next;
				free(close_it);
				break;
			case OP_UNLINK:
				unlink_it = (unlink_item_t *) com_it;
				item = unlink_it->item.next;
				free(unlink_it);
				break;
			case OP_LSEEK:
				lseek_it = (lseek_item_t *) com_it;
				item = lseek_it->item.next;
				free(lseek_it);
				break;
			case OP_LLSEEK:
				llseek_it = (llseek_item_t *) com_it;
				item = llseek_it->item.next;
				free(llseek_it);
				break;
			case OP_CLONE:
				clone_it = (clone_item_t *) com_it;
				item = clone_it->item.next;
				free(clone_it);
				break;
			case OP_MKDIR:
				mkdir_it = (mkdir_item_t *) com_it;
				item = mkdir_it->item.next;
				free(mkdir_it);
				break;
			case OP_RMDIR:
				rmdir_it = (rmdir_item_t *) com_it;
				item = rmdir_it->item.next;
				free(rmdir_it);
				break;
			case OP_DUP:
			case OP_DUP2:
			case OP_DUP3:
				dup_it = (dup_item_t *) com_it;
				item = dup_it->item.next;
				free(dup_it);
				break;
			case OP_PIPE:
				pipe_it = (pipe_item_t *) com_it;
				item = pipe_it->item.next;
				free(pipe_it);
				break;
			case OP_ACCESS:
				access_it = (access_item_t *) com_it;
				item = access_it->item.next;
				free(access_it);
				break;
			case OP_STAT:
				stat_it = (stat_item_t *) com_it;
				item = stat_it->item.next;
				free(stat_it);
				break;
			case OP_SOCKET:
				socket_it = (socket_item_t *) com_it;
				item = socket_it->item.next;
				free(socket_it);
				break;
			default:
				ERRORPRINTF("Unknown operation identifier: '%c'\n", com_it->type);
				return -1;
				break;
		}
	}
	return 0;
}



/** Reads integer from string @a str and stores it to the memore referenced by @a num.
 * This function DOES error checking. And it also skips the last possible space char.
 *
 * @arg str buffer from which to read - must be open.
 * @arg num pointer to the memory where to store number retrieved from the file
 * @return 0 on success -1 otherwise
 */

int read_integer(char * str, long long * num) {
	*num = 0;
	char digit;	
	int char_read = 0;
	char c = *str;

	char_read++;
	while ( c != 0 && c != ' ' && c != '\n') {
		*num *= 10;
		digit= c - '0';

		if (digit >=0 && digit <=9) {
			*num += digit;
		} else {
			DEBUGPRINTF("Non digit character while parsing number%s", "\n");
			return -1;
		}

		str++;
		c = *str;
		char_read++;
	}
	
	if (char_read == 1) {
		return 0; //error
	} else {		
		return char_read; //including any space character
	}
}

int read_open_flag(char * s) {
	int flag = 0;

	if (!strcmp(s, "O_APPEND")) {
		flag |= O_APPEND;
	} else if ( !strcmp(s, "O_RDONLY")) {
		flag |= O_RDONLY;
	} else if ( !strcmp(s, "O_WRONLY")) {
		flag |= O_WRONLY;
	} else if ( !strcmp(s, "O_RDWR")) {
		flag |= O_RDWR;
	} else if ( !strcmp(s, "O_ASYNC")) {
		flag |= O_ASYNC;
	} else if ( !strcmp(s, "O_CLOEXEC")) { //this is only defined on kernels 2.6.23+, which we are not yet using..
		DEBUGPRINTF("Unsuported flag: %s\n", "O_CLOEXEC");			
	} else if ( !strcmp(s, "O_CREAT")) {
		flag |= O_CREAT;
#ifdef _GNU_SOURCE			
	} else if ( !strcmp(s, "O_DIRECT")) {
		flag |= O_DIRECT;
	} else if ( !strcmp(s, "O_DIRECTORY")) {
		flag |= O_DIRECTORY;
	} else if ( !strcmp(s, "O_LARGEFILE")) {
		flag |= O_LARGEFILE;
	} else if ( !strcmp(s, "O_NOATIME")) {
		flag |= O_NOATIME;
	} else if ( !strcmp(s, "O_NOFOLLOW")) {
		flag |= O_NOFOLLOW;
#endif			
	} else if ( !strcmp(s, "O_NOCTTY")) {
		flag |= O_NOCTTY;
	} else if ( !strcmp(s, "O_EXCL")) {
		flag |= O_EXCL;
	} else if ( !strcmp(s, "O_NONBLOCK")) {
		flag |= O_NONBLOCK;
	} else if ( !strcmp(s, "O_NDELAY")) {
		flag |= O_NDELAY;
	} else if ( !strcmp(s, "O_SYNC")) {
		flag |= O_SYNC;
	} else if ( !strcmp(s, "O_TRUNC")) {
		flag |= O_TRUNC;
	}
	return flag;
}


int read_open_flags(char * str) {
	int flags = 0;
	char * s = NULL;

	s = strtok(str, "|");
	while ( s ) {
		flags |= read_open_flag(s);
		s = strtok(NULL, "|");
	}
	return flags;
}


/** Counts number of occurences of char c in string str.
 * @arg str string in which to search
 * @arg c char to look for
 * @return number of occurences of @a c in @a str, or -1 if str is NULL
 */

int strccount(char * str, char c) {
	char * s = str - 1;
	int rv = 0;

	while(s) {
		s = strchr(s + 1, c);
		rv++;
	}

	return rv -1;
}



/** Reads some flag that can be in clone (2) sys call. Only important ones are parsed.
 */

int read_clone_flag(char * s) {
	int flag = 0;

	if (!strcmp(s, "CLONE_FILES")) {
		flag |= CLONE_FILES;
	}
	return flag;
}

int read_clone_flags(char * str) {
	int flags = 0;
	char * s = NULL;

	s = strtok(str, "|");
	while ( s ) {
		flags |= read_clone_flag(s);
		s = strtok(NULL, "|");
	}
	return flags;
}

int read_seek_flag(char * s) {
   int flag = 0;

   if (!strcmp(s, "SEEK_SET")) {
      flag |= SEEK_SET;
   } else if ( !strcmp(s, "SEEK_CUR")) {
      flag |= SEEK_CUR;
   } else if ( !strcmp(s, "SEEK_END")) {
      flag |= SEEK_END;
   }
   return flag;
}

int read_access_flag(char * s) {
   int flag = 0;

  if (!strcmp(s, "F_OK")) {
      flag |= F_OK;
   } else if ( !strcmp(s, "R_OK")) {
      flag |= R_OK;
  	} else if ( !strcmp(s, "W_OK")) {
      flag |= W_OK;
	} else if ( !strcmp(s, "X_OK")) {
      flag |= X_OK;
	}
   
   return flag;
}

int read_access_flags(char * str) {
	int flags = 0;
	char * s = NULL;

	s = strtok(str, "|");
	while ( s ) {
		flags |= read_access_flag(s);
		s = strtok(NULL, "|");
	}
	return flags;
}

int read_dup3_flags(char * str) {
#ifdef O_CLOEXEC //2.6.23+ kernels
	if (!strcmp(str, "O_CLOEXEC")) {
		return O_CLOEXEC;
	} else {
		return 0;
	}
#else
	return 0;
#endif
}

struct int32timeval read_time(char * timestr) {
	struct int32timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	char * s = NULL;

	s = strtok(timestr, ".");
	if (s) {
		tv.tv_sec = atoi(s);
	} else {
		ERRORPRINTF("Error parsing time, unexpected end of string%s", "\n");
		return tv;
	}
	s = strtok(NULL, ".");

	if (s) {
		tv.tv_usec = atoi(s);
	} else {
		ERRORPRINTF("Error parsing time, unexpected end of string%s", "\n");
		return tv;
	}

	s = strtok(NULL, ".");
	if (s != NULL) {
		fprintf(stderr, "Error parsing time, end of string expected!\n");
	}
	return tv;
}

int32_t read_duration(char * timestr) {
	int32_t usec = 0;
	char * s = NULL;

	s = strtok(timestr, ".");
	if (s) {
		usec = atoi(s);
	} else {
		ERRORPRINTF("Error parsing time, unexpected end of string: %s\n", timestr);
		return usec;
	}
	usec *= 1000000;
	s = strtok(NULL, ".");

	if (s) {
		usec += atoi(s);
	} else {
		ERRORPRINTF("Error parsing time, unexpected end of string:%s\n", timestr);
		return usec;
	}

	s = strtok(NULL, ".");
	if (s != NULL) {
		fprintf(stderr, "Error parsing time, end of string expected!\n");
	}
	return usec;
}
