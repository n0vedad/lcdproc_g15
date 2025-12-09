// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/client.c
 * \brief Client data and actions implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2002
 *
 *
 * \features
 * - Implementation of client management functions for LCDd server core
 * - Client creation and destruction with complete resource management
 * - Message queue management using linked list data structures
 * - Screen list operations for client-owned display screens
 * - Menu hierarchy cleanup and menuitem destruction handling
 * - Socket handling with proper connection management
 * - Key reservation management and client key release functionality
 * - Memory management with error handling and proper cleanup
 * - Debug logging throughout all client operations
 * - Client state management (NEW, ACTIVE, GONE) with lifecycle tracking
 * - Linked list integration for message queues and screen collections
 * - Automatic resource cleanup on client disconnect or destruction
 *
 * \usage
 * - Used by LCDd server core for managing client connections and operations
 * - Client creation when new TCP connections are established via client_create()
 * - Message handling for client command processing via add/get_message functions
 * - Screen management for organizing client display content via add/remove/find functions
 * - Client cleanup during disconnect or server shutdown via client_destroy()
 * - Screen counting for resource monitoring via client_screen_count()
 * - Socket management for client communication channel handling
 * - Menu system integration for interactive client menu hierarchies
 * - Key reservation handling for input event routing to specific clients
 *
 * \details Implementation of client management functions for LCDd server
 * handling client lifecycle, message queuing, screen management, and cleanup.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "input.h"
#include "menuscreens.h"
#include "render.h"
#include "screen.h"
#include "screenlist.h"

#include "shared/LL.h"
#include "shared/report.h"

// Create a new client structure for incoming connection
Client *client_create(int sock)
{
	Client *c;

	debug(RPT_DEBUG, "%s(sock=%i)", __FUNCTION__, sock);

	c = malloc(sizeof(Client));
	if (!c) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		return NULL;
	}

	c->sock = sock;
	c->messages = NULL;
	c->backlight = BACKLIGHT_OPEN;
	c->heartbeat = HEARTBEAT_OPEN;

	c->messages = LL_new();
	if (!c->messages) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		free(c);
		return NULL;
	}

	c->state = NEW;
	c->name = NULL;
	c->menu = NULL;

	c->screenlist = LL_new();
	if (!c->screenlist) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		LL_Destroy(c->messages);
		free(c);
		return NULL;
	}

	return c;
}

// Destroy client and free all associated resources
int client_destroy(Client *c)
{
	Screen *s;
	Menu *m;
	char *str;

	if (!c)
		return -1;

	debug(RPT_DEBUG, "%s(c=[%d])", __FUNCTION__, c->sock);

	while ((str = client_get_message(c))) {
		free(str);
	}
	LL_Destroy(c->messages);

	debug(RPT_DEBUG, "%s: Cleaning screenlist", __FUNCTION__);

	for (s = LL_GetFirst(c->screenlist); s; s = LL_GetNext(c->screenlist)) {
		screen_destroy(s);
	}
	LL_Destroy(c->screenlist);

	m = (Menu *)c->menu;
	if (m) {
		menuscreen_inform_item_destruction(m);
		menu_remove_item(m->parent, m);
		menuscreen_inform_item_modified(m->parent);
		menuitem_destroy(m);
	}

	input_release_client_keys(c);
	close(c->sock);

	c->state = GONE;

	if (c->name)
		free(c->name);

	free(c);

	debug(RPT_DEBUG, "%s: Client data removed", __FUNCTION__);
	return 0;
}

// Close client socket connection
void client_close_sock(Client *c)
{
	if (!c)
		return;

	debug(RPT_DEBUG, "%s(c=[%d])", __FUNCTION__, c->sock);

	if (c->sock >= 0) {
		close(c->sock);
		c->sock = -1;
	}
}

// Add message to client's incoming message queue
int client_add_message(Client *c, char *message)
{
	int err = 0;

	if (!c)
		return -1;
	if (!message)
		return -1;

	if (strlen(message) > 0) {
		debug(RPT_DEBUG, "%s(c=[%d], message=\"%s\")", __FUNCTION__, c->sock, message);
		err = LL_Enqueue(c->messages, (void *)message);
	}

	return err;
}

// Get next message from client's message queue
char *client_get_message(Client *c)
{
	char *str;

	debug(RPT_DEBUG, "%s(c=[%d])", __FUNCTION__, c->sock);

	if (!c)
		return NULL;

	str = (char *)LL_Dequeue(c->messages);

	return str;
}

// Find screen by ID in client's screen list
Screen *client_find_screen(Client *c, char *id)
{
	Screen *s;

	if (!c)
		return NULL;
	if (!id)
		return NULL;

	debug(RPT_DEBUG, "%s(c=[%d], id=\"%s\")", __FUNCTION__, c->sock, id);

	LL_Rewind(c->screenlist);
	do {
		s = LL_Get(c->screenlist);
		if ((s) && (0 == strcmp(s->id, id))) {
			debug(RPT_DEBUG, "%s: Found %s", __FUNCTION__, id);
			return s;
		}
	} while (LL_Next(c->screenlist) == 0);

	return NULL;
}

// Add screen to client's screen list and global screen list
int client_add_screen(Client *c, Screen *s)
{
	if (!c)
		return -1;
	if (!s)
		return -1;

	debug(RPT_DEBUG, "%s(c=[%d], s=[%s])", __FUNCTION__, c->sock, s->id);

	LL_Push(c->screenlist, (void *)s);
	screenlist_add(s);

	return 0;
}

// Remove screen from client's screen list and global screen list
int client_remove_screen(Client *c, Screen *s)
{
	if (!c)
		return -1;
	if (!s)
		return -1;

	debug(RPT_DEBUG, "%s(c=[%d], s=[%s])", __FUNCTION__, c->sock, s->id);

	/**
	 * \todo Check for errors here?
	 *
	 * Missing error handling when removing screens from client's screenlist. The LL_Remove()
	 * operation may fail (e.g., screen not in list, memory issues), but errors are currently
	 * not checked or handled.
	 *
	 * Should check LL_Remove() return value and handle failures appropriately:
	 * - Log error if removal fails
	 * - Return error code to caller
	 * - Prevent screenlist_remove() if LL_Remove() failed
	 *
	 * Impact: Robustness, error handling completeness
	 *
	 * \ingroup ToDo_low
	 */

	LL_Remove(c->screenlist, (void *)s, NEXT);
	screenlist_remove(s);

	return 0;
}

// Get number of screens owned by client
int client_screen_count(Client *c) { return LL_Length(c->screenlist); }
