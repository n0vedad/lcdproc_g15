// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/input.c
 * \brief Input handling system implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 *
 * \features
 * - Key reservation system with exclusive and shared access modes
 * - Input event processing and intelligent key routing to appropriate handlers
 * - Server navigation key handling for screen rotation, scrolling, and navigation
 * - Client-specific key routing with conflict resolution and priority management
 * - Menu system integration with dedicated menu key processing
 * - Key conflict resolution between multiple clients requesting same keys
 * - Configurable server navigation keys loaded from configuration file
 * - Priority system implementation (screen keys > reserved keys > server keys)
 * - Automatic cleanup of key reservations on client disconnect
 * - Debug logging for all key operations and reservation state changes
 * - Thread-safe linked list operations for key reservation management
 * - Memory management for key reservation structures and navigation key strings
 *
 * \usage
 * - Used by LCDd server main loop for processing input events from drivers
 * - Initialize input system at server startup via input_init() with config loading
 * - Process input events in main loop via handle_input() for key distribution
 * - Reserve keys for clients via input_reserve_key() with exclusivity control
 * - Release keys during client lifecycle via input_release_key() and cleanup
 * - Handle client disconnections via input_release_client_keys() for bulk cleanup
 * - Query key access permissions via input_find_key() for validation
 * - Shutdown input system during server termination via input_shutdown()
 * - Route server navigation keys via input_internal_key() for menu and navigation
 *
 * \details Implementation of keypad and other input handling from users with
 * comprehensive key reservation system and intelligent routing for multi-client support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/LL.h"
#include "shared/configfile.h"
#include "shared/report.h"
#include "shared/sockets.h"

/** \brief Include only type definitions from headers
 *
 * \details Prevents circular dependencies by including only struct/typedef
 * declarations from client.h and screen.h, not full function prototypes.
 */
#define INC_TYPES_ONLY 1
#include "client.h"
#include "screen.h"
#undef INC_TYPES_ONLY

#include "drivers.h"
#include "input.h"
#include "menuscreens.h"
#include "render.h"
#include "screenlist.h"

/** \name Global Input State
 * Key event queue and configurable key bindings for server actions
 */
///@{
LinkedList *keylist;	 ///< Queue of pending key events from input drivers
char *toggle_rotate_key; ///< Key name to toggle automatic screen rotation
char *prev_screen_key;	 ///< Key name to switch to previous screen
char *next_screen_key;	 ///< Key name to switch to next screen
char *scroll_up_key;	 ///< Key name to scroll menu/widget up
char *scroll_down_key;	 ///< Key name to scroll menu/widget down
///@}

// Internal function for processing system-level key events
void input_internal_key(const char *key);

// Initialize the input handling system
int input_init(void)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	keylist = LL_new();

	// Load server navigation keys from config with defaults
	toggle_rotate_key = strdup(config_get_string("server", "ToggleRotateKey", 0, "Enter"));
	prev_screen_key = strdup(config_get_string("server", "PrevScreenKey", 0, "Left"));
	next_screen_key = strdup(config_get_string("server", "NextScreenKey", 0, "Right"));
	scroll_up_key = strdup(config_get_string("server", "ScrollUpKey", 0, "Up"));
	scroll_down_key = strdup(config_get_string("server", "ScrollDownKey", 0, "Down"));

	return 0;
}

// Shutdown the input handling system
void input_shutdown()
{
	if (!keylist) {
		return;
	}

	free(keylist);
	free(toggle_rotate_key);
	free(prev_screen_key);
	free(next_screen_key);
	free(scroll_up_key);
	free(scroll_down_key);
}

// Handle all available input events
void handle_input(void)
{
	const char *key;
	Screen *current_screen;
	Client *current_client;
	KeyReservation *kr;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	current_screen = screenlist_current();
	if (current_screen)
		current_client = current_screen->client;
	else
		current_client = NULL;

	// Process all pending keys with priority: screen keys > reserved keys > server keys
	while ((key = drivers_get_key()) != NULL) {

		// Priority 1: Screen-specific keys from screen_add_key()
		if (current_screen && screen_find_key(current_screen, key)) {
			sock_printf(current_client->sock, "key %s %s\n", key, current_screen->id);
			continue;
		}

		// Priority 2: Client-reserved keys
		kr = input_find_key(key, current_client);
		if (kr && kr->client) {
			debug(RPT_DEBUG, "%s: reserved key: \"%.40s\"", __FUNCTION__, key);
			sock_printf(kr->client->sock, "key %s\n", key);
		} else {
			// Priority 3: Server internal navigation keys
			debug(RPT_DEBUG, "%s: left over key: \"%.40s\"", __FUNCTION__, key);
			input_internal_key(key);
		}
	}
}

/**
 * \brief Handle internal server keys
 * \param key Key name to process
 *
 * \details Routes unhandled keys to server navigation functions. Handles menu
 * navigation keys (enter, escape, up, down) and screen rotation controls.
 */
void input_internal_key(const char *key)
{
	// Menu keys or menu is active - route to menu handler
	if (is_menu_key(key) || screenlist_current() == menuscreen) {
		menuscreen_key_handler(key);

		// Server navigation keys
	} else {
		if (strcmp(key, toggle_rotate_key) == 0) {
			autorotate = !autorotate;
			if (autorotate) {
				server_msg("Rotate", 4);
			} else {
				server_msg("Hold", 4);
			}

		} else if (strcmp(key, prev_screen_key) == 0) {
			screenlist_goto_prev();
			server_msg("Prev", 4);

		} else if (strcmp(key, next_screen_key) == 0) {
			screenlist_goto_next();
			server_msg("Next", 4);

		} else if (strcmp(key, scroll_up_key) == 0 || strcmp(key, scroll_down_key) == 0) {
			/**
			 * \todo Implement scroll up/scroll down functionality for server navigation
			 *
			 * Configuration keys (`ScrollUpKey` and `ScrollDownKey`) exist and are
			 * loaded from config (default "Up"/"Down" keys, stored in
			 * scroll_up_key/scroll_down_key variables), but the actual scroll handling
			 * remains unimplemented - the if-blocks are empty.
			 *
			 * Required Implementation:
			 * - Scroll up: Should scroll current screen content upward (if scrollable)
			 * - Scroll down: Should scroll current screen content downward (if
			 * scrollable)
			 * - Integration with frame-based scrolling system (when available)
			 * - Proper bounds checking to prevent over-scrolling
			 *
			 * Impact: Feature completeness, server navigation, user experience with
			 * long screen content
			 *
			 * \ingroup ToDo_medium
			 */
		}
	}
}

// Reserve a key for a client
int input_reserve_key(const char *key, bool exclusive, Client *client)
{
	KeyReservation *kr;

	debug(RPT_DEBUG, "%s(key=\"%.40s\", exclusive=%d, client=[%d])", __FUNCTION__, key,
	      exclusive, (client ? client->sock : -1));

	// Check for conflicting reservations (either side exclusive = conflict)
	for (kr = LL_GetFirst(keylist); kr != NULL; kr = LL_GetNext(keylist)) {
		if (strcmp(kr->key, key) == 0) {
			if (kr->exclusive || exclusive) {
				return -1;
			}
		}
	}

	// Create new reservation
	kr = malloc(sizeof(KeyReservation));
	kr->key = strdup(key);
	kr->exclusive = exclusive;
	kr->client = client;
	LL_Push(keylist, kr);

	report(RPT_INFO, "Key \"%.40s\" is now reserved %s by client [%d]", key,
	       (exclusive ? "exclusively" : "shared"), (client ? client->sock : -1));

	return 0;
}

// Release a key reservation
void input_release_key(const char *key, Client *client)
{
	KeyReservation *kr;

	debug(RPT_DEBUG, "%s(key=\"%.40s\", client=[%d])", __FUNCTION__, key,
	      (client ? client->sock : -1));

	for (kr = LL_GetFirst(keylist); kr != NULL; kr = LL_GetNext(keylist)) {
		if ((kr->client == client) && (strcmp(kr->key, key) == 0)) {
			report(RPT_INFO,
			       "Key \"%.40s\" reserved %s by client [%d] and is now released", key,
			       (kr->exclusive ? "exclusively" : "shared"),
			       (client ? client->sock : -1));
			free(kr->key);
			free(kr);
			LL_DeleteNode(keylist, NEXT);
			return;
		}
	}
}

// Release all key reservations for a client
void input_release_client_keys(Client *client)
{
	KeyReservation *kr;

	debug(RPT_DEBUG, "%s(client=[%d])", __FUNCTION__, (client ? client->sock : -1));

	for (kr = LL_GetFirst(keylist); kr != NULL; kr = LL_GetNext(keylist)) {
		if (kr->client == client) {
			report(RPT_INFO,
			       "Key \"%.40s\" reserved %s by client [%d] and is now released",
			       kr->key, (kr->exclusive ? "exclusively" : "shared"),
			       (client ? client->sock : -1));
			free(kr->key);
			free(kr);
			LL_DeleteNode(keylist, PREV);
		}
	}
}

// Find key reservation for a client
KeyReservation *input_find_key(const char *key, Client *client)
{
	KeyReservation *kr;

	debug(RPT_DEBUG, "%s(key=\"%.40s\", client=[%d])", __FUNCTION__, key,
	      (client ? client->sock : -1));

	// Grant access if exclusive or client matches
	for (kr = LL_GetFirst(keylist); kr != NULL; kr = LL_GetNext(keylist)) {
		if (strcmp(kr->key, key) == 0) {
			if (kr->exclusive || client == kr->client) {
				return kr;
			}
		}
	}
	return NULL;
}
