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
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "in_common.h"

#define TIMEVAL_DIFF(t1, t2) (((uint64_t)(t1.tv_sec) * 1000000 + (uint64_t)(t1.tv_usec)) - ((uint64_t)(t2.tv_sec) * 1000000 + (uint64_t)(t2.tv_usec)))
#define CALL_TIME(x) ((uint64_t)(x->o.info.start.tv_sec) * 1000000 + (uint64_t)(x->o.info.start.tv_usec))
#define DUR_TIME(x) ((uint32_t)(x->o.info.dur))


void print_read(read_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tread(%"PRIi32", addr, %"PRIi64") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.size, op_it->o.retval, op_it->o.info.dur);
}

void print_write(write_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\twrite(%"PRIi32", addr, %"PRIi64") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.size, op_it->o.retval, op_it->o.info.dur);
}

void print_pread(pread_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tpread(%"PRIi32", addr, %"PRIi64", %"PRIi64") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.size, op_it->o.offset, op_it->o.retval, op_it->o.info.dur);
}


void print_pwrite(pwrite_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tpwrite(%"PRIi32", addr, %"PRIi64", %"PRIi64") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.size, op_it->o.offset, op_it->o.retval, op_it->o.info.dur);
}


void print_pipe(pipe_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tpipe(%"PRIi32", %"PRIi32", 0x%"PRIx32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd1,\
               op_it->o.fd2, op_it->o.mode, op_it->o.retval, op_it->o.info.dur);
}

void print_open(open_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\topen(%s, 0x%"PRIx32", 0x%"PRIx32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.flags, op_it->o.mode, op_it->o.retval, op_it->o.info.dur);
}


void print_clone(clone_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tclone(addr, addr, 0x%"PRIx32", args..) = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid,\
               op_it->o.mode, op_it->o.retval, op_it->o.info.dur);
}


void print_close(close_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tclose(%"PRIi32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.retval, op_it->o.info.dur);
}


void print_unlink(unlink_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tunlink(%s) = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.retval, op_it->o.info.dur);
}


void print_llseek(llseek_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tllseek(%"PRIi32", offset=%"PRIi64", result=%"PRIi64", %0x"PRIx32") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.offset, op_it->o.f_offset, op_it->o.flag, op_it->o.retval, op_it->o.info.dur);
}


void print_lseek(lseek_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tlseek(%"PRIi32", %"PRIi64", %0x"PRIx32") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.fd,\
               op_it->o.offset, op_it->o.flag, op_it->o.retval, op_it->o.info.dur);
}


void print_sendfile(sendfile_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tsendfile(%"PRIi32", %"PRIi32", %"PRIi64", %"PRIi64") = %"PRIi64" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.out_fd,\
               op_it->o.in_fd, op_it->o.offset, op_it->o.size, op_it->o.retval, op_it->o.info.dur);
}

void print_mkdir(mkdir_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tmkdir(%s, 0x%"PRIx32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.mode, op_it->o.retval, op_it->o.info.dur);
}

void print_rmdir(rmdir_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\trmdir(%s) = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.retval, op_it->o.info.dur);
}



void print_dup(dup_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tdup2(%"PRIi32", %"PRIi32", 0x%"PRIx32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.old_fd,\
               op_it->o.new_fd, op_it->o.flags, op_it->o.retval, op_it->o.info.dur);
}


void print_access(access_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\taccess(%s, 0x%"PRIx32") = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.mode, op_it->o.retval, op_it->o.info.dur);
}


void print_stat(stat_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tstat(%s, addr) = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid, op_it->o.name,\
               op_it->o.retval, op_it->o.info.dur);
}

void print_socket(socket_item_t * op_it) {
	printf("%"PRIi32".%"PRIi32"\t%"PRIi32"\tsocket(domain, type, protocol) = %"PRIi32" <%"PRIi32">\n",\
               op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.info.pid,\
               op_it->o.retval, op_it->o.info.dur);
}



int print_items(list_t * list) {
	long long i = 0;
	item_t * item = list->head;
	common_op_item_t * com_it;

	while (item) { 
		i++;
		com_it = list_entry(item, common_op_item_t, item);
		switch (com_it->type) {
			case OP_WRITE:
				print_write((write_item_t *) com_it);
				break;
			case OP_READ:
				print_read((read_item_t *) com_it);
				break;
			case OP_PWRITE:
				print_pwrite((pwrite_item_t *) com_it);
				break;
			case OP_PREAD:
				print_pread((pread_item_t *) com_it);
				break;
			case OP_OPEN:
				print_open((open_item_t *) com_it);
				break;
			case OP_CLOSE:
				print_close((close_item_t *) com_it);
				break;
			case OP_UNLINK:
				print_unlink((unlink_item_t *) com_it);
				break;
			case OP_LSEEK:
				print_lseek((lseek_item_t *) com_it);
				break;
			case OP_LLSEEK:
				print_llseek((llseek_item_t *) com_it);
				break;
			case OP_CLONE:
				print_clone((clone_item_t *) com_it);
				break;
			case OP_MKDIR:
				print_mkdir((mkdir_item_t *) com_it);
				break;
			case OP_RMDIR:
				print_rmdir((rmdir_item_t *) com_it);
				break;
			case OP_DUP:
			case OP_DUP2:
			case OP_DUP3:
				print_dup((dup_item_t *) com_it);
				break;
			case OP_PIPE:
				print_pipe((pipe_item_t *) com_it);
				break;
			case OP_ACCESS:
				print_access((access_item_t *) com_it);
				break;
			case OP_STAT:
				print_stat((stat_item_t *) com_it);
				break;
			case OP_SOCKET:
				print_socket((socket_item_t *) com_it);
				break;
			case OP_SENDFILE:
				print_sendfile((sendfile_item_t *) com_it);
				break;
			default:
				ERRORPRINTF("Unknown operation identifier: '%c'\n", com_it->type);
				return -1;
				break;
		}
		item = item->next;
	}
	return 0;
}
