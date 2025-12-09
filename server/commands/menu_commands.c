// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/menu_commands.c
 * \brief Implementation of menu system command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn, Peter Marschall, n0vedad
 * \date 1999-2025
 *
 * \features
 * - Hierarchical menu structure with parent-child relationships
 * - Multiple item types: menu, action, checkbox, ring, slider, numeric, alpha, IP
 * - Dynamic menu creation and modification during runtime
 * - Client-specific menu management and isolation
 * - Event-driven navigation and interaction handling
 * - Comprehensive option table for menu item configuration
 * - Menu validation and error handling with detailed error messages
 * - Automatic client menu creation when first item is added
 * - Menu cleanup when last item is removed
 * - Navigation with predecessor/successor relationships
 * - Special menu actions (_quit_, _close_, _none_)
 *
 * \usage
 * - Used by the LCDd server protocol parser for menu command dispatch
 * - Functions are called when clients send menu-related commands
 * - Provides interactive menu system for LCD display navigation
 * - Used for client application configuration and control interfaces
 * - Handles menu event callbacks and client notification
 *
 * \details
 * Implements handlers for client commands concerning the interactive
 * menu system. These functions process menu-related protocol commands and
 * manage the hierarchical menu structure for client applications.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared/report.h"
#include "shared/sockets.h"

#include "client.h"
#include "menu.h"
#include "menu_commands.h"
#include "menuitem.h"
#include "menuscreens.h"

// Internal function declarations: menu event handler and functions to set predecessor/successor
/** @cond */
MenuEventFunc(menu_commands_handler);
/** @endcond */
int set_predecessor(MenuItem *item, char *itemid, Client *client);
int set_successor(MenuItem *item, char *itemid, Client *client);

// Handle menu_add_item command for creating menu items
int menu_add_item_func(Client *c, int argc, char **argv)
{
	char *menu_id;
	char *item_id;
	char *text = NULL;
	Menu *menu = NULL;
	MenuItem *item;
	MenuItemType itemtype;

	debug(RPT_DEBUG, "%s(Client [%d], %s, %s)", __FUNCTION__, c->sock, argv[1], argv[2]);

	if (c->state != ACTIVE)
		return 1;

	if (c->name == NULL) {
		sock_send_error(c->sock, "You need to give your client a name first\n");
		return 0;
	}

	if (argc < 4) {
		sock_send_error(
		    c->sock,
		    "Usage: menu_add_item <menuid> <newitemid> <type> [<text>] [<option>]+\n");
		return 0;
	}

	menu_id = argv[1];
	item_id = argv[2];

	// Automatic client menu creation on first item addition
	if (c->menu == NULL) {
		report(RPT_INFO, "Client [%d] is using the menu", c->sock);
		c->menu = menu_create("_client_menu_", menu_commands_handler, c->name, c);
		if (c->menu == NULL) {
			sock_send_error(c->sock, "Cannot create menu\n");
			return 1;
		}
		menu_add_item(main_menu, c->menu);
	}

	// Use either the given menu or the client's main menu if none was specified
	menu = (menu_id[0] != '\0') ? menu_find_item(c->menu, menu_id, true) : c->menu;
	if (menu == NULL) {
		sock_send_error(c->sock, "Cannot find menu id\n");
		return 0;
	}

	item = menu_find_item(c->menu, item_id, true);
	if (item != NULL) {
		sock_printf_error(c->sock, "Item id '%s' already in use\n", item_id);
		return 0;
	}

	itemtype = menuitem_typename_to_type(argv[3]);
	if (itemtype == MENUITEM_INVALID) {
		sock_send_error(c->sock, "Invalid menuitem type\n");
		return 0;
	}

	// Text parameter: use empty string if not provided or starts with '-'
	if ((argc >= 5) && (argv[4][0] != '-')) {
		text = argv[4];
	} else {
		text = "";
	}

	// Create menu item with type-specific defaults
	switch (itemtype) {

	// Create submenu container
	case MENUITEM_MENU:
		item = menu_create(item_id, menu_commands_handler, text, c);
		break;

	// Create action item with no default result
	case MENUITEM_ACTION:
		item = menuitem_create_action(item_id, menu_commands_handler, text, c,
					      MENURESULT_NONE);
		break;

	// Create checkbox: unchecked, no gray state
	case MENUITEM_CHECKBOX:
		item =
		    menuitem_create_checkbox(item_id, menu_commands_handler, text, c, false, false);
		break;

	// Create ring: empty, first item selected
	case MENUITEM_RING:
		item = menuitem_create_ring(item_id, menu_commands_handler, text, c, "", 0);
		break;

	// Create slider: 0-100, step 1, position 25
	case MENUITEM_SLIDER:
		item = menuitem_create_slider(item_id, menu_commands_handler, text, c, "", "", 0,
					      100, 1, 25);
		break;

	// Create numeric input: range 0-100, starts at 0
	case MENUITEM_NUMERIC:
		item = menuitem_create_numeric(item_id, menu_commands_handler, text, c, 0, 100, 0);
		break;

	// Create text input: max 10 chars, caps/numbers, common symbols
	case MENUITEM_ALPHA:
		item = menuitem_create_alpha(item_id, menu_commands_handler, text, c, 0, 0, 10,
					     true, false, true, "-./", "");
		break;

	// Create IP address input: IPv4, example placeholder
	case MENUITEM_IP:
		item =
		    menuitem_create_ip(item_id, menu_commands_handler, text, c, 0, "192.168.1.245");
		break;

	// This should never happen
	default:
		assert(!"unexpected menuitem type");
	}

	menu_add_item(menu, item);
	menuscreen_inform_item_modified(menu);

	// Delegate remaining options to menu_set_item for processing
	if ((argc > 5) || ((argc == 5) && (argv[4][0] == '-'))) {
		int i, j;
		char **tmp_argv = (char **)malloc(argc * sizeof(char *));

		assert(tmp_argv);
		tmp_argv[0] = "menu_set_item";

		for (i = j = 1; i < argc; i++) {
			// Skip "type" parameter
			if (i == 3)
				continue;

			// Skip "text" parameter if present
			if ((i == 4) && (argv[4][0] != '-'))
				continue;

			tmp_argv[j++] = argv[i];
		}
		menu_set_item_func(c, j, tmp_argv);
		free((void *)tmp_argv);
	} else
		sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle menu_del_item command for removing menu items
int menu_del_item_func(Client *c, int argc, char **argv)
{
	MenuItem *item;
	char *item_id;

	if (c->state != ACTIVE)
		return 1;

	if (argc != 3 && argc != 2) {
		sock_send_error(c->sock, "Usage: menu_del_item [ignored] <itemid>\n");
		return 0;
	}

	item_id = argv[argc - 1];

	if (c->menu == NULL) {
		sock_send_error(c->sock, "Client has no menu\n");
		return 0;
	}

	item = menu_find_item(c->menu, item_id, true);
	if (item == NULL) {
		sock_send_error(c->sock, "Cannot find item\n");
		return 0;
	}
	menuscreen_inform_item_destruction(item);
	menu_remove_item(item->parent, item);
	menuscreen_inform_item_modified(item->parent);
	menuitem_destroy(item);

	// Automatic menu cleanup when last item removed
	if (menu_getfirst_item(c->menu) == NULL) {
		menuscreen_inform_item_destruction(c->menu);
		menu_remove_item(main_menu, c->menu);
		menuscreen_inform_item_modified(main_menu);
		menu_destroy(c->menu);
		c->menu = NULL;
	}
	sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle menu_set_item command for modifying menu item properties
int menu_set_item_func(Client *c, int argc, char **argv)
{
	typedef enum AttrType {
		NOVALUE,
		BOOLEAN,
		CHECKBOX_VALUE,
		SHORT,
		INT,
		FLOAT,
		STRING
	} AttrType;

	// Menu item attribute mapping table: defines all configurable properties for each menu item
	// type with attribute name, type, and struct field offset for direct memory access
	struct OptionTable {
		MenuItemType menuitem_type;
		char *name;
		AttrType attr_type;
		int attr_offset;
	} option_table[] = {
	    {-1, "text", STRING, offsetof(MenuItem, text)},
	    {-1, "is_hidden", BOOLEAN, offsetof(MenuItem, is_hidden)},
	    {-1, "prev", STRING, -1},
	    {-1, "next", STRING, -1},

	    {MENUITEM_ACTION, "menu_result", STRING, -1},
	    {MENUITEM_CHECKBOX, "value", CHECKBOX_VALUE, offsetof(MenuItem, data.checkbox.value)},
	    {MENUITEM_CHECKBOX, "allow_gray", BOOLEAN,
	     offsetof(MenuItem, data.checkbox.allow_gray)},

	    {MENUITEM_RING, "value", SHORT, offsetof(MenuItem, data.ring.value)},
	    {MENUITEM_RING, "strings", STRING, -1},
	    {MENUITEM_SLIDER, "value", INT, offsetof(MenuItem, data.slider.value)},
	    {MENUITEM_SLIDER, "minvalue", INT, offsetof(MenuItem, data.slider.minvalue)},
	    {MENUITEM_SLIDER, "maxvalue", INT, offsetof(MenuItem, data.slider.maxvalue)},
	    {MENUITEM_SLIDER, "stepsize", INT, offsetof(MenuItem, data.slider.stepsize)},
	    {MENUITEM_SLIDER, "mintext", STRING, offsetof(MenuItem, data.slider.mintext)},
	    {MENUITEM_SLIDER, "maxtext", STRING, offsetof(MenuItem, data.slider.maxtext)},
	    {MENUITEM_NUMERIC, "value", INT, offsetof(MenuItem, data.numeric.value)},
	    {MENUITEM_NUMERIC, "minvalue", INT, offsetof(MenuItem, data.numeric.minvalue)},
	    {MENUITEM_NUMERIC, "maxvalue", INT, offsetof(MenuItem, data.numeric.maxvalue)},

	    {MENUITEM_ALPHA, "value", STRING, -1},
	    {MENUITEM_ALPHA, "minlength", SHORT, offsetof(MenuItem, data.alpha.minlength)},
	    {MENUITEM_ALPHA, "maxlength", SHORT, offsetof(MenuItem, data.alpha.maxlength)},
	    {MENUITEM_ALPHA, "password_char", STRING, -1},
	    {MENUITEM_ALPHA, "allow_caps", BOOLEAN, offsetof(MenuItem, data.alpha.allow_caps)},
	    {MENUITEM_ALPHA, "allow_noncaps", BOOLEAN,
	     offsetof(MenuItem, data.alpha.allow_noncaps)},

	    {MENUITEM_ALPHA, "allow_numbers", BOOLEAN,
	     offsetof(MenuItem, data.alpha.allow_numbers)},
	    {MENUITEM_ALPHA, "allowed_extra", STRING, offsetof(MenuItem, data.alpha.allowed_extra)},
	    {MENUITEM_IP, "v6", BOOLEAN, offsetof(MenuItem, data.ip.v6)},

	    {MENUITEM_IP, "value", STRING, -1},
	    {0, NULL, NOVALUE, -1}};

	bool bool_value = false;
	CheckboxValue checkbox_value = CHECKBOX_OFF;
	short short_value = 0;
	int int_value = 0;
	float float_value = 0;
	char *string_value = NULL;

	MenuItem *item;
	char *item_id;
	int argnr;

	if (c->state != ACTIVE)
		return 1;

	if (argc < 4) {
		sock_send_error(c->sock, "Usage: menu_set_item "
					 " <itemid> {<option>}+\n");
		return 0;
	}

	item_id = argv[2];

	item = menu_find_item(c->menu, item_id, true);
	if (item == NULL) {
		sock_send_error(c->sock, "Cannot find item\n");
		return 0;
	}

	// Process all option arguments
	for (argnr = 3; argnr < argc; argnr++) {
		int option_nr = -1;
		int found_option_name = 0;
		int error = 0;
		void *location;
		char *p;

		// Find the option in the table
		if (argv[argnr][0] == '-') {
			int i;

			for (i = 0; option_table[i].name != NULL; i++) {
				if (strcmp(argv[argnr] + 1, option_table[i].name) == 0) {
					found_option_name = 1;
					if (item->type == option_table[i].menuitem_type ||
					    option_table[i].menuitem_type == -1) {
						option_nr = i;
					}
				}
			}

		} else {
			sock_printf_error(c->sock, "Found non-option: \"%.40s\"\n", argv[argnr]);
			continue;
		}
		if (option_nr == -1) {
			if (found_option_name) {
				sock_printf_error(c->sock,
						  "Option not valid for menuitem type: \"%.40s\"\n",
						  argv[argnr]);
			} else {
				sock_printf_error(c->sock, "Unknown option: \"%.40s\"\n",
						  argv[argnr]);
			}
			continue;
		}

		// Check for value
		if (option_table[option_nr].attr_type != NOVALUE) {
			if (argnr + 1 >= argc) {
				sock_printf_error(c->sock, "Missing value at option: \"%.40s\"\n",
						  argv[argnr]);
				continue;
			}
		}
		location = (void *)item + option_table[option_nr].attr_offset;

		// Parse value based on type
		switch (option_table[option_nr].attr_type) {

		// Options without values
		case NOVALUE:
			break;

		// Boolean options accept "true" or "false"
		case BOOLEAN:
			if (strcmp(argv[argnr + 1], "false") == 0) {
				bool_value = false;
			} else if (strcmp(argv[argnr + 1], "true") == 0) {
				bool_value = true;
			} else {
				error = 1;
				break;
			}
			if (option_table[option_nr].attr_offset != -1) {
				*(bool *)location = bool_value;
			}
			break;

		// Checkbox values support "off", "on", and "gray"
		case CHECKBOX_VALUE:
			if (strcmp(argv[argnr + 1], "off") == 0) {
				checkbox_value = CHECKBOX_OFF;
			} else if (strcmp(argv[argnr + 1], "on") == 0) {
				checkbox_value = CHECKBOX_ON;
			} else if (strcmp(argv[argnr + 1], "gray") == 0) {
				checkbox_value = CHECKBOX_GRAY;
			} else {
				error = 1;
				break;
			}
			if (option_table[option_nr].attr_offset != -1) {
				*(CheckboxValue *)location = checkbox_value;
			}
			break;

		// Short integer values parsed with strtol()
		case SHORT:
			short_value = strtol(argv[argnr + 1], &p, 0);
			if ((argv[argnr + 1][0] == '\0') || (*p != '\0')) {
				error = 1;
				break;
			}
			if (option_table[option_nr].attr_offset != -1) {
				*(short *)location = short_value;
			}
			break;

		// Integer values parsed with strtol()
		case INT:
			int_value = strtol(argv[argnr + 1], &p, 0);
			if ((argv[argnr + 1][0] == '\0') || (*p != '\0')) {
				error = 1;
				break;
			}
			if (option_table[option_nr].attr_offset != -1) {
				*(int *)location = int_value;
			}
			break;

		// Float values parsed with strtod()
		case FLOAT:
			float_value = strtod(argv[argnr + 1], &p);
			if ((argv[argnr + 1][0] == '\0') || (*p != '\0')) {
				error = 1;
				break;
			}
			if (option_table[option_nr].attr_offset != -1) {
				*(float *)location = float_value;
			}
			break;

		// String values handle memory allocation and navigation options
		case STRING:
			string_value = argv[argnr + 1];
			if (option_table[option_nr].attr_offset != -1) {
				free(*(char **)location);
				*(char **)location = strdup(string_value);
			} else if (strcmp(argv[argnr], "-prev") == 0) {
				set_predecessor(item, string_value, c);
			} else if (strcmp(argv[argnr], "-next") == 0) {
				set_successor(item, string_value, c);
			}
			break;
		}

		switch (error) {

		// Value parsing error occurred
		case 1:
			sock_printf_error(c->sock,
					  "Could not interpret value at option: \"%.40s\"\n",
					  argv[argnr]);
			argnr++;
			continue;

		// No error or unknown error code
		default:
			break;
		}

		// Item-specific post-processing
		switch (item->type) {

		// Action items process menu_result parameter
		case MENUITEM_ACTION:
			if (strcmp(argv[argnr] + 1, "menu_result") == 0) {
				if (strcmp(argv[argnr + 1], "none") == 0) {
					set_successor(item, "_none_", c);
				} else if (strcmp(argv[argnr + 1], "close") == 0) {
					set_successor(item, "_close_", c);
				} else if (strcmp(argv[argnr + 1], "quit") == 0) {
					set_successor(item, "_quit_", c);
				} else {
					error = 1;
				}
			}
			break;

		// Slider items validate and clamp values to range
		case MENUITEM_SLIDER:
			if (item->data.slider.value < item->data.slider.minvalue) {
				item->data.slider.value = item->data.slider.minvalue;
			} else if (item->data.slider.value > item->data.slider.maxvalue) {
				item->data.slider.value = item->data.slider.maxvalue;
			}
			break;

		// Ring items process string lists and validate index
		case MENUITEM_RING:
			if (strcmp(argv[argnr] + 1, "strings") == 0) {
				free(item->data.ring.strings);
				item->data.ring.strings = tablist2linkedlist(string_value);
			}
			item->data.ring.value %= LL_Length(item->data.ring.strings);
			break;

		// Numeric items reset cursor position
		case MENUITEM_NUMERIC:
			menuitem_reset(item);
			break;

		// Alpha items handle password masking and buffer reallocation
		case MENUITEM_ALPHA:
			if (strcmp(argv[argnr] + 1, "password_char") == 0) {
				item->data.alpha.password_char = string_value[0];
			} else if (strcmp(argv[argnr] + 1, "maxlength") == 0) {
				char *new_buf;
				if ((short_value < 0) || (short_value > 1000)) {
					error = 2;
					break;
				}
				new_buf = malloc(short_value + 1);
				strncpy(new_buf, item->data.alpha.value, short_value);
				new_buf[short_value] = '\0';
				free(item->data.alpha.value);
				item->data.alpha.value = new_buf;
				free(item->data.alpha.edit_str);
				item->data.alpha.edit_str = malloc(short_value + 1);
				item->data.alpha.edit_str[0] = '\0';

			} else if (strcmp(argv[argnr] + 1, "value") == 0) {
				strncpy(item->data.alpha.value, string_value,
					item->data.alpha.maxlength);
				item->data.alpha.value[item->data.alpha.maxlength] = 0;
			}
			menuitem_reset(item);
			break;

		// IP items adjust buffer size for IPv4/IPv6 format
		case MENUITEM_IP:
			if (strcmp(argv[argnr] + 1, "v6") == 0) {
				char *new_buf;
				item->data.ip.maxlength = (bool_value == 0) ? 15 : 39;

				new_buf = malloc(item->data.ip.maxlength + 1);
				strncpy(new_buf, item->data.ip.value, item->data.ip.maxlength);
				new_buf[item->data.ip.maxlength] = '\0';
				free(item->data.ip.value);
				item->data.ip.value = new_buf;
				free(item->data.ip.edit_str);
				item->data.ip.edit_str = malloc(item->data.ip.maxlength + 1);
				item->data.ip.edit_str[0] = '\0';

			} else if (strcmp(argv[argnr] + 1, "value") == 0) {
				strncpy(item->data.ip.value, string_value, item->data.ip.maxlength);
				item->data.ip.value[item->data.ip.maxlength] = '\0';
			}
			menuitem_reset(item);
			break;

		// Unhandled menu item types
		default:
			break;
		}
		switch (error) {

		// Value interpretation error
		case 1:
			sock_printf_error(c->sock,
					  "Could not interpret value at option: \"%.40s\"\n",
					  argv[argnr]);
			continue;

		// Value out of range error
		case 2:
			sock_printf_error(c->sock, "Value out of range at option: \"%.40s\"\n",
					  argv[argnr]);
			argnr++;
			continue;

		// No error or unknown error code
		default:
			break;
		}

		menuscreen_inform_item_modified(item);
		if (option_table[option_nr].attr_type != NOVALUE) {
			argnr++;
		}
	}
	sock_send_string(c->sock, "success\n");

	return 0;
}

// Handle menu_goto command for menu navigation
int menu_goto_func(Client *c, int argc, char **argv)
{
	char *menu_id;
	Menu *menu;

	debug(RPT_DEBUG, "%s(Client [%d], %s, %s)", __FUNCTION__, c->sock,
	      ((argc > 1) ? argv[1] : "<null>"), ((argc > 2) ? argv[2] : "<null>"));

	if (c->state != ACTIVE)
		return 1;

	if ((argc < 2) || (argc > 3)) {
		sock_send_error(c->sock, "Usage: menu_goto <menuid> [<predecessor_id>]\n");
		return 0;
	}

	menu_id = argv[1];

	if (strcmp("_quit_", menu_id) == 0) {
		menu = NULL;
	} else {
		menu = (menu_id[0] != '\0') ? menuitem_search(menu_id, c) : c->menu;
		if (menu == NULL) {
			sock_send_error(c->sock, "Cannot find menu id\n");
			return 0;
		}

		if (argc > 2)
			set_predecessor(menu, argv[2], c);
	}

	menuscreen_goto(menu);
	sock_send_string(c->sock, "success\n");

	return 0;
}

/**
 * \brief Set the predecessor of a menu item for wizard navigation
 * \param item Menu item to configure
 * \param itemid ID of predecessor item (or "_quit_", "_close_", "_none_")
 * \param c Client owning the menu item
 * \retval 0 Predecessor set successfully
 * \retval -1 Error setting predecessor (item not found)
 *
 * \details Configures menu navigation for wizard-style menus. Supports special
 * navigation commands (_quit_, _close_, _none_) and validates that predecessor
 * menu item exists.
 */
int set_predecessor(MenuItem *item, char *itemid, Client *c)
{
	assert(item != NULL);
	debug(RPT_DEBUG, "%s(%s, %s, %d)", __FUNCTION__, item->id, itemid, c->sock);

	// Validate special navigation commands
	if ((strcmp("_quit_", itemid) != 0) && (strcmp("_close_", itemid) != 0) &&
	    (strcmp("_none_", itemid) != 0)) {
		MenuItem *predecessor = menuitem_search(itemid, c);

		if (predecessor == NULL) {
			sock_printf_error(c->sock,
					  "Cannot find predecessor '%s'"
					  " for item '%s'\n",
					  itemid, item->id);
			return -1;
		}
	}

	debug(RPT_DEBUG,
	      "%s(Client [%d], ...)"
	      " setting '%s's predecessor from '%s' to '%s'",
	      __FUNCTION__, c->sock, item->id, item->predecessor_id, itemid);

	if (item->predecessor_id != NULL)
		free(item->predecessor_id);
	item->predecessor_id = strdup(itemid);

	return 0;
}

/**
 * \brief Set the successor of a menu item for wizard navigation
 * \param item Menu item to configure
 * \param itemid ID of successor item (or "_quit_", "_close_", "_none_")
 * \param c Client owning the menu item
 * \retval 0 Successor set successfully
 * \retval -1 Error setting successor (item not found)
 *
 * \details Configures forward navigation for wizard-style menus. Supports special
 * navigation commands (_quit_, _close_, _none_) and validates that successor
 * menu item exists.
 */
int set_successor(MenuItem *item, char *itemid, Client *c)
{
	assert(item != NULL);
	debug(RPT_DEBUG, "%s(%s, %s, %d)", __FUNCTION__, item->id, itemid, c->sock);

	// Validate special navigation commands
	if ((strcmp("_quit_", itemid) != 0) && (strcmp("_close_", itemid) != 0) &&
	    (strcmp("_none_", itemid) != 0)) {
		MenuItem *successor = menuitem_search(itemid, c);

		if (successor == NULL) {
			sock_printf_error(c->sock,
					  "Cannot find successor '%s'"
					  " for item '%s'\n",
					  itemid, item->id);
			return -1;
		}
	}

	// Menu items cannot have successors
	if (item->type == MENUITEM_MENU) {
		sock_printf_error(c->sock,
				  "Cannot set successor of '%s':"
				  " wrong type '%s'\n",
				  item->id, menuitem_type_to_typename(item->type));
		return -1;
	}
	debug(RPT_DEBUG,
	      "%s(Client [%d], ...)"
	      " setting '%s's successor from '%s' to '%s'",
	      __FUNCTION__, c->sock, item->id, item->successor_id, itemid);

	if (item->successor_id != NULL)
		free(item->successor_id);
	item->successor_id = strdup(itemid);

	return 0;
}

// Handle menu_set_main command for setting the root menu
int menu_set_main_func(Client *c, int argc, char **argv)
{
	char *menu_id;
	Menu *menu;

	debug(RPT_DEBUG, "%s(Client [%d], %s, %s)", __FUNCTION__, c->sock,
	      ((argc > 1) ? argv[1] : "<null>"), ((argc > 2) ? argv[2] : "<null>"));

	if (c->state != ACTIVE)
		return 1;

	if (argc != 2) {
		sock_send_error(c->sock, "Usage: menu_set_main <menuid>\n");
		return 0;
	}

	menu_id = argv[1];

	if (menu_id[0] == '\0') {
		menu = c->menu;
	} else if (strcmp(menu_id, "_main_") == 0) {
		menu = NULL;
	} else {
		menu = menu_find_item(c->menu, menu_id, true);
		if (menu == NULL) {
			sock_send_error(c->sock, "Cannot find menu id\n");
			return 0;
		}
	}

	menuscreen_set_main(menu);
	sock_send_string(c->sock, "success\n");

	return 0;
}

/** @cond */
MenuEventFunc(menu_commands_handler)
/** @endcond */
{
	/** @cond */
	Client *c;

	c = menuitem_get_client(item);
	if (c == NULL) {
		report(RPT_ERR, "%s: Could not find client of item \"%s\"", __FUNCTION__, item->id);
		return -1;
	}

	// Send event-specific messages to client
	if ((event == MENUEVENT_UPDATE) || (event == MENUEVENT_MINUS) ||
	    (event == MENUEVENT_PLUS)) {
		switch (item->type) {

		// Checkbox events report current state as text
		case MENUITEM_CHECKBOX:
			sock_printf(c->sock, "menuevent %s %.40s %s\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    ((char *[]){"off", "on", "gray"})[item->data.checkbox.value]);
			break;

		// Slider events report current numeric value
		case MENUITEM_SLIDER:
			sock_printf(c->sock, "menuevent %s %.40s %d\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    item->data.slider.value);
			break;

		// Ring events report selected index
		case MENUITEM_RING:
			sock_printf(c->sock, "menuevent %s %.40s %d\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    item->data.ring.value);
			break;

		// Numeric events report current integer value
		case MENUITEM_NUMERIC:
			sock_printf(c->sock, "menuevent %s %.40s %d\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    item->data.numeric.value);
			break;

		// Alpha events report current text string
		case MENUITEM_ALPHA:
			sock_printf(c->sock, "menuevent %s %.40s %.40s\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    item->data.alpha.value);
			break;

		// IP events report current IP address string
		case MENUITEM_IP:
			sock_printf(c->sock, "menuevent %s %.40s %.40s\n",
				    menuitem_eventtype_to_eventtypename(event), item->id,
				    item->data.ip.value);
			break;

		// Default events for unsupported item types
		default:
			sock_printf(c->sock, "menuevent %s %.40s\n",
				    menuitem_eventtype_to_eventtypename(event), item->id);
		}

		// MENUEVENT_ENTER, MENUEVENT_LEAVE, and other events without specific values
	} else {
		sock_printf(c->sock, "menuevent %s %.40s\n",
			    menuitem_eventtype_to_eventtypename(event), item->id);
	}

	return 0;
	/** @endcond */
}
