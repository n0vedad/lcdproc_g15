// SPDX-License-Identifier: GPL-2.0+
/*
 * Mock hidraw library for testing G-Series device detection
 *
 * Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
 */

#ifndef MOCK_HIDRAW_LIB_H
#define MOCK_HIDRAW_LIB_H

#include <asm/types.h>
#include <linux/hidraw.h>
#include <linux/input.h>

#define LIB_HIDRAW_DESC_HDR_SZ 16

/* Mock device definitions for testing */
struct mock_device_info {
	unsigned short vendor_id;
	unsigned short product_id;
	const char *name;
	int expected_rgb_support;
	int should_fail_open;
};

/* Test device database */
extern struct mock_device_info test_devices[];
extern int num_test_devices;

/* Mock handle structure (simplified for testing) */
struct lib_hidraw_handle {
	int fd;
	unsigned short current_product_id;
	const struct lib_hidraw_id *ids;
};

struct lib_hidraw_id {
	struct hidraw_devinfo devinfo;
	unsigned char descriptor_header[LIB_HIDRAW_DESC_HDR_SZ];
};

/* Mock API functions */
struct lib_hidraw_handle *lib_hidraw_open(const struct lib_hidraw_id *ids);
void lib_hidraw_send_output_report(struct lib_hidraw_handle *handle,
				   unsigned char *data,
				   int count);
int lib_hidraw_send_feature_report(struct lib_hidraw_handle *handle,
				   unsigned char *data,
				   int count);
void lib_hidraw_close(struct lib_hidraw_handle *handle);
unsigned short lib_hidraw_get_product_id(struct lib_hidraw_handle *handle);

/* Test control functions */
void mock_set_current_device(unsigned short product_id);
void mock_set_device_failure(int should_fail);
void mock_reset_state(void);
int mock_get_rgb_commands_sent(void);
int mock_get_feature_reports_sent(void);
void mock_increment_rgb_commands(void);

#endif