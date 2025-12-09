// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/screen_commands.c
 * \brief Implementation of screen management command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - Dynamic screen creation and destruction during client sessions
 * - Priority-based screen scheduling and rotation management
 * - Comprehensive screen property configuration (name, size, duration, priority)
 * - Backlight control per screen (on, off, toggle, blink, flash, open)
 * - Cursor control and positioning (off, on, under, block)
 * - Heartbeat indicator management (on, off, open)
 * - Key event binding and routing for user interaction
 * - Screen ownership validation and access control
 * - Detailed error reporting for invalid commands
 * - Memory management for screen resources and key buffers
 * - Screen dimension validation and boundary checking
 * - screen_add: Creates new display screens for client applications
 * - screen_del: Removes screens and cleans up associated resources
 * - screen_set: Configures screen properties and display behavior
 * - key_add/key_del: Manages key bindings for interactive screens
 *
 * \usage
 * - Used by the LCDd server protocol parser for screen command dispatch
 * - Functions are called when clients send screen-related commands
 * - Provides LCD display screen management for client applications
 * - Used for dynamic screen creation and configuration
 * - Handles screen lifecycle from creation to destruction
 * - Called exclusively from parse.c's command interpreter
 * - Validates client state and screen existence before operations
 * - Manages screen ownership and access control
 *
 * \details
 * Implements handlers for client commands concerning display screens.
 * These functions process screen-related protocol commands and manage the
 * screen lifecycle within the LCDd server framework.
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
#include "screen.h"
#include "screen_commands.h"

// Handle screen_add command for creating new screens
int screen_add_func(Client *c, int argc, char **argv)
{
	int err = 0;
	Screen *s;

	if (c->state != ACTIVE)
		return 1;

	if (argc != 2) {
		sock_send_error(c->sock, "Usage: screen_add <screenid>\n");
		return 0;
	}

	debug(RPT_DEBUG, "screen_add: Adding screen %s", argv[1]);

	s = client_find_screen(c, argv[1]);
	if (s != NULL) {
		sock_send_error(c->sock, "Screen already exists\n");
		return 0;
	}

	s = screen_create(argv[1], c);
	if (s == NULL) {
		sock_send_error(c->sock, "failed to create screen\n");
		return 0;
	}

	err = client_add_screen(c, s);

	if (err == 0) {
		sock_send_string(c->sock, "success\n");
	} else {
		sock_send_error(c->sock, "failed to add screen\n");
	}
	report(RPT_INFO, "Client on socket %d added added screen \"%s\"", c->sock, s->id);

	return 0;
}

// Handle screen_del command for removing screens
int screen_del_func(Client *c, int argc, char **argv)
{
	int err = 0;
	Screen *s;

	if (c->state != ACTIVE)
		return 1;

	if (argc != 2) {
		sock_send_error(c->sock, "Usage: screen_del <screenid>\n");
		return 0;
	}

	debug(RPT_DEBUG, "screen_del: Deleting screen %s", argv[1]);

	s = client_find_screen(c, argv[1]);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\n");
		return 0;
	}

	err = client_remove_screen(c, s);
	if (err == 0) {
		sock_send_string(c->sock, "success\n");
	} else if (err < 0) {
		sock_send_error(c->sock, "failed to remove screen\n");
	} else {
		sock_send_error(c->sock, "Unknown screen id\n");
	}

	report(RPT_INFO, "Client on socket %d removed screen \"%s\"", c->sock, s->id);
	screen_destroy(s);

	return 0;
}

// Handle screen_set command for configuring screen properties
int screen_set_func(Client *c, int argc, char **argv)
{
	int i;
	int number;
	char *id;
	Screen *s;

	if (c->state != ACTIVE)
		return 1;

	if (argc == 1) {
		sock_send_error(c->sock, "Usage: screen_set <id> [-name <name>]"
					 " [-wid <width>] [-hgt <height>] [-priority <prio>]"
					 " [-duration <int>] [-timeout <int>]"
					 " [-heartbeat <type>] [-backlight <type>]"
					 " [-cursor <type>]"
					 " [-cursor_x <xpos>] [-cursor_y <ypos>]\n");
		return 0;

	} else if (argc == 2) {
		sock_send_error(c->sock, "What do you want to set?\n");
		return 0;
	}

	id = argv[1];

	s = client_find_screen(c, id);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\n");
		return 0;
	}

	// Process all property configuration parameters
	for (i = 2; i < argc; i++) {
		char *p = argv[i];

		// Allow both "-name" and "name" parameter formats
		if (*p == '-')
			p++;

		// Configure screen display name
		if (strcmp(p, "name") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: name=\"%s\"", argv[i]);

				if (s->name != NULL)
					free(s->name);
				s->name = strdup(argv[i]);
				sock_send_string(c->sock, "success\n");
			} else {
				sock_send_error(c->sock, "-name requires a parameter\n");
			}
		}

		// Configure screen display priority for scheduling
		else if (strcmp(p, "priority") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: priority=\"%s\"", argv[i]);

				// Parse priority as numeric value first
				number = atoi(argv[i]);
				if (number > 0) {
					// Map numeric ranges to priority classes
					if (number <= 64)
						number = PRI_FOREGROUND;
					else if (number < 192)
						number = PRI_INFO;
					else
						number = PRI_BACKGROUND;

				} else {
					number = screen_pri_name_to_pri(argv[i]);
				}
				if (number >= 0) {
					s->priority = number;
					sock_send_string(c->sock, "success\n");

				} else {
					sock_send_error(c->sock, "invalid argument at -priority\n");
				}

			} else {
				sock_send_error(c->sock, "-priority requires a parameter\n");
			}
		}

		// Configure screen display duration in rotation
		else if (strcmp(p, "duration") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: duration=\"%s\"", argv[i]);

				number = atoi(argv[i]);
				if (number > 0)
					s->duration = number;
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-duration requires a parameter\n");
			}
		}

		// Configure heartbeat indicator display mode
		else if (strcmp(p, "heartbeat") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: heartbeat=\"%s\"", argv[i]);

				if (0 == strcmp(argv[i], "on"))
					s->heartbeat = HEARTBEAT_ON;
				else if (0 == strcmp(argv[i], "off"))
					s->heartbeat = HEARTBEAT_OFF;
				else if (0 == strcmp(argv[i], "open"))
					s->heartbeat = HEARTBEAT_OPEN;
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-heartbeat requires a parameter\n");
			}
		}

		// Configure screen width dimension
		else if (strcmp(p, "wid") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: wid=\"%s\"", argv[i]);

				number = atoi(argv[i]);
				if (number > 0)
					s->width = number;
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-wid requires a parameter\n");
			}

		}

		// Configure screen height dimension
		else if (strcmp(p, "hgt") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: hgt=\"%s\"", argv[i]);

				number = atoi(argv[i]);
				if (number > 0)
					s->height = number;
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-hgt requires a parameter\n");
			}
		}

		// Configure screen timeout in TIME_UNITS (1/8th second)
		else if (strcmp(p, "timeout") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: timeout=\"%s\"", argv[i]);
				number = atoi(argv[i]);

				if (number > 0) {
					s->timeout = number;
					report(RPT_NOTICE, "Timeout set.");
				}
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-timeout requires a parameter\n");
			}
		}

		// Configure screen backlight behavior
		else if (strcmp(p, "backlight") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: backlight=\"%s\"", argv[i]);

				if (strcmp("on", argv[i]) == 0)
					s->backlight = BACKLIGHT_ON;
				else if (strcmp("off", argv[i]) == 0)
					s->backlight = BACKLIGHT_OFF;

				// Toggle between on and off states only
				else if (strcmp("toggle", argv[i]) == 0) {
					if (s->backlight == BACKLIGHT_ON)
						s->backlight = BACKLIGHT_OFF;
					else if (s->backlight == BACKLIGHT_OFF)
						s->backlight = BACKLIGHT_ON;

				} else if (strcmp("blink", argv[i]) == 0)
					s->backlight |= BACKLIGHT_BLINK;
				else if (strcmp("flash", argv[i]) == 0)
					s->backlight |= BACKLIGHT_FLASH;
				else if (strcmp("open", argv[i]) == 0)
					s->backlight = BACKLIGHT_OPEN;
				else
					sock_send_error(c->sock, "unknown backlight mode\n");

				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-backlight requires a parameter\n");
			}
		}

		// Configure cursor display type
		else if (strcmp(p, "cursor") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: cursor=\"%s\"", argv[i]);

				if (0 == strcmp(argv[i], "off"))
					s->cursor = CURSOR_OFF;
				if (0 == strcmp(argv[i], "on"))
					s->cursor = CURSOR_DEFAULT_ON;
				if (0 == strcmp(argv[i], "under"))
					s->cursor = CURSOR_UNDER;
				if (0 == strcmp(argv[i], "block"))
					s->cursor = CURSOR_BLOCK;
				sock_send_string(c->sock, "success\n");

			} else {
				sock_send_error(c->sock, "-cursor requires a parameter\n");
			}
		}

		// Configure cursor horizontal position
		else if (strcmp(p, "cursor_x") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: cursor_x=\"%s\"", argv[i]);

				number = atoi(argv[i]);
				if (number > 0 && number <= s->width) {
					s->cursor_x = number;
					sock_send_string(c->sock, "success\n");

				} else {
					sock_send_error(c->sock,
							"Cursor position outside screen\n");
				}

			} else {
				sock_send_error(c->sock, "-cursor_x requires a parameter\n");
			}
		}

		// Configure cursor vertical position
		else if (strcmp(p, "cursor_y") == 0) {
			if (argc > i + 1) {
				i++;
				debug(RPT_DEBUG, "screen_set: cursor_y=\"%s\"", argv[i]);

				number = atoi(argv[i]);
				if (number > 0 && number <= s->height) {
					s->cursor_y = number;
					sock_send_string(c->sock, "success\n");

				} else {
					sock_send_error(c->sock,
							"Cursor position outside screen\n");
				}

			} else {
				sock_send_error(c->sock, "-cursor_y requires a parameter\n");
			}
		}
		// Report unrecognized parameter
		else
			sock_send_error(c->sock, "invalid parameter\n");
	}

	return 0;
}

// Handle key_add command for binding key events to screens
int key_add_func(Client *c, int argc, char **argv)
{
	Screen *s;
	int len;

	if (argc < 3) {
		sock_send_error(c->sock, "Usage: key_add screen_id {<key>}+\n");
		return 0;
	}

	s = client_find_screen(c, argv[1]);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\n");
		return 0;
	}

	// Calculate total length of key arguments to copy
	len = argv[argc - 1] - argv[2] + strlen(argv[argc - 1]) + 1;

	char *new_keys = realloc(s->keys, len + s->keys_size);
	if (new_keys == NULL) {
		sock_send_error(c->sock, "memory allocation failed\n");
		return -1;
	}
	s->keys = new_keys;
	memcpy(&s->keys[s->keys_size], argv[2], len);
	s->keys_size += len;

	sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle key_del command for removing key bindings
int key_del_func(Client *c, int argc, char **argv)
{
	Screen *s;
	int i, len;
	char *key, *p;

	if (argc < 3) {
		sock_send_error(c->sock, "Usage: key_del screen_id {<key>}+\n");
		return 0;
	}

	s = client_find_screen(c, argv[1]);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\n");
		return 0;
	}

	for (i = 2; argv[i]; ++i) {
		key = argv[i];
		p = screen_find_key(s, key);
		if (p) {
			len = strlen(key) + 1;
			// Remove key by shifting remaining buffer content
			memmove(p, p + len, s->keys_size - (p - s->keys));
			s->keys_size -= len;

			sock_send_string(c->sock, "success\n");
		} else
			sock_send_error(c->sock, "Key not requested\n");
	}

	return 0;
}
