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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sched.h>

#include "replicate.h"
#include "fdmap.h"
#include "namemap.h"
#include "in_common.h"
#include "simulate.h"
#include "adt/hash_table.h"

#define TIMEVAL_DIFF(t1, t2) (((uint64_t)(t1.tv_sec) * 1000000 + (uint64_t)(t1.tv_usec)) - ((uint64_t)(t2.tv_sec) * 1000000 + (uint64_t)(t2.tv_usec)))
#define CALL_TIME(x) ((uint64_t)(x->o.info.start.tv_sec) * 1000000 + (uint64_t)(x->o.info.start.tv_usec))
#define DUR_TIME(x) ((uint32_t)(x->o.info.dur))

extern hash_table_operations_t ht_ops_fdusage;

char data_buffer[MAX_DATA];
list_t * fd_mappings; /** list of hashtables - one hashtable for each process id. The hashtable for process is used for
							  * mappings of file descriptors recorded --> actually used.
							  */

hash_table_t * usage_map; /** hashtables of fds which are used by this process - to track
                          * whether we can really close fd or it is used by another "cloned process"
								  */


#ifndef PY_MODULE
extern struct timeval global_start;
#endif

unsigned long long clock_rate = 0;

struct timeval start_time;


static inline unsigned long long int rdtsc(void) {
	unsigned a, d;

	__asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

	return ((unsigned long long)a) | (((unsigned long long)d) << 32);;
}

unsigned long long  get_clock_rate() {
	unsigned long long x1;
	unsigned long long x2;
	struct timeval tv1, tv2;

	gettimeofday(&tv1, NULL);
	x1 = rdtsc();
	sleep(1);
	x2 = rdtsc();
	gettimeofday(&tv2, NULL);

	return (x2-x1)*1000000/TIMEVAL_DIFF(tv2, tv1);
}

#define REPLICATE(x) x##_it = (x##_item_t *) com_it; \
				if ( i==1 ) /*first thing on the list!*/ {\
					if( replicate_init(x##_it->o.info.pid, cpu, ifilename, mfilename))\
						return -1; \
					last_call_orig = CALL_TIME(x##_it); \
					first_call_orig = CALL_TIME(x##_it); \
					counter_first = rdtsc(); \
					counter_last = rdtsc(); \
				}\
				/** wait for delivering of next call, if enabled */\
				if ( op_mask & TIME_DIFF ) { \
					diff_orig = CALL_TIME(x##_it) - last_call_orig; \
					diff_orig = diff_orig * scale; \
					counter_real = rdtsc(); \
					diff_real = ((counter_real - counter_last)*1000000)/(clock_rate); \
					diff = diff_orig - diff_real; \
					/* fprintf(stderr, "orig:%"PRIi64", counter_diff: %"PRIu64", real: %"PRIi64" , diff:%"PRIi64"\n", diff_orig, (counter_real - counter_last), diff_real, diff); */\
					while (diff > 60) { \
						int y = 0; \
					   for(y= 0; y < 20000;) { \
							y+=1; \
						} \
						counter_real = rdtsc(); \
						diff_real = ((counter_real - counter_last)*1000000)/(clock_rate); \
						diff = diff_orig - diff_real; \
					} \
				} else if ( op_mask & TIME_EXACT ) { \
					diff_orig = CALL_TIME(x##_it) - first_call_orig; \
					counter_real = rdtsc(); \
					diff_real = ((counter_real - counter_first)*1000000)/(clock_rate); \
					diff = diff_orig - diff_real; \
					while (diff > 60) { \
						int y = 0; \
					   for(y= 0; y < 20000;) { \
							y+=1; \
						} \
						counter_real = rdtsc(); \
						diff_real = ((counter_real - counter_first)*1000000)/(clock_rate); \
						diff = diff_orig - diff_real; \
					} \
					\
				}\
				replicate_##x(x##_it, op_mask);\
				if ( op_mask & TIME_DIFF ) { \
					last_call_orig = CALL_TIME(x##_it) + DUR_TIME(x##_it); \
					counter_last = rdtsc(); \
				} \

int supported_type(mode_t type) {
	if ( type == S_IFDIR || type == S_IFREG ) {
		return  1;
	} else {
		return 0;
	}
}

/** Replicates one read operation.
 * @arg op_it operation item structure in which are information about the write operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_read(read_item_t * op_it, int op_mask) {
	int64_t retval = 0;
	int fd = op_it->o.fd;
	fd_item_t * fd_item;
	int myfd;
	item_t * fd_map;
	char * data;
	int32_t pid = op_it->o.info.pid;
	hash_table_t * ht;

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist!\n", pid);
		return;
	}

	if ( (fd_map = hash_table_find(ht, &fd)) == NULL) {
		ERRORPRINTF("%d: Can not find mapping for fd: %d. Corresponding open call probably missing. Time:%d.%d\n", pid, fd,  op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec);
		//hash_table_dump2(ht, dump_fd_list_item);
	} else {
		fd_item = hash_table_entry(fd_map, fd_item_t, item);
		myfd = fd_item->fd_map->my_fd;

		if ( ! supported_type(fd_item->fd_map->type)) {
			//DEBUGPRINTF("Unsupported fd (%d -> %d) type: %d\n", fd, myfd, fd_item->fd_map->type);
			return;
		}
		
		if (op_it->o.size > MAX_DATA) {
			data = malloc(op_it->o.size);
		} else {
			data = data_buffer;
		}		

		if (op_mask & SIM_MASK) {
			retval = op_it->o.retval;
			if (op_it->o.retval != -1) { //do not take unsuccessfull reads into account
				simulate_read(fd_item, op_it);
			}
		} else if ( op_mask & REP_MASK) {
			retval = read(myfd, data_buffer, op_it->o.size);
		} else {
			assert(0);
		}
		fd_item->fd_map->cur_pos += retval;
	
		if ( op_it->o.size > MAX_DATA) {
			free(data);
		}

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("%d: Read from fd %d->%d failed: %s\n", pid, fd, myfd, strerror(errno));
//			hash_table_dump2(ht, dump_fd_list_item);
		} else if (retval != op_it->o.size && retval != op_it->o.retval) {
			DEBUGPRINTF("Warning, not every data were read! Only %"PRIi64" bytes were \
successfully read (expected: %"PRIi64")\n", retval, op_it->o.retval);
		}
	}
}

/** Replicates one write operation.
 * @arg op_it operation item structure in which are information about the write operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_write(write_item_t * op_it, int op_mask) {
	int64_t retval = 0;
	int fd = op_it->o.fd;
	fd_item_t * fd_item;
	int myfd;
	int32_t pid = op_it->o.info.pid;
	item_t * fd_map;
	char * data;
	hash_table_t * ht;

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist!\n", pid);
		return;
	}

	if ( (fd_map = hash_table_find(ht, &fd)) == NULL) {
		ERRORPRINTF("%d: Can not find mapping for fd: %d. Corresponding open call probably missing.\n", pid, fd);
	} else {
		fd_item = hash_table_entry(fd_map, fd_item_t, item);
		myfd = fd_item->fd_map->my_fd;

		mode_t type = fd_item->fd_map->type;
		if ( ! supported_type(type)) {
			//DEBUGPRINTF("Unsupported fd (%d -> %d) type: %d\n", fd, myfd, type);
			return;
		}

		if (op_it->o.size > MAX_DATA) {
			data = malloc(op_it->o.size);
		} else {
			data = data_buffer;
		}		

		if (op_mask & SIM_MASK) {
			retval = op_it->o.retval;
			if (op_it->o.retval != -1) { //do not take unsuccessfull writes into account
				simulate_write(fd_item, op_it);
			}
		} else if ( op_mask & REP_MASK) {
			retval = write(myfd, data_buffer, op_it->o.size);
		} else {
			assert(0);
		}

		fd_item->fd_map->cur_pos += retval;

		if ( op_it->o.size > MAX_DATA) {
			free(data);
		}

		if (retval == -1) {
			ERRORPRINTF("Write to fd %d failed: %s\n", fd, strerror(errno));
		} else if (retval != op_it->o.size) {
			DEBUGPRINTF("Warning, not every data were written! Only %"PRIi64" bytes were successfully outputed\n", retval);
		}
	}
}

inline int32_t get_pipe_fd() {
	static int32_t fd = INT_MAX;
	fd--;
	return fd;
}

inline int32_t get_socket_fd() {
	static int32_t fd = 100000000+1;
	fd--;
	return fd;
}

/** Replicates one pipe(2) operation. It actually does not call pipe syscall, but just keep track of fds.
 * @arg op_it operation item structure in which are information about the open/creat operation
 */

void replicate_pipe(pipe_item_t * op_it, int op_mask) {
	int32_t retval = op_it->o.retval;
	hash_table_t * ht;
	int32_t pid = op_it->o.info.pid;
	fd_item_t * fd_item;;
	int32_t fd1 = op_it->o.fd1;
	int32_t fd2 = op_it->o.fd2;

	if (retval == -1) { //original open call failed, don't do anything
		DEBUGPRINTF("Original pipe(2) call failed, skipping%s", "\n");
		return;
	}

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		DEBUGPRINTF("HT for pid %d doesn't exist! Clone/fork call missing\n", pid);
		return;
	}

	if ( hash_table_find(ht, &fd1) == NULL && hash_table_find(ht, &fd2) == NULL ) { //we didn't open any fd before
		fd_item = new_fd_item();
		fd_item->fd_map->my_fd = get_pipe_fd();
		fd_item->fd_map->type = S_IFIFO;
		fd_item->old_fd = fd1;
		insert_parent_fd(fd_item, fd1);
		hash_table_insert(ht, &fd1, &fd_item->item);
		increase_fd_usage(usage_map, fd1);

		fd_item = new_fd_item();
		fd_item->fd_map->my_fd = get_pipe_fd();
		fd_item->fd_map->type = S_IFIFO;
		fd_item->old_fd = fd2;
		insert_parent_fd(fd_item, fd2);
		hash_table_insert(ht, &fd2, &fd_item->item);
		increase_fd_usage(usage_map, fd2);

//		DEBUGPRINTF("%d: Pipes %d and %d inserted.\n", pid, fd1, fd2);
	} else {
		ERRORPRINTF("%d(%d.%d): One of the fds: %d %d already opened!\n", pid, op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, fd1, fd2);
	}
}

/** Replicates one open(2)/creat(2) operation.
 * @arg op_it operation item structure in which are information about the open/creat operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_open(open_item_t * op_it, int op_mask) {
	int retval;
	char * name = NULL;
	int fd = op_it->o.retval;
	int flags = 0;
	hash_table_t * ht;
	int32_t pid = op_it->o.info.pid;
	fd_item_t * fd_item = new_fd_item();

	if (fd == -1) { //original open call failed, just replicate it	
		name = namemap_get_name(op_it->o.name);
		if ( name == NULL ) { // I should ignore it
			//DEBUGPRINTF("Ignoring opening of file %s\n", op_it->o.name);
			return;
		}
		if (op_mask & REP_MASK) {
			retval = open(name, flags);
		} else {
			if (op_mask & SIM_MASK) {
				if (name != op_it->o.name) {
					strcpy(op_it->o.name, name);
				}
				simulate_creat(&op_it->o);
			}
			retval = -1;
		}
      if (retval != -1) {
			ERRORPRINTF("%d: Error replicating originally failed open call with file %s\n", pid, name);
			close(retval);
		}
		delete_fd_item(fd_item);
		return;
	}

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist! Clone/fork call missing\n", pid);
		delete_fd_item(fd_item);
		return;
	}

	if ( hash_table_find(ht, &fd) == NULL ) { //we didn't open this file before
		flags = op_it->o.flags;

		name = namemap_get_name(op_it->o.name);
		if ( name == NULL ) { // I should ignore it
			//DEBUGPRINTF("Ignoring opening of file %s\n", op_it->o.name);
			flags |= O_IGNORE;
			name = op_it->o.name;
		}

		if (op_mask & REP_MASK && ! (flags & O_IGNORE) ) { //i should replicate and not ignore it
			if (op_it->o.mode == MODE_UNDEF) { //we know, that we don't want to use mode flag at all
				retval = open(name, flags);
			} else {
				retval = open(name, flags, op_it->o.mode);
			}
		} else { // SIM_MASK or O_IGNORE
			if (op_it->o.name != name) {
				strcpy(op_it->o.name, name);
			}
			if (op_mask & SIM_MASK && ! (flags & O_IGNORE)) {
				simulate_creat(&op_it->o);
			}
			retval = simulate_get_open_fd();
		}

		if (retval == -1) {
			ERRORPRINTF("Open of file %s failed: %s\n", name, strerror(errno));
		} else { //everything went OK, lets insert it into our mapping
			fd_item->fd_map->my_fd = retval;
			fd_item->fd_map->cur_pos = 0;
			fd_item->fd_map->time_open = op_it->o.info.start;
			strncpy(fd_item->fd_map->name, name, MAX_STRING);
			fd_item->fd_map->name[MAX_STRING-1] = 0; //just to make sure it will be terminated
			fd_item->old_fd = op_it->o.retval;
			fd_item->fd_map->created = flags & O_CREAT;

			insert_parent_fd(fd_item, op_it->o.retval);
			
			if ( flags & O_IGNORE ) {
				fd_item->fd_map->type = S_IFIGNORE;
			} else if ( flags & O_DIRECTORY ) {
				fd_item->fd_map->type = S_IFDIR;
			} else {					
				fd_item->fd_map->type = S_IFREG;
			}

			hash_table_insert(ht, &op_it->o.retval, &fd_item->item);
			increase_fd_usage(usage_map, retval);
			//DEBUGPRINTF("%d: File %s inserted with key %d->%d\n", pid, name, op_it->o.retval, retval);
		}
	} else {
		ERRORPRINTF("%d: File %s is already opened!\n", pid, op_it->o.name);
		delete_fd_item(fd_item);
	}
}

/** Replicate clone operation.
 * @arg op_it operation item structure in which are information about the close operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_clone(clone_item_t * op_it, int op_mask) {
	//item_t * item;
	int32_t pid = op_it->o.retval;
	hash_table_t * ht;

	//sanity check:
	if ( (ht = get_process_ht(fd_mappings, pid)) != NULL) {
		ERRORPRINTF("Table for process %d already exist!\n", pid);
		return;
	}

	item_t * item = new_process_ht(pid);
	process_hash_item_t * h_it = list_entry(item, process_hash_item_t, list_item);
	if (op_it->o.mode & CLONE_FILES) { //we should have the same FD table
		free(h_it->ht); //we don't want our own ht, because we just want a pointer to the same list
		h_it->ht = get_process_ht(fd_mappings, op_it->o.info.pid);
	} else { //we will just copy FD table
		free(h_it->ht);
		h_it->ht  = duplicate_process_ht(get_process_ht(fd_mappings, op_it->o.info.pid), usage_map);	
	}

	list_append(fd_mappings, &h_it->list_item);
	//hash_table_dump2(h_it->ht, dump_fd_list_item);
	//list_dump(fd_mappings, dump_process_hash_list_item);
}

/** Replicates one close operation.
 * @arg op_it operation item structure in which are information about the close operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_close(close_item_t * op_it, int op_mask) {
	int retval;
	int fd = op_it->o.fd;
	int myfd;
	item_t * item;
	fd_item_t * fd_item;
	fd_map_t * fd_map = NULL;
	int32_t pid = op_it->o.info.pid;
	hash_table_t * ht;
	int last = 0;

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist yet!!!\n", pid);
		return;
	}

	if ( (item = hash_table_find(ht, &fd)) == NULL) { //we didn't open this file before
		if ( op_it->o.retval != -1 ) {
			ERRORPRINTF("%d: File descriptor %d is not opened!\n", pid, fd);
		}
	} else {	
		fd_item = list_entry(item, fd_item_t, item);	
		myfd = fd_item->fd_map->my_fd;
	
		//Maybe it was just a pipe or socket?
		if ( ! supported_type(fd_item->fd_map->type)) {
			retval = 0;
		} else {
			if (decrease_fd_usage(usage_map, myfd)) { // it was the last one, really close it
				if (op_mask & REP_MASK) {
					retval = close(myfd);
				} else { //simulating mode
					retval = 0;
				}						
			} else {
				retval = 0; //don't close it, another "process" have it open
			}
		}
	
		if (retval == -1) {
			ERRORPRINTF("%d: Close of file with fd %d->%d failed: %s\n", pid, fd, myfd, strerror(errno));
		} else { //everything went OK, lets remove it from the mapping
			last = delete_parent_fd(fd_item, fd);
			fd_map = fd_item->fd_map;

			hash_table_remove(ht, &fd);
			if (last) {
				assert(fd_map);
				free(fd_map); //See comment 4 lines above
			}
//			DEBUGPRINTF("%d: Mapping of fd: %d->%d removed\n", pid, fd, myfd);
		}
//		hash_table_dump2(ht, dump_fd_list_item);
	}
}

/** Replicates one unlink operation.
 * @arg op_it operation item structure in which are information about the unlink operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_unlink(unlink_item_t * op_it, int op_mask) {
	int retval;
	char * name;
	
	name = namemap_get_name(op_it->o.name);
	if ( name == NULL ) { // I should ignore it
		return;
	} 

	if ( op_mask & REP_MASK) {
		retval = unlink(name);

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("Unlink of file with %s failed (which was not expected): %s\n", name, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("Unlink result of file %s other than expected: %d\n", name, retval);
		}
	} else if ( op_mask & SIM_MASK ) {
		simulate_unlink(&op_it->o);
	}
}

/** Replicates one _llseek operation. This syscall is apparently only available on x86 kernels, but we simulate 
 * actual seek on 64bit machines too.
 *
 * @arg op_it operation item structure in which are information about the _llseek operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_llseek(llseek_item_t * op_it, int op_mask) {
	int64_t retval;
	loff_t result = 0;
	int fd = op_it->o.fd;
	int myfd;
	item_t * fd_map;
	fd_item_t * fd_item;
	int32_t pid = op_it->o.info.pid;
	hash_table_t * ht;
	unsigned long high, low;

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist yet!!!\n", pid);
		return;
	}

	high = (off_t) (op_it->o.offset>>32);
	low = (off_t) (op_it->o.offset & 0xFFFFFFFF);

   if ( (fd_map = hash_table_find(ht, &fd)) == NULL) {
      ERRORPRINTF("%d: Can not find mapping for fd: %d. Corresponding open call probably missing.\n", pid, fd);
   } else {
      fd_item = hash_table_entry(fd_map, fd_item_t, item);
      myfd = fd_item->fd_map->my_fd;

		mode_t type = fd_item->fd_map->type;
		if ( ! supported_type(type)) {
//			DEBUGPRINTF("%d: Unsupported fd (%d -> %d) type: %d\n", pid, fd, myfd, type);
			return;
		}
	
		if ( op_mask & REP_MASK) {
#ifdef SYS__llseek //32bit machine
			retval = syscall(SYS__llseek, myfd, high, low, &result, op_it->o.flag);
#else 
			retval = lseek(myfd, op_it->o.offset, op_it->o.flag);
#endif
		} else {
			result = op_it->o.f_offset;
			retval = op_it->o.retval;
		}

#ifdef SYS__llseek //32bit machine
		if (retval == -1 && retval != op_it->o.retval) {
#else 
		if (retval == (off_t) -1 && retval != op_it->o.retval) {
#endif			
			ERRORPRINTF("lseek with time %d.%d of file with %d failed (which was not expected): %s, %d\n",
					op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.fd, strerror(errno), errno);
		} else if (result != op_it->o.f_offset) {
			ERRORPRINTF("_llseek's final offset (%"PRIi64") is different from what expected(%"PRIi64")\n",
					result, op_it->o.f_offset);
			if (op_mask & SIM_MASK) {
				fd_item->fd_map->cur_pos = op_it->o.f_offset;
			}
		} else {
			if (op_mask & SIM_MASK) {
				fd_item->fd_map->cur_pos = op_it->o.f_offset;
			}
		}
	}
}

/** Replicates one lseek operation. This call apparently only exists on 64bit systems, but we simulate it
 *  using _llseek on 32bit systems too.
 *
 * @arg op_it operation item structure in which are information about the lseek operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_lseek(lseek_item_t * op_it, int op_mask) {

#ifdef SYS__llseek //32bit machine, simulate it through _llseek call
	llseek_item_t * llop_it = new_llseek_item();
	memcpy(&(llop_it->o.info), &(op_it->o.info), sizeof(op_info_t));
	llop_it->o.fd = op_it->o.fd;
	llop_it->o.offset = op_it->o.offset;
	llop_it->o.flag = op_it->o.flag;
	llop_it->o.f_offset = op_it->o.retval;
	llop_it->o.retval = op_it->o.retval;
	replicate_llseek(llop_it, op_mask);
	free(llop_it);
#else 
	int64_t retval;
	int32_t fd = op_it->o.fd;
	int32_t myfd;
	item_t * fd_map;
	fd_item_t * fd_item;
	int32_t pid = op_it->o.info.pid;
	hash_table_t * ht;
	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist yet!!!\n", pid);
		return;
	}

   if ( (fd_map = hash_table_find(ht, &fd)) == NULL) {
      ERRORPRINTF("%d: Can not find mapping for fd: %d. Corresponding open call probably missing.\n", pid, fd);
   } else {
      fd_item = hash_table_entry(fd_map, fd_item_t, item);
      myfd = fd_item->fd_map->my_fd;

		mode_t type = fd_item->fd_map->type;
		if ( ! supported_type(type)) {
//			DEBUGPRINTF("%d: Unsupported fd (%d -> %d) type: %d\n", pid, fd, myfd, type);
			return;
		}
	
		if ( op_mask & REP_MASK) {
			retval = lseek(myfd, op_it->o.offset, op_it->o.flag);
		} else {
			retval = op_it->o.retval;
		}

		if (retval == (off_t) -1 && retval != op_it->o.retval) {
			ERRORPRINTF("lseek with time %d.%d of file with %d failed (which was not expected): %s\n",
					op_it->o.info.start.tv_sec, op_it->o.info.start.tv_usec, op_it->o.fd, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("lseek's final offset (%"PRIi64") is different from what expected(%"PRIi64")\n",
					retval, op_it->o.retval);
			if (op_mask & SIM_MASK) {
				fd_item->fd_map->cur_pos = retval;
			}
		} else {
			if (op_mask & SIM_MASK) {
				fd_item->fd_map->cur_pos = retval;
			}
		}
	}
#endif
}

/** Replicates one mkdir operation.
 * @arg op_it operation item structure in which are information about the _llseek operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_mkdir(mkdir_item_t * op_it, int op_mask) {
	int retval;
	char * name;

	name = namemap_get_name(op_it->o.name);
	if ( name == NULL ) { // I should ignore it
		return;
	}

	if (op_mask & REP_MASK) {
		retval = mkdir(name, op_it->o.mode);

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("Mkdir of file with %s failed (which was not expected): %s\n", name, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("Mkdir result of file %s other than expected: %d\n", name, retval);
		}
	} else if ( op_mask & SIM_MASK ) {
		simulate_mkdir(&op_it->o);
	}
}

/** Replicates one rmdir operation.
 * @arg op_it operation item structure in which are information about the _llseek operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_rmdir(rmdir_item_t * op_it, int op_mask) {
	int retval;
	char * name;

	name = namemap_get_name(op_it->o.name);
	if ( name == NULL ) { // I should ignore it
		return;
	}

	if (op_mask & REP_MASK) {
		retval = rmdir(name);

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("Rmdir of file with %s failed (which was not expected): %s\n", name, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("Rmdir result of file %s other than expected: %d\n", name, retval);
		}
	} else if ( op_mask & SIM_MASK ) {
		simulate_rmdir(&op_it->o);
	}
}


/** Replicates one dup operation. It actually don't call the operation itself, but only
 * keeps track of what it did in original process .
 *
 * @arg op_it operation item structure in which are information about the _llseek operation
 * @arg op_mask whether really replicate or just simulate it. SIM_MASK or REP_MASK.
 */

void replicate_dup(dup_item_t * op_it, int op_mask) {
	int32_t pid = op_it->o.info.pid;
	hash_table_t * ht;
	fd_item_t * fd_item;
	fd_item_t * fd_item_new;
	item_t * it;
	int fd = op_it->o.old_fd;
	int new_fd = op_it->o.retval;

	if (op_it->o.retval == -1) { //dup call failed, which was not expected
		DEBUGPRINTF("Previous duplicate call failed, doing nothing%s", "\n");
		return;
	}

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist yet!!!\n", pid);
		return;
	}
   if ( (it = hash_table_find(ht, &fd)) == NULL) {
      ERRORPRINTF("Can not find mapping for fd: %d. Corresponding open call probably missing.\n", fd);
   } else {
      fd_item = hash_table_entry(it, fd_item_t, item);
		fd_item_new = new_fd_item();
		fd_item_new->old_fd = new_fd;
		free(fd_item_new->fd_map); // we will use the same fd_map as previous fd, because we are just dupl.
		fd_item_new->fd_map = fd_item->fd_map;
		insert_parent_fd(fd_item, new_fd);
//		DEBUGPRINTF("%d: Duplicating fd %d->%d to %d->%d.\n", pid, fd, fd_item->fd_map->my_fd, new_fd,
//				fd_item->fd_map->my_fd);
		hash_table_insert(ht, &new_fd, &fd_item_new->item);
		increase_fd_usage(usage_map, fd_item_new->fd_map->my_fd);
	}
}

/** Replicates one access operation.
 * @arg op_it operation item structure in which are information about the _llseek operation
 */

void replicate_access(access_item_t * op_it, int op_mask) {
	int retval;
	char * name;

	name = namemap_get_name(op_it->o.name);
	if ( name == NULL ) { // I should ignore it
		return;
	} else {
		if (name != op_it->o.name) {
			strcpy(op_it->o.name, name);
		}
	}
	
	if (op_mask & REP_MASK) {
		retval = access(op_it->o.name, op_it->o.mode);

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("Access of file with %s failed (which was not expected): %s\n", op_it->o.name, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("Access result of file %s other than expected: %d\n", op_it->o.name, retval);
		}
	} else if (op_mask & SIM_MASK) {
		simulate_access(&op_it->o);
	}
}

/** Replicates one stat operation.
 * @arg op_it operation item structure in which are information about the _llseek operation
 */

void replicate_stat(stat_item_t * op_it, int op_mask) {
	int retval;
	char * name;
	struct stat st_buf;

	name = namemap_get_name(op_it->o.name);
	if ( name == NULL ) { // I should ignore it
		return;
	} else {
		if (name != op_it->o.name) {
			strcpy(op_it->o.name, name);
		}
	}
	
	if (op_mask & REP_MASK) {
		retval = stat(op_it->o.name, &st_buf);

		if (retval == -1 && retval != op_it->o.retval) {
			ERRORPRINTF("Stat on file with %s failed (which was not expected): %s\n", op_it->o.name, strerror(errno));
		} else if (retval != op_it->o.retval) {
			ERRORPRINTF("Stat result of file %s other than expected: %d\n", op_it->o.name, retval);
		}
	} else if (op_mask & SIM_MASK) {
		simulate_stat(&op_it->o);
	}
}

/** Replicates one socket(2) operation. It just keep tracks track of fd_mapping, no actual socket
 * calls are performed.
 *
 * @arg op_it operation item structure in which are information about the open/creat operation
 */

void replicate_socket(socket_item_t * op_it, int op_mask) {
	int retval;
	int fd = op_it->o.retval;
	hash_table_t * ht;
	int32_t pid = op_it->o.info.pid;

	if (fd == -1) { //original scoket call failed, just exit
		retval = -1;
		return;
	}

	ht = get_process_ht(fd_mappings, pid);

	if (! ht) {
		ERRORPRINTF("HT for pid %d doesn't exist! Clone/fork call missing\n", pid);
		return;
	}

	if ( hash_table_find(ht, &fd) == NULL ) { //we didn't open this file before
		fd_item_t * fd_item = new_fd_item();	
		retval = get_socket_fd();

		fd_item->fd_map->my_fd = retval;
		fd_item->old_fd = fd;
		fd_item->fd_map->time_open = op_it->o.info.start;
		fd_item->fd_map->name[MAX_STRING-1] = 0; //just to make sure it will be terminated
		fd_item->fd_map->type = S_IFSOCK;
		insert_parent_fd(fd_item, op_it->o.retval);
		hash_table_insert(ht, &fd, &fd_item->item);
		increase_fd_usage(usage_map, retval);
//		DEBUGPRINTF("%d: Socket inserted with key %d->%d\n", pid, fd, retval);
//		hash_table_dump2(ht, dump_fd_list_item);
	} else {
		ERRORPRINTF("%d: Fd %d is already opened!\n", pid, fd);
	}
}

/** This functions initialize all structures that are needed. It also creates initial mappings of FDs such as
 * stdin, stdout and stderr.
 *
 * @args fd_mappings list of hash tables of processes
 * @arg pid pid of the main process
 */

int replicate_init(int32_t pid, int cpu, char * ifilename, char * mfilename) {
	fd_item_t * fd_item;
	hash_table_t * ht;
	int i;

#ifndef PY_MODULE
	/* Make sure we are bounded only to first processor. This is important when using program counter as they differ per processor */
	cpu_set_t mask;; /* processors to bind */
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	unsigned int len = sizeof(mask);
	if (sched_setaffinity(0, len, (cpu_set_t *)&mask) < 0) {
	    perror("sched_setaffinity");
	}
	fprintf(stderr, "Determining clock rate..");
	clock_rate = get_clock_rate();
	fprintf(stderr, ": %lfMHz\n", (double)(clock_rate)/1000000.0);
#endif

	//prepare list of files to ignores + list of mapped files
	if ( namemap_init(ifilename, mfilename) ) {
		return -1;
	}

	fd_mappings = malloc(sizeof(list_t));
	usage_map = malloc(sizeof(hash_table_t));

	list_init(fd_mappings); 

	//Init usage_map
	hash_table_init(usage_map, HASH_TABLE_SIZE, &ht_ops_fdusage);


	//create a new ht for the process
	DEBUGPRINTF("Initializing with pid %d\n", pid);
	item_t * item = new_process_ht(pid);
	process_hash_item_t * h_it = list_entry(item, process_hash_item_t, list_item);
	list_append(fd_mappings, item);
	ht = h_it->ht;

	fd_item = new_fd_item();
	fd_item->old_fd = STDIN_FILENO;
	fd_item->fd_map->my_fd = STDIN_FILENO;
	strncpy(fd_item->fd_map->name, "stdin", MAX_STRING);
	fd_item->fd_map->type = FT_SPEC; // in reality, this should by S_IFREG, but we need some special handling
	insert_parent_fd(fd_item, STDIN_FILENO);
	i = STDOUT_FILENO;
	hash_table_insert(ht, &i, &fd_item->item);
	increase_fd_usage(usage_map,i);

	fd_item = new_fd_item();
	fd_item->old_fd = STDOUT_FILENO;
	fd_item->fd_map->my_fd = STDOUT_FILENO;
	strncpy(fd_item->fd_map->name, "stdout", MAX_STRING);
	fd_item->fd_map->type = FT_SPEC; // in reality, this should by S_IFREG, but we need some special handling
	fd_item->fd_map->type = FT_SPEC; // in reality, this should by S_IFREG, but we need some special handling
	insert_parent_fd(fd_item, STDOUT_FILENO);
	i = STDOUT_FILENO;
	hash_table_insert(ht, &i, &fd_item->item);
	increase_fd_usage(usage_map,i);

	fd_item = new_fd_item();
	fd_item->old_fd = STDERR_FILENO;
	fd_item->fd_map->my_fd = STDERR_FILENO;
	strncpy(fd_item->fd_map->name, "stderr", MAX_STRING);
	fd_item->fd_map->type = FT_SPEC; // in reality, this should by S_IFREG, but we need some special handling
	insert_parent_fd(fd_item, STDERR_FILENO);
	i = STDERR_FILENO;
	hash_table_insert(ht, &i, &fd_item->item);
	increase_fd_usage(usage_map,i);

	DEBUGPRINTF("Initialized%s", "\n");

#ifndef PY_MODULE
	gettimeofday(&start_time, NULL);
	DEBUGPRINTF("Time elapsed so far: %lf\n", TIMEVAL_DIFF(start_time, global_start)/1000000.0);
#endif

	return 0;
}

/** Dealocates all support structures used by replicate.
 */

void replicate_finish() {
	item_t * i;
	process_hash_item_t * process_ht_item;

#ifndef PY_MODULE	
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	DEBUGPRINTF("The replication itself lasted for %lf\n", TIMEVAL_DIFF(cur_time, start_time)/(1000000.0));
	fprintf(stdout, "%lf\n", TIMEVAL_DIFF(cur_time, start_time)/(1000000.0));
#endif

	namemap_finish();

	i = fd_mappings->head;
	while(i) {
		process_ht_item = list_entry(i, process_hash_item_t, list_item);
		i = i->next;
///< @todo get rif of process_map_hts & fd_maps. This is tricky, as they are shared across processes,
//   so one can't just iterate throw them and blindly delete them
//		hash_table_apply(process_ht_item->ht, fd_item_remove_fd_map);
//		hash_table_destroy(process_ht_item->ht);
//		free(process_ht_item->ht);
//		free(process_ht_item);
	}
	
	hash_table_destroy(usage_map);
	free(fd_mappings);
	free(usage_map);

}

/** This function replicates every file operation in the @a list.
 * @arg list list of operations to replicate
 * @arg cpu cpu number to bind this process.
 * @arg scale factor by which to scale time window between calls in TIME_DIFF mode
 * @arg op_mask mode of replication, it can only simulate replication or really duplicate.
 *              This also affects timing behaviour.
 * @arg ifile name of the file containing file names to ignore. NULL to disable this feature.     
 * @arg mfile name of the file containing mapping of file names. Operation will be performed 
 *      on mapped file instead of the recorded one. NULL to disable this feature.     
 *
 * @return zero if succesfull, non-zero otherwise
 * */

int replicate(list_t * list, int cpu, double scale, int op_mask, char * ifilename, char * mfilename) {
	long long i = 0;
	item_t * item = list->head;
	common_op_item_t * com_it;
	read_item_t * read_it;
	write_item_t * write_it;
	open_item_t * open_it;
	close_item_t * close_it;
	unlink_item_t * unlink_it;
	llseek_item_t * llseek_it;
	lseek_item_t * lseek_it;
	clone_item_t * clone_it;
	dup_item_t * dup_it;
	mkdir_item_t * mkdir_it;
	rmdir_item_t * rmdir_it;
	pipe_item_t * pipe_it;
	access_item_t * access_it;
	stat_item_t * stat_it;
	socket_item_t * socket_it;
	uint64_t last_call_orig; ///< when was the last original call made
	uint64_t first_call_orig; ///< when was the first original call made
	int64_t diff_orig, diff_real;
	int64_t diff;
	uint64_t counter_first, counter_last, counter_real;

	while (item) { 
		i++;
		com_it = list_entry(item, common_op_item_t, item);
		switch (com_it->type) {
			case OP_WRITE:
				REPLICATE(write);
				break;
			case OP_READ:
				REPLICATE(read);
				break;
			case OP_OPEN:
				REPLICATE(open);
				break;
			case OP_CLOSE:
				REPLICATE(close);
				break;
			case OP_UNLINK:
				REPLICATE(unlink);
				break;
			case OP_LSEEK:
				REPLICATE(lseek);
				break;
			case OP_LLSEEK:
				REPLICATE(llseek);
				break;
			case OP_CLONE:
				REPLICATE(clone);
				break;
			case OP_MKDIR:
				REPLICATE(mkdir);
				break;
			case OP_RMDIR:
				REPLICATE(rmdir);
				break;
			case OP_DUP:
			case OP_DUP2:
			case OP_DUP3:
				REPLICATE(dup);
				break;
			case OP_PIPE:
				REPLICATE(pipe);
				break;
			case OP_ACCESS:
				REPLICATE(access);
				break;
			case OP_STAT:
				REPLICATE(stat);
				break;
			case OP_SOCKET:
				REPLICATE(socket);
				break;
			default:
				ERRORPRINTF("Unknown operation identifier: '%c'\n", com_it->type);
				return -1;
				break;
		}
		item = item->next;
	}
	replicate_finish();
	return 0;
}
