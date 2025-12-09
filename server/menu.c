// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/menu.c
 * \brief Menu system implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \date 1999-2005
 *
 *
 * \features
 * - Menu creation and management with hierarchical structure and lifecycle control
 * - Menu item navigation and selection with current item tracking and state management
 * - Screen building and widget management for visual menu representation
 * - Input processing and event handling for interactive menu operation and user response
 * - Hierarchical menu traversal with parent-child relationships and recursive navigation
 * - Item search and enumeration with support for hidden items and filtering
 * - Menu association support for linking external data structures to menu items
 * - Menu reset functionality for returning menu state to initial configuration
 * - Predecessor and successor checking for navigation flow control and validation
 * - Memory management with proper cleanup and destruction of menu hierarchies
 * - Widget integration for visual representation with screen updates and rendering
 * - Hidden item handling with proper skipping during navigation and indexing operations
 * - Debug logging for all menu operations and state changes for troubleshooting
 *
 * \usage
 * - Used by LCDd server for implementing interactive menu systems and navigation
 * - Create menu structures via menu_create() with unique identifiers and event handlers
 * - Add menu items via menu_add_item() to build hierarchical menu structures
 * - Navigate menus via menu_get_current_item() and internal selection mechanisms
 * - Process user input via menu_process_input() for interactive menu operation
 * - Build visual display via menu_build_screen() for screen rendering and updates
 * - Find specific items via menu_find_item() with optional recursive searching
 * - Manage menu lifecycle via menu_destroy() and cleanup functions for memory management
 * - Reset menu state via menu_reset() for returning to initial menu configuration
 * - Handle menu associations via menu_set_association() for external data linking
 *
 * \details Implementation of menu system handling all actions that can be performed
 * on menus, with comprehensive support for hierarchical navigation and visual display.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "drivers.h"
#include "menu.h"
#include "menuitem.h"
#include "screen.h"
#include "widget.h"

#include "shared/report.h"

/** \brief User-configurable main menu (defined in menuscreens.c) */
extern Menu *custom_main_menu;

/**
 * \brief Get menu subitem by visible index
 * \param menu Menu to search
 * \param index Zero-based index of visible item
 * \return Pointer to menu item, or NULL if index out of range
 *
 * \details Skips hidden items when counting. Index 0 = first visible item.
 */
static void *menu_get_subitem(Menu *menu, int index)
{
	MenuItem *item;
	int i = 0;

	debug(RPT_DEBUG, "%s(menu=[%s], index=%d)", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), index);

	for (item = LL_GetFirst(menu->data.menu.contents); item != NULL;
	     item = LL_GetNext(menu->data.menu.contents)) {
		if (!item->is_hidden) {
			if (i == index)
				return item;
			++i;
		}
	}
	return NULL;
}

/**
 * \brief Menu get index of
 * \param menu Menu *menu
 * \param item_id char *item_id
 * \return Return value
 */
static int menu_get_index_of(Menu *menu, char *item_id)
{
	MenuItem *item;
	int i = 0;

	debug(RPT_DEBUG, "%s(menu=[%s], item_id=%s)", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), item_id);

	for (item = LL_GetFirst(menu->data.menu.contents); item != NULL;
	     item = LL_GetNext(menu->data.menu.contents)) {
		if (!item->is_hidden) {
			if (strcmp(item_id, item->id) == 0)
				return i;
			++i;
		}
	}
	return -1;
}

/**
 * \brief Menu visible item count
 * \param menu Menu *menu
 * \return Return value
 */
static int menu_visible_item_count(Menu *menu)
{
	MenuItem *item;
	int i = 0;

	for (item = LL_GetFirst(menu->data.menu.contents); item != NULL;
	     item = LL_GetNext(menu->data.menu.contents)) {
		if (!item->is_hidden)
			++i;
	}
	return i;
}

/** \brief Display mode: show label only */
#define LV_LABEL_ONLY 1
/** \brief Display mode: show value only */
#define LV_VALUE_ONLY 2
/** \brief Display mode: show label and first part of value */
#define LV_LABEL_VALU 3
/** \brief Display mode: show label and second part of value */
#define LV_LABEL_ALUE 4

/**
 * \brief Format label-value pair with overflow handling
 * \param string Output buffer
 * \param len Buffer length
 * \param text Label text (left-aligned)
 * \param value Value text (right-aligned)
 * \param mode Display mode (LV_LABEL_ONLY, LV_VALUE_ONLY, LV_LABEL_VALU, LV_LABEL_ALUE)
 * \return Pointer to formatted string
 *
 * \details Formats "label      value" with smart overflow handling.
 * When text+value too long, mode determines truncation strategy.
 */
char *fill_labeled_value(char *string, int len, const char *text, const char *value, int mode)
{
	if ((string != NULL) && (text != NULL) && (len > 0)) {
		int textlen = strlen(text);

		len--;

		debug(RPT_DEBUG, "%s(string=[%p], len=%d, text=\"%s\", value=\"%s\", mode=%d)",
		      __FUNCTION__, string, len, text, ((value != NULL) ? value : "(null)"), mode);

		if ((value != NULL) && (textlen + strlen(value) < len - 1)) {
			memset(string, ' ', len);
			strncpy(string, text, textlen);
			strncpy(string + len - strlen(value), value, strlen(value));
			string[len] = '\0';

		} else {
			// Safeguard against missing value or too long labels
			if ((value == NULL) || (textlen >= len - 3))
				mode = LV_LABEL_ONLY;

			switch (mode) {

			// Show first characters in value
			case LV_LABEL_VALU:
				memset(string, ' ', len);
				strncpy(string, text, textlen);
				strncpy(string + textlen + 2, value, len - textlen - 2);
				strncpy(string + len - 2, "..", 2);
				string[len] = '\0';
				break;

			// Show label with ".." and last characters of value
			case LV_LABEL_ALUE:
				memset(string, ' ', len);
				strncpy(string, text, textlen);
				strncpy(string + textlen + 2, "..", 2);
				strncpy(string + textlen + 4,
					value + strlen(value) - len + textlen + 4,
					len - textlen - 4);
				string[len] = '\0';
				break;

			// Show only value with leading space indentation
			case LV_VALUE_ONLY:
				strncpy(string, " ", 1);
				strncpy(string + 1, value, len - 1);
				string[len] = '\0';
				break;

			// Show only label text
			case LV_LABEL_ONLY:

			default:
				strncpy(string, text, len);
				string[len] = '\0';
				break;
			}
		}
		return (string);
	}
	return NULL;
}

// Create new menu with specified properties
Menu *menu_create(char *id, MenuEventFunc(*event_func), char *text, Client *client)
{
	Menu *new_menu;

	debug(RPT_DEBUG, "%s(id=\"%s\", event_func=%p, text=\"%s\", client=%p)", __FUNCTION__, id,
	      event_func, text, client);

	new_menu = menuitem_create(MENUITEM_MENU, id, event_func, text, client);

	if (new_menu != NULL) {
		new_menu->data.menu.contents = LL_new();
		new_menu->data.menu.association = NULL;
	}

	return new_menu;
}

// Destroy menu and clean up its resources
void menu_destroy(Menu *menu)
{
	debug(RPT_DEBUG, "%s(menu=[%s])", __FUNCTION__, ((menu != NULL) ? menu->id : "(null)"));

	if (menu == NULL)
		return;

	if (custom_main_menu == menu)
		custom_main_menu = NULL;

	menu_destroy_all_items(menu);
	LL_Destroy(menu->data.menu.contents);
	menu->data.menu.contents = NULL;
}

// Add menu item to menu
void menu_add_item(Menu *menu, MenuItem *item)
{
	debug(RPT_DEBUG, "%s(menu=[%s], item=[%s])", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), ((item != NULL) ? item->id : "(null)"));

	if ((menu == NULL) || (item == NULL))
		return;

	LL_Push(menu->data.menu.contents, item);
	item->parent = menu;
}

// Remove menu item from menu without destroying it
void menu_remove_item(Menu *menu, MenuItem *item)
{
	int i;
	MenuItem *item2;

	debug(RPT_DEBUG, "%s(menu=[%s], item=[%s])", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), ((item != NULL) ? item->id : "(null)"));

	if ((menu == NULL) || (item == NULL))
		return;

	for (item2 = LL_GetFirst(menu->data.menu.contents), i = 0; item2 != NULL;
	     item2 = LL_GetNext(menu->data.menu.contents), i++) {
		if (item == item2) {
			LL_DeleteNode(menu->data.menu.contents, NEXT);
			if (menu->data.menu.selector_pos >= i) {
				menu->data.menu.selector_pos--;
				if (menu->data.menu.scroll > 0)
					menu->data.menu.scroll--;
			}
			return;
		}
	}
}

// Destroy and remove all items from menu
void menu_destroy_all_items(Menu *menu)
{
	MenuItem *item;

	debug(RPT_DEBUG, "%s(menu=[%s])", __FUNCTION__, ((menu != NULL) ? menu->id : "(null)"));

	if (menu == NULL)
		return;

	for (item = menu_getfirst_item(menu); item != NULL; item = menu_getfirst_item(menu)) {
		menuitem_destroy(item);
		LL_Remove(menu->data.menu.contents, item, NEXT);
	}
}

// Get currently selected menu item
MenuItem *menu_get_current_item(Menu *menu)
{
	return (MenuItem *)((menu != NULL) ? menu_get_subitem(menu, menu->data.menu.selector_pos)
					   : NULL);
}

// Find menu item by ID within menu
MenuItem *menu_find_item(Menu *menu, char *id, bool recursive)
{
	MenuItem *item;

	debug(RPT_DEBUG, "%s(menu=[%s], id=\"%s\", recursive=%d)", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), id, recursive);

	if ((menu == NULL) || (id == NULL))
		return NULL;
	if (strcmp(menu->id, id) == 0)
		return menu;

	for (item = menu_getfirst_item(menu); item != NULL; item = menu_getnext_item(menu)) {
		if (strcmp(item->id, id) == 0) {
			return item;
		}
		if (recursive && (item->type == MENUITEM_MENU)) {
			MenuItem *res = menu_find_item(item, id, recursive);

			if (res != NULL)
				return res;
		}
	}

	return NULL;
}

// Set association data for menu
void menu_set_association(Menu *menu, void *assoc)
{
	debug(RPT_DEBUG, "%s(menu=[%s], assoc=[%s])", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), ((assoc != NULL) ? "(data)" : "(null)"));

	if (menu == NULL)
		return;

	menu->data.menu.association = assoc;
}

// Reset menu to initial state
void menu_reset(Menu *menu)
{
	debug(RPT_DEBUG, "%s(menu=[%s])", __FUNCTION__, ((menu != NULL) ? menu->id : "(null)"));

	if (menu == NULL)
		return;

	menu->data.menu.selector_pos = 0;
	menu->data.menu.scroll = 0;
}

// Build screen widgets for menu display
void menu_build_screen(MenuItem *menu, Screen *s)
{
	Widget *w;
	MenuItem *subitem;
	int itemnr;

	debug(RPT_DEBUG, "%s(menu=[%s], screen=[%s])", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((menu == NULL) || (s == NULL))
		return;

	/**
	 * \todo Put menu in a frame to do easy scrolling
	 *
	 * Menu scrolling should be implemented using frames for better performance
	 * and clean architecture. Currently scrolling is manually handled by adjusting
	 * widget positions, which is error-prone and requires manual bounds checking.
	 *
	 * Using frames would:
	 * - Simplify scrolling logic (frames handle viewport automatically)
	 * - Improve performance (only visible widgets rendered)
	 * - Enable horizontal scrolling support
	 * - Reduce code duplication across menu types
	 *
	 * Impact: Performance, code architecture, maintainability
	 *
	 * \ingroup ToDo_medium
	 */

	w = widget_create("title", WID_TITLE, s);
	if (w != NULL) {
		screen_add_widget(s, w);
		w->text = strdup(menu->text);
		w->x = 1;
	}

	for (subitem = LL_GetFirst(menu->data.menu.contents), itemnr = 0; subitem != NULL;
	     subitem = LL_GetNext(menu->data.menu.contents), itemnr++) {
		char buf[10];

		if (subitem->is_hidden)
			continue;
		snprintf(buf, sizeof(buf) - 1, "text%d", itemnr);
		buf[sizeof(buf) - 1] = '\0';
		w = widget_create(buf, WID_STRING, s);

		if (w != NULL) {
			screen_add_widget(s, w);
			w->x = 2;

			switch (subitem->type) {

			// Checkbox items
			case MENUITEM_CHECKBOX:

				w->text = strdup(subitem->text);
				if (strlen(subitem->text) >= display_props->width - 2)
					w->text[display_props->width - 2] = '\0';

				snprintf(buf, sizeof(buf) - 1, "icon%d", itemnr);
				buf[sizeof(buf) - 1] = '\0';
				w = widget_create(buf, WID_ICON, s);

				screen_add_widget(s, w);
				w->x = display_props->width - 1;
				w->length = ICON_CHECKBOX_OFF;
				break;

			// Ring items
			case MENUITEM_RING:
				w->text = malloc(display_props->width);
				break;

			// Menu items
			case MENUITEM_MENU:
				w->text = malloc(strlen(subitem->text) + 4);
				snprintf(w->text, strlen(subitem->text) + 4, "%s >", subitem->text);
				if (strlen(subitem->text) >= display_props->width - 1)
					w->text[display_props->width - 1] = '\0';
				break;

			// Input and action items
			case MENUITEM_ACTION:
			case MENUITEM_SLIDER:
			case MENUITEM_NUMERIC:
			case MENUITEM_ALPHA:
			case MENUITEM_IP:
				w->text = malloc(display_props->width);
				strncpy(w->text, subitem->text, display_props->width);
				w->text[display_props->width - 1] = '\0';
				break;

			default:
				assert(!"unexpected menuitem type");
			}
		}
	}

	w = widget_create("selector", WID_ICON, s);
	if (w != NULL) {
		screen_add_widget(s, w);
		w->length = ICON_SELECTOR_AT_LEFT;
		w->x = 1;
	}

	w = widget_create("upscroller", WID_ICON, s);
	if (w != NULL) {
		screen_add_widget(s, w);
		w->length = ICON_ARROW_UP;
		w->x = display_props->width;
		w->y = 1;
	}

	w = widget_create("downscroller", WID_ICON, s);
	if (w != NULL) {
		screen_add_widget(s, w);
		w->length = ICON_ARROW_DOWN;
		w->x = display_props->width;
		w->y = display_props->height;
	}

	/**
	 * \todo Remove scrollers when menu is in a frame
	 * \ingroup ToDo_medium
	 *
	 * Manual scroller widgets (upscroller/downscroller icons) are a workaround
	 * for the lack of frame-based scrolling. When menus are properly implemented
	 * in frames, these manual scroll indicators can be removed as frames provide
	 * automatic scroll indication.
	 *
	 * Current: Manual upscroller/downscroller widgets added to show scroll state
	 * Future: Frames handle scrolling automatically, making these widgets redundant
	 *
	 * \see menu_build_screen (line 387: "Put menu in a frame to do easy scrolling")
	 *
	 * Impact: Code cleanup, reduced widget count, simpler menu implementation
	 */
}

/**
 * \brief Determine widget visibility based on screen position
 * \param y Y coordinate of widget
 * \param visible_type Widget type to return if visible
 * \return visible_type if on-screen (y between 1 and height), WID_NONE otherwise
 *
 * \details Widgets outside display bounds are marked WID_NONE (hidden).
 */
static inline WidgetType set_widget_visibility(int y, WidgetType visible_type)
{
	return ((y > 0) && (y <= display_props->height)) ? visible_type : WID_NONE;
}

// Update screen widgets with current menu state
void menu_update_screen(MenuItem *menu, Screen *s)
{
	Widget *w;
	MenuItem *subitem;
	int itemnr;
	int hidden_count = 0;

	debug(RPT_DEBUG, "%s(menu=[%s], screen=[%s])", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), ((s != NULL) ? s->id : "(null)"));

	if ((menu == NULL) || (s == NULL))
		return;

	w = screen_find_widget(s, "title");
	if (w == NULL) {
		report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, "title");
		return;
	}
	w->y = 1 - menu->data.menu.scroll;

	/**
	 * \todo Remove visibility workaround when rendering is safe
	 *
	 * The set_widget_visibility() calls are workarounds for rendering system limitations.
	 * Widgets outside display bounds are manually marked as WID_NONE (hidden) to prevent
	 * crashes from accessing invalid widget properties.
	 *
	 * Rendering safety significantly improved in September 2025 through NULL-pointer fixes:
	 * - Added NULL checks for widgets before accessing properties (w->y, w->x)
	 * - Prevents crashes when widgets are scrolled off-screen
	 * - Makes rendering more robust
	 *
	 * Once rendering system fully handles out-of-bounds widgets safely, these visibility
	 * workarounds can be removed and widgets can keep their original types regardless of
	 * scroll position.
	 *
	 * Impact: Code cleanup, performance (fewer type changes), simpler menu logic
	 *
	 * \ingroup ToDo_medium
	 */

	w->type = set_widget_visibility(w->y, WID_TITLE);

	for (subitem = LL_GetFirst(menu->data.menu.contents), itemnr = 0; subitem != NULL;
	     subitem = LL_GetNext(menu->data.menu.contents), itemnr++) {
		char buf[LCD_MAX_WIDTH];
		char *p;
		int len = display_props->width - 1;

		if (subitem->is_hidden) {
			debug(RPT_DEBUG, "%s: menu %s has hidden menu: %s", __FUNCTION__, menu->id,
			      subitem->id);
			hidden_count++;
			continue;
		}

		snprintf(buf, sizeof(buf) - 1, "text%d", itemnr);
		buf[sizeof(buf) - 1] = '\0';
		w = screen_find_widget(s, buf);
		if (w == NULL) {
			report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, buf);
			continue;
		}

		w->y = 2 + itemnr - hidden_count - menu->data.menu.scroll;
		w->type = set_widget_visibility(w->y, WID_STRING);

		switch (subitem->type) {

		// Checkbox items
		case MENUITEM_CHECKBOX:
			snprintf(buf, sizeof(buf) - 1, "icon%d", itemnr);
			buf[sizeof(buf) - 1] = '\0';
			w = screen_find_widget(s, buf);
			if (w == NULL) {
				report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, buf);
				break;
			}
			w->y = 2 + itemnr - menu->data.menu.scroll;
			w->length = ((int[]){ICON_CHECKBOX_OFF, ICON_CHECKBOX_ON,
					     ICON_CHECKBOX_GRAY})[subitem->data.checkbox.value];
			w->type = set_widget_visibility(w->y, WID_ICON);
			break;

		// Ring items
		case MENUITEM_RING:
			p = LL_GetByIndex(subitem->data.ring.strings, subitem->data.ring.value);
			fill_labeled_value(w->text, len, subitem->text, p, LV_VALUE_ONLY);

			break;

		// Slider items
		case MENUITEM_SLIDER:
			snprintf(buf, display_props->width, "%d", subitem->data.slider.value);
			buf[display_props->width - 1] = '\0';
			fill_labeled_value(w->text, len, subitem->text, buf, LV_LABEL_VALU);
			break;

		// Numeric items
		case MENUITEM_NUMERIC:
			snprintf(buf, display_props->width, "%d", subitem->data.numeric.value);
			buf[display_props->width - 1] = '\0';
			fill_labeled_value(w->text, len, subitem->text, buf, LV_LABEL_VALU);
			break;

		// Alpha items
		case MENUITEM_ALPHA:
			fill_labeled_value(w->text, len, subitem->text, subitem->data.alpha.value,
					   LV_LABEL_VALU);
			break;

		// IP items
		case MENUITEM_IP:
			fill_labeled_value(w->text, len, subitem->text, subitem->data.ip.value,
					   LV_LABEL_ALUE);
			break;

		default:
			break;
		}
	}

	w = screen_find_widget(s, "selector");
	if (w != NULL)
		w->y = 2 + menu->data.menu.selector_pos - menu->data.menu.scroll;
	else
		report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, "selector");

	w = screen_find_widget(s, "upscroller");
	if (w != NULL)
		w->type = (menu->data.menu.scroll > 0) ? WID_ICON : WID_NONE;
	else
		report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, "upscroller");

	w = screen_find_widget(s, "downscroller");
	if (w != NULL)
		w->type = (menu_visible_item_count(menu) >=
			   menu->data.menu.scroll + display_props->height)
			      ? WID_ICON
			      : WID_NONE;
	else
		report(RPT_ERR, "%s: could not find widget: %s", __FUNCTION__, "downscroller");
}

// Get menu item for predecessor navigation checking
MenuItem *menu_get_item_for_predecessor_check(Menu *menu)
{
	MenuItem *subitem = menu_get_subitem(menu, menu->data.menu.selector_pos);

	if (subitem == NULL)
		return NULL;

	switch (subitem->type) {

	// Items without own screen
	case MENUITEM_ACTION:
	case MENUITEM_CHECKBOX:
	case MENUITEM_RING:
		if (subitem->predecessor_id == NULL)
			return menu;

		return subitem;

	// Items with own screens
	case MENUITEM_MENU:
	case MENUITEM_SLIDER:
	case MENUITEM_NUMERIC:
	case MENUITEM_ALPHA:
	case MENUITEM_IP:
		return menu;

	default:
		return NULL;
	}
}

// Get menu item for successor navigation checking
MenuItem *menu_get_item_for_successor_check(Menu *menu)
{
	MenuItem *subitem = menu_get_subitem(menu, menu->data.menu.selector_pos);

	if (subitem == NULL)
		return NULL;

	switch (subitem->type) {

	// Items without own screen
	case MENUITEM_ACTION:
	case MENUITEM_CHECKBOX:
	case MENUITEM_RING:

		return subitem;

	// Items with own screens
	case MENUITEM_MENU:
	case MENUITEM_SLIDER:
	case MENUITEM_NUMERIC:
	case MENUITEM_ALPHA:
	case MENUITEM_IP:
		return menu;

	default:
		return NULL;
	}
}

// Process input events for menu interaction
MenuResult menu_process_input(Menu *menu, MenuToken token, const char *key, unsigned int keymask)
{
	MenuItem *subitem;

	debug(RPT_DEBUG, "%s(menu=[%s], token=%d, key=\"%s\")", __FUNCTION__,
	      ((menu != NULL) ? menu->id : "(null)"), token, key);

	if (menu == NULL)
		return MENURESULT_ERROR;

	switch (token) {

	// Handle menu/back button
	case MENUTOKEN_MENU:
		subitem = menu_get_item_for_predecessor_check(menu);
		if (subitem == NULL)
			return MENURESULT_ERROR;

		return menuitem_predecessor2menuresult(subitem->predecessor_id, MENURESULT_CLOSE);

	// Handle enter/select button
	case MENUTOKEN_ENTER:
		subitem = menu_get_subitem(menu, menu->data.menu.selector_pos);
		if (!subitem)
			break;

		switch (subitem->type) {

		// Action items
		case MENUITEM_ACTION:
			if (subitem->event_func)
				subitem->event_func(subitem, MENUEVENT_SELECT);

			return menuitem_successor2menuresult(subitem->successor_id,
							     MENURESULT_NONE);

		// Checkbox items
		case MENUITEM_CHECKBOX:
			if (subitem->successor_id == NULL) {
				subitem->data.checkbox.value++;
				subitem->data.checkbox.value %=
				    (subitem->data.checkbox.allow_gray) ? 3 : 2;

				if (subitem->event_func)
					subitem->event_func(subitem, MENUEVENT_UPDATE);
			}

			return menuitem_successor2menuresult(subitem->successor_id,
							     MENURESULT_NONE);

		// Ring items
		case MENUITEM_RING:
			if (subitem->successor_id == NULL) {
				subitem->data.ring.value++;
				subitem->data.ring.value %= LL_Length(subitem->data.ring.strings);

				if (subitem->event_func)
					subitem->event_func(subitem, MENUEVENT_UPDATE);
			}

			return menuitem_successor2menuresult(subitem->successor_id,
							     MENURESULT_NONE);

		// Menu and input items
		case MENUITEM_MENU:
		case MENUITEM_SLIDER:
		case MENUITEM_NUMERIC:
		case MENUITEM_ALPHA:
		case MENUITEM_IP:
			return MENURESULT_ENTER;

		default:
			break;
		}

		return MENURESULT_ERROR;

	// Handle up navigation
	case MENUTOKEN_UP:
		if (menu->data.menu.selector_pos > 0) {
			if (menu->data.menu.selector_pos < menu->data.menu.scroll)
				menu->data.menu.scroll--;
			menu->data.menu.selector_pos--;
		} else if (menu->data.menu.selector_pos == 0) {
			menu->data.menu.selector_pos = menu_visible_item_count(menu) - 1;
			if (menu_visible_item_count(menu) >= display_props->height)
				menu->data.menu.scroll =
				    menu->data.menu.selector_pos + 2 - display_props->height;
			else
				menu->data.menu.scroll = 0;
		}

		return MENURESULT_NONE;

	// Handle down navigation
	case MENUTOKEN_DOWN:
		if (menu->data.menu.selector_pos < menu_visible_item_count(menu) - 1) {
			menu->data.menu.selector_pos++;
			if (menu->data.menu.selector_pos - menu->data.menu.scroll + 2 >
			    display_props->height)
				menu->data.menu.scroll++;
		} else {
			menu->data.menu.selector_pos = 0;
			menu->data.menu.scroll = 0;
		}

		return MENURESULT_NONE;

	// Handle left navigation
	case MENUTOKEN_LEFT:
		if (!(keymask & MENUTOKEN_LEFT))
			return MENURESULT_NONE;

		subitem = menu_get_subitem(menu, menu->data.menu.selector_pos);
		if (subitem == NULL)
			break;

		switch (subitem->type) {

		// Checkbox left navigation
		case MENUITEM_CHECKBOX:
			if (subitem->data.checkbox.value == 0)
				subitem->data.checkbox.value =
				    (subitem->data.checkbox.allow_gray) ? 2 : 1;
			else
				subitem->data.checkbox.value--;

			if (subitem->event_func)
				subitem->event_func(subitem, MENUEVENT_UPDATE);

			return MENURESULT_NONE;

		// Ring left navigation
		case MENUITEM_RING:
			if (subitem->data.ring.value == 0)
				subitem->data.ring.value =
				    LL_Length(subitem->data.ring.strings) - 1;
			else
				subitem->data.ring.value--;

			if (subitem->event_func)
				subitem->event_func(subitem, MENUEVENT_UPDATE);
			return MENURESULT_NONE;

		default:
			break;
		}

		return MENURESULT_NONE;

	// Handle right navigation
	case MENUTOKEN_RIGHT:
		if (!(keymask & MENUTOKEN_RIGHT))
			return MENURESULT_NONE;

		subitem = menu_get_subitem(menu, menu->data.menu.selector_pos);
		if (subitem == NULL)
			break;

		switch (subitem->type) {

		// Checkbox right navigation
		case MENUITEM_CHECKBOX:
			subitem->data.checkbox.value++;
			subitem->data.checkbox.value %= (subitem->data.checkbox.allow_gray) ? 3 : 2;

			if (subitem->event_func)
				subitem->event_func(subitem, MENUEVENT_UPDATE);

			return MENURESULT_NONE;

		// Ring right navigation
		case MENUITEM_RING:
			subitem->data.ring.value++;
			subitem->data.ring.value %= LL_Length(subitem->data.ring.strings);

			if (subitem->event_func)
				subitem->event_func(subitem, MENUEVENT_UPDATE);

			return MENURESULT_NONE;

		// Menu items
		case MENUITEM_MENU:
			return MENURESULT_ENTER;

		default:
			break;
		}

		return MENURESULT_NONE;

	// Handle other input tokens
	case MENUTOKEN_OTHER:

		/**
		 * \todo Implement numeric menu navigation: move to the selected number and enter it
		 *
		 * Feature for direct numeric menu navigation is not implemented. Users should be
		 * able to press number keys (1-9) to jump directly to menu item N and optionally
		 * activate it.
		 *
		 * Desired behavior:
		 * - Press '3' -> jump to 3rd menu item
		 * - Press '3' twice quickly -> jump to 3rd item and activate it
		 * - Press '1' then '2' -> jump to 12th item (multi-digit support)
		 *
		 * Current: MENUTOKEN_OTHER (numeric input) is ignored
		 * Future: Parse numeric input and navigate accordingly
		 *
		 * Impact: User experience, navigation efficiency for long menus
		 *
		 * \ingroup ToDo_medium
		 */

	default:
		return MENURESULT_NONE;
	}

	return MENURESULT_ERROR;
}

// Position current item pointer on entry with given ID
void menu_select_subitem(Menu *menu, char *item_id)
{
	int position;

	assert(menu != NULL);
	position = menu_get_index_of(menu, item_id);

	debug(RPT_DEBUG, "%s(menu=[%s], item_id=\"%s\")", __FUNCTION__, menu->id, item_id);

	if (position < 0) {
		debug(RPT_DEBUG,
		      "%s: item \"%s\" not found"
		      " or hidden in \"%s\", ignored",
		      __FUNCTION__, item_id, menu->id);
		return;
	}

	debug(RPT_DEBUG,
	      "%s: %s->%s is at position %d,"
	      " current item is at menu position: %d, scroll: %d",
	      __FUNCTION__, menu->id, item_id, position, menu->data.menu.selector_pos,
	      menu->data.menu.scroll);
	menu->data.menu.selector_pos = position;
	menu->data.menu.scroll = position;
}
