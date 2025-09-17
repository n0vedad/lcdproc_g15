// SPDX-License-Identifier: GPL-2.0+
/*
 * G15 Hardware Mock for Integration Tests
 * Bridges the unit test mocks with integration test environment
 *
 * Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
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

/* Integration test mock configuration */
#define MOCK_SOCKET_PATH "/tmp/lcdproc_g15_mock.sock"
#define MAX_CLIENTS 4
#define BUFFER_SIZE 1024

/* Mock server state */
static int server_socket = -1;
static int client_sockets[MAX_CLIENTS];
static int running = 1;

/* Mock device state (shared with unit test mock) */
extern int mock_g15_rgb_command_count;
extern int mock_g15_device_state;

/* Integration mock protocol commands */
typedef enum {
	MOCK_CMD_INIT_DEVICE = 1,
	MOCK_CMD_SET_RGB,
	MOCK_CMD_GET_STATE,
	MOCK_CMD_SIMULATE_ERROR,
	MOCK_CMD_RESET_COUNTERS,
	MOCK_CMD_SHUTDOWN
} mock_command_t;

/* Mock message structure */
typedef struct {
	mock_command_t cmd;
	int device_id;
	int param1;
	int param2;
	int param3;
	char data[256];
} mock_message_t;

/* Initialize client sockets array */
static void init_client_sockets(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		client_sockets[i] = -1;
	}
}

/* Find free client slot */
static int find_free_client_slot(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] == -1) {
			return i;
		}
	}
	return -1;
}

/* Remove client from slot */
static void remove_client(int slot)
{
	if (slot >= 0 && slot < MAX_CLIENTS && client_sockets[slot] != -1) {
		close(client_sockets[slot]);
		client_sockets[slot] = -1;
	}
}

/* Process mock command */
static int process_mock_command(const mock_message_t *msg, mock_message_t *response)
{
	memset(response, 0, sizeof(*response));
	response->cmd = msg->cmd;

	switch (msg->cmd) {
	case MOCK_CMD_INIT_DEVICE:
		/* Simulate device initialization */
		mock_g15_device_state = 1; /* Device available */
		response->param1 = 1;	   /* Success */
		snprintf(response->data,
			 sizeof(response->data),
			 "Device %d initialized",
			 msg->device_id);
		break;

	case MOCK_CMD_SET_RGB:
		/* Simulate RGB backlight command */
		mock_g15_rgb_command_count++;
		response->param1 = mock_g15_rgb_command_count;
		snprintf(response->data,
			 sizeof(response->data),
			 "RGB set to (%d,%d,%d)",
			 msg->param1,
			 msg->param2,
			 msg->param3);
		break;

	case MOCK_CMD_GET_STATE:
		/* Return current mock state */
		response->param1 = mock_g15_device_state;
		response->param2 = mock_g15_rgb_command_count;
		snprintf(response->data,
			 sizeof(response->data),
			 "State: device=%d, rgb_count=%d",
			 mock_g15_device_state,
			 mock_g15_rgb_command_count);
		break;

	case MOCK_CMD_SIMULATE_ERROR:
		/* Simulate device error */
		mock_g15_device_state = 0; /* Device error */
		response->param1 = 0;	   /* Error */
		snprintf(response->data, sizeof(response->data), "Simulated device error");
		break;

	case MOCK_CMD_RESET_COUNTERS:
		/* Reset all counters */
		mock_g15_rgb_command_count = 0;
		mock_g15_device_state = 1;
		response->param1 = 1; /* Success */
		snprintf(response->data, sizeof(response->data), "Counters reset");
		break;

	case MOCK_CMD_SHUTDOWN:
		/* Shutdown mock server */
		running = 0;
		response->param1 = 1; /* Success */
		snprintf(response->data, sizeof(response->data), "Mock server shutting down");
		break;

	default:
		response->param1 = 0; /* Error */
		snprintf(response->data, sizeof(response->data), "Unknown command: %d", msg->cmd);
		return -1;
	}

	return 0;
}

/* Handle client connection */
static void handle_client(int client_fd)
{
	mock_message_t msg, response;
	ssize_t bytes_received;

	bytes_received = recv(client_fd, &msg, sizeof(msg), 0);
	if (bytes_received == sizeof(msg)) {
		if (process_mock_command(&msg, &response) == 0) {
			send(client_fd, &response, sizeof(response), 0);
		}
	} else if (bytes_received == 0) {
		/* Client disconnected */
		printf("Integration mock: Client disconnected\n");
	} else {
		perror("recv");
	}
}

/* Main mock server loop */
static void run_mock_server(void)
{
	fd_set readfds;
	int max_fd;
	struct timeval timeout;

	printf("🎮 G15 Integration Mock Server started (socket: %s)\n", MOCK_SOCKET_PATH);

	while (running) {
		FD_ZERO(&readfds);
		FD_SET(server_socket, &readfds);
		max_fd = server_socket;

		/* Add client sockets to fd_set */
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] != -1) {
				FD_SET(client_sockets[i], &readfds);
				if (client_sockets[i] > max_fd) {
					max_fd = client_sockets[i];
				}
			}
		}

		/* Set timeout for select */
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
			/* Timeout - continue loop */
			continue;
		}

		/* Check for new connection */
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

		/* Check client sockets for data */
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &readfds)) {
				handle_client(client_sockets[i]);
				remove_client(i); /* Close connection after handling command */
			}
		}
	}
}

/* Signal handler */
static void signal_handler(int sig)
{
	(void)sig;
	running = 0;
}

/* Cleanup function */
static void cleanup_mock_server(void)
{
	/* Close all client connections */
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] != -1) {
			close(client_sockets[i]);
		}
	}

	/* Close server socket */
	if (server_socket >= 0) {
		close(server_socket);
	}

	/* Remove socket file */
	unlink(MOCK_SOCKET_PATH);

	printf("G15 Integration Mock Server cleaned up\n");
}

/* Initialize mock server */
static int init_mock_server(void)
{
	struct sockaddr_un addr;

	/* Remove existing socket file */
	unlink(MOCK_SOCKET_PATH);

	/* Create Unix domain socket */
	server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_socket < 0) {
		perror("socket");
		return -1;
	}

	/* Setup address */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, MOCK_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	/* Bind socket */
	if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(server_socket);
		return -1;
	}

	/* Listen for connections */
	if (listen(server_socket, MAX_CLIENTS) < 0) {
		perror("listen");
		close(server_socket);
		unlink(MOCK_SOCKET_PATH);
		return -1;
	}

	return 0;
}

/* Main function for standalone mock server */
int main(void)
{
	printf("🧪 G15 Hardware Integration Mock Server\n");
	printf("Bridging unit test mocks with integration tests\n");
	printf("==========================================\n");

	/* Setup signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Initialize client sockets */
	init_client_sockets();

	/* Initialize mock server */
	if (init_mock_server() < 0) {
		fprintf(stderr, "Failed to initialize mock server\n");
		return 1;
	}

	/* Run server */
	run_mock_server();

	/* Cleanup */
	cleanup_mock_server();

	return 0;
}

/* API functions for integration test control */

/* Connect to mock server */
static int connect_to_mock_server(void)
{
	int sock;
	struct sockaddr_un addr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, MOCK_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	return sock;
}

/* Send command to mock server */
int integration_mock_send_command(
    mock_command_t cmd, int device_id, int param1, int param2, int param3, const char *data)
{
	int sock = connect_to_mock_server();
	if (sock < 0) {
		return -1;
	}

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
		return response.param1; /* Return result code */
	}

	return -1;
}

/* Helper functions for common operations */
int integration_mock_init_device(int device_id)
{
	return integration_mock_send_command(MOCK_CMD_INIT_DEVICE, device_id, 0, 0, 0, NULL);
}

int integration_mock_set_rgb(int r, int g, int b)
{
	return integration_mock_send_command(MOCK_CMD_SET_RGB, 0, r, g, b, NULL);
}

int integration_mock_get_rgb_count(void)
{
	int result = integration_mock_send_command(MOCK_CMD_GET_STATE, 0, 0, 0, 0, NULL);
	return (result >= 0) ? result : 0; /* Return RGB command count or 0 on error */
}

int integration_mock_simulate_error(void)
{
	return integration_mock_send_command(MOCK_CMD_SIMULATE_ERROR, 0, 0, 0, 0, NULL);
}

int integration_mock_reset_counters(void)
{
	return integration_mock_send_command(MOCK_CMD_RESET_COUNTERS, 0, 0, 0, 0, NULL);
}

int integration_mock_shutdown_server(void)
{
	return integration_mock_send_command(MOCK_CMD_SHUTDOWN, 0, 0, 0, 0, NULL);
}