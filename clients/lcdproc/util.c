// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/util.c
 * \brief Utility functions for numerical value formatting and widget management
 * \author Peter Marschall
 * \date 2005-2006
 *
 * \features
 * - Memory size formatting with appropriate units (B, kB, MB, GB, etc.)
 * - Percentage value formatting with precision control
 * - Unit conversion with configurable base (1000 vs 1024)
 * - Progress bar widget abstraction
 * - Adaptive precision based on value magnitude
 * - Automatic unit scaling and prefix selection
 *
 * \usage
 * - Called by various lcdproc screens for consistent formatting
 * - Provides standardized numerical display across all screens
 * - Used for memory, percentage, and progress bar displays
 * - Ensures consistent user experience across LCD sizes
 *
 * \details
 * This file implements utility functions for the lcdproc client that
 * provide standardized formatting of numerical values and widget management.
 * It includes functions for memory size formatting, percentage display, unit
 * conversion, and progress bar widget handling.
 */

#include "util.h"
#include "main.h"
#include "shared/sockets.h"
#include <sys/types.h>

// Format memory value with appropriate unit suffix
char *sprintf_memory(char *dst, double value, double roundlimit)
{
	if (dst != NULL) {
		char *format = "%.1f%s";
		char *unit = convert_double(&value, 1024, roundlimit);

		// Adaptive precision: smaller values get more decimal places
		if (value <= 99.994999999)
			format = "%.2f%s";
		if (value <= 9.9994999999)
			format = "%.3f%s";

		snprintf(dst, 12, format, value, unit);
	}
	return dst;
}

// Format percentage value with edge case handling
char *sprintf_percent(char *dst, double percent)
{
	if (dst != NULL) {
		if (percent > 99.9)
			strncpy(dst, "100%", 5);
		else
			snprintf(dst, 12, "%.1f%%", (percent >= 0) ? percent : 0);
	}
	return dst;
}

// Convert numerical value to appropriate unit scale
char *convert_double(double *value, int base, double roundlimit)
{
	static char *units[] = {"", "k", "M", "G", "T", "P", "E", "Z", "Y", NULL};
	int off = 0;

	if ((roundlimit <= 0.0) || (roundlimit > 1.0))
		roundlimit = 0.5;

	// Scale value down until it's in displayable range
	while (units[off] != NULL) {
		if (*value < 1000 * roundlimit)
			break;
		off++;
		*value /= base;
	}
	return units[off];
}

// Add progress bar widget using native pbar
void pbar_widget_add(const char *screen, const char *name)
{
	sock_printf(sock, "widget_add %s %s pbar\n", screen, name);
}

// Configure progress bar widget parameters
void pbar_widget_set(const char *screen, const char *name, int x, int y, int width, int promille,
		     char *begin_label, char *end_label)
{
	// Use native pbar widget with optional labels
	if (begin_label || end_label)
		sock_printf(sock, "widget_set %s %s %d %d %d %d {%s} {%s}\n", screen, name, x, y,
			    width, promille, begin_label ? begin_label : "",
			    end_label ? end_label : "");
	else
		sock_printf(sock, "widget_set %s %s %d %d %d %d\n", screen, name, x, y, width,
			    promille);
}
