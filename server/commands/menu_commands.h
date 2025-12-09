// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/menu_commands.h
 * \brief Menu system command function declarations for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn, n0vedad
 * \date 1999-2025
 *
 * \features
 * - **menu_add_item_func()**: Create new menu items and submenus
 * - **menu_del_item_func()**: Remove menu items from hierarchy
 * - **menu_set_item_func()**: Modify existing menu item properties
 * - **menu_goto_func()**: Navigate to specific menu locations
 * - **menu_set_main_func()**: Set the main/root menu for the client
 * - Hierarchical menu structure with parent-child relationships
 * - Multiple menu item types (menu, action, checkbox, ring, slider, numeric, alpha, IP)
 * - Dynamic menu creation and modification during runtime
 * - Menu navigation and event handling
 * - Client-specific menu management and isolation
 * - Menu validation and error handling
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Functions are called by the command parser for menu-related commands
 * - Used for interactive menu system in LCDd server
 * - Provides client menu management functionality
 * - Used by protocol parser to handle menu protocol commands
 *
 * *
 * \details
 * Header file containing function declarations for menu system
 * commands in the LCDd server. These functions handle the interactive
 * menu system that allows clients to create hierarchical menus for
 * display navigation and configuration.
 */

#ifndef COMMANDS_MENU_H
#define COMMANDS_MENU_H

/**
 * \brief Handle menu_add_item command for creating menu items.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "menu_add_item" commands to create new menu items
 * and submenus in the client's menu hierarchy. Supports various item
 * types including actions, checkboxes, rings, sliders, and input fields.
 */
int menu_add_item_func(Client *c, int argc, char **argv);

/**
 * \brief Handle menu_del_item command for removing menu items.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "menu_del_item" commands to remove menu items
 * from the client's menu hierarchy. Handles cleanup of child items
 * and updates parent menu references.
 */

int menu_del_item_func(Client *c, int argc, char **argv);
/**
 * \brief Handle menu_set_item command for modifying menu items.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "menu_set_item" commands to modify properties
 * of existing menu items such as text, values, visibility, and
 * navigation behavior.
 */

int menu_set_item_func(Client *c, int argc, char **argv);
/**
 * \brief Handle menu_goto command for menu navigation.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "menu_goto" commands to navigate to specific
 * menu locations in the client's menu hierarchy. Updates the current
 * menu position and refreshes the display.
 */

int menu_goto_func(Client *c, int argc, char **argv);
/**
 * \brief Handle menu_set_main command for setting the root menu.
 * \param c Client connection context
 * \param argc Number of command arguments
 * \param argv Array of command argument strings
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Processes "menu_set_main" commands to set the main/root
 * menu for the client. This menu becomes the entry point for the
 * client's menu system.
 */

int menu_set_main_func(Client *c, int argc, char **argv);

#endif
