// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/sockets.c
 * \brief Socket communication functions for LCDproc clients and server
 * \author LCDproc Development Team
 * \date Various years
 *
 * \features
 * - TCP socket connection and disconnection
 * - Non-blocking socket I/O operations
 * - Printf-style formatted socket output
 * - String and raw data transmission
 * - Error message formatting and transmission
 * - Hostname resolution support
 * - Robust error handling with errno reporting
 *
 * \usage
 * - Use sock_connect() to establish connections to LCDd server
 * - Use sock_printf() for formatted message transmission
 * - Use sock_recv_string() for receiving line-based responses
 * - Use sock_close() to properly terminate connections
 * - Check return values for error handling
 *
 * \details This file provides a comprehensive set of socket communication functions
 * that are used by both LCDproc clients and the server. It includes functions for
 * connection management, data transmission, error handling, and formatted output.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "report.h"
#include "sockets.h"

/** \brief Maximum message size for socket communication (8KB) */
#define MAXMSG 8192

/** \brief Socket address structure typedef
 *
 * \details Shorthand typedef for struct sockaddr_in to reduce verbosity.
 * Used throughout socket-related functions.
 */
typedef struct sockaddr_in sockaddr_in;

/**
 * \brief Sock init sockaddr
 * \param name sockaddr_in *name
 * \param hostname const char *hostname
 * \param port unsigned short int port
 * \return Return value
 */
static int sock_init_sockaddr(sockaddr_in *name, const char *hostname, unsigned short int port)
{
	struct hostent *hostinfo;

	memset(name, '\0', sizeof(*name));
	name->sin_family = AF_INET;
	name->sin_port = htons(port);
	hostinfo = gethostbyname(hostname);

	if (hostinfo == NULL) {
		report(RPT_ERR, "sock_init_sockaddr: Unknown host %s.", hostname);
		return -1;
	}

	name->sin_addr = *(struct in_addr *)hostinfo->h_addr_list[0];

	return 0;
}

// Connect to server on specified host and port
int sock_connect(char *host, unsigned short int port)
{
	struct sockaddr_in servername;
	int sock;
	int err = 0;

	report(RPT_INFO, "sock_connect: Creating socket");
	sock = socket(PF_INET, SOCK_STREAM, 0);

	if (sock < 0) {
		report(RPT_ERR, "sock_connect: Error creating socket");
		return sock;
	}

	report(RPT_INFO, "sock_connect: Created socket (%i)", sock);

	if (sock_init_sockaddr(&servername, host, port) < 0)
		return -1;

	err = connect(sock, (struct sockaddr *)&servername, sizeof(servername));

	if (err < 0) {
		report(RPT_ERR, "sock_connect: connect failed");
		shutdown(sock, SHUT_RDWR);
		return -1;
	}

	// Set non-blocking mode for async I/O
	fcntl(sock, F_SETFL, O_NONBLOCK);

	return sock;
}

// Disconnect from server and close socket
int sock_close(int fd)
{
	int err;

	err = shutdown(fd, SHUT_RDWR);

	if (!err)
		close(fd);

	return err;
}

// Send printf-like formatted output over socket
int sock_printf(int fd, const char *format, ...)
{
	char buf[MAXMSG];
	va_list ap;
	int size = 0;

	va_start(ap, format);
	size = vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	if (size < 0) {
		report(RPT_ERR, "sock_printf: vsnprintf failed");
		return -1;
	}

	if (size > sizeof(buf))
		report(RPT_WARNING, "sock_printf: vsnprintf truncated message");

	return sock_send_string(fd, buf);
}

// Send null-terminated string over socket
int sock_send_string(int fd, const char *string) { return sock_send(fd, string, strlen(string)); }

// Receive a line of text from socket
int sock_recv_string(int fd, char *dest, size_t maxlen)
{
	char *ptr = dest;
	int recvBytes = 0;

	if (!dest)
		return -1;

	if (maxlen <= 0)
		return 0;

	// Read byte-by-byte until delimiter or buffer full
	while (1) {
		int err = read(fd, ptr, 1);

		// Non-blocking: continue if partially read, else return 0
		if (err == -1) {
			if (errno == EAGAIN) {
				if (recvBytes) {
					continue;
				}
				return 0;
			} else {
				report(RPT_ERR, "sock_recv_string: socket read error");
				return err;
			}
		} else if (err == 0) {
			return recvBytes;
		}

		recvBytes++;

		// Stop at buffer limit, NUL, or newline
		if (recvBytes == maxlen || *ptr == '\0' || *ptr == '\n') {
			*ptr = '\0';
			break;
		}
		ptr++;
	}

	// Filter out single NUL byte (empty string)
	if (recvBytes == 1 && dest[0] == '\0')
		return 0;

	if (recvBytes < maxlen - 1)
		dest[recvBytes] = '\0';

	return recvBytes;
}

// Send raw data over socket
int sock_send(int fd, const void *src, size_t size)
{
	int offset = 0;

	if (!src)
		return -1;

	// Loop to handle partial writes
	while (offset != size) {
		int sent = write(fd, ((char *)src) + offset, size - offset);

		if (sent == -1) {
			if (errno != EAGAIN) {
				report(RPT_ERR, "sock_send: socket write error");
				report(RPT_DEBUG, "Message was: '%.*s'", size - offset,
				       (char *)src);
				return sent;
			}

			// Retry on EAGAIN
			continue;
		} else if (sent == 0) {
			return sent + offset;
		}

		offset += sent;
	}

	return offset;
}

// Receive raw data from socket
int sock_recv(int fd, void *dest, size_t maxlen)
{
	int err;

	report(RPT_DEBUG, "sock_recv: fd=%d, dest=%p, maxlen=%zu", fd, dest, maxlen);

	if (!dest) {
		report(RPT_ERR, "sock_recv: dest is NULL!");
		return -1;
	}

	if (maxlen <= 0) {
		report(RPT_WARNING, "sock_recv: maxlen=%zu is invalid!", maxlen);
		return 0;
	}

	err = read(fd, dest, maxlen);

	if (err < 0) {
		// EAGAIN/EWOULDBLOCK are NOT errors for non-blocking sockets - just means no data
		// available
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			// Thread-safe error message generation
			char err_buf[256];
			strerror_r(errno, err_buf, sizeof(err_buf));
			report(RPT_ERR, "sock_recv: socket read error: %s (errno=%d)", err_buf,
			       errno);
		}
		return err;
	}

	debug(RPT_DEBUG, "sock_recv: Got message \"%s\"", (char *)dest);
	return err;
}

// Return the error message for the last error that occurred
// Thread-safe implementation using thread-local storage
char *sock_geterror(void)
{
	static _Thread_local char err_buf[256];
	strerror_r(errno, err_buf, sizeof(err_buf));
	return err_buf;
}

// Send an already formatted error message to the client
int sock_send_error(int fd, const char *message) { return sock_printf_error(fd, "%s", message); }

// Print printf-like formatted output to logfile and send it to the client
int sock_printf_error(int fd, const char *format, ...)
{
	static const char huh[] = "huh? ";
	char buf[MAXMSG];
	va_list ap;
	int size = 0;

	strncpy(buf, huh, sizeof(huh));

	// Format variable arguments into buffer after "huh? " prefix and ensure null-termination
	va_start(ap, format);
	size = vsnprintf(buf + (sizeof(huh) - 1), sizeof(buf) - (sizeof(huh) - 1), format, ap);
	buf[sizeof(buf) - 1] = '\0';
	va_end(ap);

	if (size < 0) {
		report(RPT_ERR, "sock_printf_error: vsnprintf failed");
		return -1;
	}

	if (size >= sizeof(buf) - (sizeof(huh) - 1))
		report(RPT_WARNING, "sock_printf_error: vsnprintf truncated message");

	report(RPT_WARNING, "client error: %s", buf);

	return sock_send_string(fd, buf);
}
