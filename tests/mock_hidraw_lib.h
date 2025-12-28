// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/mock_hidraw_lib.h
 * \brief Mock hidraw library for testing G-Series device detection
 * \author n0vedad <https://github.com/n0vedad/>
 * \date 2025
 *
 * \features
 * - Mock device structures and type definitions
 * - Test device database with various G-Series models
 * - API function declarations for hidraw simulation
 * - Test control functions for mock behavior management
 * - State tracking and verification interfaces
 *
 * \usage
 * - Include in test files requiring G-Series device simulation
 * - Use lib_hidraw_* functions as drop-in replacement for real hidraw
 * - Control mock behavior with mock_set_* functions during tests
 *
 * \details Header file defining mock hidraw library API for testing G-Series keyboards
 * without requiring real hardware devices. Provides complete simulation of hidraw
 * functionality including device detection, RGB command processing, feature reports,
 * and error condition simulation for comprehensive testing coverage.
 */

#ifndef MOCK_HIDRAW_LIB_H
#define MOCK_HIDRAW_LIB_H

#include <asm/types.h>
#include <linux/hidraw.h>
#include <linux/input.h>

#define LIB_HIDRAW_DESC_HDR_SZ 16

/**
 * \brief Mock device information structure
 * \details Contains all information needed to simulate a G-Series device
 */
struct mock_device_info {
	unsigned short vendor_id;  // USB vendor ID
	unsigned short product_id; // USB product ID
	const char *name;	   // Device name
	int expected_rgb_support;  // Whether device supports RGB
	int should_fail_open;	   // Simulate open failure
};

// Test device database
extern struct mock_device_info test_devices[];
extern int num_test_devices; // Number of test devices

/**
 * \brief Mock hidraw handle structure
 * \details Simplified handle structure for testing hidraw operations
 */
struct lib_hidraw_handle {
	int fd;				   // File descriptor
	unsigned short current_product_id; // Current device product ID
	const struct lib_hidraw_id *ids;   // Device ID structure
};

/**
 * \brief Device identification structure
 * \details Contains device information and HID descriptor data
 */
struct lib_hidraw_id {
	struct hidraw_devinfo devinfo;				 // Device info
	unsigned char descriptor_header[LIB_HIDRAW_DESC_HDR_SZ]; // HID descriptor
};

/**
 * \name Mock API functions
 * \brief Mock implementations of hidraw library functions
 * \details API functions that mock the hidraw library for testing purposes
 */

/**
 * \brief Open hidraw device
 * \param ids Device identification structure
 * \retval handle Pointer to hidraw handle on success
 * \retval NULL Device open failed or device not found
 *
 * \details Opens a mock hidraw device based on the provided device identification.
 * Returns a handle that can be used for subsequent hidraw operations.
 */
struct lib_hidraw_handle *lib_hidraw_open(const struct lib_hidraw_id *ids);

/**
 * \brief Send output report to device
 * \param handle Device handle
 * \param data Report data to send
 * \param count Number of bytes to send
 *
 * \details Sends an output report to the mock hidraw device. Used for
 * controlling device features like RGB backlighting.
 */
void lib_hidraw_send_output_report(struct lib_hidraw_handle *handle, unsigned char *data,
				   int count);

/**
 * \brief Send feature report to device
 * \param handle Device handle
 * \param data Feature report data
 * \param count Number of bytes to send
 * \retval >=0 Number of bytes sent
 * \retval -1 Feature report send failed
 *
 * \details Sends a feature report to the mock hidraw device for configuration
 * and control purposes.
 */
int lib_hidraw_send_feature_report(struct lib_hidraw_handle *handle, unsigned char *data,
				   int count);

/**
 * \brief Close hidraw device
 * \param handle Device handle to close
 *
 * \details Closes the mock hidraw device and frees associated resources.
 */
void lib_hidraw_close(struct lib_hidraw_handle *handle);

/**
 * \brief Get product ID from device handle
 * \param handle Device handle
 * \retval product_id USB product ID of the device
 * \retval 0 Invalid handle or device not found
 *
 * \details Retrieves the USB product ID from the mock device handle.
 */
unsigned short lib_hidraw_get_product_id(struct lib_hidraw_handle *handle);

/**
 * \name Test control functions
 * \brief Functions to control mock behavior during testing
 * \details Internal functions for controlling mock state and behavior
 */

/**
 * \brief Set current mock device
 * \param product_id USB product ID of device to simulate
 *
 * \details Sets the active mock device for subsequent hidraw operations.
 */
void mock_set_current_device(unsigned short product_id);

/**
 * \brief Set device failure mode
 * \param should_fail 1 to simulate device failures, 0 for normal operation
 *
 * \details Controls whether the mock device should simulate failure conditions
 * for testing error handling.
 */
void mock_set_device_failure(int should_fail);

/**
 * \brief Reset mock state to defaults
 *
 * \details Resets all mock counters and state variables to their initial values.
 */
void mock_reset_state(void);

/**
 * \brief Get number of RGB commands sent
 * \retval count Number of RGB output reports sent to mock device
 *
 * \details Returns the count of RGB backlight commands sent for verification
 * in test cases.
 */
int mock_get_rgb_commands_sent(void);

/**
 * \brief Get number of feature reports sent
 * \retval count Number of feature reports sent to mock device
 *
 * \details Returns the count of feature reports sent for verification
 * in test cases.
 */
int mock_get_feature_reports_sent(void);

/**
 * \brief Increment RGB command counter
 *
 * \details Internal function to increment the RGB command counter when
 * RGB output reports are processed.
 */
void mock_increment_rgb_commands(void);

#endif
