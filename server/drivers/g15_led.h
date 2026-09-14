// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/g15_led.h
 * \brief sysfs helpers for the G510 RGB backlight LED class devices
 * \author n0vedad
 * \date 2026
 *
 *
 * \features
 * - Writes values to LED sysfs attributes with flush-time error detection
 * - Sets RGB colors via the multicolor LED API (Linux 6.15+)
 * - Falls back to the legacy "color" attribute on older kernels
 *
 * \usage
 * - Included by the g15 driver (g15.c)
 * - Included by the unit tests, which exercise these helpers against temporary
 *   directories instead of real sysfs files
 *
 * \details Header-only so that the unit tests test the same code the driver
 * runs, without linking the driver itself.
 */

#ifndef G15_LED_H
#define G15_LED_H

#include <stdio.h>

/**
 * \brief Write value to LED subsystem file
 * \param path LED sysfs file path
 * \param value Value string to write
 * \retval 0 Success
 * \retval -1 Error (open, write or flush failed)
 *
 * \details Opens LED control file, writes value, closes file. sysfs reports
 * rejected values only when the buffered write is flushed, so the fclose()
 * result is part of the success check.
 */
static inline int write_led_file(const char *path, const char *value)
{
	FILE *f = fopen(path, "w");
	if (f == NULL) {
		return -1;
	}

	int result = fprintf(f, "%s", value);

	if (fclose(f) != 0) {
		return -1;
	}

	return (result > 0) ? 0 : -1;
}

/**
 * \brief Write RGB color to a G510 LED class device
 * \param led_dir LED sysfs directory, e.g. /sys/class/leds/g15::kbd_backlight
 * \param red Red component (0-255)
 * \param green Green component (0-255)
 * \param blue Blue component (0-255)
 * \retval 0 Success
 * \retval -1 Neither color interface accepted the value
 *
 * \details Since Linux 6.15 (commit a3a064146c50 "HID: hid-lg-g15: Use standard
 * multicolor LED API") hid-lg-g15 exposes the color as multi_intensity
 * ("R G B", order given by multi_index) and no longer has the undocumented
 * "color" attribute ("#RRGGBB"). Older kernels only have "color".
 */
static inline int write_led_color(const char *led_dir, int red, int green, int blue)
{
	char path[128];
	char value[32];

	snprintf(path, sizeof(path), "%s/multi_intensity", led_dir);
	snprintf(value, sizeof(value), "%d %d %d", red, green, blue);

	if (write_led_file(path, value) == 0) {
		return 0;
	}

	snprintf(path, sizeof(path), "%s/color", led_dir);
	snprintf(value, sizeof(value), "#%02x%02x%02x", red, green, blue);

	return write_led_file(path, value);
}

#endif
