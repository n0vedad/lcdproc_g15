// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/cpu.h
 * \brief CPU usage monitoring screen functions for single processor systems
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **CPU Screen**: Detailed numerical breakdown of CPU usage by category
 * - **CPUGraph Screen**: Real-time scrolling histogram of CPU usage
 * - Adaptive layouts for different LCD display sizes
 * - Rolling average calculations for smooth data presentation
 * - User, System, Nice, and Idle time monitoring
 * - Progress bar and histogram visualizations
 * - Single processor system optimization
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call cpu_screen() for detailed numerical CPU usage display
 * - Call cpu_graph_screen() for real-time histogram visualization
 * - Used by the main lcdproc screen rotation system
 *
 * \details Header file providing function prototypes for CPU usage monitoring
 * screens in the lcdproc client. Contains declarations for both detailed
 * numerical CPU usage display and real-time graphical histogram screens
 * optimized for single processor systems.
 */

#ifndef CPU_H
#define CPU_H

/**
 * \brief Display detailed CPU usage screen with numerical breakdown.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Displays CPU usage information with detailed breakdown by category.
 * Shows User, System, Nice, and Idle percentages with adaptive layout based
 * on LCD dimensions. Includes progress bar visualization on larger displays.
 */
int cpu_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display real-time CPU usage histogram screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Shows a scrolling histogram of CPU usage over time using vertical
 * bars. The graph scrolls from right to left, with the newest data appearing
 * on the right edge. Height of bars represents current CPU load percentage.
 */
int cpu_graph_screen(int rep, int display, int *flags_ptr);

#endif
