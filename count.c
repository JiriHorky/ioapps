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

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "common.h"
#include "adt/list.h"
#include "in_common.h"
#include "fdmap.h"
#include "adt/hash_table.h"

inline int strace_process_line(FILE * outfile, char * line, hash_table_t * ht);
int get_items_strace(char * filename, char * filenameout) ;

typedef struct isyscall {
	item_t item;
	int32_t pid;
	char line[MAX_STRING];
} isyscall_t;


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

static int ht_compare_str(key_t *key, item_t *item) {
	str_item_t * str_item;

	str_item = hash_table_entry(item, str_item_t, item);

	return ! strncmp(str_item->name, (char *) key, MAX_STRING);
}

static inline void ht_remove_callback_str(item_t * item) {
	str_item_t * str_item = hash_table_entry(item, str_item_t, item);	
	//@todo we should free the list before!!!
	free(str_item);
	return;
}

/** hash table operations. */
static hash_table_operations_t ht_ops_str = {
	.hash = ht_hash_str,
	.compare = ht_compare_str,
	.remove_callback = ht_remove_callback_str /* = NULL if not used */
};


char get_operation_code(char * line) {
	char operation[MAX_STRING];
	char * c = line;
	int i;

	// skip first part containg PID and time of the syscall
	while (*c && (isspace(*c) || isdigit(*c) || *c == '.' || *c == '<' )) {
		c++;
	}

	i = 0;
	while ( *c && (*c) != '(' ) {
		operation[i] = *c;
		c++;
		i++;
	}
	operation[i] = 0;

}

void read_unfinished_strace(char * line, hash_table_t * ht) {
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

void read_resumed_strace(FILE * f, char * line, hash_table_t * ht) {
	isyscall_t * isyscall;
	item_t * item;
	char buff[MAX_STRING *2];
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
		strace_process_line(f, buff,ht);
	}

}

inline int strace_process_line(FILE * outfile, char * line, hash_table_t * ht) {
	int retval = 0;
	char c;
	char *s;
	c = get_operation_code(line);

	if (strstr(line, "unfinished") && c != OP_UNKNOWN) {
		read_unfinished_strace(line, ht);
		return 0;
	}

	if ((s = strstr(line, "resumed")) != NULL) {
		if (s !=line) {
			s--;
			*s = '('; //lets hack the line, so it recognized
			if ( get_operation_code(line) != OP_UNKNOWN) {
				read_resumed_strace(outfile, line, ht);
			}
			return 0;
		}
	}

	switch(c) {
		case OP_WRITE:
		case OP_READ:
		case OP_LSEEK:
		case OP_LLSEEK:
		case OP_OPEN:
		case OP_CLOSE:
		case OP_UNLINK:
		case OP_CLONE:
		case OP_DUP3:
		case OP_DUP2:
		case OP_DUP:
		case OP_PIPE:
		case OP_MKDIR:
		case OP_ACCESS:
		case OP_SOCKET:
		case OP_MMAP:
			if (strstr(line, "MAP_ANONYMOUS") == NULL) {
				fprintf(outfile, "%s", line);
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

int get_items_strace(char * filename, char *filenameout) {
	FILE * f, *fout;
	char line[MAX_LINE];
	hash_table_t ht;
	int retval;
	long long linenum = 0;

	if ( (f = fopen(filename, "r")) == NULL) {
		DEBUGPRINTF("Error opening file %s: %s\n", filename, strerror(errno));
		return errno;
	}

	if ( (fout = fopen(filenameout, "w")) == NULL) {
		DEBUGPRINTF("Error opening file %s: %s\n", filename, strerror(errno));
		return errno;
	}

	hash_table_init(&ht, HASH_TABLE_SIZE, &ht_ops_isyscall);

	while(fgets(line, MAX_LINE, f) != NULL) {
		linenum++;
		retval = strace_process_line(fout, line, &ht);
		if (retval != 0) {
			ERRORPRINTF("Error parsing file %s: on line %lld, position %ld\n",
					filename, linenum, ftell(f));
		}
		if (  linenum % 100000 == 0)  {
			printf("line num: %lld\n", linenum);
		}
	}
	printf("line num: %lld\n", linenum);

	return 0;
}

int main(int argc, char * * argv) {
	if (argc != 3) {
		fprintf(stderr, "<inputfile> <outputfile> needed\n");
		return EXIT_FAILURE;
	} else {
		get_items_strace(argv[1], argv[2]);
	}
	return 0;
}
