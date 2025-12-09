// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/iface.c
 * \brief Network interface monitoring screen for lcdproc client
 * \author Luis Llorente Campo <luisllorente@luisllorente.com>
 * \author Stephan Skrodzki <skrodzki@stevekist.de>
 * \author Andrew Foss, M. Dolze, Peter Marschall
 * \date 2002-2006
 *
 * \features
 * - Multi-interface support with scrolling display
 * - Configurable units (bytes, bits, packets)
 * - Upload/Download speed monitoring
 * - Total transfer statistics
 * - Interface status tracking (up/down)
 * - Adaptive layout for different LCD sizes
 * - Configuration-based interface aliases
 * - Time-based activity tracking
 * - Single and multi-interface display modes
 * - Real-time throughput calculations
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically displays network interface statistics
 * - Adapts display format based on LCD dimensions
 * - Updates network statistics in real-time
 * - Supports configurable interface filtering
 *
 * \details
 * This file implements network interface monitoring functionality
 * for the lcdproc client. Originally imported from netlcdclient, it shows
 * network statistics including throughput, transfer totals, and interface
 * status for one or multiple network interfaces.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "shared/configfile.h"
#include "shared/report.h"
#include "shared/sockets.h"

#include "iface.h"
#include "machine.h"
#include "main.h"
#include "util.h"

/** \brief Sentinel value for unset integer configuration options */
#define UNSET_INT (-1)

/** \brief Sentinel value for unset string configuration options */
#define UNSET_STR "\01"

/** \name Network Interface State Variables
 * Global state for network interface monitoring
 */
///@{
IfaceInfo iface[MAX_INTERFACES];  ///< Array of monitored network interfaces
static int iface_count = 0;	  ///< Number of active interfaces being monitored
static char unit_label[10] = "B"; ///< Current unit label (B=bytes, b=bits, pkt=packets)
static int transfer_screen = 0;	  ///< Current screen in multi-screen transfer display mode
///@}

/**
 * \brief Format network value with appropriate unit prefix
 * \param buff Output buffer for formatted value
 * \param bufsize Size of output buffer
 * \param value Network value (bytes, packets, etc.)
 * \param unit Base unit string ("B/s", "b/s", "pkt/s")
 * \param compact Use compact format flag (4-char vs 8-char width)
 *
 * \details Automatically scales value with K/M/G prefix. Uses binary (1024)
 * scaling for bytes, decimal (1000) for bits/packets. Converts bytes to bits
 * if unit contains 'b'.
 */
static void format_net_value(char *buff, size_t bufsize, double value, const char *unit,
			     int compact)
{
	char *mag;

	// Convert bytes to bits if measuring in bits
	if (strstr(unit, "b"))
		value *= 8;

	// Scale: binary (1024) for bytes, decimal (1000) for bits/packets
	mag = convert_double(&value, (strstr(unit, "B")) ? 1024 : 1000, 1.0f);

	if (compact) {
		// Compact format for multi-interface mode
		if (mag[0] == 0)
			snprintf(buff, bufsize, "%4ld", (long)value);
		else if (value < 10)
			snprintf(buff, bufsize, "%3.1f%s", value, mag);
		else
			snprintf(buff, bufsize, "%3.0f%s", value, mag);
	} else {
		// Full format for single-interface mode
		if (mag[0] == 0)
			snprintf(buff, bufsize, "%8ld %s", (long)value, unit);
		else
			snprintf(buff, bufsize, "%7.3f %s%s", value, mag, unit);
	}
}

/**
 * \brief Calculate network transfer speed per second
 * \param new_val Current counter value
 * \param old_val Previous counter value
 * \param interval Time interval in seconds
 * \return Transfer speed per second
 *
 * \details Calculates rate of change: (new - old) / time_interval
 */
static double calculate_speed(double new_val, double old_val, unsigned int interval)
{
	return (new_val - old_val) / interval;
}

/**
 * \brief Display interface offline status with last-online time
 * \param screen Screen name
 * \param widget Widget name
 * \param line Line number for display
 * \param last_online Timestamp when interface was last online
 *
 * \details Formats and displays "NA (HH:MM:SS)" to show interface is offline
 * with time since last online state.
 */
static void display_offline_status(const char *screen, const char *widget, int line,
				   time_t last_online)
{
	char timebuff[30];

	get_time_string(timebuff, last_online);
	sock_printf(sock, "widget_set %s %s 1 %d {NA (%s)}\n", screen, widget, line, timebuff);
}

/**
 * \brief Iface process configfile
 * \return Return value
 */
static int iface_process_configfile(void)
{
	const char *unit;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	// Load interface configurations: Interface0, Interface1, Interface2
	for (iface_count = 0; iface_count < MAX_INTERFACES; iface_count++) {
		char iface_label[12];

		snprintf(iface_label, sizeof(iface_label), "Interface%i", iface_count);
		debug(RPT_DEBUG, "Label %s count %i", iface_label, iface_count);
		iface[iface_count].name = strdup(config_get_string("Iface", iface_label, 0, ""));

		if (iface[iface_count].name == NULL) {
			report(RPT_CRIT, "malloc failure");
			return -1;
		}

		if (*iface[iface_count].name == '\0')
			break;

		// Load interface alias (defaults to interface name)
		snprintf(iface_label, sizeof(iface_label), "Alias%i", iface_count);
		iface[iface_count].alias =
		    strdup(config_get_string("Iface", iface_label, 0, iface[iface_count].name));

		if (iface[iface_count].alias == NULL)
			iface[iface_count].alias = iface[iface_count].name;

		debug(RPT_DEBUG, "Interface %i: %s alias %s", iface_count, iface[iface_count].name,
		      iface[iface_count].alias);
	}

	// Parse unit configuration: byte/bytes -> "B", bit/bits -> "b", packet/packets -> "pkt"
	unit = config_get_string("Iface", "Unit", 0, "byte");
	if ((strcasecmp(unit, "byte") == 0) || (strcasecmp(unit, "bytes") == 0))
		strncpy(unit_label, "B", sizeof(unit_label));
	else if ((strcasecmp(unit, "bit") == 0) || (strcasecmp(unit, "bits") == 0))
		strncpy(unit_label, "b", sizeof(unit_label));
	else if ((strcasecmp(unit, "packet") == 0) || (strcasecmp(unit, "packets") == 0))
		strncpy(unit_label, "pkt", sizeof(unit_label));
	else {
		report(RPT_ERR, "illegal Unit value: %s", unit);
		return -1;
	}

	unit_label[sizeof(unit_label) - 1] = '\0';

	transfer_screen = config_get_bool("Iface", "Transfer", 0, 0);

	return 0;
}

// Display network interface monitoring screen
int iface_screen(int rep, int display, int *flags_ptr)
{
	unsigned int interval = difftime(time(NULL), iface[0].last_online);
	int iface_nmbr;

	if (!interval)
		return 0;

	// One-time screen initialization on first call
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		iface_process_configfile();
		initialize_speed_screen();

		if (transfer_screen) {
			initialize_transfer_screen();
		}

		for (iface_nmbr = 0; iface_nmbr < iface_count; iface_nmbr++) {
			iface[iface_nmbr].last_online = 0;
			iface[iface_nmbr].status = down;
		}
		return 0;
	}

	// Update each configured interface
	for (iface_nmbr = 0; iface_nmbr < iface_count; iface_nmbr++) {
		if (!machine_get_iface_stats(&iface[iface_nmbr])) {
			break;
		}

		actualize_speed_screen(&iface[iface_nmbr], interval, iface_nmbr);

		if (transfer_screen)
			actualize_transfer_screen(&iface[iface_nmbr], iface_nmbr);

		// Store current values for next speed calculation
		iface[iface_nmbr].rc_byte_old = iface[iface_nmbr].rc_byte;
		iface[iface_nmbr].tr_byte_old = iface[iface_nmbr].tr_byte;
		iface[iface_nmbr].rc_pkt_old = iface[iface_nmbr].rc_pkt;
		iface[iface_nmbr].tr_pkt_old = iface[iface_nmbr].tr_pkt;
	}

	return 0;
}

/**
 * \brief Initialize network monitoring screen with widgets
 * \param screen_id Screen identifier (e.g., "I" or "N")
 * \param screen_name Full screen name for display
 * \param title_prefix Prefix for screen title
 * \param show_unit_in_title Flag to include unit in title
 *
 * \details Creates screen and widgets for network interface monitoring.
 * Supports both single-interface and multi-interface layouts.
 */
static void initialize_net_screen(const char *screen_id, const char *screen_name,
				  const char *title_prefix, int show_unit_in_title)
{
	int iface_nmbr;

	sock_printf(sock, "screen_add %s\n", screen_id);
	sock_printf(sock, "screen_set %s name {%s}\n", screen_id, screen_name);
	sock_printf(sock, "widget_add %s title title\n", screen_id);

	// Single interface layout: show DL/UL/Total on separate lines
	if ((iface_count == 1) && (lcd_hgt >= 4)) {
		sock_printf(sock, "widget_set %s title {%s: %s}\n", screen_id, title_prefix,
			    iface[0].alias);
		sock_printf(sock, "widget_add %s dl string\n", screen_id);
		sock_printf(sock, "widget_set %s dl 1 2 {DL:}\n", screen_id);
		sock_printf(sock, "widget_add %s ul string\n", screen_id);
		sock_printf(sock, "widget_set %s ul 1 3 {UL:}\n", screen_id);
		sock_printf(sock, "widget_add %s total string\n", screen_id);
		sock_printf(sock, "widget_set %s total 1 4 {Total:}\n", screen_id);
	}

	// Multi-interface layout: scrollable list with compact format
	else {
		if (show_unit_in_title) {
			if (strstr(unit_label, "B"))
				sock_printf(sock, "widget_set %s title {%s (bytes)}\n", screen_id,
					    title_prefix);
			else if (strstr(unit_label, "b"))
				sock_printf(sock, "widget_set %s title {%s (bits)}\n", screen_id,
					    title_prefix);
			else
				sock_printf(sock, "widget_set %s title {%s (packets)}\n", screen_id,
					    title_prefix);
		} else {
			sock_printf(sock, "widget_set %s title {%s (bytes)}\n", screen_id,
				    title_prefix);
		}

		sock_printf(sock, "widget_add %s f frame\n", screen_id);

		// Scroll rate: 1 line every 8 ticks for tall displays, 16 for short
		sock_printf(sock, "widget_set %s f 1 2 %d %d %d %d v 16\n", screen_id, lcd_wid,
			    lcd_hgt, lcd_wid, iface_count, ((lcd_hgt >= 4) ? 8 : 16));

		for (iface_nmbr = 0; iface_nmbr < iface_count; iface_nmbr++) {
			sock_printf(sock, "widget_add %s i%1d string -in f\n", screen_id,
				    iface_nmbr);
			sock_printf(sock, "widget_set %s i%1d 1 %1d {%5.5s NA (never)}\n",
				    screen_id, iface_nmbr, iface_nmbr + 1, iface[iface_nmbr].alias);
		}
	}
}

// Initialize speed monitoring screen with widgets
void initialize_speed_screen(void) { initialize_net_screen("I", "Load", "Net Load", 1); }

// Format time value for display
void get_time_string(char *buff, time_t last_online)
{
	time_t act_time;
	char timebuff[30];
	struct tm tm_result;

	act_time = time(NULL);

	if (last_online == 0) {
		strncpy(buff, "never", 6);
		return;
	}

	// localtime_r() + strftime() is thread-safe (POSIX.1-2008, replaces deprecated ctime)
	if (localtime_r(&last_online, &tm_result) == NULL) {
		strncpy(buff, "error", 6);
		return;
	}

	// Show date if >24 hours ago, otherwise show time
	if ((act_time - last_online) > 86400) {
		strftime(timebuff, sizeof(timebuff), "%b %d", &tm_result);
	} else {
		strftime(timebuff, sizeof(timebuff), "%H:%M:%S", &tm_result);
	}

	strncpy(buff, timebuff, 10);
}

// Update speed monitoring screen with current data
void actualize_speed_screen(IfaceInfo *iface, unsigned int interval, int index)
{
	char speed[20];
	char speed1[20];
	double rc_speed;
	double tr_speed;

	// Calculate speeds based on unit type
	if (strstr(unit_label, "pkt")) {
		rc_speed = calculate_speed(iface->rc_pkt, iface->rc_pkt_old, interval);
		tr_speed = calculate_speed(iface->tr_pkt, iface->tr_pkt_old, interval);
	} else {
		rc_speed = calculate_speed(iface->rc_byte, iface->rc_byte_old, interval);
		tr_speed = calculate_speed(iface->tr_byte, iface->tr_byte_old, interval);
	}

	// Single interface mode: show DL/UL/Total on separate lines
	if ((iface_count == 1) && (lcd_hgt >= 4)) {
		if (iface->status == up) {
			format_net_value(speed, sizeof(speed), rc_speed, unit_label, 0);
			sock_printf(sock, "widget_set I dl 1 2 {DL: %*s/s}\n", lcd_wid - 6, speed);

			format_net_value(speed, sizeof(speed), tr_speed, unit_label, 0);
			sock_printf(sock, "widget_set I ul 1 3 {UL: %*s/s}\n", lcd_wid - 6, speed);

			format_net_value(speed, sizeof(speed), rc_speed + tr_speed, unit_label, 0);
			sock_printf(sock, "widget_set I total 1 4 {Total: %*s/s}\n", lcd_wid - 9,
				    speed);
		} else {
			display_offline_status("I", "dl", 2, iface->last_online);
			sock_send_string(sock, "widget_set I ul 1 3 {}\n");
			sock_send_string(sock, "widget_set I total 1 4 {}\n");
		}
	}

	// Multi-interface mode: compact format with all interfaces in scrolling list
	else {
		if (iface->status == up) {
			format_net_value(speed, sizeof(speed), rc_speed, unit_label, 1);
			format_net_value(speed1, sizeof(speed1), tr_speed, unit_label, 1);

			// Wide LCD: "Name U:upload D:download", Narrow LCD: "Name ^up vdown"
			if (lcd_wid > 16)
				sock_printf(sock, "widget_set I i%1d 1 %1d {%5.5s U:%.4s D:%.4s}\n",
					    index, index + 1, iface->alias, speed1, speed);
			else
				sock_printf(sock, "widget_set I i%1d 1 %1d {%4.4s ^%.4s v%.4s}\n",
					    index, index + 1, iface->alias, speed1, speed);
		} else {
			get_time_string(speed, iface->last_online);
			sock_printf(sock, "widget_set I i%1d 1 %1d {%5.5s NA (%s)}\n", index,
				    index + 1, iface->alias, speed);
		}
	}
}

// Initialize transfer statistics screen with widgets
void initialize_transfer_screen(void) { initialize_net_screen("NT", "Transfer", "Transfer", 0); }

// Update transfer statistics screen with current data
void actualize_transfer_screen(IfaceInfo *iface, int index)
{
	char transfer[20];
	char transfer1[20];

	// Single interface mode: show DL/UL/Total on separate lines
	if ((iface_count == 1) && (lcd_hgt >= 4)) {
		if (iface->status == up) {
			format_net_value(transfer, sizeof(transfer), iface->rc_byte, "B", 0);
			sock_printf(sock, "widget_set NT dl 1 2 {DL: %*s}\n", lcd_wid - 4,
				    transfer);

			format_net_value(transfer, sizeof(transfer), iface->tr_byte, "B", 0);
			sock_printf(sock, "widget_set NT ul 1 3 {UL: %*s}\n", lcd_wid - 4,
				    transfer);

			format_net_value(transfer, sizeof(transfer),
					 iface->rc_byte + iface->tr_byte, "B", 0);
			sock_printf(sock, "widget_set NT total 1 4 {Total: %*s}\n", lcd_wid - 7,
				    transfer);
		} else {
			display_offline_status("NT", "dl", 2, iface->last_online);
			sock_send_string(sock, "widget_set NT ul 1 3 {}\n");
			sock_send_string(sock, "widget_set NT total 1 4 {}\n");
		}
	}

	// Multi-interface mode: compact format with all interfaces in scrolling list
	else {
		if (iface->status == up) {
			format_net_value(transfer, sizeof(transfer), iface->rc_byte, "B", 1);
			format_net_value(transfer1, sizeof(transfer1), iface->tr_byte, "B", 1);

			// Wide LCD: "Name U:upload D:download", Narrow LCD: "Name ^up vdown"
			if (lcd_wid > 16)
				sock_printf(sock,
					    "widget_set NT i%1d 1 %1d {%5.5s U:%.4s D:%.4s}\n",
					    index, index + 1, iface->alias, transfer1, transfer);
			else
				sock_printf(sock, "widget_set NT i%1d 1 %1d {%4.4s ^%.4s v%.4s}\n",
					    index, index + 1, iface->alias, transfer1, transfer);
		} else {
			get_time_string(transfer, iface->last_online);
			sock_printf(sock, "widget_set NT i%1d 1 %1d {%5.5s NA (%s)}\n", index,
				    index + 1, iface->alias, transfer);
		}
	}
}
