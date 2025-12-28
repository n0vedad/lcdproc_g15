// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/chrono.c
 * \brief Time and date display screens for lcdproc client
 * \author William Ferrell, Selene Scriven, David Glaude
 * \date 1999-2006
 *
 * \features
 * - **TimeDate Screen**: Current time, date, uptime, and system load
 * - **OldTime Screen**: Classic time and date display with hostname
 * - **Uptime Screen**: System uptime and OS version information
 * - **BigClock Screen**: Large digital clock display using numeric widgets
 * - **MiniClock Screen**: Minimal centered time display
 * - Configurable time and date formats via config file
 * - Adaptive layouts for different LCD sizes (2-line vs 4-line)
 * - Heartbeat animation with blinking colons
 * - System information integration (hostname, OS version, uptime)
 * - Load average and idle time display
 * - Optional title bar display control
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Configure time formats in lcdproc configuration file
 * - Automatically adapts to LCD dimensions
 * - Provides multiple time display styles for user preference
 * - Updates time information in real-time
 *
 * \details This file implements multiple time and date display screens
 * for the lcdproc client, each with different layouts and information.
 * The screens adapt to different LCD sizes and provide various formatting
 * options for displaying current time, date, system uptime, and related
 * system information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include "shared/configfile.h"
#include "shared/report.h"
#include "shared/sockets.h"

#include "chrono.h"
#include "machine.h"
#include "main.h"
#include "mode.h"

// Toggle colons in time strings for heartbeat animation
static char *tickTime(char *time, int heartbeat);

/**
 * \brief Calculate X position for centered text
 * \param text Text string to center
 * \return X coordinate (1-based) for left edge of centered text
 *
 * \details Calculates horizontal position to center text on LCD display.
 * Returns 1 if text is too long to center.
 */
static int calculate_centered_xpos(const char *text)
{
	int len = strlen(text);
	return (lcd_wid > len) ? ((lcd_wid - len) / 2) + 1 : 1;
}

// Format current time according to specified format string
void get_formatted_time(char *buffer, size_t bufsize, const char *format)
{
	time_t thetime;
	struct tm rtime;

	time(&thetime);
	localtime_r(&thetime, &rtime);

	if (format == NULL || strftime(buffer, bufsize, format, &rtime) == 0) {
		*buffer = '\0';
	}
}

/**
 * \brief Format system uptime as human-readable string
 * \param buffer Output buffer for formatted uptime
 * \param bufsize Size of output buffer
 * \param uptime System uptime in seconds
 * \param compact Use compact format flag
 *
 * \details Formats uptime as "Xd HH:MM:SS" (compact) or "X day(s) HH:MM:SS" (full).
 * Compact format used automatically for displays < 20 characters wide.
 */
static void format_uptime_string(char *buffer, size_t bufsize, double uptime, int compact)
{
	int days = (int)uptime / 86400;
	int hour = ((int)uptime % 86400) / 3600;
	int min = ((int)uptime % 3600) / 60;
	int sec = ((int)uptime % 60);

	if (compact || lcd_wid < 20)
		snprintf(buffer, bufsize, "%dd %02d:%02d:%02d", days, hour, min, sec);
	else
		snprintf(buffer, bufsize, "%d day%s %02d:%02d:%02d", days, (days != 1) ? "s" : "",
			 hour, min, sec);
}

/**
 * \brief Send widget_set command with centered text
 * \param sock Socket file descriptor for server communication
 * \param screen Screen name
 * \param widget Widget name
 * \param line Line number (1-based)
 * \param text Text content to display centered
 *
 * \details Calculates center position and sends widget_set command to LCDd server.
 */
static void send_widget_centered(int sock, const char *screen, const char *widget, int line,
				 const char *text)
{
	int xoffs = calculate_centered_xpos(text);
	sock_printf(sock, "widget_set %s %s %d %d {%s}\n", screen, widget, xoffs, line, text);
}

// Display comprehensive time, date, and system information screen
int time_screen(int rep, int display, int *flags_ptr)
{
	char now[40];
	char today[40];
	char tmp[40];
	static int heartbeat = 0;
	static const char *timeFormat = NULL;
	static const char *dateFormat = NULL;
	double uptime, idle;
	load_type load;
	int current_idle;

	// Two-phase initialization to handle race condition with server's "listen" command:
	// Phase 1: Send screen_add and return immediately (don't block main_loop)
	// Phase 2: After server processes screen_add and sends "listen", create widgets
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add T\n");
		*flags_ptr |= INITIALIZED;

		// CRITICAL: Return immediately to allow main_loop to read the "listen T"
		// response from the server. Widget creation happens on the NEXT call.
		return 0;
	}

	// Phase 2: Create widgets (only executes after INITIALIZED is set and we return)
	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100; // Mark widgets as created

		timeFormat = config_get_string("TimeDate", "TimeFormat", 0, "%H:%M:%S");
		dateFormat = config_get_string("TimeDate", "DateFormat", 0, "%b %d %Y");
		sock_printf(sock, "screen_set T -name {Time Screen: %s}\n", get_hostname());
		sock_send_string(sock, "widget_add T title title\n");
		sock_send_string(sock, "widget_add T one string\n");

		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add T two string\n");
			sock_send_string(sock, "widget_add T three string\n");

			sock_printf(sock, "widget_set T title {%s %s:%s}\n", get_sysname(),
				    get_sysrelease(), get_hostname());

		} else {
			sock_printf(sock, "widget_set T title {TIME:%s}\n", get_hostname());
		}
	}

	heartbeat ^= 1;

	get_formatted_time(today, sizeof(today), dateFormat);
	get_formatted_time(now, sizeof(now), timeFormat);
	tickTime(now, heartbeat);

	// Layout for 4-line displays: comprehensive information
	if (lcd_hgt >= 4) {
		char uptime_buf[40];

		machine_get_uptime(&uptime, &idle);
		machine_get_load(&load);

		format_uptime_string(uptime_buf, sizeof(uptime_buf), uptime, 0);
		snprintf(tmp, sizeof(tmp), "Up %s", uptime_buf);
		if (display)
			send_widget_centered(sock, "T", "one", 2, tmp);

		if (display)
			send_widget_centered(sock, "T", "two", 3, today);

		// Calculate idle percentage from load statistics (avoid division by zero)
		current_idle =
		    (load.total > 0) ? (int)(100.0 * ((double)load.idle / (double)load.total)) : 0;

		snprintf(tmp, sizeof(tmp), "%s %3i%% idle", now, current_idle);

		if (display)
			send_widget_centered(sock, "T", "three", 4, tmp);

	} else {
		snprintf(tmp, sizeof(tmp), "%s %s", today, now);
		if (display)
			send_widget_centered(sock, "T", "one", 2, tmp);
	}

	return 0;
}

// Display classic time and date screen with optional title
int clock_screen(int rep, int display, int *flags_ptr)
{
	char now[40];
	char today[40];
	char tmp[257];
	static int heartbeat = 0;
	static int showTitle = 1;
	static const char *timeFormat = NULL;
	static const char *dateFormat = NULL;

	// One-time screen initialization: create widgets based on display size and title preference
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		timeFormat = config_get_string("OldTime", "TimeFormat", 0, "%H:%M:%S");
		dateFormat = config_get_string("OldTime", "DateFormat", 0, "%b %d %Y");
		showTitle = config_get_bool("OldTime", "ShowTitle", 0, 1);

		sock_send_string(sock, "screen_add O\n");
		sock_printf(sock, "screen_set O -name {Old Clock Screen: %s}\n", get_hostname());

		if (!showTitle)
			sock_send_string(sock, "screen_set O -heartbeat off\n");
		sock_send_string(sock, "widget_add O one string\n");

		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add O title title\n");
			sock_send_string(sock, "widget_add O two string\n");
			sock_send_string(sock, "widget_add O three string\n");

			sock_printf(sock, "widget_set O title {DATE & TIME}\n");

			send_widget_centered(sock, "O", "one", 2, get_hostname());

		} else {
			if (showTitle) {
				sock_send_string(sock, "widget_add O title title\n");
				sock_printf(sock, "widget_set O title {TIME: %s}\n",
					    get_hostname());
			} else {
				sock_send_string(sock, "widget_add O two string\n");
			}
		}
	}

	heartbeat ^= 1;

	get_formatted_time(today, sizeof(today), dateFormat);
	get_formatted_time(now, sizeof(now), timeFormat);
	tickTime(now, heartbeat);

	// Layout for 4-line displays: separate lines for date and time
	if (lcd_hgt >= 4) {
		if (display) {
			send_widget_centered(sock, "O", "two", 3, today);
			send_widget_centered(sock, "O", "three", 4, now);
		}

	} else {
		if (showTitle) {
			snprintf(tmp, sizeof(tmp), "%s %s", today, now);
			if (display)
				send_widget_centered(sock, "O", "one", 2, tmp);

		} else {
			// No title: date and time on separate lines (full screen usage)
			if (display) {
				send_widget_centered(sock, "O", "one", 1, today);
				send_widget_centered(sock, "O", "two", 2, now);
			}
		}
	}

	return 0;
}

// Display system uptime and OS version information screen
int uptime_screen(int rep, int display, int *flags_ptr)
{
	double uptime, idle;
	char tmp[257];

	// One-time screen initialization: create widgets for uptime display
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		sock_send_string(sock, "screen_add U\n");
		sock_printf(sock, "screen_set U -name {Uptime Screen: %s}\n", get_hostname());
		sock_send_string(sock, "widget_add U title title\n");

		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add U one string\n");
			sock_send_string(sock, "widget_add U two string\n");
			sock_send_string(sock, "widget_add U three string\n");

			sock_send_string(sock, "widget_set U title {SYSTEM UPTIME}\n");

			send_widget_centered(sock, "U", "one", 2, get_hostname());

			snprintf(tmp, sizeof(tmp), "%s %s", get_sysname(), get_sysrelease());
			send_widget_centered(sock, "U", "three", 4, tmp);

		} else {
			sock_send_string(sock, "widget_add U one string\n");

			sock_printf(sock, "widget_set U title {%s %s: %s}\n", get_sysname(),
				    get_sysrelease(), get_hostname());
		}
	}

	machine_get_uptime(&uptime, &idle);
	format_uptime_string(tmp, sizeof(tmp), uptime, 0);

	if (display) {
		if (lcd_hgt >= 4)
			send_widget_centered(sock, "U", "two", 3, tmp);
		else
			send_widget_centered(sock, "U", "one", 2, tmp);
	}

	return 0;
}

// Display large digital clock using numeric widgets
int big_clock_screen(int rep, int display, int *flags_ptr)
{
	time_t thetime;
	struct tm rtime_buf;
	struct tm *rtime = &rtime_buf;

	// X-positions for each digit: HH:MM:SS at columns 1,4,8,11,15,18
	int pos[] = {1, 4, 8, 11, 15, 18};
	char fulltxt[16];
	static char old_fulltxt[16];
	static int heartbeat = 0;
	static int TwentyFourHour = 1;
	int j = 0;

	// Show 6 digits (HH:MM:SS) on wide displays, 4 digits (HH:MM) on narrow ones
	int digits = (lcd_wid >= 20) ? 6 : 4;
	int xoffs = 0;

	int showSecs = config_get_bool("BigClock", "showSecs", 0, 1);
	if (!showSecs) {
		digits = 4;
	}

	// Calculate offset to center the clock: (display_width - clock_width) / 2
	xoffs = (lcd_wid + 1 - (pos[digits - 1] + 2)) / 2;

	heartbeat ^= 1;

	// One-time screen initialization: create numeric digit widgets for big clock display
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		sock_send_string(sock, "screen_add K\n");
		sock_send_string(sock, "screen_set K -name {Big Clock Screen} -heartbeat off\n");
		sock_send_string(sock, "widget_add K d0 num\n");
		sock_send_string(sock, "widget_add K d1 num\n");
		sock_send_string(sock, "widget_add K d2 num\n");
		sock_send_string(sock, "widget_add K d3 num\n");
		sock_send_string(sock, "widget_add K c0 num\n");

		if (digits > 4) {
			sock_send_string(sock, "widget_add K d4 num\n");
			sock_send_string(sock, "widget_add K d5 num\n");
			sock_send_string(sock, "widget_add K c1 num\n");
		}

		strncpy(old_fulltxt, "      ", sizeof(old_fulltxt) - 1);
		old_fulltxt[sizeof(old_fulltxt) - 1] = '\0';
	}

	time(&thetime);
	localtime_r(&thetime, &rtime_buf);

	// Format time digits: convert 24h to 12h if needed using modulo arithmetic
	snprintf(fulltxt, sizeof(fulltxt), "%02d%02d%02d",
		 ((TwentyFourHour) ? rtime->tm_hour : (((rtime->tm_hour + 11) % 12) + 1)),
		 rtime->tm_min, rtime->tm_sec);

	// Optimization: only update digits that have changed to reduce display refreshes
	for (j = 0; j < digits; j++) {
		if (fulltxt[j] != old_fulltxt[j]) {
			sock_printf(sock, "widget_set K d%d %d %c\n", j, xoffs + pos[j],
				    fulltxt[j]);
			old_fulltxt[j] = fulltxt[j];
		}
	}

	// Animate colons: 10=visible colon, 11=hidden (creates blinking effect)
	if (heartbeat) {
		sock_printf(sock, "widget_set K c0 %d 10\n", xoffs + 7);
		if (digits > 4)
			sock_printf(sock, "widget_set K c1 %d 10\n", xoffs + 14);
	} else {
		sock_printf(sock, "widget_set K c0 %d 11\n", xoffs + 7);
		if (digits > 4)
			sock_printf(sock, "widget_set K c1 %d 11\n", xoffs + 14);
	}

	return 0;
}

// Display minimal centered time with configurable format
int mini_clock_screen(int rep, int display, int *flags_ptr)
{
	char now[40];
	static const char *timeFormat = NULL;
	static int heartbeat = 0;
	int xoffs;

	heartbeat ^= 1;

	// One-time screen initialization: create minimal centered time display
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		timeFormat = config_get_string("MiniClock", "TimeFormat", 0, "%H:%M");

		sock_send_string(sock, "screen_add N\n");
		sock_send_string(sock, "screen_set N -name {Mini Clock Screen} -heartbeat off\n");
		sock_send_string(sock, "widget_add N one string\n");
	}

	get_formatted_time(now, sizeof(now), timeFormat);
	tickTime(now, heartbeat);

	xoffs = calculate_centered_xpos(now);
	sock_printf(sock, "widget_set N one %d %d {%s}\n", xoffs, (lcd_hgt / 2), now);

	return 0;
}

/**
 * \brief Toggle colons in time string for heartbeat animation
 * \param time Time string to modify (modified in place)
 * \param heartbeat Heartbeat state (even=show colons, odd=hide colons)
 * \return Pointer to modified time string
 *
 * \details Creates blinking effect by replacing colons with spaces on odd heartbeats.
 * Provides visual feedback that display is updating.
 */
static char *tickTime(char *time, int heartbeat)
{
	if (time != NULL) {
		// Array with colon and space: index 0=':' (visible), index 1=' ' (hidden)
		static char colon[] = {':', ' '};
		char *ptr = time;

		// Replace all colons with either ':' or ' ' based on heartbeat state
		// heartbeat %= 2 ensures we use only 0 or 1 as array index
		for (heartbeat %= 2; *ptr != '\0'; ptr++) {
			if (*ptr == colon[0])
				*ptr = colon[heartbeat];
		}
	}

	return (time);
}
