// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/iface.h
 * \brief Network interface monitoring function prototypes for lcdproc client
 * \author Luis Llorente Campo
 * \author Stephan Skrodzki, Andrew Foss, M. Dolze, Peter Marschall
 * \date 2002-2006
 *
 * \features
 * - **iface_screen()**: Main network interface monitoring screen
 * - **initialize_speed_screen()**: Setup speed monitoring display
 * - **initialize_transfer_screen()**: Setup transfer statistics display
 * - **format_value()**: Format network values with appropriate units
 * - **actualize_*_screen()**: Update display widgets with current data
 * - Multi-interface support (up to MAX_INTERFACES)
 * - Configurable units (bytes, bits, packets)
 * - Speed and transfer monitoring modes
 * - Time-based activity tracking
 * - Adaptive display formatting
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call iface_screen() to display network interface statistics
 * - Used by the main lcdproc screen rotation system
 * - Configure interfaces and units via lcdproc configuration
 *
 * \details
 * Header file providing function prototypes and definitions for
 * network interface monitoring in the lcdproc client. Originally imported
 * from netlcdclient.h, this module supports monitoring of multiple network
 * interfaces with throughput and transfer statistics.
 */

#ifndef IFACE_H
#define IFACE_H

#include "machine.h"

/** \brief Maximum number of network interfaces to monitor simultaneously */
#define MAX_INTERFACES 3

/** \brief Global array containing interface information for all monitored interfaces
 *
 * \details Shared between iface.c and other modules. Stores state and statistics
 * for up to MAX_INTERFACES network interfaces being monitored.
 */
extern IfaceInfo iface[MAX_INTERFACES];

/**
 * \brief Display network interface monitoring screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \details Main function for displaying network interface usage information
 * including upload/download speeds and transfer totals. Supports both single
 * and multi-interface modes with adaptive layouts.
 */
int iface_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Read interface statistics from system.
 * \param interface Pointer to interface info structure to populate
 * \retval 0 Statistics read successfully
 * \retval -1 Error reading statistics
 *
 * \details Reads network interface statistics from /proc/net/dev or
 * equivalent system interface and populates the interface structure.
 */
int get_iface_stats(IfaceInfo *interface);

/**
 * \brief Initialize speed monitoring screen with widgets.
 *
 * \details Creates the speed monitoring screen and adds all necessary widgets
 * for displaying network interface throughput. Supports both single and
 * multi-interface modes with different layouts based on LCD dimensions.
 */
void initialize_speed_screen(void);

/**
 * \brief Initialize transfer statistics screen with widgets.
 *
 * \details Creates the transfer statistics screen and adds all necessary
 * widgets for displaying cumulative network transfer data. Shows total
 * bytes transferred per interface since system startup.
 */
void initialize_transfer_screen(void);

/**
 * \brief Format time value for display.
 * \param buff Pointer to buffer for storing formatted time string
 * \param last_online Time value to format
 *
 * \details Formats a time value into a human-readable string for display.
 * Shows either time of day (if within 24 hours) or date (if older).
 */
void get_time_string(char *buff, time_t last_online);

/**
 * \brief Format network value with appropriate unit scaling.
 * \param buff Pointer to target buffer for formatted string
 * \param value Number of bytes to format
 * \param unit String describing the unit ("B" for bytes, "b" for bits, "pkt" for packets)
 *
 * \details Formats network values with appropriate magnitude scaling
 * (K, M, G, T) and unit conversion (bytes to bits if needed).
 */
void format_value(char *buff, double value, char *unit);

/**
 * \brief Format network value for multi-interface display mode.
 * \param buff Pointer to target buffer for formatted string
 * \param value Number of bytes to format
 * \param unit String describing the unit ("B" for bytes, "b" for bits, "pkt" for packets)
 *
 * \details Compact version of format_value() optimized for multi-interface
 * mode where space is limited. Uses shorter formatting rules.
 */
void format_value_multi_interface(char *buff, double value, char *unit);

/**
 * \brief Update speed monitoring screen with current data.
 * \param iface Pointer to interface data structure
 * \param interval Time interval since last update (in seconds)
 * \param index Interface index in the display
 *
 * \details Updates the speed monitoring widgets with current throughput
 * calculations based on the time interval and interface statistics.
 */
void actualize_speed_screen(IfaceInfo *iface, unsigned int interval, int index);

/**
 * \brief Update transfer statistics screen with current data.
 * \param iface Pointer to interface data structure
 * \param index Interface index in the display
 *
 * \details Updates the transfer statistics widgets with cumulative
 * transfer totals for the specified interface.
 */
void actualize_transfer_screen(IfaceInfo *iface, int index);

#endif
