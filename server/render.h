// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/render.h
 * \brief Screen rendering and display management interface
 * \author William Ferrell
 * \author Selene Scriven
 * \date 1999
 *
 * \features
 * - Screen rendering and display output
 * - Heartbeat indicator management
 * - Backlight control and effects
 * - Cursor positioning and styling
 * - Title scrolling speed control
 * - Server message display
 *
 * \usage
 * - Heartbeat states (OFF, ON, OPEN)
 * - Backlight states and effects (BLINK, FLASH)
 * - Cursor types (OFF, BLOCK, UNDER)
 * - Title scrolling speed settings
 *
 * \details Provides interface for rendering screens to the display hardware,
 * managing display properties like heartbeat, backlight, and cursor settings.
 * Handles the conversion from logical screen representation to physical display output.
 */

#ifndef RENDER_H
#define RENDER_H

#include "screen.h"

/** \name Heartbeat Control Constants
 * Heartbeat indicator states
 */
///@{
#define HEARTBEAT_OFF 0	 ///< Heartbeat indicator disabled
#define HEARTBEAT_ON 1	 ///< Heartbeat indicator enabled
#define HEARTBEAT_OPEN 2 ///< Heartbeat indicator open (client controlled)
///@}

/** \name Backlight Control Constants
 * Backlight states and effects
 */
///@{
#define BACKLIGHT_OFF 0	      ///< Backlight disabled
#define BACKLIGHT_ON 1	      ///< Backlight enabled
#define BACKLIGHT_OPEN 2      ///< Backlight open (client controlled)
#define BACKLIGHT_BLINK 0x100 ///< Backlight blink effect
#define BACKLIGHT_FLASH 0x200 ///< Backlight flash effect
///@}

/** \name Cursor Control Constants
 * Cursor display types
 */
///@{
#define CURSOR_OFF 0	    ///< Cursor disabled
#define CURSOR_DEFAULT_ON 1 ///< Default cursor style
#define CURSOR_BLOCK 4	    ///< Block cursor
#define CURSOR_UNDER 5	    ///< Underline cursor
///@}

/** \name Title Speed Constants
 * Title scrolling speed settings
 */
///@{
#define TITLESPEED_NO 0	  ///< No title scrolling (needs to be TITLESPEED_MIN - 1)
#define TITLESPEED_MIN 1  ///< Minimum title scrolling speed
#define TITLESPEED_MAX 10 ///< Maximum title scrolling speed
///@}

/** \name Global Render State Variables
 * Current display settings
 */
///@{
extern int heartbeat;	 ///< Current heartbeat setting
extern int backlight;	 ///< Current backlight setting
extern int titlespeed;	 ///< Current title scrolling speed
extern int output_state; ///< Current output state
///@}

/**
 * \brief Renders a screen to the display
 * \param s Screen to render
 * \param timer Current timer value for animations
 * \retval 0 Success
 * \retval <0 Rendering failed
 *
 * \details Converts the logical screen representation to physical
 * display output, handling widgets, text positioning, and effects.
 */
int render_screen(Screen *s, long timer);

/**
 * \brief Displays a short server message
 * \param text Message text (must be shorter than 16 characters)
 * \param expire Expiration time for the message
 * \retval 0 Success
 * \retval <0 Display failed
 *
 * \details Displays a short message in a corner of the screen,
 * typically used for server status or error messages.
 */
int server_msg(const char *text, int expire);

#endif
