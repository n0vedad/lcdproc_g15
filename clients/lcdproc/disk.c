// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/disk.c
 * \brief Disk usage monitoring screen for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - Multi-filesystem monitoring with scrolling display
 * - Configurable filesystem ignore list
 * - Adaptive layout for different LCD sizes (16-char vs 20+ char)
 * - Horizontal bar graphs showing disk usage percentage
 * - Memory-formatted capacity display (KB/MB/GB/TB)
 * - Real-time filesystem detection and unmount handling
 * - Mountpoint path truncation for better display fit
 * - Support for both compact and full display formats
 *
 * \usage
 * - Called by the main lcdproc screen rotation system
 * - Automatically scans and displays all mounted filesystems
 * - Adapts display format based on LCD dimensions
 * - Updates filesystem statistics in real-time
 * - Respects configured filesystem ignore list
 *
 * \details This file implements the disk usage screen that displays
 * filesystem usage statistics for all mounted filesystems. It shows
 * disk space utilization with both text information and horizontal
 * bar graphs, adapting the display layout based on LCD dimensions
 * and providing configurable filtering of unwanted filesystems.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared/configfile.h"
#include "shared/sockets.h"

#include "disk.h"
#include "machine.h"
#include "main.h"
#include "mode.h"
#include "util.h"

/** \brief Maximum number of disk mount points that can be ignored */
#define DISK_IGNORE_MAX 10

/**
 * \brief Check if disk mount point should be ignored
 * \param mount Mount point path to check
 * \param ignore_list Array of mount points to ignore
 * \retval true Mount point should be ignored
 * \retval false Mount point should be displayed
 *
 * \details Searches ignore_list for matching mount point using strcmp().
 */
static bool disk_is_ignored(const char *mount, char *ignore_list[DISK_IGNORE_MAX])
{
	for (int i = 0; i < DISK_IGNORE_MAX; i++) {
		if (ignore_list[i] == NULL) {
			return false;
		}
		if (strcmp(mount, ignore_list[i]) == 0) {
			return true;
		}
	}
	return false;
}

// Display disk usage screen with filesystem statistics
int disk_screen(int rep, int display, int *flags_ptr)
{
	mounts_type mnt[256];
	int count = 0;

	int i;
	static int num_disks = 0;
	static int dev_wid = 6;

	static int gauge_wid = 6, gauge_scale, hbar_pos;
	int i_widget = 0;
	static char *disk_ignore[DISK_IGNORE_MAX] = {NULL};

	// Two-phase initialization to handle race condition with server's "listen" command
	if ((*flags_ptr & INITIALIZED) == 0) {
		sock_send_string(sock, "screen_add D\n");
		*flags_ptr |= INITIALIZED;
		return 0;
	}

	if ((*flags_ptr & (INITIALIZED | 0x100)) == INITIALIZED) {
		*flags_ptr |= 0x100;

		// Load ignored mountpoints from config [Disk] section
		const char *cfg_val = NULL;
		while ((count < DISK_IGNORE_MAX) &&
		       (cfg_val = config_get_string("Disk", "Ignore", count, NULL))) {
			disk_ignore[count] = strdup(cfg_val);
			count++;
		}

		// Adaptive layout: wide displays (20+) show capacity, compact (16-19) don't
		dev_wid = (lcd_wid >= 20) ? (lcd_wid - 8) / 2 : (lcd_wid / 2) - 1;
		hbar_pos = (lcd_wid >= 20) ? dev_wid + 10 : dev_wid + 3;
		gauge_wid = lcd_wid - hbar_pos;
		gauge_scale = gauge_wid * lcd_cellwid;

		sock_printf(sock, "screen_set D -name {Disk Use: %s}\n", get_hostname());
		sock_send_string(sock, "widget_add D title title\n");
		sock_printf(sock, "widget_set D title {DISKS:%s}\n", get_hostname());
		sock_send_string(sock, "widget_add D f frame\n");
		sock_printf(sock, "widget_set D f 1 2 %i %i %i %i v 12\n", lcd_wid, lcd_hgt,
			    lcd_wid, lcd_hgt - 1);
		sock_send_string(sock, "widget_add D err1 string\n");
		sock_send_string(sock, "widget_add D err2 string\n");
		sock_send_string(sock, "widget_set D err1 5 2 {  Reading  }\n");
		sock_send_string(sock, "widget_set D err2 5 3 {Filesystems}\n");
	}

	machine_get_fs(mnt, &count);
	if (!count) {
		sock_send_string(sock, "widget_set D err1 1 2 {Error Retrieving}\n");
		sock_send_string(sock, "widget_set D err2 1 3 {Filesystem Stats}\n");
		return 0;
	}

	// Hide error messages (filesystem data available)
	sock_send_string(sock, "widget_set D err1 0 0 .\n");
	sock_send_string(sock, "widget_set D err2 0 0 .\n");

	// Set frame height to accommodate all filesystems for scrolling
	sock_printf(sock, "widget_set D f 1 2 %i %i %i %i v 12\n", lcd_wid, lcd_hgt, lcd_wid,
		    count);

	// Process each filesystem and create/update display widgets
	for (i = 0; i < count; i++) {
		char tmp[lcd_wid + 1];
		char cap[8] = {'\0'};
		char dev[dev_wid + 1];
		u_int64_t size = 0;
		int full = 0;

		if (mnt[i].mpoint[0] == '\0' || disk_is_ignored(mnt[i].mpoint, disk_ignore))
			continue;

		// Truncate long mountpoint paths from left with "-" prefix
		if (strlen(mnt[i].mpoint) > dev_wid)
			snprintf(dev, sizeof(dev), "-%s",
				 (mnt[i].mpoint) + (strlen(mnt[i].mpoint) - (dev_wid - 1)));
		else
			snprintf(dev, sizeof(dev), "%s", mnt[i].mpoint);

		// Calculate bar fill: (used_blocks / total_blocks) × bar_width_in_pixels
		full = !mnt[i].blocks ? gauge_scale
				      : gauge_scale * (u_int64_t)(mnt[i].blocks - mnt[i].bfree) /
					    mnt[i].blocks;

		size = (u_int64_t)mnt[i].bsize * mnt[i].blocks;
		sprintf_memory(cap, (double)size, 1);

		// Create widgets for new filesystems (dynamic widget management)
		if (i_widget >= num_disks) {
			sock_printf(sock, "widget_add D s%i string -in f\n", i_widget);
			sock_printf(sock, "widget_add D h%i hbar -in f\n", i_widget);
		}

		// Adaptive format: wide displays show capacity, compact don't
		if (lcd_wid >= 20) {
			snprintf(tmp, sizeof(tmp), "%-*s %6s E%*sF", dev_wid, dev, cap, gauge_wid,
				 "");
		} else {
			snprintf(tmp, sizeof(tmp), "%-*s E%*sF", dev_wid, dev, gauge_wid, "");
		}

		sock_printf(sock, "widget_set D s%i 1 %i {%s}\n", i_widget, i_widget + 1, tmp);
		sock_printf(sock, "widget_set D h%i %i %i %i\n", i_widget, hbar_pos, i_widget + 1,
			    full);
		i_widget++;
	}

	// Clean up widgets for unmounted filesystems
	for (i = i_widget; i < num_disks; i++) {
		sock_printf(sock, "widget_del D s%i\n", i);
		sock_printf(sock, "widget_del D h%i\n", i);
	}
	num_disks = i_widget;

	// Update frame height to match number of displayed filesystems
	sock_printf(sock, "widget_set D f 1 2 %i %i %i %i v 12\n", lcd_wid, lcd_hgt, lcd_wid,
		    num_disks);

	return 0;
}
