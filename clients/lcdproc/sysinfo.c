// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/sysinfo.c
 * \brief Comprehensive system information display screen for G510s
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - **Custom System Info Screen**: Optimized 4-line layout for G510s LCD
 * - Hostname display on line 1
 * - System uptime on line 2
 * - Current date and time on line 3
 * - CPU load, RAM usage, and GPU info on line 4
 * - Real-time CPU load percentage calculation
 * - Memory usage in percentage format
 * - GPU temperature and load monitoring
 * - Compact single-screen system overview
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Optimized for 20x4 LCD displays (G510s: 160x43 = 20x5 character cells)
 * - Updates all system metrics in real-time
 * - Enable in lcdproc.conf: Active=true in [SysInfo] section
 *
 * \details Custom system information screen that provides a comprehensive
 * overview of key system metrics on a single 4-line display. Designed
 * specifically for the G510s keyboard's 160x43 monochrome LCD, showing
 * hostname, uptime, date/time, and resource usage at a glance.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "shared/configfile.h"
#include "shared/report.h"
#include "shared/sockets.h"

#include "chrono.h"
#include "machine.h"
#include "main.h"
#include "sysinfo.h"

/**
 * \brief Get GPU temperature (stub for future implementation)
 * \param temp Pointer to store temperature in degrees Celsius
 * \retval 0 Success
 * \retval -1 GPU monitoring not available
 *
 * \details Placeholder for GPU temperature monitoring. Future implementations
 * should read from /sys/class/drm or nvidia-smi for NVIDIA GPUs.
 */
static int get_gpu_temp(int *temp)
{
	// TODO: Implement GPU temperature reading
	// For AMD: /sys/class/drm/card0/device/hwmon/hwmon*/temp1_input
	// For NVIDIA: nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader
	*temp = 0;
	return -1; // Not implemented yet
}

/**
 * \brief Get GPU load percentage
 * \param load Pointer to store load percentage (0-100), or -1 if unavailable
 * \retval 0 Success (GPU monitoring available)
 * \retval -1 GPU monitoring not available
 *
 * \details Attempts to read GPU load from various sources (in order):
 * 1. AMD GPUs: /sys/class/drm/cardN/device/gpu_busy_percent (AMDGPU driver)
 * 2. NVIDIA GPUs: nvidia-smi utility (requires nvidia-smi in PATH)
 * 3. Intel GPUs: intel_gpu_top utility (requires intel_gpu_top in PATH)
 * - Scans card0-card9 to find available AMD GPU
 * - Logs warning once if no supported GPU found
 * - Returns -1 in load parameter if GPU monitoring unavailable
 */
static int get_gpu_load(int *load)
{
	FILE *fp;
	int value;
	char path[256];
	static int warned = 0; // Log warning only once

	// Try AMD GPU (AMDGPU driver) - scan all DRM cards
	for (int card = 0; card < 10; card++) {
		snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/gpu_busy_percent", card);
		fp = fopen(path, "r");
		if (fp != NULL) {
			if (fscanf(fp, "%d", &value) == 1) {
				*load = value;
				fclose(fp);
				return 0; // Success
			}
			fclose(fp);
		}
	}

	// Try NVIDIA GPU (nvidia-smi utility)
	// CERT-ENV33-C: Safe - uses hardcoded command with no user input
	// NOLINTNEXTLINE(cert-env33-c)
	fp = popen(
	    "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits 2>/dev/null",
	    "r");
	if (fp != NULL) {
		if (fscanf(fp, "%d", &value) == 1) {
			*load = value;
			pclose(fp);
			return 0; // Success
		}
		pclose(fp);
	}

	// Try Intel GPU (intel_gpu_top utility with 1 sample)
	// CERT-ENV33-C: Safe - uses hardcoded command with no user input
	// NOLINTNEXTLINE(cert-env33-c)
	fp = popen("timeout 0.5 intel_gpu_top -J -s 100 2>/dev/null | grep -oP "
		   "'\"Render/3D/0\".*?\"busy\":\\s*\\K[0-9.]+' | head -1",
		   "r");
	if (fp != NULL) {
		float float_value;
		if (fscanf(fp, "%f", &float_value) == 1) {
			*load = (int)float_value;
			pclose(fp);
			return 0; // Success
		}
		pclose(fp);
	}

	// GPU monitoring not available - log warning once
	if (!warned) {
		report(RPT_WARNING,
		       "SysInfo: GPU monitoring not available (no supported GPU found)");
		warned = 1;
	}

	*load = -1; // Signal: not available
	return -1;
}

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

/**
 * \brief Format uptime as human-readable string
 * \param buffer Output buffer for formatted uptime
 * \param bufsize Size of output buffer
 * \param uptime System uptime in seconds
 *
 * \details Formats uptime as "X day(s) HH:MM:SS" (full format).
 */
static void format_uptime_string(char *buffer, size_t bufsize, double uptime)
{
	int days = (int)uptime / 86400;
	int hour = ((int)uptime % 86400) / 3600;
	int min = ((int)uptime % 3600) / 60;
	int sec = ((int)uptime % 60);

	snprintf(buffer, bufsize, "%d day%s %02d:%02d:%02d", days, (days != 1) ? "s" : "", hour,
		 min, sec);
}

// Display comprehensive system information screen (G510s optimized)
int sysinfo_screen(int rep, int display, int *flags_ptr)
{
	char line1[40]; // For 2-line displays
	double uptime, idle;
	load_type load;
	meminfo_type mem[2]; // Array: mem[0]=RAM, mem[1]=Swap
	int cpu_percent, ram_percent;
	static const char *timeFormat = NULL;
	static const char *dateFormat = NULL;

	// Two-phase initialization to handle race condition with server's "listen" command:
	// Phase 1: Send screen_add and return immediately (don't block main_loop)
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add Y\n");
		*flags_ptr |= INITIALIZED;

		// CRITICAL: Return immediately to allow main_loop to read the "listen Y"
		// response from the server. Widget creation happens on the NEXT call.
		return 0;
	}

	// Phase 2: Create widgets (only executes after INITIALIZED is set and we return)
	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100; // Mark widgets as created

		timeFormat = config_get_string("SysInfo", "TimeFormat", 0, "%H:%M:%S");
		dateFormat = config_get_string("SysInfo", "DateFormat", 0, "%b %d %Y");

		sock_send_string(sock, "screen_set Y -name {System Info}\n");

		// For 4+ line displays: Title widget with symmetric blocks
		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add Y title title\n");
			sock_send_string(sock, "widget_add Y uptime_str string\n");
			sock_send_string(sock, "widget_add Y datetime_str string\n");
			sock_send_string(sock, "widget_add Y stats_str string\n");
			sock_printf(sock, "widget_set Y title {%s}\n", get_hostname());
		} else {
			// For 2-line displays: minimal info with title
			sock_send_string(sock, "widget_add Y title title\n");
			sock_send_string(sock, "widget_add Y line1 string\n");
			sock_printf(sock, "widget_set Y title {%s}\n", get_hostname());
			sock_send_string(sock, "widget_set Y line1 1 2 {Initializing...}\n");
		}

		// Don't prime machine_get_load() here - it would cause load.total=0
		// on first update because no time passes between priming and first call
		// Let first call establish baseline, second call will show actual load

		// Don't return - continue to display update (like TimeDate screen)
	}

	// Calculate system metrics
	machine_get_uptime(&uptime, &idle);
	machine_get_load(&load);
	machine_get_meminfo(mem);

	// CPU percentage: calculate from load delta (non-idle time / total time)
	// load.total is the delta of CPU ticks since last call
	if (load.total > 0) {
		// Calculate percentage of non-idle time
		cpu_percent = (int)(100.0 * (double)(load.total - load.idle) / (double)load.total);
		if (cpu_percent < 0)
			cpu_percent = 0;
		if (cpu_percent > 100)
			cpu_percent = 100;
	} else {
		// On first update after initialization, load.total is 0
		// Display 0% (like cpu.c does) instead of causing errors
		cpu_percent = 0;
	}

	// RAM percentage
	if (mem[0].total > 0) {
		long used = mem[0].total - mem[0].free - mem[0].buffers - mem[0].cache;
		ram_percent = (int)(100.0 * ((double)used / (double)mem[0].total));
	} else {
		ram_percent = 0;
	}

	// Update display
	if (lcd_hgt >= 4) {
		char uptime_buf[40];
		char uptime_display[40];
		char date_str[40];
		char time_str[40];
		char datetime_display[40];
		int xoffs;

		// Line 1: Hostname (title widget handles rendering)
		if (display) {
			sock_printf(sock, "widget_set Y title {%s}\n", get_hostname());
		}

		// Line 2: Uptime (centered)
		format_uptime_string(uptime_buf, sizeof(uptime_buf), uptime);
		snprintf(uptime_display, sizeof(uptime_display), "Up %s", uptime_buf);
		if (display) {
			xoffs = calculate_centered_xpos(uptime_display);
			sock_printf(sock, "widget_set Y uptime_str %d 2 {%s}\n", xoffs,
				    uptime_display);
		}

		// Line 3: Date and Time (centered)
		get_formatted_time(date_str, sizeof(date_str), dateFormat);
		get_formatted_time(time_str, sizeof(time_str), timeFormat);
		snprintf(datetime_display, sizeof(datetime_display), "%s %s", date_str, time_str);
		if (display) {
			xoffs = calculate_centered_xpos(datetime_display);
			sock_printf(sock, "widget_set Y datetime_str %d 3 {%s}\n", xoffs,
				    datetime_display);
		}

		// Line 4: CPU/RAM/GPU stats (centered)
		int gpu_temp = 0;
		int gpu_load = 0;
		char stats_display[40];

		get_gpu_temp(&gpu_temp);
		get_gpu_load(&gpu_load);

		// Format: "C:45% R:62% G:30%" or "C:45% R:62% G:N/A" if GPU unavailable
		if (gpu_load >= 0) {
			// GPU monitoring available
			snprintf(stats_display, sizeof(stats_display), "C:%2d%% R:%2d%% G:%2d%%",
				 cpu_percent, ram_percent, gpu_load);
		} else {
			// GPU monitoring not available
			snprintf(stats_display, sizeof(stats_display), "C:%2d%% R:%2d%% G:N/A",
				 cpu_percent, ram_percent);
		}

		if (display) {
			xoffs = calculate_centered_xpos(stats_display);
			sock_printf(sock, "widget_set Y stats_str %d 4 {%s}\n", xoffs,
				    stats_display);
		}
	} else {
		// 2-line layout: minimal info
		time_t now_t;
		struct tm now_tm;
		char time_str[20];
		time(&now_t);
		localtime_r(&now_t, &now_tm);
		strftime(time_str, sizeof(time_str), "%H:%M", &now_tm);
		snprintf(line1, sizeof(line1), "%sCPU:%d%%RAM:%d%%", time_str, cpu_percent,
			 ram_percent);

		if (display) {
			sock_printf(sock, "widget_set Y line1 1 2 {%s}\n", line1);
		}
	}

	return 0;
}
