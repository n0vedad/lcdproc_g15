// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/debug.h
 * \brief Debug driver header for LCDd testing and debugging
 * \author Peter Marschall
 * \author Markus Dolze
 * \date 2008-2010
 *
 *
 * \features
 * - Virtual LCD display simulation for testing and debugging purposes
 * - Debug output for all LCD operations instead of controlling actual hardware
 * - Frame buffer implementation for virtual display state
 * - Standard LCD driver interface compliance and compatibility
 * - Core driver functions: init, close, width, height, clear, flush
 * - Graphics functions: vbar, hbar, num, icon, cursor, custom characters
 * - Display control: contrast, brightness, backlight, output control
 * - Key input simulation and handling
 * - Driver information reporting
 * - Memory management for virtual display buffer
 *
 * \usage
 * - Include this header in LCDd server for debug driver support
 * - Used for testing LCD applications without physical hardware
 * - Used for debugging driver implementations and development
 * - Used for development and validation of LCD functionality
 * - Driver automatically selected when "debug" is specified in configuration
 * - All LCD operations output debug messages for inspection
 *
 * \details Header file for the debug driver that provides a virtual LCD
 * display for testing and debugging purposes. This driver outputs all
 * operations as debug messages instead of controlling actual hardware.
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "lcd.h"

/**
 * \brief Initialize the debug driver
 * \param drvthis Pointer to driver structure
 * \retval 0 Success
 * \retval -1 Error during initialization
 *
 * \details Initializes the debug driver by allocating virtual display
 * buffer and setting up debug output for testing purposes.
 */
MODULE_EXPORT int debug_init(Driver *drvthis);

/**
 * \brief Close the debug driver and clean up resources
 * \param drvthis Pointer to driver structure
 *
 * \details Performs cleanup by freeing virtual display buffer
 * and releasing allocated memory.
 */
MODULE_EXPORT void debug_close(Driver *drvthis);

/**
 * \brief Return the display width in characters
 * \param drvthis Pointer to driver structure
 * \return Display width in characters
 *
 * \details Returns the number of character columns available on the virtual display.
 */
MODULE_EXPORT int debug_width(Driver *drvthis);

/**
 * \brief Return the display height in characters
 * \param drvthis Pointer to driver structure
 * \return Display height in characters
 *
 * \details Returns the number of character rows available on the virtual display.
 */
MODULE_EXPORT int debug_height(Driver *drvthis);

/**
 * \brief Return the width of a character cell in pixels
 * \param drvthis Pointer to driver structure
 * \return Character cell width in pixels
 *
 * \details Returns the pixel width of a single character cell on the virtual display.
 */
MODULE_EXPORT int debug_cellwidth(Driver *drvthis);

/**
 * \brief Return the height of a character cell in pixels
 * \param drvthis Pointer to driver structure
 * \return Character cell height in pixels
 *
 * \details Returns the pixel height of a single character cell on the virtual display.
 */
MODULE_EXPORT int debug_cellheight(Driver *drvthis);

/**
 * \brief Clear the virtual LCD screen
 * \param drvthis Pointer to driver structure
 *
 * \details Clears the virtual display buffer and outputs debug message.
 */
MODULE_EXPORT void debug_clear(Driver *drvthis);

/**
 * \brief Flush the frame buffer to debug output
 * \param drvthis Pointer to driver structure
 *
 * \details Outputs the current virtual display state as debug messages
 * for inspection and testing purposes.
 */
MODULE_EXPORT void debug_flush(Driver *drvthis);

/**
 * \brief Print a string on the virtual display at specified position
 * \param drvthis Pointer to driver structure
 * \param x Horizontal starting position
 * \param y Vertical position
 * \param string String to display
 *
 * \details Places a string in the virtual display buffer and outputs
 * debug message with position and content information.
 */
MODULE_EXPORT void debug_string(Driver *drvthis, int x, int y, const char string[]);

/**
 * \brief Place a single character on the virtual display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Vertical position
 * \param c Character to display
 *
 * \details Places a character in the virtual display buffer and outputs
 * debug message with position and character information.
 */
MODULE_EXPORT void debug_chr(Driver *drvthis, int x, int y, char c);

/**
 * \brief Get key input from virtual keyboard
 * \param drvthis Pointer to driver structure
 * \return Key string or NULL if no key pressed
 *
 * \details Returns simulated key input for testing purposes.
 * Always returns NULL in current implementation.
 */
MODULE_EXPORT const char *debug_get_key(Driver *drvthis);

/**
 * \brief Draw a vertical bar on the virtual display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Starting vertical position (bottom)
 * \param len Maximum height of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options
 *
 * \details Simulates drawing a vertical progress bar and outputs
 * debug message with position, length, and fill level information.
 */
MODULE_EXPORT void debug_vbar(Driver *drvthis, int x, int y, int len, int promille, int options);

/**
 * \brief Draw a horizontal bar on the virtual display
 * \param drvthis Pointer to driver structure
 * \param x Starting horizontal position
 * \param y Vertical position
 * \param len Maximum length of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options
 *
 * \details Simulates drawing a horizontal progress bar and outputs
 * debug message with position, length, and fill level information.
 */
MODULE_EXPORT void debug_hbar(Driver *drvthis, int x, int y, int len, int promille, int options);

/**
 * \brief Display a large number on the virtual display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position for the number
 * \param num Number to display
 *
 * \details Simulates rendering a large number and outputs
 * debug message with position and number information.
 */
MODULE_EXPORT void debug_num(Driver *drvthis, int x, int num);

/**
 * \brief Draw an icon on the virtual display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Vertical position
 * \param icon Icon identifier
 * \retval 0 Icon handled by driver
 * \retval -1 Let core handle icon
 *
 * \details Simulates drawing an icon and outputs debug message
 * with position and icon identifier information.
 */
MODULE_EXPORT int debug_icon(Driver *drvthis, int x, int y, int icon);

/**
 * \brief Set cursor position and type on virtual display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal cursor position
 * \param y Vertical cursor position
 * \param type Cursor type identifier
 *
 * \details Simulates cursor positioning and outputs debug message
 * with cursor position and type information.
 */
MODULE_EXPORT void debug_cursor(Driver *drvthis, int x, int y, int type);

/**
 * \brief Define a custom character in virtual display
 * \param drvthis Pointer to driver structure
 * \param n Character number to define
 * \param dat Character bitmap data
 *
 * \details Simulates custom character definition and outputs
 * debug message with character number and data information.
 */
MODULE_EXPORT void debug_set_char(Driver *drvthis, int n, char *dat);

/**
 * \brief Get number of free custom character slots
 * \param drvthis Pointer to driver structure
 * \return Number of free custom character slots
 *
 * \details Returns the number of available custom character slots
 * in the virtual display for testing purposes.
 */
MODULE_EXPORT int debug_get_free_chars(Driver *drvthis);

/**
 * \brief Get current contrast setting of virtual display
 * \param drvthis Pointer to driver structure
 * \return Current contrast value in promille
 *
 * \details Returns the simulated contrast setting for testing
 * display control functionality.
 */
MODULE_EXPORT int debug_get_contrast(Driver *drvthis);

/**
 * \brief Set contrast of virtual display
 * \param drvthis Pointer to driver structure
 * \param promille Contrast value in promille (0-1000)
 *
 * \details Simulates contrast adjustment and outputs debug message
 * with contrast value information.
 */
MODULE_EXPORT void debug_set_contrast(Driver *drvthis, int promille);

/**
 * \brief Get current brightness setting of virtual display
 * \param drvthis Pointer to driver structure
 * \param state Brightness state identifier
 * \return Current brightness value in promille
 *
 * \details Returns the simulated brightness setting for the specified
 * state for testing display control functionality.
 */
MODULE_EXPORT int debug_get_brightness(Driver *drvthis, int state);

/**
 * \brief Set brightness of virtual display
 * \param drvthis Pointer to driver structure
 * \param state Brightness state identifier
 * \param promille Brightness value in promille (0-1000)
 *
 * \details Simulates brightness adjustment and outputs debug message
 * with state and brightness value information.
 */
MODULE_EXPORT void debug_set_brightness(Driver *drvthis, int state, int promille);

/**
 * \brief Control virtual display backlight
 * \param drvthis Pointer to driver structure
 * \param promille Backlight level in promille (0-1000)
 *
 * \details Simulates backlight control and outputs debug message
 * with backlight level information.
 */
MODULE_EXPORT void debug_backlight(Driver *drvthis, int promille);

/**
 * \brief Control output state of virtual display
 * \param drvthis Pointer to driver structure
 * \param value Output control value
 *
 * \details Simulates output control and outputs debug message
 * with output control value information.
 */
MODULE_EXPORT void debug_output(Driver *drvthis, int value);

/**
 * \brief Get driver information string
 * \param drvthis Pointer to driver structure
 * \return Driver information string
 *
 * \details Returns a string containing information about the debug
 * driver version, capabilities, and configuration.
 */
MODULE_EXPORT const char *debug_get_info(Driver *drvthis);

#endif
