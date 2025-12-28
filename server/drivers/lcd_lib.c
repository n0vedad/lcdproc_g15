// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/lcd_lib.c
 * \brief Implementation of LCD driver utility library
 * \author LCDproc developers
 * \date 2000-2024
 *
 *
 * \features
 * - Implementation of LCD driver utility library providing common drawing functions
 * - Static horizontal bar drawing with custom character support and configurable dimensions
 * - Static vertical bar drawing with custom character support and configurable dimensions
 * - Promille-based fill level calculation for precise progress indication (0-1000 scale)
 * - Support for both seamless and discrete bar rendering modes via options parameter
 * - Configurable custom character offsets for different LCD controller character sets
 * - Cell width and height abstraction supporting various LCD geometries
 * - Cross-driver compatibility for shared bar drawing functionality
 * - Legacy v0.5 API format support for backward compatibility
 * - Pixel-level precision in bar rendering calculations
 * - Icon-based block filling for standard LCD icons
 * - Custom character assumption: char 1 = 1 pixel, char 2 = 2 pixels, etc.
 *
 * \usage
 * - Used by LCD drivers requiring horizontal or vertical bar drawing functionality
 * - Primary usage with character-based LCD displays (HD44780 and compatible controllers)
 * - Called by driver-specific hbar/vbar functions for common bar rendering logic
 * - Supports progress bars, level indicators, and graphical meter elements
 * - Compatible with LCD drivers using statically defined custom characters
 * - Used for BAR_SEAMLESS rendering mode with custom character sets
 * - Abstraction for different LCD cell dimensions and character offsets
 * - Legacy support for drivers migrated from "base driver" architecture
 *
 * \details Library of useful functions for LCD drivers containing common
 * functionality for drawing bars and graphical elements shared across drivers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lcd.h"

/**
 * \brief Draw bar using pre-defined custom characters
 * \param drvthis Driver instance
 * \param x Starting X coordinate
 * \param y Starting Y coordinate
 * \param len Bar length in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar options flags
 * \param cellsize Cell dimension in pixels (width for hbar, height for vbar)
 * \param cc_offset Custom character offset in driver's character set
 * \param dx X direction increment (+1 right, -1 left, 0 vertical)
 * \param dy Y direction increment (+1 down, -1 up, 0 horizontal)
 *
 * \details Uses custom characters to draw smooth bars with sub-character resolution.
 * Pre-generates custom characters for partial fill levels.
 */
static void lib_bar_static_internal(Driver *drvthis, int x, int y, int len, int promille,
				    int options, int cellsize, int cc_offset, int dx, int dy)
{
	int total_pixels = ((long)2 * len * cellsize + 1) * promille / 2000;
	int pos;

	for (pos = 0; pos < len; pos++) {
		int pixels = total_pixels - cellsize * pos;
		int cur_x = x + pos * dx;
		int cur_y = y + pos * dy;

		if (pixels >= cellsize) {
			// Full block
			if ((options & BAR_SEAMLESS) && dx != 0) {
				drvthis->chr(drvthis, cur_x, cur_y, cellsize + cc_offset);
			} else {
				drvthis->icon(drvthis, cur_x, cur_y, ICON_BLOCK_FILLED);
			}
		} else if (pixels > 0) {
			// Partial block
			drvthis->chr(drvthis, cur_x, cur_y, pixels + cc_offset);
			break;
		}
	}
}

// Draw a horizontal bar using static custom characters
void lib_hbar_static(Driver *drvthis, int x, int y, int len, int promille, int options,
		     int cellwidth, int cc_offset)
{
	lib_bar_static_internal(drvthis, x, y, len, promille, options, cellwidth, cc_offset, 1, 0);
}

// Draw a vertical bar using static custom characters
void lib_vbar_static(Driver *drvthis, int x, int y, int len, int promille, int options,
		     int cellheight, int cc_offset)
{
	lib_bar_static_internal(drvthis, x, y, len, promille, options, cellheight, cc_offset, 0,
				-1);
}
