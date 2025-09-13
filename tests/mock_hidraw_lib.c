// SPDX-License-Identifier: GPL-2.0+
/*
 * Mock hidraw library implementation for testing G-Series device detection
 *
 * Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
 */

#include "mock_hidraw_lib.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test device database - covers all supported and unsupported models */
struct mock_device_info test_devices[] = {
    {0x046d, 0xc222, "Logitech G15 (Original)", 0, 0},
    {0x046d, 0xc227, "Logitech G15 v2", 0, 0},
    {0x046d, 0xc22d, "Logitech G510", 1, 0},
    {0x046d, 0xc22e, "Logitech G510s", 1, 0},
    {0x046d, 0xc221, "Unknown Logitech Device", 0, 0},
    {0x046d, 0x0000, "Invalid Device", 0, 1}, /* Simuliert Fehlerfall */
};

int num_test_devices = sizeof(test_devices) / sizeof(test_devices[0]);

/* Mock state variables */
static unsigned short current_mock_device = 0xc22e; /* Default: G510s */
static int device_open_should_fail = 0;
static int rgb_commands_sent = 0;
static int feature_reports_sent = 0;

/* Find device info by product ID */
static struct mock_device_info *find_device_info(unsigned short product_id)
{
	for (int i = 0; i < num_test_devices; i++) {
		if (test_devices[i].product_id == product_id) {
			return &test_devices[i];
		}
	}
	return NULL;
}

/* Mock API Implementation */

struct lib_hidraw_handle *lib_hidraw_open(const struct lib_hidraw_id *ids)
{
	struct mock_device_info *device = find_device_info(current_mock_device);

	if (device_open_should_fail || !device || device->should_fail_open) {
		errno = ENODEV;
		return NULL;
	}

	struct lib_hidraw_handle *handle = calloc(1, sizeof(*handle));
	if (!handle) {
		errno = ENOMEM;
		return NULL;
	}

	handle->fd = 1; /* Mock file descriptor */
	handle->current_product_id = current_mock_device;
	handle->ids = ids;

	printf("[MOCK] Opened device: %s (USB ID: 046d:%04x)\n", device->name, device->product_id);

	return handle;
}

void lib_hidraw_send_output_report(struct lib_hidraw_handle *handle, unsigned char *data, int count)
{
	if (!handle || handle->fd == -1) {
		return;
	}

	printf("[MOCK] Output report sent: %d bytes\n", count);
	/* Hier könnten wir LCD-Kommandos verfolgen */
}

int lib_hidraw_send_feature_report(struct lib_hidraw_handle *handle, unsigned char *data, int count)
{
	if (!handle || handle->fd == -1) {
		errno = ENODEV;
		return -1;
	}

	/* Check if this is an RGB command (feature reports starting with 0x06 or 0x07) */
	if (count >= 4 && (data[0] == 0x06 || data[0] == 0x07)) {
		rgb_commands_sent++;
		struct mock_device_info *device = find_device_info(handle->current_product_id);
		printf("[MOCK] RGB command sent to %s (R=%d, G=%d, B=%d)\n",
		       device ? device->name : "Unknown",
		       data[1],
		       data[2],
		       data[3]);
	}

	feature_reports_sent++;
	return count;
}

void lib_hidraw_close(struct lib_hidraw_handle *handle)
{
	if (handle) {
		printf("[MOCK] Closed device (USB ID: 046d:%04x)\n", handle->current_product_id);
		free(handle);
	}
}

unsigned short lib_hidraw_get_product_id(struct lib_hidraw_handle *handle)
{
	if (!handle || handle->fd == -1) {
		return 0;
	}

	return handle->current_product_id;
}

/* Test control functions */

void mock_set_current_device(unsigned short product_id)
{
	struct mock_device_info *device = find_device_info(product_id);
	current_mock_device = product_id;

	printf("[MOCK] Switched to device: %s (USB ID: 046d:%04x)\n",
	       device ? device->name : "Unknown",
	       product_id);
}

void mock_set_device_failure(int should_fail)
{
	device_open_should_fail = should_fail;
	printf("[MOCK] Device open failure mode: %s\n", should_fail ? "ENABLED" : "DISABLED");
}

void mock_reset_state(void)
{
	current_mock_device = 0xc22e; /* Reset to G510s */
	device_open_should_fail = 0;
	rgb_commands_sent = 0;
	feature_reports_sent = 0;
	printf("[MOCK] State reset to defaults\n");
}

int mock_get_rgb_commands_sent(void) { return rgb_commands_sent; }

int mock_get_feature_reports_sent(void) { return feature_reports_sent; }

void mock_increment_rgb_commands(void) { rgb_commands_sent++; }