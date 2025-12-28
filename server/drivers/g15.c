// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/g15.c
 * \brief LCDd driver for Logitech G-Series keyboards with 160x43 monochrome LCDs
 * \author Anthony J. Mirabella
 * \author n0vedad
 * \date 2006-2025
 *
 *
 * \features
 * - Driver implementation for Logitech G-Series keyboards supporting 160x43 monochrome LCD displays
 * - Comprehensive support for G15, G15 v2, and G510 keyboards with device-specific capabilities
 * - USB HID communication via hidraw interface for reliable device access
 * - RGB backlight control for G510/G510s keyboards with zone support
 * - Macro LED control for G510/G510s keyboards (M1, M2, M3, MR LEDs)
 * - Big number display rendering with 32x32 pixel bitmaps
 * - Icon and graphics rendering with predefined icon library
 * - Horizontal and vertical progress bar rendering
 * - Key input handling and event processing
 * - Double buffering with canvas and backingstore for smooth updates
 * - libg15render integration for advanced display operations
 * - Font rendering support with TTF_SUPPORT workaround
 * - Device detection and capability auto-configuration
 * - Memory management for display buffers and device state
 *
 * \usage
 * - Used by LCDd server when "g15" driver is specified in configuration
 * - Supports Logitech G15 (Original, USB ID: 046d:c222) - Monochrome LCD only
 * - Supports Logitech G15 v2 (USB ID: 046d:c227) - Monochrome LCD only
 * - Supports Logitech G510 (USB ID: 046d:c22d/046d:c22e) - LCD + RGB backlight
 * - Primary target: Logitech G510s keyboards using G510 USB IDs
 * - Driver automatically detects device capabilities and configures features
 * - Requires hidraw kernel module and appropriate device permissions
 * - Configuration options for RGB backlight and macro LED control
 *
 * \details Driver implementation for Logitech G-Series keyboards supporting
 * 160x43 monochrome LCD displays. Provides comprehensive support for G15,
 * G15 v2, and G510 keyboards with additional RGB backlight and macro LED
 * control for G510 models.
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

/**
 * \note Workaround for upstream bug: Assume libg15render is built with TTF support
 *
 * The TTF_SUPPORT define makes the size of the g15 struct bigger. If we do not set this
 * define while libg15render is built with TTF support, we get heap corruption. The other
 * way around does not matter - we just allocate a little bit too much memory (the TTF
 * related variables live at the end of the struct).
 */
#define TTF_SUPPORT
#include <libg15render.h>

#include "g15.h"
#include "lcd.h"

#include "shared/defines.h"
#include "shared/report.h"

/** \name G15 Driver Module Exports
 * Driver metadata exported to the LCDd server core
 */
///@{
MODULE_EXPORT char *api_version = API_VERSION; ///< Driver API version string
MODULE_EXPORT int stay_in_foreground = 0;      ///< G15 driver can run as daemon
MODULE_EXPORT int supports_multiple = 0;       ///< G15 driver does not support multiple instances
MODULE_EXPORT char *symbol_prefix = "g15_";    ///< Function symbol prefix for this driver
///@}

void g15_close(Driver *drvthis);
static void g15_pixmap_to_lcd(unsigned char *lcd_buffer, unsigned char const *data);

/** \brief Supported Logitech G-Series keyboard USB device IDs
 *
 * \details Array of USB Vendor/Product IDs and HID report descriptor headers
 * for supported G15/G510 keyboards. Used by lib_hidraw_open_device() to find
 * and open the correct HID device. Table is null-terminated.
 */
static const struct lib_hidraw_id hidraw_ids[] = {
    // G15 (Original) - Monochrome LCD only
    {{BUS_USB, 0x046d, 0xc222}},
    // G15 v2 - Monochrome LCD only
    {{BUS_USB, 0x046d, 0xc227}},
    // G510 without headset - Monochrome LCD + RGB backlight
    {{BUS_USB, 0x046d, 0xc22d},
     {0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95,
      0x07}},
    // G510 with headset / G510s - Monochrome LCD + RGB backlight
    {{BUS_USB, 0x046d, 0xc22e},
     {0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x02, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95,
      0x07}},
    // Z-10
    {{BUS_USB, 0x046d, 0x0a07},
     {0x06, 0x00, 0xff, 0x09, 0x00, 0xa1, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95,
      0x08}},
    {}};

// Initialize the G15 driver
MODULE_EXPORT int g15_init(Driver *drvthis)
{
	PrivateData *p;

	p = (PrivateData *)calloc(1, sizeof(PrivateData));
	if (p == NULL)
		return -1;
	drvthis->private_data = p;

	p->backlight_state = BACKLIGHT_ON;
	p->macro_leds = 0;

	const char *rgb_method_str =
	    drvthis->config_get_string(drvthis->name, "RGBMethod", 0, "led_subsystem");
	p->rgb_method_hid = (strcmp(rgb_method_str, "hid_reports") == 0) ? 1 : 0;
	report(RPT_INFO, "%s: Using RGB method: %s", drvthis->name, rgb_method_str);

	p->hidraw_handle = lib_hidraw_open(hidraw_ids);
	if (!p->hidraw_handle) {
		report(RPT_ERR, "%s: Sorry, cannot find a G15 keyboard", drvthis->name);
		g15_close(drvthis);
		return -1;
	}

	unsigned short product_id = lib_hidraw_get_product_id(p->hidraw_handle);
	if (product_id == 0xc22d || product_id == 0xc22e) {
		p->has_rgb_backlight = 1;
		report(RPT_INFO,
		       "%s: Detected G510/G510s device (USB ID: 046d:%04x) - RGB backlight enabled",
		       drvthis->name, product_id);
	} else {
		p->has_rgb_backlight = 0;
		report(RPT_INFO,
		       "%s: Detected G15 device (USB ID: 046d:%04x) - RGB backlight disabled",
		       drvthis->name, product_id);
	}

	int backlight_disabled = drvthis->config_get_bool(drvthis->name, "BacklightDisabled", 0, 0);

	if (backlight_disabled) {
		p->rgb_red = 0;
		p->rgb_green = 0;
		p->rgb_blue = 0;
		p->backlight_state = BACKLIGHT_OFF;
		report(RPT_INFO, "%s: RGB backlight completely disabled via BacklightDisabled=true",
		       drvthis->name);

	} else {
		int config_red = drvthis->config_get_int(drvthis->name, "BacklightRed", 0, 255);
		int config_green = drvthis->config_get_int(drvthis->name, "BacklightGreen", 0, 255);
		int config_blue = drvthis->config_get_int(drvthis->name, "BacklightBlue", 0, 255);

		if (config_red >= 0 && config_red <= 255 && config_green >= 0 &&
		    config_green <= 255 && config_blue >= 0 && config_blue <= 255) {

			p->rgb_red = (unsigned char)config_red;
			p->rgb_green = (unsigned char)config_green;
			p->rgb_blue = (unsigned char)config_blue;

			report(RPT_INFO, "%s: RGB backlight configured to (%d,%d,%d)",
			       drvthis->name, config_red, config_green, config_blue);

		} else {
			p->rgb_red = 255;
			p->rgb_green = 255;
			p->rgb_blue = 255;
			report(RPT_WARNING, "%s: Invalid RGB config values, using default white",
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

	if (p->has_rgb_backlight && p->backlight_state == BACKLIGHT_ON) {
		g15_set_rgb_backlight(drvthis, p->rgb_red, p->rgb_green, p->rgb_blue);
	}

	if (p->has_rgb_backlight) {
		g15_set_macro_leds(drvthis, 1, 0, 0, 0);
	}

	// CRITICAL: Send blank frame to force-clear hardware logo after USB reset
	// The G510 shows a boot logo that can sometime persists until we send data
	// Explicitly clear canvas and send it to overwrite the logo
	g15r_clearScreen(&p->canvas, G15_COLOR_WHITE);
	unsigned char lcd_buf[G15_LCD_OFFSET + 6 * G15_LCD_WIDTH];
	memset(lcd_buf + 1, 0, G15_LCD_OFFSET - 1);
	g15_pixmap_to_lcd(lcd_buf, p->canvas.buffer);
	lib_hidraw_send_output_report(p->hidraw_handle, lcd_buf, sizeof(lcd_buf));
	memcpy(p->backingstore.buffer, p->canvas.buffer, G15_BUFFER_LEN * sizeof(unsigned char));
	report(RPT_INFO, "%s: Sent blank frame to force-clear hardware logo", drvthis->name);

	return 0;
}

// Close the G15 driver and clean up resources
MODULE_EXPORT void g15_close(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	drvthis->private_data = NULL;
	g15r_deleteG15Font(p->font);

	if (p->hidraw_handle) {
		lib_hidraw_close(p->hidraw_handle);
	}

	free(p);
}

// Return the display width in characters
MODULE_EXPORT int g15_width(Driver *drvthis) { return G15_CHAR_WIDTH; }

// Return the display height in characters
MODULE_EXPORT int g15_height(Driver *drvthis) { return G15_CHAR_HEIGHT; }

// Return the width of a character cell in pixels
MODULE_EXPORT int g15_cellwidth(Driver *drvthis) { return G15_CELL_WIDTH; }

// Return the height of a character cell in pixels
MODULE_EXPORT int g15_cellheight(Driver *drvthis) { return G15_CELL_HEIGHT; }

// Clear the LCD screen
MODULE_EXPORT void g15_clear(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	report(RPT_DEBUG, "%s: Clearing ONLY canvas buffer (backingstore kept for diff)",
	       drvthis->name);
	g15r_clearScreen(&p->canvas, 0);
	// NEVER clear backingstore - it must keep the last sent frame for memcmp optimization
}

/**
 * \brief Convert libg15render canvas format to raw data for the USB output endpoint
 * \param lcd_buffer Destination buffer for LCD data (must be sized for G15_LCD_OFFSET + 6 *
 * G15_LCD_WIDTH bytes)
 * \param data Source pixel data from libg15render canvas buffer
 *
 * \details Transforms the horizontal pixel layout used by libg15render into the
 * vertical column-oriented format required by the G15 LCD hardware. The G15 LCD
 * uses a column-major layout where each byte represents 8 vertical pixels.
 */
static void g15_pixmap_to_lcd(unsigned char *lcd_buffer, unsigned char const *data)
{
	/**
	 * \note For a set of bytes (A, B, C, etc.) the bits representing pixels will appear
	 * on the LCD like this:
	 *
	 * A0 B0 C0
	 * A1 B1 C1
	 * A2 B2 C2
	 * A3 B3 C3 ... and across for G15_LCD_WIDTH bytes
	 * A4 B4 C4
	 * A5 B5 C5
	 * A6 B6 C6
	 * A7 B7 C7
	 *
	 * A0
	 * A1  <- second 8-pixel-high row starts straight after the last byte on
	 * A2     the previous row
	 * A3
	 * A4
	 * A5
	 * A6
	 * A7
	 * A8
	 *
	 * A0
	 * ...
	 * A0
	 * ...
	 * A0
	 * ...
	 * A0
	 * A1 <- only the first three bits are shown on the bottom row (the last three
	 * A2    pixels of the 43-pixel high display.)
	 */

	const unsigned int stride = G15_LCD_WIDTH / 8;
	unsigned int row, col;

	// Set output report ID and initialize buffer header
	lcd_buffer[0] = 0x03;
	memset(lcd_buffer + 1, 0, G15_LCD_OFFSET - 1);
	lcd_buffer += G15_LCD_OFFSET;

	// Process 6 rows of 8 pixels each (43 pixel height requires 6 bytes per column)
	for (row = 0; row < 6; row++) {
		for (col = 0; col < G15_LCD_WIDTH; col++) {
			unsigned int bit = col % 8;

			// Extract 8 vertical pixels and pack into single byte
			*lcd_buffer++ = (((data[stride * 0] << bit) & 0x80) >> 7) |
					(((data[stride * 1] << bit) & 0x80) >> 6) |
					(((data[stride * 2] << bit) & 0x80) >> 5) |
					(((data[stride * 3] << bit) & 0x80) >> 4) |
					(((data[stride * 4] << bit) & 0x80) >> 3) |
					(((data[stride * 5] << bit) & 0x80) >> 2) |
					(((data[stride * 6] << bit) & 0x80) >> 1) |
					(((data[stride * 7] << bit) & 0x80) >> 0);

			// Advance to next byte in source data after processing 8 columns
			if (bit == 7)
				data++;
		}
		// Skip 7 rows in source (we already processed row 0 above)
		data += 7 * stride;
	}
}

// Flush the frame buffer to the LCD display
MODULE_EXPORT void g15_flush(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	unsigned char lcd_buf[G15_LCD_OFFSET + 6 * G15_LCD_WIDTH];
	static int flush_count = 0;

	flush_count++;

	// Calculate checksums for debugging
	unsigned int canvas_sum = 0;
	unsigned int backing_sum = 0;
	for (int i = 0; i < G15_BUFFER_LEN; i++) {
		canvas_sum += p->canvas.buffer[i];
		backing_sum += p->backingstore.buffer[i];
	}

	report(RPT_DEBUG, "%s: flush #%d - canvas_checksum=%u, backing_checksum=%u", drvthis->name,
	       flush_count, canvas_sum, backing_sum);

	if (memcmp(p->backingstore.buffer, p->canvas.buffer,
		   G15_BUFFER_LEN * sizeof(unsigned char)) == 0) {
		report(RPT_DEBUG, "%s: Buffers identical - SKIPPING update to hardware",
		       drvthis->name);
		return;
	}

	report(RPT_DEBUG, "%s: Buffers differ - SENDING update to hardware", drvthis->name);
	memcpy(p->backingstore.buffer, p->canvas.buffer, G15_BUFFER_LEN * sizeof(unsigned char));
	g15_pixmap_to_lcd(lcd_buf, p->canvas.buffer);
	lib_hidraw_send_output_report(p->hidraw_handle, lcd_buf, sizeof(lcd_buf));
	report(RPT_DEBUG, "%s: Hardware update completed", drvthis->name);
}

/**
 * \brief Convert LCDd character coordinates to pixel coordinates
 * \param x Character column position (1-based, typically 1-26)
 * \param y Character row position (1-based, typically 1-5)
 * \param px Pointer to store resulting pixel X coordinate
 * \param py Pointer to store resulting pixel Y coordinate
 * \retval 1 Coordinates valid and converted successfully
 * \retval 0 Coordinates out of bounds
 *
 * \details Converts character cell coordinates to pixel coordinates with inter-row
 * spacing to prevent descender collisions. Validates that the character cell fits
 * within display boundaries.
 */
int g15_convert_coords(int x, int y, int *px, int *py)
{
	*px = (x - 1) * G15_CELL_WIDTH;
	*py = (y - 1) * G15_CELL_HEIGHT;

	/**
	 * \note Add spacing between rows to avoid descender collisions (g, y, p, q, j)
	 * We have 5 lines × 8 pixels = 40 pixels, LCD is 43 pixels high
	 * This gives us 3 extra pixels to distribute between 4 gaps
	 */
	*py += min(y - 1, 3);

	// Validate that character cell fits within display boundaries
	if ((*px + G15_CELL_WIDTH) > G15_LCD_WIDTH || (*py + G15_CELL_HEIGHT) > G15_LCD_HEIGHT) {
		return 0;
	}

	return 1;
}

// Place a single character on the LCD at specified position
MODULE_EXPORT void g15_chr(Driver *drvthis, int x, int y, char c)
{
	PrivateData *p = drvthis->private_data;
	int px, py;

	if (!g15_convert_coords(x, y, &px, &py)) {
		return;
	}

	g15r_pixelReverseFill(&p->canvas, px, py, px + G15_CELL_WIDTH - 1, py + G15_CELL_HEIGHT - 1,
			      G15_PIXEL_FILL, G15_COLOR_WHITE);

	g15r_renderG15Glyph(&p->canvas, p->font, c, px - 1, py - 1, G15_COLOR_BLACK, 0);
}

// Print a string on the LCD display at specified position
MODULE_EXPORT void g15_string(Driver *drvthis, int x, int y, const char string[])
{
	int i;

	report(RPT_DEBUG, "%s: Rendering string at (%d,%d): '%s'", drvthis->name, x, y, string);

	// Render each character sequentially
	for (i = 0; string[i] != '\0'; i++) {
		g15_chr(drvthis, x + i, y, string[i]);
	}
}

// Draw an icon on the LCD display
MODULE_EXPORT int g15_icon(Driver *drvthis, int x, int y, int icon)
{
	PrivateData *p = drvthis->private_data;
	unsigned char character;
	int px1, py1, px2, py2;

	switch (icon) {

	// Filled block icon - draw black rectangle
	case ICON_BLOCK_FILLED:
		if (!g15_convert_coords(x, y, &px1, &py1)) {
			return -1;
		}

		px2 = px1 + G15_CELL_WIDTH - 2;
		py2 = py1 + G15_CELL_HEIGHT - 2;

		g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
		return 0;

	// Open heart icon - requires reverse mode
	case ICON_HEART_OPEN:
		p->canvas.mode_reverse = 1;
		g15_chr(drvthis, x, y, G15_ICON_HEART_OPEN);
		p->canvas.mode_reverse = 0;
		return 0;

	// Filled heart icon
	case ICON_HEART_FILLED:
		character = G15_ICON_HEART_FILLED;
		break;

	// Up arrow icon
	case ICON_ARROW_UP:
		character = G15_ICON_ARROW_UP;
		break;

	// Down arrow icon
	case ICON_ARROW_DOWN:
		character = G15_ICON_ARROW_DOWN;
		break;

	// Left arrow icon
	case ICON_ARROW_LEFT:
		character = G15_ICON_ARROW_LEFT;
		break;

	// Right arrow icon
	case ICON_ARROW_RIGHT:
		character = G15_ICON_ARROW_RIGHT;
		break;

	// Unchecked checkbox icon
	case ICON_CHECKBOX_OFF:
		character = G15_ICON_CHECKBOX_OFF;
		break;

	// Checked checkbox icon
	case ICON_CHECKBOX_ON:
		character = G15_ICON_CHECKBOX_ON;
		break;

	// Gray checkbox icon
	case ICON_CHECKBOX_GRAY:
		character = G15_ICON_CHECKBOX_GRAY;
		break;

	// Stop icon
	case ICON_STOP:
		character = G15_ICON_STOP;
		break;

	// Pause icon
	case ICON_PAUSE:
		character = G15_ICON_PAUSE;
		break;

	// Play icon
	case ICON_PLAY:
		character = G15_ICON_PLAY;
		break;

	// Play reverse icon
	case ICON_PLAYR:
		character = G15_ICON_PLAYR;
		break;

	// Fast forward icon
	case ICON_FF:
		character = G15_ICON_FF;
		break;

	// Fast reverse icon
	case ICON_FR:
		character = G15_ICON_FR;
		break;

	// Next track icon
	case ICON_NEXT:
		character = G15_ICON_NEXT;
		break;

	// Previous track icon
	case ICON_PREV:
		character = G15_ICON_PREV;
		break;

	// Record icon
	case ICON_REC:
		character = G15_ICON_REC;
		break;

	// Unsupported icon - let core handle it
	default:
		return -1;
	}

	// Render the mapped character
	g15_chr(drvthis, x, y, character);
	return 0;
}

// Draw a horizontal bar growing to the right
MODULE_EXPORT void g15_hbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	int total_pixels = ((long)2 * len * G15_CELL_WIDTH + 1) * promille / 2000;
	int px1, py1, px2, py2;

	if (!g15_convert_coords(x, y, &px1, &py1)) {
		return;
	}

	px2 = px1 + total_pixels;
	py2 = py1 + G15_CELL_HEIGHT - 2;

	g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
}

// Draw a vertical bar growing upward
MODULE_EXPORT void g15_vbar(Driver *drvthis, int x, int y, int len, int promille, int options)
{
	PrivateData *p = drvthis->private_data;
	int total_pixels = ((long)2 * len * G15_CELL_WIDTH + 1) * promille / 2000;
	int px1, py1, px2, py2;

	if (!g15_convert_coords(x, y, &px1, &py1)) {
		return;
	}

	py1 = py1 + G15_CELL_HEIGHT - total_pixels;
	py2 = py1 + total_pixels - 1;
	px2 = px1 + G15_CELL_WIDTH - 2;

	g15r_pixelBox(&p->canvas, px1, py1, px2, py2, G15_COLOR_BLACK, 1, G15_PIXEL_FILL);
}

// Get key input from the G15 keyboard
MODULE_EXPORT const char *g15_get_key(Driver *drvthis) { return NULL; }

// Control the LCD backlight
MODULE_EXPORT void g15_backlight(Driver *drvthis, int on)
{
	PrivateData *p = drvthis->private_data;

	if (p->backlight_state == on) {
		return;
	}

	p->backlight_state = on;

	if (p->has_rgb_backlight) {
		if (on == BACKLIGHT_ON) {
			g15_set_rgb_backlight(drvthis, p->rgb_red, p->rgb_green, p->rgb_blue);
		} else {
			g15_set_rgb_backlight(drvthis, 0, 0, 0);
		}
	}
}

/**
 * \brief Write value to LED subsystem file
 * \param path LED sysfs file path
 * \param value Value string to write
 * \retval 0 Success
 * \retval -1 Error (open or write failed)
 *
 * \details Opens LED control file, writes value, closes file.
 */
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

/**
 * \brief Set G510 RGB backlight via HID feature reports
 * \param drvthis Driver instance
 * \param red Red component (0-255)
 * \param green Green component (0-255)
 * \param blue Blue component (0-255)
 * \retval 0 Success
 * \retval -1 HID report send failed
 *
 * \details Sends HID feature reports for both RGB zones on G510.
 */
static int g15_set_rgb_hid_reports(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;
	unsigned char rgb_report[G510_RGB_REPORT_SIZE];
	int result = 0;

	rgb_report[0] = G510_FEATURE_RGB_ZONE0;
	rgb_report[1] = (unsigned char)red;
	rgb_report[2] = (unsigned char)green;
	rgb_report[3] = (unsigned char)blue;

	if (lib_hidraw_send_feature_report(p->hidraw_handle, rgb_report, G510_RGB_REPORT_SIZE) <
	    0) {
		report(RPT_ERR, "%s: Failed to set RGB zone 0 via HID reports", drvthis->name);
		result = -1;
	}

	rgb_report[0] = G510_FEATURE_RGB_ZONE1;

	if (lib_hidraw_send_feature_report(p->hidraw_handle, rgb_report, G510_RGB_REPORT_SIZE) <
	    0) {
		report(RPT_ERR, "%s: Failed to set RGB zone 1 via HID reports", drvthis->name);
		result = -1;
	}

	if (result == 0) {
		report(RPT_INFO, "%s: Set RGB backlight via HID reports to (%d,%d,%d)",
		       drvthis->name, red, green, blue);
	}

	return result;
}

/**
 * \brief Set RGB backlight via Linux LED subsystem
 * \param drvthis Driver instance
 * \param red Red component (0-255)
 * \param green Green component (0-255)
 * \param blue Blue component (0-255)
 * \retval 0 Success
 * \retval -1 LED subsystem write failed
 *
 * \details Writes hex color in format \#RRGGBB to LED sysfs files.
 */
static int g15_set_rgb_led_subsystem(Driver *drvthis, int red, int green, int blue)
{
	char color_hex[8];
	int result = 0;

	snprintf(color_hex, sizeof(color_hex), "#%02x%02x%02x", red, green, blue);

	if (write_led_file("/sys/class/leds/g15::kbd_backlight/color", color_hex) < 0) {
		report(RPT_ERR, "%s: Failed to set keyboard backlight color via LED subsystem",
		       drvthis->name);
		result = -1;
	}

	if (write_led_file("/sys/class/leds/g15::power_on_backlight_val/color", color_hex) < 0) {
		report(RPT_ERR, "%s: Failed to set power-on backlight color via LED subsystem",
		       drvthis->name);
		result = -1;
	}

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
		if (write_led_file("/sys/class/leds/g15::kbd_backlight/brightness", "0") < 0) {
			report(RPT_ERR, "%s: Failed to turn off backlight", drvthis->name);
			result = -1;
		}
	}

	if (result == 0) {
		report(RPT_INFO, "%s: Set RGB backlight via LED subsystem to (%d,%d,%d)",
		       drvthis->name, red, green, blue);
	}

	return result;
}

// Set RGB backlight colors
MODULE_EXPORT int g15_set_rgb_backlight(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;
	int result = 0;

	if (!p->has_rgb_backlight) {
		report(RPT_WARNING, "%s: Device does not support RGB backlight", drvthis->name);
		return -1;
	}

	if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
		report(RPT_ERR, "%s: Invalid RGB values (%d,%d,%d)", drvthis->name, red, green,
		       blue);
		return -1;
	}

	p->rgb_red = (unsigned char)red;
	p->rgb_green = (unsigned char)green;
	p->rgb_blue = (unsigned char)blue;

	if (p->rgb_method_hid) {
		result = g15_set_rgb_hid_reports(drvthis, red, green, blue);
	} else {
		result = g15_set_rgb_led_subsystem(drvthis, red, green, blue);
	}

	return result;
}

// Set macro LED status
MODULE_EXPORT int g15_set_macro_leds(Driver *drvthis, int m1, int m2, int m3, int mr)
{
	PrivateData *p = drvthis->private_data;
	unsigned char led_report[G510_MACRO_LED_REPORT_SIZE];
	unsigned char led_mask = 0;

	report(RPT_DEBUG, "%s: g15_set_macro_leds called with m1=%d m2=%d m3=%d mr=%d",
	       drvthis->name, m1, m2, m3, mr);

	if (!p) {
		report(RPT_ERR, "%s: PrivateData is NULL", drvthis->name);
		return -1;
	}

	if (!p->hidraw_handle) {
		report(RPT_ERR, "%s: Device not initialized (hidraw_handle is NULL)",
		       drvthis->name);
		return -1;
	}

	// Build LED bitmask from individual LED states
	if (m1)
		led_mask |= G510_LED_M1;
	if (m2)
		led_mask |= G510_LED_M2;
	if (m3)
		led_mask |= G510_LED_M3;
	if (mr)
		led_mask |= G510_LED_MR;

	p->macro_leds = led_mask;

	// Prepare HID feature report
	led_report[0] = G510_FEATURE_MACRO_LEDS;
	led_report[1] = led_mask;

	report(RPT_DEBUG, "%s: Setting macro LEDs with mask 0x%02x", drvthis->name, led_mask);
	report(RPT_DEBUG, "%s: Sending HID feature report: %02x %02x (size=2)", drvthis->name,
	       led_report[0], led_report[1]);

	if (lib_hidraw_send_feature_report(p->hidraw_handle, led_report, 2) < 0) {
		report(
		    RPT_ERR,
		    "%s: Failed to set macro LEDs - lib_hidraw_send_feature_report returned error",
		    drvthis->name);
		return -1;
	}

	report(RPT_DEBUG, "%s: Macro LEDs set successfully", drvthis->name);

	report(RPT_DEBUG, "%s: Set macro LEDs: M1=%s M2=%s M3=%s MR=%s (mask=0x%02x)",
	       drvthis->name, m1 ? "ON" : "OFF", m2 ? "ON" : "OFF", m3 ? "ON" : "OFF",
	       mr ? "ON" : "OFF", led_mask);

	return 0;
}

// Display a large number on the LCD
MODULE_EXPORT void g15_num(Driver *drvthis, int x, int num)
{
	PrivateData *p = drvthis->private_data;

	// Convert 1-based coordinate to 0-based pixel offset
	x--;
	int ox = x * G15_CELL_WIDTH;

	if ((num < 0) || (num > 10)) {
		return;
	}

	// Determine bitmap dimensions based on character type
	int width = 0;
	int height = 43;

	if ((num >= 0) && (num <= 9)) {
		width = 24;
	} else {
		width = 9;
	}

	int i = 0;

	// Render bitmap pixel by pixel
	for (i = 0; i < (width * height); ++i) {

		// Convert bitmap data to color (1=white, 0=black)
		int color = (g15_bignum_data[num][i] ? G15_COLOR_WHITE : G15_COLOR_BLACK);

		// Calculate pixel position from linear index
		int px = ox + i % width;
		int py = i / width;

		g15r_setPixel(&p->canvas, px, py, color);
	}
}
