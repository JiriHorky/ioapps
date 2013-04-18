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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "fdmap.h"
#include "namemap.h"

static int ht_compare_fdusage(key_t *key, item_t *item) {
	fd_usage_t * fd_usage;

	fd_usage = hash_table_entry(item, fd_usage_t, item);
	return fd_usage->my_fd == *key;
}

static int ht_compare_fditem(key_t *key, item_t *item) {
	fd_item_t * fd_item;

	fd_item = hash_table_entry(item, fd_item_t, item);
	return fd_item->old_fd == *key;
}

static int ht_compare_processhash(key_t *key, item_t *item) {
	process_hash_item_t * p_item;

	p_item = hash_table_entry(item, process_hash_item_t, item);
	return p_item->pid == *key;
}

static inline void ht_remove_callback_fdusage(item_t * item) {
	fd_usage_t * fd_usage = hash_table_entry(item, fd_usage_t, item);	
	free(fd_usage);
	return;
}

static inline void ht_remove_callback_processhash(item_t * item) {
	process_hash_item_t * p_item;
	p_item = hash_table_entry(item, process_hash_item_t, item);
	free(p_item);
	return;
}

inline void ht_remove_callback_fditem(item_t * item) {
	fd_item_t * fd_item = hash_table_entry(item, fd_item_t, item);	
	free(fd_item);
}

/** hash table operations. */
static hash_table_operations_t ht_ops_fditem = {
	.hash = ht_hash_int,
	.compare = ht_compare_fditem,
	.remove_callback = ht_remove_callback_fditem /* = NULL if not used */
};


/** hash table operations. */
///< @todo why is the previous one is static and this can't be using extern?
hash_table_operations_t ht_ops_fdusage = {
	.hash = ht_hash_int,
	.compare = ht_compare_fdusage,
	.remove_callback = ht_remove_callback_fdusage /* = NULL if not used */
};

/** hash table operations. */
///< @todo why is the previous one is static and this can't be using extern?
hash_table_operations_t ht_ops_fdmapping = {
	.hash = ht_hash_int,
	.compare = ht_compare_processhash,
	.remove_callback = ht_remove_callback_processhash /* = NULL if not used */
};

/** Frees fd_map structure of fd_item. Useful when deleting  whole ht_process_map_t using
 * remove callback of ht.
 * @arg item pointer to item_t in fd_item_t structure
 */


void fd_item_remove_fd_map(item_t * item) {
	fd_item_t * fd_item = hash_table_entry(item, fd_item_t, item);

	free(fd_item->fd_map);
}


/** 
 * Returns hash table of fd mappings for process with pid @a pid, or NULL if such 
 * mapping doesn't exist.
 *
 * @arg pid process id to lookup
 * @arg fd_mappings list of hashtables 
 * @return returns hash table of fd mappings for process with pid @a pid, or NULL if such mapping doesn't exist.
 */

hash_table_t * get_process_ht(hash_table_t * fd_mappings, int32_t pid) {
	process_hash_item_t * p_item;
	item_t * process_ht_item;

	if ( (process_ht_item = hash_table_find(fd_mappings, &pid)) != NULL ) {
		p_item = hash_table_entry(process_ht_item, process_hash_item_t, item);
		if ( pid == p_item->pid) {
			return (p_item->ht);
		}
	}
	return NULL;
}


/** Returns pointer to item_t which is part of process_hash_item_t structure. So the
 * caller can append this item to the list of processes fd mappings 
 *
 * @arg pid process id for this hash table
 * @return pointer to item_t which is part of process_hash_item_t structure
 * */

item_t * new_process_ht(int32_t pid) {
	process_hash_item_t * p_ht_it = malloc(sizeof(process_hash_item_t));
	p_ht_it->ht = malloc(sizeof(hash_table_t));

	item_init(&p_ht_it->item);
	hash_table_init(p_ht_it->ht, HASH_TABLE_SIZE, &ht_ops_fditem);
	p_ht_it->pid = pid;

	return &p_ht_it->item;
}

/** Deletes fd mappings for process with pid @a pid from the list @fd_mappings.
 *
 * @arg fd_mappings list of fd mappings (hash tables)
 * @arg pid process id
 */

void delete_process_ht(hash_table_t * fd_mappings, int32_t pid) {
	item_t * process_ht_item;

	if ( (process_ht_item = hash_table_find(fd_mappings, &pid)) != NULL ) {
		hash_table_remove(fd_mappings, &pid);
	} else {
		ERRORPRINTF("Can not find pid %"PRIi32" when removing delete_process_ht\n", pid);
	}
}

/** Duplicates whole hash table of fd mappings
 *
 * @parm h Hash table.
 * @return newly created hash table which is exactl clone of @a h
 *
 */

hash_table_t * duplicate_process_ht(hash_table_t * h, hash_table_t * usage_map) {
	int i;
   item_t * cur;
	fd_item_t * fd_it;
	fd_item_t * fd_it_old;
   hash_table_t * ht = malloc(sizeof(hash_table_t));

   hash_table_init(ht, h->entries, h->op);
   for (i = 0; i < h->entries; i++) {
      cur = h->entry[i].head;
      while ( cur != NULL ) {
			fd_it = new_fd_item();			
			fd_it_old = list_entry(cur, fd_item_t, item);

			fd_it->old_fd = fd_it_old->old_fd;
			item_init(&fd_it->item);

			memcpy(fd_it->fd_map, fd_it_old->fd_map, sizeof(fd_map_t));

			hash_table_insert(ht, &fd_it->old_fd, &fd_it->item);
			increase_fd_usage(usage_map, fd_it_old->fd_map->my_fd);
         cur = cur->next;
      }
   }
   return ht;
}

void dump_fd_item(fd_item_t * fd_item) {
	int i;
	int * parents = fd_item->fd_map->parent_fds;

	fprintf(stderr, "   Old_fd: %d. FD_MAP(%p):my_fd: %d, type: %d: parents:", fd_item->old_fd, fd_item->fd_map,
			fd_item->fd_map->my_fd, fd_item->fd_map->type);
	for (i = 0; i < MAX_PARENT_IDS; i++) {
		fprintf(stderr, "%d ", parents[i]);
	}
	fprintf(stderr, "\n");
}

void dump_fd_list_item(item_t * it) {
	fd_item_t * fd_item = list_entry(it, fd_item_t, item);
	dump_fd_item(fd_item);
}

void dump_process_hash_list_item(item_t * it) {
	process_hash_item_t  * p_it = hash_table_entry(it, process_hash_item_t, item);
	fprintf(stderr, " %d", p_it->pid);

}

inline fd_usage_t * new_fd_usage() {
	fd_usage_t * fd_usage;

	fd_usage = malloc(sizeof(fd_usage_t));
	item_init(&fd_usage->item);
	fd_usage->usage = 0;
	return fd_usage;
}


inline void delete_fd_usage(fd_usage_t *fd_usage) {
	free(fd_usage);
}

inline void increase_fd_usage(hash_table_t * ht, int fd) {
	item_t * item = hash_table_find(ht, &fd);
	fd_usage_t * fd_usage;

	if (item == NULL) { //was not opened before
		fd_usage = new_fd_usage();
		fd_usage->my_fd = fd;
		fd_usage->usage = 1;
		hash_table_insert(ht, &fd, &fd_usage->item);
	} else { //we have it already
		fd_usage = hash_table_entry(item, fd_usage_t, item);
		fd_usage->usage++;
	}
}

inline int decrease_fd_usage(hash_table_t * ht, int fd) {
	item_t * item = hash_table_find(ht, &fd);
	fd_usage_t * fd_usage;

	if (item == NULL) { //was not opened before
		ERRORPRINTF("Trying to decrease usage of fd:%d, which doesn't exist!\n", fd);
		assert(1 == 0);
		return -1;
	} else { //we have it already
		fd_usage = hash_table_entry(item, fd_usage_t, item);
		fd_usage->usage--;
		if (fd_usage->usage == 0) {
			hash_table_remove(ht, &fd);
			return 1;
		}
	}
	return 0;
}


inline fd_item_t * new_fd_item() {
	fd_item_t * fd_item;

	fd_item = malloc(sizeof(fd_item_t));
	item_init(&fd_item->item);
	fd_item->fd_map = malloc(sizeof(fd_map_t));
	memset(fd_item->fd_map->parent_fds, -1, MAX_PARENT_IDS * sizeof(int));
	fd_item->fd_map->last_par_index = -1;
	return fd_item;
}

inline void delete_fd_item(fd_item_t * item) {
	free(item->fd_map);
	item->fd_map = NULL;
	free(item);
	item = NULL;
}

void insert_parent_fd(fd_item_t * fd_item, int fd) {
	int i;
	int index = -1;
	int * parents = fd_item->fd_map->parent_fds;

	for(i = 0; i < MAX_PARENT_IDS; i++) {
		if (parents[i] == fd) {			
			ERRORPRINTF("Fd %d is already present in parent fds array...\n", fd);
			assert(1 == 0);
			return;
		}
		if (parents[i] == -1) {
			index = i;
			break;
		}
	}

	if (index != -1) {
		parents[index] = fd;
		fd_item->fd_map->last_par_index++;
	} else {
		ERRORPRINTF("Array of parrent fds is full! My_fd is: :%d\n", fd_item->fd_map->my_fd);
	}

}

/** Deletes fd from list of parents of given fd_item
 *
 * @arg bla bla
 * @return true if this was the last parent of the item, false otherwise
 */


int delete_parent_fd(fd_item_t * fd_item, int fd) {
	int i;
	int * parents = fd_item->fd_map->parent_fds;

	for(i = 0; i < MAX_PARENT_IDS; i++) {
		if (parents[i] == fd) {
			int idx = fd_item->fd_map->last_par_index;
			if ( idx == 0 ) {
				parents[i] = -1;
			} else if ( idx > 0 ) {
				parents[i] = parents[idx];
			} else {
				ERRORPRINTF("Sanity check error: last_par_index out of bounds: %d\n", idx);
				assert(1 == 0);
			}
			fd_item->fd_map->last_par_index--;
			break;
		}

		if (parents[i] == -1) {
			ERRORPRINTF("Didn't find fd %d in parent fds\n", fd);
			assert(1 == 0);
			break;
		}
	}
	return fd_item->fd_map->last_par_index == -1;
}

