// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/input.h
 * \brief Input handling system interface
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 *
 * \features
 * - Key reservation system for exclusive and shared key access control
 * - Client-specific key routing with conflict resolution mechanisms
 * - Server navigation key handling for menu system integration
 * - Input event processing from hardware drivers with key translation
 * - Key conflict resolution between multiple clients requesting same keys
 * - Client lifecycle management with automatic key release on disconnect
 * - Thread-safe key reservation operations with atomic updates
 * - Debug logging for all input operations and key state changes
 * - Memory management for key reservation structures and cleanup
 * - Global input state management with reservation tracking
 *
 * \usage
 * - Used by LCDd server core for managing client input access
 * - Reserve keys for specific clients via input_reserve_key() with exclusivity control
 * - Handle input events from drivers via handle_input() for key processing
 * - Route processed keys to appropriate clients or server navigation functions
 * - Manage client disconnections via input_release_client_keys() for cleanup
 * - Query key reservations via input_find_key() for access validation
 * - Initialize input system via input_init() during server startup
 * - Shutdown input system via input_shutdown() during server cleanup
 *
 * \details Interface for handling keypad and other input from users with
 * comprehensive key reservation and routing system for multi-client support.
 */

#ifndef INPUT_H
#define INPUT_H

#include "client.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#include "shared/defines.h"

/**
 * \brief Key reservation structure
 * \details Contains key name, exclusivity flag, and owning client
 */
typedef struct KeyReservation {
	char *key;	/**< Key name string */
	bool exclusive; /**< True if key is exclusively reserved */
	Client *client; /**< Owning client (NULL for server keys) */
} KeyReservation;

/**
 * \brief Process keypad input while displaying screens
 * \details Main input processing function that handles keypad input
 * and routes keys to appropriate clients or server functions
 */
void handle_input(void);

/**
 * \brief Initialize the input handling system
 * \return 0 on success, -1 on error
 * \details Sets up input system data structures and prepares for key handling
 */
int input_init(void);

/**
 * \brief Shutdown the input handling system
 * \details Cleans up input system resources and releases all key reservations
 */
void input_shutdown(void);

/**
 * \brief Reserve a key for a client
 * \param key Key name to reserve
 * \param exclusive True for exclusive access, false for shared
 * \param client Client requesting the key (NULL for server)
 * \return 0 on success, -1 if reservation not possible
 * \details Attempts to reserve the specified key for the given client
 */
int input_reserve_key(const char *key, bool exclusive, Client *client);

/**
 * \brief Release a specific key reservation
 * \param key Key name to release
 * \param client Client that owns the reservation
 * \details Removes the key reservation if it matches the client
 */
void input_release_key(const char *key, Client *client);

/**
 * \brief Release all key reservations for a client
 * \param client Client whose keys should be released
 * \details Called when client disconnects to clean up all its key reservations
 */
void input_release_client_keys(Client *client);

/**
 * \brief Find key reservation for given key and client
 * \param key Key name to search for
 * \param client Client to match against
 * \return Pointer to KeyReservation if found, NULL otherwise
 * \details Searches for key reservation, considering exclusivity rules
 */
KeyReservation *input_find_key(const char *key, Client *client);

#endif
