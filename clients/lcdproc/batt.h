// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/batt.h
 * \brief Header file for battery status display screen
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - Battery status display screen interface
 * - APM (Advanced Power Management) information access
 * - AC power status monitoring
 * - Battery charge level display
 * - Adaptive layouts for different LCD sizes
 * - Integration with lcdproc screen system
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call battery_screen() to display battery status information
 * - Used by the main lcdproc screen rotation system
 *
 * \details This header declares the battery status screen function for
 * the lcdproc client. The battery screen displays APM (Advanced Power
 * Management) information including AC power status, battery charge level,
 * and charging state with adaptive layouts for different LCD sizes.
 */

#ifndef BATT_H
#define BATT_H

/**
 * \brief Display battery status screen with APM information.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Implements the battery status screen that shows AC power state,
 * battery charge level, and charging status. The screen layout adapts based
 * on LCD dimensions (2-line vs 4-line modes) and includes visual battery
 * gauge representation for 4-line displays.
 */
int battery_screen(int rep, int display, int *flags_ptr);

#endif
