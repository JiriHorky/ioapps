#ifndef _FS_TRIE_H_
#define _FS_TRIE_H_

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "list.h"

#define TRIE_OK 0
#define TRIE_ALREADY 1
#define TRIE_MANY 2
#define TRIE_ERR -1

/**
 * Macro for getting a pointer to the structure which contains the trie
 * structure.
 *
 * @param link Pointer to the trie structure.
 * @param type Name of the outer structure.
 * @param member Name of trie attribute in the outer structure.
 */
#define trie_get_instance(node, type, member) \
    ((type *)(((uint8_t *)(node)) - ((uint8_t *) &(((type *) NULL)->member))))

typedef struct trie_node trie_node_t;
typedef struct trie trie_t;


//typedef bool (* trie_walker_t)(trie_node_t *, void *);

struct trie_node {
   list_t children; //list of children
	item_t item; //I am part of the list of children

   /** Node's key. */
   char * key;
};

/** Trie tree structure. */
struct trie {
   /** Trie root node pointer */
   trie_node_t *root;
	char delim;

	//create and delete functions for new node
	trie_node_t *(*new_node)(void);
	void (*delete_node)(trie_node_t * node);
};

/** Initialize node.
 *
 * @param node Node which is initialized.
 */
static inline void trie_node_init(trie_node_t *node) {
   node->key = NULL;
	list_init(&node->children);
	item_init(&node->item);
}

/** Initialize an empty Trie.
 *
 * @arg t trie tree.
 * @arg delim delimiter to use in trie.
 */

static inline void trie_init(trie_t *t, char delim, trie_node_t *(* create)(void), void (* del)(trie_node_t * node)) {
	t->delim = delim;
	t->new_node = create;
	t->delete_node = del;
	t->root = t->new_node();
	trie_node_init(t->root);
	t->root->key = malloc(sizeof(char) * 2);
	t->root->key[0] = t->delim;
	t->root->key[1] = 0;
}

trie_node_t *trie_find(trie_t *t, const char * full_key);
trie_node_t * trie_insert(trie_t *t, const char * full_key);
trie_node_t * trie_insert2(trie_t * t, const char * full_key, trie_node_t * (*crate)(void));
int trie_delete(trie_t *t, const char * full_key);
int trie_delete2(trie_t *t, trie_node_t * node);
trie_node_t * trie_find_child(trie_node_t * n, const char * part_key);
trie_node_t * trie_longest_prefix(trie_t * t, const char * full_key, char * buff);
void trie_destroy(trie_t * t);
void trie_dump(trie_t * t);
inline int trie_is_leaf(trie_node_t * node);
void trie_apply(trie_t * t, void (* function)(trie_node_t * node));
void trie_apply_full(trie_t * t, void (* function_full)(trie_node_t * node, char * name));
//extern void trie_walk(trie_t *t, trie_walker_t walker, void *arg);

#endif
