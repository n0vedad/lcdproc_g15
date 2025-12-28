// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/menuscreens.h
 * \brief Menu screen management and keyboard handling interface
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 * \features
 * - Menu screen initialization and management
 * - Keyboard input handling for menu navigation
 * - Screen addition and removal from menu system
 * - Menu switching and navigation control
 * - Item modification and destruction notifications
 *
 * \usage
 * - Initialize menu screens with menuscreens_init()
 * - Handle keyboard input with menuscreen_key_handler()
 * - Add/remove screens with menuscreen_add_screen()/menuscreen_remove_screen()
 * - Navigate menus with menuscreen_goto()
 * - Set custom main menu with menuscreen_set_main()
 *
 * \details Creates all menuscreens, menus and handles the keypresses for the
 * menuscreens. Provides interface for menu navigation, screen management,
 * and user input processing.
 */

#ifndef MENUSCREENS_H
#define MENUSCREENS_H

#include "menu.h"
#include "screen.h"

/** \brief Global menu screen instance
 *
 * \details Screen object used for displaying the menu system. Created during
 * menuscreens_init() and destroyed during menuscreens_shutdown().
 */
extern Screen *menuscreen;

/** \brief Global main menu root
 *
 * \details Root of the main menu tree. Created during menuscreens_init().
 */
extern Menu *main_menu;

/**
 * \brief Initializes the menu screen system
 * \retval 0 Success
 * \retval <0 Initialization failed
 */
int menuscreens_init(void);

/**
 * \brief Shuts down the menu screen system
 * \retval 0 Success
 * \retval <0 Shutdown failed
 */
int menuscreens_shutdown(void);

/**
 * \brief Checks if a key is the reserved menu key
 * \param key Key string to check
 * \retval true Key is the menu key
 * \retval false Key is not the menu key
 *
 * \details This function indicates to the input part whether this key was the
 * reserved menu key.
 */
bool is_menu_key(const char *key);

/**
 * \brief Notifies menu screen that an item is being destroyed
 * \param item Menu item that is about to be removed
 *
 * \details Meant for other parts of the program to inform the menuscreen that the
 * item is about to be removed.
 */
void menuscreen_inform_item_destruction(MenuItem *item);

/**
 * \brief Notifies menu screen that an item has been modified
 * \param item Menu item that has been modified
 *
 * \details Meant for other parts of the program to inform the menuscreen that some
 * properties of the item have been modified.
 */
void menuscreen_inform_item_modified(MenuItem *item);

/**
 * \brief Handles keyboard input for menu navigation
 * \param key Key string pressed by user
 *
 * \details This handler processes all keypresses for the menu system,
 * including navigation, selection, and menu operations.
 */
void menuscreen_key_handler(const char *key);

/**
 * \brief Adds a screen to the menu system
 * \param s Screen to add to menu
 *
 * \details Creates a menu entry for the given screen and adds it to
 * the menu system for navigation.
 */
void menuscreen_add_screen(Screen *s);

/**
 * \brief Removes a screen from the menu system
 * \param s Screen to remove from menu
 *
 * \details Removes the menu entry for the given screen and cleans up
 * any associated menu items.
 */
void menuscreen_remove_screen(Screen *s);

/**
 * \brief Switches to the specified menu
 * \param menu Target menu to switch to
 * \retval 0 Success
 * \retval <0 Navigation failed
 *
 * \details Navigates to the specified menu and updates the display.
 */
int menuscreen_goto(Menu *menu);

/**
 * \brief Sets a custom main menu
 * \param menu Menu to set as main menu
 * \retval 0 Success
 * \retval <0 Setting failed
 *
 * \details Replaces the default main menu with the specified custom menu.
 */
int menuscreen_set_main(Menu *menu);

#endif
