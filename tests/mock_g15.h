// SPDX-License-Identifier: GPL-2.0+
/*
 * G15 Hardware Mock for Integration Tests - Header
 * Bridges the unit test mocks with integration test environment
 *
 * Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
 */

#ifndef MOCK_G15_H
#define MOCK_G15_H

/* Mock command types */
typedef enum {
	MOCK_CMD_INIT_DEVICE = 1,
	MOCK_CMD_SET_RGB,
	MOCK_CMD_GET_STATE,
	MOCK_CMD_SIMULATE_ERROR,
	MOCK_CMD_RESET_COUNTERS,
	MOCK_CMD_SHUTDOWN
} mock_command_t;

/* API functions for integration test control */

/*
 * Send a command to the mock server
 * Returns: result code on success, -1 on error
 */
int integration_mock_send_command(
    mock_command_t cmd, int device_id, int param1, int param2, int param3, const char *data);

/*
 * Initialize a mock G15 device
 * device_id: Device ID to initialize
 * Returns: 1 on success, 0 on error
 */
int integration_mock_init_device(int device_id);

/*
 * Set RGB backlight color
 * r, g, b: RGB color values (0-255)
 * Returns: RGB command count on success, -1 on error
 */
int integration_mock_set_rgb(int r, int g, int b);

/*
 * Get the current RGB command count
 * Returns: number of RGB commands sent, or 0 on error
 */
int integration_mock_get_rgb_count(void);

/*
 * Simulate a device error condition
 * Returns: 1 on success, 0 on error
 */
int integration_mock_simulate_error(void);

/*
 * Reset all mock counters and state
 * Returns: 1 on success, 0 on error
 */
int integration_mock_reset_counters(void);

/*
 * Shutdown the mock server
 * Returns: 1 on success, 0 on error
 */
int integration_mock_shutdown_server(void);

#endif /* MOCK_G15_H */