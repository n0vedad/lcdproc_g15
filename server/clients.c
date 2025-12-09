// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/clients.c
 * \brief Client list management implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2002
 *
 *
 * \features
 * - Implementation of client list management for LCDd server core
 * - Client list initialization and shutdown with proper resource management
 * - Client addition and removal from global linked list with error handling
 * - Client search functionality by socket descriptor for message routing
 * - Client list iteration support for traversing connected clients
 * - Global client list maintenance using LinkedList data structure
 * - Proper cleanup of all clients during server shutdown
 * - Debug logging for all client list operations and state changes
 * - Error handling with proper reporting for all operations
 * - Memory management with automatic resource cleanup
 * - Client count tracking for server monitoring
 * - Direction-based client removal for list traversal control
 *
 * \usage
 * - Used by LCDd server core for managing the global list of connected clients
 * - Initialize client list at server startup via clients_init()
 * - Add new clients when TCP connections are established
 * - Remove clients when connections close or timeout
 * - Find clients by socket descriptor for routing incoming messages
 * - Iterate through client list for broadcasting server messages
 * - Track client count for server status monitoring
 * - Clean shutdown of all clients via clients_shutdown()
 * - Client lookup for command processing and state management
 *
 * \details Implementation of client list management for LCDd server
 * containing functions to handle client connections and data structures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "clients.h"
#include "render.h"

#include "shared/LL.h"
#include "shared/report.h"

/** \brief Global linked list containing all connected clients
 *
 * \details Initialized by clients_init(), destroyed by clients_shutdown().
 * Stores Client* pointers for all active client connections to LCDd server.
 */
LinkedList *clientlist = NULL;

// Initialize the global client list data structure
int clients_init(void)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	clientlist = LL_new();
	if (!clientlist) {
		report(RPT_ERR, "%s: Unable to create client list", __FUNCTION__);
		return -1;
	}

	return 0;
}

// Shutdown client list and free all resources
int clients_shutdown(void)
{
	Client *c;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	if (!clientlist) {
		return -1;
	}

	for (c = LL_GetFirst(clientlist); c; c = LL_GetNext(clientlist)) {
		debug(RPT_DEBUG, "%s: ...", __FUNCTION__);
		if (c) {
			debug(RPT_DEBUG, "%s: ... %i ...", __FUNCTION__, c->sock);
			if (client_destroy(c) != 0) {
				report(RPT_ERR, "%s: Error freeing client", __FUNCTION__);
			} else {
				debug(RPT_DEBUG, "%s: Freed client...", __FUNCTION__);
			}
		} else {
			debug(RPT_DEBUG, "%s: No client!", __FUNCTION__);
		}
	}

	LL_Destroy(clientlist);

	debug(RPT_DEBUG, "%s: done", __FUNCTION__);

	return 0;
}

// Add client to the global client list
Client *clients_add_client(Client *c)
{
	if (LL_Push(clientlist, c) == 0)
		return c;

	return NULL;
}

// Remove client from the global client list
Client *clients_remove_client(Client *c, Direction whereto)
{
	Client *client = LL_Remove(clientlist, c, whereto);

	return client;
}

// Get first client in the client list
Client *clients_getfirst(void) { return (Client *)LL_GetFirst(clientlist); }

// Get next client in the client list
Client *clients_getnext(void) { return (Client *)LL_GetNext(clientlist); }

// Get total number of clients in the list
int clients_client_count(void) { return LL_Length(clientlist); }

// Find client by socket file descriptor
Client *clients_find_client_by_sock(int sock)
{
	Client *c;

	debug(RPT_DEBUG, "%s(sock=%i)", __FUNCTION__, sock);

	for (c = LL_GetFirst(clientlist); c; c = LL_GetNext(clientlist)) {
		if (c->sock == sock) {
			return c;
		}
	}

	debug(RPT_ERR, "%s: failed", __FUNCTION__);

	return NULL;
}
