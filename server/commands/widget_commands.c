// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/widget_commands.c
 * \brief Widget command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - Widget creation with support for 8+ widget types (string, hbar, vbar, pbar, icon, title,
 * scroller, frame, num)
 * - Optional container placement using "-in" flag for frame widget hierarchies
 * - Widget deletion with automatic memory cleanup and resource management
 * - Comprehensive widget configuration with type-specific parameter validation
 * - Coordinate-based positioning with 1-based coordinate system
 * - Progress bar configuration with 0-1000 promille value range and optional labels
 * - Icon name to icon mapping for status indicators and symbols
 * - Scrolling text widgets with direction (h/v/m) and speed control
 * - Frame widgets for hierarchical widget containers and sub-screens
 * - Numeric display widgets for large number display
 * - String widgets with dynamic text content and positioning
 * - Error handling and detailed parameter validation for all widget types
 * - Memory management for widget text content, labels, and dynamic data
 *
 * \usage
 * - Used by the LCDd server protocol parser for widget command dispatch
 * - Functions are called when clients send widget-related commands
 * - Provides LCD display widget management for client applications
 * - Used for dynamic widget creation, configuration, and removal
 * - Handles widget lifecycle from creation to destruction
 *
 * \details
 * Implementation of handlers for client commands concerning widget
 * management. This file contains definitions for all widget-related functions
 * that clients can invoke through the LCDd protocol. These functions
 * handle widget creation, deletion, and configuration.
 *
 * The functions in this file are called exclusively from the command
 * parser in parse.c. Each function corresponds to a specific widget
 * command available to clients and defines the syntax and behavior
 * for that command.
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
#include "drivers.h"
#include "screen.h"
#include "widget.h"
#include "widget_commands.h"

/**
 * \brief Not direction
 * \param c char c
 * \return Return value
 */
static int not_direction(char c) { return c != 'h' && c != 'v'; }

// Add a widget to a screen
int widget_add_func(Client *c, int argc, char **argv)
{
	int err;
	char *sid;
	char *wid;
	WidgetType wtype;
	Screen *s;
	Widget *w;

	if (c->state != ACTIVE)
		return 1;

	if ((argc < 4) || (argc > 6)) {
		sock_send_error(
		    c->sock, "Usage: widget_add <screenid> <widgetid> <widgettype> [-in <id>]\\n");
		return 0;
	}

	sid = argv[1];
	wid = argv[2];

	s = client_find_screen(c, sid);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\\n");
		return 0;
	}

	wtype = widget_typename_to_type(argv[3]);
	if (wtype == WID_NONE) {
		sock_send_error(c->sock, "Invalid widget type\\n");
		return 0;
	}

	// Process optional container placement
	if (argc > 4) {
		char *p = argv[4];

		// Allow both "-in" and "in" parameter formats
		if (*p == '-')
			p++;

		if (strcmp(p, "in") == 0) {
			Widget *frame;

			if (argc < 6) {
				sock_send_error(c->sock, "Specify a frame to place widget in\\n");
				return 0;
			}

			// Replace target screen with frame's internal screen
			frame = screen_find_widget(s, argv[5]);
			if (frame == NULL) {
				sock_send_error(c->sock, "Error finding frame\\n");
				return 0;
			}
			s = frame->frame_screen;
		}
	}

	w = widget_create(wid, wtype, s);
	if (w == NULL) {
		sock_send_error(c->sock, "Error adding widget\\n");
		return 0;
	}

	err = screen_add_widget(s, w);
	if (err == 0)
		sock_send_string(c->sock, "success\\n");
	else
		sock_send_error(c->sock, "Error adding widget\\n");

	return 0;
}

// Remove a widget from a screen
int widget_del_func(Client *c, int argc, char **argv)
{
	int err = 0;
	char *sid;
	char *wid;
	Screen *s;
	Widget *w;

	if (c->state != ACTIVE)
		return 1;

	if (argc != 3) {
		sock_send_error(c->sock, "Usage: widget_del <screenid> <widgetid>\\n");
		return 0;
	}

	sid = argv[1];
	wid = argv[2];

	debug(RPT_DEBUG, "widget_del: Deleting widget %s.%s", sid, wid);

	s = client_find_screen(c, sid);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\\n");
		return 0;
	}

	w = screen_find_widget(s, wid);
	if (w == NULL) {
		sock_send_error(c->sock, "Unknown widget id\\n");
		return 0;
	}

	err = screen_remove_widget(s, w);
	if (err == 0)
		sock_send_string(c->sock, "success\\n");
	else
		sock_send_error(c->sock, "Error removing widget\\n");

	return 0;
}

// Configure widget properties
int widget_set_func(Client *c, int argc, char **argv)
{
	int i;
	char *wid;
	char *sid;
	Screen *s;
	Widget *w;

	if (c->state != ACTIVE)
		return 1;

	if (argc < 4) {
		sock_send_error(
		    c->sock, "Usage: widget_set <screenid> <widgetid> <widget-SPECIFIC-data>\\n");
		return 0;
	}

	sid = argv[1];
	s = client_find_screen(c, sid);
	if (s == NULL) {
		sock_send_error(c->sock, "Unknown screen id\\n");
		return 0;
	}

	wid = argv[2];
	w = screen_find_widget(s, wid);

	// Debug output for troubleshooting widget lookup failures
	if (w == NULL) {
		sock_send_error(c->sock, "Unknown widget id\\n");
		{
			int j;

			report(RPT_WARNING, "Unknown widget id (%s)", argv[2]);
			for (j = 0; j < argc; j++)
				report(RPT_WARNING, "    %.40s", argv[j]);
		}
		return 0;
	}

	i = 3;

	// Configure widget based on its type
	switch (w->type) {

	// String widgets: x, y coordinates and text content
	case WID_STRING:
		if (argc != i + 3) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		w->x = atoi(argv[i]);
		w->y = atoi(argv[i + 1]);
		free(w->text);
		w->text = strdup(argv[i + 2]);
		debug(RPT_DEBUG, "Widget %s set to %s", wid, w->text);

		break;

	// Horizontal and vertical bar widgets: x, y coordinates and length value
	case WID_HBAR:
	case WID_VBAR:
		if (argc != i + 3) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		w->x = atoi(argv[i]);
		w->y = atoi(argv[i + 1]);
		w->length = atoi(argv[i + 2]);

		debug(RPT_DEBUG, "Widget %s set to %i", wid, w->length);

		break;

	// Progress bar widgets: x, y, width, promille and optional labels
	case WID_PBAR:
		if (argc < i + 4 || argc > i + 6) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		free(w->begin_label);
		free(w->end_label);
		w->begin_label = NULL;
		w->end_label = NULL;

		w->x = atoi(argv[i]);
		w->y = atoi(argv[i + 1]);
		w->width = atoi(argv[i + 2]);
		w->promille = atoi(argv[i + 3]);

		if (argc >= i + 5)
			w->begin_label = strdup(argv[i + 4]);
		if (argc >= i + 6)
			w->end_label = strdup(argv[i + 5]);

		debug(RPT_DEBUG, "Widget %s set to %i", wid, w->promille);

		break;

	// Icon widgets: x, y coordinates and icon name
	case WID_ICON: {
		int icon;

		if (argc != i + 3) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		icon = widget_iconname_to_icon(argv[i + 2]);
		if (icon == -1) {
			sock_send_error(c->sock, "Invalid icon name\\n");
			return 0;
		}

		w->x = atoi(argv[i]);
		w->y = atoi(argv[i + 1]);
		w->length = icon;

		break;
	}

	// Title widgets: only text content, position is automatic
	case WID_TITLE:
		if (argc != i + 1) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		free(w->text);
		w->text = strdup(argv[i]);
		w->width = display_props->width;
		debug(RPT_DEBUG, "Widget %s set to %s", wid, w->text);

		break;

	// Scroller widgets: bounds, direction, speed and text content
	case WID_SCROLLER:
		if (argc != i + 7) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0])) ||
		    (!isdigit((unsigned int)argv[i + 2][0])) ||
		    (!isdigit((unsigned int)argv[i + 3][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		// Direction must be 'm' (marquee), 'v' (vertical) or 'h' (horizontal)
		if (not_direction(argv[i + 4][0]) && argv[i + 4][0] != 'm') {
			sock_send_error(c->sock, "Invalid direction\\n");
			return 0;
		}

		w->left = atoi(argv[i]);
		w->top = atoi(argv[i + 1]);
		w->right = atoi(argv[i + 2]);
		w->bottom = atoi(argv[i + 3]);
		w->length = (unsigned char)argv[i + 4][0];
		w->speed = atoi(argv[i + 5]);
		free(w->text);
		w->text = strdup(argv[i + 6]);

		debug(RPT_DEBUG, "Widget %s set to %s", wid, w->text);

		break;

	// Frame widgets: bounds, dimensions, direction and speed
	case WID_FRAME:
		if (argc != i + 8) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if ((!isdigit((unsigned int)argv[i][0])) ||
		    (!isdigit((unsigned int)argv[i + 1][0])) ||
		    (!isdigit((unsigned int)argv[i + 2][0])) ||
		    (!isdigit((unsigned int)argv[i + 3][0])) ||
		    (!isdigit((unsigned int)argv[i + 4][0])) ||
		    (!isdigit((unsigned int)argv[i + 5][0]))) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		// Direction must be 'v' (vertical) or 'h' (horizontal)
		if (not_direction(argv[i + 6][0])) {
			sock_send_error(c->sock, "Invalid direction\\n");
			return 0;
		}

		w->left = atoi(argv[i]);
		w->top = atoi(argv[i + 1]);
		w->right = atoi(argv[i + 2]);
		w->bottom = atoi(argv[i + 3]);
		w->width = atoi(argv[i + 4]);
		w->height = atoi(argv[i + 5]);
		w->length = (unsigned char)argv[i + 6][0];
		w->speed = atoi(argv[i + 7]);

		debug(RPT_DEBUG, "Widget %s set to (%i,%i)-(%i,%i) %ix%i", wid, w->left, w->top,
		      w->right, w->bottom, w->width, w->height);

		break;

	// Numeric widgets: x coordinate and number value
	case WID_NUM:
		if (argc != i + 2) {
			sock_send_error(c->sock, "Wrong number of arguments\\n");
			return 0;
		}

		if (!isdigit((unsigned int)argv[i][0])) {
			sock_send_error(c->sock, "Invalid coordinates\\n");
			return 0;
		}

		if (!isdigit((unsigned int)argv[i + 1][0])) {
			sock_send_error(c->sock, "Invalid number\\n");
			return 0;
		}

		w->x = atoi(argv[i]);
		w->y = atoi(argv[i + 1]);

		debug(RPT_DEBUG, "Widget %s set to %i", wid, w->y);

		break;

	// Reject invalid or uninitialized widget types
	case WID_NONE:
	default:
		sock_send_error(c->sock, "Widget has no type\\n");
		return 0;
	}

	sock_send_string(c->sock, "success\\n");

	return 0;
}
