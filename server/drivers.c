// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers.c
 * \brief Driver collection management implementation
 * \author Joris Robijn
 * \date 2001
 *
 *
 * \features
 * - Implementation of driver collection management for LCDd server
 * - Driver loading from configuration files with name-based identification
 * - Driver list management using linked lists for dynamic driver collection
 * - Unified operations across all loaded drivers with ForAllDrivers macro
 * - Display property management from output driver with automatic detection
 * - Input handling from any input driver with key aggregation
 * - Hardware control across all drivers (backlight, contrast, macro LEDs)
 * - Driver information aggregation and reporting
 * - Automatic fallback to alternative functions when driver lacks implementation
 * - Debug logging for all driver operations and state changes
 * - Memory management for driver resources and cleanup
 * - Global driver state management with output_driver identification
 *
 * \usage
 * - Used by LCDd server core for managing multiple loaded drivers
 * - Load drivers from configuration via drivers_load_driver()
 * - Perform unified operations via drivers_* wrapper functions
 * - Access display properties via global display_props structure
 * - Retrieve input from any driver via drivers_get_key()
 * - Control hardware features via drivers_backlight(), drivers_set_contrast()
 * - Server shutdown cleanup via drivers_unload_all()
 * - Driver iteration via drivers_getfirst() and drivers_getnext()
 *
 * \details Implementation of driver management for loading multiple drivers
 * and performing operations across all loaded drivers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** \brief Dynamic driver module file extension
 *
 * \details Default shared library extension for driver modules.
 * Can be overridden at compile time for different platforms.
 */
#ifndef MODULE_EXTENSION
#define MODULE_EXTENSION ".so"
#endif

#include "shared/LL.h"
#include "shared/configfile.h"
#include "shared/report.h"

#include "driver.h"
#include "drivers.h"
#include "widget.h"

// Global driver management state: primary output driver, list of all loaded drivers, and shared
// display properties
Driver *output_driver = NULL;
LinkedList *loaded_drivers = NULL;
DisplayProps *display_props = NULL;

/** \brief Iterator macro for looping through all loaded drivers
 * \param drv Driver pointer variable to use in loop
 *
 * \details Convenience macro that expands to a for-loop iterating through
 * the loaded_drivers linked list. Automatically handles list traversal using
 * LL_GetFirst() and LL_GetNext(). Loop variable must be a Driver* pointer.
 */
#define ForAllDrivers(drv)                                                                         \
	for ((drv) = LL_GetFirst(loaded_drivers); (drv); (drv) = LL_GetNext(loaded_drivers))

// Load driver based on configuration settings and add to driver list
int drivers_load_driver(const char *name)
{
	Driver *driver;
	const char *s;

	debug(RPT_DEBUG, "%s(name=\"%.40s\")", __FUNCTION__, name);

	if (!loaded_drivers) {
		loaded_drivers = LL_new();
		if (!loaded_drivers) {
			report(RPT_ERR, "Error allocating driver list.");
			return -1;
		}
	}

	// Get driver path from server config (e.g., "/usr/lib/lcdproc/")
	s = config_get_string("server", "DriverPath", 0, "");
	char driverpath[strlen(s) + 1];
	strncpy(driverpath, s, sizeof(driverpath) - 1);
	driverpath[sizeof(driverpath) - 1] = '\0';

	// Get driver filename from driver section, or use driver name as default
	s = config_get_string(name, "File", 0, name);
	char filename[strlen(driverpath) + strlen(s) + sizeof(MODULE_EXTENSION)];
	snprintf(filename, sizeof(filename), "%s%s%s", driverpath, s,
		 (s == name) ? MODULE_EXTENSION : "");

	driver = driver_load(name, filename);
	if (driver == NULL) {
		report(RPT_INFO, "Module %.40s could not be loaded", filename);
		return -1;
	}

	LL_Push(loaded_drivers, driver);

	// First output driver becomes primary and provides display properties
	if (driver_does_output(driver) && !output_driver) {
		output_driver = driver;

		display_props = malloc(sizeof(DisplayProps));
		display_props->width = driver->width(driver);
		display_props->height = driver->height(driver);

		// Use driver's cell dimensions or fallback to defaults
		if (driver->cellwidth != NULL && driver->cellwidth(driver) > 0)
			display_props->cellwidth = driver->cellwidth(driver);
		else
			display_props->cellwidth = LCD_DEFAULT_CELLWIDTH;

		if (driver->cellheight != NULL && driver->cellheight(driver) > 0)
			display_props->cellheight = driver->cellheight(driver);
		else
			display_props->cellheight = LCD_DEFAULT_CELLHEIGHT;
	}

	// Return 2 if driver needs foreground, 0 on success
	if (driver_stay_in_foreground(driver))
		return 2;

	return 0;
}

// Unload all loaded drivers from memory
void drivers_unload_all(void)
{
	Driver *driver;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	output_driver = NULL;

	while ((driver = LL_Pop(loaded_drivers)) != NULL) {
		driver_unload(driver);
	}
}

// Get information from loaded drivers
const char *drivers_get_info(void)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	ForAllDrivers(drv)
	{
		if (drv->get_info) {
			return drv->get_info(drv);
		}
	}
	return "";
}

// Clear screen on all loaded drivers
void drivers_clear(void)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	ForAllDrivers(drv)
	{
		if (drv->clear)
			drv->clear(drv);
	}
}

// Flush data on all loaded drivers to LCDs
void drivers_flush(void)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	ForAllDrivers(drv)
	{
		if (drv->flush)
			drv->flush(drv);
	}
}

// Write string to all loaded drivers
void drivers_string(int x, int y, const char *string)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, string=\"%.40s\")", __FUNCTION__, x, y, string);

	ForAllDrivers(drv)
	{
		if (drv->string)
			drv->string(drv, x, y, string);
	}
}

// Write a character to all loaded drivers
void drivers_chr(int x, int y, char c)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, c='%c')", __FUNCTION__, x, y, c);

	ForAllDrivers(drv)
	{
		if (drv->chr)
			drv->chr(drv, x, y, c);
	}
}

// Draw a vertical bar to all drivers
void drivers_vbar(int x, int y, int len, int promille, int pattern)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, len=%d, promille=%d, pattern=%d)", __FUNCTION__, x, y, len,
	      promille, pattern);

	ForAllDrivers(drv)
	{
		if (drv->vbar)
			drv->vbar(drv, x, y, len, promille, pattern);
		else
			driver_alt_vbar(drv, x, y, len, promille, pattern);
	}
}

// Draw a horizontal bar to all drivers
void drivers_hbar(int x, int y, int len, int promille, int pattern)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, len=%d, promille=%d, pattern=%d)", __FUNCTION__, x, y, len,
	      promille, pattern);

	ForAllDrivers(drv)
	{
		if (drv->hbar)
			drv->hbar(drv, x, y, len, promille, pattern);
		else
			driver_alt_hbar(drv, x, y, len, promille, pattern);
	}
}

// Draw a percentage-bar to all drivers
void drivers_pbar(int x, int y, int width, int promille, char *begin_label, char *end_label)
{
	Driver *drv;

	ForAllDrivers(drv) driver_pbar(drv, x, y, width, promille, begin_label, end_label);
}

// Write a big number to all output drivers
void drivers_num(int x, int num)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, num=%d)", __FUNCTION__, x, num);

	ForAllDrivers(drv)
	{
		if (drv->num)
			drv->num(drv, x, num);
		else
			driver_alt_num(drv, x, num);
	}
}

// Perform heartbeat on all drivers
void drivers_heartbeat(int state)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(state=%d)", __FUNCTION__, state);

	ForAllDrivers(drv)
	{
		if (drv->heartbeat)
			drv->heartbeat(drv, state);
		else
			driver_alt_heartbeat(drv, state);
	}
}

// Write icon to all drivers
void drivers_icon(int x, int y, int icon)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, icon=ICON_%s)", __FUNCTION__, x, y,
	      widget_icon_to_iconname(icon));

	ForAllDrivers(drv)
	{
		if (drv->icon) {
			if (drv->icon(drv, x, y, icon) == -1) {
				driver_alt_icon(drv, x, y, icon);
			}
		} else {
			driver_alt_icon(drv, x, y, icon);
		}
	}
}

// Set cursor on all loaded drivers
void drivers_cursor(int x, int y, int state)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(x=%d, y=%d, state=%d)", __FUNCTION__, x, y, state);

	ForAllDrivers(drv)
	{
		if (drv->cursor)
			drv->cursor(drv, x, y, state);
		else
			driver_alt_cursor(drv, x, y, state);
	}
}

// Set backlight on all drivers
void drivers_backlight(int state)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(state=%d)", __FUNCTION__, state);

	ForAllDrivers(drv)
	{
		if (drv->backlight)
			drv->backlight(drv, state);
	}
}

// Set macro LED status on all drivers
int drivers_set_macro_leds(int m1, int m2, int m3, int mr)
{
	Driver *drv;
	int result = -1;

	debug(RPT_DEBUG, "%s(m1=%d, m2=%d, m3=%d, mr=%d)", __FUNCTION__, m1, m2, m3, mr);

	ForAllDrivers(drv)
	{
		if (drv->set_macro_leds) {
			int drv_result = drv->set_macro_leds(drv, m1, m2, m3, mr);
			if (drv_result == 0) {
				result = 0;
			}
		}
	}

	return result;
}

// Set output on all drivers
void drivers_output(int state)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(state=%d)", __FUNCTION__, state);

	ForAllDrivers(drv)
	{
		if (drv->output)
			drv->output(drv, state);
	}
}

// Get key presses from loaded drivers
const char *drivers_get_key(void)
{
	Driver *drv;
	const char *keystroke;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	ForAllDrivers(drv)
	{
		if (drv->get_key) {
			keystroke = drv->get_key(drv);
			if (keystroke != NULL) {
				report(RPT_INFO, "Driver [%.40s] generated keystroke %.40s",
				       drv->name, keystroke);
				return keystroke;
			}
		}
	}
	return NULL;
}

// Set custom character definition on all drivers
void drivers_set_char(char ch, unsigned char *dat)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(ch='%c', dat=%p)", __FUNCTION__, ch, dat);

	ForAllDrivers(drv)
	{
		if (drv->set_char)
			drv->set_char(drv, ch, dat);
	}
}

// Get contrast from output driver
int drivers_get_contrast(void)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	ForAllDrivers(drv)
	{
		if (drv->get_contrast) {
			int contrast = drv->get_contrast(drv);
			debug(RPT_DEBUG, "%s: Driver [%.40s] returned contrast %d", __FUNCTION__,
			      drv->name, contrast);
			return contrast;
		}
	}
	return -1;
}

// Set contrast on all drivers
void drivers_set_contrast(int contrast)
{
	Driver *drv;

	debug(RPT_DEBUG, "%s(contrast=%d)", __FUNCTION__, contrast);

	ForAllDrivers(drv)
	{
		if (drv->set_contrast)
			drv->set_contrast(drv, contrast);
	}
}
