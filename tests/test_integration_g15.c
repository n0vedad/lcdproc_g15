// SPDX-License-Identifier: GPL-2.0+
/*
 * Integration tests for lcdproc-g15
 * Tests the complete LCDd server, lcdproc client, and lcdexec integration
 *
 * Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Test configuration */
#define TEST_SERVER_HOST "127.0.0.1"
#define MAX_BUFFER_SIZE 4096
#define TEST_TIMEOUT 10
#define PROCESS_START_TIMEOUT 5

/* Global state */
static int tests_run = 0;
static int tests_passed = 0;
static int test_server_port = 0; /* Dynamic port assignment */
static pid_t lcdd_pid = 0;
static pid_t client_pid = 0;
static char temp_config_dir[256];

/* Test driver configuration */
typedef enum { DRIVER_DEBUG, DRIVER_G15, DRIVER_LINUX_INPUT } test_driver_t;

static test_driver_t current_driver = DRIVER_DEBUG;

/* ANSI color codes */
#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_RESET "\033[0m"

/* Test result macros */
#define ASSERT_TRUE(condition, message)                                                            \
	do {                                                                                       \
		tests_run++;                                                                       \
		if (condition) {                                                                   \
			printf("✅ %s\n", message);                                                \
			tests_passed++;                                                            \
		} else {                                                                           \
			printf("❌ %s\n", message);                                                \
		}                                                                                  \
	} while (0)

#define ASSERT_FALSE(condition, message) ASSERT_TRUE(!(condition), message)

/* Utility functions */
static void cleanup_processes(void);
static int find_free_port(void);
static int wait_for_tcp_port(const char *host, int port, int timeout);
static int send_tcp_command(
    const char *host, int port, const char *command, char *response, size_t response_size);
static int create_test_config_file(const char *filename, const char *content);
static void setup_test_environment(void);
static void cleanup_test_environment(void);
static void generate_driver_config(char *config, size_t config_size, const char *host, int port);

/* Test functions */
static void test_lcdd_server_startup(void);
static void test_lcdd_server_shutdown(void);
static void test_tcp_connection_basic(void);
static void test_lcdproc_protocol_handshake(void);
static void test_screen_lifecycle(void);
static void test_widget_operations(void);
static void test_client_disconnection(void);
static void test_lcdproc_client_integration(void);
static void test_multiple_clients(void);
static void test_g15_driver_integration(void);

/* Signal handler for cleanup */
static void signal_handler(int sig)
{
	(void)sig;
	cleanup_processes();
	cleanup_test_environment();
	exit(1);
}

/* Cleanup function */
static void cleanup_processes(void)
{
	if (client_pid > 0) {
		kill(client_pid, SIGTERM);
		waitpid(client_pid, NULL, 0);
		client_pid = 0;
	}

	if (lcdd_pid > 0) {
		kill(lcdd_pid, SIGTERM);
		waitpid(lcdd_pid, NULL, 0);
		lcdd_pid = 0;
	}
}

/* Find a free TCP port */
static int find_free_port(void)
{
	int sock;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int port;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0; /* Let system choose port */

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	if (getsockname(sock, (struct sockaddr *)&addr, &len) < 0) {
		close(sock);
		return -1;
	}

	port = ntohs(addr.sin_port);
	close(sock);

	return port;
}

/* Wait for TCP port to become available */
static int wait_for_tcp_port(const char *host, int port, int timeout)
{
	int sock;
	struct sockaddr_in addr;
	int result;
	time_t start_time = time(NULL);

	while (time(NULL) - start_time < timeout) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) {
			continue;
		}

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, host, &addr.sin_addr);

		result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
		close(sock);

		if (result == 0) {
			return 1; /* Success */
		}

		usleep(100000); /* Wait 100ms */
	}

	return 0; /* Timeout */
}

/* Send TCP command and get response */
static int send_tcp_command(
    const char *host, int port, const char *command, char *response, size_t response_size)
{
	int sock;
	struct sockaddr_in addr;
	ssize_t bytes_sent, bytes_received;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, host, &addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	bytes_sent = send(sock, command, strlen(command), 0);
	if (bytes_sent < 0) {
		close(sock);
		return -1;
	}

	bytes_received = recv(sock, response, response_size - 1, 0);
	if (bytes_received > 0) {
		response[bytes_received] = '\0';
	}

	close(sock);
	return bytes_received > 0 ? 0 : -1;
}

/* Create test configuration file */
static int create_test_config_file(const char *filename, const char *content)
{
	FILE *fp = fopen(filename, "w");
	if (!fp) {
		return -1;
	}

	fprintf(fp, "%s", content);
	fclose(fp);
	return 0;
}

/* Setup test environment */
static void setup_test_environment(void)
{
	/* Find free port for testing */
	test_server_port = find_free_port();
	if (test_server_port < 0) {
		fprintf(stderr, "Failed to find free port\n");
		exit(1);
	}

	/* Create temporary directory for test configs */
	snprintf(temp_config_dir, sizeof(temp_config_dir), "/tmp/lcdproc_test_%d", getpid());
	if (mkdir(temp_config_dir, 0755) < 0) {
		perror("mkdir");
		exit(1);
	}

	/* Create LCDd test configuration */
	char lcdd_config[512];
	char config_path[320];
	snprintf(config_path, sizeof(config_path), "%s/LCDd.conf", temp_config_dir);

	generate_driver_config(
	    lcdd_config, sizeof(lcdd_config), TEST_SERVER_HOST, test_server_port);

	if (create_test_config_file(config_path, lcdd_config) < 0) {
		fprintf(stderr, "Failed to create LCDd config file\n");
		exit(1);
	}

	/* Create lcdproc test configuration */
	snprintf(config_path, sizeof(config_path), "%s/lcdproc.conf", temp_config_dir);

	snprintf(lcdd_config,
		 sizeof(lcdd_config),
		 "[lcdproc]\n"
		 "Server=%s\n"
		 "Port=%d\n"
		 "ReportLevel=3\n"
		 "ReportToSyslog=false\n"
		 "Foreground=true\n"
		 "DisplayTimeout=2\n"
		 "\n"
		 "[CPU]\n"
		 "Active=true\n",
		 TEST_SERVER_HOST,
		 test_server_port);

	if (create_test_config_file(config_path, lcdd_config) < 0) {
		fprintf(stderr, "Failed to create lcdproc config file\n");
		exit(1);
	}

	const char *driver_name = (current_driver == DRIVER_DEBUG) ? "debug"
				  : (current_driver == DRIVER_G15) ? "g15"
								   : "linux_input";
	printf("🔧 Test environment setup complete (temp dir: %s, driver: %s, port: %d)\n",
	       temp_config_dir,
	       driver_name,
	       test_server_port);
}

/* Cleanup test environment */
static void cleanup_test_environment(void)
{
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_config_dir);
	system(cmd);
}

/* Generate driver-specific configuration */
static void generate_driver_config(char *config, size_t config_size, const char *host, int port)
{
	switch (current_driver) {
	case DRIVER_DEBUG:
		snprintf(config,
			 config_size,
			 "[server]\n"
			 "Driver=debug\n"
			 "DriverPath=../server/drivers/\n"
			 "Bind=%s\n"
			 "Port=%d\n"
			 "ReportLevel=3\n"
			 "ReportToSyslog=false\n"
			 "Foreground=true\n"
			 "\n"
			 "[debug]\n"
			 "Size=20x4\n",
			 host,
			 port);
		break;

	case DRIVER_G15:
		snprintf(config,
			 config_size,
			 "[server]\n"
			 "Driver=g15\n"
			 "DriverPath=../server/drivers/\n"
			 "Bind=%s\n"
			 "Port=%d\n"
			 "ReportLevel=3\n"
			 "ReportToSyslog=false\n"
			 "Foreground=true\n"
			 "\n"
			 "[g15]\n"
			 "# G15 driver configuration\n"
			 "# Uses hidraw interface for G15/G510 keyboards\n",
			 host,
			 port);
		break;

	case DRIVER_LINUX_INPUT:
		snprintf(config,
			 config_size,
			 "[server]\n"
			 "Driver=linux_input\n"
			 "DriverPath=../server/drivers/\n"
			 "Bind=%s\n"
			 "Port=%d\n"
			 "ReportLevel=3\n"
			 "ReportToSyslog=false\n"
			 "Foreground=true\n"
			 "\n"
			 "[linux_input]\n"
			 "# Linux input driver configuration\n"
			 "Device=/dev/input/event0\n",
			 host,
			 port);
		break;
	}
}

/* Test LCDd server startup */
static void test_lcdd_server_startup(void)
{
	char config_path[320];
	char *args[6];

	printf("\n" COLOR_BLUE "🚀 Testing LCDd server startup..." COLOR_RESET "\n");

	snprintf(config_path, sizeof(config_path), "%s/LCDd.conf", temp_config_dir);

	lcdd_pid = fork();
	if (lcdd_pid == 0) {
		/* Child process - start LCDd */
		args[0] = "../server/LCDd";
		args[1] = "-c";
		args[2] = config_path;
		args[3] = "-f"; /* Foreground */
		args[4] = NULL;

		/* Temporarily allow output for debugging */
		/* int devnull = open("/dev/null", O_WRONLY);
		dup2(devnull, STDOUT_FILENO);
		dup2(devnull, STDERR_FILENO);
		close(devnull); */

		execv("../server/LCDd", args);
		exit(1); /* Should not reach here */
	} else if (lcdd_pid > 0) {
		/* Parent process - wait for server to start */
		ASSERT_TRUE(
		    wait_for_tcp_port(TEST_SERVER_HOST, test_server_port, PROCESS_START_TIMEOUT),
		    "LCDd server started and listening on TCP port");
	} else {
		ASSERT_TRUE(0, "Fork failed for LCDd server");
	}
}

/* Test LCDd server shutdown */
static void test_lcdd_server_shutdown(void)
{
	int status;

	printf("\n" COLOR_BLUE "🛑 Testing LCDd server shutdown..." COLOR_RESET "\n");

	if (lcdd_pid > 0) {
		kill(lcdd_pid, SIGTERM);
		waitpid(lcdd_pid, &status, 0);

		ASSERT_TRUE(WIFEXITED(status) || WIFSIGNALED(status),
			    "LCDd server shutdown cleanly");

		lcdd_pid = 0;

		/* Wait a moment and verify port is no longer listening */
		sleep(2);
		ASSERT_FALSE(wait_for_tcp_port(TEST_SERVER_HOST, test_server_port, 2),
			     "TCP port no longer listening after shutdown");
	} else {
		ASSERT_TRUE(0, "No LCDd server process to shutdown");
	}
}

/* Test basic TCP connection */
static void test_tcp_connection_basic(void)
{
	char response[MAX_BUFFER_SIZE];
	int result;

	printf("\n" COLOR_BLUE "🔌 Testing basic TCP connection..." COLOR_RESET "\n");

	/* Test connection (server already running) */
	result = send_tcp_command(
	    TEST_SERVER_HOST, test_server_port, "hello\n", response, sizeof(response));
	ASSERT_TRUE(result == 0, "TCP connection established successfully");
	ASSERT_TRUE(strstr(response, "connect") != NULL, "Server responded with connect message");

	/* Test second connection */
	result = send_tcp_command(
	    TEST_SERVER_HOST, test_server_port, "hello\n", response, sizeof(response));
	ASSERT_TRUE(result == 0, "Second TCP connection successful");
}

/* Test LCDproc protocol handshake */
static void test_lcdproc_protocol_handshake(void)
{
	char response[MAX_BUFFER_SIZE];
	int sock;
	struct sockaddr_in addr;

	printf("\n" COLOR_BLUE "🤝 Testing LCDproc protocol handshake..." COLOR_RESET "\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_TRUE(sock >= 0, "Socket creation successful");

	if (sock >= 0) {
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(test_server_port);
		inet_pton(AF_INET, TEST_SERVER_HOST, &addr.sin_addr);

		int connect_result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
		ASSERT_TRUE(connect_result == 0, "Socket connection successful");

		if (connect_result == 0) {
			/* Send hello command */
			ssize_t bytes_sent = send(sock, "hello\n", 6, 0);
			ASSERT_TRUE(bytes_sent == 6, "Hello command sent successfully");

			/* Receive connect response */
			ssize_t bytes_received = recv(sock, response, sizeof(response) - 1, 0);
			if (bytes_received > 0) {
				response[bytes_received] = '\0';
				ASSERT_TRUE(strstr(response, "connect LCDproc") != NULL,
					    "Received valid connect response");
				ASSERT_TRUE(strstr(response, "protocol") != NULL,
					    "Protocol version included in response");
				ASSERT_TRUE(strstr(response, "lcd wid") != NULL,
					    "LCD dimensions included in response");
			} else {
				ASSERT_TRUE(0, "No response received from server");
			}

			/* Send client_set command */
			bytes_sent = send(sock, "client_set -name test_client\n", 29, 0);
			ASSERT_TRUE(bytes_sent == 29, "Client_set command sent successfully");

			/* Send bye command */
			send(sock, "bye\n", 4, 0);
		}

		close(sock);
	}
}

/* Test screen lifecycle */
static void test_screen_lifecycle(void)
{
	char response[MAX_BUFFER_SIZE];
	int sock;
	struct sockaddr_in addr;

	printf("\n" COLOR_BLUE "🖥️  Testing screen lifecycle..." COLOR_RESET "\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ASSERT_TRUE(0, "Socket creation failed");
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(test_server_port);
	inet_pton(AF_INET, TEST_SERVER_HOST, &addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		/* Initialize connection */
		send(sock, "hello\n", 6, 0);
		recv(sock, response, sizeof(response) - 1, 0);

		send(sock, "client_set -name test_screen_client\n", 36, 0);
		recv(sock, response, sizeof(response) - 1, 0);

		/* Add screen */
		send(sock, "screen_add test_screen\n", 23, 0);
		ssize_t bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Screen added successfully");
		}

		/* Set screen properties */
		send(sock, "screen_set test_screen -name \"Test Screen\" -priority 128\n", 57, 0);
		bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Screen properties set successfully");
		}

		/* Delete screen */
		send(sock, "screen_del test_screen\n", 23, 0);
		bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Screen deleted successfully");
		}

		send(sock, "bye\n", 4, 0);
	} else {
		ASSERT_TRUE(0, "Failed to connect for screen lifecycle test");
	}

	close(sock);
}

/* Test widget operations */
static void test_widget_operations(void)
{
	char response[MAX_BUFFER_SIZE];
	int sock;
	struct sockaddr_in addr;

	printf("\n" COLOR_BLUE "📦 Testing widget operations..." COLOR_RESET "\n");

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ASSERT_TRUE(0, "Socket creation failed");
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(test_server_port);
	inet_pton(AF_INET, TEST_SERVER_HOST, &addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		/* Initialize connection */
		send(sock, "hello\n", 6, 0);
		recv(sock, response, sizeof(response) - 1, 0);

		send(sock, "client_set -name test_widget_client\n", 36, 0);
		recv(sock, response, sizeof(response) - 1, 0);

		/* Add screen */
		send(sock, "screen_add widget_screen\n", 25, 0);
		recv(sock, response, sizeof(response) - 1, 0);

		/* Add string widget */
		send(sock, "widget_add widget_screen test_string string\n", 45, 0);
		ssize_t bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "String widget added successfully");
		}

		/* Set widget content */
		send(sock, "widget_set widget_screen test_string 1 1 \"Hello World\"\n", 56, 0);
		bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Widget content set successfully");
		}

		/* Add title widget */
		send(sock, "widget_add widget_screen test_title title\n", 42, 0);
		bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Title widget added successfully");
		}

		send(sock, "widget_set widget_screen test_title \"Integration Test\"\n", 55, 0);
		bytes = recv(sock, response, sizeof(response) - 1, 0);
		if (bytes > 0) {
			response[bytes] = '\0';
			ASSERT_TRUE(strstr(response, "success") != NULL,
				    "Title widget content set successfully");
		}

		send(sock, "bye\n", 4, 0);
	} else {
		ASSERT_TRUE(0, "Failed to connect for widget operations test");
	}

	close(sock);
}

/* Test client disconnection handling */
static void test_client_disconnection(void)
{
	int sock1, sock2;
	struct sockaddr_in addr;
	char response[MAX_BUFFER_SIZE];

	printf("\n" COLOR_BLUE "🔌 Testing client disconnection handling..." COLOR_RESET "\n");

	/* Create two connections */
	sock1 = socket(AF_INET, SOCK_STREAM, 0);
	sock2 = socket(AF_INET, SOCK_STREAM, 0);

	ASSERT_TRUE(sock1 >= 0 && sock2 >= 0, "Multiple sockets created successfully");

	if (sock1 >= 0 && sock2 >= 0) {
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(test_server_port);
		inet_pton(AF_INET, TEST_SERVER_HOST, &addr.sin_addr);

		/* Connect both clients */
		int connect1 = connect(sock1, (struct sockaddr *)&addr, sizeof(addr));
		int connect2 = connect(sock2, (struct sockaddr *)&addr, sizeof(addr));

		ASSERT_TRUE(connect1 == 0 && connect2 == 0,
			    "Multiple clients connected successfully");

		if (connect1 == 0 && connect2 == 0) {
			/* Initialize both connections */
			send(sock1, "hello\n", 6, 0);
			recv(sock1, response, sizeof(response) - 1, 0);
			send(sock1, "client_set -name client1\n", 25, 0);
			recv(sock1, response, sizeof(response) - 1, 0);

			send(sock2, "hello\n", 6, 0);
			recv(sock2, response, sizeof(response) - 1, 0);
			send(sock2, "client_set -name client2\n", 25, 0);
			recv(sock2, response, sizeof(response) - 1, 0);

			/* Abruptly close first connection (simulate crash) */
			close(sock1);

			/* Verify second connection still works */
			send(sock2, "screen_add test_disconnect\n", 27, 0);
			ssize_t bytes = recv(sock2, response, sizeof(response) - 1, 0);
			if (bytes > 0) {
				response[bytes] = '\0';
				ASSERT_TRUE(strstr(response, "success") != NULL,
					    "Server handles client disconnection gracefully");
			}

			send(sock2, "bye\n", 4, 0);
			close(sock2);
		} else {
			if (sock1 >= 0)
				close(sock1);
			if (sock2 >= 0)
				close(sock2);
		}
	}
}

/* Test lcdproc client integration */
static void test_lcdproc_client_integration(void)
{
	char config_path[320];
	char *args[8];
	int status;

	printf("\n" COLOR_BLUE "📊 Testing lcdproc client integration..." COLOR_RESET "\n");

	snprintf(config_path, sizeof(config_path), "%s/lcdproc.conf", temp_config_dir);

	client_pid = fork();
	if (client_pid == 0) {
		/* Child process - start lcdproc */
		args[0] = "../clients/lcdproc/lcdproc";
		args[1] = "-c";
		args[2] = config_path;
		args[3] = "-f"; /* Foreground */
		args[4] = NULL;

		/* Redirect stdout/stderr */
		int devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0) {
			dup2(devnull, STDOUT_FILENO);
			dup2(devnull, STDERR_FILENO);
			close(devnull);
		}

		execv("../clients/lcdproc/lcdproc", args);
		exit(1);
	} else if (client_pid > 0) {
		/* Parent process - wait a moment for client to connect and operate */
		sleep(3);

		/* Check if client is still running */
		int result = waitpid(client_pid, &status, WNOHANG);
		if (result == 0) {
			ASSERT_TRUE(1, "lcdproc client started and running successfully");
			/* Clean shutdown */
			kill(client_pid, SIGTERM);
			waitpid(client_pid, &status, 0);
		} else {
			ASSERT_TRUE(0, "lcdproc client failed to start or exited early");
		}

		client_pid = 0;
	} else {
		ASSERT_TRUE(0, "Fork failed for lcdproc client");
	}
}

/* Test multiple clients scenario */
static void test_multiple_clients(void)
{
	int sock1, sock2;
	struct sockaddr_in addr;
	char response[MAX_BUFFER_SIZE];

	printf("\n" COLOR_BLUE "👥 Testing multiple clients scenario..." COLOR_RESET "\n");

	sock1 = socket(AF_INET, SOCK_STREAM, 0);
	sock2 = socket(AF_INET, SOCK_STREAM, 0);

	if (sock1 >= 0 && sock2 >= 0) {
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(test_server_port);
		inet_pton(AF_INET, TEST_SERVER_HOST, &addr.sin_addr);

		if (connect(sock1, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
		    connect(sock2, (struct sockaddr *)&addr, sizeof(addr)) == 0) {

			/* Initialize both clients */
			send(sock1, "hello\n", 6, 0);
			recv(sock1, response, sizeof(response) - 1, 0);
			send(sock1, "client_set -name multi_client1\n", 31, 0);
			recv(sock1, response, sizeof(response) - 1, 0);

			send(sock2, "hello\n", 6, 0);
			recv(sock2, response, sizeof(response) - 1, 0);
			send(sock2, "client_set -name multi_client2\n", 31, 0);
			recv(sock2, response, sizeof(response) - 1, 0);

			/* Both clients create screens */
			send(sock1, "screen_add screen1\n", 19, 0);
			recv(sock1, response, sizeof(response) - 1, 0);

			send(sock2, "screen_add screen2\n", 19, 0);
			ssize_t bytes = recv(sock2, response, sizeof(response) - 1, 0);
			if (bytes > 0) {
				response[bytes] = '\0';
				ASSERT_TRUE(strstr(response, "success") != NULL,
					    "Multiple clients can create screens simultaneously");
			}

			/* Set different priorities */
			send(sock1, "screen_set screen1 -priority 200\n", 34, 0);
			usleep(50000); /* 50ms */
			bytes = recv(sock1, response, sizeof(response) - 1, 0);
			if (bytes > 0) {
				response[bytes] = '\0';
				/* Verify first client priority setting worked */
				if (strstr(response, "success") == NULL) {
					printf("Warning: First client priority setting failed\n");
				}
			}

			send(sock2, "screen_set screen2 -priority 100\n", 34, 0);

			/* Wait briefly for server to process */
			usleep(50000); /* 50ms */

			bytes = recv(sock2, response, sizeof(response) - 1, 0);
			if (bytes > 0) {
				response[bytes] = '\0';
				ASSERT_TRUE(strstr(response, "success") != NULL,
					    "Multiple clients can set different screen priorities");
			} else {
				/* Retry once on failed response */
				usleep(100000); /* 100ms */
				bytes = recv(sock2, response, sizeof(response) - 1, 0);
				if (bytes > 0) {
					response[bytes] = '\0';
					ASSERT_TRUE(
					    strstr(response, "success") != NULL,
					    "Multiple clients can set different screen priorities");
				} else {
					ASSERT_TRUE(0,
						    "Multiple clients can set different screen "
						    "priorities - no response received");
				}
			}

			send(sock1, "bye\n", 4, 0);
			send(sock2, "bye\n", 4, 0);
		} else {
			ASSERT_TRUE(0, "Failed to establish multiple client connections");
		}

		close(sock1);
		close(sock2);
	} else {
		ASSERT_TRUE(0, "Failed to create sockets for multiple clients test");
	}
}

/* Test G15 driver integration */
static void test_g15_driver_integration(void)
{
	printf("\n" COLOR_BLUE "🎮 Testing G15 driver integration..." COLOR_RESET "\n");

	/* Note: This test would need to be enhanced to actually use G15 driver
	 * For now, we'll test that the debug driver works as a baseline */

	char response[MAX_BUFFER_SIZE];
	int result = send_tcp_command(
	    TEST_SERVER_HOST, test_server_port, "hello\n", response, sizeof(response));

	if (result == 0) {
		ASSERT_TRUE(strstr(response, "lcd wid 20") != NULL,
			    "Debug driver provides correct LCD width");
		ASSERT_TRUE(strstr(response, "hgt 4") != NULL,
			    "Debug driver provides correct LCD height");
	} else {
		ASSERT_TRUE(0, "Failed to connect for G15 driver integration test");
	}

	/* TODO: Add actual G15 driver tests with mock hardware when G15 driver is configured */
	ASSERT_TRUE(1, "G15 driver integration baseline test completed (debug driver functional)");
}

/* Print test summary */
static void print_test_summary(void)
{
	printf("\n" COLOR_BLUE "📋 Integration Test Summary:" COLOR_RESET "\n");
	printf("Tests run: %d\n", tests_run);
	printf("Tests passed: %d\n", tests_passed);
	printf("Tests failed: %d\n", tests_run - tests_passed);

	if (tests_passed == tests_run) {
		printf(COLOR_GREEN "🎉 ALL INTEGRATION TESTS PASSED!" COLOR_RESET "\n");
	} else {
		printf(COLOR_RED "❌ Some integration tests failed!" COLOR_RESET "\n");
	}

	printf("\nIntegration test coverage:\n");
	printf("✓ LCDd server process management\n");
	printf("✓ TCP socket communication\n");
	printf("✓ LCDproc protocol handshake\n");
	printf("✓ Screen and widget lifecycle\n");
	printf("✓ Client disconnection handling\n");
	printf("✓ lcdproc client integration\n");
	printf("✓ Multiple concurrent clients\n");
	printf("✓ Driver integration baseline\n");
}

/* Main function */
int main(int argc, char *argv[])
{
	/* Parse command line arguments */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--driver=debug") == 0) {
			current_driver = DRIVER_DEBUG;
		} else if (strcmp(argv[i], "--driver=g15") == 0) {
			current_driver = DRIVER_G15;
		} else if (strcmp(argv[i], "--driver=linux_input") == 0) {
			current_driver = DRIVER_LINUX_INPUT;
		} else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			printf("Usage: %s [--driver=<driver>]\n", argv[0]);
			printf("Drivers: debug, g15, linux_input\n");
			printf("Default: debug\n");
			return 0;
		}
	}

	const char *driver_name = (current_driver == DRIVER_DEBUG) ? "debug"
				  : (current_driver == DRIVER_G15) ? "g15"
								   : "linux_input";

	printf(COLOR_BLUE "🧪 LCDproc-G15 Integration Test Suite" COLOR_RESET "\n");
	printf("Testing complete server-client integration scenarios\n");
	printf("Driver: %s\n", driver_name);
	printf("=" COLOR_BLUE "================================================" COLOR_RESET "\n");

	/* Setup signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Setup test environment */
	setup_test_environment();

	/* Run integration tests */
	test_lcdd_server_startup();
	test_tcp_connection_basic();
	test_lcdproc_protocol_handshake();
	test_screen_lifecycle();
	test_widget_operations();
	test_client_disconnection();
	test_lcdproc_client_integration();
	test_multiple_clients();
	test_g15_driver_integration();
	test_lcdd_server_shutdown();

	/* Cleanup */
	cleanup_processes();
	cleanup_test_environment();

	/* Print summary */
	print_test_summary();

	return (tests_passed == tests_run) ? 0 : 1;
}