// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/cpu_smp.c
 * \brief SMP CPU usage display screen for multi-processor systems
 * \author J Robert Ray, Peter Marschall
 * \date 2000-2007
 *
 * \features
 * - Individual CPU core usage monitoring with percentage graphs
 * - Adaptive layout: 1 CPU per line or 2 CPUs per line based on LCD size
 * - Supports up to 2×LCD_height or MAX_CPUS cores, whichever is smaller
 * - Rolling average calculation over multiple samples for smooth display
 * - Optional title display when screen space permits
 * - Horizontal bar graphs with configurable width
 * - Real-time CPU usage visualization for SMP systems
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically detects and monitors all available CPU cores
 * - Adapts display layout based on LCD dimensions
 * - Updates CPU usage information in real-time
 * - Works with any multi-processor system configuration
 *
 * \details This file implements CPU usage monitoring for multi-processor
 * systems (SMP). It displays real-time CPU usage graphs for each processor
 * core with adaptive layouts based on the number of CPUs and LCD dimensions.
 *
 * **Layout Logic:**
 * - If CPUs ≤ LCD height: One CPU per line with full-width bars
 * - If CPUs > LCD height: Two CPUs per line with half-width bars
 * - Title shown only when lines_used < LCD height
 * - Maximum display limit: 2×LCD_height or 8 CPUs (MAX_CPUS)
 *
 * Adapted from the original single-CPU cpu.c implementation.
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared/sockets.h"

#include "cpu_smp.h"
#include "machine.h"
#include "main.h"
#include "mode.h"

// Display SMP CPU usage screen with individual core monitoring
int cpu_smp_screen(int rep, int display, int *flags_ptr)
{
	// Redefine CPU_BUF_SIZE for SMP screen's rolling average calculation
/** @cond */
#undef CPU_BUF_SIZE
/** \brief Rolling average buffer size for SMP CPU screen */
#define CPU_BUF_SIZE 4
	/** @endcond */
	int z;
	static float cpu[MAX_CPUS][CPU_BUF_SIZE + 1];
	load_type load[MAX_CPUS];
	int num_cpus = MAX_CPUS;
	int bar_size;
	int lines_used;

	machine_get_smpload(load, &num_cpus);

	// Limit display to maximum 2×LCD height
	if (num_cpus > 2 * lcd_hgt)
		num_cpus = 2 * lcd_hgt;

	// Adaptive layout: if CPUs > LCD height, use 2-per-line layout with half-width bars
	bar_size = (num_cpus > lcd_hgt) ? (lcd_wid / 2 - 6) : (lcd_wid - 6);
	lines_used = (num_cpus > lcd_hgt) ? (num_cpus + 1) / 2 : num_cpus;

	// One-time screen initialization: create widgets based on CPU count and layout
	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		sock_send_string(sock, "screen_add P\n");

		// Show title only if there's available space (not all lines used by CPUs)
		if (lines_used < lcd_hgt) {
			sock_send_string(sock, "widget_add P title title\n");
			sock_printf(sock, "widget_set P title {SMP CPU%s}\n", get_hostname());
		} else {
			sock_send_string(sock, "screen_set P -heartbeat off\n");
		}

		sock_printf(sock, "screen_set P -name {CPU Use: %s}\n", get_hostname());

		// Create widgets for each CPU core with adaptive positioning
		for (z = 0; z < num_cpus; z++) {
			// Position calculation: y_offs=2 with title, y_offs=1 without (saves space)
			// For 2-per-line: x alternates between left/right, y advances every 2 CPUs
			int y_offs = (lines_used < lcd_hgt) ? 2 : 1;
			int x = (num_cpus > lcd_hgt) ? ((z % 2) * (lcd_wid / 2) + 1) : 1;
			int y = (num_cpus > lcd_hgt) ? (z / 2 + y_offs) : (z + y_offs);

			sock_printf(sock, "widget_add P cpu%d_title string\n", z);
			sock_printf(sock, "widget_set P cpu%d_title %d %d \"CPU%d[%*s]\"\n", z, x,
				    y, z, bar_size, "");
			sock_printf(sock, "widget_add P cpu%d_bar hbar\n", z);
		}

		return 0;
	}

	// Update CPU usage displays for all cores
	for (z = 0; z < num_cpus; z++) {
		int y_offs = (lines_used < lcd_hgt) ? 2 : 1;
		int x = (num_cpus > lcd_hgt) ? ((z % 2) * (lcd_wid / 2) + 6) : 6;
		int y = (num_cpus > lcd_hgt) ? (z / 2 + y_offs) : (z + y_offs);
		float value = 0.0;
		int i, n;

		// Shift rolling buffer: move all samples one position left for new value
		for (i = 0; i < (CPU_BUF_SIZE - 1); i++)
			cpu[z][i] = cpu[z][i + 1];

		// Calculate CPU usage: (user + system + nice) / total × 100%
		cpu[z][CPU_BUF_SIZE - 1] =
		    (load[z].total > 0L)
			? (((float)load[z].user + (float)load[z].system + (float)load[z].nice) /
			   (float)load[z].total) *
			      100.0
			: 0.0;

		// Calculate rolling average over buffer for smooth display
		for (i = 0; i < CPU_BUF_SIZE; i++) {
			value += cpu[z][i];
		}
		value /= CPU_BUF_SIZE;

		// Convert percentage to bar pixel width: percentage × cell_width × bar_size / 100
		n = (int)((value * lcd_cellwid * bar_size) / 100.0 + 0.5);
		sock_printf(sock, "widget_set P cpu%d_bar %d %d %d\n", z, x, y, n);
	}

	return 0;
}
