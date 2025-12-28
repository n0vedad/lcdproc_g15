// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/driver.h
 * \brief Driver management interface
 * \author Joris Robijn
 * \date 2001
 *
 *
 * \features
 * - Header file for driver management interface in LCDd server
 * - Dynamic driver loading and unloading with shared library support
 * - Driver symbol binding and validation for function pointer assignment
 * - Driver capability detection for output/input support determination
 * - Fallback implementations for optional driver functions
 * - Alternative drawing functions (bars, icons, cursor) when driver lacks implementation
 * - Driver module binding and unbinding operations
 * - Boolean capability queries for driver feature detection
 * - Alternative function implementations for progressive bars, vertical/horizontal bars
 * - Alternative number display, heartbeat, icon, and cursor functions
 * - Integration with lcd.h driver API definitions
 * - Configuration-based conditional compilation support
 *
 * \usage
 * - Used by LCDd server core for managing LCD driver modules
 * - Load drivers at server startup via driver_load() with module name and filename
 * - Bind driver symbols and validate requirements via driver_bind_module()
 * - Query driver capabilities via driver_does_output() and driver_does_input()
 * - Use fallback functions when driver doesn't provide optional implementations
 * - Alternative drawing via driver_alt_* functions for missing driver functionality
 * - Driver cleanup via driver_unbind_module() and driver_unload()
 * - Multiple instance support detection via driver_support_multiple()
 * - Foreground requirement detection via driver_stay_in_foreground()
 *
 * \details Interface for driver loading, binding, and management operations
 * providing functions for dynamic driver loading and fallback implementations.
 */

#ifndef DRIVER_H
#define DRIVER_H

#include "drivers/lcd.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif
#include "shared/defines.h"

/**
 * \brief Load a driver from a shared library file
 * \param name Driver name for identification and error reporting
 * \param filename Path to shared library (.so) file containing driver
 * \return Pointer to loaded Driver structure on success, NULL on error
 * \details Dynamically loads a driver module from the specified shared library
 * file and creates a Driver structure. The driver name is used for identification
 * and configuration lookup. Returns NULL if the library cannot be loaded or
 * required symbols are missing.
 */
Driver *driver_load(const char *name, const char *filename);

/**
 * \brief Unload a driver and free all associated resources
 * \param driver Pointer to Driver structure to unload
 * \return 0 on success, -1 on error
 * \details Properly unloads the driver module, closes the shared library
 * handle, and frees all allocated memory associated with the driver.
 * Should be called during server shutdown or driver replacement.
 */
int driver_unload(Driver *driver);

/**
 * \brief Bind driver module symbols to function pointers
 * \param driver Pointer to Driver structure with loaded module
 * \return 0 on success, -1 on error (missing required symbols)
 * \details Resolves and binds all required and optional driver function
 * symbols from the loaded shared library to the driver's function pointers.
 * Validates that all mandatory functions are present and sets up fallback
 * implementations for optional functions that are missing.
 */
int driver_bind_module(Driver *driver);

/**
 * \brief Unbind driver module and release binding resources
 * \param driver Pointer to Driver structure to unbind
 * \return 0 on success, -1 on error
 * \details Clears all function pointer bindings and releases any resources
 * associated with symbol binding. Prepares the driver for safe unloading
 * or rebinding operations.
 */
int driver_unbind_module(Driver *driver);

/**
 * \brief Check if driver supports output operations to display
 * \param driver Pointer to Driver structure to check
 * \return true if driver can output to display, false otherwise
 * \details Determines whether the driver provides output functionality
 * such as text display, graphics, or other visual output capabilities.
 * Used to categorize drivers and determine server functionality.
 */
bool driver_does_output(Driver *driver);

/**
 * \brief Check if driver supports input operations from hardware
 * \param driver Pointer to Driver structure to check
 * \return true if driver can receive input, false otherwise
 * \details Determines whether the driver provides input functionality
 * such as key presses, button input, or other user interaction capabilities.
 * Used for input routing and event handling setup.
 */
bool driver_does_input(Driver *driver);

/**
 * \brief Check if driver supports multiple simultaneous instances
 * \param driver Pointer to Driver structure to check
 * \return true if driver supports multiple instances, false otherwise
 * \details Determines whether multiple instances of the same driver can
 * be loaded and operated simultaneously. Important for multi-display
 * configurations and resource management.
 */
bool driver_support_multiple(Driver *driver);

/**
 * \brief Check if driver requires the server to stay in foreground
 * \param driver Pointer to Driver structure to check
 * \return true if driver requires foreground operation, false otherwise
 * \details Determines whether the driver needs the server process to remain
 * in the foreground rather than daemonizing. Some drivers require this for
 * proper hardware access or signal handling.
 */
bool driver_stay_in_foreground(Driver *driver);

/**
 * \brief Draw a progress bar with optional start and end labels
 * \param drv Pointer to Driver structure
 * \param x Horizontal position of progress bar start
 * \param y Vertical position of progress bar
 * \param width Total width of progress bar in characters
 * \param promille Fill level in promille (0-1000, where 1000 = 100%)
 * \param begin_label Optional label text at start of bar (can be NULL)
 * \param end_label Optional label text at end of bar (can be NULL)
 * \details Draws a progress bar with configurable labels at both ends.
 * The bar shows completion percentage based on promille value. Labels
 * are positioned at the start and end of the bar area.
 */
void driver_pbar(Driver *drv, int x, int y, int width, int promille, char *begin_label,
		 char *end_label);

/**
 * \brief Alternative vertical bar implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param x Horizontal position of vertical bar
 * \param y Vertical position (bottom of bar)
 * \param len Length/height of bar in character rows
 * \param promille Fill level in promille (0-1000, where 1000 = 100%)
 * \param pattern Bar rendering pattern/options (BAR_SEAMLESS, etc.)
 * \details Provides fallback vertical bar implementation using basic character
 * output when the driver doesn't have native vertical bar support. Creates
 * a vertical progress indicator using standard characters and patterns.
 */
void driver_alt_vbar(Driver *drv, int x, int y, int len, int promille, int pattern);

/**
 * \brief Alternative horizontal bar implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param x Horizontal position (start of bar)
 * \param y Vertical position of horizontal bar
 * \param len Length/width of bar in character columns
 * \param promille Fill level in promille (0-1000, where 1000 = 100%)
 * \param pattern Bar rendering pattern/options (BAR_SEAMLESS, etc.)
 * \details Provides fallback horizontal bar implementation using basic character
 * output when the driver doesn't have native horizontal bar support. Creates
 * a horizontal progress indicator using standard characters and patterns.
 */
void driver_alt_hbar(Driver *drv, int x, int y, int len, int promille, int pattern);

/**
 * \brief Alternative big number display implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param x Horizontal position for number display
 * \param num Number to display (typically 0-9)
 * \details Provides fallback big number implementation using standard characters
 * when the driver doesn't have native big number support. Creates large digit
 * displays using multiple character positions and basic ASCII characters.
 */
void driver_alt_num(Driver *drv, int x, int num);

/**
 * \brief Alternative heartbeat indicator implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param state Heartbeat state (HEARTBEAT_ON, HEARTBEAT_OFF, etc.)
 * \details Provides fallback heartbeat implementation using standard character
 * output when the driver doesn't have native heartbeat indicator support.
 * Creates a visual heartbeat indicator using basic characters or icons.
 */
void driver_alt_heartbeat(Driver *drv, int state);

/**
 * \brief Alternative icon display implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param x Horizontal position of icon
 * \param y Vertical position of icon
 * \param icon Icon identifier (ICON_BLOCK_FILLED, ICON_HEART, etc.)
 * \details Provides fallback icon implementation using standard characters
 * when the driver doesn't have native icon support. Maps icon identifiers
 * to appropriate ASCII characters or character combinations.
 */
void driver_alt_icon(Driver *drv, int x, int y, int icon);

/**
 * \brief Alternative cursor implementation for drivers without native support
 * \param drv Pointer to Driver structure
 * \param x Horizontal position of cursor
 * \param y Vertical position of cursor
 * \param state Cursor state/type (CURSOR_OFF, CURSOR_DEFAULT_ON, etc.)
 * \details Provides fallback cursor implementation using character manipulation
 * when the driver doesn't have native cursor support. Creates cursor effects
 * using character inversion, underlining, or other text-based methods.
 */
void driver_alt_cursor(Driver *drv, int x, int y, int state);

#endif
