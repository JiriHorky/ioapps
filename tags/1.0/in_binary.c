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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "rependian.h"

#include "common.h"
#include "in_common.h"
#include "adt/list.h"
#include "adt/hash_table.h"

#define BIN_READ_ERROR_FREE ERRORPRINTF("Error reading event (%c) structure number: %"PRIi64"\n", c, num); \
		free(op_it); \
		return -1
#define BIN_READ_ERROR ERRORPRINTF("Error reading event (%c) structure number: %"PRIi64"\n", c, num); \
		return -1
#define BIN_WRITE_ERROR ERRORPRINTF("Error writing event. Retval: %d\n", rv); \
		return -1

#define read_int32(var) \
	if ( (rv = fread(&i32, sizeof(int32_t), 1, f)) != 1 ) { \
		BIN_READ_ERROR_FREE; \
	} else { \
		var = le32toh(i32); \
	}

#define read_int64(var) \
	if ( (rv = fread(&i64, sizeof(int64_t), 1, f)) != 1 ) { \
		BIN_READ_ERROR_FREE; \
	} else { \
		var = le64toh(i64); \
	}

#define write_int32(var) \
	i32 = htole32(var); \
	if ( (rv = fwrite(&i32, sizeof(int32_t), 1, f)) != 1 ) { \
		BIN_WRITE_ERROR; \
	} 

#define write_int64(var) \
	i64 = htole64(var); \
	if ( (rv = fwrite(&i64, sizeof(int64_t), 1, f)) != 1 ) { \
		BIN_WRITE_ERROR; \
	} 

#define write_char(var) \
	if ( (rv = fwrite(&var, sizeof(char), 1, f)) != 1 ) { \
		BIN_WRITE_ERROR; \
	} 

#define write_string(str, size) \
	if ( (rv = fwrite(str, sizeof(char), size, f)) != size ) { \
		BIN_WRITE_ERROR; \
	} 


inline int bin_read_info(FILE *f, op_info_t * info, char c, int64_t num) {
	int32_t i32;
	int rv;

	if ( (rv = fread(&i32, sizeof(int32_t), 1, f)) != 1 ) {
		BIN_READ_ERROR;
	} else {
		info->pid = le32toh(i32);
	}

	if ( (rv = fread(&i32, sizeof(int32_t), 1, f)) != 1 ) {
		BIN_READ_ERROR;
	} else {
		info->dur = le32toh(i32);
	}

	if ( (rv = fread(&i32, sizeof(int32_t), 1, f)) != 1 ) {
		BIN_READ_ERROR;
	} else {
		info->start.tv_sec = le32toh(i32);
	}

	if ( (rv = fread(&i32, sizeof(int32_t), 1, f)) != 1 ) {
		BIN_READ_ERROR;
	} else {
		info->start.tv_usec = le32toh(i32);
	}
	return 0;
}

int bin_read_read(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_READ;
	read_item_t * op_it;
	op_it = new_read_item();
	op_it->type = c;

	read_int32(op_it->o.fd);
	read_int64(op_it->o.size);
	read_int64(op_it->o.retval);

	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_write(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_WRITE;
	write_item_t * op_it;
	op_it = new_write_item();
	op_it->type = c;

	read_int32(op_it->o.fd);
	read_int64(op_it->o.size);
	read_int64(op_it->o.retval);

	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_open(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_OPEN;
	char buff[MAX_STRING];
	open_item_t * op_it;
	op_it = new_open_item();
	op_it->type = c;

	int32_t len;
	read_int32(len);
	if ( (rv = fread(buff, sizeof(char), len, f)) != len ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[len] = 0;
		strncpy(op_it->o.name, buff, len+1);
	}

	read_int32(op_it->o.flags);
	read_int32(op_it->o.mode);
	read_int32(op_it->o.retval);

	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_close(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_CLOSE;
	close_item_t * op_it;
	op_it = new_close_item();
	op_it->type = c;

	read_int32(op_it->o.fd);
	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_unlink(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_UNLINK;
	char buff[MAX_STRING];
	unlink_item_t * op_it;
	op_it = new_unlink_item();
	op_it->type = c;

	read_int32(i32);
	if ( (rv = fread(buff, sizeof(char), i32, f)) != i32 ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[i32] = 0;
		strncpy(op_it->o.name, buff, i32+1);
	}

	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_lseek(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_LSEEK;
	lseek_item_t * op_it;
	op_it = new_lseek_item();
	op_it->type = c;

	read_int32(op_it->o.fd);
	read_int32(op_it->o.flag);
	read_int64(op_it->o.offset);
	read_int64(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_llseek(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_LLSEEK;
	llseek_item_t * op_it;
	op_it = new_llseek_item();
	op_it->type = c;

	read_int32(op_it->o.fd);
	read_int64(op_it->o.offset);
	read_int64(op_it->o.f_offset);
	read_int32(op_it->o.flag);
	read_int64(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_clone(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_CLONE;
	clone_item_t * op_it;
	op_it = new_clone_item();
	op_it->type = c;

	read_int32(op_it->o.mode);
	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_mkdir(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_MKDIR;
	char buff[MAX_STRING];
	mkdir_item_t * op_it;
	op_it = new_mkdir_item();
	op_it->type = c;

	read_int32(i32);

	if ( (rv = fread(buff, sizeof(char), i32, f)) != i32 ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[i32] = 0;
		strncpy(op_it->o.name, buff, i32+1);
	}

	read_int32(op_it->o.mode);
	read_int32(op_it->o.retval);

	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_rmdir(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_MKDIR;
	char buff[MAX_STRING];
	rmdir_item_t * op_it;
	op_it = new_rmdir_item();
	op_it->type = c;

	read_int32(i32);

	if ( (rv = fread(buff, sizeof(char), i32, f)) != i32 ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[i32] = 0;
		strncpy(op_it->o.name, buff, i32+1);
	}

	read_int32(op_it->o.retval);

	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_dup(FILE * f, char c, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	dup_item_t * op_it;
	op_it = new_dup_item();
	op_it->type = c;

	read_int32(op_it->o.new_fd);
	read_int32(op_it->o.old_fd);
	read_int32(op_it->o.flags);
	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_pipe(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_PIPE;
	pipe_item_t * op_it;
	op_it = new_pipe_item();
	op_it->type = c;

	read_int32(op_it->o.fd1);
	read_int32(op_it->o.fd2);
	read_int32(op_it->o.mode);
	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_access(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_ACCESS;
	char buff[MAX_STRING];
	access_item_t * op_it;
	op_it = new_access_item();
	op_it->type = c;

	read_int32(i32);
	if ( (rv = fread(buff, sizeof(char), i32, f)) != i32 ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[i32] = 0;
		strncpy(op_it->o.name, buff, i32+1);
	}

	read_int32(op_it->o.mode);
	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_stat(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_STAT;
	char buff[MAX_STRING];
	stat_item_t * op_it;
	op_it = new_stat_item();
	op_it->type = c;

	read_int32(i32);
	if ( (rv = fread(buff, sizeof(char), i32, f)) != i32 ) {
		BIN_READ_ERROR_FREE;
	} else {
		buff[i32] = 0;
		strncpy(op_it->o.name, buff, i32+1);
	}

	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}

int bin_read_socket(FILE * f, list_t * list, int64_t num) {
	int rv;
	int32_t i32;
	char c = OP_SOCKET;
	socket_item_t * op_it;
	op_it = new_socket_item();
	op_it->type = c;

	read_int32(op_it->o.retval);
	
	if ( (rv = bin_read_info(f, &op_it->o.info, c, num)) != 0) {
		BIN_READ_ERROR_FREE;
	}

	list_append(list, &op_it->item);
	return 0;
}


int bin_get_items(char * filename, list_t * list) {
	FILE * f;
	char c;
	long long i = 0;

	if ((f = fopen(filename, "rb")) == NULL ) {
		ERRORPRINTF("Error opening file %s: %s\n", filename, strerror(errno));
		return errno;
	}
	while((c = getc(f)) != EOF) {
		i++;
		switch (c) {
			case OP_WRITE:
				if ( bin_read_write(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_READ:
				if ( bin_read_read(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_OPEN:
				if ( bin_read_open(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_CLOSE:
				if ( bin_read_close(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_UNLINK:
				if ( bin_read_unlink(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_LSEEK:
				if ( bin_read_lseek(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_LLSEEK:
				if ( bin_read_llseek(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_CLONE:
				if ( bin_read_clone(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_MKDIR:
				if ( bin_read_mkdir(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_RMDIR:
				if ( bin_read_rmdir(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_DUP:
			case OP_DUP2:
			case OP_DUP3:
				if ( bin_read_dup(f, c, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_PIPE:
				if ( bin_read_pipe(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_ACCESS:
				if ( bin_read_access(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_STAT:
				if ( bin_read_stat(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			case OP_SOCKET:
				if ( bin_read_socket(f, list, i) != 0 ) {
					ERRORPRINTF("Error reading binary file: %s\n", filename);
					return -1;
				}
				break;
			default:
				ERRORPRINTF("Unknown operation identifier: '%c' reading item no %lld at filepos:%ld\n", c, i, ftell(f));
				return -1;
				break;
		}
	}
	fclose(f);
	return 0;
}

///////////////////////////////
// Item saving:
///////////////////////////////
inline int bin_write_info(FILE *f, op_info_t * info) {
	int32_t i32;
	int rv;

	write_int32(info->pid);
	write_int32(info->dur);
	write_int32(info->start.tv_sec);
	write_int32(info->start.tv_usec);

	return 0;
}

int bin_save_write(FILE * f, write_op_t * op_it) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_WRITE;

	write_char(c);
	write_int32(op_it->fd);
	write_int64(op_it->size);
	write_int64(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_read(FILE * f, read_op_t * op_it) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_READ;

	write_char(c);
	write_int32(op_it->fd);
	write_int64(op_it->size);
	write_int64(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_open(FILE * f, open_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_OPEN;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->flags);
	write_int32(op_it->mode);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_close(FILE * f, close_op_t * op_it) {
	int rv;
	int32_t i32;
	char c = OP_CLOSE;

	write_char(c);
	write_int32(op_it->fd);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_unlink(FILE * f, unlink_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_UNLINK;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_lseek(FILE * f, lseek_op_t * op_it) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_LSEEK;

	write_char(c);
	write_int32(op_it->fd);
	write_int32(op_it->flag);
	write_int64(op_it->offset);
	write_int64(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_llseek(FILE * f, llseek_op_t * op_it) {
	int rv;
	int32_t i32;
	int64_t i64;
	char c = OP_LLSEEK;

	write_char(c);
	write_int32(op_it->fd);
	write_int64(op_it->offset);
	write_int64(op_it->f_offset);
	write_int32(op_it->flag);
	write_int64(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_clone(FILE * f, clone_op_t * op_it) {
	int rv;
	int32_t i32;
	char c = OP_CLONE;

	write_char(c);
	write_int32(op_it->mode);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_dup(FILE * f, char c, dup_op_t * op_it) {
	int rv;
	int32_t i32;

	write_char(c);
	write_int32(op_it->new_fd);
	write_int32(op_it->old_fd);
	write_int32(op_it->flags);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_mkdir(FILE * f, mkdir_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_MKDIR;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->mode);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_rmdir(FILE * f, rmdir_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_MKDIR;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_pipe(FILE * f, pipe_op_t * op_it) {
	int rv;
	int32_t i32;
	char c = OP_PIPE;

	write_char(c);
	write_int32(op_it->fd1);
	write_int32(op_it->fd2);
	write_int32(op_it->mode);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_access(FILE * f, access_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_ACCESS;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->mode);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_stat(FILE * f, stat_op_t * op_it) {
	int rv;
	int32_t i32;
	int32_t len;
	char c = OP_STAT;

	write_char(c);
	len = strlen(op_it->name);
	write_int32(len);
	write_string(op_it->name, len);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_socket(FILE * f, socket_op_t * op_it) {
	int rv;
	int32_t i32;
	char c = OP_SOCKET;

	write_char(c);
	write_int32(op_it->retval);

	if ( (rv = bin_write_info(f, &op_it->info)) != 0) {
		BIN_WRITE_ERROR;
	}

	return 0;
}

int bin_save_items(char * filename, list_t * list) {
	FILE * f;
	long long i = 0;
	item_t * item = list->head;
	common_op_item_t * com_it;
	write_item_t * write_it;
	read_item_t * read_it;
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

	
	if ((f = fopen(filename, "wb")) == NULL ) {
		ERRORPRINTF("Error opening file %s: %s\n", filename, strerror(errno));
		return errno;
	}

	while (item) { 
		i++;
		com_it = list_entry(item, common_op_item_t, item);
		switch (com_it->type) {
			case OP_WRITE:
				write_it = (write_item_t *) com_it;
				if ( bin_save_write(f, &write_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_READ:
				read_it = (read_item_t *) com_it;
				if ( bin_save_read(f, &read_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_OPEN:
				open_it = (open_item_t *) com_it;
				if ( bin_save_open(f, &open_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_CLOSE:
				close_it = (close_item_t *) com_it;
				if ( bin_save_close(f, &close_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_UNLINK:
				unlink_it = (unlink_item_t *) com_it;
				if ( bin_save_unlink(f, &unlink_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_LSEEK:
				lseek_it = (lseek_item_t *) com_it;
				if ( bin_save_lseek(f, &lseek_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_LLSEEK:
				llseek_it = (llseek_item_t *) com_it;
				if ( bin_save_llseek(f, &llseek_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_CLONE:
				clone_it = (clone_item_t *) com_it;
				if ( bin_save_clone(f, &clone_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_MKDIR:
				mkdir_it = (mkdir_item_t *) com_it;
				if ( bin_save_mkdir(f, &mkdir_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_RMDIR:
				rmdir_it = (rmdir_item_t *) com_it;
				if ( bin_save_rmdir(f, &rmdir_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_DUP:
			case OP_DUP2:
			case OP_DUP3:
				dup_it = (dup_item_t *) com_it;
				if ( bin_save_dup(f, com_it->type, &dup_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_PIPE:
				pipe_it = (pipe_item_t *) com_it;
				if ( bin_save_pipe(f, &pipe_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_ACCESS:
				access_it = (access_item_t *) com_it;
				if ( bin_save_access(f, &access_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_STAT:
				stat_it = (stat_item_t *) com_it;
				if ( bin_save_stat(f, &stat_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			case OP_SOCKET:
				socket_it = (socket_item_t *) com_it;
				if ( bin_save_close(f, &close_it->o) != 0 ) {
					ERRORPRINTF("Error saving to binary file %s\n", filename);
					return -1;
				}
				break;
			default:
				ERRORPRINTF("Unknown operation identifier: '%c'\n", com_it->type);
				return -1;
				break;
		}
		item = item->next;
	}
	fclose(f);
	return 0;
}
