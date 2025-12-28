// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/hidraw_lib.h
 * \brief Header file for HID raw device utility library
 * \author Hans de Goede
 * \author n0vedad
 * \date 2017-2025
 *
 *
 * \features
 * - Header file for HID raw device utility library supporting Linux hidraw subsystem
 * - Device identification and opening by USB vendor/product IDs
 * - Output report transmission for display data and device control
 * - Feature report transmission for advanced device configuration
 * - USB descriptor matching for multi-interface devices
 * - Automatic device discovery in /dev filesystem
 * - Opaque handle management for device connections
 * - Cross-platform HID communication abstraction
 * - Support for multiple device types and interfaces
 *
 * \usage
 * - Used by LCDd drivers requiring HID raw device communication
 * - Primary interface for Logitech G15/G510 keyboard LCD drivers
 * - Supports any HID device accessible via Linux hidraw interface
 * - Device identification through vendor/product ID matching
 * - Handle-based API for safe device management
 * - Used for sending LCD display data via output reports
 * - Used for RGB backlight control via feature reports
 * - Compatible with multi-interface USB devices
 *
 * \details Header file for HID raw device utility library providing
 * abstraction layer for Linux hidraw subsystem communication.
 */

#ifndef HIDRAW_LIB_H
#define HIDRAW_LIB_H

#include <asm/types.h>
#include <linux/hidraw.h>
#include <linux/input.h>

/** \brief Size of HID report descriptor header
 *
 * \details Length of HID descriptor header used for device identification.
 * Used to match devices in lib_hidraw_id table during device enumeration.
 */
#define LIB_HIDRAW_DESC_HDR_SZ 16

/**
 * \brief Forward declaration of HID raw device handle
 *
 * \details Opaque handle structure for managing HID raw device connections.
 * The actual structure is defined in the implementation file.
 */
struct lib_hidraw_handle;

/**
 * \brief HID device identification structure
 *
 * \details Structure used to identify and match HID raw devices.
 * An entry entirely filled with zeros terminates the list of IDs.
 */
struct lib_hidraw_id {
	// Device information (bus, vendor, product)
	struct hidraw_devinfo devinfo; // Device information (bus, vendor, product)
	/**
	 * \note Optional descriptor header for multi-interface devices.
	 * May be used on devices with multiple USB interfaces to
	 * pick the right interface.
	 */
	unsigned char descriptor_header[LIB_HIDRAW_DESC_HDR_SZ];
};

/**
 * \brief Open a HID raw device matching the provided IDs
 * \param ids Array of device IDs to match against (terminated by zero entry)
 * \return Handle to the opened device, or NULL on failure
 *
 * \details Searches for and opens the first HID raw device that matches
 * any of the provided device IDs. The returned handle must be closed
 * with lib_hidraw_close() when no longer needed.
 */
struct lib_hidraw_handle *lib_hidraw_open(const struct lib_hidraw_id *ids);

/**
 * \brief Send an output report to the HID device
 * \param handle Device handle from lib_hidraw_open()
 * \param data Output report data to send
 * \param count Number of bytes to send
 *
 * \details Sends an output report to the HID device. If the device
 * is disconnected, attempts to reconnect automatically.
 */
void lib_hidraw_send_output_report(struct lib_hidraw_handle *handle, unsigned char *data,
				   int count);

/**
 * \brief Send a feature report to the HID device
 * \param handle Device handle from lib_hidraw_open()
 * \param data Feature report data to send
 * \param count Number of bytes to send
 * \retval 0 Success
 * \retval -1 Error
 *
 * \details Sends a feature report to the HID device using ioctl.
 * If the device is disconnected, attempts to reconnect automatically.
 */
int lib_hidraw_send_feature_report(struct lib_hidraw_handle *handle, unsigned char *data,
				   int count);

/**
 * \brief Close a HID raw device handle
 * \param handle Device handle from lib_hidraw_open()
 *
 * \details Closes the device file descriptor and frees the handle.
 * The handle must not be used after calling this function.
 */
void lib_hidraw_close(struct lib_hidraw_handle *handle);

/**
 * \brief Get the USB product ID of the device
 * \param handle Device handle from lib_hidraw_open()
 * \return USB product ID, or 0 on error
 *
 * \details Retrieves the USB product ID from the device information.
 * Useful for determining device capabilities or variants.
 */
unsigned short lib_hidraw_get_product_id(struct lib_hidraw_handle *handle);

#endif
