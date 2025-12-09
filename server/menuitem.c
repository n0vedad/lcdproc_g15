// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/menuitem.c
 * \brief Menu item implementation and management functions
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \date 1999-2005
 *
 * \features
 * - Menu item creation and destruction for all types
 * - Screen building and updating for interactive items
 * - Input processing and event handling
 * - Type conversion utilities
 * - Validation functions for IP addresses
 * - Function tables for type-specific operations
 *
 * \usage
 * - Used by menu system to handle menu item operations
 * - Separate functions for each menu item type
 * - IP address validation for IPv4 and IPv6
 * - Character input handling for alphanumeric items
 * - Slider and numeric value management
 *
 * \details Handles menu items and all actions that can be performed on them.
 * Provides implementation for different menu item types including creation,
 * destruction, screen building, and input processing.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/defines.h"
#include "shared/report.h"

#include "drivers.h"
#include "menu.h"
#include "menuitem.h"
#include "screen.h"
#include "sock.h"
#include "widget.h"

/** \brief Maximum buffer size for numeric value string conversion
 *
 * \details Size of temporary buffer used when converting numeric menu item
 * values to strings for display.
 */
#define MAX_NUMERIC_LEN 40

extern Menu *main_menu;

/** \brief Enable permissive menu navigation mode
 *
 * \details When true, allows navigation to any menu item ID even if not found
 * in the menu tree. When false, strict validation is enforced.
 */
bool menu_permissive_goto;

/** \brief Error message strings for menu validation
 *
 * \details Array of user-facing error messages indexed by error code.
 * Index 0 is empty (no error), followed by range, length, and format errors.
 */
char *error_strs[] = {"", "Out of range", "Too long", "Too short", "Invalid Address"};

/** \brief Menu item type name strings
 *
 * \details String names for each MenuItem type, indexed by MenuItemType enum.
 * Used for protocol messages and debugging output.
 */
char *menuitemtypenames[] = {"menu",   "action",  "checkbox", "ring",
			     "slider", "numeric", "alpha",    "ip"};

/** \brief Menu event type name strings
 *
 * \details String names for each menu event type, indexed by MenuEventType enum.
 * Used for protocol messages and debugging output.
 */
char *menueventtypenames[] = {"select", "update", "plus", "minus", "enter", "leave"};

// Internal lifecycle management functions for menu item types: destroy, reset, rebuild, and update
// operations
void menuitem_destroy_ring(MenuItem *item);
void menuitem_destroy_slider(MenuItem *item);
void menuitem_destroy_numeric(MenuItem *item);
void menuitem_destroy_alpha(MenuItem *item);
void menuitem_destroy_ip(MenuItem *item);

void menuitem_reset_numeric(MenuItem *item);
void menuitem_reset_alpha(MenuItem *item);
void menuitem_reset_ip(MenuItem *item);

void menuitem_rebuild_screen_slider(MenuItem *item, Screen *s);
void menuitem_rebuild_screen_numeric(MenuItem *item, Screen *s);
void menuitem_rebuild_screen_alpha(MenuItem *item, Screen *s);
void menuitem_rebuild_screen_ip(MenuItem *item, Screen *s);

/**
 * \brief Update slider menu item's on-screen display
 * \param item Slider menu item to update
 * \param s Screen containing the slider widgets
 *
 * \details Updates the visual representation of a slider item on screen by
 * modifying the text and bar widgets to reflect current value.
 */
void menuitem_update_screen_slider(MenuItem *item, Screen *s);
void menuitem_update_screen_numeric(MenuItem *item, Screen *s);
void menuitem_update_screen_alpha(MenuItem *item, Screen *s);
void menuitem_update_screen_ip(MenuItem *item, Screen *s);

// Internal input processing handlers for interactive menu item types (slider, numeric, alpha, IP)
MenuResult menuitem_process_input_slider(MenuItem *item, MenuToken token, const char *key,
					 unsigned int keymask);

MenuResult menuitem_process_input_numeric(MenuItem *item, MenuToken token, const char *key,
					  unsigned int keymask);

MenuResult menuitem_process_input_alpha(MenuItem *item, MenuToken token, const char *key,
					unsigned int keymask);

MenuResult menuitem_process_input_ip(MenuItem *item, MenuToken token, const char *key,
				     unsigned int keymask);

/**
 * \brief IP address string properties structure
 * \details Configuration for IP address input validation and formatting
 */
typedef struct {
	int maxlen;		     ///< Maximum string length
	char sep;		     ///< Separator character (e.g., '.' for IPv4)
	int base;		     ///< Number base for conversion
	int width;		     ///< Field width for each component
	int limit;		     ///< Maximum value for each component
	int posValue[5];	     ///< Positional value multipliers
	char format[5];		     ///< Format string for display
	int (*verify)(const char *); ///< Verification function pointer
	char dummy[16];		     ///< Padding/dummy field
} IpSstringProperties;

/** \brief IP address formatting configuration table
 *
 * \details Array indexed by IP address type (IPv4=0, IPv6=1) containing
 * formatting rules, validation parameters, and default values for each type.
 */
const IpSstringProperties IPinfo[] = {
    {15, '.', 10, 3, 255, {100, 10, 1, 0, 0}, "%03d", verify_ipv4, "0.0.0.0"},
    {39, ':', 16, 4, 65535, {4096, 256, 16, 1, 0}, "%04x", verify_ipv6, "0:0:0:0:0:0:0:0"}};

// Translate predecessor_id string into MenuResult enum
MenuResult menuitem_predecessor2menuresult(char *predecessor_id, MenuResult default_result)
{
	if (predecessor_id == NULL)
		return default_result;
	if (strcmp("_quit_", predecessor_id) == 0)
		return MENURESULT_QUIT;
	else if (strcmp("_close_", predecessor_id) == 0)
		return MENURESULT_CLOSE;
	else if (strcmp("_none_", predecessor_id) == 0)
		return MENURESULT_NONE;
	else
		return MENURESULT_PREDECESSOR;
}

// Translate successor_id string into MenuResult enum
MenuResult menuitem_successor2menuresult(char *successor_id, MenuResult default_result)
{
	if (successor_id == NULL)
		return default_result;
	if (strcmp("_quit_", successor_id) == 0)
		return MENURESULT_QUIT;
	else if (strcmp("_close_", successor_id) == 0)
		return MENURESULT_CLOSE;
	else if (strcmp("_none_", successor_id) == 0)
		return MENURESULT_NONE;
	else
		return MENURESULT_SUCCESSOR;
}

// Search for a menu item by ID within a client's menus
MenuItem *menuitem_search(char *menu_id, Client *client)
{
	MenuItem *top = menu_permissive_goto ? main_menu : client->menu;

	return menu_find_item(top, menu_id, true);
}

/** \brief Destructor function table for menu item types
 *
 * \details Function pointer table indexed by MenuItemType enum. Maps each menu
 * item type to its destructor function for polymorphic cleanup. NULL entries
 * indicate types that don't need special cleanup.
 */
void (*destructor_table[NUM_ITEMTYPES])(MenuItem *item) = {menu_destroy,
							   NULL,
							   NULL,
							   menuitem_destroy_ring,
							   menuitem_destroy_slider,
							   menuitem_destroy_numeric,
							   menuitem_destroy_alpha,
							   menuitem_destroy_ip};

/** \brief Reset function table for menu item types
 *
 * \details Function pointer table indexed by MenuItemType enum. Maps each menu
 * item type to its reset function for restoring default values. NULL entries
 * indicate types that don't support reset.
 */
void (*reset_table[NUM_ITEMTYPES])(MenuItem *item) = {
    menu_reset,	      NULL, NULL, NULL, NULL, menuitem_reset_numeric, menuitem_reset_alpha,
    menuitem_reset_ip};

/** \brief Screen build function table for menu item types
 *
 * \details Function pointer table indexed by MenuItemType enum. Maps each menu
 * item type to its screen construction function for creating display widgets.
 * NULL entries indicate types that don't require widget construction.
 */
void (*build_screen_table[NUM_ITEMTYPES])(MenuItem *item,
					  Screen *s) = {menu_build_screen,
							NULL,
							NULL,
							NULL,
							menuitem_rebuild_screen_slider,
							menuitem_rebuild_screen_numeric,
							menuitem_rebuild_screen_alpha,
							menuitem_rebuild_screen_ip};

/** \brief Screen update function table for menu item types
 *
 * \details Function pointer table indexed by MenuItemType enum. Maps each menu
 * item type to its screen update function for dynamic widget refresh. NULL
 * entries indicate types that don't require dynamic updates.
 */
void (*update_screen_table[NUM_ITEMTYPES])(MenuItem *item,
					   Screen *s) = {menu_update_screen,
							 NULL,
							 NULL,
							 NULL,
							 menuitem_update_screen_slider,
							 menuitem_update_screen_numeric,
							 menuitem_update_screen_alpha,
							 menuitem_update_screen_ip};

/** \brief Input processing function table for menu item types
 *
 * \details Function pointer table indexed by MenuItemType enum. Maps each menu
 * item type to its input handler for user interaction processing. NULL entries
 * indicate types that don't process user input.
 */
MenuResult (*process_input_table[NUM_ITEMTYPES])(MenuItem *item, MenuToken token, const char *key,
						 unsigned int keymask) = {
    menu_process_input,
    NULL,
    NULL,
    NULL,
    menuitem_process_input_slider,
    menuitem_process_input_numeric,
    menuitem_process_input_alpha,
    menuitem_process_input_ip};

// Create a generic menu item of specified type
MenuItem *menuitem_create(MenuItemType type, char *id, MenuEventFunc(*event_func), char *text,
			  Client *client)
{
	MenuItem *new_item;

	debug(RPT_DEBUG, "%s(type=%d, id=\"%s\", event_func=%p, text=\"%s\")", __FUNCTION__, type,
	      id, event_func, text);

	if ((id == NULL) || (text == NULL)) {
		report(RPT_ERR, "%s: illegal id or text", __FUNCTION__);
		return NULL;
	}

	new_item = malloc(sizeof(MenuItem));
	if (!new_item) {
		report(RPT_ERR, "%s: Could not allocate memory", __FUNCTION__);
		return NULL;
	}

	new_item->type = type;
	new_item->id = strdup(id);
	if (!new_item->id) {
		report(RPT_ERR, "%s: Could not allocate memory", __FUNCTION__);
		free(new_item);
		return NULL;
	}

	new_item->successor_id = NULL;
	new_item->predecessor_id = NULL;
	new_item->parent = NULL;
	new_item->event_func = event_func;
	new_item->text = strdup(text);
	if (!new_item->text) {
		report(RPT_ERR, "%s: Could not allocate memory", __FUNCTION__);
		free(new_item->id);
		free(new_item);
		return NULL;
	}

	new_item->client = client;
	new_item->is_hidden = false;

	memset(&(new_item->data), '\0', sizeof(new_item->data));

	return new_item;
}

// Create an action item (a selectable string)
MenuItem *menuitem_create_action(char *id, MenuEventFunc(*event_func), char *text, Client *client,
				 MenuResult menu_result)
{
	/**
	 * \todo the menu_result arg is obsoleted (use char* successor_id)
	 *
	 * Function parameter `menu_result` in menuitem_create_action() should be replaced with
	 * `char* successor_id` for better API design. Current MenuResult enum is limited and
	 * inflexible compared to using successor IDs directly.
	 *
	 * Current: MenuResult enum (CLOSE, QUIT, NONE) limits navigation options
	 * Desired: char* successor_id allows flexible menu navigation to any menu by ID
	 *
	 * Benefits of successor_id:
	 * - Direct navigation to specific menus
	 * - Removes dependency on MenuResult enum
	 * - Consistent with other menuitem functions
	 * - More flexible menu flow control
	 *
	 * Impact: API consistency, code modernization, menu navigation flexibility
	 *
	 * \ingroup ToDo_medium
	 */

	MenuItem *new_item;

	debug(RPT_DEBUG, "%s(id=[%s], event_func=%p, text=\"%s\", close_menu=%d)", __FUNCTION__, id,
	      event_func, text, menu_result);

	new_item = menuitem_create(MENUITEM_ACTION, id, event_func, text, client);
	if (new_item != NULL) {

		switch (menu_result) {

		// No action after selection
		case MENURESULT_NONE:
			new_item->successor_id = strdup("_none_");
			break;

		// Close current menu after selection
		case MENURESULT_CLOSE:
			new_item->successor_id = strdup("_close_");
			break;

		// Quit all menus after selection
		case MENURESULT_QUIT:
			new_item->successor_id = strdup("_quit_");
			break;

		// Invalid menu result
		default:
			assert(!"unexpected MENURESULT");
		}
	}

	return new_item;
}

// Create a checkbox menu item
MenuItem *menuitem_create_checkbox(char *id, MenuEventFunc(*event_func), char *text, Client *client,
				   bool allow_gray, bool value)
{
	MenuItem *new_item;

	debug(RPT_DEBUG, "%s(id=[%s], event_func=%p, text=\"%s\", allow_gray=%d, value=%d)",
	      __FUNCTION__, id, event_func, text, allow_gray, value);

	new_item = menuitem_create(MENUITEM_CHECKBOX, id, event_func, text, client);
	if (new_item != NULL) {
		new_item->data.checkbox.allow_gray = allow_gray;
		new_item->data.checkbox.value = value;
	}

	return new_item;
}

// Create a ring menu item with selectable options
MenuItem *menuitem_create_ring(char *id, MenuEventFunc(*event_func), char *text, Client *client,
			       char *strings, short value)
{
	MenuItem *new_item;

	debug(RPT_DEBUG, "%s(id=[%s], event_func=%p, text=\"%s\", strings=\"%s\", value=%d)",
	      __FUNCTION__, id, event_func, text, strings, value);

	new_item = menuitem_create(MENUITEM_RING, id, event_func, text, client);
	if (new_item != NULL) {
		new_item->data.ring.strings = tablist2linkedlist(strings);
		new_item->data.ring.value = value;
	}

	return new_item;
}

// Create a slider menu item with adjustable value
MenuItem *menuitem_create_slider(char *id, MenuEventFunc(*event_func), char *text, Client *client,
				 char *mintext, char *maxtext, int minvalue, int maxvalue,
				 int stepsize, int value)
{
	MenuItem *new_item;

	debug(RPT_DEBUG,
	      "%s(id=[%s], event_func=%p, text=\"%s\", mintext=\"%s\", maxtext=\"%s\", "
	      "minvalue=%d, maxvalue=%d, stepsize=%d, value=%d)",
	      __FUNCTION__, id, event_func, text, mintext, maxtext, minvalue, maxvalue, stepsize,
	      value);

	new_item = menuitem_create(MENUITEM_SLIDER, id, event_func, text, client);
	if (new_item != NULL) {
		new_item->data.slider.mintext = strdup(mintext);
		new_item->data.slider.maxtext = strdup(maxtext);
		if (new_item->data.slider.mintext == NULL ||
		    new_item->data.slider.maxtext == NULL) {
			menuitem_destroy(new_item);
			return NULL;
		}
		new_item->data.slider.minvalue = minvalue;
		new_item->data.slider.maxvalue = maxvalue;
		new_item->data.slider.stepsize = stepsize;
		new_item->data.slider.value = value;
	}

	return new_item;
}

// Create a numeric input menu item
MenuItem *menuitem_create_numeric(char *id, MenuEventFunc(*event_func), char *text, Client *client,
				  int minvalue, int maxvalue, int value)
{
	MenuItem *new_item;

	debug(RPT_DEBUG,
	      "%s(id=[%s], event_func=%p, text=\"%s\", minvalue=%d, maxvalue=%d, value=%d)",
	      __FUNCTION__, id, event_func, text, minvalue, minvalue, value);

	new_item = menuitem_create(MENUITEM_NUMERIC, id, event_func, text, client);
	if (new_item != NULL) {
		new_item->data.numeric.maxvalue = maxvalue;
		new_item->data.numeric.minvalue = minvalue;
		new_item->data.numeric.value = value;
		new_item->data.numeric.edit_str = malloc(MAX_NUMERIC_LEN);
		if (new_item->data.numeric.edit_str == NULL) {
			menuitem_destroy(new_item);
			return NULL;
		}
	}

	return new_item;
}

// Create an alphanumeric string input menu item
MenuItem *menuitem_create_alpha(char *id, MenuEventFunc(*event_func), char *text, Client *client,
				char password_char, short minlength, short maxlength,
				bool allow_caps, bool allow_noncaps, bool allow_numbers,
				char *allowed_extra, char *value)
{
	MenuItem *new_item;

	debug(RPT_DEBUG,
	      "%s(id=\"%s\", event_func=%p, text=\"%s\", password_char=%d, maxlength=%d, "
	      "value=\"%s\")",
	      __FUNCTION__, id, event_func, text, password_char, maxlength, value);

	new_item = menuitem_create(MENUITEM_ALPHA, id, event_func, text, client);

	if (new_item != NULL) {
		// Configure password masking and length constraints
		new_item->data.alpha.password_char = password_char;
		new_item->data.alpha.minlength = minlength;
		new_item->data.alpha.maxlength = maxlength;

		// Configure allowed character types (caps, noncaps, numbers, extra)
		new_item->data.alpha.allow_caps = allow_caps;
		new_item->data.alpha.allow_noncaps = allow_noncaps;
		new_item->data.alpha.allow_numbers = allow_numbers;
		new_item->data.alpha.allowed_extra = strdup(allowed_extra);
		if (new_item->data.alpha.allowed_extra == NULL) {
			menuitem_destroy(new_item);
			return NULL;
		}

		// Allocate value buffer and copy initial string with truncation
		new_item->data.alpha.value = malloc(maxlength + 1);
		if (new_item->data.alpha.value == NULL) {
			menuitem_destroy(new_item);
			return NULL;
		}

		strncpy(new_item->data.alpha.value, value, maxlength);
		new_item->data.alpha.value[maxlength] = 0;

		// Allocate separate edit buffer for input processing
		new_item->data.alpha.edit_str = malloc(maxlength + 1);
		if (new_item->data.alpha.edit_str == NULL) {
			menuitem_destroy(new_item);
			return NULL;
		}
	}

	return new_item;
}

// Create an IP address input menu item
MenuItem *menuitem_create_ip(char *id, MenuEventFunc(*event_func), char *text, Client *client,
			     bool v6, char *value)
{
	MenuItem *new_item;
	const IpSstringProperties *ipinfo;

	debug(RPT_DEBUG, "%s(id=\"%s\", event_func=%p, text=\"%s\", v6=%d, value=\"%s\")",
	      __FUNCTION__, id, event_func, text, v6, value);

	new_item = menuitem_create(MENUITEM_IP, id, event_func, text, client);
	if (new_item == NULL)
		return NULL;

	// Select IPv4 or IPv6 format configuration
	new_item->data.ip.v6 = v6;
	ipinfo = (v6) ? &IPinfo[1] : &IPinfo[0];

	// Allocate value buffer based on IP version (15 for IPv4, 39 for IPv6)
	new_item->data.ip.maxlength = ipinfo->maxlen;
	new_item->data.ip.value = malloc(new_item->data.ip.maxlength + 1);
	if (new_item->data.ip.value == NULL) {
		menuitem_destroy(new_item);
		return NULL;
	}

	strncpy(new_item->data.ip.value, value, new_item->data.ip.maxlength);
	new_item->data.ip.value[new_item->data.ip.maxlength] = '\0';

	// Normalize and validate IP address format
	if (ipinfo->verify != NULL) {
		char *start = new_item->data.ip.value;

		// Strip leading spaces and zeros from each field
		while (start != NULL) {
			char *skip = start;

			while ((*skip == ' ') ||
			       ((*skip == '0') && (skip[1] != ipinfo->sep) && (skip[1] != '\0')))
				skip++;
			memccpy(start, skip, '\0', new_item->data.ip.maxlength + 1);
			skip = strchr(start, ipinfo->sep);
			start = (skip != NULL) ? (skip + 1) : NULL;
		}

		// Replace invalid IP address with dummy value
		if (!ipinfo->verify(new_item->data.ip.value)) {
			report(RPT_WARNING, "%s(id=\"%s\") ip address not verified: \"%s\"",
			       __FUNCTION__, id, value);
			strncpy(new_item->data.ip.value, ipinfo->dummy,
				new_item->data.ip.maxlength);
			new_item->data.ip.value[new_item->data.ip.maxlength] = '\0';
		}
	}

	// Allocate separate edit buffer for input processing
	new_item->data.ip.edit_str = malloc(new_item->data.ip.maxlength + 1);
	if (new_item->data.ip.edit_str == NULL) {
		menuitem_destroy(new_item);
		return NULL;
	}

	return new_item;
}

// Delete menu item from memory
void menuitem_destroy(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		void (*destructor)(MenuItem *);

		destructor = destructor_table[item->type];
		if (destructor)
			destructor(item);

		free(item->text);
		free(item->id);
		free(item);
	}
}

/**
 * \brief Destroy ring menu item
 * \param item Menu item to destroy
 *
 * \details Frees ring strings list and list structure.
 */
void menuitem_destroy_ring(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		char *s;

		for (s = LL_GetFirst(item->data.ring.strings); s != NULL;
		     s = LL_GetNext(item->data.ring.strings)) {
			free(s);
		}
		LL_Destroy(item->data.ring.strings);
	}
}

/**
 * \brief Destroy slider menu item
 * \param item Menu item to destroy
 *
 * \details Frees min/max text strings.
 */
void menuitem_destroy_slider(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		free(item->data.slider.mintext);
		free(item->data.slider.maxtext);
	}
}

/**
 * \brief Destroy numeric menu item
 * \param item Menu item to destroy
 *
 * \details Frees numeric edit buffer.
 */
void menuitem_destroy_numeric(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		free(item->data.numeric.edit_str);
	}
}

/**
 * \brief Destroy alpha menu item
 * \param item Menu item to destroy
 *
 * \details Frees allowed characters, value, and edit buffer.
 */
void menuitem_destroy_alpha(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		free(item->data.alpha.allowed_extra);
		free(item->data.alpha.value);
		free(item->data.alpha.edit_str);
	}
}

/**
 * \brief Destroy IP address menu item
 * \param item Menu item to destroy
 *
 * \details Frees IP field edit buffers.
 */
void menuitem_destroy_ip(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item == NULL)
		return;

	free(item->data.ip.value);
	free(item->data.ip.edit_str);
}

// Reset menu item to initial state
void menuitem_reset(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		void (*func)(MenuItem *);
		func = reset_table[item->type];
		if (func)
			func(item);
	}
}

/**
 * \brief Reset numeric item to default value
 * \param item Menu item to reset
 *
 * \details Sets edit buffer to current value, resets cursor position.
 */
void menuitem_reset_numeric(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		item->data.numeric.edit_pos = 0;
		item->data.numeric.edit_offs = 0;

		memset(item->data.numeric.edit_str, '\0', MAX_NUMERIC_LEN);

		if (item->data.numeric.minvalue < 0) {
			snprintf(item->data.numeric.edit_str, MAX_NUMERIC_LEN, "%+d",
				 item->data.numeric.value);
		} else {
			snprintf(item->data.numeric.edit_str, MAX_NUMERIC_LEN, "%d",
				 item->data.numeric.value);
		}
	}
}

/**
 * \brief Reset alpha item to default value
 * \param item Menu item to reset
 *
 * \details Copies current value to edit buffer, resets cursor.
 */
void menuitem_reset_alpha(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if (item != NULL) {
		item->data.alpha.edit_pos = 0;
		item->data.alpha.edit_offs = 0;

		memset(item->data.alpha.edit_str, '\0', item->data.alpha.maxlength + 1);

		strncpy(item->data.alpha.edit_str, item->data.alpha.value,
			item->data.alpha.maxlength);

		item->data.alpha.edit_str[item->data.alpha.maxlength] = '\0';
	}
}

/**
 * \brief Reset IP item to default value
 * \param item Menu item to reset
 *
 * \details Parses current IP, formats to edit buffer, resets cursor.
 */
void menuitem_reset_ip(MenuItem *item)
{
	char *start = item->data.ip.value;
	const IpSstringProperties *ipinfo = (item->data.ip.v6) ? &IPinfo[1] : &IPinfo[0];

	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	item->data.ip.edit_pos = 0;
	item->data.ip.edit_offs = 0;

	memset(item->data.ip.edit_str, '\0', item->data.ip.maxlength + 1);

	// Parse and format IP address components: convert each numeric segment to string format,
	// append to edit_str with separator, iterate through all segments until end of string
	while (start != NULL) {
		char *end;
		char tmpstr[5];
		int num = (int)strtol(start, (char **)NULL, ipinfo->base);

		snprintf(tmpstr, 5, ipinfo->format, num);
		strncat(item->data.ip.edit_str, tmpstr,
			item->data.ip.maxlength - strlen(item->data.ip.edit_str) - 1);

		end = strchr(start, ipinfo->sep);
		start = (end != NULL) ? (end + 1) : NULL;

		if (start != NULL) {
			tmpstr[0] = ipinfo->sep;
			tmpstr[1] = '\0';
			strncat(item->data.ip.edit_str, tmpstr,
				item->data.ip.maxlength - strlen(item->data.ip.edit_str) - 1);
		}
	}
}

// Rebuild menu item screen widgets
void menuitem_rebuild_screen(MenuItem *item, Screen *s)
{
	Widget *w;
	void (*build_screen)(MenuItem *item, Screen *s);

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if (!display_props) {
		report(RPT_ERR, "%s: display size unknown", __FUNCTION__);
		return;
	}

	if (s != NULL) {
		while ((w = screen_getfirst_widget(s)) != NULL) {

			screen_remove_widget(s, w);
			widget_destroy(w);
		}

		if (item != NULL) {
			build_screen = build_screen_table[item->type];
			if (build_screen) {
				build_screen(item, s);

			} else {
				report(RPT_ERR, "%s: given menuitem cannot be active",
				       __FUNCTION__);
				return;
			}

			menuitem_update_screen(item, s);
		}
	}
}

/**
 * \brief Rebuild screen for slider item
 * \param item Menu item
 * \param s Screen to build on
 *
 * \details Creates title, value bar, and numeric value widgets.
 */
void menuitem_rebuild_screen_slider(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	if (display_props->height >= 2) {
		w = widget_create("text", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup(item->text);
		w->x = 1;
		w->y = 1;
	}

	w = widget_create("bar", WID_HBAR, s);
	screen_add_widget(s, w);
	w->width = display_props->width;

	if (display_props->height > 2) {
		w->x = 2;
		w->y = display_props->height / 2 + 1;
		w->width = display_props->width - 2;
	}

	w = widget_create("min", WID_STRING, s);
	screen_add_widget(s, w);
	w->text = NULL;
	w->x = 1;
	if (display_props->height > 2) {
		w->y = display_props->height / 2 + 2;
	} else {
		w->y = display_props->height / 2 + 1;
	}

	w = widget_create("max", WID_STRING, s);
	screen_add_widget(s, w);
	w->text = NULL;
	w->x = 1;
	if (display_props->height > 2) {
		w->y = display_props->height / 2 + 2;
	} else {
		w->y = display_props->height / 2 + 1;
	}
}

/**
 * \brief Rebuild screen for numeric item
 * \param item Menu item
 * \param s Screen to build on
 *
 * \details Creates title, value text, and optional error message widgets.
 */
void menuitem_rebuild_screen_numeric(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	if (display_props->height >= 2) {
		w = widget_create("text", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup(item->text);
		w->x = 1;
		w->y = 1;
	}

	w = widget_create("value", WID_STRING, s);
	screen_add_widget(s, w);
	w->text = malloc(MAX_NUMERIC_LEN);
	w->x = 2;
	w->y = display_props->height / 2 + 1;

	if (display_props->height > 2) {
		w = widget_create("error", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup("");
		w->x = 1;
		w->y = display_props->height;
	}
}

/**
 * \brief Rebuild screen for alpha item
 * \param item Menu item
 * \param s Screen to build on
 *
 * \details Creates title and text edit widgets with cursor support.
 */
void menuitem_rebuild_screen_alpha(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	if (display_props->height >= 2) {
		w = widget_create("text", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup(item->text);
		w->x = 1;
		w->y = 1;
	}

	w = widget_create("value", WID_STRING, s);
	screen_add_widget(s, w);
	w->text = malloc(item->data.alpha.maxlength + 1);
	w->x = 2;
	w->y = display_props->height / 2 + 1;

	if (display_props->height > 2) {
		w = widget_create("error", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup("");
		w->x = 1;
		w->y = display_props->height;
	}
}

/**
 * \brief Rebuild screen for IP address item
 * \param item Menu item
 * \param s Screen to build on
 *
 * \details Creates title and IP field widgets with cursor support.
 */
void menuitem_rebuild_screen_ip(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	if (display_props->height >= 2) {
		w = widget_create("text", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup(item->text);
		w->x = 1;
		w->y = 1;
	}

	w = widget_create("value", WID_STRING, s);
	screen_add_widget(s, w);
	w->text = malloc(item->data.ip.maxlength + 1);
	w->x = 2;
	w->y = display_props->height / 2 + 1;

	if (display_props->height > 2) {
		w = widget_create("error", WID_STRING, s);
		screen_add_widget(s, w);
		w->text = strdup("");
		w->x = 1;
		w->y = display_props->height;
	}
}

// Update menu item screen widgets with current values
void menuitem_update_screen(MenuItem *item, Screen *s)
{
	void (*update_screen)(MenuItem *item, Screen *s);

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	s->cursor = CURSOR_OFF;
	update_screen = update_screen_table[item->type];

	if (update_screen) {
		update_screen(item, s);
	} else {
		report(RPT_ERR, "%s: given menuitem cannot be active", __FUNCTION__);
		return;
	}
}

// Update screen widgets for slider menu item
void menuitem_update_screen_slider(MenuItem *item, Screen *s)
{
	Widget *w;
	int min_len, max_len;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	min_len = strlen(item->data.slider.mintext);
	max_len = strlen(item->data.slider.maxtext);
	w = screen_find_widget(s, "bar");

	if (display_props->height <= 2) {
		w->x = 1 + min_len;
		w->y = display_props->height;
		w->width = display_props->width - min_len - max_len;
	}

	/**
	 * \todo Use promille calculation for slider rendering instead of direct pixel calculation
	 *
	 * Current implementation calculates widget length directly in pixels, but should use
	 * promille (per-thousand) value for hardware-independent rendering:
	 *
	 * Proposed formula:
	 * ```
	 * w->promille = 1000 * (item->data.slider.value - item->data.slider.minvalue) /
	 *                      (item->data.slider.maxvalue - item->data.slider.minvalue)
	 * ```
	 *
	 * Benefits:
	 * - Hardware-independent rendering (drivers handle pixel conversion)
	 * - More accurate for different display sizes
	 * - Consistent with other widget types (bars, etc.)
	 * - Better precision than percentage (1000 steps vs 100 steps)
	 *
	 * Impact: Slider widget rendering accuracy, cross-driver compatibility
	 *
	 * \ingroup ToDo_medium
	 */

	w->length = w->width * display_props->cellwidth *
		    (item->data.slider.value - item->data.slider.minvalue) /
		    (item->data.slider.maxvalue - item->data.slider.minvalue);

	w = screen_find_widget(s, "min");
	if (w->text)
		free(w->text);
	w->text = strdup(item->data.slider.mintext);

	w = screen_find_widget(s, "max");
	if (w->text)
		free(w->text);
	w->x = 1 + display_props->width - max_len;
	w->text = strdup(item->data.slider.maxtext);
}

/**
 * \brief Update screen for numeric item
 * \param item Menu item
 * \param s Screen to update
 *
 * \details Updates value display, cursor position, and error message.
 */
void menuitem_update_screen_numeric(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	w = screen_find_widget(s, "value");
	strncpy(w->text, item->data.numeric.edit_str + item->data.numeric.edit_offs,
		MAX_NUMERIC_LEN - 1);
	w->text[MAX_NUMERIC_LEN - 1] = '\0';

	s->cursor = CURSOR_DEFAULT_ON;
	s->cursor_x = w->x + item->data.numeric.edit_pos - item->data.numeric.edit_offs;
	s->cursor_y = w->y;

	if (display_props->height > 2) {
		w = screen_find_widget(s, "error");
		free(w->text);
		w->text = strdup(error_strs[item->data.numeric.error_code]);
	}
}

/**
 * \brief Update screen for alpha item
 * \param item Menu item
 * \param s Screen to update
 *
 * \details Updates text (or password chars), cursor position, and error message.
 */
void menuitem_update_screen_alpha(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	w = screen_find_widget(s, "value");
	if (item->data.alpha.password_char == '\0') {
		strncpy(w->text, item->data.alpha.edit_str + item->data.alpha.edit_offs,
			item->data.alpha.maxlength);
		w->text[item->data.alpha.maxlength] = '\0';

	} else {
		int len = strlen(item->data.alpha.edit_str) - item->data.alpha.edit_offs;

		memset(w->text, item->data.alpha.password_char, len);
		w->text[len] = '\0';
	}

	s->cursor = CURSOR_DEFAULT_ON;
	s->cursor_x = w->x + item->data.alpha.edit_pos - item->data.alpha.edit_offs;
	s->cursor_y = w->y;

	if (display_props->height > 2) {
		w = screen_find_widget(s, "error");
		free(w->text);
		w->text = strdup(error_strs[item->data.alpha.error_code]);
	}
}

/**
 * \brief Update screen for IP address item
 * \param item Menu item
 * \param s Screen to update
 *
 * \details Updates IP field displays, cursor position, and error message.
 */
void menuitem_update_screen_ip(MenuItem *item, Screen *s)
{
	Widget *w;

	debug(RPT_DEBUG, "%s(item=[%s], screen=[%s])", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((item == NULL) || (s == NULL))
		return;

	w = screen_find_widget(s, "value");
	if (w != NULL) {
		strncpy(w->text, item->data.ip.edit_str + item->data.ip.edit_offs,
			item->data.ip.maxlength);
		w->text[item->data.ip.maxlength] = '\0';

		s->cursor = CURSOR_DEFAULT_ON;
		s->cursor_x = w->x + item->data.ip.edit_pos - item->data.ip.edit_offs;
		s->cursor_y = w->y;
	}

	if (display_props->height > 2) {
		w = screen_find_widget(s, "error");
		free(w->text);
		w->text = strdup(error_strs[item->data.ip.error_code]);
	}
}

// Process input events for menu items
MenuResult menuitem_process_input(MenuItem *item, MenuToken token, const char *key,
				  unsigned int keymask)
{
	MenuResult (*process_input)(MenuItem *item, MenuToken token, const char *key,
				    unsigned int keymask);

	debug(RPT_DEBUG, "%s(item=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), token, key);

	if (item == NULL)
		return MENURESULT_ERROR;

	process_input = process_input_table[item->type];
	if (process_input) {
		return process_input(item, token, key, keymask);

	} else {
		report(RPT_ERR, "%s: given menuitem cannot be active", __FUNCTION__);
		return MENURESULT_ERROR;
	}
}

/**
 * \brief Process user input for slider menu item
 * \param item Slider menu item
 * \param token Input token (key press, enter, etc.)
 * \param key Key string from input
 * \param keymask Key modifier mask
 * \return MenuResult indicating action taken
 *
 * \details Handles Up/Down to adjust value, Enter to confirm.
 */
MenuResult menuitem_process_input_slider(MenuItem *item, MenuToken token, const char *key,
					 unsigned int keymask)
{
	debug(RPT_DEBUG, "%s(item=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), token, key);

	if (item == NULL)
		return MENURESULT_ERROR;

	switch (token) {

	// Handle menu key
	case MENUTOKEN_MENU:
		return menuitem_predecessor2menuresult(item->predecessor_id, MENURESULT_CLOSE);

	// Handle enter key
	case MENUTOKEN_ENTER:
		return menuitem_successor2menuresult(item->successor_id, MENURESULT_CLOSE);

	// Handle up key
	case MENUTOKEN_UP:

	// Handle right key
	case MENUTOKEN_RIGHT:
		if ((!(keymask & (MENUTOKEN_LEFT | MENUTOKEN_DOWN))) &&
		    (item->data.slider.value == item->data.slider.maxvalue))
			item->data.slider.value = item->data.slider.minvalue;
		else
			item->data.slider.value =
			    min(item->data.slider.maxvalue,
				item->data.slider.value + item->data.slider.stepsize);
		if (item->event_func)
			item->event_func(item, MENUEVENT_PLUS);

		return MENURESULT_NONE;

	// Handle down key
	case MENUTOKEN_DOWN:

	// Handle left key
	case MENUTOKEN_LEFT:
		if ((!(keymask & (MENUTOKEN_RIGHT | MENUTOKEN_UP))) &&
		    (item->data.slider.value == item->data.slider.minvalue))
			item->data.slider.value = item->data.slider.maxvalue;
		else
			item->data.slider.value =
			    max(item->data.slider.minvalue,
				item->data.slider.value - item->data.slider.stepsize);
		if (item->event_func)
			item->event_func(item, MENUEVENT_MINUS);

		return MENURESULT_NONE;

	// Handle other keys
	case MENUTOKEN_OTHER:
	default:
		break;
	}

	return MENURESULT_ERROR;
}

/**
 * \brief Process user input for numeric entry menu item
 * \param item Numeric menu item
 * \param token Input token (key press, enter, etc.)
 * \param key Key string from input
 * \param keymask Key modifier mask
 * \return MenuResult indicating action taken
 *
 * \details Handles digit entry, backspace, Enter to confirm.
 */
MenuResult menuitem_process_input_numeric(MenuItem *item, MenuToken token, const char *key,
					  unsigned int keymask)
{
	char buf1[MAX_NUMERIC_LEN];
	char buf2[MAX_NUMERIC_LEN];

	int max_len;

	debug(RPT_DEBUG, "%s(item=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), token, key);

	if (item != NULL) {
		char *str = item->data.numeric.edit_str;
		int pos = item->data.numeric.edit_pos;
		int allow_signed = (item->data.numeric.minvalue < 0);
		char *format_str = (allow_signed) ? "%+d" : "%d";

		snprintf(buf1, MAX_NUMERIC_LEN, format_str, item->data.numeric.minvalue);
		snprintf(buf2, MAX_NUMERIC_LEN, format_str, item->data.numeric.maxvalue);

		max_len = max(strlen(buf1), strlen(buf2));

		item->data.numeric.error_code = 0;

		switch (token) {

		// Handle menu key
		case MENUTOKEN_MENU:
			if (pos == 0) {
				return menuitem_predecessor2menuresult(item->predecessor_id,
								       MENURESULT_CLOSE);
			} else {
				menuitem_reset_numeric(item);
			}
			return MENURESULT_NONE;

		// Handle enter key
		case MENUTOKEN_ENTER:
			if ((keymask & MENUTOKEN_RIGHT) || (str[pos] == '\0')) {
				int value;

				if (sscanf(str, "%d", &value) != 1) {
					return MENURESULT_ERROR;
				}
				if ((value < item->data.numeric.minvalue) ||
				    (value > item->data.numeric.maxvalue)) {
					item->data.numeric.error_code = 1;
					item->data.numeric.edit_pos = 0;
					item->data.numeric.edit_offs = 0;

					return MENURESULT_NONE;
				}

				item->data.numeric.value = value;

				if (item->event_func)
					item->event_func(item, MENUEVENT_UPDATE);

				return menuitem_successor2menuresult(item->successor_id,
								     MENURESULT_CLOSE);

			} else {
				if (pos < max_len) {
					item->data.numeric.edit_pos++;
					if (pos >= display_props->width - 2)
						item->data.numeric.edit_offs++;
				}
			}

			return MENURESULT_NONE;

		// Handle up key
		case MENUTOKEN_UP:
			if (pos >= max_len) {
				item->data.numeric.error_code = 2;
				item->data.numeric.edit_pos = 0;
				item->data.numeric.edit_offs = 0;
				return MENURESULT_NONE;
			}
			if (allow_signed && pos == 0) {
				str[0] = (str[0] == '-') ? '+' : '-';
			} else {
				if (str[pos] >= '0' && str[pos] < '9') {
					str[pos]++;
				} else if (str[pos] == '9') {
					str[pos] = '\0';
				} else if (str[pos] == '\0') {
					str[pos] = '0';
				}
			}

			return MENURESULT_NONE;

		// Handle down key
		case MENUTOKEN_DOWN:
			if (pos >= max_len) {
				item->data.numeric.error_code = 2;
				item->data.numeric.edit_pos = 0;
				item->data.numeric.edit_offs = 0;

				return MENURESULT_NONE;
			}
			if (allow_signed && pos == 0) {
				str[0] = (str[0] == '-') ? '+' : '-';
			} else {
				if (str[pos] > '0' && str[pos] <= '9') {
					str[pos]--;
				} else if (str[pos] == '0') {
					str[pos] = '\0';
				} else if (str[pos] == '\0') {
					str[pos] = '9';
				}
			}

			return MENURESULT_NONE;

		// Handle right key
		case MENUTOKEN_RIGHT:
			if (str[pos] != '\0' && pos < max_len) {
				item->data.numeric.edit_pos++;
				if (pos >= display_props->width - 2)
					item->data.numeric.edit_offs++;
			}

			return MENURESULT_NONE;

		// Handle left key
		case MENUTOKEN_LEFT:
			if (pos > 0) {
				item->data.numeric.edit_pos--;
				if (item->data.numeric.edit_offs > item->data.numeric.edit_pos)
					item->data.numeric.edit_offs = item->data.numeric.edit_pos;
			}

			return MENURESULT_NONE;

		// Handle other keys
		case MENUTOKEN_OTHER:
			if (pos >= max_len) {
				item->data.numeric.error_code = 2;
				item->data.numeric.edit_pos = 0;
				item->data.numeric.edit_offs = 0;
				return MENURESULT_NONE;
			}
			if ((strlen(key) == 1) && isdigit(key[0])) {
				str[pos] = key[0];
				item->data.numeric.edit_pos++;
				if (pos >= display_props->width - 2)
					item->data.numeric.edit_offs++;
			}

		default:
			return MENURESULT_NONE;
		}
	}
	return MENURESULT_ERROR;
}

/**
 * \brief Process user input for alphanumeric text entry menu item
 * \param item Alpha menu item
 * \param token Input token (key press, enter, etc.)
 * \param key Key string from input
 * \param keymask Key modifier mask
 * \return MenuResult indicating action taken
 *
 * \details Handles character entry, backspace, allowed character filtering, Enter to confirm.
 */
MenuResult menuitem_process_input_alpha(MenuItem *item, MenuToken token, const char *key,
					unsigned int keymask)
{
	char *p;
	static char *chars = NULL;

	debug(RPT_DEBUG, "%s(item=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), token, key);

	if (item != NULL) {
		char *str = item->data.alpha.edit_str;
		int pos = item->data.alpha.edit_pos;

		size_t chars_size = 26 + 26 + 10 + strlen(item->data.alpha.allowed_extra) + 1;
		char *new_chars = realloc(chars, chars_size);
		if (new_chars == NULL) {
			return MENURESULT_ERROR;
		}
		chars = new_chars;
		chars[0] = '\0';

		if (item->data.alpha.allow_caps)
			strncat(chars, "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
				chars_size - strlen(chars) - 1);
		if (item->data.alpha.allow_noncaps)
			strncat(chars, "abcdefghijklmnopqrstuvwxyz",
				chars_size - strlen(chars) - 1);
		if (item->data.alpha.allow_numbers)
			strncat(chars, "0123456789", chars_size - strlen(chars) - 1);
		strncat(chars, item->data.alpha.allowed_extra, chars_size - strlen(chars) - 1);

		item->data.alpha.error_code = 0;

		switch (token) {

		// Handle menu key
		case MENUTOKEN_MENU:
			if (pos == 0) {
				return menuitem_predecessor2menuresult(item->predecessor_id,
								       MENURESULT_CLOSE);
			} else {
				menuitem_reset_alpha(item);
			}

			return MENURESULT_NONE;

		// Handle enter key
		case MENUTOKEN_ENTER:
			if ((keymask & MENUTOKEN_RIGHT) ||
			    (str[item->data.alpha.edit_pos] == '\0')) {

				if (strlen(item->data.alpha.edit_str) <
				    item->data.alpha.minlength) {
					item->data.alpha.error_code = 3;

					return MENURESULT_NONE;
				}

				strncpy(item->data.alpha.value, item->data.alpha.edit_str,
					item->data.alpha.maxlength);
				item->data.alpha.value[item->data.alpha.maxlength] = '\0';

				if (item->event_func)
					item->event_func(item, MENUEVENT_UPDATE);

				return menuitem_successor2menuresult(item->successor_id,
								     MENURESULT_CLOSE);

			} else {
				if (pos < item->data.alpha.maxlength) {
					item->data.alpha.edit_pos++;
					if (pos >= display_props->width - 2)
						item->data.alpha.edit_offs++;
				}
			}

			return MENURESULT_NONE;

		// Handle up key
		case MENUTOKEN_UP:
			if (pos >= item->data.alpha.maxlength) {
				item->data.alpha.error_code = 2;
				item->data.alpha.edit_pos = 0;
				item->data.alpha.edit_offs = 0;

				return MENURESULT_NONE;
			}
			if (str[pos] == '\0') {
				str[pos] = chars[0];
			} else {
				p = strchr(chars, str[pos]);
				if (p != NULL) {
					str[pos] = *(++p);
				} else {
					str[pos] = '\0';
				}
			}

			return MENURESULT_NONE;

		// Handle down key
		case MENUTOKEN_DOWN:
			if (pos >= item->data.alpha.maxlength) {
				item->data.alpha.error_code = 2;
				item->data.alpha.edit_pos = 0;
				item->data.alpha.edit_offs = 0;

				return MENURESULT_NONE;
			}

			if (str[pos] == '\0') {
				str[pos] = chars[strlen(chars) - 1];
			} else {
				p = strchr(chars, str[pos]);
				if ((p != NULL) && (p != chars)) {
					str[pos] = *(--p);
				} else {
					str[pos] = '\0';
				}
			}

			return MENURESULT_NONE;

		// Handle right key
		case MENUTOKEN_RIGHT:
			if (str[item->data.alpha.edit_pos] != '\0' &&
			    pos < item->data.alpha.maxlength - 1) {
				item->data.alpha.edit_pos++;
				if (pos >= display_props->width - 2)
					item->data.alpha.edit_offs++;
			}

			return MENURESULT_NONE;

		// Handle left key
		case MENUTOKEN_LEFT:
			if (pos > 0) {
				item->data.alpha.edit_pos--;
				if (item->data.alpha.edit_offs > item->data.alpha.edit_pos)
					item->data.alpha.edit_offs = item->data.alpha.edit_pos;
			}

			return MENURESULT_NONE;

		// Handle other keys
		case MENUTOKEN_OTHER:
			if (pos >= item->data.alpha.maxlength) {
				item->data.alpha.error_code = 2;
				item->data.alpha.edit_pos = 0;
				item->data.alpha.edit_offs = 0;
				return MENURESULT_NONE;
			}
			if ((strlen(key) == 1) && (key[0] >= ' ') &&
			    (strchr(chars, key[0]) != NULL)) {
				str[pos] = key[0];
				item->data.alpha.edit_pos++;
				if (pos >= display_props->width - 2)
					item->data.alpha.edit_offs++;
			}

		default:
			return MENURESULT_NONE;
		}
	}
	return MENURESULT_ERROR;
}

/**
 * \brief Process user input for IP address entry menu item
 * \param item IP menu item
 * \param token Input token (key press, enter, etc.)
 * \param key Key string from input
 * \param keymask Key modifier mask
 * \return MenuResult indicating action taken
 *
 * \details Handles IP address entry with field navigation, digit entry, and validation.
 */
MenuResult menuitem_process_input_ip(MenuItem *item, MenuToken token, const char *key,
				     unsigned int keymask)
{
	char *str = item->data.ip.edit_str;
	char numstr[5];
	int num;
	int pos = item->data.ip.edit_pos;
	const IpSstringProperties *ipinfo = (item->data.ip.v6) ? &IPinfo[1] : &IPinfo[0];

	debug(RPT_DEBUG, "%s(item=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), token, key);

	item->data.ip.error_code = 0;

	switch (token) {

	// Handle menu key
	case MENUTOKEN_MENU:
		if (pos == 0) {
			return menuitem_predecessor2menuresult(item->predecessor_id,
							       MENURESULT_CLOSE);
		} else {
			menuitem_reset_ip(item);
		}

		return MENURESULT_NONE;

	// Handle enter key
	case MENUTOKEN_ENTER:
		if ((keymask & MENUTOKEN_RIGHT) || (pos >= item->data.ip.maxlength - 1)) {
			char tmp[40];
			char *start = tmp;

			memccpy(tmp, str, '\0', sizeof(tmp));

			while (start != NULL) {
				char *skip = start;

				while ((*skip == ' ') ||
				       ((*skip == '0') && (skip[1] != ipinfo->sep) &&
					(skip[1] != '\0')))
					skip++;
				memccpy(start, skip, '\0', item->data.ip.maxlength + 1);
				skip = strchr(start, ipinfo->sep);
				start = (skip != NULL) ? (skip + 1) : NULL;
			}

			if ((ipinfo->verify != NULL) && (!ipinfo->verify(tmp))) {
				report(RPT_WARNING, "%s(id=\"%s\") ip address not verified: \"%s\"",
				       __FUNCTION__, item->id, tmp);
				item->data.ip.error_code = 4;

				return MENURESULT_NONE;
			}

			strncpy(item->data.ip.value, tmp, item->data.ip.maxlength);
			item->data.ip.value[item->data.ip.maxlength] = '\0';

			if (item->event_func)
				item->event_func(item, MENUEVENT_UPDATE);

			return menuitem_successor2menuresult(item->successor_id, MENURESULT_CLOSE);

		} else {
			item->data.ip.edit_pos++;
			if (str[item->data.ip.edit_pos] == ipinfo->sep)
				item->data.ip.edit_pos++;
			while (item->data.ip.edit_pos - item->data.ip.edit_offs >
			       display_props->width - 2)
				item->data.ip.edit_offs++;

			return MENURESULT_NONE;
		}

	// Handle up key
	case MENUTOKEN_UP:
		num = (int)strtol(&str[pos - (pos % (ipinfo->width + 1))], (char **)NULL,
				  ipinfo->base);
		num += ipinfo->posValue[(pos - (pos / (ipinfo->width + 1))) % ipinfo->width];
		if (num > ipinfo->limit)
			num = 0;
		snprintf(numstr, 5, ipinfo->format, num);
		memcpy(&str[pos - (pos % (ipinfo->width + 1))], numstr, ipinfo->width);

		return MENURESULT_NONE;

	// Handle down key
	case MENUTOKEN_DOWN:
		num = (int)strtol(&str[pos - (pos % (ipinfo->width + 1))], (char **)NULL,
				  ipinfo->base);
		num -= ipinfo->posValue[(pos - (pos / (ipinfo->width + 1))) % ipinfo->width];
		if (num < 0)
			num = ipinfo->limit;
		snprintf(numstr, 5, ipinfo->format, num);
		memcpy(&str[pos - (pos % (ipinfo->width + 1))], numstr, ipinfo->width);

		return MENURESULT_NONE;

	// Handle right key
	case MENUTOKEN_RIGHT:
		if (pos < item->data.ip.maxlength - 1) {
			item->data.ip.edit_pos++;
			if (str[item->data.ip.edit_pos] == ipinfo->sep)
				item->data.ip.edit_pos++;
			while (item->data.ip.edit_pos - item->data.ip.edit_offs >
			       display_props->width - 2)
				item->data.ip.edit_offs++;
		}

		return MENURESULT_NONE;

	// Handle left key
	case MENUTOKEN_LEFT:
		if (pos > 0) {
			item->data.ip.edit_pos--;
			if (str[item->data.ip.edit_pos] == ipinfo->sep)
				item->data.ip.edit_pos--;
			if (item->data.ip.edit_offs > item->data.ip.edit_pos)
				item->data.ip.edit_offs = item->data.ip.edit_pos;
		}

		return MENURESULT_NONE;

	// Handle other keys
	case MENUTOKEN_OTHER:
		if ((strlen(key) == 1) &&
		    ((item->data.ip.v6) ? isxdigit(key[0]) : isdigit(key[0]))) {
			str[pos] = tolower(key[0]);
			if (pos < item->data.ip.maxlength - 1) {
				item->data.ip.edit_pos++;
				if (str[item->data.ip.edit_pos] == ipinfo->sep)
					item->data.ip.edit_pos++;
				while (item->data.ip.edit_pos - item->data.ip.edit_offs >
				       display_props->width - 2)
					item->data.ip.edit_offs++;
			}
		}
	// FALLTHROUGH
	default:
		return MENURESULT_NONE;
	}
	return MENURESULT_ERROR;
}

// Return the Client that owns the MenuItem
Client *menuitem_get_client(MenuItem *item) { return item->client; }

// Convert tab-separated string to LinkedList
LinkedList *tablist2linkedlist(char *strings)
{
	LinkedList *list;

	list = LL_new();

	if (strings != NULL) {
		char *p = strings;
		char *tabptr, *new_s;

		while ((tabptr = strchr(p, '\t')) != NULL) {
			int len = (int)(tabptr - p);

			new_s = malloc(len + 1);
			if (new_s != NULL) {
				strncpy(new_s, p, len);
				new_s[len] = 0;

				LL_Push(list, new_s);
			}

			p = tabptr + 1;
		}
		new_s = strdup(p);
		if (new_s != NULL)
			LL_Push(list, new_s);
	}

	return list;
}

// Convert menu item type name to MenuItemType enum
MenuItemType menuitem_typename_to_type(char *name)
{
	if (name != NULL) {
		MenuItemType type;

		for (type = 0; type < NUM_ITEMTYPES; type++) {
			if (strcmp(menuitemtypenames[type], name) == 0) {
				return type;
			}
		}
	}

	return MENUITEM_INVALID;
}

// Convert MenuItemType enum to type name string
char *menuitem_type_to_typename(MenuItemType type)
{
	return ((type >= 0 && type < NUM_ITEMTYPES) ? menuitemtypenames[type] : NULL);
}

// Convert menu event type name to MenuEventType enum
MenuEventType menuitem_eventtypename_to_eventtype(char *name)
{
	if (name != NULL) {
		MenuEventType type;

		for (type = 0; type < NUM_EVENTTYPES; type++) {
			if (strcmp(menueventtypenames[type], name) == 0) {
				return type;
			}
		}
	}

	return MENUEVENT_INVALID;
}

// Convert MenuEventType enum to event type name string
char *menuitem_eventtype_to_eventtypename(MenuEventType type)
{
	return ((type >= 0 && type < NUM_EVENTTYPES) ? menueventtypenames[type] : NULL);
}
