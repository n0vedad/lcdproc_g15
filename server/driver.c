// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/driver.c
 * \brief Driver management implementation
 * \author Joris Robijn
 * \date 2001
 *
 *
 * \features
 * - Implementation of driver management for LCDd server core
 * - Dynamic driver loading and unloading with dlopen/dlsym shared library support
 * - Driver symbol binding and validation with symbol table driven process
 * - Driver capability detection for input/output support determination
 * - Fallback implementations for optional driver functions (bars, icons, numbers)
 * - Alternative drawing functions when driver lacks specific implementations
 * - Error handling and cleanup with proper resource management
 * - Configuration file access integration for driver-specific settings
 * - Display property access functions for driver adaptation
 * - Version checking and API compatibility validation
 * - Driver structure memory management and initialization
 * - Symbol offset mapping for automated driver function binding
 * - Debug logging throughout all driver operations
 *
 * \usage
 * - Used by LCDd server core for managing LCD driver module lifecycle
 * - Load drivers at server startup via driver_load() with module identification
 * - Bind driver symbols and validate API requirements via driver_bind_module()
 * - Query driver capabilities via capability detection functions
 * - Provide fallback functionality via driver_alt_* functions for missing features
 * - Alternative drawing implementations for bars, icons, cursors, and numbers
 * - Driver cleanup and resource release via driver_unbind_module() and driver_unload()
 * - Configuration access for driver-specific parameter retrieval
 * - Display property queries for driver coordination and adaptation
 *
 * \details Implementation of driver loading, binding, and management operations
 * handling dynamic loading of driver modules and fallback implementations.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "shared/posix_wrappers.h"
#endif

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "driver.h"
#include "drivers.h"
#include "drivers/lcd.h"
#include "main.h"
#include "shared/posix_wrappers.h"
#include "widget.h"

#include "shared/configfile.h"
#include "shared/posix_wrappers.h"
#include "shared/report.h"

/**
 * \brief Driver symbol definition structure
 * \details Maps driver property/method names to struct offsets for dynamic loading
 */
typedef struct driver_symbols {
	const char *name; ///< Symbol name
	short offset;	  ///< Offset in Driver struct
	short required;	  ///< 1 if required, 0 if optional
} DriverSymbols;

/** \brief Driver symbol lookup table for dynamic driver loading
 *
 * \details Maps driver API symbol names to their offsets in the Driver struct.
 * Required flag indicates mandatory symbols (1) vs optional (0). Used during
 * driver module loading to resolve and validate driver entry points. Table is
 * null-terminated.
 */
DriverSymbols driver_symbols[] = {{"api_version", offsetof(Driver, api_version), 1},
				  {"stay_in_foreground", offsetof(Driver, stay_in_foreground), 1},
				  {"supports_multiple", offsetof(Driver, supports_multiple), 1},
				  {"symbol_prefix", offsetof(Driver, symbol_prefix), 1},
				  {"init", offsetof(Driver, init), 1},
				  {"close", offsetof(Driver, close), 1},
				  {"width", offsetof(Driver, width), 0},
				  {"height", offsetof(Driver, height), 0},
				  {"clear", offsetof(Driver, clear), 0},
				  {"flush", offsetof(Driver, flush), 0},
				  {"string", offsetof(Driver, string), 0},
				  {"chr", offsetof(Driver, chr), 0},
				  {"vbar", offsetof(Driver, vbar), 0},
				  {"hbar", offsetof(Driver, hbar), 0},
				  {"pbar", offsetof(Driver, pbar), 0},
				  {"num", offsetof(Driver, num), 0},
				  {"heartbeat", offsetof(Driver, heartbeat), 0},
				  {"icon", offsetof(Driver, icon), 0},
				  {"cursor", offsetof(Driver, cursor), 0},
				  {"set_char", offsetof(Driver, set_char), 0},
				  {"get_free_chars", offsetof(Driver, get_free_chars), 0},
				  {"cellwidth", offsetof(Driver, cellwidth), 0},
				  {"cellheight", offsetof(Driver, cellheight), 0},
				  {"get_contrast", offsetof(Driver, get_contrast), 0},
				  {"set_contrast", offsetof(Driver, set_contrast), 0},
				  {"get_brightness", offsetof(Driver, get_brightness), 0},
				  {"set_brightness", offsetof(Driver, set_brightness), 0},
				  {"backlight", offsetof(Driver, backlight), 0},
				  {"output", offsetof(Driver, output), 0},
				  {"set_macro_leds", offsetof(Driver, set_macro_leds), 0},
				  {"get_key", offsetof(Driver, get_key), 0},
				  {"get_info", offsetof(Driver, get_info), 0},
				  {NULL, 0, 0}};

// Internal helper function declarations: display dimension requests and private data storage for
// driver instances
static int request_display_width(void);
static int request_display_height(void);
static int driver_store_private_ptr(Driver *driver, void *private_data);

// Load driver from shared library file
Driver *driver_load(const char *name, const char *filename)
{
	Driver *driver = NULL;
	int res;

	report(RPT_DEBUG, "%s(name=\"%.40s\", filename=\"%.80s\")", __FUNCTION__, name, filename);

	if ((name == NULL) || (filename == NULL))
		return NULL;

	driver = calloc(1, sizeof(Driver));
	if (driver == NULL) {
		report(RPT_ERR, "%s: error allocating driver", __FUNCTION__);
		return NULL;
	}

	driver->name = malloc(strlen(name) + 1);
	if (driver->name == NULL) {
		report(RPT_ERR, "%s: error allocating driver name", __FUNCTION__);
		free(driver);
		return NULL;
	}
	strncpy(driver->name, name, strlen(name));
	driver->name[strlen(name)] = '\0';

	driver->filename = malloc(strlen(filename) + 1);
	if (driver->filename == NULL) {
		report(RPT_ERR, "%s: error allocating driver filename", __FUNCTION__);
		free(driver->name);
		free(driver);
		return NULL;
	}
	strncpy(driver->filename, filename, strlen(filename));
	driver->filename[strlen(filename)] = '\0';

	if (driver_bind_module(driver) < 0) {
		report(RPT_ERR, "Driver [%.40s] binding failed", name);
		free(driver->name);
		free(driver->filename);
		free(driver);
		return NULL;
	}

	if (driver->api_version == NULL || strcmp(*(driver->api_version), API_VERSION) != 0) {
		report(RPT_ERR, "Driver [%.40s] is of an incompatible version", name);
		driver_unbind_module(driver);
		free(driver->name);
		free(driver->filename);
		free(driver);
		return NULL;
	}

	debug(RPT_DEBUG, "%s: Calling driver [%.40s] init function", __FUNCTION__, driver->name);

	res = driver->init(driver);
	if (res < 0) {
		report(RPT_ERR, "Driver [%.40s] init failed, return code %d", driver->name, res);
		driver_unbind_module(driver);
		free(driver->name);
		free(driver->filename);
		free(driver);
		return NULL;
	}

	debug(RPT_NOTICE, "Driver [%.40s] loaded", driver->name);

	return driver;
}

// Unload driver from memory and free all resources
int driver_unload(Driver *driver)
{
	debug(RPT_NOTICE, "Closing driver [%.40s]", driver->name);

	if (driver->close != NULL)
		driver->close(driver);

	driver_unbind_module(driver);

	free(driver->filename);
	driver->filename = NULL;
	free(driver->name);
	driver->name = NULL;
	free(driver);
	driver = NULL;

	debug(RPT_DEBUG, "%s: Driver unloaded", __FUNCTION__);

	return 0;
}

// Dynamically load module and bind all driver symbols
int driver_bind_module(Driver *driver)
{
	int i;
	int missing_symbols = 0;

	debug(RPT_DEBUG, "%s(driver=[%.40s])", __FUNCTION__, driver->name);

	driver->module_handle = dlopen(driver->filename, RTLD_NOW);
	if (driver->module_handle == NULL) {
		// dlerror() is thread-safe on Linux (glibc uses thread-local storage)
		const char *err_msg = safe_dlerror();
		report(RPT_ERR, "Could not open driver module %.40s: %s", driver->filename,
		       err_msg);
		return -1;
	}

	for (i = 0; driver_symbols[i].name != NULL; i++) {
		void (**p)();

		// Calculate address of function pointer in Driver struct using offset
		p = (void (**)())((char *)driver + (driver_symbols[i].offset));
		*p = NULL;

		// Try prefixed symbol first (e.g., "g15_init"), then unprefixed ("init")
		if (driver->symbol_prefix != NULL) {
			char *s = malloc(strlen(*(driver->symbol_prefix)) +
					 strlen(driver_symbols[i].name) + 1);
			snprintf(s,
				 strlen(*(driver->symbol_prefix)) + strlen(driver_symbols[i].name) +
				     1,
				 "%s%s", *(driver->symbol_prefix), driver_symbols[i].name);
			debug(RPT_DEBUG, "%s: finding symbol: %s", __FUNCTION__, s);
			*p = dlsym(driver->module_handle, s);
			free(s);
		}

		if (*p == NULL) {
			debug(RPT_DEBUG, "%s: finding symbol: %s", __FUNCTION__,
			      driver_symbols[i].name);
			*p = dlsym(driver->module_handle, driver_symbols[i].name);
		}

		if (*p != NULL) {
			debug(RPT_DEBUG, "%s: found symbol at: %p", __FUNCTION__, *p);
		} else {
			if (driver_symbols[i].required) {
				report(RPT_ERR, "Driver [%.40s] does not have required symbol: %s",
				       driver->name, driver_symbols[i].name);
				missing_symbols++;
			}
		}
	}

	if (missing_symbols > 0) {
		report(RPT_ERR, "Driver [%.40s]  misses %d required symbols", driver->name,
		       missing_symbols);
		dlclose(driver->module_handle);
		return -1;
	}

	driver->config_get_bool = config_get_bool;
	driver->config_get_int = config_get_int;
	driver->config_get_float = config_get_float;
	driver->config_get_string = config_get_string;
	driver->config_has_section = config_has_section;
	driver->config_has_key = config_has_key;

	driver->store_private_ptr = driver_store_private_ptr;

	driver->request_display_width = request_display_width;
	driver->request_display_height = request_display_height;

	return 0;
}

// Unload driver module and close shared library handle
int driver_unbind_module(Driver *driver)
{
	debug(RPT_DEBUG, "%s(driver=[%.40s])", __FUNCTION__, driver->name);

	dlclose(driver->module_handle);

	return 0;
}

// Determine if driver supports output operations
bool driver_does_output(Driver *driver)
{
	return (driver->width != NULL || driver->height != NULL || driver->clear != NULL ||
		driver->string != NULL || driver->chr != NULL)
		   ? 1
		   : 0;
}

// Determine if driver supports input operations
bool driver_does_input(Driver *driver) { return (driver->get_key != NULL) ? 1 : 0; }

// Check if driver requires server to stay in foreground
bool driver_stay_in_foreground(Driver *driver) { return *driver->stay_in_foreground; }

/**
 * \brief Driver supports multiple
 * \param driver Driver *driver
 * \return Return value
 */
bool driver_supports_multiple(Driver *driver) { return *driver->supports_multiple; }

/**
 * \brief Driver store private ptr
 * \param driver Driver *driver
 * \param private_data void *private_data
 * \return Return value
 */
static int driver_store_private_ptr(Driver *driver, void *private_data)
{
	debug(RPT_DEBUG, "%s(driver=[%.40s], ptr=%p)", __FUNCTION__, driver->name, private_data);

	driver->private_data = private_data;
	return 0;
}

/**
 * \brief Request display width
 * \return Return value
 */
static int request_display_width(void)
{
	if (!display_props)
		return 0;
	return display_props->width;
}

/**
 * \brief Request display height
 * \return Return value
 */
static int request_display_height(void)
{
	if (!display_props)
		return 0;
	return display_props->height;
}

/**
 * \brief Fallback bar drawing for drivers without native bar support
 * \param drv Driver instance
 * \param x Starting X coordinate
 * \param y Starting Y coordinate
 * \param len Bar length in characters
 * \param promille Fill level in promille (0-1000)
 * \param character Character to use for filled portion
 * \param dx X direction increment (+1 right, -1 left, 0 vertical)
 * \param dy Y direction increment (+1 down, -1 up, 0 horizontal)
 *
 * \details Uses chr() function to emulate bar drawing character-by-character.
 * Provides fallback when driver lacks native vbar/hbar/pbar support.
 */
static void driver_alt_bar_internal(Driver *drv, int x, int y, int len, int promille,
				    char character, int dx, int dy)
{
	int pos;

	if (drv->chr == NULL)
		return;

	for (pos = 0; pos < len; pos++) {
		if (2 * pos < ((long)promille * len / 500 + 1)) {
			drv->chr(drv, x + pos * dx, y + pos * dy, character);
		}
	}
}

// Alternative vertical bar implementation for drivers without native support
void driver_alt_vbar(Driver *drv, int x, int y, int len, int promille, int options)
{
	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, y=%d, len=%d, promille=%d, options=%d)",
	      __FUNCTION__, drv->name, x, y, len, promille, options);

	driver_alt_bar_internal(drv, x, y, len, promille, '|', 0, -1);
}

// Alternative horizontal bar implementation for drivers without native support
void driver_alt_hbar(Driver *drv, int x, int y, int len, int promille, int options)
{
	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, y=%d, len=%d, promille=%d, options=%d)",
	      __FUNCTION__, drv->name, x, y, len, promille, options);

	driver_alt_bar_internal(drv, x, y, len, promille, '-', 1, 0);
}

// Draw percentage bar with optional begin and end labels
void driver_pbar(Driver *drv, int x, int y, int width, int promille, char *begin_label,
		 char *end_label)
{
	int begin_length, end_length, len;

	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, y=%d, width=%d, promille=%d)", __FUNCTION__,
	      drv->name, x, y, width, promille);

	if (drv->chr == NULL || drv->string == NULL)
		return;

	if (!drv->pbar && begin_label == NULL && end_label == NULL) {
		begin_label = "[";
		end_label = "]";
	}

	begin_length = begin_label ? strlen(begin_label) : 0;
	end_length = end_label ? strlen(end_label) : 0;

	if ((begin_length + end_length + 2) > width)
		begin_length = end_length = 0;

	len = width - begin_length - end_length;

	if (begin_length) {
		drv->string(drv, x, y, begin_label);
		x += begin_length;
	}

	if (drv->pbar)
		drv->pbar(drv, x, y, len, promille);
	else if (drv->hbar)
		drv->hbar(drv, x, y, len, promille, BAR_PATTERN_FILLED);
	else
		driver_alt_hbar(drv, x, y, len, promille, BAR_PATTERN_FILLED);
	x += len;

	if (end_length)
		drv->string(drv, x, y, end_label);
}

// Alternative big number display implementation for drivers without native support
void driver_alt_num(Driver *drv, int x, int num)
{
	static char num_map[][4][4] = {{" _ ", "| |", "|_|", "   "}, {"   ", "  |", "  |", "   "},
				       {" _ ", " _|", "|_ ", "   "}, {" _ ", " _|", " _|", "   "},
				       {"   ", "|_|", "  |", "   "}, {" _ ", "|_ ", " _|", "   "},
				       {" _ ", "|_ ", "|_|", "   "}, {" _ ", "  |", "  |", "   "},
				       {" _ ", "|_|", "|_|", "   "}, {" _ ", "|_|", " _|", "   "},
				       {" ", ".", ".", " "}};

	int y, dx;

	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, num=%d)", __FUNCTION__, drv->name, x, num);

	if ((num < 0) || (num > 10))
		return;
	if (drv->chr == NULL)
		return;

	for (y = 0; y < 4; y++)
		for (dx = 0; num_map[num][y][dx] != '\0'; dx++)
			drv->chr(drv, x + dx, y + 1, num_map[num][y][dx]);
}

// Alternative heartbeat indicator implementation for drivers without native support
void driver_alt_heartbeat(Driver *drv, int state)
{
	int icon;

	debug(RPT_DEBUG, "%s(drv=[%.40s], state=%d)", __FUNCTION__, drv->name, state);

	if (state == HEARTBEAT_OFF)
		return;

	if (drv->width == NULL)
		return;

	/**
	 * \todo Consider migrating to clock_gettime(CLOCK_MONOTONIC) for more precise
	 * and system-independent timing instead of the global timer variable.
	 * Current method works but could be improved with monotonic time for
	 * heartbeat timing that's independent of main loop performance.
	 * Alternative: Improve current pattern from (timer & 5) to (timer & 4)
	 * for more regular 50/50 blink pattern.
	 *
	 * \ingroup ToDo_critical
	 */

	icon = (timer & 5) ? ICON_HEART_FILLED : ICON_HEART_OPEN;

	if (drv->icon)
		drv->icon(drv, drv->width(drv), 1, icon);
	else
		driver_alt_icon(drv, drv->width(drv), 1, icon);
}

// Alternative icon display implementation for drivers without native support
void driver_alt_icon(Driver *drv, int x, int y, int icon)
{
	char ch1 = '?';
	char ch2 = '\0';

	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, y=%d, icon=ICON_%s)", __FUNCTION__, drv->name, x, y,
	      widget_icon_to_iconname(icon));

	if (drv->chr == NULL)
		return;

	switch (icon) {

	// Filled block icon
	case ICON_BLOCK_FILLED:
		ch1 = '#';
		break;

	// Open heart icon
	case ICON_HEART_OPEN:
		ch1 = '-';
		break;

	// Filled heart icon
	case ICON_HEART_FILLED:
		ch1 = '#';
		break;

	// Up arrow icon
	case ICON_ARROW_UP:
		ch1 = '^';
		break;

	// Down arrow icon
	case ICON_ARROW_DOWN:
		ch1 = 'v';
		break;

	// Left arrow icon
	case ICON_ARROW_LEFT:
		ch1 = '<';
		break;

	// Right arrow icon
	case ICON_ARROW_RIGHT:
		ch1 = '>';
		break;

	// Unchecked checkbox icon
	case ICON_CHECKBOX_OFF:
		ch1 = 'N';
		break;

	// Checked checkbox icon
	case ICON_CHECKBOX_ON:
		ch1 = 'Y';
		break;

	default:
		break;

	// Gray checkbox icon
	case ICON_CHECKBOX_GRAY:
		ch1 = 'o';
		break;

	// Left selector icon
	case ICON_SELECTOR_AT_LEFT:
		ch1 = '>';
		break;

	// Right selector icon
	case ICON_SELECTOR_AT_RIGHT:
		ch1 = '<';
		break;

	// Ellipsis icon
	case ICON_ELLIPSIS:
		ch1 = '_';
		break;

	// Stop icon (two characters)
	case ICON_STOP:
		ch1 = '[';
		ch2 = ']';
		break;

	// Pause icon (two characters)
	case ICON_PAUSE:
		ch1 = '|';
		ch2 = '|';
		break;

	// Play icon (two characters)
	case ICON_PLAY:
		ch1 = '>';
		ch2 = ' ';
		break;

	// Play reverse icon (two characters)
	case ICON_PLAYR:
		ch1 = '<';
		ch2 = ' ';
		break;

	// Fast forward icon (two characters)
	case ICON_FF:
		ch1 = '>';
		ch2 = '>';
		break;

	// Fast reverse icon (two characters)
	case ICON_FR:
		ch1 = '<';
		ch2 = '<';
		break;

	// Next track icon (two characters)
	case ICON_NEXT:
		ch1 = '>';
		ch2 = '|';
		break;

	// Previous track icon (two characters)
	case ICON_PREV:
		ch1 = '|';
		ch2 = '<';
		break;

	// Record icon (two characters)
	case ICON_REC:
		ch1 = '(';
		ch2 = ')';
		break;
	}

	drv->chr(drv, x, y, ch1);
	if (ch2 != '\0')
		drv->chr(drv, x + 1, y, ch2);
}

// Alternative cursor implementation for drivers without native support
void driver_alt_cursor(Driver *drv, int x, int y, int state)
{
	/**
	 * \todo Same question about timer timing applies here: Consider migrating
	 * to clock_gettime(CLOCK_MONOTONIC) for system-independent cursor blink timing.
	 * Current method (timer & 2) provides good 50/50 blink pattern but depends on
	 * main loop performance. Monotonic time would ensure consistent cursor blink
	 * frequency even under system load or I/O blocking.
	 *
	 * \ingroup ToDo_critical
	 */

	debug(RPT_DEBUG, "%s(drv=[%.40s], x=%d, y=%d, state=%d)", __FUNCTION__, drv->name, x, y,
	      state);

	switch (state) {

	// Blinking block cursor or default cursor
	case CURSOR_BLOCK:
	case CURSOR_DEFAULT_ON:
		if ((timer & 2) && (drv->chr != NULL)) {
			if (drv->icon != NULL) {
				drv->icon(drv, x, y, ICON_BLOCK_FILLED);
			} else {
				driver_alt_icon(drv, x, y, ICON_BLOCK_FILLED);
			}
		}
		break;

	// Blinking underline cursor
	case CURSOR_UNDER:
		if ((timer & 2) && (drv->chr != NULL)) {
			drv->chr(drv, x, y, '_');
		}
		break;

	// Handle invalid or off cursor states
	default:
		break;
	}
}
