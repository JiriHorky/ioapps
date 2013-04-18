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
#include "stats.h"

hash_table_t g_stats_ht;

static int ht_compare_stat(key_t *key, item_t *item) {
	statistic_item_t * statistic_item;

	statistic_item = hash_table_entry(item, statistic_item_t, item);

	return ! strncmp(statistic_item->operation, (char *) key, MAX_STRING);
}

static inline void ht_remove_callback_stat(item_t * item) {
	statistic_item_t * statistic_item = hash_table_entry(item, statistic_item_t, item);	
	free(statistic_item);
}

/** hash table operations. */
static hash_table_operations_t ht_ops_stat = {
	.hash = ht_hash_str,
	.compare = ht_compare_stat,
	.remove_callback = ht_remove_callback_stat /* = NULL if not used */
};

/** Adds operation to statistics.
 *
 * @arg line line on which operation occured. Currently unused.
 * @arg operation string determining operation
 */

void stats_add_op(const char * line, const char * operation, int32_t duration) {
	item_t * item;
	statistic_item_t * statistic_item;

	if ( (item = hash_table_find(&g_stats_ht, (key_t *) operation)) == NULL) {
		statistic_item = malloc(sizeof(statistic_item_t));
		item_init(&statistic_item->item);
		strncpy(statistic_item->operation, operation, MAX_OPERATION);
		statistic_item->count = 1;
		statistic_item->sumdur = duration/1000.0;
		hash_table_insert(&g_stats_ht,(key_t *)  statistic_item->operation, &statistic_item->item);
	} else {
		statistic_item = hash_table_entry(item, statistic_item_t, item);
		statistic_item->count++;
		statistic_item->sumdur += duration/1000.0;
	}
}

/** Prints one stats item
 * @arg item item to print
 */

void stats_print_item(item_t * item) {
	statistic_item_t * statistic_item = hash_table_entry(item, statistic_item_t, item);

	printf("%s : %"PRIu64" (%0.2lfms)\n", statistic_item->operation, statistic_item->count, statistic_item->sumdur);
}

/** Prints statistics for every operation.
 */

void stats_print() {
	hash_table_apply(&g_stats_ht, stats_print_item);
}

/** Initialize statistics structure. This function must be called before any other stats_* call.
 */

void stats_init() {
	hash_table_init(&g_stats_ht, HASH_TABLE_SIZE, &ht_ops_stat);
}
