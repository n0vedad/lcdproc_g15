// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/serverscreens.c
 * \brief Server screen generation and management implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author Peter Marschall
 * \date 1999-2007
 *
 * \features
 * - Server status screen with client and screen counts
 * - Welcome message display on startup
 * - Goodbye message display on shutdown
 * - Configurable hello and goodbye messages
 * - Blank screen mode support
 * - Dynamic screen content updates
 * - Automatic hello to status transition
 *
 * \usage
 * - Server screen creation and initialization
 * - Welcome/goodbye message management
 * - Client and screen count display
 * - Display size adaptation and formatting
 * - Screen rotation and priority control
 *
 * \details This file contains code to allow the server to generate its own screens.
 * Currently, the startup, goodbye and server status screen are provided. The
 * server status screen shows total number of connected clients, and the
 * combined total of screens they provide. Server creates a special screen definition
 * for its screens, uses the same widget set made available to clients, automatically
 * switches from hello to status display when clients connect, supports different
 * display sizes and formats appropriately, and integrates with the normal screen
 * rotation system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/configfile.h"
#include "shared/report.h"

#include "clients.h"
#include "drivers.h"
#include "main.h"
#include "render.h"
#include "screen.h"
#include "screenlist.h"
#include "serverscreens.h"
#include "widget.h"

// Global server screen instance
Screen *server_screen = NULL;

// Server screen rotation setting
int rotate_server_screen = UNSET_INT;

/** \brief Flag indicating if custom hello message is configured
 *
 * \details Set to 1 if "Hello" key exists in config, 0 otherwise.
 * Determines whether to display custom or default startup message.
 */
static int has_hello_msg = 0;
static int reset_server_screen(int rotate, int heartbeat, int title);

// Create and initialize server screen with widgets for each display line
int server_screen_init(void)
{
	Widget *w;
	int i;

	has_hello_msg = config_has_key("Server", "Hello");

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	server_screen = screen_create("_server_screen", NULL);
	if (!server_screen) {
		report(RPT_ERR, "server_screen_init: Error allocating screen");
		return -1;
	}

	server_screen->name = "Server screen";
	server_screen->duration = 1e6 / frame_interval + 1;

	// Create one string widget per display line for dynamic server message rendering
	for (i = 0; i < display_props->height; i++) {
		char id[8];
		sprintf(id, "line%d", i + 1);

		w = widget_create(id, WID_STRING, server_screen);
		if (w == NULL) {
			report(RPT_ERR, "server_screen_init: Can't create a widget");
			return -1;
		}

		screen_add_widget(server_screen, w);
		w->x = 1;
		w->y = i + 1;
		w->text = calloc(LCD_MAX_WIDTH + 1, 1);
	}

	reset_server_screen(rotate_server_screen, !has_hello_msg, !has_hello_msg);

	// Populate server screen widgets with multi-line hello message from config file
	if (has_hello_msg) {
		int i;

		for (i = 0; i < display_props->height; i++) {
			const char *line = config_get_string("Server", "Hello", i, "");
			char id[8];

			sprintf(id, "line%d", i + 1);

			w = screen_find_widget(server_screen, id);

			if ((w != NULL) && (w->text != NULL)) {
				strncpy(w->text, line, LCD_MAX_WIDTH);
				w->text[LCD_MAX_WIDTH] = '\0';
			}
		}
	}

	screenlist_add(server_screen);

	debug(RPT_DEBUG, "%s() done", __FUNCTION__);

	return 0;
}

// Clean up server screen and free all resources
int server_screen_shutdown(void)
{
	if (server_screen == NULL)
		return -1;

	screenlist_remove(server_screen);

	screen_destroy(server_screen);

	return 0;
}

// Update server screen with client and screen counts
int update_server_screen(void)
{
	static int hello_done = 0;
	Client *c;
	Widget *w;
	int num_clients = 0;
	int num_screens = 0;

	num_clients = clients_client_count();

	// Hello message transition logic: display hello screen until client connects, then switch
	// to server info screen once
	if (has_hello_msg && !hello_done) {

		/**
		 * \todo Check num_screens instead of num_clients for hello message logic
		 *
		 * Current implementation checks `num_clients` but should check `num_screens`
		 * because update_server_screen() is only called when the server screen is active,
		 * causing the screen count to update too late.
		 *
		 * Problem: If a client connects and disconnects quickly during first screen
		 * rotation, num_screens is still 0 when update_server_screen() is eventually
		 * called, causing the hello message to display incorrectly.
		 *
		 * Solution:
		 * - Check num_screens instead of num_clients
		 * - Call update_server_screen() on every client connect/disconnect event
		 * - Ensure screen count updates immediately, not lazily
		 *
		 * \see server/main.c:910 (related ToDo about moving update_server_screen call)
		 * \ingroup ToDo_low
		 */

		if (num_clients != 0) {
			reset_server_screen(rotate_server_screen, 1, 1);
			hello_done = 1;

		} else {
			return 0;
		}
	}

	// Count total screens across all clients and update server screen widgets with
	// client/screen statistics, adapting layout to display dimensions
	for (c = clients_getfirst(); c != NULL; c = clients_getnext()) {
		num_screens += client_screen_count(c);
	}

	if (rotate_server_screen != SERVERSCREEN_BLANK) {
		if (display_props->height >= 3) {

			w = screen_find_widget(server_screen, "line2");
			if ((w != NULL) && (w->text != NULL)) {
				snprintf(w->text, LCD_MAX_WIDTH, "Clients: %i", num_clients);
			}

			w = screen_find_widget(server_screen, "line3");
			if ((w != NULL) && (w->text != NULL)) {
				snprintf(w->text, LCD_MAX_WIDTH, "Screens: %i", num_screens);
			}
		} else {

			w = screen_find_widget(server_screen, "line2");
			if ((w != NULL) && (w->text != NULL)) {
				snprintf(w->text, LCD_MAX_WIDTH,
					 ((display_props->width >= 16) ? "Cli: %i  Scr: %i"
								       : "C: %i  S: %i"),
					 num_clients, num_screens);
			}
		}
	}

	return 0;
}

// Display custom or default centered goodbye message
int goodbye_screen(void)
{
	if (!display_props)
		return 0;

	drivers_clear();

	// Display goodbye message: custom multi-line from config if defined, otherwise centered
	// default message with platform-specific branding
	if (config_has_key("Server", "GoodBye")) {
		int i;

		for (i = 0; i < display_props->height; i++) {
			const char *line = config_get_string("Server", "GoodBye", i, "");

			drivers_string(1, 1 + i, line);
		}
	} else {
		if ((display_props->height >= 2) && (display_props->width >= 16)) {
			int xoffs = (display_props->width - 16) / 2;
			int yoffs = (display_props->height - 2) / 2;

			char *top = "Thanks for using";

#ifdef LINUX
			char *low = "LCDproc & Linux!";
#else
			char *low = "    LCDproc!    ";
#endif

			drivers_string(1 + xoffs, 1 + yoffs, top);
			drivers_string(1 + xoffs, 2 + yoffs, low);
		}
	}

	drivers_cursor(1, 1, CURSOR_OFF);
	drivers_flush();

	return 0;
}

/**
 * \brief Configure server screen display properties
 * \param rotate Enable screen rotation flag
 * \param heartbeat Heartbeat display mode
 * \param title Show title flag
 * \retval 0 Success
 * \retval -1 Server screen not initialized
 *
 * \details Sets priority, heartbeat mode, backlight, and optionally adds title widget.
 */
static int reset_server_screen(int rotate, int heartbeat, int title)
{
	int i;

	if (server_screen == NULL)
		return -1;

	server_screen->heartbeat =
	    (heartbeat && (rotate != SERVERSCREEN_BLANK)) ? HEARTBEAT_OPEN : HEARTBEAT_OFF;
	server_screen->priority = (rotate == SERVERSCREEN_ON) ? PRI_INFO : PRI_BACKGROUND;

	// Reset all server screen widgets: clear text, set positions, and configure first line as
	// title widget if enabled
	for (i = 0; i < display_props->height; i++) {
		char id[8];
		Widget *w;

		sprintf(id, "line%d", i + 1);
		w = screen_find_widget(server_screen, id);

		if (w != NULL) {
			w->x = 1;
			w->y = i + 1;
			w->type = ((i == 0) && (title) && (rotate != SERVERSCREEN_BLANK))
				      ? WID_TITLE
				      : WID_STRING;

			if (w->text != NULL) {
				w->text[0] = '\0';
				if ((i == 0) && (title) && (rotate != SERVERSCREEN_BLANK)) {
					strncpy(w->text, "LCDproc Server", LCD_MAX_WIDTH);
					w->text[LCD_MAX_WIDTH] = '\0';
				}
			}
		}
	}

	return 0;
}
