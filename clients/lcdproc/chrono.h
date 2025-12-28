// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/chrono.h
 * \brief Header file for time and date display screens
 * \author William Ferrell, Selene Scriven, David Glaude
 * \date 1999-2006
 *
 * \features
 * - Multiple time and date display screen interfaces
 * - Classic clock screen with time and date
 * - System uptime and OS version display
 * - Comprehensive time screen with system information
 * - Large digital clock using numeric widgets
 * - Minimal centered time display
 * - Adaptive layouts for different LCD sizes
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call specific screen functions for different time display modes
 * - Used by the main lcdproc screen rotation system
 * - Configure display formats via lcdproc configuration
 *
 * \details This header declares the time and date display screen functions
 * for the lcdproc client. These screens provide various layouts and formats
 * for displaying current time, date, system uptime, and related information
 * with adaptive layouts for different LCD sizes.
 */

#ifndef CHRONO_H
#define CHRONO_H

/**
 * \brief Display classic time and date screen with optional title.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 */
int clock_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display system uptime and OS version information screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 */
int uptime_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display comprehensive time, date, and system information screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 */
int time_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display large digital clock using numeric widgets.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 */
int big_clock_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display minimal centered time with configurable format.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 */
int mini_clock_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Format current time according to specified format string.
 * \param buffer Output buffer for formatted time string
 * \param bufsize Size of output buffer
 * \param format strftime format string (e.g., "%H:%M:%S" or "%b %d %Y")
 *
 * \details Formats the current system time using strftime format specifiers.
 * Sets buffer to empty string if format is NULL or formatting fails.
 */
void get_formatted_time(char *buffer, size_t bufsize, const char *format);

#endif
