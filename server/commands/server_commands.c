// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/server_commands.c
 * \brief Implementation of server control command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - Hardware output port control for Matrix Orbital and compatible displays
 * - No-operation commands for connectivity testing and keep-alive functionality
 * - Server information and capability reporting (planned for info_func)
 * - Connection testing and protocol responsiveness verification
 * - Hardware output state management (on/off/numeric values)
 * - Error handling and parameter validation for all commands
 * - Client state verification and access control
 * - Deadlock prevention for shell scripts and automated programs
 * - Numeric output state parsing with range validation
 * - Backward compatibility with existing client implementations
 *
 * \usage
 * - Used by the LCDd server protocol parser for server command dispatch
 * - Functions are called when clients send server-related commands
 * - Provides server control and hardware management for client applications
 * - Used for output port control on compatible LCD displays
 * - Used for connection testing and synchronization in client programs
 * - Called exclusively from parse.c's command interpreter
 * - Provides detailed error reporting for invalid parameters
 *
 * \details
 * Implements handlers for client commands concerning server settings
 * and control operations. These functions process server-related protocol
 * commands and manage server-wide configuration and testing capabilities.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared/report.h"
#include "shared/sockets.h"

#include "client.h"
#include "render.h"
#include "server_commands.h"

/** \name Hardware Output Control Constants
 * Special values to enable or disable all output ports simultaneously
 */
///@{
#define ALL_OUTPUTS_ON (-1) ///< Enable all hardware output ports
#define ALL_OUTPUTS_OFF 0   ///< Disable all hardware output ports
///@}

// Handle output command for hardware output port control
int output_func(Client *c, int argc, char **argv)
{
	if (c->state != ACTIVE)
		return 1;

	if (argc != 2) {
		sock_send_error(c->sock, "Usage: output {on|off|<num>}\n");
		return 0;
	}

	if (0 == strcmp(argv[1], "on"))
		output_state = ALL_OUTPUTS_ON;
	else if (0 == strcmp(argv[1], "off"))
		output_state = ALL_OUTPUTS_OFF;

	else {
		long out;
		char *endptr;

		errno = 0;
		out = strtol(argv[1], &endptr, 0);

		if (errno) {
			// Thread-safe error message generation
			char err_buf[256];
			strerror_r(errno, err_buf, sizeof(err_buf));
			sock_printf_error(c->sock, "number argument: %s\n", err_buf);
			return 0;
		} else if ((*argv[1] != '\0') && (*endptr == '\0')) {
			output_state = out;
		} else {
			sock_send_error(c->sock, "invalid parameter...\n");
			return 0;
		}
	}

	sock_send_string(c->sock, "success\n");

	// Outputs are applied later in draw_screen()
	report(RPT_NOTICE, "output states changed");
	return 0;
}

// Handle noop command for connectivity testing
int noop_func(Client *c, int argc, char **argv)
{
	if (c->state != ACTIVE)
		return 1;

	sock_send_string(c->sock, "noop complete\n");
	return 0;
}
