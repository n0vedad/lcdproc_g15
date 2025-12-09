// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/client.h
 * \brief Client data structures and function declarations
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2002
 *
 *
 * \features
 * - Header file defining client data structures and function declarations for LCDd server
 * - Client connection management with socket handling and state tracking
 * - Client state enumeration (NEW, ACTIVE, GONE) for connection lifecycle management
 * - Message queue handling for client command processing and communication
 * - Screen list management for client-owned display screens
 * - Menu hierarchy support for interactive client menus
 * - Client structure with name, state, socket, backlight, and heartbeat properties
 * - Linked list integration for message queues and screen collections
 * - Function declarations for client lifecycle management (create, destroy, close)
 * - Screen management functions for adding, removing, and finding client screens
 * - Message handling functions for queuing and retrieving client commands
 * - Conditional compilation support for type-only includes
 *
 * \usage
 * - Used by LCDd server core for managing client connections and state
 * - Primary header for client-related operations and data structures
 * - Include normally for full client functionality and function declarations
 * - Use INC_TYPES_ONLY pattern for type-only includes without function dependencies
 * - Client creation when new TCP connections are established
 * - State management during client hello/bye protocol handling
 * - Screen management for client display content organization
 * - Message queuing for command processing and client communication
 * - Menu system integration for interactive client applications
 *
 * \details Defines all the client data and actions for LCDd server
 * managing client connections, state, and associated screens.
 */

#ifndef CLIENT_H_TYPES
#define CLIENT_H_TYPES

#include "shared/LL.h"

#define CLIENT_NAME_SIZE 256 ///< Maximum size for client name strings including null terminator

/**
 * \brief Possible states of a client connection
 * \details Enumeration tracking client lifecycle from connection to disconnection
 */
typedef enum _clientstate {
	// Client connected but hasn't sent hello command yet
	NEW,
	// Client sent hello command and is actively communicating
	ACTIVE,
	// Client sent bye command or connection was terminated
	GONE
} ClientState;

/**
 * \brief Structure representing a client connection to the LCDd server
 * \details Contains all client-specific data, state, and associated resources
 */
typedef struct Client {
	// Client name string (max CLIENT_NAME_SIZE characters)
	char *name;
	// Current connection state (NEW, ACTIVE, GONE)
	ClientState state;
	// Socket file descriptor for client connection
	int sock;
	// Backlight state preference for this client
	int backlight;
	// Heartbeat mode setting for connection monitoring
	int heartbeat;

	// Queue of messages received from the client
	LinkedList *messages;
	// List of screens owned by this client
	LinkedList *screenlist;

	// Optional menu hierarchy for interactive clients
	void *menu;
} Client;

#endif

#ifndef INC_TYPES_ONLY
#ifndef CLIENT_H_FNCS
#define CLIENT_H_FNCS ///< Include guard for client function declarations

#define INC_TYPES_ONLY 1 ///< Flag to include only type definitions from dependent headers
#include "screen.h"
#undef INC_TYPES_ONLY

/**
 * \brief Create a new client structure for an incoming connection
 * \param sock Socket file descriptor for the client connection
 * \return Pointer to newly allocated Client structure, or NULL on error
 * \details Initializes a new client with default state NEW and allocates
 * required data structures for message and screen management.
 */
Client *client_create(int sock);

/**
 * \brief Destroy a client structure and free all associated resources
 * \param c Pointer to Client structure to destroy
 * \return 0 on success, -1 on error
 * \details Frees all client resources including name, message queue,
 * screen list, and the client structure itself.
 */
int client_destroy(Client *c);

/**
 * \brief Close the client's socket connection
 * \param c Pointer to Client structure
 * \details Safely closes the client socket and marks it as invalid.
 */
void client_close_sock(Client *c);

/**
 * \brief Add a message to the client's incoming message queue
 * \param c Pointer to Client structure
 * \param message Message string to add to queue
 * \return 0 on success, -1 on error
 * \details Queues incoming messages from client for later processing.
 */
int client_add_message(Client *c, char *message);

/**
 * \brief Retrieve the next message from client's message queue
 * \param c Pointer to Client structure
 * \return Pointer to message string, or NULL if queue is empty
 * \details Returns and removes the oldest message from the queue.
 */
char *client_get_message(Client *c);

/**
 * \brief Find a screen by ID in the client's screen list
 * \param c Pointer to Client structure
 * \param id Screen identifier string
 * \return Pointer to Screen structure, or NULL if not found
 * \details Searches the client's screen list for a screen with matching ID.
 */
Screen *client_find_screen(Client *c, char *id);

/**
 * \brief Add a screen to the client's screen list
 * \param c Pointer to Client structure
 * \param s Pointer to Screen structure to add
 * \return 0 on success, -1 on error
 * \details Associates a screen with the client for display management.
 */
int client_add_screen(Client *c, Screen *s);

/**
 * \brief Remove a screen from the client's screen list
 * \param c Pointer to Client structure
 * \param s Pointer to Screen structure to remove
 * \return 0 on success, -1 on error
 * \details Disassociates a screen from the client.
 */
int client_remove_screen(Client *c, Screen *s);

/**
 * \brief Get the number of screens owned by the client
 * \param c Pointer to Client structure
 * \return Number of screens in client's screen list
 * \details Returns count of screens currently associated with the client.
 */
int client_screen_count(Client *c);

#endif
#endif
