// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/batt.c
 * \brief Battery status display screen for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - AC power status monitoring (On/Off/Backup/Unknown)
 * - Battery status tracking (High/Low/Critical/Charging/Absent/Unknown)
 * - Percentage-based charge level display
 * - Horizontal bar graph for visual charge indication
 * - Adaptive layout for 2-line and 4-line LCD displays
 * - Hostname integration in screen title
 * - APM interface integration
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically displays battery information when available
 * - Adapts display format based on LCD dimensions
 * - Updates battery status in real-time
 *
 * \details This file implements the battery status screen that displays
 * APM (Advanced Power Management) battery information including AC power
 * status, battery charge level, and charging state. The screen adapts to
 * different LCD sizes and provides both numeric and graphical battery
 * level indicators.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "batt.h"
#include "machine.h"
#include "main.h"
#include "mode.h"

#include "shared/sockets.h"

/**
 * \brief Status code to name mapping table entry
 * \details Maps numeric status codes to human-readable text strings
 */
typedef struct {
	int status;	  ///< Numeric status code
	const char *name; ///< Human-readable name
} NameTable;

/**
 * \brief Ac status
 * \param status int status
 * \return Return value
 */
static const char *ac_status(int status)
{
	const NameTable ac_table[] = {{LCDP_AC_OFF, "Off"},
				      {LCDP_AC_ON, "On"},
				      {LCDP_AC_BACKUP, "Backup"},
				      {LCDP_AC_UNKNOWN, "Unknown"},
				      {0, NULL}};
	int i;

	for (i = 0; ac_table[i].name != NULL; i++)
		if (status == ac_table[i].status)
			return ac_table[i].name;

	return ac_table[LCDP_AC_UNKNOWN].name;
}

/**
 * \brief Convert battery status code to human-readable string
 * \param status Battery status code (LCDP_BATT_* constants)
 * \return String representation of battery status
 *
 * \details Maps battery status codes to descriptive strings like "High",
 * "Low", "Critical", "Charging", "Absent", or "Unknown".
 */
static const char *battery_status(int status)
{
	const NameTable batt_table[] = {{LCDP_BATT_HIGH, "High"},
					{LCDP_BATT_LOW, "Low"},
					{LCDP_BATT_CRITICAL, "Critical"},
					{LCDP_BATT_CHARGING, "Charging"},
					{LCDP_BATT_ABSENT, "Absent"},
					{LCDP_BATT_UNKNOWN, "Unknown"},
					{0, NULL}};
	int i;

	for (i = 0; batt_table[i].name != NULL; i++)
		if (status == batt_table[i].status)
			return batt_table[i].name;

	return batt_table[LCDP_AC_UNKNOWN].name;
}

// Display battery status screen with APM information
int battery_screen(int rep, int display, int *flags_ptr)
{
	int acstat = 0, battstat = 0, percent = 0;
	int gauge_wid = lcd_wid - 2;

	// Two-phase initialization to handle race condition with server's "listen" command
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add B\n");
		*flags_ptr |= INITIALIZED;
		return 0;
	}

	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100;

		sock_printf(sock, "screen_set B -name {APM stats:%s}\n", get_hostname());
		sock_send_string(sock, "widget_add B title title\n");
		sock_printf(sock, "widget_set B title {LCDPROC %s}\n", version);
		sock_send_string(sock, "widget_add B one string\n");

		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add B two string\n");
			sock_send_string(sock, "widget_add B three string\n");
			sock_send_string(sock, "widget_add B gauge hbar\n");

			sock_send_string(sock, "widget_set B one 1 2 {AC: Unknown}\n");
			sock_send_string(sock, "widget_set B two 1 3 {Batt: Unknown}\n");
			sock_printf(sock, "widget_set B three 1 4 {E%*sF}\n", gauge_wid, "");
			sock_send_string(sock, "widget_set B gauge 2 4 0\n");
		}
	}

	machine_get_battstat(&acstat, &battstat, &percent);

	// Update battery display with formatted percentage, AC/battery status, and gauge widget
	if (display) {
		char tmp[20];

		if (percent >= 0)
			snprintf(tmp, sizeof(tmp), "%d%%", percent);
		else
			snprintf(tmp, sizeof(tmp), "??%%");

		sock_printf(sock, "widget_set B title {%s: %s:%s}\n",
			    (acstat == LCDP_AC_ON && battstat == LCDP_BATT_ABSENT) ? "AC" : "Batt",
			    tmp, get_hostname());

		if (lcd_hgt >= 4) {
			sock_printf(sock, "widget_set B one 1 2 {AC: %s}\n", ac_status(acstat));
			sock_printf(sock, "widget_set B two 1 3 {Batt: %s}\n",
				    battery_status(battstat));

			if (percent > 0)
				sock_printf(sock, "widget_set B gauge 2 4 %d\n",
					    (percent * gauge_wid * lcd_cellwid) / 100);

		} else {
			sock_printf(sock, "widget_set B one 1 2 {%sBatt: %s}\n",
				    (acstat == LCDP_AC_ON) ? "AC, " : "", battery_status(battstat));
		}
	}

	return 0;
}
