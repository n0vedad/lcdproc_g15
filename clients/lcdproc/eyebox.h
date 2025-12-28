// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/eyebox.h
 * \brief EyeboxOne device support functions for lcdproc client
 * \author Cedric TESSIER (aka NeZetiC)
 * \date 2006
 *
 * \features
 * - **eyebox_screen()**: Update LED indicators with system metrics
 * - **eyebox_clear()**: Reset and clear LED indicators
 * - LED-based CPU usage indicator (Bar ID 2)
 * - LED-based RAM usage indicator (Bar ID 1)
 * - Rolling average calculations for smooth transitions
 * - Clean shutdown with LED reset functionality
 * - Integration with lcdproc monitoring systems
 * - EyeboxOne specific command protocol support
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call eyebox_screen() to update LED indicators with system status
 * - Call eyebox_clear() during shutdown to reset device state
 * - Used for specialized EyeboxOne LCD device support
 *
 * \details Header file providing function prototypes for EyeboxOne device
 * support in the lcdproc client. The EyeboxOne is a special LCD device
 * that includes controllable LED indicators for system status display
 * using a specialized command protocol.
 */

#ifndef EYEBOX_H
#define EYEBOX_H

/**
 * \brief Update EyeboxOne LED indicators with system status.
 * \param display Character identifier for the display screen
 * \param init Initialization flag (0=initialize widgets, 1=update display)
 * \retval 0 Always returns success
 *
 * \details Controls the EyeboxOne LED indicators to show real-time system
 * status. Updates CPU usage (Bar ID 2) and memory usage (Bar ID 1) LEDs
 * based on current system metrics. Uses rolling averages for smooth
 * LED brightness transitions.
 */
int eyebox_screen(char display, int init);

/**
 * \brief Clear and reset EyeboxOne LED indicators.
 *
 * \details Performs cleanup of the EyeboxOne LED indicators by turning off
 * all LED bars and displaying a reset message. Called during program
 * shutdown to ensure the device returns to a clean state.
 */
void eyebox_clear(void);

#endif
