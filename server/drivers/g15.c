/** \file server/drivers/g15.c
 * LCDd \c g15 driver for the LCD on the Logitech G15 keyboard.
 */

/*
    Copyright (C) 2006 Anthony J. Mirabella.
    Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>

    2006-07-23 Version 1.0: Most functions should be implemented and working
    2025-08-21 RGB backlights support
	2025-08-31 G-Key macro system

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301


    ==============================================================================
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libg15.h>

/*
 * Workaround for upstream bug: Assume libg15render is built with TTF support,
 * the TTF_SUPPORT define makes the size of the g15 struct bigger, if we do
 * not set this define while libg15render is built with TTF support we get
 * heap corruption. The other way around does not matter, then we just alloc
 * a little bit too much memory (the TTF related variables live at the end
 * of the struct).
 */
#define TTF_SUPPORT
#include <libg15render.h>

#include "g15.h"
#include "lcd.h"

#include "shared/defines.h"
#include "shared/report.h"

/* Vars for the server core */
MODULE_EXPORT char *api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 0;
MODULE_EXPORT int supports_multiple = 0;
MODULE_EXPORT char *symbol_prefix = "g15_";

void g15_close(Driver *drvthis);

static const struct lib_hidraw_id hidraw_ids[] = {
    /* G15 */
    {{BUS_USB, 0x046d, 0xc222}},
    /* G15 v2 */
    {{BUS_USB, 0x046d, 0xc227}},
    /* G510 without a headset plugged in */
    {{BUS_USB, 0x046d, 0xc22d},
     {0x05,
      0x0c,
      0x09,
      0x01,
      0xa1,
      0x01,
      0x85,
      0x02,
      0x15,
      0x00,
      0x25,
      0x01,
      0x75,
      0x01,
      0x95,
      0x07}},
    /* G510 with headset plugged in / with extra USB audio interface */
    {{BUS_USB, 0x046d, 0xc22e},
     {0x05,
      0x0c,
      0x09,
      0x01,
      0xa1,
      0x01,
      0x85,
      0x02,
      0x15,
      0x00,
      0x25,
      0x01,
      0x75,
      0x01,
      0x95,
      0x07}},
    /* Z-10 */
    {{BUS_USB, 0x046d, 0x0a07},
     {0x06,
      0x00,
      0xff,
      0x09,
      0x00,
      0xa1,
      0x01,
      0x15,
      0x00,
      0x26,
      0xff,
      0x00,
      0x75,
      0x08,
      0x95,
      0x08}},
    /* Terminator */
    {}};

// Find the proper usb device and initialize it
//
MODULE_EXPORT int g15_init(Driver *drvthis)
{
	PrivateData *p;

	/* Allocate and store private data */
	p = (PrivateData *)calloc(1, sizeof(PrivateData));
	if (p == NULL)
		return -1;
	drvthis->private_data = p;

	/* Initialize the PrivateData structure */
	p->backlight_state = BACKLIGHT_ON;
	p->hidraw_handle = lib_hidraw_open(hidraw_ids);
	if (!p->hidraw_handle) {
		report(RPT_ERR, "%s: Sorry, cannot find a G15 keyboard", drvthis->name);
		g15_close(drvthis);
		return -1;
	}

	/* Check if device supports RGB backlight (G510 devices) */
	/* TODO: Implement proper device detection based on USB product ID */
	p->has_rgb_backlight = 1; /* Assume RGB support for now */

	/* Check master backlight disable setting */
	int backlight_disabled = drvthis->config_get_bool(drvthis->name, "BacklightDisabled", 0, 0);

	if (backlight_disabled) {
		/* Master disable - ignore all RGB channel settings */
		p->rgb_red = 0;
		p->rgb_green = 0;
		p->rgb_blue = 0;
		p->backlight_state = BACKLIGHT_OFF;
		report(RPT_INFO,
		       "%s: RGB backlight completely disabled via BacklightDisabled=true",
		       drvthis->name);
	} else {
		/* Read RGB configuration from config file (standard 0-255 range) */
		int config_red = drvthis->config_get_int(drvthis->name, "BacklightRed", 0, 255);
		int config_green = drvthis->config_get_int(drvthis->name, "BacklightGreen", 0, 255);
		int config_blue = drvthis->config_get_int(drvthis->name, "BacklightBlue", 0, 255);

		/* Validate and set RGB values */
		if (config_red >= 0 && config_red <= 255 && config_green >= 0 &&
		    config_green <= 255 && config_blue >= 0 && config_blue <= 255) {

			p->rgb_red = (unsigned char)config_red;
			p->rgb_green = (unsigned char)config_green;
			p->rgb_blue = (unsigned char)config_blue;

			report(RPT_INFO,
			       "%s: RGB backlight configured to (%d,%d,%d)",
			       drvthis->name,
			       config_red,
			       config_green,
			       config_blue);
		} else {
			/* Use default white if invalid values */
			p->rgb_red = 255;
			p->rgb_green = 255;
			p->rgb_blue = 255;
			report(RPT_WARNING,
			       "%s: Invalid RGB config values, using default white",
			       drvthis->name);
		}
	}

	p->font = g15r_requestG15DefaultFont(G15_TEXT_LARGE);
	if (p->font == NULL) {
		report(RPT_ERR, "%s: unable to load default large font", drvthis->name);
		g15_close(drvthis);
		return -1;
	}

	g15r_initCanvas(&p->canvas);
	g15r_initCanvas(&p->backingstore);

	/* Set initial RGB backlight colors */
	if (p->has_rgb_backlight && p->backlight_state == BACKLIGHT_ON) {
		g15_set_rgb_backlight(drvthis, p->rgb_red, p->rgb_green, p->rgb_blue);
	}

	return 0;
}

// Close the connection to the LCD
//
MODULE_EXPORT void g15_close(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	drvthis->private_data = NULL;

	g15r_deleteG15Font(p->font);
	if (p->hidraw_handle)
		lib_hidraw_close(p->hidraw_handle);

	free(p);
}

// Returns the display width in characters
//
MODULE_EXPORT int g15_width(Driver *drvthis) { return G15_CHAR_WIDTH; }

// Returns the display height in characters
//
MODULE_EXPORT int g15_height(Driver *drvthis) { return G15_CHAR_HEIGHT; }

// Returns the width of a character in pixels
//
MODULE_EXPORT int g15_cellwidth(Driver *drvthis) { return G15_CELL_WIDTH; }

// Returns the height of a character in pixels
//
MODULE_EXPORT int g15_cellheight(Driver *drvthis) { return G15_CELL_HEIGHT; }

// Clears the LCD screen
//
MODULE_EXPORT void g15_clear(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	g15r_clearScreen(&p->canvas, 0);
	g15r_clearScreen(&p->backingstore, 0);
}

// Convert libg15render canvas format to raw data for the USB output endpoint.
// Based on libg15.c code, which is licensed GPL v2 or later.
//
static void g15_pixmap_to_lcd(unsigned char *lcd_buffer, unsigned char const *data)
{
	/* For a set of bytes (A, B, C, etc.) the bits representing pixels will appear
	   on the LCD like this:

		A0 B0 C0
		A1 B1 C1
		A2 B2 C2
		A3 B3 C3 ... and across for G15_LCD_WIDTH bytes
		A4 B4 C4
		A5 B5 C5
		A6 B6 C6
		A7 B7 C7

		A0
		A1  <- second 8-pixel-high row starts straight after the last byte on
		A2     the previous row
		A3
		A4
		A5
		A6
		A7
		A8

		A0
		...
		A0
		...
		A0
		...
		A0
		A1 <- only the first three bits are shown on the bottom row (the last three
		A2    pixels of the 43-pixel high display.)
	*/

	const unsigned int stride = G15_LCD_WIDTH / 8;
	unsigned int row, col;

	lcd_buffer[0] = 0x03; /* Set output report 3 */
	memset(lcd_buffer + 1, 0, G15_LCD_OFFSET - 1);
	lcd_buffer += G15_LCD_OFFSET;

	/* 43 pixels height, requires 6 bytes for each column */
	for (row = 0; row < 6; row++) {
		for (col = 0; col < G15_LCD_WIDTH; col++) {
			unsigned int bit = col % 8;

			/* Copy a 1x8 column of pixels across from the source
			 * image to the LCD buffer. */

			*lcd_buffer++ = (((data[stride * 0] << bit) & 0x80) >> 7) |
					(((data[stride * 1] << bit) & 0x80) >> 6) |
					(((data[stride * 2] << bit) & 0x80) >> 5) |
					(((data[stride * 3] << bit) & 0x80) >> 4) |
					(((data[stride * 4] << bit) & 0x80) >> 3) |
					(((data[stride * 5] << bit) & 0x80) >> 2) |
					(((data[stride * 6] << bit) & 0x80) >> 1) |
					(((data[stride * 7] << bit) & 0x80) >> 0);

			if (bit == 7)
				data++;
		}
		/* Jump down seven pixel-rows in the source image,
		 * since we've just done a row of eight pixels in one pass
		 * (and we counted one pixel-row while we were going). */
		data += 7 * stride;
	}
}

// Blasts a single frame onscreen, to the lcd...
//
MODULE_EXPORT void g15_flush(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	/* 43 pixels height, requires 6 bytes for each column */
	unsigned char lcd_buf[G15_LCD_OFFSET + 6 * G15_LCD_WIDTH];

	if (memcmp(p->backingstore.buffer,
		   p->canvas.buffer,
		   G15_BUFFER_LEN * sizeof(unsigned char)) == 0)
		return;

	memcpy(p->backingstore.buffer, p->canvas.buffer, G15_BUFFER_LEN * sizeof(unsigned char));

	g15_pixmap_to_lcd(lcd_buf, p->canvas.buffer);
	lib_hidraw_send_output_report(p->hidraw_handle, lcd_buf, sizeof(lcd_buf));
}

// LCDd 1-dimension char coordinates to g15r 0-(dimension-1) pixel coords */
int g15_convert_coords(int x, int y, int *px, int *py)
{
	*px = (x - 1) * G15_CELL_WIDTH;
	*py = (y - 1) * G15_CELL_HEIGHT;

	/* We have 5 lines of 8 pixels heigh, so 40 pixels, but the LCD is
	 * 43 pixels high. This allows us to add an empty line between 4 of
	 * the 5 lines. This is desirable to avoid the descenders from the
	 * non caps 'g' and 'y' glyphs touching the top of the chars of the
	 * next line.
	 * This also makes us better use the whole height of the LCD.
	 */
	*py += min(y - 1, 3);

	if ((*px + G15_CELL_WIDTH) > G15_LCD_WIDTH || (*py + G15_CELL_HEIGHT) > G15_LCD_HEIGHT)
		return 0; /* Failure */

	return 1; /* Success */
}

// Character function for the lcdproc driver API
//
MODULE_EXPORT void g15_chr(Driver *drvthis, int x, int y, char c)
{
	PrivateData *p = drvthis->private_data;
	int px, py;

	if (!g15_convert_coords(x, y, &px, &py))
		return;

	/* Clear background */
	g15r_pixelReverseFill(&p->canvas,
			      px,
			      py,
			      px + G15_CELL_WIDTH - 1,
			      py + G15_CELL_HEIGHT - 1,
			      G15_PIXEL_FILL,
			      G15_COLOR_WHITE);
	/* Render character, coords - 1 because of g15r peculiarities  */
	g15r_renderG15Glyph(&p->canvas, p->font, c, px - 1, py - 1, G15_COLOR_BLACK, 0);
}

// Prints a string on the lcd display, at position (x,y).  The
// upper-left is (1,1), and the lower right should be (20,5).
//
MODULE_EXPORT void g15_string(Driver *drvthis, int x, int y, const char string[])
{
	int i;

	for (i = 0; string[i] != '\0'; i++)
		g15_chr(drvthis, x + i, y, string[i]);
}

// Draws an icon on the screen
MODULE_EXPORT int g15_icon(Driver *drvthis, int x, int y, int icon)
{
	PrivateData *p = drvthis->private_data;
	unsigned char character;
	int px1, py1, px2, py2;

	switch (icon) {
	/* Special cases */
	case ICON_BLOCK_FILLED:
		if (!g15_convert_coords(x, y, &px1, &py1))
			return -1;

		px2 = px1 + G15_CELL_WIDTH - 2;
		py2 = py1 + G15_CELL_HEIGHT - 2;
		g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
		return 0;

	case ICON_HEART_OPEN:
		p->canvas.mode_reverse = 1;
		g15_chr(drvthis, x, y, G15_ICON_HEART_OPEN);
		p->canvas.mode_reverse = 0;
		return 0;

	/* Simple 1:1 mapping cases */
	case ICON_HEART_FILLED:
		character = G15_ICON_HEART_FILLED;
		break;
	case ICON_ARROW_UP:
		character = G15_ICON_ARROW_UP;
		break;
	case ICON_ARROW_DOWN:
		character = G15_ICON_ARROW_DOWN;
		break;
	case ICON_ARROW_LEFT:
		character = G15_ICON_ARROW_LEFT;
		break;
	case ICON_ARROW_RIGHT:
		character = G15_ICON_ARROW_RIGHT;
		break;
	case ICON_CHECKBOX_OFF:
		character = G15_ICON_CHECKBOX_OFF;
		break;
	case ICON_CHECKBOX_ON:
		character = G15_ICON_CHECKBOX_ON;
		break;
	case ICON_CHECKBOX_GRAY:
		character = G15_ICON_CHECKBOX_GRAY;
		break;
	case ICON_STOP:
		character = G15_ICON_STOP;
		break;
	case ICON_PAUSE:
		character = G15_ICON_PAUSE;
		break;
	case ICON_PLAY:
		character = G15_ICON_PLAY;
		break;
	case ICON_PLAYR:
		character = G15_ICON_PLAYR;
		break;
	case ICON_FF:
		character = G15_ICON_FF;
		break;
	case ICON_FR:
		character = G15_ICON_FR;
		break;
	case ICON_NEXT:
		character = G15_ICON_NEXT;
		break;
	case ICON_PREV:
		character = G15_ICON_PREV;
		break;
	case ICON_REC:
		character = G15_ICON_REC;
		break;
	/* Let the core do other icons */
	default:
		return -1;
	}

	g15_chr(drvthis, x, y, character);
	return 0;
}

// Draws a horizontal bar growing to the right
//
MODULE_EXPORT void g15_hbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	int total_pixels = ((long)2 * len * G15_CELL_WIDTH + 1) * promille / 2000;
	int px1, py1, px2, py2;

	if (!g15_convert_coords(x, y, &px1, &py1))
		return;

	px2 = px1 + total_pixels;
	py2 = py1 + G15_CELL_HEIGHT - 2;

	g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
}

// Draws a vertical bar growing up
//
MODULE_EXPORT void g15_vbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	int total_pixels = ((long)2 * len * G15_CELL_WIDTH + 1) * promille / 2000;
	int px1, py1, px2, py2;

	if (!g15_convert_coords(x, y, &px1, &py1))
		return;

	/* vbar grow from the bottom upwards, flip the Y-coordinates */
	py1 = py1 + G15_CELL_HEIGHT - total_pixels;
	py2 = py1 + total_pixels - 1;
	px2 = px1 + G15_CELL_WIDTH - 2;

	g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
}

//  Return one char from the Keyboard
//
MODULE_EXPORT const char *g15_get_key(Driver *drvthis)
{
	/* Key input not implemented for direct hidraw access */
	return NULL;
}

// Set the backlight
//
MODULE_EXPORT void g15_backlight(Driver *drvthis, int on)
{
	PrivateData *p = drvthis->private_data;

	/* DEBUG: Log every call to this function */
	report(RPT_DEBUG,
	       "%s: g15_backlight() called with on=%d (current state=%d)",
	       drvthis->name,
	       on,
	       p->backlight_state);

	if (p->backlight_state == on)
		return;

	p->backlight_state = on;

	if (p->has_rgb_backlight) {
		if (on == BACKLIGHT_ON) {
			/* Restore saved RGB values */
			g15_set_rgb_backlight(drvthis, p->rgb_red, p->rgb_green, p->rgb_blue);
		} else {
			/* Turn off backlight by setting all colors to 0 */
			g15_set_rgb_backlight(drvthis, 0, 0, 0);
		}
	}
}

// Set RGB backlight colors (G510 only)
//
/* Helper function to write to LED subsystem file */
static int write_led_file(const char *path, const char *value)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		return -1;
	}

	int result = fprintf(f, "%s", value);
	fclose(f);

	return (result > 0) ? 0 : -1;
}

MODULE_EXPORT int g15_set_rgb_backlight(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;
	char color_hex[8];
	int result = 0;

	if (!p->has_rgb_backlight) {
		report(RPT_WARNING, "%s: Device does not support RGB backlight", drvthis->name);
		return -1;
	}

	/* Validate RGB values */
	if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
		report(
		    RPT_ERR, "%s: Invalid RGB values (%d,%d,%d)", drvthis->name, red, green, blue);
		return -1;
	}

	/* Store current RGB values */
	p->rgb_red = (unsigned char)red;
	p->rgb_green = (unsigned char)green;
	p->rgb_blue = (unsigned char)blue;

	/* Use LED subsystem for RGB control - works with hardware button */
	snprintf(color_hex, sizeof(color_hex), "#%02x%02x%02x", red, green, blue);

	/* Set keyboard backlight color */
	if (write_led_file("/sys/class/leds/g15::kbd_backlight/color", color_hex) < 0) {
		report(RPT_ERR,
		       "%s: Failed to set keyboard backlight color via LED subsystem",
		       drvthis->name);
		result = -1;
	}

	/* Set power-on backlight color (persistent setting) */
	if (write_led_file("/sys/class/leds/g15::power_on_backlight_val/color", color_hex) < 0) {
		report(RPT_ERR,
		       "%s: Failed to set power-on backlight color via LED subsystem",
		       drvthis->name);
		result = -1;
	}

	/* Ensure backlight is on with full brightness */
	if (red > 0 || green > 0 || blue > 0) {
		if (write_led_file("/sys/class/leds/g15::kbd_backlight/brightness", "255") < 0) {
			report(RPT_ERR, "%s: Failed to set backlight brightness", drvthis->name);
			result = -1;
		}
		if (write_led_file("/sys/class/leds/g15::power_on_backlight_val/brightness",
				   "255") < 0) {
			report(RPT_ERR, "%s: Failed to set power-on brightness", drvthis->name);
			result = -1;
		}
	} else {
		/* All colors are 0, turn off backlight */
		if (write_led_file("/sys/class/leds/g15::kbd_backlight/brightness", "0") < 0) {
			report(RPT_ERR, "%s: Failed to turn off backlight", drvthis->name);
			result = -1;
		}
	}

	if (result == 0) {
		report(RPT_INFO,
		       "%s: Set RGB backlight via LED subsystem to (%d,%d,%d)",
		       drvthis->name,
		       red,
		       green,
		       blue);
	}

	return result;
}

MODULE_EXPORT void g15_num(Driver *drvthis, int x, int num)
{
	PrivateData *p = drvthis->private_data;

	x--;
	int ox = x * G15_CELL_WIDTH;

	if ((num < 0) || (num > 10))
		return;

	int width = 0;
	int height = 43;

	if ((num >= 0) && (num <= 9))
		width = 24;
	else
		width = 9;

	int i = 0;

	for (i = 0; i < (width * height); ++i) {
		int color = (g15_bignum_data[num][i] ? G15_COLOR_WHITE : G15_COLOR_BLACK);
		int px = ox + i % width;
		int py = i / width;
		g15r_setPixel(&p->canvas, px, py, color);
	}
}
