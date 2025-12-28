// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/screenlist.h
 * \brief Screen list management interface
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 * \features
 * - Screen list initialization and shutdown
 * - Screen addition and removal
 * - Automatic screen rotation control
 * - Priority-based screen scheduling
 * - Current screen tracking
 * - Manual screen navigation
 *
 * \usage
 * - Central screen management for the LCDproc server
 * - Screen switching and rotation logic
 * - Integration with client screen management
 * - Display timing and priority handling
 * - Function prototypes for screen list operations
 *
 * \details Provides interface for managing the global screen list and
 * controlling screen rotation and display scheduling. Handles screen
 * switching, priority-based ordering, and automatic rotation.
 */

#ifndef SCREENLIST_H
#define SCREENLIST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shared/defines.h"

/** \name Autorotate Control Constants
 * Screen rotation mode settings
 */
///@{
#define AUTOROTATE_OFF 0 ///< Automatic screen rotation disabled
#define AUTOROTATE_ON 1	 ///< Automatic screen rotation enabled
///@}

extern int
    autorotate; ///< Global autorotate setting - if enabled, screens will rotate automatically

/**
 * \brief Initializes the screenlist
 * \retval 0 Success
 * \retval <0 Initialization failed
 *
 * \details Sets up the internal screen list data structures
 * and prepares the system for screen management.
 */
int screenlist_init(void);

/**
 * \brief Shuts down the screenlist
 * \retval 0 Success
 * \retval <0 Shutdown failed
 *
 * \details Cleans up all screen list resources and
 * destroys internal data structures.
 */
int screenlist_shutdown(void);

/**
 * \brief Adds a screen to the screenlist
 * \param s Screen to add
 * \retval 0 Success
 * \retval <0 Addition failed
 *
 * \details Adds the screen to the global screen list
 * and makes it available for rotation and display.
 */
int screenlist_add(Screen *s);

/**
 * \brief Removes a screen from the screenlist
 * \param s Screen to remove
 * \retval 0 Success
 * \retval <0 Removal failed
 *
 * \details Removes the screen from the global screen list.
 * Does not destroy the screen itself.
 */
int screenlist_remove(Screen *s);

/**
 * \brief Processes the screenlist
 *
 * \details Processes the screenlist and decides if we need to switch
 * to another screen based on priorities, timeouts, and rotation settings.
 * This function is typically called from the main server loop.
 */
void screenlist_process(void);

/**
 * \brief Switches to another screen
 * \param s Screen to switch to
 *
 * \details Switches to another screen in the proper way and informs
 * clients of the switch. ALWAYS USE THIS FUNCTION TO SWITCH SCREENS.
 * Handles all necessary cleanup and notification procedures.
 */
void screenlist_switch(Screen *s);

/**
 * \brief Returns the currently active screen
 * \retval Screen* Currently active screen
 * \retval NULL No screen is currently active
 *
 * \details Returns a pointer to the screen that is currently
 * being displayed on the LCD.
 */
Screen *screenlist_current(void);

/**
 * \brief Moves to the next screen
 * \retval 0 Success
 * \retval <0 No next screen available
 *
 * \details Advances to the next screen in the rotation order.
 */
int screenlist_goto_next(void);

/**
 * \brief Moves to the previous screen
 * \retval 0 Success
 * \retval <0 No previous screen available
 *
 * \details Goes back to the previous screen in the rotation order.
 */
int screenlist_goto_prev(void);

#endif
