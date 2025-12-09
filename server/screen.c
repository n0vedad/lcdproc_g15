// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/screen.c
 * \brief Screen management implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author Peter Marschall
 * \date 1999-2008
 *
 * \features
 * - Screen creation and destruction
 * - Widget management within screens
 * - Priority system implementation
 * - Key reservation and lookup
 * - Client association and ownership
 * - Menu system integration
 *
 * \usage
 * - Screen lifecycle management functions
 * - Widget list operations and iteration
 * - Priority name conversion utilities
 * - Key list search functionality
 * - Screen property initialization
 *
 * \details This file stores all the screen definition-handling code. Functions here
 * provide means to create new screens and destroy existing ones. Screens are
 * identified by client and by the client's own identifiers for screens.
 * Screens are managed through linked lists, each screen maintains its own widget list,
 * priority names are mapped to enumeration values, key lists are stored as
 * null-terminated string arrays, and automatic integration with menu system is provided.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/report.h"

#include "clients.h"
#include "drivers.h"
#include "main.h"
#include "menuscreens.h"
#include "render.h"
#include "screenlist.h"
#include "widget.h"

/** \name Global Screen Defaults
 * Default duration and timeout settings for new screens
 */
///@{
int default_duration = 0; ///< Default screen display duration (0 = infinite)
int default_timeout = -1; ///< Default screen timeout (-1 = never timeout)
///@}

/** \brief Priority level name strings
 *
 * \details Array of string names for priority levels, indexed by Priority enum.
 * Null-terminated for iteration. Used for protocol messages and configuration.
 */
char *pri_names[] = {
    "hidden", "background", "info", "foreground", "alert", "input", NULL,
};

// Create new screen with default properties and menu integration
Screen *screen_create(char *id, Client *client)
{
	Screen *s;

	debug(RPT_DEBUG, "%s(id=\"%.40s\", client=[%d])", __FUNCTION__, id,
	      (client ? client->sock : -1));

	if (!id) {
		report(RPT_ERR, "%s: Need id string", __FUNCTION__);
		return NULL;
	}

	s = calloc(sizeof(Screen), 1);
	if (s == NULL) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		return NULL;
	}

	s->id = strdup(id);
	if (s->id == NULL) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		free(s);
		return NULL;
	}

	s->priority = PRI_INFO;
	s->duration = default_duration;
	s->heartbeat = HEARTBEAT_OPEN;
	s->width = display_props->width;
	s->height = display_props->height;
	s->client = client;
	s->timeout = default_timeout;
	s->backlight = BACKLIGHT_OPEN;
	s->cursor = CURSOR_OFF;
	s->cursor_x = 1;
	s->cursor_y = 1;

	s->widgetlist = LL_new();
	if (s->widgetlist == NULL) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		free(s->id);
		free(s);
		return NULL;
	}

	menuscreen_add_screen(s);

	return s;
}

// Destroy screen and free all associated resources
void screen_destroy(Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(s=[%.40s])", __FUNCTION__, s->id);

	menuscreen_remove_screen(s);
	screenlist_remove(s);

	for (w = LL_GetFirst(s->widgetlist); w; w = LL_GetNext(s->widgetlist)) {
		widget_destroy(w);
	}
	LL_Destroy(s->widgetlist);

	if (s->id != NULL)
		free(s->id);

	if (s->name != NULL)
		free(s->name);

	if (s->keys != NULL)
		free(s->keys);

	free(s);
}

// Add widget to screen's widget list
int screen_add_widget(Screen *s, Widget *w)
{
	debug(RPT_DEBUG, "%s(s=[%.40s], widget=[%.40s])", __FUNCTION__, s->id, w->id);

	LL_Push(s->widgetlist, (void *)w);

	return 0;
}

// Remove widget from screen's widget list (does not destroy widget)
int screen_remove_widget(Screen *s, Widget *w)
{
	debug(RPT_DEBUG, "%s(s=[%.40s], widget=[%.40s])", __FUNCTION__, s->id, w->id);

	LL_Remove(s->widgetlist, (void *)w, NEXT);

	return 0;
}

// Find widget by ID (searches recursively in frame widgets)
Widget *screen_find_widget(Screen *s, char *id)
{
	Widget *w;

	if (!s)
		return NULL;
	if (!id)
		return NULL;

	debug(RPT_DEBUG, "%s(s=[%.40s], id=\"%.40s\")", __FUNCTION__, s->id, id);

	// Widget search loop with recursive frame traversal for nested container support
	for (w = LL_GetFirst(s->widgetlist); w != NULL; w = LL_GetNext(s->widgetlist)) {
		if (0 == strcmp(w->id, id)) {
			debug(RPT_DEBUG, "%s: Found %s", __FUNCTION__, id);
			return w;
		}
		if (w->type == WID_FRAME) {
			w = widget_search_subs(w, id);
			if (w != NULL)
				return w;
		}
	}
	debug(RPT_DEBUG, "%s: Not found", __FUNCTION__);
	return NULL;
}

// Test if key is reserved by screen
char *screen_find_key(Screen *s, const char *key)
{
	char *start = s->keys, *end;
	int len = strlen(key);

	if (!start)
		return NULL;

	// Search null-terminated key string list for matching key, skipping to next string boundary
	// on mismatch
	end = start + s->keys_size - len;
	while (start < end) {
		if (start[len] == 0) {
			if (strcmp(start, key) == 0)
				return start;
			else
				start += len;
		}

		while (*start)
			++start;
		++start;
	}

	return NULL;
}

// Convert priority name string to priority enumeration value
Priority screen_pri_name_to_pri(char *priname)
{
	Priority pri = PRI_HIDDEN;
	int i;

	for (i = 0; pri_names[i]; i++) {
		if (strcmp(pri_names[i], priname) == 0) {
			pri = i;
			break;
		}
	}

	return pri;
}

// Convert priority enumeration value to name string
char *screen_pri_to_pri_name(Priority pri) { return pri_names[pri]; }
