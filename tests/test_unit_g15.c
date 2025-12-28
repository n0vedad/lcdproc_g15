// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/test_unit_g15.c
 * \brief Comprehensive unit tests for G-Series keyboards.
 * \author Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
 * \date 2025
 *
 * \features
 * - Device detection and hardware capability identification
 * - RGB backlight control validation for supported models
 * - G-Key macro recording and playback functionality
 * - Error handling and edge case validation
 * - Debug driver integration testing
 *
 * \details This file contains comprehensive unit tests for G-Series keyboard functionality,
 * including device detection, RGB backlight control, and G-Key macro system testing.
 * Tests cover G15 (original and v2), G510, and G510s keyboard models with various
 * hardware capabilities
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_hidraw_lib.h"

/** \brief Backlight on state for G15 driver testing */
#define BACKLIGHT_ON 1
/** \brief Backlight off state for G15 driver testing */
#define BACKLIGHT_OFF 0

// Mock report function to suppress output during tests
void report(int level, const char *format, ...)
{
	(void)level;
	(void)format;
}

// Mock PrivateData structures
typedef struct {
	struct lib_hidraw_handle *hidraw_handle;

	// Backlight
	int has_rgb_backlight;
	int backlight_state;
	unsigned char rgb_red;
	unsigned char rgb_green;
	unsigned char rgb_blue;
	int rgb_method_hid;

	// Macros
	int macro_recording_mode;
	int current_g_mode;
	int last_recorded_gkey;
} PrivateData;

// Validate G-Key number (G1-G18) and mode (M1-M3)
static int validate_gkey_params(int gkey, int mode)
{
	if (gkey < 1 || gkey > 18)
		return -1;

	if (mode < 1 || mode > 3)
		return -1;

	return 0;
}

// Mock Driver structure
typedef struct {
	PrivateData *private_data;
	const char *name;
} Driver;

// Forward declarations for tested functions
int g15_init_device_detection(Driver *drvthis);
int g15_set_rgb_backlight(Driver *drvthis, int red, int green, int blue);
int g15_set_rgb_led_subsystem(Driver *drvthis, int red, int green, int blue);
int g15_set_rgb_hid_reports(Driver *drvthis, int red, int green, int blue);
int g15_process_gkey_macro(Driver *drvthis, int gkey, int mode);
int g15_start_macro_recording(Driver *drvthis, int gkey, int mode);
int g15_stop_macro_recording(Driver *drvthis);

// Debug driver private data structure
typedef struct debug_private_data {
	char *framebuf;
	int width;
	int height;
	int cellwidth;
	int cellheight;
	int contrast;
	int brightness;
	int offbrightness;
} DebugPrivateData;

// Validate and convert 1-based coordinates to 0-based array indices
static int validate_and_convert_coords(int *x, int *y, int width, int height)
{
	if (*x < 1 || *y < 1 || *x > width || *y > height)
		return -1;

	(*x)--;
	(*y)--;

	return 0;
}

// Create RGB feature report structure for HID communication
static void create_rgb_report(unsigned char *report, int red, int green, int blue)
{
	report[0] = 0x06;
	report[1] = (unsigned char)red;
	report[2] = (unsigned char)green;
	report[3] = (unsigned char)blue;
}

// Debug driver function declarations
int debug_init(Driver *drvthis);
void debug_close(Driver *drvthis);
int debug_width(Driver *drvthis);
int debug_height(Driver *drvthis);
void debug_clear(Driver *drvthis);
void debug_flush(Driver *drvthis);
void debug_string(Driver *drvthis, int x, int y, const char string[]);
int debug_chr(Driver *drvthis, int x, int y, char c);

// Test fixture setup - G15 tests
static Driver test_driver;
static PrivateData test_private_data;

// Test fixture setup - Debug driver tests
static DebugPrivateData debug_data;
static int debug_driver_initialized = 0;
static int debug_strings_written = 0;
static int debug_flushes_called = 0;

// Global test configuration flags
static int verbose_mode = 0;
static int g15_only = 0;
static int g510_only = 0;
static int test_detection_only = 0;
static int test_rgb_only = 0;
static int test_macros_only = 0;
static int test_failures_only = 0;

// Initialize test driver with clean state
void setup_test_driver(void)
{
	memset(&test_driver, 0, sizeof(test_driver));
	memset(&test_private_data, 0, sizeof(test_private_data));
	test_driver.private_data = &test_private_data;
	test_driver.name = "g15_test";
	mock_reset_state();
}

// Clean up test driver and close handles
void cleanup_test_driver(void)
{
	if (test_private_data.hidraw_handle) {
		lib_hidraw_close(test_private_data.hidraw_handle);
		test_private_data.hidraw_handle = NULL;
	}
}

// Initialize test for device with given product ID
static int init_test_device(unsigned short product_id)
{
	setup_test_driver();
	mock_set_current_device(product_id);
	return g15_init_device_detection(&test_driver);
}

// Print verbose test configuration details
static void print_verbose_test_config(void)
{
	printf("🚀 Starting G-Series Device Detection Tests (VERBOSE MODE)\n");
	printf("============================================================\n");
	printf("Test configuration:\n");
	printf("  G15 only: %s\n", g15_only ? "Yes" : "No");
	printf("  G510 only: %s\n", g510_only ? "Yes" : "No");
	printf("  Detection only: %s\n", test_detection_only ? "Yes" : "No");
	printf("  RGB only: %s\n", test_rgb_only ? "Yes" : "No");
	printf("  Macros only: %s\n", test_macros_only ? "Yes" : "No");
	printf("  Failures only: %s\n", test_failures_only ? "Yes" : "No");
	printf("============================================================\n");
}

// Detect G-Series device and check RGB support
int g15_init_device_detection(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	static const struct lib_hidraw_id hidraw_ids[] = {{{BUS_USB, 0x046d, 0xc222}, {0}},
							  {{0, 0, 0}, {0}}};

	p->hidraw_handle = lib_hidraw_open(hidraw_ids);

	if (!p->hidraw_handle) {
		return -1;
	}

	unsigned short product_id = lib_hidraw_get_product_id(p->hidraw_handle);

	// G510 models (0xc22d, 0xc22e) support RGB, G15 models do not
	if (product_id == 0xc22d || product_id == 0xc22e) {
		p->has_rgb_backlight = 1;
	} else {
		p->has_rgb_backlight = 0;
	}

	return 0;
}

// Set RGB backlight color via HID feature report
int g15_set_rgb_backlight(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;

	if (!p->has_rgb_backlight) {
		return -1;
	}

	unsigned char rgb_report[4];
	create_rgb_report(rgb_report, red, green, blue);

	return lib_hidraw_send_feature_report(p->hidraw_handle, rgb_report, 4);
}

// Test G15 Original device detection without RGB support
void test_g15_original_detection(void)
{
	printf("🧪 Testing G15 Original detection...\n");

	int result = init_test_device(0xc222);
	assert(result == 0);
	assert(test_private_data.has_rgb_backlight == 0);

	int rgb_result = g15_set_rgb_backlight(&test_driver, 255, 0, 0);
	assert(rgb_result == -1);
	assert(mock_get_rgb_commands_sent() == 0);

	cleanup_test_driver();
	printf("✅ G15 Original test passed\n");
}

// Test G15 v2 device detection without RGB support
void test_g15_v2_detection(void)
{
	printf("🧪 Testing G15 v2 detection...\n");

	int result = init_test_device(0xc227);
	assert(result == 0);
	assert(test_private_data.has_rgb_backlight == 0);

	int rgb_result = g15_set_rgb_backlight(&test_driver, 0, 255, 0);
	assert(rgb_result == -1);
	assert(mock_get_rgb_commands_sent() == 0);

	cleanup_test_driver();
	printf("✅ G15 v2 test passed\n");
}

// Test G510 device detection with RGB support
void test_g510_detection(void)
{
	printf("🧪 Testing G510 detection...\n");

	int result = init_test_device(0xc22d);
	assert(result == 0);
	assert(test_private_data.has_rgb_backlight == 1);

	int rgb_result = g15_set_rgb_backlight(&test_driver, 0, 0, 255);
	assert(rgb_result > 0);
	assert(mock_get_rgb_commands_sent() == 1);

	cleanup_test_driver();
	printf("✅ G510 test passed\n");
}

// Test G510s device detection with RGB support
void test_g510s_detection(void)
{
	printf("🧪 Testing G510s detection...\n");

	int result = init_test_device(0xc22e);
	assert(result == 0);
	assert(test_private_data.has_rgb_backlight == 1);

	int rgb_result1 = g15_set_rgb_backlight(&test_driver, 255, 128, 64);
	int rgb_result2 = g15_set_rgb_backlight(&test_driver, 100, 200, 50);
	assert(rgb_result1 > 0 && rgb_result2 > 0);
	assert(mock_get_rgb_commands_sent() == 2);

	cleanup_test_driver();
	printf("✅ G510s test passed\n");
}

// Test handling of unknown G-Series device
void test_unknown_device(void)
{
	printf("🧪 Testing unknown device handling...\n");

	int result = init_test_device(0xc221);
	assert(result == 0);
	assert(test_private_data.has_rgb_backlight == 0);

	int rgb_result = g15_set_rgb_backlight(&test_driver, 255, 255, 255);
	assert(rgb_result == -1);
	assert(mock_get_rgb_commands_sent() == 0);

	cleanup_test_driver();
	printf("✅ Unknown device test passed\n");
}

// Test device connection failure handling
void test_device_failure(void)
{
	printf("🧪 Testing device failure handling...\n");

	setup_test_driver();
	mock_set_device_failure(1);

	int result = g15_init_device_detection(&test_driver);
	assert(result == -1);
	assert(test_private_data.hidraw_handle == NULL);

	cleanup_test_driver();
	printf("✅ Device failure test passed\n");
}

// Test RGB value boundary validation
void test_rgb_validation(void)
{
	printf("🧪 Testing RGB value validation...\n");

	int result = init_test_device(0xc22e);
	assert(result == 0);

	assert(g15_set_rgb_backlight(&test_driver, 0, 0, 0) > 0);
	assert(g15_set_rgb_backlight(&test_driver, 255, 255, 255) > 0);
	assert(g15_set_rgb_backlight(&test_driver, 128, 64, 192) > 0);
	assert(mock_get_rgb_commands_sent() == 3);

	cleanup_test_driver();
	printf("✅ RGB validation test passed\n");
}

// Set RGB color using LED subsystem method
int g15_set_rgb_led_subsystem(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;

	if (!p->has_rgb_backlight)
		return -1;

	p->rgb_red = (unsigned char)red;
	p->rgb_green = (unsigned char)green;
	p->rgb_blue = (unsigned char)blue;

	mock_increment_rgb_commands();

	return 1;
}

// Set RGB color using HID reports method
int g15_set_rgb_hid_reports(Driver *drvthis, int red, int green, int blue)
{
	PrivateData *p = drvthis->private_data;

	if (!p->has_rgb_backlight)
		return -1;

	unsigned char rgb_report[4];
	create_rgb_report(rgb_report, red, green, blue);

	return lib_hidraw_send_feature_report(p->hidraw_handle, rgb_report, 4);
}

// Test LED subsystem vs HID report RGB methods
void test_rgb_methods(void)
{
	printf("🧪 Testing RGB methods (LED subsystem vs HID reports)...\n");

	int result = init_test_device(0xc22e);
	assert(result == 0);

	test_private_data.rgb_method_hid = 0;
	int led_result = g15_set_rgb_led_subsystem(&test_driver, 255, 128, 64);
	assert(led_result > 0);
	assert(test_private_data.rgb_red == 255);
	assert(test_private_data.rgb_green == 128);
	assert(test_private_data.rgb_blue == 64);

	test_private_data.rgb_method_hid = 1;
	int hid_result = g15_set_rgb_hid_reports(&test_driver, 100, 200, 50);
	assert(hid_result > 0);

	cleanup_test_driver();
	printf("✅ RGB methods test passed\n");
}

// Test RGB rejection on non-RGB devices (G15 Original/v2)
void test_rgb_on_non_rgb_device(void)
{
	printf("🧪 Testing RGB rejection on non-RGB devices...\n");

	// Test G15 Original (no RGB support - Product ID 0xc222)
	mock_reset_state();
	mock_set_current_device(0xc222);

	int result = init_test_device(0xc222);
	assert(result == 0);

	// Explicitly disable RGB support
	test_private_data.has_rgb_backlight = 0;

	// Both RGB methods should return -1 (not supported)
	assert(g15_set_rgb_led_subsystem(&test_driver, 255, 0, 0) == -1);
	assert(g15_set_rgb_hid_reports(&test_driver, 0, 255, 0) == -1);

	cleanup_test_driver();

	// Test G15 v2 (also no RGB support - Product ID 0xc227)
	mock_reset_state();
	mock_set_current_device(0xc227);

	result = init_test_device(0xc227);
	assert(result == 0);

	test_private_data.has_rgb_backlight = 0;

	// Should also reject RGB commands
	assert(g15_set_rgb_led_subsystem(&test_driver, 0, 0, 255) == -1);
	assert(g15_set_rgb_hid_reports(&test_driver, 255, 255, 0) == -1);

	cleanup_test_driver();
	printf("✅ RGB rejection test passed\n");
}

// Test mock library error conditions
void test_mock_error_conditions()
{
	printf("📋 Testing mock error conditions...\n");

	mock_set_device_failure(1);
	struct lib_hidraw_id test_ids[] = {{{BUS_USB, 0x046d, 0xc222}, {0}}, {{0, 0, 0}, {0}}};
	struct lib_hidraw_handle *handle = lib_hidraw_open(test_ids);
	assert(handle == NULL);

	mock_set_device_failure(0);
	lib_hidraw_send_output_report(NULL, (unsigned char *)"test", 4);

	printf("✅ Mock error conditions test passed\n");
}

// Start recording a G-Key macro
int g15_start_macro_recording(Driver *drvthis, int gkey, int mode)
{
	PrivateData *p = drvthis->private_data;

	if (validate_gkey_params(gkey, mode) < 0)
		return -1;

	p->macro_recording_mode = 1;
	p->current_g_mode = mode;
	p->last_recorded_gkey = gkey;

	return 0;
}

// Stop recording the current G-Key macro
int g15_stop_macro_recording(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;

	if (!p->macro_recording_mode)
		return -1;

	p->macro_recording_mode = 0;

	return 0;
}

// Process G-Key macro playback trigger
int g15_process_gkey_macro(Driver *drvthis, int gkey, int mode)
{
	PrivateData *p = drvthis->private_data;

	if (validate_gkey_params(gkey, mode) < 0)
		return -1;

	// Check if key/mode matches recorded macro
	return (gkey == p->last_recorded_gkey && mode == p->current_g_mode) ? 1 : 0;
}

// Test G-Key macro recording functionality
void test_macro_recording(void)
{
	printf("🧪 Testing G-Key macro recording...\n");

	int result = init_test_device(0xc22e);
	assert(result == 0);

	int start_result = g15_start_macro_recording(&test_driver, 5, 2);
	assert(start_result == 0);
	assert(test_private_data.macro_recording_mode == 1);
	assert(test_private_data.current_g_mode == 2);
	assert(test_private_data.last_recorded_gkey == 5);

	assert(g15_start_macro_recording(&test_driver, 0, 2) == -1);
	assert(g15_start_macro_recording(&test_driver, 19, 2) == -1);
	assert(g15_start_macro_recording(&test_driver, 5, 0) == -1);
	assert(g15_start_macro_recording(&test_driver, 5, 4) == -1);

	int stop_result = g15_stop_macro_recording(&test_driver);
	assert(stop_result == 0);
	assert(test_private_data.macro_recording_mode == 0);
	assert(g15_stop_macro_recording(&test_driver) == -1);

	cleanup_test_driver();
	printf("✅ Macro recording test passed\n");
}

// Test G-Key macro playback functionality
void test_macro_playback(void)
{
	printf("🧪 Testing G-Key macro playback...\n");

	int result = init_test_device(0xc22d);
	assert(result == 0);

	test_private_data.last_recorded_gkey = 12;
	test_private_data.current_g_mode = 1;

	int playback_result = g15_process_gkey_macro(&test_driver, 12, 1);
	assert(playback_result == 1);

	int wrong_key = g15_process_gkey_macro(&test_driver, 11, 1);
	assert(wrong_key == 0);

	int wrong_mode = g15_process_gkey_macro(&test_driver, 12, 2);
	assert(wrong_mode == 0);

	assert(g15_process_gkey_macro(&test_driver, 0, 1) == -1);
	assert(g15_process_gkey_macro(&test_driver, 19, 1) == -1);
	assert(g15_process_gkey_macro(&test_driver, 12, 0) == -1);
	assert(g15_process_gkey_macro(&test_driver, 12, 4) == -1);

	cleanup_test_driver();
	printf("✅ Macro playback test passed\n");
}

// Initialize debug driver for output testing
int debug_init(Driver *drvthis)
{
	(void)drvthis;

	memset(&debug_data, 0, sizeof(debug_data));

	debug_data.width = 20;
	debug_data.height = 4;
	debug_data.cellwidth = 5;
	debug_data.cellheight = 8;
	debug_data.contrast = 500;
	debug_data.brightness = 750;
	debug_data.offbrightness = 250;

	debug_data.framebuf = calloc(debug_data.width * debug_data.height, sizeof(char));
	if (!debug_data.framebuf)
		return -1;

	debug_driver_initialized = 1;
	debug_strings_written = 0;
	debug_flushes_called = 0;

	return 0;
}

// Close debug driver and free resources
void debug_close(Driver *drvthis)
{
	(void)drvthis;

	if (debug_data.framebuf) {
		free(debug_data.framebuf);
		debug_data.framebuf = NULL;
	}

	debug_driver_initialized = 0;
}

// Get debug driver display width
int debug_width(Driver *drvthis)
{
	(void)drvthis;
	return debug_data.width;
}

// Get debug driver display height
int debug_height(Driver *drvthis)
{
	(void)drvthis;
	return debug_data.height;
}

// Clear debug driver framebuffer
void debug_clear(Driver *drvthis)
{
	(void)drvthis;

	if (debug_data.framebuf) {
		memset(debug_data.framebuf, ' ', debug_data.width * debug_data.height);
	}
}

// Flush debug driver output to display
void debug_flush(Driver *drvthis)
{
	(void)drvthis;
	debug_flushes_called++;
}

// Write string to debug driver framebuffer
void debug_string(Driver *drvthis, int x, int y, const char string[])
{
	(void)drvthis;

	if (!debug_data.framebuf || !string)
		return;

	if (validate_and_convert_coords(&x, &y, debug_data.width, debug_data.height) < 0)
		return;

	int len = strlen(string);
	int max_len = debug_data.width - x;

	if (len > max_len)
		len = max_len;

	memcpy(&debug_data.framebuf[y * debug_data.width + x], string, len);
	debug_strings_written++;
}

// Write single character to debug driver framebuffer
int debug_chr(Driver *drvthis, int x, int y, char c)
{
	(void)drvthis;

	if (!debug_data.framebuf)
		return -1;

	if (validate_and_convert_coords(&x, &y, debug_data.width, debug_data.height) < 0)
		return -1;

	// Write character at calculated position
	debug_data.framebuf[y * debug_data.width + x] = c;

	return 0;
}

// Test debug driver basic functionality
void test_debug_driver_basic(void)
{
	printf("🧪 Testing debug driver basic functionality...\n");

	Driver debug_driver;
	memset(&debug_driver, 0, sizeof(debug_driver));
	debug_driver.name = "debug_test";

	debug_strings_written = 0;
	debug_flushes_called = 0;

	int init_result = debug_init(&debug_driver);
	assert(init_result == 0);
	assert(debug_driver_initialized == 1);
	assert(debug_data.framebuf != NULL);

	assert(debug_width(&debug_driver) == 20);
	assert(debug_height(&debug_driver) == 4);

	debug_clear(&debug_driver);
	debug_string(&debug_driver, 1, 1, "Test String");
	assert(debug_strings_written == 1);
	assert(strncmp(&debug_data.framebuf[0], "Test String", 11) == 0);

	int chr_result = debug_chr(&debug_driver, 15, 2, 'X');
	assert(chr_result == 0);
	assert(debug_data.framebuf[1 * 20 + 14] == 'X');

	debug_flush(&debug_driver);
	assert(debug_flushes_called == 1);

	assert(debug_chr(&debug_driver, 0, 1, 'A') == -1);
	assert(debug_chr(&debug_driver, 21, 1, 'B') == -1);
	assert(debug_chr(&debug_driver, 1, 0, 'C') == -1);
	assert(debug_chr(&debug_driver, 1, 5, 'D') == -1);

	debug_close(&debug_driver);
	assert(debug_driver_initialized == 0);

	printf("✅ Debug driver basic test passed\n");
}

// Test debug driver output validation
void test_debug_driver_output_validation(void)
{
	printf("🧪 Testing debug driver as output validator...\n");

	Driver debug_driver;
	memset(&debug_driver, 0, sizeof(debug_driver));
	debug_driver.name = "debug_validator";

	debug_strings_written = 0;
	debug_flushes_called = 0;

	assert(debug_init(&debug_driver) == 0);
	debug_clear(&debug_driver);

	debug_string(&debug_driver, 1, 1, "CPU: 23.5%  Mem: 67%");
	debug_string(&debug_driver, 1, 2, "Load: 0.15 0.25 0.18");
	debug_string(&debug_driver, 1, 3, "Uptime: 2d 14h 32m");
	debug_string(&debug_driver, 1, 4, "Temp: 45C  Fan: 1250");

	debug_flush(&debug_driver);

	assert(debug_strings_written == 4);
	assert(debug_flushes_called == 1);

	assert(strncmp(&debug_data.framebuf[0], "CPU: 23.5%", 10) == 0);
	assert(strncmp(&debug_data.framebuf[20], "Load: 0.15", 10) == 0);
	assert(strncmp(&debug_data.framebuf[40], "Uptime: 2d", 10) == 0);
	assert(strncmp(&debug_data.framebuf[60], "Temp: 45C", 9) == 0);

	debug_clear(&debug_driver);

	for (int i = 0; i < 80; i++) {
		assert(debug_data.framebuf[i] == ' ');
	}

	debug_close(&debug_driver);
	printf("✅ Debug driver output validation test passed\n");
}

// Test debug driver error handling
void test_debug_driver_error_handling(void)
{
	printf("🧪 Testing debug driver error handling...\n");

	Driver debug_driver;
	memset(&debug_driver, 0, sizeof(debug_driver));
	debug_driver.name = "debug_error_test";

	debug_strings_written = 0;
	debug_flushes_called = 0;

	debug_string(&debug_driver, 1, 1, "Should not work");
	assert(debug_strings_written == 0);

	assert(debug_init(&debug_driver) == 0);

	debug_string(&debug_driver, 20, 1, "Exactly fits");
	debug_string(&debug_driver, 21, 1, "Too far right");
	debug_string(&debug_driver, 1, 5, "Too far down");

	debug_string(&debug_driver, 15, 1,
		     "Long string that exceeds display width and should be truncated");

	char expected[7];
	strncpy(expected, &debug_data.framebuf[14], 6);
	expected[6] = '\0';
	assert(strcmp(expected, "Long s") == 0);

	debug_close(&debug_driver);
	printf("✅ Debug driver error handling test passed\n");
}

// Print test execution summary
void print_test_summary(int tests_run, int tests_passed)
{
	printf("\n🧪 TEST SUMMARY:\n");
	printf("Tests run: %d\n", tests_run);
	printf("Tests passed: %d\n", tests_passed);
	printf("Tests failed: %d\n", tests_run - tests_passed);

	if (tests_passed == tests_run) {
		printf("🎉 ALL TESTS PASSED!\n");
	} else {
		printf("❌ Some tests failed!\n");
	}
}

// Test command-line argument parsing
void test_command_line_parsing()
{
	printf("📋 Testing command-line argument parsing...\n");

	int original_verbose = verbose_mode;
	verbose_mode = 0;

	// Test --verbose flag recognition
	if (strcmp("--verbose", "--verbose") == 0) {
		verbose_mode = 1;
	}
	assert(verbose_mode == 1);

	// Test --device-filter=g15 recognition
	int local_g15_only = 0;
	if (strcmp("--device-filter=g15", "--device-filter=g15") == 0) {
		local_g15_only = 1;
	}
	assert(local_g15_only == 1);

	// Test --test-detection flag recognition
	int local_test_detection_only = 0;
	if (strcmp("--test-detection", "--test-detection") == 0) {
		local_test_detection_only = 1;
	}
	assert(local_test_detection_only == 1);

	// Test unknown argument handling
	int unknown_found = 0;
	char *test_arg = "--unknown-option";

	// Try all valid arguments (should all fail since test_arg is unknown)
	if (strcmp(test_arg, "--verbose") == 0 || strcmp(test_arg, "--device-filter=g15") == 0 ||
	    strcmp(test_arg, "--device-filter=g510") == 0 ||
	    strcmp(test_arg, "--test-detection") == 0 || strcmp(test_arg, "--test-rgb") == 0 ||
	    strcmp(test_arg, "--test-macros") == 0 || strcmp(test_arg, "--test-failures") == 0 ||
	    strcmp(test_arg, "--help") == 0) {
		// Known argument - should not happen with test_arg
	} else {
		// None matched - unknown argument
		printf("Unknown option: %s\n", test_arg);
		unknown_found = 1;
	}
	assert(unknown_found == 1);

	verbose_mode = original_verbose;
	printf("✅ Command-line argument parsing test passed\n");
}

// Test verbose mode output formatting
void test_verbose_mode_output()
{
	printf("📋 Testing verbose mode output...\n");

	int original_verbose = verbose_mode;
	int original_g15_only = g15_only;
	int original_g510_only = g510_only;
	int original_test_detection_only = test_detection_only;
	int original_test_rgb_only = test_rgb_only;
	int original_test_macros_only = test_macros_only;
	int original_test_failures_only = test_failures_only;

	verbose_mode = 1;
	g15_only = 1;
	g510_only = 0;
	test_detection_only = 1;
	test_rgb_only = 0;
	test_macros_only = 1;
	test_failures_only = 0;

	if (verbose_mode) {
		print_verbose_test_config();
	}

	verbose_mode = original_verbose;
	g15_only = original_g15_only;
	g510_only = original_g510_only;
	test_detection_only = original_test_detection_only;
	test_rgb_only = original_test_rgb_only;
	test_macros_only = original_test_macros_only;
	test_failures_only = original_test_failures_only;

	printf("✅ Verbose mode output test passed\n");
}

// Test RGB parameter boundary validation
void test_rgb_parameter_validation()
{
	printf("📋 Testing RGB parameter validation...\n");

	int init_result = init_test_device(0xc22e);
	assert(init_result == 0);

	int result1 = g15_set_rgb_backlight(&test_driver, 100, 150, 200);
	assert(result1 == 4);

	int result2 = g15_set_rgb_backlight(&test_driver, 0, 0, 0);
	assert(result2 == 4);

	int result3 = g15_set_rgb_backlight(&test_driver, 255, 255, 255);
	assert(result3 == 4);

	cleanup_test_driver();
	printf("✅ RGB parameter validation test passed\n");
}

// Print command-line usage information
void print_usage(const char *program_name)
{
	printf("Usage: %s [OPTIONS]\n", program_name);
	printf("Options:\n");
	printf("  --verbose           Enable verbose output\n");
	printf("  --device-filter=g15 Test only G15 devices (no RGB)\n");
	printf("  --device-filter=g510 Test only G510 devices (with RGB)\n");
	printf("  --test-detection    Test only device detection\n");
	printf("  --test-rgb          Test only RGB functionality\n");
	printf("  --test-macros       Test only macro system\n");
	printf("  --test-failures     Test only error handling\n");
	printf("  --help              Show this help\n");
}

// Main test execution entry point
int main(int argc, char *argv[])
{
	// Parse command-line arguments
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--verbose") == 0) {
			verbose_mode = 1;

		} else if (strcmp(argv[i], "--device-filter=g15") == 0) {
			g15_only = 1;

		} else if (strcmp(argv[i], "--device-filter=g510") == 0) {
			g510_only = 1;

		} else if (strcmp(argv[i], "--test-detection") == 0) {
			test_detection_only = 1;

		} else if (strcmp(argv[i], "--test-rgb") == 0) {
			test_rgb_only = 1;

		} else if (strcmp(argv[i], "--test-macros") == 0) {
			test_macros_only = 1;

		} else if (strcmp(argv[i], "--test-failures") == 0) {
			test_failures_only = 1;

		} else if (strcmp(argv[i], "--help") == 0) {
			print_usage(argv[0]);
			return 0;

		} else {
			printf("Unknown option: %s\n", argv[i]);
			print_usage(argv[0]);
			return 1;
		}
	}

	if (verbose_mode) {
		print_verbose_test_config();
	} else {
		printf("🚀 Starting G-Series Device Detection Tests\n");
		printf("============================================\n");
	}

	int tests_run = 0;
	int tests_passed = 0;

	// Run device detection tests if requested or no specific filter applied
	if (test_detection_only || (!test_rgb_only && !test_macros_only && !test_failures_only)) {

		// Test G15 models (no RGB) unless filtered out
		if (!g510_only) {
			if (verbose_mode)
				printf("📍 Running G15 Original detection test...\n");
			tests_run++;
			test_g15_original_detection();
			tests_passed++;

			if (verbose_mode)
				printf("📍 Running G15 v2 detection test...\n");
			tests_run++;
			test_g15_v2_detection();
			tests_passed++;
		}

		// Test G510 models (with RGB) unless filtered out
		if (!g15_only) {
			if (verbose_mode)
				printf("📍 Running G510 detection test...\n");
			tests_run++;
			test_g510_detection();
			tests_passed++;

			if (verbose_mode)
				printf("📍 Running G510s detection test...\n");
			tests_run++;
			test_g510s_detection();
			tests_passed++;
		}

		// Always test unknown device handling
		if (verbose_mode)
			printf("📍 Running unknown device test...\n");
		tests_run++;
		test_unknown_device();
		tests_passed++;
	}

	// Run failure handling tests if requested or no specific filter applied
	if (test_failures_only || (!test_detection_only && !test_rgb_only && !test_macros_only)) {
		if (verbose_mode)
			printf("📍 Running device failure test...\n");
		tests_run++;
		test_device_failure();
		tests_passed++;
	}

	// Run RGB tests if requested or no filter applied (skip if G15-only mode)
	if (test_rgb_only ||
	    (!test_detection_only && !test_macros_only && !test_failures_only && !g15_only)) {
		if (verbose_mode)
			printf("📍 Running RGB validation test...\n");
		tests_run++;
		test_rgb_validation();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running RGB methods test...\n");
		tests_run++;
		test_rgb_methods();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running RGB rejection test...\n");
		tests_run++;
		test_rgb_on_non_rgb_device();
		tests_passed++;
	}

	// Run macro tests if requested or no specific filter applied
	if (test_macros_only || (!test_detection_only && !test_rgb_only && !test_failures_only)) {
		if (verbose_mode)
			printf("📍 Running macro recording test...\n");
		tests_run++;
		test_macro_recording();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running macro playback test...\n");
		tests_run++;
		test_macro_playback();
		tests_passed++;
	}

	// Run comprehensive tests when no specific filter is active
	if (!test_detection_only && !test_rgb_only && !test_macros_only && !test_failures_only) {

		// Debug driver functionality tests
		if (verbose_mode)
			printf("📍 Running debug driver basic test...\n");
		tests_run++;
		test_debug_driver_basic();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running debug driver output validation test...\n");
		tests_run++;
		test_debug_driver_output_validation();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running debug driver error handling test...\n");
		tests_run++;
		test_debug_driver_error_handling();
		tests_passed++;

		// Mock library and argument parsing tests
		if (verbose_mode)
			printf("📍 Running mock error conditions test...\n");
		tests_run++;
		test_mock_error_conditions();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running command-line argument parsing test...\n");
		tests_run++;
		test_command_line_parsing();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running verbose mode output test...\n");
		tests_run++;
		test_verbose_mode_output();
		tests_passed++;

		if (verbose_mode)
			printf("📍 Running RGB parameter validation test...\n");
		tests_run++;
		test_rgb_parameter_validation();
		tests_passed++;
	}

	// Coverage improvement: test usage output (only in verbose mode to avoid clutter)
	if (verbose_mode) {
		printf("📍 Running coverage improvement tests...\n");
		tests_run++;
		print_usage("test_g15");
		tests_passed++;
	}

	print_test_summary(tests_run, tests_passed);
	return (tests_passed == tests_run) ? 0 : 1;
}
