// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/sock.h
 * \brief Socket management interface for LCDproc server
 * \author William Ferrell
 * \author Selene Scriven
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \date 1999-2008
 *
 * \features
 * - TCP socket creation and management
 * - Client connection handling
 * - IP address validation (IPv4 and IPv6)
 * - Socket polling and event handling
 * - Client socket cleanup and destruction
 * - Network communication abstraction
 *
 * \usage
 * - Socket system initialization and configuration
 * - Client connection management and polling
 * - IP address validation for access control
 * - Socket resource cleanup and shutdown
 * - Function prototypes for socket operations
 *
 * \details Function declarations for LCDproc sockets code. Provides
 * network communication functionality for the LCDproc server including
 * client connection management, socket creation, and IP address validation.
 * Handles TCP socket creation and management, client connection handling,
 * IP address validation for both IPv4 and IPv6, socket polling and event
 * handling, and client socket cleanup and destruction.
 */

#ifndef SOCK_H
#define SOCK_H

#include "shared/sockets.h"

/** \brief Include only type definitions from client.h
 *
 * \details Prevents circular dependencies by including only struct/typedef
 * declarations from client.h, not full function prototypes.
 */
#define INC_TYPES_ONLY 1
#include "client.h"
#undef INC_TYPES_ONLY

/**
 * \brief Initializes the socket system
 * \param bind_addr IP address to bind to (NULL for any)
 * \param bind_port Port number to bind to
 * \retval 0 Success
 * \retval <0 Initialization failed
 *
 * \details Sets up the server socket and prepares for client connections.
 */
int sock_init(char *bind_addr, int bind_port);

/**
 * \brief Shuts down the socket system
 * \retval 0 Success
 * \retval <0 Shutdown failed
 *
 * \details Closes all sockets and cleans up network resources.
 */
int sock_shutdown(void);

/**
 * \brief Creates an Internet socket
 * \param bind_addr IP address to bind to
 * \param port Port number to bind to
 * \retval >=0 Socket file descriptor
 * \retval <0 Socket creation failed
 *
 * \details Creates and configures a TCP socket for client connections.
 */
int sock_create_inet_socket(char *bind_addr, unsigned int port);

/**
 * \brief Polls for client activity
 * \retval 0 Success
 * \retval <0 Polling failed
 *
 * \details Checks for new client connections and incoming data
 * from existing clients. Handles socket events and timeouts.
 */
int sock_poll_clients(void);

/**
 * \brief Destroys a client socket
 * \param client Client whose socket should be destroyed
 * \retval 0 Success
 * \retval <0 Destruction failed
 *
 * \details Closes the client's socket and cleans up associated resources.
 */
int sock_destroy_client_socket(Client *client);

/**
 * \brief Verifies IPv4 address format
 * \param addr IPv4 address string to verify
 * \retval 1 Valid IPv4 address
 * \retval 0 Invalid IPv4 address
 *
 * \details Validates that the given string is a properly formatted IPv4 address.
 */
int verify_ipv4(const char *addr);

/**
 * \brief Verifies IPv6 address format
 * \param addr IPv6 address string to verify
 * \retval 1 Valid IPv6 address
 * \retval 0 Invalid IPv6 address
 *
 * \details Validates that the given string is a properly formatted IPv6 address.
 */
int verify_ipv6(const char *addr);

#endif
