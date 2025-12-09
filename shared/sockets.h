// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/sockets.h
 * \brief Socket communication functions for LCDproc clients and server
 * \author LCDproc Development Team
 * \date Various years
 *
 * \features
 * - TCP socket connection and disconnection functions
 * - Non-blocking socket I/O operations
 * - Printf-style formatted socket output
 * - String and raw data transmission
 * - Error message formatting and transmission
 * - Hostname resolution support
 * - Robust error handling with errno reporting
 *
 * \usage
 * Include this header to access socket communication functions in LCDproc
 * client applications and server components.
 *
 * \details Header file for socket communication functions used by both LCDproc
 * clients and the server. Provides TCP socket connection management,
 * data transmission, error handling, and formatted output capabilities.
 */

#ifndef SOCKETS_H
#define SOCKETS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>

/** \brief Default LCDd server port number */
#ifndef LCDPORT
#define LCDPORT 13666
#endif

/** \brief Socket shutdown mode for both read and write (fallback for old systems) */
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

/**
 * \brief Connect to server on host and port
 * \param host Hostname or IP address of the server
 * \param port Port number to connect to
 * \retval >=0 Valid socket file descriptor
 * \retval -1 Error: connection failed
 *
 * \details Creates a TCP socket and establishes a connection to the specified
 * host and port. The socket is set to non-blocking mode after connection.
 * Uses hostname resolution via gethostbyname().
 */
int sock_connect(char *host, unsigned short int port);

/**
 * \brief Disconnect from server
 * \param fd Socket file descriptor to close
 * \retval 0 Success: socket closed properly
 * \retval -1 Error: shutdown failed
 *
 * \details Properly shuts down and closes a socket connection using
 * shutdown() to gracefully close both read and write channels before
 * closing the socket file descriptor.
 */
int sock_close(int fd);

/**
 * \brief Send printf-like formatted output
 * \param fd Socket file descriptor
 * \param format Printf-style format string
 * \param ... Arguments for the format string
 * \retval >=0 Number of bytes sent
 * \retval -1 Error: formatting failed
 *
 * \details Formats a message using printf-style formatting and sends it
 * over the socket. The formatted message is limited to MAXMSG bytes.
 */
int sock_printf(int fd, const char *format, ...);

/**
 * \brief Send lines of text
 * \param fd Socket file descriptor
 * \param string Pointer to the null-terminated string to send
 * \retval >=0 Number of bytes sent
 *
 * \details Sends a null-terminated string over the socket. This is a convenience
 * wrapper around sock_send() that automatically determines the string length.
 */
int sock_send_string(int fd, const char *string);

/**
 * \brief Send raw data
 * \param fd Socket file descriptor
 * \param src Buffer holding the data to send
 * \param size Number of bytes to send
 * \retval >=0 Number of bytes sent
 * \retval -1 Error: invalid parameters
 *
 * \details Sends raw binary data over the socket. Handles partial writes
 * by sending data in a loop until all bytes are transmitted or an error occurs.
 */
int sock_send(int fd, const void *src, size_t size);

/**
 * \brief Receive a line of text
 * \param fd Socket file descriptor
 * \param dest Pointer to buffer to store the received data
 * \param maxlen Maximum number of bytes to read (size of buffer)
 * \retval >=0 Number of bytes received
 * \retval 0 No data available (EAGAIN)
 * \retval -1 Error: read failed
 *
 * \details Receives data from the socket one byte at a time until a newline,
 * null character, or maximum length is reached. The received string is
 * null-terminated. Handles EAGAIN for non-blocking sockets gracefully.
 */
int sock_recv_string(int fd, char *dest, size_t maxlen);

/**
 * \brief Receive raw data
 * \param fd Socket file descriptor
 * \param dest Pointer to buffer to store the received data
 * \param maxlen Number of bytes to read at most (size of buffer)
 * \retval >=0 Number of bytes received
 * \retval -1 Error: invalid parameters or read failed
 *
 * \details Receives raw binary data from the socket. Unlike sock_recv_string(),
 * this function reads data as-is without any string processing.
 */
int sock_recv(int fd, void *dest, size_t maxlen);

/**
 * \brief Return the error message for the last error that occurred
 * \retval !NULL Human-readable error message string
 *
 * \details Returns a human-readable error message string corresponding
 * to the current value of errno. This is a convenience wrapper around strerror().
 */
char *sock_geterror(void);

/**
 * \brief Send an already formatted error message to the client
 * \param fd Socket file descriptor
 * \param message The message to send (without the "huh? " prefix)
 * \retval >=0 Number of bytes sent
 * \retval -1 Error: send failed
 *
 * \details Sends an error message to the client with the standard "huh? "
 * prefix. This is a convenience wrapper around sock_printf_error().
 */
int sock_send_error(int fd, const char *message);

/**
 * \brief Print printf-like formatted output to logfile and send it to the client
 * \param fd Socket file descriptor
 * \param format Printf-style format string
 * \param ... Arguments for the format string
 * \retval >=0 Number of bytes sent
 * \retval -1 Error: formatting or send failed
 * \warning Do not add "huh? " to the message - this is done automatically
 *
 * \details Formats an error message using printf-style formatting, logs it
 * to the system log, and sends it to the client with the standard "huh? " prefix.
 */
int sock_printf_error(int fd, const char *format, ...);

#endif
