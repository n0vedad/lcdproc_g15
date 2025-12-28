// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/screen_commands.h
 * \brief Screen management command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - **screen_add_func()**: Create new display screens for client applications
 * - **screen_del_func()**: Remove screens and clean up associated resources
 * - **screen_set_func()**: Configure screen properties and display behavior
 * - **key_add_func()**: Bind key events to screens for user interaction
 * - **key_del_func()**: Remove key bindings from screens
 * - Dynamic screen creation and destruction during client sessions
 * - Priority-based screen scheduling and rotation management
 * - Configurable screen properties (name, size, duration, priority)
 * - Backlight and cursor control per screen
 * - Interactive key event binding and routing
 * - Screen ownership and access control validation\n * - Automatic cleanup on client disconnect\n *
 * - Widget container and layout management
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Functions are called by the command parser for screen-related commands
 * - Used for LCD display screen management in LCDd server
 * - Provides client screen lifecycle management functionality
 * - Used by protocol parser to handle screen protocol commands
 *
 * \details
 * Function declarations for screen-related protocol command handlers.
 * These functions process client commands for creating, modifying, and
 * managing display screens within the LCDd server framework.
 */

#ifndef COMMANDS_SCREEN_H
#define COMMANDS_SCREEN_H

/**
 * \brief Handle screen_add command for creating new screens.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "screen_add" commands to create new display screens
 * for client applications. Initializes screen properties and adds the
 * screen to the client's screen collection for display management.
 */
int screen_add_func(Client *c, int argc, char **argv);

/**
 * \brief Handle screen_del command for removing screens.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "screen_del" commands to remove existing screens
 * from client applications. Handles cleanup of associated widgets and
 * removes the screen from display rotation.
 */
int screen_del_func(Client *c, int argc, char **argv);

/**
 * \brief Handle screen_set command for configuring screen properties.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "screen_set" commands to modify screen properties
 * such as priority, duration, visibility, and display behavior. Updates
 * screen configuration and refreshes display scheduling.
 */
int screen_set_func(Client *c, int argc, char **argv);

/**
 * \brief Handle key_add command for binding key events to screens.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "key_add" commands to bind specific key events
 * to screen instances. Enables interactive screen control and user
 * input handling for client applications.
 */
int key_add_func(Client *c, int argc, char **argv);

/**
 * \brief Handle key_del command for removing key bindings.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "key_del" commands to remove existing key bindings
 * from screens. Disables specific key event handling and updates the
 * input routing configuration.
 */
int key_del_func(Client *c, int argc, char **argv);

#endif
