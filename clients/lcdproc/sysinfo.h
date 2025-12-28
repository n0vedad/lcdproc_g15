// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/sysinfo.h
 * \brief Header for comprehensive system information display screen
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - System information screen function declaration
 * - Optimized for G510s 20x4 LCD displays
 *
 * \usage
 * - Include in main.c for screen registration
 * - Call sysinfo_screen() from screen rotation system
 *
 * \details Declares the system information screen function that displays
 * hostname, uptime, date/time, and resource usage in a compact 4-line format.
 */

#ifndef _lcdproc_sysinfo_h_
#define _lcdproc_sysinfo_h_

/**
 * \brief Display comprehensive system information screen
 * \param rep Repetition counter (incremented on each call)
 * \param display Flag indicating whether to update display (0=no, 1=yes)
 * \param flags_ptr Pointer to screen state flags (used for initialization)
 * \retval 0 Success
 *
 * \details Displays a 4-line system information summary:
 * - Line 1: Hostname
 * - Line 2: System uptime
 * - Line 3: Current date and time
 * - Line 4: CPU load, RAM usage, and GPU temperature
 *
 * Uses two-phase initialization pattern to handle server communication properly.
 * First call sends screen_add, subsequent calls create widgets and update data.
 */
int sysinfo_screen(int rep, int display, int *flags_ptr);

#endif
