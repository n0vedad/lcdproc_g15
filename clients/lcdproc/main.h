// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/main.h
 * \brief Core definitions, structures, and function declarations for lcdproc client
 * \author William Ferrell, Selene Scriven, n0vedad
 * \date 1999-2025
 *
 * \features
 * - Screen mode structure and management flags
 * - Battery and AC adapter status definitions
 * - LCD display dimension constants
 * - Global variable declarations
 * - System information function prototypes
 * - Protocol version checking
 * - G-Key macro system integration
 *
 * \details Header file containing core data structures, constants, and function
 * declarations for the lcdproc client. Defines screen mode management structures,
 * battery and AC power status constants, LCD dimension limits, and global variable
 * declarations.
 */

#ifndef MAIN_H
#define MAIN_H

#include "gkey_macro.h"
#include "shared/defines.h"

#ifndef TRUE
#define TRUE 1 ///< Boolean true value
#endif
#ifndef FALSE
#define FALSE 0 ///< Boolean false value
#endif

/** \name Battery Status Constants
 * Battery charge level and status definitions
 */
///@{
#define LCDP_BATT_HIGH 0x00	///< Battery charge level is high
#define LCDP_BATT_LOW 0x01	///< Battery charge level is low
#define LCDP_BATT_CRITICAL 0x02 ///< Battery charge level is critical
#define LCDP_BATT_CHARGING 0x03 ///< Battery is currently charging
#define LCDP_BATT_ABSENT 0x04	///< No battery present
#define LCDP_BATT_UNKNOWN 0xFF	///< Battery status unknown
///@}

/** \name AC Adapter Status Constants
 * AC power adapter status definitions
 */
///@{
#define LCDP_AC_OFF 0x00     ///< AC adapter is disconnected
#define LCDP_AC_ON 0x01	     ///< AC adapter is connected and providing power
#define LCDP_AC_BACKUP 0x02  ///< AC adapter is on backup power
#define LCDP_AC_UNKNOWN 0x03 ///< AC adapter status unknown
///@}

/** \name Global Program State
 * Core program state variables
 */
///@{
extern int Quit;	 ///< Global quit flag for main loop termination
extern int sock;	 ///< Socket descriptor for server connection
extern char *version;	 ///< Program version string
extern char *build_date; ///< Build date string
///@}

/** \name LCD Display Properties
 * Current LCD display dimensions and characteristics
 */
///@{
extern int lcd_wid;	///< LCD width in characters
extern int lcd_hgt;	///< LCD height in characters
extern int lcd_cellwid; ///< LCD cell width in pixels
extern int lcd_cellhgt; ///< LCD cell height in pixels
///@}

/**
 * \brief Screen mode configuration and state structure.
 *
 * \details Defines a screen mode with its configuration parameters,
 * timing settings, state flags, and associated update function.
 * Each screen mode represents a different type of system information
 * display (CPU, memory, network, etc.).
 */
typedef struct _screen_mode {
	char *longname;	    ///< Display name of the screen (e.g., "CPU")
	char which;	    ///< Single character identifier (e.g., 'C' for CPU)
	int on_time;	    ///< Update interval when screen is visible (in time units)
	int off_time;	    ///< Update interval when screen is not visible (in time units)
	int show_invisible; ///< Whether to update data when screen is not visible
	int timer;	    ///< Time units elapsed since last update
	int flags;	    ///< State flags (VISIBLE, ACTIVE, INITIALIZED)
	int (*func)(int, int, int *); ///< Function pointer to screen update function
} ScreenMode;

/** \name Screen Mode Flags
 * State flags for screen mode management
 */
///@{
#define VISIBLE 0x00000001     ///< Screen is currently visible on display
#define ACTIVE 0x00000002      ///< Screen is selected for display rotation
#define INITIALIZED 0x00000004 ///< Screen has been initialized
///@}

/** \name Display Control Constants
 * Display state and backlight control definitions
 */
///@{
#define BLINK_ON 0x10	   ///< Enable backlight blinking
#define BLINK_OFF 0x11	   ///< Disable backlight blinking
#define BACKLIGHT_OFF 0x20 ///< Turn backlight off
#define BACKLIGHT_ON 0x21  ///< Turn backlight on
#define HOLD_SCREEN 0x30   ///< Hold current screen display
#define CONTINUE 0x31	   ///< Continue normal screen rotation
///@}

/** \name LCD Dimension Limits
 * Maximum supported LCD dimensions
 */
///@{
#define LCD_MAX_WIDTH 80  ///< Maximum LCD width in characters
#define LCD_MAX_HEIGHT 80 ///< Maximum LCD height in characters
///@}

/**
 * \brief Get the hostname of this machine.
 * \retval hostname Network name of the machine (may include leading space)
 */
const char *get_hostname(void);

/**
 * \brief Get the operating system name.
 * \retval sysname Name of the operating system (e.g., "Linux")
 */
const char *get_sysname(void);

/**
 * \brief Get the operating system release version.
 * \retval release OS release version string (e.g., "5.4.0")
 */
const char *get_sysrelease(void);

#endif
