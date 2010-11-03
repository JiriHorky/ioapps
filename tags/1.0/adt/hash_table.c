/*
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

/*******************************************************************************
 * This file was shamelessly borrowed from HelenOS	 (and slightly rewritten)   *
 ******************************************************************************/

/**
 * @file hash_table.c
 * @brief	Implementation of generic chained hash table.
 *
 * This file contains implementation of generic chained hash table.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "../common.h"
#include "list.h"

#include "hash_table.h"

index_t ht_hash_int(hash_table_t * ht, key_t * key) {
	assert(*key >= 0);
	//return (*key * 9587 + 13) % ht->entries;
	return *key % ht->entries;
}

index_t ht_hash_str(hash_table_t * ht, key_t * key) {
	char c;
	char * str = (char *) key;
	unsigned long index = 4357;

	while ((c = *str++) != 0)
		index = c + (index << 6) + (index << 16) - index;

	return index % ht->entries;
}

/** Initialize chained hash table and allocate the table of slots (chain holders).
 *
 * @param h Hash table structure. Will be initialized by this call.
 * @param m Number of slots in the hash table.
 * @param op Hash table operations structure.
 */
void hash_table_init(hash_table_t *h, ssize_t m, hash_table_operations_t *op) {
	index_t i;

	assert(h);
	assert(op && op->hash && op->compare);
	
	h->entry = (list_t *) malloc(m * sizeof(list_t));
	if (!h->entry) {
		DEBUGPRINTF("cannot allocate memory for hash table%s", "\n");
	}
//	memsetb(h->entry, m * sizeof(list_t), 0);
	
	for (i = 0; i < m; i++)
		list_init(&h->entry[i]);
	
	h->entries = m;
	h->op = op;
}

/** Insert item into hash table.
 *
 * @param h Hash table.
 * @param key Array of all keys necessary to compute hash index.
 * @param item Item to be inserted into the hash table.
 */
void hash_table_insert(hash_table_t *h, key_t * key, item_t *item) {
	index_t chain;

	assert(item);
	assert(h && h->op && h->op->hash && h->op->compare);

	chain = h->op->hash(h, key);
	assert(chain < h->entries);
	
	list_append(&h->entry[chain], item);
}

/** Search hash table for an item matching key.
 *
 * @param h Hash table.
 * @param key pointer to the key needed to compute hash index.
 *
 * @return Matching item on success, NULL if there is no such item.
 */

item_t *hash_table_find(hash_table_t *h, key_t * key) {
	item_t *cur;
	index_t chain;

	assert(h && h->op && h->op->hash && h->op->compare);

	chain = h->op->hash(h, key);
	assert(chain < h->entries);
	int i =0;	
	for (cur = h->entry[chain].head; cur != NULL; cur = cur->next) {
		if (h->op->compare(key, cur)) {
			/*
			 * The entry is there.
			 */
			return cur;
		}
		i++;
	}
	
	return NULL;
}

/** Remove all matching items from hash table.
 *
 * For each removed item, h->remove_callback() is called.
 *
 * @param h Hash table.
 * @param key pointer to key that will be compared against items of the hash table.
 */

void hash_table_remove(hash_table_t *h, key_t * key) {
	index_t chain;
	item_t *cur;

	assert(h && h->op && h->op->hash && h->op->compare && h->op->remove_callback);
	
	/*
	 * hash_table_find() can be used to find the entry.
	 */
	chain = h->op->hash(h, key);	
	cur = hash_table_find(h, key);
	if (cur) {
		list_remove(&h->entry[chain], cur);
		h->op->remove_callback(cur);
	}
	return;
}


/** Prints whole hash table.
 *
 * @param h Hash table.
 * 
 */

void hash_table_dump(hash_table_t * h) {
	item_t * cur;
	unsigned int i;

	assert(h);

	DEBUGPRINTF("Dumping hash table....%s", "\n");
	for (i = 0; i < h->entries; i++) {
		fprintf(stderr, "[%u]",  i);
		cur = h->entry[i].head;
		while ( cur != NULL ) {
			fprintf(stderr, "X");
			cur = cur->next;
		}
		fprintf(stderr, "\n");
	}
}


/** Applys given function to all item_t. The function should not delete the item!
 * @param h hash table
 * @param function function to call on item_t *
 */
void hash_table_apply(hash_table_t * h, void (* function)(item_t * item)) {
	item_t * cur;
	unsigned int i;

	assert(h);

	for (i = 0; i < h->entries; i++) {
		cur = h->entry[i].head;
		while ( cur != NULL ) {
			function(cur);
			cur = cur->next;
		}
	}
}

/** Prints whole hash table, it uses function f to print item_ts
 *
 * @param h Hash table.
 * @param print_item function that takes item_t * as argument and prints it
 * 
 */

void hash_table_dump2(hash_table_t * h, void (* print_item)(item_t * item)) {
	item_t * cur;
	unsigned int i;

	assert(h);

	DEBUGPRINTF("Dumping hash table....%s", "\n");
	for (i = 0; i < h->entries; i++) {
		fprintf(stderr, "[%u]",  i);
		cur = h->entry[i].head;
		while ( cur != NULL ) {
			print_item(cur);
			cur = cur->next;
		}
		fprintf(stderr, "\n");
	}
}

/** Destroys all content of hash table. The remove callback function
 * will be called on all items in ht.
 *
 * @param h Hash table.
 * 
 */

void hash_table_destroy(hash_table_t * h) {
	item_t * cur;
	item_t * next;
	unsigned int i;

	for (i = 0; i < h->entries; i++) {
		cur = h->entry[i].head;
		while ( cur != NULL ) {
			next = cur->next;
			list_remove(&h->entry[i], cur);
			h->op->remove_callback(cur);
			cur = next;
		}
	}
	free(h->entry);
}
