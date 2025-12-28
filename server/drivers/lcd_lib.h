// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/lcd_lib.h
 * \brief Header file for LCD driver utility library
 * \author LCDproc developers
 * \date 2000-2024
 *
 *
 * \features
 * - Header file for LCD driver utility library providing common drawing functions
 * - Static horizontal bar drawing functions with configurable cell dimensions
 * - Static vertical bar drawing functions with configurable cell dimensions
 * - Support for custom character offsets in LCD character sets
 * - BAR_SEAMLESS option support for smooth bar rendering
 * - Configurable promille-based progress indication (0-1000 scale)
 * - Cross-driver compatibility for shared bar drawing functionality
 * - Cell width and height abstraction for different LCD geometries
 * - Options parameter for rendering mode control
 *
 * \usage
 * - Used by LCD drivers requiring horizontal or vertical bar drawing
 * - Include in drivers that have statically defined custom characters
 * - Primary usage with character-based LCD displays (HD44780, compatible)
 * - Supports both seamless and discrete bar rendering modes
 * - Used for progress bars, level indicators, and graphical elements
 * - Compatible with drivers using custom character generation
 * - Abstraction layer for different LCD cell dimensions
 *
 * \details Header file for utility functions useful for LCD drivers
 * providing common functionality for drawing bars and graphical elements.
 */

#ifndef LCD_LIB_H
#define LCD_LIB_H

#ifndef LCD_H
#include "lcd.h"
#endif

/**
 * \brief Draw a horizontal bar using static custom characters
 * \param drvthis Pointer to driver structure
 * \param x Starting horizontal position
 * \param y Vertical position
 * \param len Maximum length of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options (BAR_SEAMLESS, etc.)
 * \param cellwidth Width of character cell in pixels
 * \param cc_offset Offset for custom character numbers
 *
 * \details Places a horizontal bar using the v0.5 API format and the given
 * cellwidth. Assumes that custom chars have been statically defined, so that
 * character 1 has 1 pixel, character 2 has 2 pixels, etc.
 * LCDs that have custom chars at positions other than 0 should put the
 * first custom char number in cc_offset.
 */
void lib_hbar_static(Driver *drvthis, int x, int y, int len, int promille, int options,
		     int cellwidth, int cc_offset);

/**
 * \brief Draw a vertical bar using static custom characters
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Starting vertical position (bottom)
 * \param len Maximum height of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options
 * \param cellheight Height of character cell in pixels
 * \param cc_offset Offset for custom character numbers
 *
 * \details Places a vertical bar using the v0.5 API format and the given
 * cellheight. Assumes that custom chars have been statically defined, so that
 * character 1 has 1 pixel, character 2 has 2 pixels, etc.
 * LCDs that have custom chars at positions other than 0 should put the
 * first custom char number in cc_offset.
 */
void lib_vbar_static(Driver *drvthis, int x, int y, int len, int promille, int options,
		     int cellheight, int cc_offset);

#endif
