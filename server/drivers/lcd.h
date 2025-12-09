// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/lcd.h
 * \brief LCDd driver API definition
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2001
 *
 *
 * \features
 * - Core LCDd driver API definition for loadable driver module interface
 * - Driver structure definition with function pointers for all LCD operations
 * - Standard LCD display operations: init, close, clear, flush, width, height
 * - Text and character output functions: string, chr with position control
 * - Extended drawing functions: bars (hbar/vbar), icons, cursor positioning
 * - Hardware control functions: contrast, brightness, backlight, macro LEDs
 * - Input functions for key reading and event processing
 * - Configuration file access functions for driver-specific settings
 * - Display property access functions for driver adaptation
 * - Custom character generation support (CGRAM management)
 * - Icon definitions for standard LCD symbols and media controls
 * - Bar rendering options and patterns (seamless, filled, etc.)
 * - Cursor type definitions (off, block, underline)
 * - CGmode enumeration for character generator RAM state tracking
 * - Module loading and symbol export macros
 * - Memory management guidelines and private data handling
 *
 * \usage
 * - Used by all LCDd drivers as the primary API interface
 * - Driver modules implement function pointers in the Driver structure
 * - Server core loads drivers dynamically and calls functions via this API
 * - Provides abstraction layer between server core and hardware drivers
 * - Used for driver development - see docs/lcdproc-dev/ directory
 * - Configuration access via config_get_* functions in driver init
 * - Hardware control via set_contrast, set_brightness, backlight functions
 * - Display rendering via string, chr, hbar, vbar, icon functions
 * - Input handling via get_key function for interactive drivers
 *
 * \details This file defines the LCDd-driver API to facilitate loadable
 * driver modules with no further interaction between driver and server core
 * other than via this API.
 */

#ifndef LCD_H
#define LCD_H

#include <stddef.h>

/** \name LCD Display Dimensions
 * Maximum and default display size constraints
 */
///@{
#define LCD_MAX_WIDTH 256	 ///< Maximum display width in characters
#define LCD_MAX_HEIGHT 256	 ///< Maximum display height in characters
#define LCD_DEFAULT_WIDTH 20	 ///< Default display width (20 columns)
#define LCD_DEFAULT_HEIGHT 4	 ///< Default display height (4 rows)
#define LCD_DEFAULT_CELLWIDTH 5	 ///< Default character cell width in pixels
#define LCD_DEFAULT_CELLHEIGHT 8 ///< Default character cell height in pixels
///@}

/** \name Backlight Control
 * Backlight on/off states
 */
///@{
#define BACKLIGHT_OFF 0 ///< Backlight disabled
#define BACKLIGHT_ON 1	///< Backlight enabled
///@}

/**
 * \note Icons. If a driver does not support an icon, it can return -1 from the
 * icon function, and let the core place a replacement character.
 */

/** \name Single-Width Icons
 * Icons that occupy one character cell
 */
///@{
#define ICON_BLOCK_FILLED 0x100	     ///< Filled block character
#define ICON_HEART_OPEN 0x108	     ///< Hollow heart symbol
#define ICON_HEART_FILLED 0x109	     ///< Filled heart symbol
#define ICON_ARROW_UP 0x110	     ///< Upward arrow
#define ICON_ARROW_DOWN 0x111	     ///< Downward arrow
#define ICON_ARROW_LEFT 0x112	     ///< Leftward arrow
#define ICON_ARROW_RIGHT 0x113	     ///< Rightward arrow
#define ICON_CHECKBOX_OFF 0x120	     ///< Unchecked checkbox
#define ICON_CHECKBOX_ON 0x121	     ///< Checked checkbox
#define ICON_CHECKBOX_GRAY 0x122     ///< Disabled/gray checkbox
#define ICON_SELECTOR_AT_LEFT 0x128  ///< Selection indicator at left
#define ICON_SELECTOR_AT_RIGHT 0x129 ///< Selection indicator at right
#define ICON_ELLIPSIS 0x130	     ///< Ellipsis symbol (...)
///@}

/** \name Double-Width Media Icons
 * Media control icons that occupy two character cells
 */
///@{
#define ICON_STOP 0x200	 ///< Stop icon (should look like [])
#define ICON_PAUSE 0x201 ///< Pause icon (should look like ||)
#define ICON_PLAY 0x202	 ///< Play icon (should look like >)
#define ICON_PLAYR 0x203 ///< Play reverse icon (should look like <)
#define ICON_FF 0x204	 ///< Fast forward icon (should look like >>)
#define ICON_FR 0x205	 ///< Fast rewind icon (should look like <<)
#define ICON_NEXT 0x206	 ///< Next track icon (should look like >|)
#define ICON_PREV 0x207	 ///< Previous track icon (should look like |<)
#define ICON_REC 0x208	 ///< Record icon (should look like ())
///@}

/**
 * \note Icon numbers from 0 to 0x0FF could be used for client defined chars.
 * However this is not implemented and there are no crystallized ideas
 * about how to do it. get_free_chars and set_char should be used, but a
 * lot of things in that area might need to be changed.
 */

/** \name Heartbeat Control
 * Heartbeat indicator states
 */
///@{
#define HEARTBEAT_OFF 0 ///< Heartbeat indicator disabled
#define HEARTBEAT_ON 1	///< Heartbeat indicator enabled
///@}

/**
 * \todo Implement missing bar patterns for hbar/vbar functions:
 * - BAR_NEG: Negative direction bars (promille -1000 to 0)
 * - BAR_POS_AND_NEG: Bidirectional bars (promille -1000 to +1000)
 * - BAR_PATTERN_OPEN: Hollow/open bar rendering
 * - BAR_PATTERN_STRIPED: Striped bar pattern
 * - BAR_WITH_PERCENTAGE: Bars with percentage text display
 *
 * Currently only BAR_POS (standard) and partial BAR_SEAMLESS are implemented.
 * These patterns would enhance bar visualization capabilities across all drivers.
 *
 * \ingroup ToDo_medium
 */

/** \name Bar Display Modes
 * Bar graph direction and pattern options
 */

///@{
#define BAR_POS 0x001		  ///< Positive direction bars (0 to +1000 promille)
#define BAR_NEG 0x002		  ///< Negative direction bars (-1000 to 0 promille)
#define BAR_POS_AND_NEG 0x003	  ///< Bidirectional bars (-1000 to +1000 promille)
#define BAR_PATTERN_FILLED 0x000  ///< Default filled bar pattern
#define BAR_PATTERN_OPEN 0x010	  ///< Hollow/open bar rendering
#define BAR_PATTERN_STRIPED 0x020 ///< Striped bar pattern
#define BAR_SEAMLESS 0x040	  ///< Seamless bar rendering without gaps
#define BAR_WITH_PERCENTAGE 0x100 ///< Bar with percentage text display
///@}

/** \name Cursor Types
 * Cursor display styles
 */
///@{
#define CURSOR_OFF 0	    ///< Cursor disabled
#define CURSOR_DEFAULT_ON 1 ///< Default cursor style enabled
#define CURSOR_BLOCK 4	    ///< Block cursor style
#define CURSOR_UNDER 5	    ///< Underline cursor style
///@}

/**
 * \brief CGRAM (Character Generator RAM) content modes
 *
 * \details Defines what type of custom characters are currently loaded
 * in the display's character generator RAM.
 */
typedef enum {
	standard, ///< One char is used for heartbeat animation
	vbar,	  ///< Vertical bar graph characters
	hbar,	  ///< Horizontal bar graph characters
	icons,	  ///< Standard icon set
	custom,	  ///< Custom user-defined characters
	bignum,	  ///< Large number display characters
} CGmode;

/** \name Module Export Definitions
 * Platform-specific module loading definitions
 */
///@{
#define MODULE_HANDLE void * ///< Shared library handle type
#define MODULE_EXPORT	     ///< Module function export marker
///@}

/**
 * \brief LCD driver function pointer table structure
 * \details Defines the interface for LCD display drivers with function pointers
 * for all driver operations
 */
typedef struct lcd_logical_driver {

	char **api_version;	 ///< Driver API version string
	int *stay_in_foreground; ///< Does this driver require foreground mode?
	int *supports_multiple;	 ///< Does this driver support multiple instances?
	char **symbol_prefix;	 ///< Alternative function name prefix

	// Mandatory functions (necessary for all drivers)
	int (*init)(struct lcd_logical_driver *drvthis);   ///< Initialize driver
	void (*close)(struct lcd_logical_driver *drvthis); ///< Close driver

	// Essential output functions (necessary for output drivers)
	int (*width)(struct lcd_logical_driver *drvthis);
	int (*height)(struct lcd_logical_driver *drvthis);
	void (*clear)(struct lcd_logical_driver *drvthis);
	void (*flush)(struct lcd_logical_driver *drvthis);
	void (*string)(struct lcd_logical_driver *drvthis, int x, int y, const char *str);
	void (*chr)(struct lcd_logical_driver *drvthis, int x, int y, char c);

	// Essential input functions (necessary for all input drivers)
	const char *(*get_key)(struct lcd_logical_driver *drvthis);

	// Extended output functions (optional; core provides alternatives)
	void (*vbar)(struct lcd_logical_driver *drvthis, int x, int y, int len, int promille,
		     int pattern);
	void (*hbar)(struct lcd_logical_driver *drvthis, int x, int y, int len, int promille,
		     int pattern);
	void (*pbar)(struct lcd_logical_driver *drvthis, int x, int y, int width, int promille);
	void (*num)(struct lcd_logical_driver *drvthis, int x, int num);
	void (*heartbeat)(struct lcd_logical_driver *drvthis, int state);
	int (*icon)(struct lcd_logical_driver *drvthis, int x, int y, int icon);
	void (*cursor)(struct lcd_logical_driver *drvthis, int x, int y, int type);

	// user-defined character functions, are those still supported ?
	void (*set_char)(struct lcd_logical_driver *drvthis, int n, unsigned char *dat);
	int (*get_free_chars)(struct lcd_logical_driver *drvthis);
	int (*cellwidth)(struct lcd_logical_driver *drvthis);
	int (*cellheight)(struct lcd_logical_driver *drvthis);

	// Hardware functions
	int (*get_contrast)(struct lcd_logical_driver *drvthis);
	void (*set_contrast)(struct lcd_logical_driver *drvthis, int promille);
	int (*get_brightness)(struct lcd_logical_driver *drvthis, int state);
	void (*set_brightness)(struct lcd_logical_driver *drvthis, int state, int promille);
	void (*backlight)(struct lcd_logical_driver *drvthis, int on);
	int (*set_macro_leds)(struct lcd_logical_driver *drvthis, int m1, int m2, int m3, int mr);
	void (*output)(struct lcd_logical_driver *drvthis, int state);

	// informational functions
	const char *(*get_info)(struct lcd_logical_driver *drvthis);
	char *name;	// Name of this driver
	char *filename; // Filename of the shared module

	/**
	 * \note The handle of the loaded shared module
	 * (platform specific)
	 */
	MODULE_HANDLE module_handle;

	/**
	 * \note Filled by server by calling store_private_ptr().
	 * Driver should cast this to its own
	 * private structure pointer
	 */
	void *private_data;

	// Store the driver's private data
	int (*store_private_ptr)(struct lcd_logical_driver *driver, void *private_data);

	// Configfile functions
	short (*config_get_bool)(const char *sectionname, const char *keyname, int skip,
				 short default_value);
	long int (*config_get_int)(const char *sectionname, const char *keyname, int skip,
				   long int default_value);
	double (*config_get_float)(const char *sectionname, const char *keyname, int skip,
				   double default_value);
	const char *(*config_get_string)(const char *sectionname, const char *keyname, int skip,
					 const char *default_value);
	int (*config_has_section)(const char *sectionname);
	int (*config_has_key)(const char *sectionname, const char *keyname);

	// Display properties functions (for drivers that adapt to other loaded drivers)
	int (*request_display_width)();
	int (*request_display_height)();

} Driver;

#endif
