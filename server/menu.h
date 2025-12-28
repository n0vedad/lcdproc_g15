// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/menu.h
 * \brief Menu system interface and data structures
 * \author William Ferrell
 * \author Selene Scriven
 * \author F5 Networks, Inc.
 * \author Peter Marschall
 * \date 1999-2005
 *
 *
 * \features
 * - Menu creation and destruction with hierarchical structure support
 * - Menu item management including add, remove, find, and enumeration operations
 * - Menu navigation and selection with current item tracking and state management
 * - Screen building and updating for menu display with widget integration
 * - Input processing for menu interaction with key handling and navigation
 * - Hierarchical menu traversal with parent-child relationships and recursion
 * - Menu association support for linking external data to menu structures
 * - Menu reset functionality for returning to initial state
 * - Predecessor and successor checking for menu navigation flow control
 * - Memory management with proper cleanup and destruction of menu hierarchies
 * - Integration with MenuItem system where Menu is specialized MenuItem
 * - Screen integration for visual representation of menu content
 *
 * \usage
 * - Used by LCDd server for implementing interactive menu systems
 * - Create menu hierarchies via menu_create() with unique identifiers and event handlers
 * - Add menu items via menu_add_item() to build menu structure and navigation
 * - Navigate menus via menu_get_current_item() and menu selection functions
 * - Process user input via menu_process_input() for interactive menu operation
 * - Build visual representation via menu_build_screen() for display rendering
 * - Find specific items via menu_find_item() with optional recursive searching
 * - Manage menu lifecycle via menu_destroy() and menu_destroy_all_items()
 * - Reset menu state via menu_reset() for returning to initial configuration
 *
 * \details Interface for creating, managing and navigating hierarchical menus
 * with comprehensive support for menu operations and visual representation.
 */

#ifndef MENU_H
#define MENU_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#include "shared/defines.h"

#include "menuitem.h"
#include "shared/LL.h"

/**
 * \brief A Menu is a MenuItem too
 * \details This definition is only for better understanding of this code.
 * Menus are implemented as specialized MenuItems that can contain other items.
 */
typedef MenuItem Menu;

/**
 * \brief Creates a new menu
 * \param id Unique identifier for the menu
 * \param event_func Event handler function pointer
 * \param text Display text for the menu
 * \param client Client that owns this menu
 * \retval Menu* Pointer to created menu
 * \retval NULL Creation failed
 */
Menu *menu_create(char *id, MenuEventFunc(*event_func), char *text, Client *client);

/**
 * \brief Deletes menu from memory
 * \param menu Menu to destroy
 * \warning DO NOT CALL THIS FUNCTION, CALL menuitem_destroy INSTEAD!
 *
 * \details Destructors will be called for all subitems.
 */
void menu_destroy(Menu *menu);

/**
 * \brief Adds an item to the menu
 * \param menu Target menu
 * \param item MenuItem to add
 */
void menu_add_item(Menu *menu, MenuItem *item);

/**
 * \brief Removes an item from the menu (does not destroy it)
 * \param menu Target menu
 * \param item MenuItem to remove
 */
void menu_remove_item(Menu *menu, MenuItem *item);

/**
 * \brief Destroys and removes all items from the menu
 * \param menu Menu to clear
 */
void menu_destroy_all_items(Menu *menu);

/**
 * \brief Enumeration function - retrieves the first item from the menu
 * \param menu Menu to enumerate
 * \retval MenuItem* First menu item
 * \retval NULL Menu is empty or NULL
 *
 * \details Retrieves the first item from the list of items in the menu.
 */
static inline MenuItem *menu_getfirst_item(Menu *menu)
{
	return (MenuItem *)((menu != NULL) ? LL_GetFirst(menu->data.menu.contents) : NULL);
}

/**
 * \brief Enumeration function - retrieves the next item from the menu
 * \param menu Menu to enumerate
 * \retval MenuItem* Next menu item
 * \retval NULL No more items or NULL menu
 * \warning No other menu calls should be made between menu_getfirst_item() and
 * this function, to keep the list-cursor where it is.
 *
 * \details Retrieves the next item from the list of items in the menu.
 */
static inline MenuItem *menu_getnext_item(Menu *menu)
{
	return (MenuItem *)((menu != NULL) ? LL_GetNext(menu->data.menu.contents) : NULL);
}

/**
 * \brief Retrieves the current (non-hidden) item from the menu
 * \param menu Menu to query
 * \retval MenuItem* Current menu item
 * \retval NULL No current item or NULL menu
 */
MenuItem *menu_get_current_item(Menu *menu);

/**
 * \brief Finds an item in the menu by the given id
 * \param menu Menu to search
 * \param id Item identifier to find
 * \param recursive True to search recursively in submenus
 * \retval MenuItem* Found menu item
 * \retval NULL Item not found
 */
MenuItem *menu_find_item(Menu *menu, char *id, bool recursive);

/**
 * \brief Sets the association member of a Menu
 * \param menu Menu to modify
 * \param assoc Association data pointer
 */
void menu_set_association(Menu *menu, void *assoc);

/**
 * \brief Resets menu to initial state
 * \param menu Menu to reset
 *
 * \warning DO NOT CALL THIS FUNCTION, CALL menuitem_reset_screen INSTEAD!
 */
void menu_reset(Menu *menu);

/**
 * \brief Builds the selected menuitem on screen using widgets
 * \param menu Menu to build
 * \param s Screen to build on
 *
 * \warning DO NOT CALL THIS FUNCTION, CALL menuitem_rebuild_screen INSTEAD!
 */
void menu_build_screen(Menu *menu, Screen *s);

/**
 * \brief Updates the widgets of the selected menuitem
 * \param menu Menu to update
 * \param s Screen to update
 *
 * \warning DO NOT CALL THIS FUNCTION, CALL menuitem_update_screen INSTEAD!
 */
void menu_update_screen(Menu *menu, Screen *s);

/**
 * \brief For predecessor-Check: returns selected subitem or menu
 * \param menu Menu to check
 * \retval MenuItem* Selected subitem if it has no own screen and has a predecessor, menu otherwise
 * \retval NULL Error occurred
 *
 * \details Returns selected subitem of menu if this subitem
 * has no own screen (action, checkbox, ...) and this subitem has a
 * predecessor and menu otherwise.
 */
MenuItem *menu_get_item_for_predecessor_check(Menu *menu);

/**
 * \brief For successor-Check: returns selected subitem or menu
 * \param menu Menu to check
 * \retval MenuItem* Selected subitem if it has no own screen, menu otherwise
 * \retval NULL Error occurred
 *
 * \details Returns selected subitem of menu if
 * this subitem has no own screen (action, checkbox, ...) or menu
 * otherwise.
 */
MenuItem *menu_get_item_for_successor_check(Menu *menu);

/**
 * \brief Processes input for the menu
 * \param menu Menu to process input for
 * \param token Input token type
 * \param key Key string (only used if token is MENUTOKEN_OTHER)
 * \param keymask Key modifier mask
 * \retval MenuResult Result of input processing
 * \warning DO NOT CALL THIS FUNCTION, CALL menuitem_process_input INSTEAD!
 *
 * \details Does something with the given input.
 * Key is only used if token is MENUTOKEN_OTHER.
 */
MenuResult menu_process_input(Menu *menu, MenuToken token, const char *key, unsigned int keymask);

/**
 * \brief Positions current item pointer on subitem
 * \param menu Menu to modify
 * \param item_id Item identifier to select
 */
void menu_select_subitem(Menu *menu, char *item_id);
#endif
