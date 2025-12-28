// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/LL.h
 * \brief Doubly linked list data structure definitions and API
 * \author Selene Scriven, William Ferrell, Guillaume Filion, Joris Robijn, Peter Marschall
 * \date 1994-2008
 *
 * \usage
 * Include this header to use the doubly linked list data structure in your
 * application. See the extensive documentation in the file header for usage examples.
 *
 * \details Header file for doubly linked list implementation with support for
 *          various operations including traversal, insertion, deletion, searching,
 *          sorting, and stack/queue operations. The implementation uses sentinel
 *          nodes (head and tail) to simplify edge case handling.
 */

#ifndef LL_H
#define LL_H

/**
 * \page LinkedListUsage Linked List Usage Guide
 * \brief Comprehensive guide for using the doubly-linked list implementation
 *
 * \section Creating Creating a List
 *
 * To create a list, do the following:

    LinkedList *list;
    list = LL_new();
    if(!list) handle_an_error();

  The list can hold any type of data.  You will need to typecast your
  datatype to a "void *", though.  So, to add something to the list,
  the following would be a good way to start:

    typedef struct my_data {
      char string[16];
      int number;
    } my_data;

    my_data *thingie;

    for(something to something else)
    {
      thingie = malloc(sizeof(my_data));
      LL_AddNode(list, (void *)thingie);  // typecast it to a "void *"
    }

    For errors, the general convention is that "0" means success, and
    a negative number means failure.  Check LL.c to be sure, though.

 * \section DataManagement Data Management
 *
 * To change the data, try this:

    thingie = (my_data *)LL_Get(list);  // typecast it back to "my_data"
    thingie->number = another_number;

  You don't need to "Put" the data back, but it doesn't hurt anything.

    LL_Put(list, (void *)thingie);

  However, if you want to point the node's data somewhere else, you'll
  need to get the current data first, keep track of it, then set the data
  to a new location:

    my_data * old_thingie, new_thingie;

    old_thingie = (my_data *)LL_Get(list);
    LL_Put(list, (void *)new_thingie);

    // Now, do something with old_thingie.  (maybe, free it?)

  Or, you could just delete the node entirely and then add a new one:

    my_data * thingie;

    thingie = (my_data *)LL_DeleteNode(list, NEXT);
    free(thingie);

    thingie->number = 666;

    LL_InsertNode(list, (void *)thingie);

 * \section Iteration List Iteration
 *
 * To operate on each list item, try this:

    LL_Rewind(list);
    do {
      my_data = (my_data *)LL_Get(list);
      ... do something to it ...
    } while(LL_Next(list) == 0);

  *******************************************************************

  You can also treat the list like a stack, or a queue.  Just use the
  following functions:

    LL_Push()      // Regular stack stuff: add, remove, peek, rotate
    LL_Pop()
    LL_Top()

    LL_Shift()     // Other end of the stack (like in perl)
    LL_Unshift()
    LL_Look()

    LL_Enqueue()   // Standard queue operations
    LL_Dequeue()

  There are also other goodies, like sorting and searching.

  // See LL.c for more detailed descriptions of these functions.

  *******************************************************************
 * That's about it, for now... Be sure to free the list when you're done!
 */

/**
 * \brief Direction enumeration for linked list navigation
 *
 * \details Symbolic values for navigating and positioning in linked lists.
 * Used by LL_Get() and related functions to specify relative or absolute positions.
 */
typedef enum _direction {
	HEAD = -2,   ///< Beginning of list
	PREV = -1,   ///< Previous node
	CURRENT = 0, ///< Current node
	NEXT = +1,   ///< Next node
	TAIL = +2    ///< End of list
} Direction;

/**
 * \brief Structure for a node in a doubly linked list
 * \details Each node contains pointers to previous and next nodes,
 * plus a void pointer to the actual data payload.
 */
typedef struct LL_node {
	struct LL_node *prev; ///< Pointer to previous node
	struct LL_node *next; ///< Pointer to next node
	void *data;	      ///< Payload (user data)
} LL_node;

/**
 * \brief Structure for a doubly linked list
 * \details Uses sentinel nodes (head/tail anchors) for simplified traversal.
 * The current pointer tracks the active position during iteration.
 */
typedef struct LinkedList {
	LL_node head;	  ///< List's head anchor (sentinel)
	LL_node tail;	  ///< List's tail anchor (sentinel)
	LL_node *current; ///< Pointer to current node during iteration
} LinkedList;

/**
 * \brief Create a new doubly linked list
 * \retval NULL Memory allocation failure
 * \retval !NULL Pointer to freshly created list object
 *
 * \details Allocates memory for a new doubly linked list and initializes
 * it with sentinel head and tail nodes. The current pointer is set to
 * the head node initially.
 */
LinkedList *LL_new(void);

/**
 * \brief Destroy the entire list and free memory
 * \param list List object to be destroyed
 * \retval -1 Error: NULL list pointer provided
 * \retval 0 Success: list destroyed successfully
 * \warning This does not free the data, only the list structure itself
 *
 * \details Frees all memory used by the list structure, including all nodes.
 * The data pointed to by the nodes is not freed - this is the responsibility
 * of the caller to avoid memory leaks.
 */
int LL_Destroy(LinkedList *list);

/**
 * \brief Move to another entry in the list
 * \param list List object to operate on
 * \param whereto Direction where to set the list's current pointer (HEAD, PREV, CURRENT, NEXT,
 * TAIL)
 * \retval NULL Error or when moving beyond ends
 * \retval !NULL New value of list's current pointer
 *
 * \details Set list's current pointer to the node denoted by the direction parameter.
 * This function provides directional navigation within the list.
 */
LL_node *LL_GoTo(LinkedList *list, Direction whereto);

/**
 * \brief Return to the beginning of the list
 * \param list List object
 * \retval -1 Error: no list given
 * \retval 0 Success
 *
 * \details Set list's current pointer to the first node in the list.
 */
int LL_Rewind(LinkedList *list);

/**
 * \brief Jump to the end of the list
 * \param list List object
 * \retval -1 Error: no list given
 * \retval 0 Success
 *
 * \details Set list's current pointer to the last node in the list.
 */
int LL_End(LinkedList *list);

/**
 * \brief Go to the next node
 * \param list List object
 * \retval -1 Error: no list given or no next node
 * \retval 0 Success
 *
 * \details Advance list's current pointer to the next node in the list.
 */
int LL_Next(LinkedList *list);

/**
 * \brief Go to the previous node
 * \param list List object
 * \retval -1 Error: no list given or no previous node
 * \retval 0 Success
 *
 * \details Set list's current pointer to the previous node in the list.
 */
int LL_Prev(LinkedList *list);

/**
 * \brief Access current node's data
 * \param list List object
 * \retval NULL Empty payload or error
 * \retval !NULL Pointer to current node's payload data
 *
 * \details Return pointer to list's current node's data.
 */
void *LL_Get(LinkedList *list);

/**
 * \brief Set/change current node's data
 * \param list List object
 * \param data Pointer to data to be set
 * \retval -1 Error: no list given, or no current node
 * \retval 0 Success
 *
 * \details Set the data payload of the current node to the provided pointer.
 */
int LL_Put(LinkedList *list, void *data);

/**
 * \brief Get current node in list
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to current node
 * \warning Don't use this unless you really know what you're doing
 *
 * \details Return a pointer to the current node structure, not its data.
 */
LL_node *LL_GetNode(LinkedList *list);

/**
 * \brief Set list's current pointer to a specific node
 * \param list List object
 * \param node Node to become new current
 * \retval -1 Error
 * \retval 0 Success
 * \warning Don't use this unless you really know what you're doing
 *
 * \details Directly set the current pointer to a specific node without validation.
 */
int LL_PutNode(LinkedList *list, LL_node *node);

/**
 * \brief Access list's first node's data
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to first node's data
 *
 * \details Set list's current pointer to the first node and return its data.
 */
void *LL_GetFirst(LinkedList *list);

/**
 * \brief Access next node's data
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to next node's data
 *
 * \details Advance list's current pointer to the next node and return its data.
 */
void *LL_GetNext(LinkedList *list);

/**
 * \brief Access previous node's data
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to previous node's data
 *
 * \details Set list's current pointer to the previous node and return its data.
 */
void *LL_GetPrev(LinkedList *list);

/**
 * \brief Access list's last node's data
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to last node's data
 *
 * \details Set list's current pointer to the last node and return its data.
 */
void *LL_GetLast(LinkedList *list);

/**
 * \brief Add/append a new node after current one in the list
 * \param list List object
 * \param add Pointer to new node's data
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details Update the list's current pointer to point to the freshly created node.
 */
int LL_AddNode(LinkedList *list, void *add);

/**
 * \brief Add/insert a new node before current one in the list
 * \param list List object
 * \param add Pointer to new node's data
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details Update the list's current pointer to point to the freshly created node.
 */
int LL_InsertNode(LinkedList *list, void *add);

/**
 * \brief Remove current node from the list
 * \param list List object
 * \param whereto Direction where to set the list's current pointer
 * \retval NULL Error
 * \retval !NULL Pointer to data of deleted node
 *
 * \details Set the list's current pointer to the one denoted by whereto.
 */
void *LL_DeleteNode(LinkedList *list, Direction whereto);

/**
 * \brief Remove a specific node from the list
 * \param list List object
 * \param data Pointer to data of node to delete
 * \param whereto Direction where to set the list's current pointer
 * \retval NULL Error
 * \retval !NULL Pointer to data of deleted node
 *
 * \details Find a node by a pointer to its data and remove it.
 * Set the list's current pointer to the one denoted by whereto.
 */
void *LL_Remove(LinkedList *list, void *data, Direction whereto);

/**
 * \brief Add/append a new node after the last one in the list (stack push)
 * \param list List object
 * \param add Pointer to new node's data
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details Jump to the last node in the list, append a new node
 * and make this new one the list's current one.
 */
int LL_Push(LinkedList *list, void *add);

/**
 * \brief Remove the last node from the list (stack pop)
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to data of deleted node
 *
 * \details Jump to the last node in the list, remove it from the list
 * and return its data.
 */
void *LL_Pop(LinkedList *list);

/**
 * \brief Access list's last node's data (stack top)
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to last node's data
 *
 * \details Set list's current pointer to the last node and return its data.
 */
void *LL_Top(LinkedList *list);

/**
 * \brief Remove the first node from the list (queue shift)
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to data of deleted node
 *
 * \details Jump to the first node in the list, remove it from the list and return its data.
 */
void *LL_Shift(LinkedList *list);

/**
 * \brief Access list's first node's data (queue peek)
 * \param list List object
 * \retval NULL Error
 * \retval !NULL Pointer to first node's data
 *
 * \details Set list's current pointer to the first node and return its data.
 */
void *LL_Look(LinkedList *list);

/**
 * \brief Add/insert a new node before the first one in the list (queue unshift)
 * \param list List object
 * \param add Pointer to new node's data
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details Jump to the first node in the list and insert a new node before that one.
 */
int LL_Unshift(LinkedList *list, void *add);

/**
 * \brief Queue enqueue operation (alias for LL_Push)
 * \param list List object
 * \param add Data to enqueue
 * \details Adds an element to the end of the queue.
 */
#define LL_Enqueue(list, add) LL_Push(list, add)

/**
 * \brief Queue dequeue operation (alias for LL_Shift)
 * \param list List object
 * \details Removes and returns an element from the front of the queue.
 */
#define LL_Dequeue(list) LL_Shift(list)

/**
 * \brief Add an item to the end of its "priority group"
 * \param list List object
 * \param add Pointer to new node's data
 * \param compare Pointer to a comparison function
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details The list is assumed to be sorted already.
 */
int LL_PriorityEnqueue(LinkedList *list, void *add, int (*compare)(void *, void *));

/**
 * \brief Switch two nodes positions
 * \param one First list node
 * \param two Second list node
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details Exchange the positions of two nodes in the list by updating all necessary pointer
 * connections.
 */
int LL_SwapNodes(LL_node *one, LL_node *two);

/**
 * \brief Calculate the length of a list
 * \param list List object
 * \retval -1 Error
 * \retval >=0 Number of nodes in the list
 *
 * \details Traverse the entire list and count the number of nodes, excluding sentinels.
 */
int LL_Length(LinkedList *list);

/**
 * \brief Find a node by giving a comparison function and a value
 * \param list List object to search in
 * \param compare Pointer to a comparison function that returns 0 when matching
 * \param value Pointer to the value used for matching
 * \retval NULL Not found or error
 * \retval !NULL The found node's data pointer
 * \warning This does not rewind the list first! Call LL_Rewind() if you want to start from the
 * beginning
 *
 * \details Go to the list node whose data matches the given value and return the data.
 * The search starts from the current position in the list.
 */
void *LL_Find(LinkedList *list, int (*compare)(void *, void *), void *value);

/**
 * \brief Perform an action for all list elements
 * \param list List object
 * \param action Pointer to the action function
 * \param value Pointer to data used as second argument to action()
 * \warning Removing/creating client payload is up to the action() function
 *
 * \details Execute a function on the data of each node in the list.
 * Depending on the result of the function, new nodes may get added,
 * nodes may get deleted or simply changed by the function itself.
 */
void LL_ForAll(LinkedList *list, void *(*action)(void *, void *), void *value);

/**
 * \brief Go to the n-th node in the list and return its data
 * \param list List object
 * \param index Index of the node whose data we want (0-based)
 * \retval NULL Error or index out of bounds
 * \retval !NULL The found node's data pointer
 *
 * \details Go to the list node with the given index and return the data.
 */
void *LL_GetByIndex(LinkedList *list, int index);

/**
 * \brief Sort list by its contents
 * \param list List object
 * \param compare Pointer to a comparison function
 * \retval -1 Error
 * \retval 0 Success
 *
 * \details The list gets sorted using a comparison function for the data of its nodes.
 * After the sorting, the list's current pointer is set to the first node.
 */
int LL_Sort(LinkedList *list, int (*compare)(void *, void *));

/**
 * \brief Print debug information about the linked list structure
 * \param list List object to examine
 *
 * \details Prints memory addresses and pointers for each node in the list
 * for debugging purposes. Only available in debug builds.
 */
void LL_dprint(LinkedList *list);

#endif
