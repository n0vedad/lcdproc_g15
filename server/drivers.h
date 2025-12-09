// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers.h
 * \brief Driver collection management interface
 * \author Joris Robijn
 * \date 2001
 *
 *
 * \features
 * - Header file for driver collection management interface in LCDd server
 * - Driver loading and unloading with name-based driver identification
 * - Unified driver operations across all loaded drivers (clear, flush, string, char)
 * - Drawing operations for all drivers (bars, icons, cursor, numbers)
 * - Hardware control functions (backlight, contrast, macro LEDs)
 * - Input handling with key retrieval from any input driver
 * - Display property management with DisplayProps structure
 * - Global driver list management with LinkedList integration
 * - Output driver identification and access
 * - Inline functions for driver list iteration
 * - Function declarations for all driver operation wrappers
 * - External variable declarations for global driver state
 *
 * \usage
 * - Used by LCDd server core for managing multiple loaded drivers
 * - Load drivers at server startup via drivers_load_driver() with driver names
 * - Perform operations on all loaded drivers simultaneously via drivers_* functions
 * - Query display properties from output driver via display_props global
 * - Retrieve input from any input driver via drivers_get_key()
 * - Access driver information via drivers_get_info()
 * - Control display hardware via drivers_backlight(), drivers_set_contrast()
 * - Manage display content via drivers_clear(), drivers_flush(), drivers_string()
 * - Server shutdown cleanup via drivers_unload_all()
 *
 * \details Interface for managing multiple loaded drivers and performing
 * operations across all loaded drivers with unified access to functionality.
 */

#ifndef DRIVERS_H
#define DRIVERS_H

#include "drivers/lcd.h"
#include "shared/LL.h"

/**
 * \brief Display properties structure
 * \details Contains display dimensions and cell dimensions
 * for the output driver
 */
typedef struct DisplayProps {
	int width, height;	   /**< Display dimensions in characters */
	int cellwidth, cellheight; /**< Cell dimensions in pixels */
} DisplayProps;

/**
 * \brief Global display properties pointer
 * \details Points to display properties of the current output driver
 */
extern DisplayProps *display_props;

/**
 * \brief Load a driver by name
 * \param name Driver name to load
 * \return 0 on success, -1 on error
 */
int drivers_load_driver(const char *name);

/**
 * \brief Unload all loaded drivers
 */
void drivers_unload_all(void);

/**
 * \brief Get information about loaded drivers
 * \return String containing driver information
 */
const char *drivers_get_info(void);

/**
 * \brief Clear the display on all output drivers
 */
void drivers_clear(void);

/**
 * \brief Flush the display on all output drivers
 */
void drivers_flush(void);

/**
 * \brief Display string on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param string String to display
 */
void drivers_string(int x, int y, const char *string);

/**
 * \brief Display character on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param c Character to display
 */
void drivers_chr(int x, int y, char c);

/**
 * \brief Display vertical bar on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param len Length of bar
 * \param promille Fill level in promille (0-1000)
 * \param pattern Bar pattern/options
 */
void drivers_vbar(int x, int y, int len, int promille, int pattern);

/**
 * \brief Display horizontal bar on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param len Length of bar
 * \param promille Fill level in promille (0-1000)
 * \param pattern Bar pattern/options
 */
void drivers_hbar(int x, int y, int len, int promille, int pattern);

/**
 * \brief Display progress bar with labels on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param width Width of progress bar
 * \param promille Fill level in promille (0-1000)
 * \param begin_label Label at start of bar
 * \param end_label Label at end of bar
 */
void drivers_pbar(int x, int y, int width, int promille, char *begin_label, char *end_label);

/**
 * \brief Display big number on all output drivers
 * \param x Horizontal position
 * \param num Number to display (0-10, 10=colon)
 */
void drivers_num(int x, int num);

/**
 * \brief Display heartbeat on all output drivers
 * \param state Heartbeat state (on/off)
 */
void drivers_heartbeat(int state);

/**
 * \brief Display icon on all output drivers
 * \param x Horizontal position
 * \param y Vertical position
 * \param icon Icon identifier
 */
void drivers_icon(int x, int y, int icon);

/**
 * \brief Set custom character on all output drivers
 * \param ch Character slot to define
 * \param dat Character bitmap data
 */
void drivers_set_char(char ch, unsigned char *dat);

/**
 * \brief Get contrast from output driver
 * \return Current contrast value
 */
int drivers_get_contrast(void);

/**
 * \brief Set contrast on all output drivers
 * \param contrast Contrast value to set
 */
void drivers_set_contrast(int contrast);

/**
 * \brief Set cursor position and state on all output drivers
 * \param x Horizontal cursor position
 * \param y Vertical cursor position
 * \param state Cursor state/type
 */
void drivers_cursor(int x, int y, int state);

/**
 * \brief Set backlight brightness on all output drivers
 * \param brightness Brightness level
 */
void drivers_backlight(int brightness);

/**
 * \brief Set macro LED states on all output drivers
 * \param m1 M1 LED state
 * \param m2 M2 LED state
 * \param m3 M3 LED state
 * \param mr MR LED state
 * \return 0 on success, -1 on error
 */
int drivers_set_macro_leds(int m1, int m2, int m3, int mr);

/**
 * \brief Set output state on all output drivers
 * \param state Output state
 */
void drivers_output(int state);

/**
 * \brief Get key from any input driver
 * \return Key string or NULL if no key available
 */
const char *drivers_get_key(void);

/**
 * \brief Global output driver pointer
 * \details Points to the currently active output driver
 */
extern Driver *output_driver;

/**
 * \brief Global list of loaded drivers
 * \note Please don't access this list directly, use the provided functions
 */
extern LinkedList *loaded_drivers;

/**
 * \brief Get first driver from loaded drivers list
 * \return Pointer to first driver or NULL if list is empty
 */
static inline Driver *drivers_getfirst(void) { return LL_GetFirst(loaded_drivers); }

/**
 * \brief Get next driver from loaded drivers list
 * \return Pointer to next driver or NULL if end of list
 */
static inline Driver *drivers_getnext(void) { return LL_GetNext(loaded_drivers); }

#endif
