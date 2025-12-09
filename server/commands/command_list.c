// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/command_list.c
 * \brief Implementation of client command dispatcher for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn, n0vedad
 * \date 1999-2025
 *
 * \features
 * - Master command lookup table with keyword-to-function mappings
 * - Linear search algorithm for command resolution
 * - Support for 20+ client protocol commands across multiple categories
 * - Client management commands (hello, bye, client_set, key management)
 * - Screen management commands (screen_add, screen_del, screen_set)
 * - Widget management commands (widget_add, widget_del, widget_set)
 * - Menu system commands (menu operations and navigation)
 * - Display control commands (backlight, macro_leds, output)
 * - Server utility commands (info, sleep, noop, test_func)
 * - Null-terminated command table for safe iteration
 * - Case-sensitive command keyword matching
 *
 * \usage
 * - Used by the LCDd server protocol parser for command dispatch
 * - get_command_function() is called to resolve command keywords
 * - Command table is statically defined and does not change at runtime
 * - Used by main server loop to process client protocol requests
 * - Provides central registry of all supported client commands
 *
 * \details
 * Implements the command dispatcher that maps client protocol
 * commands to their handler functions. Contains the master command lookup
 * table and provides the command resolution function used by the protocol
 * parser.
 */

#include <stdlib.h>
#include <string.h>

#include "client_commands.h"
#include "command_list.h"
#include "menu_commands.h"
#include "screen_commands.h"
#include "server_commands.h"
#include "widget_commands.h"

/** \brief Master command lookup table mapping keywords to handler functions
 *
 * \details Static array defining all supported client protocol commands.
 * Maps command keywords (strings) to their corresponding handler functions.
 * Table is null-terminated and searched linearly by get_command_function().
 * Organized by functional category for maintainability.
 */
static client_function commands[] = {
    // Development and debugging commands
    {"test_func", test_func_func},

    // Client connection management commands
    {"hello", hello_func},
    {"client_set", client_set_func},
    {"client_add_key", client_add_key_func},
    {"client_del_key", client_del_key_func},
    {"bye", bye_func},

    // Screen management commands
    {"screen_add", screen_add_func},
    {"screen_del", screen_del_func},
    {"screen_set", screen_set_func},

    // Key event management commands
    {"key_add", key_add_func},
    {"key_del", key_del_func},

    // Widget management commands
    {"widget_add", widget_add_func},
    {"widget_del", widget_del_func},
    {"widget_set", widget_set_func},

    // Menu system commands
    {"menu_add_item", menu_add_item_func},
    {"menu_del_item", menu_del_item_func},
    {"menu_set_item", menu_set_item_func},
    {"menu_goto", menu_goto_func},
    {"menu_set_main", menu_set_main_func},

    // Display and hardware control commands
    {"backlight", backlight_func},
    {"macro_leds", macro_leds_func},
    {"output", output_func},

    // Server utility commands
    {"info", info_func},
    {"noop", noop_func},

    // Terminator entry for safe iteration
    {NULL, NULL},
};

// Look up command function by keyword
CommandFunc get_command_function(char *cmd)
{
	int i;

	if (cmd == NULL)
		return NULL;

	for (i = 0; commands[i].keyword != NULL; i++) {
		if (0 == strcmp(cmd, commands[i].keyword))
			return commands[i].function;
	}

	return NULL;
}
