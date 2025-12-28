// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/clients.h
 * \brief Client list management interface
 * \author William Ferrell
 * \author Selene Scriven
 * \date 1999
 *
 *
 * \features
 * - Header file for client list management interface in LCDd server
 * - Client list initialization and shutdown functions for server lifecycle
 * - Client addition and removal operations with directional positioning
 * - Client search functionality by socket descriptor for message routing
 * - Client list iteration functions for traversing connected clients
 * - Client count tracking for server monitoring and resource management
 * - Function declarations for client list operations and management
 * - Integration with client.h for Client structure and related definitions
 * - Direction enumeration support for client list positioning operations
 *
 * \usage
 * - Used by LCDd server core for managing the global list of connected clients
 * - Initialize client list at server startup via clients_init()
 * - Add new clients when TCP connections are established via clients_add_client()
 * - Remove clients when connections close or timeout via clients_remove_client()
 * - Find clients by socket descriptor for message routing via clients_find_client_by_sock()
 * - Iterate through client list for broadcasting or monitoring via getfirst/getnext
 * - Track client count for server status and resource monitoring
 * - Server shutdown cleanup via clients_shutdown()
 *
 * \details Manages the list of clients that are connected to the LCDd server
 * providing functions for client list initialization, addition/removal, and lookup.
 */

#ifndef CLIENTS_H
#define CLIENTS_H

#include "client.h"

/**
 * \brief Initialize the client list data structure
 * \return 0 on success, -1 on error
 * \details Sets up the internal client list structure and prepares
 * the client management system for operation.
 */
int clients_init(void);

/**
 * \brief Shutdown and cleanup the client list
 * \return 0 on success, -1 on error
 * \details Destroys all clients in the list and cleans up the
 * client list data structure.
 */
int clients_shutdown(void);

/**
 * \brief Add a client to the client list
 * \param c Pointer to Client structure to add
 * \return Pointer to added Client, or NULL on error
 * \details Adds the specified client to the global client list
 * for server management and tracking.
 */
Client *clients_add_client(Client *c);

/**
 * \brief Remove a client from the client list
 * \param c Pointer to Client structure to remove
 * \param whereto Direction for list positioning after removal
 * \return Pointer to next Client in specified direction, or NULL
 * \details Removes the specified client from the global client list
 * and returns the next client in the specified direction.
 */
Client *clients_remove_client(Client *c, Direction whereto);

/**
 * \brief Get the first client in the client list
 * \return Pointer to first Client, or NULL if list is empty
 * \details Positions the list iterator at the first client and
 * returns a pointer to it for iteration purposes.
 */
Client *clients_getfirst(void);

/**
 * \brief Get the next client in the client list
 * \return Pointer to next Client, or NULL if at end of list
 * \details Advances the list iterator to the next client and
 * returns a pointer to it for continued iteration.
 */
Client *clients_getnext(void);

/**
 * \brief Get the total number of clients in the list
 * \return Number of clients currently in the client list
 * \details Returns the count of clients for server monitoring
 * and resource management purposes.
 */
int clients_client_count(void);

/**
 * \brief Find a client by socket file descriptor
 * \param sock Socket file descriptor to search for
 * \return Pointer to Client with matching socket, or NULL if not found
 * \details Searches the client list for a client with the specified
 * socket descriptor for message routing and client identification.
 */
Client *clients_find_client_by_sock(int sock);

#endif
