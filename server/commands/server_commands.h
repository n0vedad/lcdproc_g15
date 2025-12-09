// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/server_commands.h
 * \brief Server control command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - **output_func()**: Hardware output port control for compatible displays
 * - **noop_func()**: No-operation commands for connectivity testing
 * - **info_func()**: Server information and capability reporting
 * - Server status and capability reporting functionality
 * - Hardware output port management and control
 * - Connection testing and keep-alive functionality
 * - Protocol responsiveness verification
 * - Debug output management and control
 * - Test functionality for protocol validation
 * - Timing and synchronization utilities
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Functions are called by the command parser for server-related commands
 * - Used for server control and information retrieval in LCDd server
 * - Provides server management functionality for client applications
 * - Used by protocol parser to handle server protocol commands
 *
 * \details
 * Function declarations for server-related protocol command handlers.
 * These functions process client commands for server control, testing,
 * and information retrieval within the LCDd server framework.
 */

#ifndef COMMANDS_SERVER_H
#define COMMANDS_SERVER_H

/**
 * \brief Handle output command for debug output redirection.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "output" commands to redirect debug and diagnostic
 * output to the client connection. Enables real-time monitoring of
 * server operations and troubleshooting capabilities.
 */
int output_func(Client *c, int argc, char **argv);

/**
 * \brief Handle noop command for no-operation testing.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "noop" (no-operation) commands that perform
 * no action but confirm protocol connectivity and responsiveness.
 * Useful for keep-alive functionality and connection testing.
 */
int noop_func(Client *c, int argc, char **argv);

/**
 * \brief Handle info command for server information retrieval.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Processes "info" commands to retrieve server information
 * such as protocol version, server capabilities, display dimensions,
 * and supported features. Essential for client capability negotiation.
 *
 * \todo Implement info_func to provide server information including:
 *       - Protocol version
 *       - Server name and version
 *       - Display dimensions (width/height)
 *       - Supported command features
 *       - Driver information
 *
 * \ingroup ToDo_low
 */
int info_func(Client *c, int argc, char **argv);

#endif
