// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/load.c
 * \brief System load average monitoring screen for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - Real-time scrolling histogram display
 * - Configurable low/high load thresholds
 * - Automatic scaling based on maximum load
 * - Backlight control based on load levels
 * - Adaptive layout for different LCD sizes
 * - Visual load indicators (vertical bars)
 * - Hostname display integration
 * - Load history tracking and averaging
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically displays system load histogram
 * - Adapts display format based on LCD dimensions
 * - Updates load statistics in real-time
 * - Provides backlight control based on load thresholds
 *
 * \details
 * This file implements the load average monitoring screen that
 * displays system load as a real-time histogram similar to xload. The
 * display shows a scrolling graph of load averages with configurable
 * thresholds for backlight control and visual alerts.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "load.h"
#include "machine.h"
#include "main.h"
#include "mode.h"

#include "shared/configfile.h"
#include "shared/sockets.h"

// Display system load average as a scrolling histogram
int xload_screen(int rep, int display, int *flags_ptr)
{
	static int gauge_hgt = 0;
	static double loads[LCD_MAX_WIDTH];
	static double lowLoad = LOAD_MIN;
	static double highLoad = LOAD_MAX;
	int loadtop;
	int i;
	double loadmax = 0;
	double factor;
	int status = BACKLIGHT_ON;

	// Two-phase initialization to handle race condition with server's "listen" command
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add L\n");
		*flags_ptr |= INITIALIZED;
		return 0;
	}

	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100;

		lowLoad = config_get_float("Load", "LowLoad", 0, LOAD_MIN);
		highLoad = config_get_float("Load", "HighLoad", 0, LOAD_MAX);

		// Calculate histogram height: reserve top row for title if lcd_hgt > 2
		gauge_hgt = (lcd_hgt > 2) ? (lcd_hgt - 1) : lcd_hgt;
		memset(loads, '\0', sizeof(double) * LCD_MAX_WIDTH);

		sock_printf(sock, "screen_set L -name {Load: %s}\n", get_hostname());

		// Create vertical bar for each column (scrolling histogram)
		for (i = 1; i < lcd_wid; i++) {
			sock_printf(sock, "widget_add L bar%i vbar\n", i);
			sock_printf(sock, "widget_set L bar%i %i %i 0\n", i, i, lcd_hgt);
		}

		if (lcd_hgt > 2) {
			sock_send_string(sock, "widget_add L title title\n");
			sock_send_string(sock, "widget_set L title {LOAD        }\n");
		} else {
			sock_send_string(sock, "widget_add L title string\n");
			sock_send_string(sock, "widget_set L title 1 1 {LOAD}\n");
			sock_send_string(sock, "screen_set L -heartbeat off\n");
		}

		// Scale indicators: "0" at bottom, max value at top
		sock_send_string(sock, "widget_add L zero string\n");
		sock_send_string(sock, "widget_add L top string\n");
		sock_printf(sock, "widget_set L zero %i %i 0\n", lcd_wid, lcd_hgt);
		sock_printf(sock, "widget_set L top %i %i 1\n", lcd_wid, (lcd_hgt + 1 - gauge_hgt));
	}

	// Shift histogram left (scrolling effect)
	for (i = 0; i < (lcd_wid - 2); i++)
		loads[i] = loads[i + 1];

	machine_get_loadavg(&(loads[lcd_wid - 2]));

	// Find maximum load for auto-scaling
	for (i = 0; i < lcd_wid - 1; i++)
		loadmax = max(loadmax, loads[i]);

	// Calculate scale ceiling: round up to next integer, minimum 1
	loadtop = (int)loadmax;
	if (loadtop < loadmax)
		loadtop++;
	loadtop = max(1, loadtop);

	// Scaling factor: convert load values to pixel heights
	factor = (double)(lcd_cellhgt * gauge_hgt) / (double)loadtop;

	sock_printf(sock, "widget_set L top %i %i %i\n", lcd_wid, (lcd_hgt + 1 - gauge_hgt),
		    loadtop);

	// Update all histogram bars with scaled values
	for (i = 0; i < lcd_wid - 1; i++) {
		double x = loads[i] * factor;
		sock_printf(sock, "widget_set L bar%i %i %i %i\n", i + 1, i + 1, lcd_hgt, (int)x);
	}

	// Update title with current load value
	// Ensure array bounds: lcd_wid - 2 must be within [0, LCD_MAX_WIDTH)
	int load_index = min(lcd_wid - 2, LCD_MAX_WIDTH - 1);
	load_index = max(0, load_index);

	if (lcd_hgt > 2)
		sock_printf(sock, "widget_set L title {LOAD %2.2f:%s}\n", loads[load_index],
			    get_hostname());
	else
		sock_printf(sock, "widget_set L title 1 1 {%s %2.2f}\n", get_hostname(),
			    loads[load_index]);

	// Backlight control based on load thresholds
	if (lowLoad < highLoad) {
		status = (loadmax > lowLoad) ? BACKLIGHT_ON : BACKLIGHT_OFF;
		if (loads[load_index] > highLoad)
			status = BLINK_ON;
	}

	return status;
}
