// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/client_commands.c
 * \brief Implementation of client command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn, n0vedad
 * \date 1999-2025
 *
 * \features
 * - Client connection establishment (hello command)
 * - Client termination handling (bye command)
 * - Client configuration management (client_set command)
 * - Key event registration and deregistration (client_add_key, client_del_key)
 * - Display backlight control (backlight command)
 * - G15 macro LED control (macro_leds command)
 * - Driver information reporting (info command)
 * - Debug command testing (test_func_func)
 * - Socket-based client communication
 * - Client state validation and management
 * - Protocol error handling and responses
 *
 * \usage
 * - Functions are called by the command parser when clients send commands
 * - Each function processes specific client command types
 * - Used by the main LCDd server for client protocol implementation
 * - Provides standardized command processing interface
 * - Handles client state transitions and validation
 *
 * \details
 * This file implements handlers for general client commands in the
 * LCDd server. These functions process commands sent by clients through the
 * socket connection and manage client communication, configuration, and
 * interaction with the display hardware.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared/report.h"
#include "shared/sockets.h"

#include "../client.h"
#include "../drivers.h"
#include "../input.h"
#include "../render.h"
#include "client_commands.h"

// Debug function that prints received command arguments
int test_func_func(Client *c, int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++) {
		report(RPT_INFO, "%s: %i -> %s", __FUNCTION__, i, argv[i]);
		sock_printf(c->sock, "%s:  %i -> %s\n", __FUNCTION__, i, argv[i]);
	}

	return 0;
}

// Handle client hello command for initial connection
int hello_func(Client *c, int argc, char **argv)
{
	if (argc > 1) {
		sock_send_error(c->sock, "extra parameters ignored\n");
	}

	debug(RPT_INFO, "Hello!");

	// Send connection confirmation with display capabilities
	sock_printf(c->sock,
		    "connect LCDproc %s protocol %s lcd wid %i hgt %i cellwid %i cellhgt %i\n",
		    VERSION, PROTOCOL_VERSION, display_props->width, display_props->height,
		    display_props->cellwidth, display_props->cellheight);

	c->state = ACTIVE;

	return 0;
}

// Handle client bye command for connection termination
int bye_func(Client *c, int argc, char **argv)
{
	if (c != NULL) {
		debug(RPT_INFO, "Bye, %s!", (c->name != NULL) ? c->name : "unknown client");

		c->state = GONE;
		sock_send_error(c->sock, "\"bye\" is currently ignored\n");
	}

	return 0;
}

// Handle client_set command for client configuration
int client_set_func(Client *c, int argc, char **argv)
{
	int i;

	if (c->state != ACTIVE)
		return 1;

	if (argc != 3) {
		sock_send_error(c->sock, "Usage: client_set -name <name>\n");
		return 0;
	}

	i = 1;
	do {
		char *p = argv[i];

		// Allow both -name and name parameter formats
		if (*p == '-')
			p++;

		if (strcmp(p, "name") == 0) {
			i++;
			if (argv[i] == NULL) {
				sock_printf_error(c->sock, "internal error: no parameter #%d\n", i);
				continue;
			}

			debug(RPT_DEBUG, "client_set: name=\"%s\"", argv[i]);

			if (c->name != NULL)
				free(c->name);

			if ((c->name = strdup(argv[i])) == NULL) {
				sock_send_error(c->sock, "error allocating memory!\n");
			} else {
				sock_send_string(c->sock, "success\n");
				i++;
			}
		} else {
			sock_printf_error(c->sock, "invalid parameter (%s)\n", p);
		}
	} while (++i < argc);

	return 0;
}

// Handle client_add_key command for key event registration
int client_add_key_func(Client *c, int argc, char **argv)
{
	int exclusively = 0;
	int argnr;

	if (c->state != ACTIVE)
		return 1;

	if (argc < 2) {
		sock_send_error(c->sock, "Usage: client_add_key [-exclusively|-shared] {<key>}+\n");
		return 0;
	}

	argnr = 1;
	if (argv[argnr][0] == '-') {
		if (strcmp(argv[argnr], "-shared") == 0) {
			exclusively = 0;
		} else if (strcmp(argv[argnr], "-exclusively") == 0) {
			exclusively = 1;
		} else {
			sock_printf_error(c->sock, "Invalid option: %s\n", argv[argnr]);
		}
		argnr++;
	}

	for (; argnr < argc; argnr++)
		if (input_reserve_key(argv[argnr], exclusively, c) < 0)
			sock_printf_error(c->sock, "Could not reserve key \"%s\"\n", argv[argnr]);
		else
			sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle client_del_key command for key event deregistration
int client_del_key_func(Client *c, int argc, char **argv)
{
	int argnr;

	if (c->state != ACTIVE)
		return 1;

	if (argc < 2) {
		sock_send_error(c->sock, "Usage: client_del_key {<key>}+\n");
		return 0;
	}

	for (argnr = 1; argnr < argc; argnr++) {
		input_release_key(argv[argnr], c);
	}
	sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle backlight command for display backlight control
int backlight_func(Client *c, int argc, char **argv)
{
	if (c->state != ACTIVE)
		return 1;

	if (argc != 2) {
		sock_send_error(c->sock, "Usage: backlight {on|off|toggle|blink|flash}\n");
		return 0;
	}

	debug(RPT_DEBUG, "backlight(%s)", argv[1]);

	if (strcmp("on", argv[1]) == 0) {
		c->backlight = BACKLIGHT_ON;
	} else if (strcmp("off", argv[1]) == 0) {
		c->backlight = BACKLIGHT_OFF;
	} else if (strcmp("toggle", argv[1]) == 0) {
		if (c->backlight == BACKLIGHT_ON)
			c->backlight = BACKLIGHT_OFF;
		else if (c->backlight == BACKLIGHT_OFF)
			c->backlight = BACKLIGHT_ON;
	} else if (strcmp("blink", argv[1]) == 0) {
		c->backlight |= BACKLIGHT_BLINK;
	} else if (strcmp("flash", argv[1]) == 0) {
		c->backlight |= BACKLIGHT_FLASH;
	}

	sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle macro_leds command for G15 macro LED control
int macro_leds_func(Client *c, int argc, char **argv)
{
	if (c->state != ACTIVE)
		return 1;

	if (argc != 5) {
		sock_send_error(c->sock, "Usage: macro_leds <m1> <m2> <m3> <mr>\n");
		return 0;
	}

	debug(RPT_DEBUG, "macro_leds(%s %s %s %s)", argv[1], argv[2], argv[3], argv[4]);

	// Parse LED states from string arguments to binary values
	int m1 = (strcmp("1", argv[1]) == 0) ? 1 : 0;
	int m2 = (strcmp("1", argv[2]) == 0) ? 1 : 0;
	int m3 = (strcmp("1", argv[3]) == 0) ? 1 : 0;
	int mr = (strcmp("1", argv[4]) == 0) ? 1 : 0;

	if (drivers_set_macro_leds(m1, m2, m3, mr) == 0) {
		sock_send_string(c->sock, "success\n");
	} else {
		sock_send_error(c->sock, "Failed to set macro LEDs\n");
	}

	return 0;
}

// Handle info command to report driver information
int info_func(Client *c, int argc, char **argv)
{
	if (c->state != ACTIVE)
		return 1;

	if (argc > 1) {
		sock_send_error(c->sock, "Extra arguments ignored...\n");
	}

	sock_printf(c->sock, "%s\n", drivers_get_info());

	return 0;
}
