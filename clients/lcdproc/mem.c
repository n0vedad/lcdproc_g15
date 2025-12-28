// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/mem.c
 * \brief Memory usage and process monitoring screens for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - Memory and swap usage display with progress bars
 * - Adaptive layout for different LCD sizes
 * - Process memory usage ranking (top memory consumers)
 * - Real-time memory statistics monitoring
 * - Memory values formatted in human-readable units
 * - Scrolling display for process lists
 * - Title alternation between hostname and memory info
 * - Two distinct screens: memory overview and process ranking
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically displays memory and swap usage information
 * - Provides top memory-consuming processes display
 * - Adapts display format based on LCD dimensions
 * - Updates memory statistics in real-time
 *
 * \details
 * This file implements memory-related monitoring screens for the
 * lcdproc client. It provides two main screens: a memory usage overview
 * showing RAM and swap utilization, and a process memory usage ranking
 * screen showing the top memory-consuming processes.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef IRIX
#include <strings.h>
#endif

#include "shared/LL.h"
#include "shared/sockets.h"

#include "machine.h"
#include "main.h"
#include "mem.h"
#include "mode.h"
#include "util.h"

// Display memory and swap usage information screen
int mem_screen(int rep, int display, int *flags_ptr)
{
	const char *title_sep = "##################################################################"
				"##################################";
	static int which_title = 0;
	static int gauge_wid = 0;
	static int gauge_offs = 0;
	static int title_sep_wid = 0;
	int label_wid, label_offs;
	meminfo_type mem[2];

	// Two-phase initialization to handle race condition with server's "listen" command
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add M\n");
		*flags_ptr |= INITIALIZED;
		return 0;
	}

	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100;

		sock_printf(sock, "screen_set M -name {Memory & Swap: %s}\n", get_hostname());

		// Calculate separator width: reserve 16 chars for "MEM" and "SWAP" labels
		title_sep_wid = (lcd_wid >= 16) ? lcd_wid - 16 : 0;

		// Detailed layout for 4+ line displays
		if (lcd_hgt >= 4) {
			gauge_wid = lcd_wid / 2;
			if ((lcd_wid % 2) == 0 && gauge_wid >= 9)
				gauge_wid--;

			label_wid = (title_sep_wid >= 4) ? 4 : title_sep_wid;
			label_offs = (lcd_wid - label_wid) / 2 + 1;

			sock_send_string(sock, "widget_add M title title\n");
			sock_printf(sock, "widget_set M title { MEM %.*s SWAP}\n", title_sep_wid,
				    title_sep);

			sock_send_string(sock, "widget_add M totl string\n");
			sock_send_string(sock, "widget_add M free string\n");

			sock_printf(sock, "widget_set M totl %i 2 %.*s\n", label_offs, label_wid,
				    "Totl");
			sock_printf(sock, "widget_set M free %i 3 %.*s\n", label_offs, label_wid,
				    "Free");

			sock_send_string(sock, "widget_add M memused string\n");
			sock_send_string(sock, "widget_add M swapused string\n");

			// Compact layout for 2-3 line displays
		} else {
			if (lcd_wid >= 20) {
				gauge_wid = lcd_wid - 16;
				gauge_offs = 10;
			} else if (lcd_wid >= 17) {
				gauge_wid = lcd_wid - 14;
				gauge_offs = 9;
			} else {
				gauge_wid = gauge_offs = 0;
			}

			sock_send_string(sock, "widget_add M m string\n");
			sock_send_string(sock, "widget_add M s string\n");
			sock_send_string(sock, "widget_set M m 1 1 {M}\n");
			sock_send_string(sock, "widget_set M s 1 2 {S}\n");
			sock_send_string(sock, "widget_add M mem% string\n");
			sock_send_string(sock, "widget_add M swap% string\n");
		}

		sock_send_string(sock, "widget_add M memtotl string\n");
		sock_send_string(sock, "widget_add M swaptotl string\n");

		pbar_widget_add("M", "memgauge");
		pbar_widget_add("M", "swapgauge");
	}

	// Alternate title display on tall screens
	if (lcd_hgt >= 4) {
		if (which_title & 4) {
			if (get_hostname()[0] != '\0')
				sock_printf(sock, "widget_set M title {%s}\n", get_hostname());
		} else {
			sock_printf(sock, "widget_set M title { MEM %.*s SWAP}\n", title_sep_wid,
				    title_sep);
		}
		which_title = (which_title + 1) & 7;
	}

	if (!display)
		return 0;

	machine_get_meminfo(mem);

	/**
	 * \note 4+ line displays: detailed view with totals, used values, and progress bars on row
	 * 4 2-3 line displays: compact view with totals and percentages, optional inline gauges
	 */

	// Display memory and swap statistics with adaptive layout for different screen sizes
	if (lcd_hgt >= 4) {
		char tmp[12];

		sprintf_memory(tmp, mem[0].total * 1024.0, 1);
		sock_printf(sock, "widget_set M memtotl 1 2 {%7s}\n", tmp);

		sprintf_memory(tmp, (mem[0].free + mem[0].buffers + mem[0].cache) * 1024.0, 1);
		sock_printf(sock, "widget_set M memused 1 3 {%7s}\n", tmp);

		sprintf_memory(tmp, mem[1].total * 1024.0, 1);
		sock_printf(sock, "widget_set M swaptotl %i 2 {%7s}\n", lcd_wid - 7, tmp);

		sprintf_memory(tmp, mem[1].free * 1024.0, 1);
		sock_printf(sock, "widget_set M swapused %i 3 {%7s}\n", lcd_wid - 7, tmp);

		// Update progress bar gauges for memory and swap usage: calculate RAM usage ratio
		// excluding buffers/cache, position memory gauge at left and swap gauge at right,
		// scale values to promille
		if (gauge_wid > 0) {
			if (mem[0].total > 0) {
				double value =
				    1.0 - (double)(mem[0].free + mem[0].buffers + mem[0].cache) /
					      (double)mem[0].total;

				pbar_widget_set("M", "memgauge", 1, 4, gauge_wid, value * 1000, "E",
						"F");
			}

			if (mem[1].total > 0) {
				double value = 1.0 - ((double)mem[1].free / (double)mem[1].total);

				pbar_widget_set("M", "swapgauge", 1 + lcd_wid - gauge_wid, 4,
						gauge_wid, value * 1000, "E", "F");
			}
		}

		// Compact layout for smaller displays: show total memory/swap values, calculate
		// usage percentages with optional progress bars, display percentage values or "N/A"
		// if unavailable
	} else {
		char tmp[12];

		sprintf_memory(tmp, mem[0].total * 1024.0, 1);
		sock_printf(sock, "widget_set M memtotl 3 1 {%6s}\n", tmp);

		sprintf_memory(tmp, mem[1].total * 1024.0, 1);
		sock_printf(sock, "widget_set M swaptotl 3 2 {%6s}\n", tmp);

		strcpy(tmp, "N/A");
		if (mem[0].total > 0) {
			double value = 1.0 - (double)(mem[0].free + mem[0].buffers + mem[0].cache) /
						 (double)mem[0].total;

			if (gauge_wid > 0)
				pbar_widget_set("M", "memgauge", gauge_offs, 1, gauge_wid,
						value * 1000, NULL, NULL);

			sprintf_percent(tmp, value * 100);
		}
		sock_printf(sock, "widget_set M mem%% %i 1 {%5s}\n", lcd_wid - 5, tmp);

		strcpy(tmp, "N/A");
		if (mem[1].total > 0) {
			double value = 1.0 - ((double)mem[1].free / (double)mem[1].total);

			if (gauge_wid > 0)
				pbar_widget_set("M", "swapgauge", gauge_offs, 2, gauge_wid,
						value * 1000, NULL, NULL);

			sprintf_percent(tmp, value * 100);
		}
		sock_printf(sock, "widget_set M swap%% %i 2 {%5s}\n", lcd_wid - 5, tmp);
	}

	return 0;
}

/**
 * \brief Sort procs
 * \param a void *a
 * \param b void *b
 * \return Return value
 */
static int sort_procs(void *a, void *b)
{
	procinfo_type *one, *two;

	if ((a == NULL) || (b == NULL))
		return 0;

	one = (procinfo_type *)a;
	two = (procinfo_type *)b;

	return (two->totl > one->totl);
}

// Display top memory-consuming processes screen
int mem_top_screen(int rep, int display, int *flags_ptr)
{
	LinkedList *procs;
	int lines;
	int i;

	// Calculate optimal number of lines for process display
	if (lcd_hgt <= 4)
		lines = 5;
	else
		lines = lcd_hgt - 1;

	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		sock_send_string(sock, "screen_add S\n");
		sock_printf(sock, "screen_set S -name {Top Memory Use: %s}\n", get_hostname());
		sock_send_string(sock, "widget_add S title title\n");
		sock_printf(sock, "widget_set S title {TOP MEM:%s}\n", get_hostname());

		// Frame from (2nd line, left) to (last line, right)
		sock_send_string(sock, "widget_add S f frame\n");

		// Scroll rate: 1 line every X ticks (= 1/8 sec)
		sock_printf(sock, "widget_set S f 1 2 %i %i %i %i v %i\n", lcd_wid, lcd_hgt,
			    lcd_wid, lines, ((lcd_hgt >= 4) ? 8 : 12));

		for (i = 1; i <= lines; i++) {
			sock_printf(sock, "widget_add S %i string -in f\n", i);
		}
		sock_send_string(sock, "widget_set S 1 1 1 Checking...\n");
	}

	if (!display)
		return 0;

	procs = LL_new();
	if (procs == NULL) {
		fprintf(stderr, "mem_top_screen: Error allocating list\n");
		return -1;
	}

	machine_get_procs(procs);

	// Sort processes by memory usage (descending)
	LL_Rewind(procs);
	LL_Sort(procs, sort_procs);
	LL_Rewind(procs);

	for (i = 1; i <= lines; i++) {
		procinfo_type *p = LL_Get(procs);

		if (p != NULL) {
			char mem[10];

			sprintf_memory(mem, (double)p->totl * 1024.0, 1);

			// Display with instance count if multiple instances exist
			if (p->number > 1)
				sock_printf(sock, "widget_set S %i 1 %i {%i %5s %s(%i)}\n", i, i, i,
					    mem, p->name, p->number);
			else
				sock_printf(sock, "widget_set S %i 1 %i {%i %5s %s}\n", i, i, i,
					    mem, p->name);
		} else {
			sock_printf(sock, "widget_set S %i 1 %i { }\n", i, i);
		}

		LL_Next(procs);
	}

	// Clean up allocated memory
	LL_Rewind(procs);
	do {
		procinfo_type *p = (procinfo_type *)LL_Get(procs);
		if (p != NULL) {
			free(p);
		}
	} while (LL_Next(procs) == 0);
	LL_Destroy(procs);

	return 0;
}
