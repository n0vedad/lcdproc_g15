// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/LL.c
 * \brief Implementation of doubly linked list data structure
 * \author Selene Scriven, William Ferrell, Guillaume Filion, Joris Robijn, Peter Marschall
 * \date 1994-2008
 *
 * \features
 * - Basic operations: create, destroy, traverse
 * - Data manipulation: get, put, find
 * - Stack operations: push, pop, top
 * - Queue operations: enqueue, dequeue, shift, unshift
 * - Advanced operations: sort, priority enqueue, node swapping
 * - Utility functions: length calculation, indexed access
 *
 * \usage
 * - Create a list with LL_new()
 * - Add data with LL_Push(), LL_AddNode(), or LL_InsertNode()
 * - Navigate with LL_Next(), LL_Prev(), LL_Rewind(), LL_End()
 * - Access data with LL_Get() and modify with LL_Put()
 * - Clean up with LL_Destroy() when finished
 *
 * \details This file provides a comprehensive implementation of a doubly linked list
 * data structure with support for various operations including traversal, insertion,
 * deletion, searching, sorting, and stack/queue operations. The implementation uses
 * sentinel nodes (head and tail) to simplify edge case handling.
 *
 * \todo Test everything?
 *
 * Linked list library needs comprehensive unit tests. Currently no systematic test coverage
 * for all LL_* functions. Critical functions that need testing:
 *
 * Core operations:
 * - LL_AddNode, LL_InsertNode, LL_Remove, LL_RemoveNode - insertion/deletion correctness
 * - LL_Push, LL_Pop, LL_Shift, LL_Unshift - stack/queue operations
 * - LL_Enqueue, LL_Dequeue - queue-specific operations
 *
 * Navigation:
 * - LL_Next, LL_Prev, LL_Rewind, LL_Reset - cursor positioning
 * - LL_GetFirst, LL_GetLast, LL_GetByIndex - element access
 *
 * Edge cases:
 * - Empty list operations
 * - Single-element list operations
 * - NULL pointer handling
 * - Memory leak prevention
 *
 * Impact: Test coverage, code quality, reliability assurance
 *
 * \ingroup ToDo_low
 */

#include <stdio.h>
#include <stdlib.h>

#include "LL.h"

#ifdef DEBUG
#undef DEBUG
#endif

// Create new doubly linked list with sentinel nodes
LinkedList *LL_new(void)
{
	LinkedList *list;

	list = malloc(sizeof(LinkedList));
	if (list == NULL)
		return NULL;

	// Initialize sentinel nodes
	list->head.data = NULL;
	list->head.prev = NULL;
	list->head.next = &list->tail;

	list->tail.data = NULL;
	list->tail.prev = &list->head;
	list->tail.next = NULL;

	list->current = &list->head;

	return list;
}

// Destroy the entire list and free all memory
int LL_Destroy(LinkedList *list)
{
	LL_node *prev, *next;
	LL_node *node;

	if (!list)
		return -1;

	node = &list->head;

	// Traverse and free all nodes except sentinels
	for (node = node->next; node && node->next; node = next) {
		next = node->next;
		prev = node->prev;

		if (next != NULL)
			next->prev = prev;
		if (prev != NULL)
			prev->next = next;

		node->next = NULL;
		node->prev = NULL;
		free(node);
	}

	free(list);
	return 0;
}

// Move to another entry in the list using directional navigation
LL_node *LL_GoTo(LinkedList *list, Direction whereto)
{
	if (!list)
		return NULL;

	switch (whereto) {

	// Move to first element in list
	case HEAD:
		list->current = (list->head.next != &list->tail) ? list->head.next : NULL;
		break;

	// Move to previous element
	case PREV:
		if (list->current->prev == &list->head)
			return NULL;
		list->current = list->current->prev;

	// Stay at current position (fallthrough intended)
	case CURRENT:
		break;

	// Move to next element
	case NEXT:
		if (list->current->next == &list->tail)
			return NULL;
		list->current = list->current->next;
		break;

	// Move to last element in list
	case TAIL:
		list->current = (list->tail.prev != &list->head) ? list->tail.prev : NULL;
		break;
	}

	return list->current;
}

// Return to the beginning of the list
int LL_Rewind(LinkedList *list)
{
	if (!list)
		return -1;

	list->current = (list->head.next != &list->tail) ? list->head.next : &list->head;
	return 0;
}

// Jump to the end of the list
int LL_End(LinkedList *list)
{
	if (!list)
		return -1;

	list->current = (list->tail.prev != &list->head) ? list->tail.prev : &list->tail;
	return 0;
}

// Go to the next node of the list
int LL_Next(LinkedList *list)
{
	if (!list)
		return -1;
	if (!list->current)
		return -1;

	if (list->current->next == &list->tail)
		return -1;

	list->current = list->current->next;
	return 0;
}

// Go to the previous node of the list
int LL_Prev(LinkedList *list)
{
	if (!list)
		return -1;

	if (!list->current)
		return -1;

	if (list->current->prev == &list->head)
		return -1;

	list->current = list->current->prev;
	return 0;
}

// Access current node's data
void *LL_Get(LinkedList *list)
{
	if (!list)
		return NULL;

	if (!list->current)
		return NULL;

	return list->current->data;
}

// Set/change current node's data
int LL_Put(LinkedList *list, void *data)
{
	if (!list)
		return -1;

	if (!list->current)
		return -1;

	list->current->data = data;
	return 0;
}

// Get current node in list
LL_node *LL_GetNode(LinkedList *list)
{
	if (!list)
		return NULL;

	return list->current;
}

// Set list's current pointer to a specific node (use with caution)
int LL_PutNode(LinkedList *list, LL_node *node)
{
	if (!list)
		return -1;
	if (!node)
		return -1;

	list->current = node;
	return 0;
}

// Access list's first node's data
void *LL_GetFirst(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_Rewind(list))
		return NULL;

	return LL_Get(list);
}

// Access next node's data
void *LL_GetNext(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_Next(list))
		return NULL;

	return LL_Get(list);
}

// Access previous node's data
void *LL_GetPrev(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_Prev(list))
		return NULL;

	return LL_Get(list);
}

// Access list's last node's data
void *LL_GetLast(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_End(list))
		return NULL;

	return LL_Get(list);
}

// Add/append a new node after current one in the list
int LL_AddNode(LinkedList *list, void *add)
{
	LL_node *node;

	if (!list)
		return -1;

	if (!list->current)
		return -1;

	node = malloc(sizeof(LL_node));
	if (node == NULL)
		return -1;

	// If at tail sentinel, move to previous node
	if (list->current == &list->tail)
		list->current = list->current->prev;

	node->next = list->current->next;
	node->prev = list->current;
	node->data = add;

	if (node->next)
		node->next->prev = node;

	list->current->next = node;
	list->current = node;

	return 0;
}

// Add/insert a new node before current one in the list
int LL_InsertNode(LinkedList *list, void *add)
{
	LL_node *node;

	if (!list)
		return -1;

	if (!add)
		return -1;

	if (!list->current)
		return -1;

	node = malloc(sizeof(LL_node));
	if (node == NULL)
		return -1;

	// If at head sentinel, move to next node
	if (list->current == &list->head)
		list->current = list->current->next;

	node->next = list->current;
	node->prev = list->current->prev;
	node->data = add;

	if (list->current->prev)
		list->current->prev->next = node;
	list->current->prev = node;

	list->current = node;

	return 0;
}

// Remove current node from the list
void *LL_DeleteNode(LinkedList *list, Direction whereto)
{
	LL_node *next, *prev;
	void *data;

	if (!list)
		return NULL;

	if (!list->current)
		return NULL;

	// Protect sentinel nodes, unlink current node from list, clean up pointers, and free memory
	if (list->current == &list->head)
		return NULL;
	if (list->current == &list->tail)
		return NULL;

	next = list->current->next;
	prev = list->current->prev;
	data = list->current->data;

	if (prev)
		prev->next = next;

	if (next)
		next->prev = prev;

	list->current->prev = NULL;
	list->current->next = NULL;
	list->current->data = NULL;
	free(list->current);

	// Set new current position based on direction
	switch (whereto) {

	// Move to first element after deletion
	case HEAD:
		list->current = list->head.next;
		break;

	// Move to last element after deletion
	case TAIL:
		list->current = list->tail.prev;
		break;

	// Move to previous element after deletion
	case PREV:
		list->current = prev;
		break;

	// Move to next element after deletion (default)
	default:
	case NEXT:
		list->current = next;
	}

	return data;
}

// Remove a specific node from the list by data pointer
void *LL_Remove(LinkedList *list, void *data, Direction whereto)
{
	if (!list)
		return NULL;

	LL_Rewind(list);

	do {
		void *find = LL_Get(list);

		if (find == data)
			return LL_DeleteNode(list, whereto);
	} while (LL_Next(list) == 0);

	return NULL;
}

// Add/append a new node after the last one in the list (stack push)
int LL_Push(LinkedList *list, void *add)
{
	if (!list)
		return -1;

	if (!add)
		return -1;

	LL_End(list);

	return LL_AddNode(list, add);
}

// Remove the last node from the list and return its data (stack pop)
void *LL_Pop(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_End(list))
		return NULL;

	return LL_DeleteNode(list, PREV);
}

// Access list's last node's data (stack top)
void *LL_Top(LinkedList *list) { return LL_GetLast(list); }

// Remove the first node from the list and return its data (queue shift)
void *LL_Shift(LinkedList *list)
{
	if (!list)
		return NULL;

	if (0 > LL_Rewind(list))
		return NULL;

	return LL_DeleteNode(list, NEXT);
}

// Access list's first node's data (queue peek)
void *LL_Look(LinkedList *list) { return LL_GetFirst(list); }

// Add/insert a new node before the first one in the list (queue unshift)
int LL_Unshift(LinkedList *list, void *add)
{
	if (!list)
		return -1;

	if (!add)
		return -1;

	LL_Rewind(list);

	return LL_InsertNode(list, add);
}

// Add an item to the end of its "priority group" in sorted list
int LL_PriorityEnqueue(LinkedList *list, void *add, int (*compare)(void *, void *))
{
	if (!list)
		return -1;

	if (!add)
		return -1;

	if (!compare)
		return -1;

	// Search from end of list backwards for correct insertion point
	LL_End(list);
	do {
		void *data = LL_Get(list);

		if (data) {
			int i = compare(add, data);

			if (i >= 0) {
				LL_AddNode(list, add);
				return 0;
			}
		}
	} while (LL_Prev(list) == 0);

	// If less than everything, insert at beginning
	LL_Unshift(list, add);

	return 0;
}

// Switch two nodes positions in the list
int LL_SwapNodes(LL_node *one, LL_node *two)
{
	LL_node *firstprev, *firstnext;
	LL_node *secondprev, *secondnext;

	if (!one || !two)
		return -1;

	if (one == two)
		return 0;

	firstprev = one->prev;
	firstnext = one->next;
	secondprev = two->prev;
	secondnext = two->next;

	if (firstprev != NULL)
		firstprev->next = two;
	if (firstnext != NULL)
		firstnext->prev = two;

	if (secondprev != NULL)
		secondprev->next = one;
	if (secondnext != NULL)
		secondnext->prev = one;

	one->next = secondnext;
	one->prev = secondprev;
	two->next = firstnext;
	two->prev = firstprev;

	// Handle special case where nodes were adjacent
	if (firstnext == two)
		one->prev = two;
	if (firstprev == two)
		one->next = two;
	if (secondprev == one)
		two->next = one;
	if (secondnext == one)
		two->prev = one;

	return 0;
}

// Calculate the length of a list
int LL_Length(LinkedList *list)
{
	LL_node *node;
	int num = 0;

	if (!list)
		return -1;

	node = &list->head;

	// Traverse list counting nodes (start at -1 to exclude head sentinel)
	for (num = -1; node != &list->tail; num++)
		node = node->next;

	return num;
}

// Find a node by giving a comparison function and a value
void *LL_Find(LinkedList *list, int (*compare)(void *, void *), void *value)
{
	if (!list)
		return NULL;

	if (!compare)
		return NULL;

	if (!value)
		return NULL;

	// Search from current position (does not rewind automatically)
	for (void *data = LL_Get(list); data; data = LL_GetNext(list))
		if (0 == compare(data, value))
			return data;

	return NULL;
}

// Perform an action for all list elements
void LL_ForAll(LinkedList *list, void *(*action)(void *, void *), void *value)
{
	if (!list)
		return;

	if (!action)
		return;

	LL_Rewind(list);

	if (list->current != NULL) {
		do {
			void *data = LL_Get(list);
			void *result = action(data, value);

			// Check if action changed the data
			if (result != data) {
				if (result != NULL)
					LL_AddNode(list, result);
				else
					LL_DeleteNode(list, PREV);
			}
		} while (LL_Next(list) == 0);
	}
}

// Go to the n-th node in the list and return its data
void *LL_GetByIndex(LinkedList *list, int index)
{
	LL_node *node;
	int num = 0;

	if (!list)
		return NULL;

	if (index < 0)
		return NULL;

	for (node = list->head.next; node != &list->tail; node = node->next) {
		if (num == index)
			return node->data;
		num++;
	}

	return NULL;
}

// Sort list by its contents using bubble sort algorithm
int LL_Sort(LinkedList *list, int (*compare)(void *, void *))
{
	int i, j;
	int numnodes;
	LL_node *best, *last;
	LL_node *current;

	if (!list)
		return -1;

	if (!compare)
		return -1;

	numnodes = LL_Length(list);

	if (0 > LL_End(list))
		return -1;
	last = LL_GetNode(list);

	if (numnodes < 2)
		return 0;

	// Bubble sort: iterate through decreasing ranges
	for (i = numnodes - 1; i > 0; i--) {
		LL_Rewind(list);
		best = last;

		// Find the largest element in current range
		for (j = 0; j < i; j++) {
			current = LL_GetNode(list);
			if (compare(current->data, best->data) > 0) {
				best = current;
			}
			LL_Next(list);
		}

		// Swap largest element to end of current range
		LL_SwapNodes(last, best);

		if (best)
			last = best->prev;
		else
			return -1;
	}

	LL_Rewind(list);

	return 0;
}

// Print debug information about the linked list structure
void LL_dprint(LinkedList *list)
{
	LL_node *current;

	current = &list->head;

	printf("Head:  prev:\t0x%p\taddr:\t0x%p\tnext:\t0x%p\n", list->head.prev, &list->head,
	       list->head.next);

	for (current = current->next; current != &list->tail; current = current->next) {
		printf("node:  prev:\t0x%p\taddr:\t0x%p\tnext:\t0x%p\n", current->prev, current,
		       current->next);
	}

	printf("Tail:  prev:\t0x%p\taddr:\t0x%p\tnext:\t0x%p\n", list->tail.prev, &list->tail,
	       list->tail.next);
}
