// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/disk.h
 * \brief Disk usage monitoring screen functions for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **Disk Screen**: Multi-filesystem usage monitoring with scrolling display
 * - Configurable filesystem ignore list
 * - Adaptive layout for different LCD display sizes
 * - Memory-formatted capacity display (KB/MB/GB/TB)
 * - Horizontal bar graphs showing disk usage percentage
 * - Real-time filesystem detection and unmount handling
 * - Mountpoint path truncation for better display fit
 * - Support for both compact and full display formats
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call disk_screen() to display filesystem usage information
 * - Used by the main lcdproc screen rotation system
 * - Configure ignored filesystems via lcdproc configuration
 *
 * \details Header file providing function prototypes for disk usage monitoring
 * screen in the lcdproc client. Contains declaration for the filesystem
 * statistics display that shows mounted filesystem usage information with
 * adaptive layouts and configurable filtering options.
 */

#ifndef DISK_H
#define DISK_H

/**
 * \brief Display disk usage screen with filesystem statistics.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Displays filesystem usage information for all mounted filesystems
 * excluding those in the configured ignore list. Shows mountpoint names,
 * capacity, and usage percentage with horizontal bar graphs. The display
 * scrolls through all filesystems and adapts layout based on LCD width.
 * Supports both compact (16-19 char) and full (20+ char) display formats.
 */
int disk_screen(int rep, int display, int *flags_ptr);

#endif
