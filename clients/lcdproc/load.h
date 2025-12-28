// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/load.h
 * \brief System load average monitoring function prototypes for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **xload_screen()**: Display load average histogram
 * - Real-time scrolling histogram display
 * - Configurable load thresholds for backlight control
 * - Automatic scaling based on maximum load
 * - Visual load indicators with vertical bars
 * - Adaptive layout for different LCD sizes
 * - Load history tracking and smoothing
 * - LOAD_MAX and LOAD_MIN threshold definitions
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call xload_screen() to display system load histogram
 * - Used by the main lcdproc screen rotation system
 * - Configure load thresholds via lcdproc configuration
 *
 * \details
 * Header file providing function prototypes and constants for
 * system load average monitoring in the lcdproc client. Contains definitions
 * for the xload-style histogram display that shows real-time load averages.
 */

#ifndef LOAD_H
#define LOAD_H

/**
 * \brief Default maximum load threshold for backlight control.
 *
 * \details Load values above this threshold will trigger blinking backlight.
 * Can be overridden in configuration file.
 */
#ifndef LOAD_MAX
#define LOAD_MAX 1.3
#endif

/**
 * \brief Default minimum load threshold for backlight control.
 *
 * \details Load values below this threshold will turn off backlight.
 * Can be overridden in configuration file.
 */
#ifndef LOAD_MIN
#define LOAD_MIN 0.05
#endif

/**
 * \brief Display system load average as a scrolling histogram.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval BACKLIGHT_ON Normal load level
 * \retval BACKLIGHT_OFF Low load level (below LOAD_MIN threshold)
 * \retval BLINK_ON High load level (above LOAD_MAX threshold)
 *
 * \details Displays system load average as a real-time scrolling histogram
 * similar to xload. The graph scrolls from right to left with automatic
 * scaling and configurable thresholds for backlight control based on
 * system load levels.
 */
int xload_screen(int rep, int display, int *flags_ptr);

#endif
