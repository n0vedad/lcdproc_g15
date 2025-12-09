// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/linux_input.h
 * \brief Header file for Linux input subsystem driver
 * \author n0vedad
 * \date 2025
 *
 *
 * \features
 * - Header file for LCDd driver supporting Linux input subsystem integration
 * - Device identification by path (/dev/input/eventX) or device name matching
 * - Configurable key mappings for input event translation to LCDd key names
 * - Automatic device reconnection on connection loss or device unplugging
 * - Non-blocking input reading with event queue processing
 * - Support for multiple event devices with unified input handling
 * - Linux evdev interface integration for kernel input events
 * - Device capability detection and filtering
 * - Key press and release event processing
 * - Input device enumeration and selection
 *
 * \usage
 * - Used by LCDd server when "linuxInput" driver is specified in configuration
 * - Primary usage for keyboard input from USB/PS2/Bluetooth keyboards
 * - G-Key macro input from gaming keyboards (Logitech, Razer, etc.)
 * - Custom input device integration for specialized hardware
 * - Event-based input handling with configurable key name mappings
 * - Configuration via Device and DeviceName options in LCDd.conf
 * - Automatic device discovery when DeviceName is specified
 * - Fallback device selection when primary device is unavailable
 *
 * \details Header file for LCDd driver reading input events from Linux
 * input subsystem providing keyboard and input device support.
 */

#ifndef LINUX_INPUT_H
#define LINUX_INPUT_H

#include "lcd.h"

/**
 * \brief Initialize the Linux input driver
 * \param drvthis Pointer to driver structure
 * \retval 0 Success
 * \retval -1 Error during initialization
 *
 * \details Initializes the Linux input driver by opening the specified
 * input device and setting up key mappings from configuration.
 */
MODULE_EXPORT int linuxInput_init(Driver *drvthis);

/**
 * \brief Close the Linux input driver and clean up resources
 * \param drvthis Pointer to driver structure
 *
 * \details Performs cleanup by closing input device file descriptor,
 * freeing button mapping list, and releasing allocated memory.
 */
MODULE_EXPORT void linuxInput_close(Driver *drvthis);

/**
 * \brief Read the next input event from Linux input device
 * \param drvthis Pointer to driver structure
 * \return String representation of the key, or NULL if nothing available
 *
 * \details Reads input events from the Linux input device and converts
 * them to LCDd key names using configured key mappings or default mappings.
 */
MODULE_EXPORT const char *linuxInput_get_key(Driver *drvthis);

#endif
