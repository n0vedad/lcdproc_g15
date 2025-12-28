// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/widget.c
 * \brief Widget management implementation for LCDproc server
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author Peter Marschall
 * \date 1999-2008
 *
 * \features
 * - Widget creation and initialization
 * - Widget destruction and cleanup
 * - Widget type name conversion
 * - Icon name/number conversion
 * - Frame widget screen management
 * - Subwidget search functionality
 *
 * \usage
 * - Widget object creation and management
 * - Type name and icon conversion utilities
 * - Frame widget with associated screen handling
 * - Widget lookup and search operations
 * - Memory management for widget structures
 *
 * \details This file houses code that handles the creation and destruction of widget
 * objects for the server. These functions are called from the command parser
 * storing the specified widget in a generic container that is parsed later
 * by the screen renderer. Supports all standard LCD widget types, frame widgets
 * create associated screens, icon lookup tables for name/number mapping, type name
 * arrays for string conversion, and memory management for widget structures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "shared/report.h"
#include "shared/sockets.h"

#include "drivers/lcd.h"
#include "render.h"
#include "screen.h"
#include "widget.h"

/** \brief Widget type name lookup table
 *
 * \details Array of string names indexed by WidgetType enum. Null-terminated
 * for iteration. Used for protocol messages and debugging output.
 */
char *typenames[] = {
    // WID_NONE
    "none",
    // WID_STRING
    "string",
    // WID_HBAR
    "hbar",
    // WID_VBAR
    "vbar",
    // WID_PBAR
    "pbar",
    // WID_ICON
    "icon",
    // WID_TITLE
    "title",
    // WID_SCROLLER
    "scroller",
    // WID_FRAME
    "frame",
    // WID_NUM
    "num",
    NULL,
};

/**
 * \brief Icon lookup table entry structure
 * \details Maps icon numbers to names for bidirectional conversion
 */
struct icontable {
	int icon;	///< Icon number constant
	char *iconname; ///< Icon name string
};

/** \brief Icon name/number bidirectional mapping table
 *
 * \details Array mapping icon constants (ICON_*) to their string names.
 * Terminated with NULL iconname. Used by widget_icon_to_iconname() and
 * widget_iconname_to_icon() for conversion.
 */
struct icontable icontable[] = {{ICON_BLOCK_FILLED, "BLOCK_FILLED"},
				{ICON_HEART_OPEN, "HEART_OPEN"},
				{ICON_HEART_FILLED, "HEART_FILLED"},
				{ICON_ARROW_UP, "ARROW_UP"},
				{ICON_ARROW_DOWN, "ARROW_DOWN"},
				{ICON_ARROW_LEFT, "ARROW_LEFT"},
				{ICON_ARROW_RIGHT, "ARROW_RIGHT"},
				{ICON_CHECKBOX_OFF, "CHECKBOX_OFF"},
				{ICON_CHECKBOX_ON, "CHECKBOX_ON"},
				{ICON_CHECKBOX_GRAY, "CHECKBOX_GRAY"},
				{ICON_SELECTOR_AT_LEFT, "SELECTOR_AT_LEFT"},
				{ICON_SELECTOR_AT_RIGHT, "SELECTOR_AT_RIGHT"},
				{ICON_ELLIPSIS, "ELLIPSIS"},
				{ICON_STOP, "STOP"},
				{ICON_PAUSE, "PAUSE"},
				{ICON_PLAY, "PLAY"},
				{ICON_PLAYR, "PLAYR"},
				{ICON_FF, "FF"},
				{ICON_FR, "FR"},
				{ICON_NEXT, "NEXT"},
				{ICON_PREV, "PREV"},
				{ICON_REC, "REC"},
				{0, NULL}};

// Create and initialize new widget with default properties
Widget *widget_create(char *id, WidgetType type, Screen *screen)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(id=\"%s\", type=%d, screen=[%s])", __FUNCTION__, id, type, screen->id);

	w = calloc(1, sizeof(Widget));

	w->id = strdup(id);
	w->type = type;
	w->screen = screen;

	w->x = 1;
	w->y = 1;
	w->left = 1;
	w->top = 1;
	w->length = 1;
	w->speed = 1;

	if (type == WID_FRAME) {
		size_t frame_name_size = sizeof("frame_") + strlen(id);
		char frame_name[frame_name_size];

		strncpy(frame_name, "frame_", frame_name_size - 1);
		frame_name[frame_name_size - 1] = '\0';

		strncat(frame_name, id, frame_name_size - strlen(frame_name) - 1);

		w->frame_screen = screen_create(frame_name, screen->client);
	}

	return w;
}

// Destroy widget and free all associated resources
void widget_destroy(Widget *w)
{
	debug(RPT_DEBUG, "%s(w=[%s])", __FUNCTION__, w->id);

	if (!w)
		return;

	free(w->id);
	free(w->text);

	if (w->type == WID_FRAME)
		screen_destroy(w->frame_screen);

	free(w);
}

// Convert widget typename string to WidgetType enum value
WidgetType widget_typename_to_type(char *typename)
{
	int i;

	for (i = 0; typenames[i] != NULL; i++) {
		if (strcmp(typenames[i], typename) == 0) {
			return i;
		}
	}

	return WID_NONE;
}

// Convert WidgetType enum value to typename string
char *widget_type_to_typename(WidgetType t) { return typenames[t]; }

// Search for widget by ID within frame widget's subwidgets
Widget *widget_search_subs(Widget *w, char *id)
{
	if (w->type == WID_FRAME) {
		return screen_find_widget(w->frame_screen, id);
	} else {
		return NULL;
	}
}

// Convert icon number to icon name string
char *widget_icon_to_iconname(int icon)
{
	int i;

	for (i = 0; icontable[i].iconname != NULL; i++) {
		if (icontable[i].icon == icon) {
			return icontable[i].iconname;
		}
	}

	return NULL;
}

// Convert icon name string to icon number (case-insensitive)
int widget_iconname_to_icon(char *iconname)
{
	int i;

	for (i = 0; icontable[i].iconname != NULL; i++) {
		if (strcasecmp(icontable[i].iconname, iconname) == 0) {
			return icontable[i].icon;
		}
	}

	return -1;
}
