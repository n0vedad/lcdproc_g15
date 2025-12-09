// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/mock_g15.c
 * \brief G15 Hardware Mock for Integration Tests
 * \author n0vedad <https://github.com/n0vedad/>
 * \date 2025
 *
 * \features
 * - Unix socket-based mock server for G15 hardware simulation
 * - Support for RGB backlight command simulation
 * - Device state management and error simulation
 * - Multi-client connection handling
 * - Integration with unit test mock functions
 *
 * \usage
 * - Used by integration tests to simulate G15 hardware
 * - Start mock server before running integration tests
 * - Send commands via integration_mock_* functions
 *
 * \details Bridges the unit test mocks with integration test environment.
 * Provides a mock server that simulates G15 hardware responses for comprehensive
 * integration testing. The server runs as a Unix domain socket server, handling
 * multiple client connections and providing device simulation, RGB backlight
 * control, error injection, and state management capabilities.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "mock_hidraw_lib.h"

/** \brief Unix socket path for mock G15 server */
#define MOCK_SOCKET_PATH "/tmp/lcdproc_g15_mock.sock"
/** \brief Maximum number of concurrent client connections */
#define MAX_CLIENTS 4
/** \brief Buffer size for socket I/O operations */
#define BUFFER_SIZE 1024

// Mock server state: listening socket, connected client socket array, and server run loop control
// flag
static int server_socket = -1;
static int client_sockets[MAX_CLIENTS];
static int running = 1;

// Shared with unit test mock
extern int mock_g15_rgb_command_count;
extern int mock_g15_device_state;

// Integration mock protocol commands
typedef enum {
	MOCK_CMD_INIT_DEVICE = 1,
	MOCK_CMD_SET_RGB,
	MOCK_CMD_GET_STATE,
	MOCK_CMD_SIMULATE_ERROR,
	MOCK_CMD_RESET_COUNTERS,
	MOCK_CMD_SHUTDOWN
} mock_command_t;

// Mock message structure
typedef struct {
	mock_command_t cmd;
	int device_id;
	int param1;
	int param2;
	int param3;
	char data[256];
} mock_message_t;

// Initialize all client socket slots to invalid state
static void init_client_sockets(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		client_sockets[i] = -1;
	}
}

// Find first free client slot for new connection
static int find_free_client_slot(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] == -1) {
			return i;
		}
	}

	return -1;
}

// Close client connection and mark slot as free
static void remove_client(int slot)
{
	if (slot >= 0 && slot < MAX_CLIENTS && client_sockets[slot] != -1) {
		close(client_sockets[slot]);
		client_sockets[slot] = -1;
	}
}

// Process mock command and generate response
static int process_mock_command(const mock_message_t *msg, mock_message_t *response)
{
	memset(response, 0, sizeof(*response));
	response->cmd = msg->cmd;

	switch (msg->cmd) {

	// Set device to operational state
	case MOCK_CMD_INIT_DEVICE:
		mock_g15_device_state = 1;
		response->param1 = 1;
		snprintf(response->data, sizeof(response->data), "Device %d initialized",
			 msg->device_id);
		break;

	// Increment RGB command counter for test verification
	case MOCK_CMD_SET_RGB:
		mock_g15_rgb_command_count++;
		response->param1 = mock_g15_rgb_command_count;
		snprintf(response->data, sizeof(response->data), "RGB set to (%d,%d,%d)",
			 msg->param1, msg->param2, msg->param3);
		break;

	// Return current device and RGB counter state
	case MOCK_CMD_GET_STATE:
		response->param1 = mock_g15_device_state;
		response->param2 = mock_g15_rgb_command_count;
		snprintf(response->data, sizeof(response->data), "State: device=%d, rgb_count=%d",
			 mock_g15_device_state, mock_g15_rgb_command_count);
		break;

	// Force device into error state for error handling tests
	case MOCK_CMD_SIMULATE_ERROR:
		mock_g15_device_state = 0;
		response->param1 = 0;
		snprintf(response->data, sizeof(response->data), "Simulated device error");
		break;

	// Reset all counters and restore operational state
	case MOCK_CMD_RESET_COUNTERS:
		mock_g15_rgb_command_count = 0;
		mock_g15_device_state = 1;
		response->param1 = 1;
		snprintf(response->data, sizeof(response->data), "Counters reset");
		break;

	// Signal server to terminate main loop
	case MOCK_CMD_SHUTDOWN:
		running = 0;
		response->param1 = 1;
		snprintf(response->data, sizeof(response->data), "Mock server shutting down");
		break;

	// Handle unknown commands
	default:
		response->param1 = 0;
		snprintf(response->data, sizeof(response->data), "Unknown command: %d", msg->cmd);
		return -1;
	}

	return 0;
}

// Receive command from client and send response
static void handle_client(int client_fd)
{
	mock_message_t msg, response;
	ssize_t bytes_received;

	bytes_received = recv(client_fd, &msg, sizeof(msg), 0);

	// Process complete message
	if (bytes_received == sizeof(msg)) {
		if (process_mock_command(&msg, &response) == 0) {
			send(client_fd, &response, sizeof(response), 0);
		}
	}

	else if (bytes_received == 0) {
		printf("Integration mock: Client disconnected\n");
	} else {
		perror("recv");
	}
}

// Run main server loop using select() for multiple clients
static void run_mock_server(void)
{
	fd_set readfds;
	int max_fd;
	struct timeval timeout;

	printf("🎮 G15 Integration Mock Server started (socket: %s)\n", MOCK_SOCKET_PATH);

	// Build file descriptor set with server and all active clients
	while (running) {
		FD_ZERO(&readfds);
		FD_SET(server_socket, &readfds);
		max_fd = server_socket;

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] != -1) {
				FD_SET(client_sockets[i], &readfds);
				if (client_sockets[i] > max_fd) {
					max_fd = client_sockets[i];
				}
			}
		}

		// Use 1 second timeout to allow periodic shutdown checks
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

		if (activity < 0) {
			if (errno != EINTR) {
				perror("select");
				break;
			}
			continue;
		}

		if (activity == 0) {
			continue;
		}

		// Handle new connection on server socket
		if (FD_ISSET(server_socket, &readfds)) {
			struct sockaddr_un client_addr;
			socklen_t client_len = sizeof(client_addr);
			int client_fd =
			    accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

			if (client_fd >= 0) {
				int slot = find_free_client_slot();

				if (slot >= 0) {
					client_sockets[slot] = client_fd;
					printf("Integration mock: New client connected (slot %d)\n",
					       slot);
				} else {
					printf("Integration mock: Too many clients, rejecting "
					       "connection\n");
					close(client_fd);
				}
			}
		}

		// Handle client commands and disconnect after processing
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &readfds)) {
				handle_client(client_sockets[i]);
				remove_client(i);
			}
		}
	}
}

// Handle SIGINT and SIGTERM for graceful shutdown
static void signal_handler(int sig)
{
	(void)sig;
	running = 0;
}

// Close all connections and cleanup server resources
static void cleanup_mock_server(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] != -1) {
			close(client_sockets[i]);
		}
	}

	if (server_socket >= 0) {
		close(server_socket);
	}

	unlink(MOCK_SOCKET_PATH);
	printf("G15 Integration Mock Server cleaned up\n");
}

// Helper to create configured Unix socket address structure
static struct sockaddr_un create_unix_socket_addr(void)
{
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, MOCK_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	return addr;
}

// Initialize Unix domain socket server
static int init_mock_server(void)
{
	struct sockaddr_un addr = create_unix_socket_addr();

	unlink(MOCK_SOCKET_PATH);
	server_socket = socket(AF_UNIX, SOCK_STREAM, 0);

	// Unix domain socket setup with error handling: validate socket creation, bind to
	// filesystem path, start listening with cleanup on failure
	if (server_socket < 0) {
		perror("socket");
		return -1;
	}

	if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(server_socket);
		return -1;
	}

	if (listen(server_socket, MAX_CLIENTS) < 0) {
		perror("listen");
		close(server_socket);
		unlink(MOCK_SOCKET_PATH);
		return -1;
	}

	return 0;
}

// Main entry point for standalone mock server
int main(void)
{
	printf("🧪 G15 Hardware Integration Mock Server\n");
	printf("Bridging unit test mocks with integration tests\n");
	printf("==========================================\n");

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	init_client_sockets();

	if (init_mock_server() < 0) {
		fprintf(stderr, "Failed to initialize mock server\n");
		return 1;
	}

	run_mock_server();
	cleanup_mock_server();

	return 0;
}

// Connect to mock server via Unix socket
static int connect_to_mock_server(void)
{
	struct sockaddr_un addr = create_unix_socket_addr();
	int sock;
	sock = socket(AF_UNIX, SOCK_STREAM, 0);

	// Client connection setup: validate socket creation, connect to Unix domain socket, cleanup
	// on failure
	if (sock < 0) {
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	return sock;
}

// Send command to mock server and return response
int integration_mock_send_command(mock_command_t cmd, int device_id, int param1, int param2,
				  int param3, const char *data)
{
	int sock = connect_to_mock_server();
	if (sock < 0) {
		return -1;
	}

	// Build request message, send to mock server, receive response, and extract return value
	// from response parameter
	mock_message_t msg;
	memset(&msg, 0, sizeof(msg));
	msg.cmd = cmd;
	msg.device_id = device_id;
	msg.param1 = param1;
	msg.param2 = param2;
	msg.param3 = param3;

	if (data) {
		strncpy(msg.data, data, sizeof(msg.data) - 1);
	}

	ssize_t bytes_sent = send(sock, &msg, sizeof(msg), 0);
	if (bytes_sent != sizeof(msg)) {
		close(sock);
		return -1;
	}

	mock_message_t response;
	ssize_t bytes_received = recv(sock, &response, sizeof(response), 0);
	close(sock);

	if (bytes_received == sizeof(response)) {
		return response.param1;
	}

	return -1;
}

// Initialize mock device with given ID
int integration_mock_init_device(int device_id)
{
	return integration_mock_send_command(MOCK_CMD_INIT_DEVICE, device_id, 0, 0, 0, NULL);
}

// Set RGB backlight color on mock device
int integration_mock_set_rgb(int r, int g, int b)
{
	return integration_mock_send_command(MOCK_CMD_SET_RGB, 0, r, g, b, NULL);
}

// Get current RGB command count from mock
int integration_mock_get_rgb_count(void)
{
	int result = integration_mock_send_command(MOCK_CMD_GET_STATE, 0, 0, 0, 0, NULL);
	return (result >= 0) ? result : 0;
}

// Simulate device error condition in mock
int integration_mock_simulate_error(void)
{
	return integration_mock_send_command(MOCK_CMD_SIMULATE_ERROR, 0, 0, 0, 0, NULL);
}

// Reset all mock counters and state
int integration_mock_reset_counters(void)
{
	return integration_mock_send_command(MOCK_CMD_RESET_COUNTERS, 0, 0, 0, 0, NULL);
}

// Shutdown mock server gracefully
int integration_mock_shutdown_server(void)
{
	return integration_mock_send_command(MOCK_CMD_SHUTDOWN, 0, 0, 0, 0, NULL);
}
