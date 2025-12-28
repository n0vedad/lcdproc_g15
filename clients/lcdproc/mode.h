// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/mode.h
 * \brief Screen mode management and credits display functions for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **mode_init()**: Mode system initialization wrapper
 * - **mode_close()**: Mode system cleanup wrapper
 * - **update_screen()**: Screen update coordination with backlight control
 * - **credit_screen()**: Credits screen with contributor information
 * - Machine-dependent initialization wrappers
 * - Backlight state management
 * - Screen update timing coordination
 * - Adaptive credit display layouts
 * - Optional EyeboxOne device support
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call mode_init() for system initialization
 * - Use update_screen() for screen updates with backlight management
 * - Call credit_screen() for contributor display
 * - Used by the main lcdproc screen management system
 *
 * \details
 * Header file for screen mode management functionality in the lcdproc
 * client. Provides function declarations for mode initialization, cleanup,
 * screen updates, and the credits display screen.
 */

#ifndef MODE_H
#define MODE_H

/**
 * \brief Initialize mode-specific subsystems.
 * \retval 0 Initialization successful
 *
 * \details Initializes machine-dependent subsystems required for
 * screen mode operation. This function serves as a wrapper around
 * machine_init() to provide a consistent interface for mode
 * initialization.
 */
int mode_init(void);

/**
 * \brief Clean up mode subsystems on exit.
 *
 * \details Performs cleanup of machine-dependent resources and
 * subsystems. This function serves as a wrapper around machine_close()
 * to ensure proper resource cleanup during program termination.
 */
void mode_close(void);

/**
 * \brief Update screen display and manage backlight state.
 * \param m Pointer to screen mode structure containing function and flags
 * \param display Flag indicating if screen should be updated even if not visible
 * \retval BACKLIGHT_OFF Low activity level, backlight should be off
 * \retval BACKLIGHT_ON Normal activity level, backlight should be on
 * \retval BLINK_ON High activity level, backlight should blink
 *
 * \features
 * The function coordinates:
 * - Screen mode function execution
 * - Backlight state transitions
 * - EyeboxOne device updates
 * - Timer and flag management
 *
 * \details Calls the mode-specific screen update function and manages
 * backlight state based on the return value. Also handles EyeboxOne
 * device integration if compiled with LCDPROC_EYEBOXONE support.
 */
int update_screen(ScreenMode *m, int display);

/**
 * \brief Display credits screen with contributor information.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \features
 * - Project version information
 * - Scrolling contributor list
 * - Adaptive layout for different LCD sizes
 * - Frame-based scrolling display
 * - Synchronized with CREDITS file
 * - 4+ line displays: Include descriptive text
 * - Smaller displays: Compact contributor-only view
 *
 * \details Displays a scrolling list of LCDproc contributors and project
 * information. The screen adapts its layout based on LCD dimensions and
 * provides a comprehensive credit listing for all project contributors.
 */
int credit_screen(int rep, int display, int *flags_ptr);

#endif
