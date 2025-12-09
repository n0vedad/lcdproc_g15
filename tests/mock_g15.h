// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/mock_g15.h
 * \brief G15 Hardware Mock for Integration Tests - Header
 * \author n0vedad <https://github.com/n0vedad/>
 * \date 2025
 *
 * \features
 * - Mock command type definitions for G15 simulation
 * - API functions for device initialization and control
 * - RGB backlight simulation interface
 * - Error simulation and state management
 * - Integration test control functions
 *
 * \usage
 * - Include this header in integration tests
 * - Use integration_mock_* functions to control mock behavior
 * - Initialize devices before testing G15 driver functionality
 *
 * \details API definitions for G15 hardware mock server used in integration tests.
 * Bridges the unit test mocks with integration test environment by providing
 * a comprehensive command interface for device simulation, RGB backlight control,
 * error injection, and state management operations.
 */

#ifndef MOCK_G15_H
#define MOCK_G15_H

/**
 * \name Mock Command Types
 * \brief Command types for mock server communication
 *
 * \details Enumeration of all available commands that can be sent to the mock server
 * for controlling G15 device simulation and test execution.
 */
typedef enum {
	MOCK_CMD_INIT_DEVICE = 1, // Initialize device
	MOCK_CMD_SET_RGB,	  // Set RGB backlight
	MOCK_CMD_GET_STATE,	  // Get device state
	MOCK_CMD_SIMULATE_ERROR,  // Simulate error
	MOCK_CMD_RESET_COUNTERS,  // Reset counters
	MOCK_CMD_SHUTDOWN	  // Shutdown server
} mock_command_t;

/**
 * \brief Send a command to the mock server
 * \param cmd Command type to send
 * \param device_id Device identifier
 * \param param1 First parameter
 * \param param2 Second parameter
 * \param param3 Third parameter
 * \param data Optional data string
 * \retval >0 Command result code on success
 * \retval -1 Communication error or command failure
 *
 * \details Sends a command to the mock server and returns the response.
 * This is the primary communication interface for controlling mock device
 * behavior during integration tests. All other API functions use this
 * function internally for server communication.
 */
int integration_mock_send_command(mock_command_t cmd, int device_id, int param1, int param2,
				  int param3, const char *data);

/**
 * \brief Initialize a mock G15 device
 * \param device_id Device ID to initialize
 * \retval 1 Device initialization successful
 * \retval 0 Device initialization failed
 *
 * \details Initializes a mock G15 device with the specified ID. This must be
 * called before performing any device operations in integration tests.
 */
int integration_mock_init_device(int device_id);

/**
 * \brief Set RGB backlight color
 * \param r Red component (0-255)
 * \param g Green component (0-255)
 * \param b Blue component (0-255)
 * \retval >0 RGB command count after successful execution
 * \retval -1 RGB command failed
 *
 * \details Sets the RGB backlight color for the mock G15 device. Returns the
 * total number of RGB commands that have been executed, which can be used
 * for testing command counting functionality.
 */
int integration_mock_set_rgb(int r, int g, int b);

/**
 * \brief Get the current RGB command count
 * \retval >=0 Number of RGB commands executed
 * \retval 0 No commands executed or query failed
 *
 * \details Retrieves the current count of RGB backlight commands that have
 * been executed by the mock server. Useful for verifying command execution
 * in integration tests.
 */
int integration_mock_get_rgb_count(void);

/**
 * \brief Simulate a device error condition
 * \retval 0 Error simulation activated successfully
 * \retval -1 Error simulation failed
 *
 * \details Forces the mock device into an error state for testing error
 * handling and recovery mechanisms. The device will remain in error state
 * until reset or reinitialized.
 */
int integration_mock_simulate_error(void);

/**
 * \brief Reset all mock counters and state
 * \retval 1 Reset operation successful
 * \retval 0 Reset operation failed
 *
 * \details Resets all mock server counters and device state to initial values.
 * This includes RGB command counters, device error states, and any other
 * accumulated state information.
 */
int integration_mock_reset_counters(void);

/**
 * \brief Shutdown the mock server
 * \retval 1 Shutdown command sent successfully
 * \retval 0 Shutdown command failed
 *
 * \details Sends a shutdown command to the mock server, causing it to
 * gracefully terminate. This should be called at the end of integration
 * test runs to clean up server resources.
 */
int integration_mock_shutdown_server(void);

#endif
