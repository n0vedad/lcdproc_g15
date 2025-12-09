// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/debug.c
 * \brief Debug driver implementation for LCDd testing and debugging
 * \author Peter Marschall
 * \author Markus Dolze
 * \date 2008-2010
 *
 *
 * \features
 * - Debug driver implementation providing virtual LCD display for testing and debugging
 * - Virtual display simulation with frame buffer for state tracking
 * - Debug output for all LCD operations instead of controlling actual hardware
 * - Standard LCD driver interface compliance and compatibility
 * - Configurable display dimensions through configuration file
 * - Brightness and contrast simulation with debug output
 * - Core driver functions: initialization, cleanup, display control
 * - Graphics functions: bars, numbers, icons, cursors, custom characters
 * - Key input simulation and event handling
 * - Memory management for virtual display buffer
 * - Configuration parsing for display parameters
 * - Driver information reporting and status
 *
 * \usage
 * - Used by LCDd server when "debug" driver is specified in configuration
 * - Used for testing LCD applications without physical hardware
 * - Used for debugging driver implementations and development
 * - Used for development and validation of LCD functionality
 * - Used for learning LCD driver interface and behavior
 * - Driver automatically loads when selected in LCDd.conf
 * - All operations produce debug messages for inspection and analysis
 *
 * \details Debug driver that provides a virtual LCD display for testing
 * and debugging purposes. This driver outputs all operations as debug
 * messages instead of controlling actual hardware, making it useful for
 * development and validation.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "lcd.h"

#include "shared/report.h"

/** \name Debug Driver Default Configuration
 * Default display parameters for the debug driver
 */
///@{
#define DEFAULT_WIDTH LCD_DEFAULT_WIDTH		  ///< Default display width in characters
#define DEFAULT_HEIGHT LCD_DEFAULT_HEIGHT	  ///< Default display height in characters
#define DEFAULT_CELLWIDTH LCD_DEFAULT_CELLWIDTH	  ///< Default cell width in pixels
#define DEFAULT_CELLHEIGHT LCD_DEFAULT_CELLHEIGHT ///< Default cell height in pixels
#define DEFAULT_CONTRAST 500			  ///< Default contrast (0-1000 scale)
#define DEFAULT_BRIGHTNESS 750			  ///< Default brightness when on (0-1000 scale)
#define DEFAULT_OFFBRIGHTNESS 250		  ///< Default brightness when off (0-1000 scale)
///@}

/**
 * \brief Debug driver private data structure
 * \details Stores internal state for the debug LCD driver
 */
typedef struct debug_private_data {
	char *framebuf;	   ///< Frame buffer for LCD content
	int width;	   ///< Display width in characters
	int height;	   ///< Display height in characters
	int cellwidth;	   ///< Cell width in pixels
	int cellheight;	   ///< Cell height in pixels
	int contrast;	   ///< Contrast setting
	int brightness;	   ///< Brightness when display is on
	int offbrightness; ///< Brightness when display is off
} PrivateData;

/** \name Debug Driver Module Exports
 * Driver metadata exported to the LCDd server core
 */
///@{
MODULE_EXPORT char *api_version = API_VERSION; ///< Driver API version string
MODULE_EXPORT int stay_in_foreground = 1;      ///< Debug driver requires foreground mode
MODULE_EXPORT int supports_multiple = 0;       ///< Debug driver does not support multiple instances
MODULE_EXPORT char *symbol_prefix = "debug_";  ///< Function symbol prefix for this driver
///@}

// Initialize the debug driver
MODULE_EXPORT int debug_init(Driver *drvthis)
{
	PrivateData *p;

	report(RPT_INFO, "%s()", __FUNCTION__);

	p = (PrivateData *)calloc(1, sizeof(PrivateData));
	if (p == NULL)
		return -1;
	if (drvthis->store_private_ptr(drvthis, p))
		return -1;

	p->width = DEFAULT_WIDTH;
	p->height = DEFAULT_HEIGHT;
	p->cellwidth = DEFAULT_CELLWIDTH;
	p->cellheight = DEFAULT_CELLHEIGHT;
	p->contrast = DEFAULT_CONTRAST;
	p->brightness = DEFAULT_BRIGHTNESS;
	p->offbrightness = DEFAULT_OFFBRIGHTNESS;

	p->framebuf = malloc(p->width * p->height);
	if (p->framebuf == NULL) {
		report(RPT_INFO, "%s: unable to allocate framebuffer", drvthis->name);
		return -1;
	}

	debug_clear(drvthis);

	return 0;
}

// Close the debug driver
MODULE_EXPORT void debug_close(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	if (p != NULL) {
		if (p->framebuf != NULL)
			free(p->framebuf);
		p->framebuf = NULL;

		free(p);
	}
	drvthis->store_private_ptr(drvthis, NULL);
}

// Return the display width in characters
MODULE_EXPORT int debug_width(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	return p->width;
}

// Return the display height in characters
MODULE_EXPORT int debug_height(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	return p->height;
}

// Return the width of a character cell in pixels
MODULE_EXPORT int debug_cellwidth(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	return p->cellwidth;
}

// Return the height of a character cell in pixels
MODULE_EXPORT int debug_cellheight(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	return p->cellheight;
}

// Clear the virtual LCD screen
MODULE_EXPORT void debug_clear(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	memset(p->framebuf, ' ', p->width * p->height);
}

// Flush the frame buffer to debug output
MODULE_EXPORT void debug_flush(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	int i, j;
	char out[LCD_MAX_WIDTH];

	report(RPT_INFO, "%s()", __FUNCTION__);

	// Create border line once: fill buffer with dashes and null-terminate
	for (i = 0; i < p->width; i++) {
		out[i] = '-';
	}
	out[p->width] = 0;

	// Draw top border
	report(RPT_DEBUG, "+%s+", out);

	// Character 0x00 may be valid - avoid string functions
	for (i = 0; i < p->height; i++) {
		for (j = 0; j < p->width; j++) {
			out[j] = p->framebuf[j + (i * p->width)];
		}
		out[p->width] = 0;
		report(RPT_DEBUG, "|%s|", out);
	}

	// Draw bottom border (restore border line with dashes)
	for (i = 0; i < p->width; i++) {
		out[i] = '-';
	}
	report(RPT_DEBUG, "+%s+", out);
}

// Print a string on the virtual display
MODULE_EXPORT void debug_string(Driver *drvthis, int x, int y, const char string[])
{
	PrivateData *p = drvthis->private_data;
	int i;

	report(RPT_INFO, "%s(%i,%i,%.40s)", __FUNCTION__, x, y, string);

	// Convert 1-based to 0-based coordinates, validate y-range, copy string to framebuffer with
	// bounds checking for partial clipping
	y--;
	x--;

	if ((y < 0) || (y >= p->height))
		return;

	for (i = 0; (string[i] != '\0') && (x < p->width); i++, x++) {
		if (x >= 0)
			p->framebuf[(y * p->width) + x] = string[i];
	}
}

// Print a single character on the virtual display
MODULE_EXPORT void debug_chr(Driver *drvthis, int x, int y, char c)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_DEBUG, "%s(%i,%i,%c)", __FUNCTION__, x, y, c);

	// Convert 1-based to 0-based coordinates, validate both x and y are within bounds, write
	// character to framebuffer
	x--;
	y--;

	if ((x >= 0) && (y >= 0) && (x < p->width) && (y < p->height))
		p->framebuf[(y * p->width) + x] = c;
}

/**
 * \brief Draw bar in any direction (horizontal or vertical)
 * \param drvthis Driver instance
 * \param x Starting X coordinate
 * \param y Starting Y coordinate
 * \param len Total length of bar in characters
 * \param promille Fill level in promille (0-1000, where 1000=100%)
 * \param character Character to use for filled portion
 * \param dx X direction increment (+1 right, -1 left, 0 vertical)
 * \param dy Y direction increment (+1 down, -1 up, 0 horizontal)
 *
 * \details Draws bar by incrementing position in specified direction.
 * Fills characters based on promille value (1000 = fully filled).
 */
static void draw_bar(Driver *drvthis, int x, int y, int len, int promille, char character, int dx,
		     int dy)
{
	int pos;

	for (pos = 0; pos < len; pos++) {
		if (2 * pos < ((long)promille * len / 500 + 1)) {
			debug_chr(drvthis, x + pos * dx, y + pos * dy, character);
		}
	}
}

// Draw a vertical bar
MODULE_EXPORT void debug_vbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	report(RPT_INFO, "%s(%i,%i,%i,%i,%i)", __FUNCTION__, x, y, len, promille, options);

	draw_bar(drvthis, x, y, len, promille, '|', 0, -1);
}

// Draw a horizontal bar
MODULE_EXPORT void debug_hbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	report(RPT_INFO, "%s(%i,%i,%i,%i,%i)", __FUNCTION__, x, y, len, promille, options);

	draw_bar(drvthis, x, y, len, promille, '-', 1, 0);
}

// Write a big number to the virtual display
MODULE_EXPORT void debug_num(Driver *drvthis, int x, int num)
{
	report(RPT_INFO, "%s(%i,%i)", __FUNCTION__, x, num);
}

// Place an icon on the virtual display
MODULE_EXPORT int debug_icon(Driver *drvthis, int x, int y, int icon)
{
	report(RPT_INFO, "%s(%i,%i,%i)", __FUNCTION__, x, y, icon);

	// Let the server core handle all icon operations
	return -1;
}

// Set cursor position and appearance
MODULE_EXPORT void debug_cursor(Driver *drvthis, int x, int y, int type)
{
	report(RPT_INFO, "%s (%i,%i,%i)", __FUNCTION__, x, y, type);
}

// Define a custom character
MODULE_EXPORT void debug_set_char(Driver *drvthis, int n, char *dat)
{
	report(RPT_INFO, "%s(%i,data)", __FUNCTION__, n);
}

// Get the number of available custom character slots
MODULE_EXPORT int debug_get_free_chars(Driver *drvthis)
{
	report(RPT_INFO, "%s()", __FUNCTION__);

	return 0;
}

// Get current contrast setting
MODULE_EXPORT int debug_get_contrast(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s()", __FUNCTION__);

	return p->contrast;
}

// Set contrast of virtual display
MODULE_EXPORT void debug_set_contrast(Driver *drvthis, int promille)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s(%i)", __FUNCTION__, promille);

	p->contrast = promille;
}

// Get current brightness setting
MODULE_EXPORT int debug_get_brightness(Driver *drvthis, int state)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s(%i)", __FUNCTION__, state);

	return (state == BACKLIGHT_ON) ? p->brightness : p->offbrightness;
}

// Set brightness of virtual display
MODULE_EXPORT void debug_set_brightness(Driver *drvthis, int state, int promille)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_INFO, "%s(%i,%i)", __FUNCTION__, state, promille);

	if (promille < 0 || promille > 1000)
		return;

	if (state == BACKLIGHT_ON) {
		p->brightness = promille;
	} else {
		p->offbrightness = promille;
	}
	debug_backlight(drvthis, state);
}

// Control virtual display backlight
MODULE_EXPORT void debug_backlight(Driver *drvthis, int on)
{
	report(RPT_INFO, "%s(%i)", __FUNCTION__, on);
}

// Control output state of virtual display
MODULE_EXPORT void debug_output(Driver *drvthis, int value)
{
	report(RPT_INFO, "%s(%i)", __FUNCTION__, value);
}

// Get key input from virtual keyboard
MODULE_EXPORT const char *debug_get_key(Driver *drvthis)
{
	report(RPT_INFO, "%s()", __FUNCTION__);

	return NULL;
}

// Get driver information string
MODULE_EXPORT const char *debug_get_info(Driver *drvthis)
{
	static char *info_string = "debug driver";

	report(RPT_INFO, "%s()", __FUNCTION__);

	return info_string;
}
