// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/menuscreens.c
 * \brief Menu screen creation and keyboard handling implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \date 1999-2005
 *
 * \features
 * - Server menu screen creation and management
 * - Menu creation and organization
 * - Keyboard input processing and token conversion
 * - Menu navigation and item selection
 * - Screen addition and removal from menu system
 * - Custom menu support
 *
 * \usage
 * - Converts key presses to standardized menu tokens
 * - Manages menu hierarchy and navigation
 * - Handles screen-specific menu items
 * - Supports both built-in and custom menus
 *
 * \details Creates the server menu screen(s) and creates the menus that should be
 * displayed on this screen. It also handles key presses and converts them to menu
 * tokens for easier processing. menuscreens.c does not know whether a menuitem is
 * displayed INSIDE a menu or on a separate SCREEN, for flexibility.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver.h"
#include "drivers.h"
#include "input.h"
#include "menuscreens.h"
#include "render.h"
#include "screen.h"
#include "screenlist.h"

#include "shared/configfile.h"
#include "shared/report.h"

/** \name Menu Key Bindings
 * Configurable key names for menu navigation
 */
///@{
char *menu_key;		     ///< Key to enter/exit menu system
char *enter_key;	     ///< Key to select/activate menu item
char *up_key;		     ///< Key to navigate up in menu
char *down_key;		     ///< Key to navigate down in menu
char *left_key;		     ///< Key to decrease value or go back
char *right_key;	     ///< Key to increase value or go forward
static unsigned int keymask; ///< Bitmask for simultaneous key press detection
///@}

/** \name Global Menu System State
 * Screen, active item, and menu hierarchy references
 */
///@{
Screen *menuscreen = NULL;	  ///< Screen object for menu display
MenuItem *active_menuitem = NULL; ///< Currently selected menu item
Menu *main_menu = NULL;		  ///< Root of main menu tree
Menu *custom_main_menu = NULL;	  ///< Root of custom client menu tree
Menu *screens_menu = NULL;	  ///< Menu for screen management
///@}

// Internal menu navigation and action handlers for menu result processing and screen transitions
static void handle_quit(void);
static void handle_close(void);
static void handle_none(void);
static void handle_enter(void);
static void handle_successor(void);
void menuscreen_switch_item(MenuItem *new_menuitem);
void menuscreen_create_menu(void);

#ifdef LCDPROC_TESTMENUS
void menuscreen_create_testmenu(void);
#endif

// Internal function hidden from Doxygen documentation
/** @cond */
Menu *menuscreen_get_main(void);
/** @endcond */
// Internal menu event handlers hidden from Doxygen documentation
/** @cond */
MenuEventFunc(heartbeat_handler);
MenuEventFunc(backlight_handler);
MenuEventFunc(titlespeed_handler);
MenuEventFunc(contrast_handler);
MenuEventFunc(brightness_handler);
/** @endcond */

// Initialize the menu screen system
int menuscreens_init(void)
{
	const char *tmp;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	menu_permissive_goto = config_get_bool("menu", "PermissiveGoto", 0, 0);

	keymask = 0;
	menu_key = enter_key = NULL;
	tmp = config_get_string("menu", "MenuKey", 0, NULL);

	if (tmp != NULL) {
		menu_key = strdup(tmp);
		keymask |= MENUTOKEN_MENU;
	}

	tmp = config_get_string("menu", "EnterKey", 0, NULL);
	if (tmp != NULL) {
		enter_key = strdup(tmp);
		keymask |= MENUTOKEN_ENTER;
	}

	up_key = down_key = NULL;
	tmp = config_get_string("menu", "UpKey", 0, NULL);
	if (tmp != NULL) {
		up_key = strdup(tmp);
		keymask |= MENUTOKEN_UP;
	}

	tmp = config_get_string("menu", "DownKey", 0, NULL);
	if (tmp != NULL) {
		down_key = strdup(tmp);
		keymask |= MENUTOKEN_DOWN;
	}

	left_key = right_key = NULL;
	tmp = config_get_string("menu", "LeftKey", 0, NULL);
	if (tmp != NULL) {
		left_key = strdup(tmp);
		keymask |= MENUTOKEN_LEFT;
	}

	tmp = config_get_string("menu", "RightKey", 0, NULL);
	if (tmp != NULL) {
		right_key = strdup(tmp);
		keymask |= MENUTOKEN_RIGHT;
	}

	if (menu_key != NULL)
		input_reserve_key(menu_key, true, NULL);
	if (enter_key != NULL)
		input_reserve_key(enter_key, false, NULL);
	if (up_key != NULL)
		input_reserve_key(up_key, false, NULL);
	if (down_key != NULL)
		input_reserve_key(down_key, false, NULL);
	if (left_key != NULL)
		input_reserve_key(left_key, false, NULL);
	if (right_key != NULL)
		input_reserve_key(right_key, false, NULL);

	menuscreen = screen_create("_menu_screen", NULL);
	if (menuscreen != NULL)
		menuscreen->priority = PRI_HIDDEN;
	active_menuitem = NULL;

	screenlist_add(menuscreen);

	menuscreen_create_menu();

	return 0;
}

// Shut down the menu screen system
int menuscreens_shutdown(void)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	if (!menuscreen)
		return -1;

	menuscreen_switch_item(NULL);
	screenlist_remove(menuscreen);
	screen_destroy(menuscreen);
	menuscreen = NULL;

	menuitem_destroy(main_menu);
	main_menu = NULL;
	custom_main_menu = NULL;
	screens_menu = NULL;

	input_release_client_keys(NULL);

	if (menu_key != NULL)
		free(menu_key);
	if (enter_key != NULL)
		free(enter_key);
	if (up_key != NULL)
		free(up_key);
	if (down_key != NULL)
		free(down_key);
	if (left_key != NULL)
		free(left_key);
	if (right_key != NULL)
		free(right_key);
	keymask = 0;

	return 0;
}

// Notify menu screen that an item is being destroyed
void menuscreen_inform_item_destruction(MenuItem *item)
{
	MenuItem *i;

	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	for (i = active_menuitem; i != NULL; i = i->parent) {
		if (i == item) {
			menuscreen_switch_item(item->parent);
		}
	}
}

// Notify menu screen that an item has been modified
void menuscreen_inform_item_modified(MenuItem *item)
{
	debug(RPT_DEBUG, "%s(item=[%s])", __FUNCTION__, ((item != NULL) ? item->id : "(null)"));

	if ((active_menuitem == NULL) || (item == NULL))
		return;

	if (active_menuitem == item || active_menuitem == item->parent) {
		menuitem_rebuild_screen(active_menuitem, menuscreen);
	}
}

// Check if a key is the reserved menu key
bool is_menu_key(const char *key)
{
	if ((menu_key != NULL) && (key != NULL) && (strcmp(key, menu_key) == 0))
		return true;
	else
		return false;
}

/**
 * \brief Switch to different menu item
 * \param new_menuitem Menu item to activate (NULL to hide menu)
 *
 * \details Handles item transitions, screen rebuilds, and ENTER/LEAVE events.
 * Updates menuscreen priority based on active state.
 */
void menuscreen_switch_item(MenuItem *new_menuitem)
{
	MenuItem *old_menuitem = active_menuitem;

	debug(RPT_DEBUG, "%s(item=[%s]) from active_menuitem=[%s]", __FUNCTION__,
	      ((new_menuitem != NULL) ? new_menuitem->id : "(null)"),
	      ((old_menuitem != NULL) ? old_menuitem->id : "(null)"));

	active_menuitem = new_menuitem;

	if (!old_menuitem && !new_menuitem) {

	} else if (old_menuitem && !new_menuitem) {
		menuscreen->priority = PRI_HIDDEN;

	} else if (!old_menuitem && new_menuitem) {
		menuitem_reset(active_menuitem);
		menuitem_rebuild_screen(active_menuitem, menuscreen);

		menuscreen->priority = PRI_INPUT;
	} else {
		if (old_menuitem->parent != new_menuitem) {
			menuitem_reset(new_menuitem);
		}
		menuitem_rebuild_screen(active_menuitem, menuscreen);
	}

	if (old_menuitem && old_menuitem->event_func)
		old_menuitem->event_func(old_menuitem, MENUEVENT_LEAVE);

	if (new_menuitem && new_menuitem->event_func)
		new_menuitem->event_func(new_menuitem, MENUEVENT_ENTER);

	return;
}

/**
 * \brief Handle menu quit action
 *
 * \details Closes menu screen by switching to NULL item.
 */
static void handle_quit(void)
{
	debug(RPT_DEBUG, "%s: Closing menu screen", __FUNCTION__);
	menuscreen_switch_item(NULL);
}

/**
 * \brief Handle menu close action
 *
 * \details Closes current item, switches to parent or NULL if at main menu.
 */
static void handle_close(void)
{
	debug(RPT_DEBUG, "%s: Closing item", __FUNCTION__);
	menuscreen_switch_item(
	    (active_menuitem == menuscreen_get_main()) ? NULL : active_menuitem->parent);
}

/**
 * \brief Handle no-action menu result
 *
 * \details Stays in current item, updates screen display.
 */
static void handle_none(void)
{
	debug(RPT_DEBUG, "%s: Staying in item", __FUNCTION__);
	if (active_menuitem) {
		menuitem_update_screen(active_menuitem, menuscreen);
	}
}

/**
 * \brief Handle menu item selection
 *
 * \details Enters current menu item's subitem.
 */
static void handle_enter(void)
{
	debug(RPT_DEBUG, "%s: Entering subitem", __FUNCTION__);
	menuscreen_switch_item(menu_get_current_item(active_menuitem));
}

/**
 * \brief Handle predecessor navigation
 *
 * \details Navigates to predecessor menu item as defined by predecessor_id.
 * Handles both simple and complex menu item types differently.
 */
static void handle_predecessor(void)
{
	MenuItem *predecessor;
	MenuItem *item = (active_menuitem->type == MENUITEM_MENU)
			     ? menu_get_item_for_predecessor_check(active_menuitem)
			     : active_menuitem;
	assert(item != NULL);

	debug(RPT_DEBUG, "%s: Switching to registered predecessor '%s' of '%s'.", __FUNCTION__,
	      item->predecessor_id, item->id);

	predecessor = menuitem_search(item->predecessor_id, (Client *)active_menuitem->client);

	if (predecessor == NULL) {
		report(RPT_ERR, "%s: cannot find predecessor '%s' of '%s'.", __FUNCTION__,
		       item->predecessor_id, item->id);
		return;
	}

	switch (predecessor->type) {

	// Simple menu items
	case MENUITEM_ACTION:
	case MENUITEM_CHECKBOX:
	case MENUITEM_RING:
		if (active_menuitem != predecessor->parent)
			menuscreen_switch_item(predecessor->parent);

		menu_select_subitem(active_menuitem, item->predecessor_id);
		menuitem_update_screen(active_menuitem, menuscreen);
		break;

	// Complex menu items
	default:
		if ((predecessor->parent != NULL) && (predecessor->parent->type == MENUITEM_MENU)) {
			menu_select_subitem(predecessor->parent, predecessor->id);
		}
		menuscreen_switch_item(predecessor);
		break;
	}
}

/**
 * \brief Handle successor navigation
 *
 * \details Navigates to successor menu item as defined by successor_id.
 * Similar logic to predecessor handling for different item types.
 */
static void handle_successor(void)
{
	MenuItem *successor;
	MenuItem *item = (active_menuitem->type == MENUITEM_MENU)
			     ? menu_get_item_for_successor_check(active_menuitem)
			     : active_menuitem;
	assert(item != NULL);

	debug(RPT_DEBUG, "%s: Switching to registered successor '%s' of '%s'.", __FUNCTION__,
	      item->successor_id, item->id);

	successor = menuitem_search(item->successor_id, (Client *)active_menuitem->client);

	if (successor == NULL) {
		report(RPT_ERR, "%s: cannot find successor '%s' of '%s'.", __FUNCTION__,
		       item->successor_id, item->id);
		return;
	}

	switch (successor->type) {

	// Simple menu items
	case MENUITEM_ACTION:
	case MENUITEM_CHECKBOX:
	case MENUITEM_RING:
		if (active_menuitem != successor->parent)
			menuscreen_switch_item(successor->parent);

		menu_select_subitem(active_menuitem, item->successor_id);
		menuitem_update_screen(active_menuitem, menuscreen);
		break;

	// Complex menu items
	default:
		if ((successor->parent != NULL) && (successor->parent->type == MENUITEM_MENU)) {
			menu_select_subitem(successor->parent, successor->id);
		}
		menuscreen_switch_item(successor);
		break;
	}
}

// Handle keyboard input for menu navigation
void menuscreen_key_handler(const char *key)
{
	MenuToken token = MENUTOKEN_NONE;
	MenuResult res;

	debug(RPT_DEBUG, "%s(\"%s\")", __FUNCTION__, key);

	if ((menu_key != NULL) && (strcmp(key, menu_key) == 0)) {
		token = MENUTOKEN_MENU;
	} else if ((enter_key != NULL) && (strcmp(key, enter_key) == 0)) {
		token = MENUTOKEN_ENTER;
	} else if ((up_key != NULL) && (strcmp(key, up_key) == 0)) {
		token = MENUTOKEN_UP;
	} else if ((down_key != NULL) && (strcmp(key, down_key) == 0)) {
		token = MENUTOKEN_DOWN;
	} else if ((left_key != NULL) && (strcmp(key, left_key) == 0)) {
		token = MENUTOKEN_LEFT;
	} else if ((right_key != NULL) && (strcmp(key, right_key) == 0)) {
		token = MENUTOKEN_RIGHT;
	} else {
		token = MENUTOKEN_OTHER;
	}

	if (!active_menuitem) {
		debug(RPT_DEBUG, "%s: Activating menu screen", __FUNCTION__);
		menuscreen_switch_item(menuscreen_get_main());
		return;
	}

	res = menuitem_process_input(active_menuitem, token, key, keymask);

	switch (res) {

	// Menu processing error
	case MENURESULT_ERROR:
		report(RPT_ERR, "%s: Error from menuitem_process_input", __FUNCTION__);
		break;

	// No menu action required
	case MENURESULT_NONE:
		handle_none();
		break;

	// Menu item selection
	case MENURESULT_ENTER:
		handle_enter();
		break;

	// Menu close action
	case MENURESULT_CLOSE:
		handle_close();
		break;

	// Menu quit action
	case MENURESULT_QUIT:
		handle_quit();
		break;

	// Predecessor navigation
	case MENURESULT_PREDECESSOR:
		handle_predecessor();
		break;

	// Successor navigation
	case MENURESULT_SUCCESSOR:
		handle_successor();
		break;

	default:
		assert(!"unexpected menuresult");
		break;
	}
}

/**
 * \brief Create main menu structure
 *
 * \details Initializes menuscreen and builds initial menu hierarchy
 * from configured menu items.
 */
void menuscreen_create_menu(void)
{
	Menu *options_menu;
	Menu *driver_menu;
	MenuItem *checkbox;
	MenuItem *slider;
	Driver *driver;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	main_menu = menu_create("mainmenu", NULL, "LCDproc Menu", NULL);
	if (main_menu == NULL) {
		report(RPT_ERR, "%s: Cannot create main menu", __FUNCTION__);
		return;
	}

	options_menu = menu_create("options", NULL, "Options", NULL);
	if (options_menu == NULL) {
		report(RPT_ERR, "%s: Cannot create options menu", __FUNCTION__);
		return;
	}

	menu_add_item(main_menu, options_menu);

#ifdef LCDPROC_TESTMENUS
	/**
	 * \todo Menu items in the screens menu currently have no functions assigned
	 *
	 * Menu items in the screens menu are dummy entries without functionality. They are
	 * currently only enabled for testing via LCDPROC_TESTMENUS define.
	 *
	 * Current state:
	 * - Screens menu exists and displays
	 * - Menu items are created but have no event handlers
	 * - No actions occur when items are selected
	 * - Code is wrapped in #ifdef LCDPROC_TESTMENUS
	 *
	 * Required implementation:
	 * - Add event handlers for screen management actions:
	 *   - Switch to selected screen
	 *   - Enable/disable screens
	 *   - Change screen priority
	 *   - Remove screens
	 * - Move code outside #ifdef once functional
	 * - Add screen state display (active, priority, etc.)
	 *
	 * Impact: Functionality, completeness, user interface for screen management
	 *
	 * \ingroup ToDo_medium
	 */
	screens_menu = menu_create("screens", NULL, "Screens", NULL);
	if (screens_menu == NULL) {
		report(RPT_ERR, "%s: Cannot create screens menu", __FUNCTION__);
		return;
	}
	menu_add_item(main_menu, screens_menu);

	menuscreen_create_testmenu();
#endif

	checkbox = menuitem_create_checkbox("heartbeat", heartbeat_handler, "Heartbeat", NULL, true,
					    heartbeat);
	menu_add_item(options_menu, checkbox);

	checkbox = menuitem_create_checkbox("backlight", backlight_handler, "Backlight", NULL, true,
					    backlight);
	menu_add_item(options_menu, checkbox);

	slider = menuitem_create_slider("titlespeed", titlespeed_handler, "TitleSpeed", NULL, "1",
					"10", TITLESPEED_MIN, TITLESPEED_MAX, 1, titlespeed);
	menu_add_item(options_menu, slider);

	// Create driver-specific submenus: iterate through all loaded drivers, check for contrast
	// and brightness support, create submenu for each driver with available controls, add
	// sliders for contrast (0-1000) and on/off brightness with current values
	for (driver = drivers_getfirst(); driver; driver = drivers_getnext()) {
		int contrast_avail = (driver->get_contrast && driver->set_contrast) ? 1 : 0;
		int brightness_avail = (driver->get_brightness && driver->set_brightness) ? 1 : 0;

		if (contrast_avail || brightness_avail) {
			driver_menu = menu_create(driver->name, NULL, driver->name, NULL);
			if (driver_menu == NULL) {
				report(RPT_ERR, "%s: Cannot create menu for driver %s",
				       __FUNCTION__, driver->name);
				continue;
			}

			menu_set_association(driver_menu, driver);
			menu_add_item(options_menu, driver_menu);

			if (contrast_avail) {
				int contrast = driver->get_contrast(driver);

				slider = menuitem_create_slider("contrast", contrast_handler,
								"Contrast", NULL, "min", "max", 0,
								1000, 25, contrast);
				menu_add_item(driver_menu, slider);
			}

			if (brightness_avail) {
				int onbrightness = driver->get_brightness(driver, BACKLIGHT_ON);
				int offbrightness = driver->get_brightness(driver, BACKLIGHT_OFF);

				slider = menuitem_create_slider("onbrightness", brightness_handler,
								"On Brightness", NULL, "min", "max",
								0, 1000, 25, onbrightness);
				menu_add_item(driver_menu, slider);

				slider = menuitem_create_slider("offbrightness", brightness_handler,
								"Off Brightness", NULL, "min",
								"max", 0, 1000, 25, offbrightness);
				menu_add_item(driver_menu, slider);
			}
		}
	}
}

#ifdef LCDPROC_TESTMENUS

// Create test menu for demonstration purposes
void menuscreen_create_testmenu(void)
{
	MenuItem *test_item;
	Menu *test_menu;

	char testiso[] = {
	    'D', 'e',  'm', 'o', '\t', 160, 161, 162,  163, 164, 165,  166, 167, '\t', 168,
	    169, 170,  171, 172, 173,  174, 175, '\t', 176, 177, 178,  179, 180, 181,  182,
	    183, '\t', 184, 185, 186,  187, 188, 189,  190, 191, '\t', 192, 193, 194,  195,
	    196, 197,  198, 199, '\t', 200, 201, 202,  203, 204, 205,  206, 207, '\t', 208,
	    209, 210,  211, 212, 213,  214, 215, '\t', 216, 217, 218,  219, 220, 221,  222,
	    223, '\t', 224, 225, 226,  227, 228, 229,  230, 231, '\t', 232, 233, 234,  235,
	    236, 237,  238, 239, '\t', 240, 241, 242,  243, 244, 245,  246, 247, '\t', 248,
	    249, 250,  251, 252, 253,  254, 255, '\0'};

	test_menu = menu_create("test", NULL, "Test menu", NULL);
	if (test_menu == NULL) {
		report(RPT_ERR, "%s: Cannot create test menu", __FUNCTION__);
		return;
	}
	menu_add_item(main_menu, test_menu);

	test_item = menuitem_create_action("", NULL, "Action", NULL, MENURESULT_NONE);
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_action("", NULL, "Action,closing", NULL, MENURESULT_CLOSE);
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_action("", NULL, "Action,quitting", NULL, MENURESULT_QUIT);
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_checkbox("", NULL, "Checkbox", NULL, false, false);
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_checkbox("", NULL, "Checkbox, gray", NULL, true, false);
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_ring(
	    "", NULL, "Ring", NULL,
	    "ABC\tDEF\t01234567890\tOr a very long string that will not fit on any display", 1);
	menu_add_item(test_menu, test_item);

	test_item =
	    menuitem_create_slider("", NULL, "Slider", NULL, "mintext", "maxtext", -20, 20, 1, 0);
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_slider("", NULL, "Slider,step=5", NULL, "mintext", "maxtext",
					   -20, 20, 5, 0);
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_numeric("", NULL, "Numeric", NULL, 1, 365, 15);
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_numeric("", NULL, "Numeric,signed", NULL, -20, +20, 15);
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_alpha("", NULL, "Alpha", NULL, 0, 3, 12, true, true, true,
					  ".-+@", "LCDproc-v0.5");
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_alpha("", NULL, "Alpha, caps only", NULL, 0, 3, 12, true, false,
					  false, "-", "LCDPROC");
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_ip("", NULL, "IPv4", NULL, false, "192.168.1.245");
	menu_add_item(test_menu, test_item);
	test_item = menuitem_create_ip("", NULL, "IPv6", NULL, true, "1080:0:0:0:8:800:200C:417A");
	menu_add_item(test_menu, test_item);

	test_item = menuitem_create_ring("", NULL, "Charset", NULL, testiso, 0);
	menu_add_item(test_menu, test_item);
}
#endif

/** @cond */
MenuEventFunc(heartbeat_handler)
/** @endcond */
{
	/** @cond */
	debug(RPT_DEBUG, "%s(item=[%s], event=%d)", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), event);

	if ((item != NULL) && (event == MENUEVENT_UPDATE)) {
		heartbeat = item->data.checkbox.value;
		report(RPT_INFO, "Menu: set heartbeat to %d", heartbeat);
	}

	return 0;
	/** @endcond */
}

/** @cond */
MenuEventFunc(backlight_handler)
/** @endcond */
{
	debug(RPT_DEBUG, "%s(item=[%s], event=%d)", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), event);

	if ((item != NULL) && (event == MENUEVENT_UPDATE)) {
		backlight = item->data.checkbox.value;
		report(RPT_INFO, "Menu: set backlight to %d", backlight);
	}

	return 0;
}

/** @cond */
MenuEventFunc(titlespeed_handler)
/** @endcond */
{
	/** @cond */
	debug(RPT_DEBUG, "%s(item=[%s], event=%d)", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), event);

	if ((item != NULL) && ((event == MENUEVENT_MINUS) || (event == MENUEVENT_PLUS))) {
		titlespeed = item->data.slider.value;
		report(RPT_INFO, "Menu: set titlespeed to %d", titlespeed);
	}
	return 0;
	/** @endcond */
}

/** @cond */
MenuEventFunc(contrast_handler)
/** @endcond */
{
	debug(RPT_DEBUG, "%s(item=[%s], event=%d)", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), event);

	if ((item != NULL) && ((event == MENUEVENT_MINUS) || (event == MENUEVENT_PLUS))) {
		Driver *driver = item->parent->data.menu.association;

		if (driver != NULL) {
			driver->set_contrast(driver, item->data.slider.value);
			report(RPT_INFO, "Menu: set contrast of [%.40s] to %d", driver->name,
			       item->data.slider.value);
		}
	}

	return 0;
}

/** @cond */
MenuEventFunc(brightness_handler)
/** @endcond */
{
	debug(RPT_DEBUG, "%s(item=[%s], event=%d)", __FUNCTION__,
	      ((item != NULL) ? item->id : "(null)"), event);

	if ((item != NULL) && ((event == MENUEVENT_MINUS) || (event == MENUEVENT_PLUS))) {
		Driver *driver = item->parent->data.menu.association;

		if (driver != NULL) {
			if (strcmp(item->id, "onbrightness") == 0) {
				driver->set_brightness(driver, BACKLIGHT_ON,
						       item->data.slider.value);
			} else if (strcmp(item->id, "offbrightness") == 0) {
				driver->set_brightness(driver, BACKLIGHT_OFF,
						       item->data.slider.value);
			}
		}
	}

	return 0;
}

// Add a screen to the menu system
void menuscreen_add_screen(Screen *s)
{
	Menu *m;
	MenuItem *mi;

	debug(RPT_DEBUG, "%s(s=[%s])", __FUNCTION__, ((s != NULL) ? s->id : "(null)"));

	if ((screens_menu == NULL) || (s == NULL))
		return;

	m = menu_create(s->id, NULL, ((s->name != NULL) ? s->name : s->id), s->client);
	if (m == NULL) {
		report(RPT_ERR, "%s: Cannot create menu", __FUNCTION__);
		return;
	}
	menu_set_association(m, s);
	menu_add_item(screens_menu, m);

	mi = menuitem_create_action("", NULL, "(don't work yet)", s->client, MENURESULT_NONE);
	menu_add_item(m, mi);

	mi = menuitem_create_action("", NULL, "To Front", s->client, MENURESULT_QUIT);
	menu_add_item(m, mi);

	mi = menuitem_create_checkbox("", NULL, "Visible", s->client, false, true);
	menu_add_item(m, mi);

	mi = menuitem_create_numeric("", NULL, "Duration", s->client, 2, 3600, s->duration);
	menu_add_item(m, mi);

	mi = menuitem_create_ring("", NULL, "Priority", s->client,
				  "Hidden\tBackground\tForeground\tAlert\tInput", s->priority);
	menu_add_item(m, mi);
}

// Remove a screen from the menu system
void menuscreen_remove_screen(Screen *s)
{
	debug(RPT_DEBUG, "%s(s=[%s])", __FUNCTION__, (s != NULL) ? s->id : "(NULL)");

	if ((s == NULL) || (s == menuscreen))
		return;

	if (screens_menu) {
		Menu *m = menu_find_item(screens_menu, s->id, false);

		menu_remove_item(screens_menu, m);
		menuitem_destroy(m);
	}
}

// Switch to the specified menu
int menuscreen_goto(Menu *menu)
{
	debug(RPT_DEBUG, "%s(m=[%s]): active_menuitem=[%s]", __FUNCTION__,
	      (menu != NULL) ? menu->id : "(NULL)",
	      (active_menuitem != NULL) ? active_menuitem->id : "(NULL)");
	menuscreen_switch_item(menu);

	return 0;
}

// Set a custom main menu
int menuscreen_set_main(Menu *menu)
{
	debug(RPT_DEBUG, "%s(m=[%s])", __FUNCTION__, (menu != NULL) ? menu->id : "(NULL)");
	custom_main_menu = menu;

	return 0;
}

/**
 * \brief Menuscreen get main
 * \return Return value
 */
Menu *menuscreen_get_main(void) { return custom_main_menu ? custom_main_menu : main_menu; }
