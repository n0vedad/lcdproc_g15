// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/main.h
 * \brief Main server definitions and global variables interface
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2001
 *
 *
 * \features
 * - Server timing configuration constants (frame rates, intervals)
 * - Global configuration variable declarations for command-line settable options
 * - Version and build information external declarations
 * - Default value constants for all server settings
 * - Driver configuration arrays and limits
 * - Processing frequency definitions for main loop timing
 * - Render lag frame limits and timing parameters
 * - Network binding configuration declarations
 * - User privilege dropping configuration
 * - Unset value definitions for initialization
 *
 * \usage
 * - Include for access to global server state and configuration variables
 * - Reference for configuration variable names in command-line processing
 * - Use timing constants (PROCESS_FREQ, MAX_RENDER_LAG_FRAMES) for main loop
 * - Access default values for fallback configuration
 * - Reference driver arrays for multi-driver support
 * - Use UNSET_INT and UNSET_STR for configuration initialization
 * - Include in modules that need server timing parameters
 * - Reference for version information display
 *
 * \details Interface for main server definitions including global configuration
 * variables, timing parameters, and constants used throughout the LCDd server.
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/**
 * \brief Version information strings
 * \details Global version strings used for display and protocol negotiation
 */
extern char *version;	       /**< LCDd server version string */
extern char *protocol_version; /**< LCDproc protocol version string */
extern char *build_date;       /**< Build date information */

/**
 * \brief Server processing frequency in Hz
 * \details Controls how often messages and keypresses are processed
 */
#define PROCESS_FREQ 32

/**
 * \brief Maximum allowed render lag in frame intervals
 * \details When rendering falls behind by more than this many frames,
 * slowdown occurs rather than trying to catch up
 */
#define MAX_RENDER_LAG_FRAMES 16

/**
 * \brief Global timer counter
 * \details Incremented at render frequency, used for timing and animations.
 * 32 bits at 8Hz will overflow in 2^29 = 5e8 seconds = 17 years.
 */
extern long timer;

/**
 * \brief Configuration variables settable from command line
 * \details Only configuration items that are settable from the command line
 * should be mentioned here. Other settings are read in their respective modules.
 */

/**
 * \brief Network binding configuration
 * \details Network settings for client connections
 */
extern unsigned int bind_port; /**< Port number to bind to */
extern char bind_addr[];       /**< IP address to bind to */

/**
 * \brief Server configuration files and user settings
 * \details Core server configuration parameters
 */
extern char configfile[]; /**< Path to configuration file */
extern char user[];	  /**< User to run as after privilege drop */

/**
 * \brief Frame timing configuration
 * \details Controls render timing (not command line settable but configurable)
 */
extern int frame_interval; /**< Microseconds between render frames */

/**
 * \brief Driver configuration
 * \details Array of driver names and count for multi-driver support
 */
extern char *drivernames[]; /**< Array of driver names to load */
extern int num_drivers;	    /**< Number of drivers in array */

/**
 * \brief Unset value constants
 * \details Special values indicating uninitialized configuration variables
 */
#define UNSET_INT -1	/**< Unset integer value marker */
#define UNSET_STR "\01" /**< Unset string value marker */

#endif
