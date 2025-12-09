// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/serverscreens.h
 * \brief Server screen management interface
 * \author William Ferrell
 * \author Selene Scriven
 * \date 1999
 *
 * \features
 * - Server status screen creation and management
 * - Welcome and goodbye screen display
 * - Server screen rotation control
 * - System information display
 * - Blank screen mode support
 * - Automatic screen updates
 *
 * \usage
 * - Interface for server-generated screens
 * - Display system information and server status
 * - Welcome/goodbye message management
 * - Screen rotation and fallback control
 * - Function prototypes for server screen operations
 *
 * \details Interface for the serverscreen implementation. Provides
 * functionality for managing server-generated screens that display
 * system information, server status, and welcome/goodbye messages.
 * Server screens are shown when no client screens are available,
 * can be configured to show in rotation or only as fallback,
 * support blank screen mode for minimal display, and automatically
 * update with current server information.
 */

#ifndef SRVSTATS_H
#define SRVSTATS_H

#include "screen.h"

/** \name Server Screen Rotation States
 * Control modes for server screen display
 */
///@{
#define SERVERSCREEN_OFF 0   ///< Show server screen in rotation
#define SERVERSCREEN_ON 1    ///< Show server screen only when there is no other screen
#define SERVERSCREEN_BLANK 2 ///< Don't rotate, and only show a blank screen
///@}

extern Screen *server_screen;	 ///< Global server screen instance
extern int rotate_server_screen; ///< Server screen rotation setting

/**
 * \brief Initializes the server screen system
 * \retval 0 Success
 * \retval <0 Initialization failed
 *
 * \details Sets up the server screen and prepares it for display.
 * Creates the necessary widgets and initializes system information.
 */
int server_screen_init(void);

/**
 * \brief Shuts down the server screen system
 * \retval 0 Success
 * \retval <0 Shutdown failed
 *
 * \details Cleans up server screen resources and destroys
 * associated widgets and data structures.
 */
int server_screen_shutdown(void);

/**
 * \brief Updates the server screen content
 * \retval 0 Success
 * \retval <0 Update failed
 *
 * \details Refreshes the server screen with current system
 * information and server status.
 */
int update_server_screen(void);

/**
 * \brief Displays the goodbye screen
 * \retval 0 Success
 * \retval <0 Display failed
 *
 * \details Shows a farewell message when the server is shutting down.
 * Typically displayed briefly before the server terminates.
 */
int goodbye_screen(void);

#endif
