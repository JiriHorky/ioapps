/**
 * @file list.c
 *
 * Simple doubly linked list implementation.
 *
 * Borrowed from Kalisto, written by:
 *
 * Copyright (c) 2001-2008
 * 	Distributed Systems Research Group
 * 	Department of Software Engineering
 * 	Faculty of Mathematics and Physics
 * 	Charles University, Czech Republic
 *
 */

#include <assert.h>
#include <stdio.h>
#include "list.h"


/** Initialize a list
 *
 * @param list The list to initialize.
 */
void list_init (list_t * list) {
	list->head = NULL;
	list->tail = NULL;
}

/** Initialize an item
 *
 * @param item The item to initialize.
 *
 */
void item_init (item_t *item) {
	/* To catch list usage errors, items are initialized.
	   This makes it possible to detect double append
	   and double remove operations easily. */

	item->list = NULL;
}


/** Append an item to a list
 *
 * @param list The list to append to.
 * @param item The item to append.
 *
 */
void list_append (list_t *list, item_t *item) {
	/* Make sure the item is not
	   a member of a list, then add it. */
	assert (item->list == NULL);
	item->list = list;

	/* In an empty list, attach us as head.
	   Otherwise, attach us to current tail. */
	if (list->tail == NULL) {
		list->head = item;
	} else {
		list->tail->next = item;
	}
	/* Our previous item is current tail.
	   We obviously have no next item. */
	item->prev = list->tail;
	item->next = NULL;

	/* We are the new tail. */
	list->tail = item;
}

void list_insert_before (item_t * target, item_t * insert) {
	assert(target && insert && target->list);

	insert->list = target->list;
	insert->prev = target->prev;

	if (target->prev) {
		target->prev->next = insert;
	}
	else {
		target->list->head = insert;
	}
	insert->next = target;
	target->prev = insert;
}

void list_insert_after (item_t * target, item_t * insert) {
	assert(target && insert && target->list);

	insert->list = target->list;
	insert->next = target->next;

	if (target->next) {
		target->next->prev = insert;
	}
	else {
		target->list->tail = insert;
	}
	insert->prev = target;
	target->next = insert;
}

/** Remove an item from the list it currently belongs to.
 *
 * @param item The item to remove.
 *
 */

void list_remove2(item_t * item) {
	assert(item->list != NULL);

	list_remove(item->list, item);
}

/** Remove an item from a list
 *
 * @param list The list to remove from.
 * @param item The item to remove.
 *
 */
void list_remove (list_t *list, item_t *item)
{
	/* Make sure the item is
	   a member of the list, then remove it. */
	assert (item->list == list);
	item->list = NULL;

	if (item->prev == NULL)
		/* If we are list head, our next item is the new head.
		   This works even if we happen to be the tail too. */
		list->head = item->next;
	else
		/* Otherwise, just make our previous
		   item point to our next item. */
		item->prev->next = item->next;

	/* The same for the other end of the list. */
	if (item->next == NULL)
		list->tail = item->prev;
	else
		item->next->prev = item->prev;
}

/** Returns true if the list is empty.
 *
 * @param list The list to examine on emptyness.
 * @return true if the list is empty.
 */
int list_empty(list_t * list) {
	return list->head == NULL;
}

/** Returns the number of elements in the list.
 *
 * @param list The list whose length to count.
 * @return Number of elements in the list.
 */
ssize_t list_length(list_t * list) {
	item_t * i;
	ssize_t length = 0;
	for (i = list->head; i != NULL; i = i->next) {
		++length;
	}
	return length;
}

/** Rotate the list by making its head into its tail
 *
 * @param list The list to rotate.
 * @return The rotated item.
 *
 */
item_t *list_rotate (list_t *list)
{
	/* Simply remove and append current list head.
	   Not most efficient but working nonetheless. */

	item_t *item = list->head;
	list_remove (list, item);
	list_append (list, item);
	return item;
}

/** Dumps the @a list 
 *
 * @arg list list to dump
 * @arg print_item function to be called to every item
 */

void list_dump(list_t * list, void (* print_item)(item_t * item)) {
	item_t * cur = list->head;

	while (cur) {
		print_item(cur);
		fprintf(stderr, " ");
		cur = cur->next;
	}
	fprintf(stderr, "\n");
}
