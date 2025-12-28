// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/cpu.c
 * \brief CPU usage monitoring screens for single processor systems
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **CPU Screen**: Detailed numerical CPU usage with breakdown by category
 * - **CPUGraph Screen**: Real-time histogram showing CPU usage over time
 * - Rolling average calculation over multiple samples for smooth display
 * - Adaptive layouts for different LCD sizes (2-line vs 4-line)
 * - User, System, Nice, and Idle time monitoring
 * - Progress bar and histogram visualizations
 * - Real-time scrolling graph display
 * - Single processor system optimization
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically displays CPU usage information
 * - Adapts display format based on LCD dimensions
 * - Updates CPU statistics in real-time
 * - Provides both numerical and graphical representations
 *
 * \details This file implements CPU usage monitoring screens for single
 * processor systems, providing both detailed numerical displays and
 * graphical histogram representations of CPU load. The implementation
 * includes rolling average calculations for smooth data presentation
 * and adaptive layouts for different LCD sizes.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared/sockets.h"

#include "cpu.h"
#include "machine.h"
#include "main.h"
#include "mode.h"
#include "util.h"

// Display detailed CPU usage screen with numerical breakdown
int cpu_screen(int rep, int display, int *flags_ptr)
{
	// Redefine CPU_BUF_SIZE for this function's specific rolling average window
/** @cond */
#undef CPU_BUF_SIZE
/** \brief Rolling average buffer size for detailed CPU screen */
#define CPU_BUF_SIZE 4
	/** @endcond */
	static double cpu[CPU_BUF_SIZE + 1][5];

	static int gauge_wid = 0;
	static int usni_wid = 0;
	static int us_wid = 0;
	static int ni_wid = 0;

	int i, j;
	double value;
	load_type load;
	char tmp[25];

	// One-time screen initialization: create layout based on display height
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		sock_send_string(sock, "screen_add C\n");
		sock_printf(sock, "screen_set C -name {CPU Use:%s}\n", get_hostname());

		// Layout for 4-line displays: detailed numerical breakdown
		if (lcd_hgt >= 4) {
			us_wid = ((lcd_wid + 1) / 2) - 7;
			ni_wid = lcd_wid / 2 - 6;

			sock_send_string(sock, "widget_add C title title\n");
			sock_send_string(sock, "widget_set C title {CPU LOAD}\n");
			sock_send_string(sock, "widget_add C one string\n");
			sock_send_string(sock, "widget_add C two string\n");

			sock_printf(sock, "widget_set C one 1 2 {%-*.*s       %-*.*s}\n", us_wid,
				    us_wid, "Usr", ni_wid, ni_wid, "Nice");

			sock_printf(sock, "widget_set C two 1 3 {%-*.*s       %-*.*s}\n", us_wid,
				    us_wid, "Sys", ni_wid, ni_wid, "Idle");

			sock_send_string(sock, "widget_add C usr string\n");
			sock_send_string(sock, "widget_add C nice string\n");
			sock_send_string(sock, "widget_add C idle string\n");
			sock_send_string(sock, "widget_add C sys string\n");
			pbar_widget_add("C", "bar");

			// Layout for 2-line displays: compact graphical view with mini bars
		} else {
			usni_wid = lcd_wid / 4;
			gauge_wid = lcd_wid - 10;

			sock_send_string(sock, "widget_add C cpu string\n");
			sock_printf(sock, "widget_set C cpu 1 1 {CPU }\n");
			sock_send_string(sock, "widget_add C cpu% string\n");
			sock_printf(sock, "widget_set C cpu%% 1 %d { 0.0%%}\n", lcd_wid - 5);

			pbar_widget_add("C", "usr");
			pbar_widget_add("C", "sys");
			pbar_widget_add("C", "nice");
			pbar_widget_add("C", "idle");
			pbar_widget_add("C", "total");
		}

		return (0);
	}

	machine_get_load(&load);

	// Shift rolling buffer: move all samples one position left
	for (i = 0; i < (CPU_BUF_SIZE - 1); i++)
		for (j = 0; j < 5; j++)
			cpu[i][j] = cpu[i + 1][j];

	// Calculate CPU percentages: User, System, Nice, Idle, Total (user+sys+nice)
	if (load.total > 0L) {
		cpu[CPU_BUF_SIZE - 1][0] = 100.0 * ((double)load.user / (double)load.total);
		cpu[CPU_BUF_SIZE - 1][1] = 100.0 * ((double)load.system / (double)load.total);
		cpu[CPU_BUF_SIZE - 1][2] = 100.0 * ((double)load.nice / (double)load.total);
		cpu[CPU_BUF_SIZE - 1][3] = 100.0 * ((double)load.idle / (double)load.total);
		cpu[CPU_BUF_SIZE - 1][4] =
		    100.0 * (((double)load.user + (double)load.system + (double)load.nice) /
			     (double)load.total);
	} else {
		cpu[CPU_BUF_SIZE - 1][0] = 0.0;
		cpu[CPU_BUF_SIZE - 1][1] = 0.0;
		cpu[CPU_BUF_SIZE - 1][2] = 0.0;
		cpu[CPU_BUF_SIZE - 1][3] = 0.0;
		cpu[CPU_BUF_SIZE - 1][4] = 0.0;
	}

	// Calculate and store rolling averages for all 5 categories (smooth display)
	for (i = 0; i < 5; i++) {
		value = 0.0;
		for (j = 0; j < CPU_BUF_SIZE; j++)
			value += cpu[j][i];
		value /= CPU_BUF_SIZE;
		cpu[CPU_BUF_SIZE][i] = value;
	}

	if (!display)
		return (0);

	// Update display based on LCD height
	if (lcd_hgt >= 4) {
		// 4-line display: detailed numerical view
		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][4]);
		sock_printf(sock, "widget_set C title {CPU %5s:%s}\n", tmp, get_hostname());

		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][0]);
		sock_printf(sock, "widget_set C usr %i 2 {%5s}\n", ((lcd_wid + 1) / 2) - 5, tmp);

		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][1]);
		sock_printf(sock, "widget_set C sys %i 3 {%5s}\n", ((lcd_wid + 1) / 2) - 5, tmp);

		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][2]);
		sock_printf(sock, "widget_set C nice %i 2 {%5s}\n", lcd_wid - 4, tmp);

		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][3]);
		sock_printf(sock, "widget_set C idle %i 3 {%5s}\n", lcd_wid - 4, tmp);

		pbar_widget_set("C", "bar", 1, 4, lcd_wid, cpu[CPU_BUF_SIZE][4] * 10, "0%", "100%");

		// 2-line display: compact graphical view with 4 mini bars
	} else {
		sprintf_percent(tmp, cpu[CPU_BUF_SIZE][4]);
		sock_printf(sock, "widget_set C cpu%% %d 1 {%5s}\n", lcd_wid - 5, tmp);

		pbar_widget_set("C", "total", 5, 1, gauge_wid, cpu[CPU_BUF_SIZE][4] * 10, NULL,
				NULL);
		pbar_widget_set("C", "usr", 1 + 0 * usni_wid, 2, usni_wid,
				cpu[CPU_BUF_SIZE][0] * 10, "U", NULL);
		pbar_widget_set("C", "sys", 1 + 1 * usni_wid, 2, usni_wid,
				cpu[CPU_BUF_SIZE][1] * 10, "S", NULL);
		pbar_widget_set("C", "nice", 1 + 2 * usni_wid, 2, usni_wid,
				cpu[CPU_BUF_SIZE][2] * 10, "N", NULL);
		pbar_widget_set("C", "idle", 1 + 3 * usni_wid, 2, usni_wid,
				cpu[CPU_BUF_SIZE][3] * 10, "I", NULL);
	}

	return (0);
}

// Display real-time CPU usage histogram screen
int cpu_graph_screen(int rep, int display, int *flags_ptr)
{
	// Redefine CPU_BUF_SIZE for graph screen's smaller rolling window
/** @cond */
#undef CPU_BUF_SIZE
/** \brief Rolling average buffer size for CPU graph screen */
#define CPU_BUF_SIZE 2
	/** @endcond */
	static double cpu[CPU_BUF_SIZE];
	static int cpu_past[LCD_MAX_WIDTH];
	static int gauge_hgt = 0;

	int i, n = 0;
	double value;
	load_type load;

	// One-time screen initialization: create scrolling histogram display
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		// Reserve top line for title if display has 3+ lines
		gauge_hgt = (lcd_hgt > 2) ? (lcd_hgt - 1) : lcd_hgt;

		sock_send_string(sock, "screen_add G\n");
		sock_printf(sock, "screen_set G -name {CPU Graph:%s}\n", get_hostname());

		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add G title title\n");
			sock_printf(sock, "widget_set G title {CPU:%s}\n", get_hostname());
		} else {
			sock_send_string(sock, "widget_add G title string\n");
			sock_printf(sock, "widget_set G title 1 1 {CPU:%s}\n", get_hostname());
		}

		// Create vertical bar for each column (scrolling histogram)
		for (i = 1; i <= lcd_wid; i++) {
			sock_printf(sock, "widget_add G bar%d vbar\n", i);
			sock_printf(sock, "widget_set G bar%d %d %d 0\n", i, i, lcd_hgt);
			cpu_past[i - 1] = 0;
		}

		for (i = 0; i < CPU_BUF_SIZE; i++)
			cpu[i] = 0.0;
	}

	// Shift rolling buffer for smooth animation
	for (i = 0; i < (CPU_BUF_SIZE - 1); i++)
		cpu[i] = cpu[i + 1];

	machine_get_load(&load);
	cpu[CPU_BUF_SIZE - 1] =
	    (load.total > 0L)
		? ((double)load.user + (double)load.system + (double)load.nice) / (double)load.total
		: 0;

	// Calculate rolling average for smooth display
	value = 0.0;
	for (i = 0; i < CPU_BUF_SIZE; i++)
		value += cpu[i];
	value /= CPU_BUF_SIZE;
	n = (int)(value * lcd_cellhgt * gauge_hgt);

	// Scroll histogram left: shift all bars one column left
	for (i = 0; i < lcd_wid - 1; i++) {
		cpu_past[i] = cpu_past[i + 1];

		if (display) {
			sock_printf(sock, "widget_set G bar%d %d %d %d\n", i + 1, i + 1, lcd_hgt,
				    cpu_past[i]);
		}
	}

	// Add newest data to rightmost column
	cpu_past[lcd_wid - 1] = n;
	if (display) {
		sock_printf(sock, "widget_set G bar%d %d %d %d\n", lcd_wid, lcd_wid, lcd_hgt, n);
	}

	return (0);
}
