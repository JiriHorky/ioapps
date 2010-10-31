/**
 * @file 
 *
 * Implementation of generic chained hash table,
 *
 * Copyright (c) 2006 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/********************************************************************************
 * This file was shamelessly borrowed from HelenOS (and slightly rewritten)     *
 *******************************************************************************/

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include "common.h"
#include "../common.h"
#include "list.h"

typedef ssize_t index_t;
#ifndef __key_t_defined
typedef ssize_t key_t;
#endif

struct hash_table;


/** Set of operations for hash table. */
typedef struct {
	/** Hash function.
	 *
	 * @param key 	Array of keys needed to compute hash index. All keys must
	 * 		be passed.
	 *
	 * @return Index into hash table.
	 */
	index_t (* hash)(struct hash_table * ht, key_t * key);

	/** Hash table item comparison function.
	 *
	 * @param key 	Array of keys that will be compared with item. It is not
	 *		necessary to pass all keys.
	 *
	 * @return true if the keys match, false otherwise.
	 */
	int (*compare)(key_t * key, item_t *item);

	/** Hash table item removal callback.
	 *
	 * @param item Item that was removed from the hash table.
	 */
	void (*remove_callback)(item_t *item);
} hash_table_operations_t;

/** Hash table structure. */
typedef struct hash_table {
	list_t *entry;
	ssize_t entries;
	hash_table_operations_t *op;
} hash_table_t;


#define hash_table_entry(item, type, member) \
	list_entry((item), type, member)

/** common hash fuctions */
index_t ht_hash_int(hash_table_t * ht, key_t * key);
index_t ht_hash_str(hash_table_t * ht, key_t * key);

extern void hash_table_init(hash_table_t *h, ssize_t m, hash_table_operations_t *op);
extern void hash_table_insert(hash_table_t *h, key_t * key, item_t *item);
extern item_t *hash_table_find(hash_table_t *h, key_t * key);
extern void hash_table_remove(hash_table_t *h, key_t * key);
extern void hash_table_dump(hash_table_t * h);
extern void hash_table_dump2(hash_table_t * h, void (* print_item)(item_t * item));
extern void hash_table_apply(hash_table_t * h, void (* function)(item_t * item));
extern void hash_table_destroy(hash_table_t *h);
extern hash_table_t * hash_table_duplicate(hash_table_t * h);
#endif

/** @}
 */
