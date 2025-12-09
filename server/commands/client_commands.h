// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/client_commands.h
 * \brief Client command function declarations for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn, n0vedad
 * \date 1999-2025
 *
 * \features
 * - **hello_func()**: Client connection establishment and protocol negotiation
 * - **bye_func()**: Clean client connection termination
 * - **client_set_func()**: Client name and configuration management
 * - **client_add_key_func()**: Key event registration for clients
 * - **client_del_key_func()**: Key event deregistration
 * - **backlight_func()**: Display backlight state control
 * - **macro_leds_func()**: G15 keyboard macro LED control
 * - **info_func()**: Driver information reporting
 * - Standard command interface with argc/argv parameters
 * - Socket-based response handling
 * - Client state validation
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Functions are called by the command parser for client requests
 * - All functions receive Client context and parsed command arguments
 * - Used for client-server protocol implementation
 * - Provides standardized command handling interface
 *
 * \details
 * Header file containing function declarations for client command
 * processing in the LCDd server. These functions handle communication
 * between clients and the server, including connection management, client
 * configuration, key management, and display control.
 *
 * **Supported client commands:**
 * - Connection management (hello, bye, info)
 * - Client configuration (client_set)
 * - Key event handling (client_add_key, client_del_key)
 * - Display control (backlight)
 * - G15 macro LED control (macro_leds)
 *
 * **Protocol specification:**
 * Each command function receives a Client pointer and parsed command
 * arguments. Functions return success/error codes and may send responses
 * back to the client through the socket connection.
 */

#ifndef COMMANDS_CLIENT_H
#define COMMANDS_CLIENT_H

#include "../client.h"

/**
 * \brief Handle client hello command for initial connection.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 *
 * \details Processes the initial "hello" command from a client to establish
 * the connection and negotiate protocol parameters. Sends back connection
 * confirmation with display capabilities and protocol version information.
 */
int hello_func(Client *c, int argc, char **argv);

/**
 * \brief Handle client bye command for connection termination.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes the "bye" command from a client to cleanly terminate
 * the connection. Performs cleanup of client resources and closes the
 * socket connection.
 */
int bye_func(Client *c, int argc, char **argv);

/**
 * \brief Handle client_set command for client configuration.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "client_set" commands to configure client properties
 * such as name, heartbeat settings, and other client-specific options.
 * Updates the client configuration and stores settings in the client context.
 */
int client_set_func(Client *c, int argc, char **argv);

/**
 * \brief Handle client_add_key command for key event registration.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "client_add_key" commands to register the client
 * for specific key events. When registered keys are pressed, the server
 * will send key events to this client. Supports various key types including
 * function keys, G-keys, and mode keys.
 */
int client_add_key_func(Client *c, int argc, char **argv);

/**
 * \brief Handle client_del_key command for key event deregistration.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "client_del_key" commands to unregister the client
 * from specific key events. After deregistration, the client will no
 * longer receive events for the specified keys.
 */
int client_del_key_func(Client *c, int argc, char **argv);

/**
 * \brief Handle backlight command for display backlight control.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "backlight" commands to control the display backlight
 * state. Supports turning the backlight on/off and setting brightness levels
 * depending on driver capabilities.
 */
int backlight_func(Client *c, int argc, char **argv);

/**
 * \brief Handle macro_leds command for G15 macro LED control.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "macro_leds" commands to control the macro LED
 * indicators on Logitech G15 keyboards. Sets the state of M1, M2, M3,
 * and MR LEDs to indicate current macro mode and recording status.
 */
int macro_leds_func(Client *c, int argc, char **argv);

/**
 * \brief Handle test_func command for debugging.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 *
 * \details Debug function that prints received command arguments to both
 * the debug log and the client socket. Used for testing command parsing
 * and argument handling during development.
 */
int test_func_func(Client *c, int argc, char **argv);

#endif
