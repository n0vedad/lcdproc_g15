// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/eyebox.c
 * \brief EyeboxOne device support module for lcdproc client
 * \author Cedric TESSIER (aka NeZetiC)
 * \date 2006
 *
 * \features
 * - LED-based CPU usage indicator (Bar ID 2)
 * - LED-based RAM usage indicator (Bar ID 1)
 * - Rolling average CPU calculation for smooth LED transitions
 * - Memory usage calculation including buffers and cache
 * - LED reset functionality for clean shutdown
 * - Integration with lcdproc's standard monitoring systems
 * - EyeboxOne specific command protocol (/xBab format)
 * - Real-time system metrics to LED brightness mapping
 *
 * \usage
 * - Called by lcdproc client for EyeboxOne device support
 * - Automatically updates LED indicators with system metrics
 * - Uses rolling averages for smooth LED brightness transitions
 * - Integrates with standard lcdproc monitoring framework
 * - Provides clean shutdown functionality
 *
 * \details This file implements support for EyeboxOne devices in the lcdproc
 * client. The EyeboxOne is a special LCD device that includes LED indicators
 * that can be controlled to show system status information in addition to
 * the standard LCD display. The module uses special EyeboxOne commands
 * (/xBab format) to control the LED bars where 'a' is the Bar ID and 'b'
 * is the brightness level.
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

#include "eyebox.h"
#include "machine.h"
#include "main.h"
#include "mode.h"
#include "util.h"

// Update EyeboxOne LED indicators with system status
int eyebox_screen(char display, int init)
{
	// Redefine CPU_BUF_SIZE for eyebox LED smooth transitions
/** @cond */
#undef CPU_BUF_SIZE
/** \brief Rolling average buffer size for eyebox CPU monitoring */
#define CPU_BUF_SIZE 4
	/** @endcond */
	int i, j;
	double value;
	static double cpu[CPU_BUF_SIZE + 1][5];
	meminfo_type mem[2];
	load_type load;

	// One-time widget initialization: create LED control widgets
	if (init == 0) {
		sock_printf(sock, "widget_add %c eyebo_cpu string\n", display);
		sock_printf(sock, "widget_add %c eyebo_mem string\n", display);
		return 0;
	}

	machine_get_load(&load);
	machine_get_meminfo(mem);

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

	// Calculate rolling averages for all 5 categories (smooth LED transitions)
	for (i = 0; i < 5; i++) {
		value = 0.0;
		for (j = 0; j < CPU_BUF_SIZE; j++)
			value += cpu[j][i];
		value /= CPU_BUF_SIZE;
		cpu[CPU_BUF_SIZE][i] = value;
	}

	// Send CPU LED control command: /xBab format (a=Bar ID, b=Level 0-10)
	// Bar ID 2 = CPU usage indicator, convert percentage to 0-10 scale
	sock_printf(sock, "widget_set %c eyebo_cpu 1 2 {/xB%d%d}\n", display, 2,
		    (int)(cpu[CPU_BUF_SIZE][4] / 10));

	// Calculate memory usage percentage (excluding free, buffers, and cache)
	value = 1.0 - (double)(mem[0].free + mem[0].buffers + mem[0].cache) / (double)mem[0].total;

	// Send Memory LED control command: /xBab format (a=Bar ID, b=Level 0-10)
	// Bar ID 1 = Memory usage indicator, convert percentage to 0-10 scale
	sock_printf(sock, "widget_set %c eyebo_mem 1 3 {/xB%d%d}\n", display, 1, (int)(value * 10));

	return 0;
}

// Clear and reset EyeboxOne LED indicators
void eyebox_clear(void)
{
	sock_send_string(sock, "screen_add OFF\n");
	sock_send_string(sock, "screen_set OFF -priority alert -name {EyeBO}\n");
	sock_send_string(sock, "widget_add OFF title title\n");
	sock_send_string(sock, "widget_set OFF title {EYEBOX ONE}\n");
	sock_send_string(sock, "widget_add OFF text string\n");
	sock_send_string(sock, "widget_add OFF about string\n");
	sock_send_string(sock, "widget_add OFF cpu string\n");
	sock_send_string(sock, "widget_add OFF mem string\n");

	sock_send_string(sock, "widget_set OFF text 1 2 {Reseting Leds...}\n");
	sock_send_string(sock, "widget_set OFF about 5 4 {EyeBO by NeZetiC}\n");

	// Turn off CPU LED (Bar ID 2) and Memory LED (Bar ID 1)
	sock_printf(sock, "widget_set OFF cpu 1 2 {/xB%d%d}\n", 2, 0);
	sock_printf(sock, "widget_set OFF mem 1 3 {/xB%d%d}\n", 1, 0);

	usleep(2000000);
}
