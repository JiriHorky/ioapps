/**
 * @file list.h
 *
 * Simple doubly linked list implementation.
 *
 * Copyright (c) 2001-2010
 *   Department of Distributed and Dependable Systems
 *   Faculty of Mathematics and Physics
 *   Charles University, Czech Republic
 *
 * Licensed under the terms of the GNU General Public License, Version 2
 *   Signed-off-by: Viliam Holub <holub@d3s.mff.cuni.cz>
 *   Signed-off-by: Lubomir Bulej <bulej@d3s.mff.cuni.cz>
 *   Signed-off-by: Petr Tuma <tuma@d3s.mff.cuni.cz>
 *   Signed-off-by: Martin Decky <decky@d3s.mff.cuni.cz>
 *
 * Modified and enhanced by Jiri Horky <jiri.horky@gmail.com>
 *
 */

#ifndef LIST_H_
#define LIST_H_

#include "common.h"
#include "../common.h"

/* Forward declaration */
struct item;


/** A doubly linked list
 *
 */
typedef struct list {
	/** The first item on the list or NULL if empty. */
	struct item * head;

	/** The last item on the list or NULL if empty. */
	struct item * tail;
} list_t;


/** An item of a doubly linked list
 *
 * The item should be first in listable structures.
 *
 */
typedef struct item {
	/** The list that we currently belong to. */
	struct list * list;

	/** The next item on the list or NULL if first. */
	struct item * prev;

	/** The previous item on the list or NULL if last. */
	struct item * next;
} item_t;

#define list_entry(ptr, type, member)	container_of(ptr, type, member)

/* Externals are commented with implementation. */
extern void list_init (list_t * list);
extern void item_init (item_t * item);
extern void list_insert_before (item_t * target, item_t * insert);
extern void list_insert_after (item_t * target, item_t * insert);
extern void list_append (list_t * list, item_t * item);
extern void list_remove (list_t * list, item_t * item);
extern void list_remove2(item_t * item);
extern int list_empty (list_t * list);
extern ssize_t list_length(list_t * list);
extern void list_dump(list_t * list, void (* print_item)(item_t * item));
extern item_t * list_rotate (list_t * list);


#endif
