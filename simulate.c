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

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "simulate.h"
#include "simfs.h"

hash_table_t * sim_map_read = NULL;
hash_table_t * sim_map_write = NULL;
list_t * sim_list_special = NULL;
int sim_mode = 0;

static int ht_compare_sim(key_t *key, item_t *item) {
	sim_item_t * sim_item;

	sim_item = hash_table_entry(item, sim_item_t, item);

	return ! strncmp(sim_item->name, (char *) key, MAX_STRING);
}

static inline void ht_remove_callback_sim(item_t * item) {
	sim_item_t * sim_item = hash_table_entry(item, sim_item_t, item);	
	item_t * i;
	rw_op_t * rw_op;

	//clear list first:
	i = sim_item->list.head;
	while(i) {
		rw_op = list_entry(i, rw_op_t, item);
		i = i->next;
		free(rw_op);		
	}

	free(sim_item);
	return;
}

/** hash table operations. */
static hash_table_operations_t ht_ops_sim = {
	.hash = ht_hash_str,
	.compare = ht_compare_sim,
	.remove_callback = ht_remove_callback_sim /* = NULL if not used */
};

/** Inits all structures needed for simulation. You have to call this function before calling any
 * of other simulate_* functions.
 * @arg mode mode in which to simulateing. Possible options are: ACT_PREPARE, ACT_CHECK and ACT_SIMULATE.
 *           ACT_SIMULATE: Only processes read and writes and make sim_map_read/write hash maps.
 *           ACT_CHECK: Also checks whether file structure on the disks is ok or not.
 *           ACT_PREPARE: Also tries to fix the filesystem. 
 */

void simulate_init(int mode) {
	if (sim_map_read != NULL) {
		ERRORPRINTF("It seems the sim_map_read hash table is already initialized!%s", "\n");
		return;
	}
	if (sim_map_write != NULL) {
		ERRORPRINTF("It seems the sim_map_read hash table is already initialized!%s", "\n");
		return;
	}
	
	sim_mode = mode;

	sim_map_read = malloc(sizeof(hash_table_t));
	sim_map_write = malloc(sizeof(hash_table_t));

	hash_table_init(sim_map_read, HASH_TABLE_SIZE, &ht_ops_sim);
	hash_table_init(sim_map_write, HASH_TABLE_SIZE, &ht_ops_sim);
	simfs_init();
}

/** Frees all variables used for simulations. Call this function at the end, as you will not be able
 * to get any simulation information after calling it.
 */

void simulate_finish() {
	if (sim_map_read == NULL) {
		ERRORPRINTF("Sim_map_read already freed. Double finish?%s", "\n");
		return;
	}
	if (sim_map_write == NULL) {
		ERRORPRINTF("Sim_map_write already freed. Double finish?%s", "\n");
		return;
	}
	hash_table_destroy(sim_map_read);
	hash_table_destroy(sim_map_write);
	
				
	free(sim_map_read);
	free(sim_map_write);
	sim_map_read = NULL;
	sim_map_write = NULL;

	DEBUGPRINTF("going to finish simfs%s","\n");
	simfs_finish();
}

inline sim_item_t * simulate_get_sim_item(fd_item_t * fd_item, hash_table_t * ht) {
	item_t * item;
	sim_item_t * sim_item;

	if ( (item = hash_table_find(ht, (key_t *)(fd_item->fd_map->name))) == NULL) {
		sim_item = malloc(sizeof(sim_item_t));
		sim_item->time_open = fd_item->fd_map->time_open;
		sim_item->created = fd_item->fd_map->created;
		strncpy(sim_item->name, fd_item->fd_map->name, MAX_STRING);
		list_init(&sim_item->list);
		item_init(&sim_item->item);
		hash_table_insert(ht, (key_t *) sim_item->name, &sim_item->item);
	} else {
		sim_item = hash_table_entry(item, sim_item_t, item);
	}
	return sim_item;
}


inline void simulate_append_rw(sim_item_t * sim_item, int64_t size, int64_t offset, struct int32timeval start, int32_t dur, int64_t retval) {
	rw_op_t * rw_op = malloc(sizeof(rw_op_t));
	item_init(&rw_op->item);
	rw_op->size = retval; //we notice only how many bytes were actually read/written, not tried
	rw_op->offset = offset;
	rw_op->start = start;
	rw_op->dur = dur;

	list_append(&sim_item->list, &rw_op->item);
}


inline void simulate_write(fd_item_t * fd_item, write_item_t * op_it) {
	simfs_t * simfs = simfs_find(fd_item->fd_map->name);
	sim_item_t * sim_item = NULL;
	uint64_t off;

	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		if ( ! simfs) {
			DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created,unlinked,written and then closed)\n", fd_item->fd_map->name);
			return;
		}

		//offset is changed in replicate functions, this is to be changed.
		off = fd_item->fd_map->cur_pos + op_it->o.retval; 

		if (simfs->virt_size < off) {
			simfs->virt_size = off;
		}

		if (simfs->physical) {
			if ( fd_item->fd_map->cur_pos > simfs->phys_size ) {
				ERRORPRINTF("Write to file %s on pos %"PRIu64" would fail as the current position is behind end of the file(%"PRIu64").\n",
						fd_item->fd_map->name, fd_item->fd_map->cur_pos, simfs->phys_size);
			} else {
				if (simfs->phys_size < off) {
					simfs->phys_size = off;
				}
			}
		}
	}

	if ( sim_mode & ACT_SIMULATE) {
		sim_item = simulate_get_sim_item(fd_item, sim_map_write);
		simulate_append_rw(sim_item, op_it->o.size, fd_item->fd_map->cur_pos, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
	}
}


inline void simulate_read(fd_item_t * fd_item, read_item_t * op_it) {
	simfs_t * simfs = simfs_find(fd_item->fd_map->name);
	sim_item_t * sim_item = NULL;
	uint64_t off;

	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		if ( ! simfs) {
			DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created/unlinked and then written)\n", fd_item->fd_map->name);
			return;
		}

		off = fd_item->fd_map->cur_pos + op_it->o.retval;
		if (simfs->virt_size < off) {
			simfs->virt_size = off;
		}
	}
	if (sim_mode & ACT_SIMULATE) {
		sim_item = simulate_get_sim_item(fd_item, sim_map_read);
		simulate_append_rw(sim_item, op_it->o.size, fd_item->fd_map->cur_pos, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
	}
}

inline void simulate_sendfile(fd_item_t * in_fd_item, fd_item_t * out_fd_item, sendfile_item_t * op_it) {
	simfs_t * in_simfs;
	simfs_t * out_simfs;
	sim_item_t * sim_item_read = NULL;
	sim_item_t * sim_item_write = NULL;
	uint64_t off; 
	
	out_simfs = simfs_find(out_fd_item->fd_map->name);

	if (in_fd_item) {
		in_simfs = simfs_find(in_fd_item->fd_map->name);
		if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
			if ( ! in_simfs) {
				DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created/unlinked and then written)\n", in_fd_item->fd_map->name);
				return;
			}

			if ( op_it->o.offset == OFFSET_INVAL) {
				off = in_fd_item->fd_map->cur_pos + op_it->o.retval;
			} else {
				off = op_it->o.offset + op_it->o.retval;
			}

			if (in_simfs->virt_size < off) {
				in_simfs->virt_size = off;
			}
		}
		if (sim_mode & ACT_SIMULATE) {
			sim_item_read = simulate_get_sim_item(in_fd_item, sim_map_read);
			if ( op_it->o.offset == OFFSET_INVAL) {
				off = in_fd_item->fd_map->cur_pos;
			} else {
				off = op_it->o.offset;
			}
			simulate_append_rw(sim_item_read, op_it->o.size, off, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
		}
	}
	if (out_fd_item) {
		out_simfs = simfs_find(out_fd_item->fd_map->name);
		if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
			if ( ! out_simfs) {
				DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created/unlinked and then written)\n", out_fd_item->fd_map->name);
				return;
			}

			off = out_fd_item->fd_map->cur_pos + op_it->o.retval;

			if (out_simfs->virt_size < off) {
				out_simfs->virt_size = off;
			}
		}
		if (sim_mode & ACT_SIMULATE) {
			sim_item_write = simulate_get_sim_item(out_fd_item, sim_map_write);
			off = out_fd_item->fd_map->cur_pos;
			simulate_append_rw(sim_item_write, op_it->o.size, off, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
		}
	}
}

inline void simulate_pwrite(fd_item_t * fd_item, pwrite_item_t * op_it) {
	simfs_t * simfs = simfs_find(fd_item->fd_map->name);
	sim_item_t * sim_item = NULL;
	uint64_t off;

	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		if ( ! simfs) {
			DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created,unlinked,written and then closed)\n", fd_item->fd_map->name);
			return;
		}
		off = fd_item->fd_map->cur_pos;

		if (simfs->virt_size < off) {
			simfs->virt_size = off;
		}

		if (simfs->physical) {
			if ( fd_item->fd_map->cur_pos > simfs->phys_size ) {
				ERRORPRINTF("Pwrite to file %s on pos %"PRIu64" would fail as the current position is behind end of the file(%"PRIu64").\n",
						fd_item->fd_map->name, fd_item->fd_map->cur_pos, simfs->phys_size);
			} else {
				if (simfs->phys_size < off) {
					simfs->phys_size = off;
				}
			}
		}
	}

	if ( sim_mode & ACT_SIMULATE) {
		sim_item = simulate_get_sim_item(fd_item, sim_map_write);
		simulate_append_rw(sim_item, op_it->o.size, op_it->o.offset, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
	}
}


inline void simulate_pread(fd_item_t * fd_item, pread_item_t * op_it) {
	simfs_t * simfs = simfs_find(fd_item->fd_map->name);
	sim_item_t * sim_item = NULL;
	uint64_t off;

	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		if ( ! simfs) {
			DEBUGPRINTF("Entry for %s in simfs missing, which might be OK (e.g. tmp file created/unlinked and then written)\n", fd_item->fd_map->name);
			return;
		}

		off = fd_item->fd_map->cur_pos;
		if (simfs->virt_size < off) {
			simfs->virt_size = off;
		}
	}
	if (sim_mode & ACT_SIMULATE) {
		sim_item = simulate_get_sim_item(fd_item, sim_map_read);
		simulate_append_rw(sim_item, op_it->o.size, op_it->o.offset, op_it->o.info.start, op_it->o.info.dur, op_it->o.retval);
	}
}


void simulate_access(access_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_access(op_it);
	}
}

void simulate_stat(stat_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_stat(op_it);
	}
}

void simulate_mkdir(mkdir_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_mkdir(op_it);
	}
}

void simulate_rmdir(rmdir_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_rmdir(op_it);
	}
}

void simulate_unlink(unlink_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_unlink(op_it);
	}
}

void simulate_creat(open_op_t * op_it) {
	if (sim_mode & ACT_CHECK || sim_mode & ACT_PREPARE) {
		simfs_creat(op_it);
	}
}

hash_table_t * simulate_get_map_read() {
	return sim_map_read;
}

hash_table_t * simulate_get_map_write() {
	return sim_map_write;
}


inline int simulate_get_open_fd() {
	static int fd = 1000-1;

	fd++;
	return fd;
}

inline int64_t simulate_get_max_offset(sim_item_t * sim_item) {
	rw_op_t * rw_op;
	uint64_t max_off = 0;
	uint64_t off = 0;
	item_t * i;
	i = sim_item->list.head;

	while (i) {
		rw_op = list_entry(i, rw_op_t, item);
		off = rw_op->offset + rw_op->size;
		if ( off > max_off ) {
			max_off = off;
		}
		i = i->next;
	}
	return max_off;
}


/** Prints filename and its size according to the successful read operations.
 * @arg item item pointer in sim_item_t
 */

void simulate_print_filename_size(item_t * item) {
	uint64_t max_off = 0;
	sim_item_t * sim_item = hash_table_entry(item, sim_item_t, item);

	max_off = simulate_get_max_offset(sim_item);
	printf("%s: ", sim_item->name);

	printf("%"PRIu64"B\n", max_off);
}

/** Checks given file for existence, read permission and if it has enough size. It ignores files that
 * was created by the application itself.
 * @arg item item pointer in sim_item_t
 */

void simulate_check_file_read(item_t * item) {
	struct stat st;
	uint64_t max_off = 0;
	sim_item_t * sim_item = hash_table_entry(item, sim_item_t, item);

	if ( sim_item->created ) { //the file was created, don't control this one
		return;
	}

	max_off = simulate_get_max_offset(sim_item);
	if ( access(sim_item->name, R_OK) != 0 ) {
		ERRORPRINTF("%s: Can't open: %s\n", sim_item->name, strerror(errno));
		return;
	}

	if (simfs_has_file(sim_item->name) == SIMFS_PHYS) {
		stat(sim_item->name, &st);
		if (st.st_size < max_off) {
			ERRORPRINTF("%s: Too small (%"PRIu64"), expected: %"PRIu64" bytes\n", sim_item->name, st.st_size, max_off);
		}
	} else {

	}

	stat(sim_item->name, &st);
	if (st.st_size < max_off) {
		ERRORPRINTF("%s: Too small (%"PRIu64"), expected: %"PRIu64" bytes\n", sim_item->name, st.st_size, max_off);
	}
}

/** Checks whether a) given file exists and is writable or b) path for it exists and destination
 * directory is writable and has enough space. All mkdir calls are taken into account.
 * @arg item item pointer in sim_item_t
 */

void simulate_check_file_write(item_t * item) {
}

/** Prints all files accessed with its size (if applicable) as seen from IO operations.
 */

void simulate_list_files() {
	printf("Read files:\n");
	hash_table_apply(sim_map_read, simulate_print_filename_size);
	printf("\nWrite files:\n");
	hash_table_apply(sim_map_write, simulate_print_filename_size);
}

void simulate_check_file(simfs_t * simfs, char * full_name) {

	if ( simfs->physical) {
		if ( simfs->virt_size > simfs->phys_size) {
			fprintf(stderr, "%s %"PRIu64": File is too small, recreate it.\n",
					full_name, simfs->virt_size);
		}
	} else { //virtual
		if ( simfs_is_file(simfs) && simfs->created == 0) {
			fprintf(stderr, "%s %"PRIu64": File doesn't exist at all.\n", full_name, simfs->virt_size );
		}
	}

	/**
	 * @todo Think about this. Can I delete this comment?
		BE CAREFULL HERE. WHAT ABOUT READS TO THE FILES THAT ARE UNLINKED BEFORE CLOSE?!
	*/
}

/** Checks for existence, size and read/write permission of all files
 * that would be accessed during replication of IO operations. Outputs error for every problematic file.
 */

void simulate_check_files() {
	fprintf(stderr, "Create following files:\n");
	simfs_apply_full_name(simulate_check_file);
}
