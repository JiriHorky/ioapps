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
#include <assert.h>
#include "fs_trie.h"


/** Inserts all necessary nodes that would represent @a full_key in trie @t
 * @arg t Trie in which to insert
 * @arg full_key t->delim separated list representing path that should be inserted into @a t
 *
 * @return pointer to last created node (or already existent node), or TRIE_ERR in case of error.
 */

trie_node_t * trie_insert(trie_t * t, const char * full_key) {
	return trie_insert2(t, full_key, t->new_node);
}


/** Inserts all necessary nodes that would represent @a full_key in trie @t
 * @arg t Trie in which to insert
 * @arg full_key t->delim separated list representing path that should be inserted into @a t
 * @arg create function to call when creating new nodes
 *
 * @return pointer to last created node (or already existent node), or TRIE_ERR in case of error.
 */

trie_node_t *trie_insert2(trie_t * t, const char * full_key, trie_node_t * (*create)(void)) {
	int i;
	trie_node_t * n;
	trie_node_t * new_node;
	char delim[2];
	char * s;
	char * strtmp;
	char * prefix = malloc(sizeof(char) * strlen(full_key) + 1);
	memset(prefix, 0, strlen(full_key) +1);

	n = trie_longest_prefix(t, full_key, prefix); 

	if ( ! strcmp(prefix, full_key) ) { //whole key was found
		free(prefix);
		return n;
	}

 	i = 0;
	if ( n != t->root) { // some prefix was found
		while (*prefix && prefix[i] == full_key[i]) //skip common part
			i++;
	}
	
	delim[0] = t->delim;
	delim[1] = 0;
	
	strtmp = strdup(full_key+i);
	char * saveptr;
	s = strtok_r(strtmp, delim, &saveptr);

	while(s) { //Insert all needed nodes
		//fprintf(stderr, "inserting node with key: %s\n", s);
		new_node = create();
		trie_node_init(new_node);
		new_node->key = malloc(strlen(s) +  1);
		new_node->key[0] = 0;
		strcat(new_node->key, s);

		list_append(&n->children, &new_node->item);
		n = new_node;
		s = strtok_r(NULL, delim, &saveptr);
	}

	free(strtmp);
	free(prefix);
	return n;
}

/** Deletes the node @node from trie @t. Note that it also deletes every
 * (even indirect) child nodes. It doesn't delete t->root node.
 * @arg t Trie in which to delete
 * @arg node node to delete
 *
 * @return TRIE_OK if just one node was deleted, TRIE_MANY if more than one node was deleted.
 */

int trie_delete2(trie_t * t, trie_node_t * node) {
	item_t * i;
	trie_node_t * n;

	assert(t);
	if ( list_length(&node->children) == 0 ) {
		if (node != t->root) {
			list_remove2(&node->item);
			t->delete_node(node);
			node = NULL;
		}
		return TRIE_OK;
	} else {
		i = node->children.head;

		while (i) {
			n = list_entry(i, trie_node_t, item);
			i = i->next;
			trie_delete2(t, n);
		}
		//don't forget to delete ourself, 
		if (node != t->root) {
			list_remove2(&node->item);
			t->delete_node(node);
			node = NULL;
		}
		return TRIE_MANY;
	}
}

/** Destroys whole trie_t tree including root node. 
 * @arg t trie to destroy
 */
void trie_destroy(trie_t * t) {
	trie_delete2(t, t->root);
	t->delete_node(t->root);
	t->root = NULL;
}


/** Deletes the node representing @a full_key. Note that it also deletes every
 * node with keys that has full_key as prefix (every descendent).
 * @arg t Trie in which to delete
 * @arg full_key t->delim separated list of keys representing path to the item to delete
 *
 * @return TRIE_OK if just one node was deleted, TRIE_MANY if more than one node was deleted, TRIE_ERR if no such
 *         exists in @t.
 */

int trie_delete(trie_t * t, const char * full_key) {
	trie_node_t * node;

	assert(t);
	node = trie_find(t, full_key);

	if ( ! node ) {
		return TRIE_ERR;
	} else {
		return trie_delete2(t, node);
	}
}

/** Looks for its direct child with given @a part_key
 * @arg n node in which to look for the child
 * @arg part_key key that the child should have
 *
 * @return pointer to the child or NULL if no such child was found
 */

trie_node_t * trie_find_child(trie_node_t * n, const char * part_key) {
	trie_node_t * node = NULL;
	item_t * i;

	i = n->children.head;

	while(i) {
		node = list_entry(i, trie_node_t, item);
		if ( strcmp(node->key, part_key) == 0 ) {
			return node;
		}
		i = i->next;
	}

	return NULL;
}

/** Looks for the node that represents @a full_key path
 *
 * @arg t trie in which to search
 * @arg full_key t->delim separated list of keys that should lead to the node
 *
 * @return pointer to the node or NULL if no such node was found
 */

trie_node_t * trie_find(trie_t * t, const char * full_key) {
	trie_node_t * n;
	char * s;
	char * strtmp;

	assert(t);

	strtmp = strdup(full_key);
	char * saveptr;
	s = strtok_r(strtmp, t->root->key, &saveptr);
	n = t->root;

	while(s) {
		n = trie_find_child(n, s);
		if ( !n ) {
			free(strtmp);
			return NULL;
		}
		s = strtok_r(NULL, t->root->key, &saveptr);
	}
	
	free(strtmp);
	return n;
}

/** Finds longest existing prefix of @a full_key in trie @a t. It returns
 *  node containg the longest prefix and fills @a buff with the prefix
 * @arg t Trie in which to search
 * @arg full_key key to look for
 * @arg buff buffer of size at least strlen(full_key)+1 that will be filled with the longest prefix found
 *
 * @return node containg the longest prefix of @a full_key in the trie @a t. It is, if a root node is returned
 *               no prefix was found. 
 */

trie_node_t * trie_longest_prefix(trie_t * t, const char * full_key, char * buff) {
	trie_node_t * n1;
	trie_node_t * n2;
	const char * c;
	char * part_key;
	char lastc;
	int num_chars = 0;
	int count = 0;
	int i = 0;
	
	assert(t);

	buff[0] = 0;

	part_key = malloc(strlen(full_key) + 1);
	part_key[0] = 0;

	n1 = t->root;

	lastc = 0;
	c = full_key;

	while (*c) {
		if (lastc == t->delim && *c == t->delim ) { //skip more delim characters
			lastc = *c;
			c++;		
			count++;
		} else {
			if (*c == t->delim && num_chars != 0) { //new delim found and it is not the leading one
				part_key[i] = 0;
				n2 = trie_find_child(n1, part_key);
				if ( !n2 ) { // it didn't find the node
					free(part_key);
					return n1;
				} else {
					n1 = n2;
					memcpy(buff, full_key, count * sizeof(char));
					buff[count+1] = 0;					
					i = 0;
				}				
			}
			if ( *c != t->delim) {
				part_key[i] = *c;
				i++;
			}
			count++;
			lastc = *c;
			c++;		
			num_chars++;
		} 
	}

	//the last portion of full_key string wasn't tested (or whole part, if it doesn't contain delimiter)
	if ( (c = rindex(full_key, t->delim)) == NULL) { // no delimiter at all
		if ((n2 = trie_find_child(t->root, full_key)) != NULL ) {
			strcpy(buff, full_key);
			n1 = n2;
		} else {
			n1 = t->root;
		}
	} else { //just the last part
		strcpy(part_key, c+1);
		n2 = trie_find_child(n1, part_key);
		if ( n2 ) { // it found the node
			strcpy(buff, full_key);
			n1 = n2;
		}				
	}

	free(part_key);
	return n1;
}

/** Recursively prints the node and all its children.
 * @arg n node to dump
 * @arg depth depth in which the node is
 */

void trie_node_dump(trie_node_t * n, unsigned int depth) {
	int j;
	item_t * i;
	trie_node_t * node;

	if ( ! n ) {
		return;
	}

	i = n->children.head;
	for (j = 0; j <= depth; j++)
		fprintf(stderr, "  ");

	fprintf(stderr, "%s\n", n->key);

	while(i) {
		node = list_entry(i, trie_node_t, item);
		trie_node_dump(node, depth+1);
		i = i->next;
	}
}

/** Dumps trie @a t.
 *
 * @arg t trie to dump.
 */

void trie_dump(trie_t * t) {
	fprintf(stderr, ".\n");
	trie_node_dump(t->root, 0);
}

/** Returns whether given node is leaf (i.e. has any child) or not.
 * @arg node to check
 * @return 1 if the node is leaf, 0 otherwise
 */

inline int trie_is_leaf(trie_node_t * node) {
	return node->children.head == NULL;
}


void trie_apply2(trie_node_t * n, void (* function)(trie_node_t * node)) {
	item_t * i;
	trie_node_t * node;

	if ( ! n ) {
		return;
	}

	i = n->children.head;
	while(i) {
		node = list_entry(i, trie_node_t, item);
		trie_apply2(node, function);
		i = i->next;
	}

	function(n);
}

void trie_apply(trie_t * t, void (* function)(trie_node_t * node)) {
	trie_apply2(t->root, function);
}


void trie_apply2_full(trie_node_t * n, void (* function_full)(trie_node_t * node, char * name), char * name, const char * delim) {
	item_t * i;
	trie_node_t * node;
	char * name_end;

	if ( ! n ) {
		return;
	}

	name_end = name + strlen(name);
	if (name[0] != 0 ) { //not empty
		strcat(name, delim);
	}
	strcat(name, n->key);

	i = n->children.head;
	while(i) {
		node = list_entry(i, trie_node_t, item);
		trie_apply2_full(node, function_full, name, delim);
		i = i->next;
	}

	function_full(n, name);
	*name_end = 0; 
}

void trie_apply_full(trie_t * t, void (* function_full)(trie_node_t * node, char * full_name)) {
	char buff[MAX_LINE];
	buff[0] = 0;
	char delim[2];
	delim[0] = t->delim;
	delim[1] = 0;

	trie_apply2_full(t->root, function_full, buff, delim);
}
