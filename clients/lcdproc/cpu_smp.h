// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/cpu_smp.h
 * \brief Header file for SMP CPU usage display screen
 * \author J Robert Ray, Peter Marschall
 * \date 2000-2007
 *
 * \features
 * - SMP CPU usage display screen interface
 * - Individual processor core monitoring
 * - Real-time CPU usage visualization
 * - Adaptive layouts for different LCD sizes
 * - Support for multi-processor systems
 * - Integration with lcdproc screen system
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call cpu_smp_screen() to display SMP CPU usage information
 * - Used by the main lcdproc screen rotation system
 * - Automatically detects number of CPU cores
 *
 * \details This header declares the SMP CPU usage screen function for
 * the lcdproc client. The screen provides real-time monitoring of individual
 * processor core usage in multi-processor systems with adaptive layouts
 * based on the number of CPUs and LCD dimensions.
 */

#ifndef CPU_SMP_H
#define CPU_SMP_H

/**
 * \brief Display SMP CPU usage screen with individual core monitoring.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Implements CPU usage monitoring for multi-processor systems
 * with adaptive layouts. Shows individual CPU core usage as horizontal
 * bar graphs with either one CPU per line or two CPUs per line depending
 * on the number of cores and LCD dimensions.
 */
int cpu_smp_screen(int rep, int display, int *flags_ptr);

#endif
