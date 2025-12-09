// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/command_list.h
 * \brief Client command dispatcher declarations and data structures
 * \author William Ferrell, Selene Scriven, n0vedad
 * \date 1999-2025
 *
 * \features
 * - **CommandFunc**: Function pointer type for standardized command handlers
 * - **client_function**: Structure mapping command keywords to handler functions
 * - **get_command_function()**: Command lookup and dispatch function
 * - Linear search command table with null terminator
 * - Case-sensitive command keyword matching
 * - Standardized function signature for all command handlers
 * - Support for multiple command categories (client, screen, widget, menu, etc.)
 * - Error handling for invalid or missing commands
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Used by protocol parser to dispatch client commands to handlers
 * - Command table is populated in command_list.c implementation
 * - All command handlers must follow the CommandFunc signature
 * - Used for client-server protocol command routing
 *
 * \details
 * Header file for the client command dispatch system in LCDd server.
 * Defines the command lookup table structure and provides function declarations
 * for finding and executing client commands based on protocol keywords.
 */

#ifndef COMMANDS_COMMAND_LIST_H
#define COMMANDS_COMMAND_LIST_H

#include "client.h"

/**
 * \brief Function pointer type for client command handlers.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval non-zero Error code
 *
 * \details Standard signature for all client command handler functions.
 * The function list for clients is stored in a table, and each entry
 * points to a function following this signature.
 */
typedef int (*CommandFunc)(Client *c, int argc, char **argv);

/**
 * \brief Structure defining an entry in the command lookup table.
 *
 * \details Each entry maps a protocol command keyword to its corresponding
 * handler function. The command dispatcher uses this table to find and
 * execute the appropriate function when a client sends a command.
 */
typedef struct client_function {
	char *keyword;	      // Command string in the protocol
	CommandFunc function; // Pointer to the associated handler function
} client_function;

/**
 * \brief Look up a command function by keyword.
 * \param cmd Command keyword string to search for
 * \retval function Pointer to command handler function
 * \retval NULL Command not found
 *
 * \details Searches the command table for the specified keyword and
 * returns the associated function pointer. Used by the command parser
 * to dispatch commands to their handlers.
 */
CommandFunc get_command_function(char *cmd);

#endif
