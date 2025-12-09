// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/sock.c
 * \brief Socket management implementation for LCDproc server
 * \author William Ferrell
 * \author Selene Scriven
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \author Markus Dolze
 * \date 1999-2009
 *
 * \features
 * - TCP socket creation and binding
 * - Client connection acceptance and management
 * - Non-blocking socket I/O with select()
 * - Message buffering with ring buffer
 * - Socket-to-client mapping management
 * - IPv4 and IPv6 address validation
 * - Socket resource pooling for efficiency
 *
 * \usage
 * - Server socket initialization and configuration
 * - Client connection polling and management
 * - Socket resource allocation and cleanup
 * - Message reading and buffering operations
 * - IP address validation utilities
 *
 * \details This file contains all the sockets code used by the server. This contains
 * the code called upon by main() to initialize the listening socket, as well
 * as code to deal with sending messages to clients, maintaining connections,
 * accepting new connections, closing dead connections (or connections
 * associated with dying/exiting clients), etc. Uses pre-allocated socket mapping
 * pool to avoid heap operations, maintains active socket list for select() polling,
 * ring buffer for incoming message assembly, non-blocking I/O prevents server blocking
 * on slow clients, and automatic client cleanup on socket errors.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "shared/defines.h"
#include "shared/report.h"
#include "shared/sring.h"

#include "clients.h"
#include "sock.h"

/** \name Global Socket Management State
 * File descriptor sets, listening socket, and connection tracking
 */
///@{
static fd_set active_fd_set;			///< Set of all active file descriptors for select()
static fd_set read_fd_set;			///< Working set for select() read operations
static int listening_fd;			///< Listening socket file descriptor
static LinkedList *openSocketList = NULL;	///< List of active ClientSocketMap objects
static LinkedList *freeClientSocketList = NULL; ///< List of unused ClientSocketMap objects
static sring_buffer *messageRing;		///< Ring buffer for queued outgoing messages
///@}

/**
 * \brief Socket to client mapping structure
 * \details Associates socket file descriptors with client objects for connection management
 */
typedef struct _ClientSocketMap {
	int socket;	///< Socket file descriptor
	Client *client; ///< Associated client object
} ClientSocketMap;

/** \brief Pre-allocated pool of socket mapping structures
 *
 * \details Array of FD_SETSIZE ClientSocketMap entries allocated at startup.
 * Provides recycled mapping objects to avoid malloc/free overhead.
 */
ClientSocketMap *freeClientSocketPool;

/** \brief Maximum message length for socket transmission
 *
 * \details Length of longest single transmission allowed. Messages exceeding
 * this size are truncated or split.
 */
#define MAXMSG 8192

// Internal socket I/O and cleanup function declarations
static int sock_read_from_client(ClientSocketMap *clientSocketMap);
static void sock_destroy_socket(void);

// Initialize socket system and prepare listening socket with resource pools
int sock_init(char *bind_addr, int bind_port)
{
	int i;

	debug(RPT_DEBUG, "%s(bind_addr=\"%s\", port=%d)", __FUNCTION__, bind_addr, bind_port);
	listening_fd = sock_create_inet_socket(bind_addr, bind_port);

	// Socket initialization with resource pools: allocate client socket pool, create socket
	// lists, initialize listening socket, and create message ring buffer
	if (listening_fd < 0) {
		report(RPT_ERR, "%s: error creating socket - %s", __FUNCTION__, sock_geterror());
		return -1;
	}

	freeClientSocketPool = (ClientSocketMap *)calloc(FD_SETSIZE, sizeof(ClientSocketMap));

	if (freeClientSocketPool == NULL) {
		report(RPT_ERR, "%s: Error allocating client sockets.", __FUNCTION__);
		return -1;
	}

	freeClientSocketList = LL_new();

	if (freeClientSocketList == NULL) {
		report(RPT_ERR, "%s: error allocating free socket list.", __FUNCTION__);
		return -1;
	}

	for (i = 0; i < FD_SETSIZE; ++i) {
		LL_AddNode(freeClientSocketList, (void *)&freeClientSocketPool[i]);
	}

	openSocketList = LL_new();

	if (openSocketList == NULL) {
		report(RPT_ERR, "%s: error allocating open socket list.", __FUNCTION__);
		return -1;

	} else {
		ClientSocketMap *entry;

		entry = (ClientSocketMap *)LL_Pop(freeClientSocketList);

		entry->socket = listening_fd;
		entry->client = NULL;

		LL_AddNode(openSocketList, (void *)entry);
	}

	if ((messageRing = sring_create(MAXMSG)) == NULL) {
		report(RPT_ERR, "%s: error allocating receive buffer.", __FUNCTION__);
		return -1;
	}

	return 0;
}

// Clean up all socket resources and close listening socket
int sock_shutdown(void)
{
	int retVal = 0;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	close(listening_fd);
	LL_Destroy(freeClientSocketList);
	free(freeClientSocketPool);
	sring_destroy(messageRing);

	return retVal;
}

// Create TCP socket, bind to address and port, and start listening
int sock_create_inet_socket(char *addr, unsigned int port)
{
	struct sockaddr_in name;
	int sock;
	int sockopt = 1;

	debug(RPT_DEBUG, "%s(addr=\"%s\", port=%i)", __FUNCTION__, addr, port);
	sock = socket(PF_INET, SOCK_STREAM, 0);

	// TCP socket setup sequence: validate socket creation, enable address reuse, configure bind
	// address/port, start listening, and initialize fd_set for select()
	if (sock < 0) {
		report(RPT_ERR, "%s: cannot create socket - %s", __FUNCTION__, sock_geterror());
		return -1;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0) {
		report(RPT_ERR, "%s: error setting socket option SO_REUSEADDR - %s", __FUNCTION__,
		       sock_geterror());
		return -1;
	}

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	inet_aton(addr, &name.sin_addr);

	if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		report(RPT_ERR, "%s: cannot bind to port %d at address %s - %s", __FUNCTION__, port,
		       addr, sock_geterror());
		return -1;
	}

	if (listen(sock, 1) < 0) {
		report(RPT_ERR,
		       "%s: error in attempting to listen to port "
		       "%d at %s - %s",
		       __FUNCTION__, port, addr, sock_geterror());
		return -1;
	}

	report(RPT_NOTICE, "Listening for queries on %s:%d", addr, port);

	FD_ZERO(&active_fd_set);
	FD_SET(sock, &active_fd_set);

	return sock;
}

// Poll all sockets for new connections and incoming data using select()
int sock_poll_clients(void)
{
	struct timeval t;
	ClientSocketMap *clientSocket;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	t.tv_sec = 0;
	t.tv_usec = 0;
	read_fd_set = active_fd_set;

	if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, &t) < 0) {
		report(RPT_ERR, "%s: Select error - %s", __FUNCTION__, sock_geterror());
		return -1;
	}

	LL_Rewind(openSocketList);

	// Process all ready sockets: accept new connections on listening socket, read data from
	// client sockets, cleanup on errors
	for (clientSocket = (ClientSocketMap *)LL_Get(openSocketList); clientSocket != NULL;
	     clientSocket = LL_GetNext(openSocketList)) {

		if (FD_ISSET(clientSocket->socket, &read_fd_set)) {
			if (clientSocket->socket == listening_fd) {
				Client *c;
				int new_sock;
				struct sockaddr_in clientname;
				socklen_t size = sizeof(clientname);

				new_sock =
				    accept(listening_fd, (struct sockaddr *)&clientname, &size);

				if (new_sock < 0) {
					report(RPT_ERR, "%s: Accept error - %s", __FUNCTION__,
					       sock_geterror());
					return -1;
				}

				// Thread-safe IP address conversion
				char client_addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(clientname.sin_addr), client_addr,
					  INET_ADDRSTRLEN);
				report(RPT_NOTICE, "Connect from host %s:%hu on socket %i",
				       client_addr, ntohs(clientname.sin_port), new_sock);
				FD_SET(new_sock, &active_fd_set);

				fcntl(new_sock, F_SETFL, O_NONBLOCK);

				// Clear the message ring buffer for new client connection
				report(RPT_NOTICE,
				       "Clearing message ring buffer (before: r=%d, w=%d)",
				       messageRing->r, messageRing->w);
				sring_clear(messageRing);
				report(RPT_NOTICE,
				       "Message ring buffer cleared (after: r=%d, w=%d)",
				       messageRing->r, messageRing->w);

				if ((c = client_create(new_sock)) == NULL) {
					report(RPT_ERR,
					       "%s: Error creating client on socket %i - %s",
					       __FUNCTION__, clientSocket->socket, sock_geterror());
					return -1;

				} else {
					ClientSocketMap *newClientSocket;
					newClientSocket =
					    (ClientSocketMap *)LL_Pop(freeClientSocketList);

					if (newClientSocket != NULL) {
						newClientSocket->socket = new_sock;
						newClientSocket->client = c;
						LL_InsertNode(openSocketList,
							      (void *)newClientSocket);

						LL_Next(openSocketList);

					} else {
						report(RPT_ERR,
						       "%s: Error - free client socket list "
						       "exhausted - %d clients.",
						       __FUNCTION__, FD_SETSIZE);
						return -1;
					}
				}

				if (clients_add_client(c) == NULL) {
					report(RPT_ERR, "%s: Could not add client on socket %i",
					       __FUNCTION__, clientSocket->socket);
					return -1;
				}

			} else {
				int err = 0;

				debug(RPT_DEBUG, "%s: reading...", __FUNCTION__);
				err = sock_read_from_client(clientSocket);
				debug(RPT_DEBUG, "%s: ...done", __FUNCTION__);
				if (err < 0)
					sock_destroy_socket();
			}
		}
	}

	return 0;
}

/**
 * \brief Sock read from client
 * \param clientSocketMap ClientSocketMap *clientSocketMap
 * \return Return value
 */
static int sock_read_from_client(ClientSocketMap *clientSocketMap)
{
	char buffer[MAXMSG];
	int nbytes;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	errno = 0;
	nbytes = sock_recv(clientSocketMap->socket, buffer, MAXMSG);

	// Read loop: buffer incoming data to ring, extract complete newline-delimited messages,
	// dispatch to client, and handle buffer overflow with EAGAIN retry
	while (nbytes > 0) {
		int fr;
		char *str;

		debug(RPT_DEBUG, "%s: received %4d bytes", __FUNCTION__, nbytes);

		sring_write(messageRing, buffer, nbytes);

		do {
			str = sring_read_string(messageRing);
			if (clientSocketMap->client) {
				client_add_message(clientSocketMap->client, str);
			} else {
				report(RPT_DEBUG, "%s: Can't find client %d", __FUNCTION__,
				       clientSocketMap->socket);
			}
		} while (str != NULL);

		fr = sring_getMaxWrite(messageRing);
		if (fr <= 0) {
			report(RPT_WARNING, "%s: Message buffer full or invalid (fr=%d)",
			       __FUNCTION__, fr);
			// Buffer is full, stop reading from socket but don't disconnect
			nbytes = 0;
		} else {
			nbytes =
			    sock_recv(clientSocketMap->socket, buffer, min(MAXMSG, (size_t)fr));
		}
	}

	if (sring_getMaxRead(messageRing) > 0) {
		report(RPT_WARNING, "%s: left over bytes in message buffer", __FUNCTION__);
		sring_clear(messageRing);
	}

	if (nbytes < 0 && errno == EAGAIN) {
		return 0;
	}

	return -1;
}

/**
 * \brief Byclient
 * \param csm void *csm
 * \param client void *client
 * \return Return value
 */
int byClient(void *csm, void *client)
{
	return (((ClientSocketMap *)csm)->client == (Client *)client) ? 0 : -1;
}

// Find and destroy socket for given client
int sock_destroy_client_socket(Client *client)
{
	ClientSocketMap *entry;

	LL_Rewind(openSocketList);
	entry = LL_Find(openSocketList, byClient, client);

	if (entry != NULL) {
		sock_destroy_socket();
		return 0;
	}

	return -1;
}

/**
 * \brief Close socket and clean up client resources
 *
 * \details Destroys client, removes from client list, closes socket,
 * and frees socket map entry.
 */
static void sock_destroy_socket(void)
{
	ClientSocketMap *entry = LL_Get(openSocketList);

	if (entry != NULL) {
		if (entry->client != NULL) {
			report(RPT_NOTICE, "Client on socket %i disconnected", entry->socket);
			client_destroy(entry->client);
			clients_remove_client(entry->client, PREV);
			entry->client = NULL;

		} else {
			report(RPT_ERR, "%s: Can't find client of socket %i", __FUNCTION__,
			       entry->socket);
		}

		FD_CLR(entry->socket, &active_fd_set);
		close(entry->socket);

		entry = (ClientSocketMap *)LL_DeleteNode(openSocketList, PREV);
		LL_Push(freeClientSocketList, (void *)entry);
	}
}

// Validate IPv4 address string format using inet_pton()
int verify_ipv4(const char *addr)
{
	int result = -1;

	if (addr != NULL) {
		struct in_addr a;

		result = inet_pton(AF_INET, addr, &a);
	}

	return (result > 0) ? 1 : 0;
}

// Validate IPv6 address string format using inet_pton()
int verify_ipv6(const char *addr)
{
	int result = 0;

	if (addr != NULL) {
		struct in6_addr a;

		result = inet_pton(AF_INET6, addr, &a);
	}

	return (result > 0) ? 1 : 0;
}
