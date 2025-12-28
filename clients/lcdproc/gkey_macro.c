// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/gkey_macro.c
 * \brief G-Key macro system implementation for Logitech G15 keyboards
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - Real-time input event recording from /dev/input/event* devices
 * - Macro playback using ydotool system calls for Wayland compatibility
 * - Three independent macro modes (M1, M2, M3) with 18 G-keys each
 * - Persistent macro storage in text format with pipe-separated commands
 * - Multi-threaded input recording system using pthread
 * - Automatic keyboard device detection and filtering
 * - Text sequence optimization (groups consecutive characters)
 * - LED status management for mode and recording indicators
 * - Support for delays, key combinations, and text typing
 * - Thread-safe recording with stop/start controls
 * - Automatic cleanup of temporary recording files
 *
 * \usage
 * - Called by lcdproc client when G15 keyboard events are received
 * - Provides macro recording, playback, and management for G-keys
 * - Used with ydotool daemon for cross-platform input simulation
 * - Integrates with LCDd server for LED status updates
 * - Requires access to /dev/input/event* devices for recording
 *
 *
 * \details
 * This file implements a comprehensive macro system for Logitech G15
 * keyboards connected to the lcdproc client. It provides recording, playback,
 * and management of macros for the 18 G-keys across 3 modes (M1, M2, M3).
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <locale.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shared/environment.h"
#include "shared/posix_wrappers.h"

#include "gkey_macro.h"
#include "main.h"
#include "shared/posix_wrappers.h"

#include "shared/posix_wrappers.h"
#include "shared/report.h"
#include "shared/sockets.h"

/** \brief Maximum path length fallback if not defined by system */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/** \brief Calculate number of longs needed for bit array */
#define NLONGS(x) (((x) + 8 * sizeof(long) - 1) / (8 * sizeof(long)))
/** \brief Test if bit is set in bit array (for input device capability testing) */
#define test_bit(bit, array)                                                                       \
	((array)[(bit) / (8 * sizeof(long))] & (1UL << ((bit) % (8 * sizeof(long)))))

/**
 * \brief Macro command storage structure
 * \details Stores ydotool commands for macro playback.
 * Each macro can contain up to 10 commands of 256 chars each.
 */
typedef struct {
	char commands[10][256]; ///< Array of ydotool command strings
	int command_count;	///< Number of commands stored
	time_t created;		///< Timestamp when macro was created
} macro_t;

/**
 * \brief Input event recording thread data structure
 * \details Manages pthread-based keyboard input recording for macro creation.
 * Records raw input events from /dev/input/event* devices.
 */
typedef struct {
	int recording;		  ///< Recording active flag
	int stop_recording;	  ///< Signal to stop recording thread
	char record_file[256];	  ///< Path to temporary recording file
	pthread_t record_thread;  ///< Recording thread handle
	int event_fds[32];	  ///< File descriptors for input devices
	int event_fd_count;	  ///< Number of open input devices
	time_t record_start_time; ///< When recording started
} input_recorder_t;

/** \brief Global macro system state
 *
 * \details Tracks current mode (M1/M2/M3), recording status, and all stored macros.
 * Manages configuration file path and input recording thread.
 */
static struct {
	char current_mode[8];
	int recording;
	char recording_target[8];
	macro_t macros[3][18];
	char config_file[256];
	time_t last_key_time;
	pid_t ydotool_record_pid;
	char record_file[256];
	input_recorder_t recorder;
} macro_state = {.current_mode = "M1",
		 .recording = 0,
		 .recording_target = "",
		 .macros = {{{0}}},
		 .config_file = "",
		 .last_key_time = 0,
		 .ydotool_record_pid = 0,
		 .record_file = "",
		 .recorder = {.recording = 0,
			      .stop_recording = 0,
			      .record_file = "",
			      .record_thread = 0,
			      .event_fds = {-1},
			      .event_fd_count = 0,
			      .record_start_time = 0}};

// Forward function declarations
static void save_macros(void);
static void load_macros(void);
static int execute_ydotool_command(const char *command);
static void play_macro(const char *g_key);
static void start_recording(const char *g_key);
static void stop_recording(void);
static void convert_ydotool_recording(void);
static void *input_recording_thread(void *arg);
static int open_input_devices(void);
static void close_input_devices(void);
static const char *translate_key_code(int code);
static int is_keyboard_device(const char *device_path);

/**
 * \brief Create directory and all parent directories
 * \param path Directory path to create
 * \param mode Directory permissions mode
 * \retval 0 Success
 * \retval -1 Failed to create directory
 *
 * \details Equivalent to 'mkdir -p', creates all intermediate directories.
 * Ignores EEXIST errors for existing directories.
 */
static int mkdir_recursive(const char *path, mode_t mode)
{
	char tmp[PATH_MAX];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);

	// Remove trailing slash
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	// Create each directory component
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			// Try to create directory, ignore error if it already exists
			if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
				return -1;
			}
			*p = '/';
		}
	}

	// Create final directory component
	if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
		return -1;
	}

	return 0;
}

// Initialize the G-Key macro system
int gkey_macro_init(void)
{
	memset(&macro_state.macros, 0, sizeof(macro_state.macros));

	// Use thread-safe cached environment variable
	const char *home = env_get_home();

	if (home) {
		snprintf(macro_state.config_file, sizeof(macro_state.config_file),
			 "%s/.config/lcdproc/g15_macros.json", home);

		// Create directory recursively to ensure both .config/ and .config/lcdproc/ exist
		char dir_path[256];
		snprintf(dir_path, sizeof(dir_path), "%s/.config/lcdproc", home);

		if (mkdir_recursive(dir_path, 0755) != 0) {
			report(RPT_WARNING,
			       "G-Key Macro: Failed to create directory %s, falling back to /tmp",
			       dir_path);
			strncpy(macro_state.config_file, "/tmp/lcdproc_g15_macros.json",
				sizeof(macro_state.config_file) - 1);
			macro_state.config_file[sizeof(macro_state.config_file) - 1] = '\0';
		}
	} else {
		strncpy(macro_state.config_file, "/tmp/lcdproc_g15_macros.json",
			sizeof(macro_state.config_file) - 1);
		macro_state.config_file[sizeof(macro_state.config_file) - 1] = '\0';
	}

	load_macros();
	gkey_macro_update_leds();
	report(RPT_INFO, "G-Key Macro: Initialized (Mode: %s, File: %s)", macro_state.current_mode,
	       macro_state.config_file);

	return 0;
}

// Clean up macro system resources
void gkey_macro_cleanup(void)
{
	if (macro_state.recorder.recording) {
		stop_input_recording();
	}
	save_macros();
}

// Handle G-Key and mode key press events
void gkey_macro_handle_key(const char *key_name)
{
	if (!key_name)
		return;

	report(RPT_DEBUG, "G-Key Macro: Key pressed: %s", key_name);

	// Handle MR key: toggle recording mode
	if (strcmp(key_name, "MR") == 0) {
		if (macro_state.recording) {
			stop_recording();
		} else {
			macro_state.recording = 1;
			report(RPT_INFO, "G-Key Macro: Recording mode active - press a G-key to "
					 "start recording");
			gkey_macro_update_leds();
		}

		// Handle mode switch keys: M1, M2, M3
	} else if (strncmp(key_name, "M", 1) == 0 && strlen(key_name) == 2) {
		strncpy(macro_state.current_mode, key_name, sizeof(macro_state.current_mode) - 1);
		macro_state.current_mode[sizeof(macro_state.current_mode) - 1] = '\0';
		report(RPT_INFO, "G-Key Macro: Switched to mode %s", macro_state.current_mode);
		gkey_macro_update_leds();

		// Handle G-key presses: G1-G18
	} else if (strncmp(key_name, "G", 1) == 0 && strlen(key_name) > 1) {
		if (macro_state.recording) {
			start_recording(key_name);
		} else {
			play_macro(key_name);
		}
	}

	macro_state.last_key_time = time(NULL);
}

// Get the current macro mode
const char *gkey_macro_get_mode(void) { return macro_state.current_mode; }

// Check if macro recording is currently active
int gkey_macro_is_recording(void) { return macro_state.recording; }

// Update macro LED status on the keyboard
void gkey_macro_update_leds(void)
{
	int m1 = (strcmp(macro_state.current_mode, "M1") == 0) ? 1 : 0;
	int m2 = (strcmp(macro_state.current_mode, "M2") == 0) ? 1 : 0;
	int m3 = (strcmp(macro_state.current_mode, "M3") == 0) ? 1 : 0;
	int mr = macro_state.recording ? 1 : 0;

	char command[64];
	snprintf(command, sizeof(command), "macro_leds %d %d %d %d\n", m1, m2, m3, mr);

	if (sock_send_string(sock, command) < 0) {
		report(RPT_ERR, "G-Key Macro: Failed to send LED command to server");
	} else {
		report(RPT_DEBUG, "G-Key Macro LED update: M1=%s M2=%s M3=%s MR=%s",
		       m1 ? "ON" : "OFF", m2 ? "ON" : "OFF", m3 ? "ON" : "OFF", mr ? "ON" : "OFF");
	}
}

/**
 * \brief Convert mode name to array index
 * \param mode Mode name string (M1, M2, or M3)
 * \retval 0 Mode M1
 * \retval 1 Mode M2
 * \retval 2 Mode M3 (or invalid mode, defaults to M1)
 *
 * \details Maps mode strings to macro_state.macros array indices.
 */
static int get_mode_index(const char *mode)
{
	if (strcmp(mode, "M1") == 0)
		return 0;
	if (strcmp(mode, "M2") == 0)
		return 1;
	if (strcmp(mode, "M3") == 0)
		return 2;
	return 0;
}

/**
 * \brief Convert G-key name to array index
 * \param gkey G-key name string (G1-G18)
 * \retval 0-17 Valid G-key index
 * \retval -1 Invalid G-key name
 *
 * \details Maps G-key names to macro_state.macros array indices.
 * G1 maps to 0, G18 maps to 17.
 */
static int get_gkey_index(const char *gkey)
{
	if (strncmp(gkey, "G", 1) == 0) {
		int num = atoi(gkey + 1);
		if (num >= 1 && num <= 18) {
			return num - 1;
		}
	}
	return -1;
}

/**
 * \brief Load macros from configuration file
 *
 * \details Reads macro definitions from macro_state.config_file.
 * Parses pipe-separated command format. Returns silently if file not found.
 */
static void load_macros(void)
{
	FILE *file = fopen(macro_state.config_file, "r");
	if (!file) {
		report(RPT_DEBUG, "G-Key Macro: No existing config file, using defaults");
		return;
	}

	// Parse file format: MODE GKEY COMMAND_COUNT COMMAND1|COMMAND2|...
	char line[512];
	while (fgets(line, sizeof(line), file)) {
		char mode[8], gkey[8];
		int cmd_count;

		if (sscanf(line, "%7s %7s %d", mode, gkey, &cmd_count) == 3) {
			int mode_idx = get_mode_index(mode);
			int gkey_idx = get_gkey_index(gkey);

			if (gkey_idx >= 0 && cmd_count > 0 && cmd_count <= 10) {
				macro_state.macros[mode_idx][gkey_idx].command_count = cmd_count;

				// Find start of command data: skip past mode, gkey, cmd_count
				// fields
				char *ptr = line;
				for (int i = 0; i < 3 && ptr; i++) {
					ptr = strchr(ptr, ' ');
					if (ptr)
						ptr++;
				}

				// Parse pipe-separated command list
				for (int i = 0; i < cmd_count && ptr; i++) {
					char *end = strchr(ptr, '|');
					if (end) {
						*end = '\0';
						strncpy(macro_state.macros[mode_idx][gkey_idx]
							    .commands[i],
							ptr, 255);
						macro_state.macros[mode_idx][gkey_idx]
						    .commands[i][255] = '\0';
						ptr = end + 1;
					} else {
						strncpy(macro_state.macros[mode_idx][gkey_idx]
							    .commands[i],
							ptr, 255);
						macro_state.macros[mode_idx][gkey_idx]
						    .commands[i][255] = '\0';
						char *nl =
						    strchr(macro_state.macros[mode_idx][gkey_idx]
							       .commands[i],
							   '\n');
						if (nl)
							*nl = '\0';
						break;
					}
				}
			}
		}
	}

	fclose(file);
	report(RPT_INFO, "G-Key Macro: Loaded macros from %s", macro_state.config_file);
}

/**
 * \brief Save macros to configuration file
 *
 * \details Writes all macro definitions to macro_state.config_file
 * in pipe-separated command format. One line per G-key with macro defined.
 */
static void save_macros(void)
{
	FILE *file = fopen(macro_state.config_file, "w");
	if (!file) {
		report(RPT_ERR, "G-Key Macro: Failed to save macros to %s",
		       macro_state.config_file);
		return;
	}

	const char *modes[] = {"M1", "M2", "M3"};

	// Iterate through all modes and G-keys
	for (int mode_idx = 0; mode_idx < 3; mode_idx++) {
		for (int gkey_idx = 0; gkey_idx < 18; gkey_idx++) {
			macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];

			// Write: MODE GKEY COMMAND_COUNT COMMAND1|COMMAND2|...
			if (macro->command_count > 0) {
				fprintf(file, "%s G%d %d ", modes[mode_idx], gkey_idx + 1,
					macro->command_count);
				for (int i = 0; i < macro->command_count; i++) {
					fprintf(file, "%s", macro->commands[i]);
					if (i < macro->command_count - 1) {
						fprintf(file, "|");
					}
				}
				fprintf(file, "\n");
			}
		}
	}

	fclose(file);
	report(RPT_INFO, "G-Key Macro: Saved macros to %s", macro_state.config_file);
}

/**
 * \brief Execute ydotool command for input simulation
 * \param command Command string to execute (without "ydotool" prefix)
 * \retval 0 Command executed successfully
 * \retval -1 Command failed or ydotool not available
 *
 * \details Uses posix_spawn() to execute ydotool with YDOTOOL_SOCKET
 * environment variable. Thread-safe, avoids shell injection vulnerabilities.
 */
static int execute_ydotool_command(const char *command)
{
	pid_t pid;
	int status;
	char *argv[64];
	int argc = 0;
	char cmd_copy[512];
	char *envp[] = {"YDOTOOL_SOCKET=/tmp/.ydotool_socket", NULL};

	// Parse command into argv array
	snprintf(cmd_copy, sizeof(cmd_copy), "%s", command);
	argv[argc++] = "ydotool";

	// strtok_r() is thread-safe (POSIX.1-2001), uses caller-provided context instead of static
	// buffer
	char *saveptr;
	char *token = strtok_r(cmd_copy, " ", &saveptr);
	while (token != NULL && argc < 63) {
		argv[argc++] = token;
		token = strtok_r(NULL, " ", &saveptr);
	}
	argv[argc] = NULL;

	// posix_spawn() is thread-safer than system() (POSIX.1-2001), avoids shell injection
	if (posix_spawn(&pid, "/usr/bin/ydotool", NULL, NULL, argv, envp) != 0) {
		report(RPT_WARNING, "G-Key Macro: posix_spawn failed for ydotool: %s",
		       strerror_l(errno, LC_GLOBAL_LOCALE));
		return -1;
	}

	if (waitpid(pid, &status, 0) == -1) {
		report(RPT_WARNING, "G-Key Macro: waitpid failed: %s",
		       strerror_l(errno, LC_GLOBAL_LOCALE));
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		report(RPT_WARNING, "G-Key Macro: ydotool command failed: %s", command);
		return -1;
	}

	return 0;
}

/**
 * \brief Play back recorded macro for G-key
 * \param g_key G-key name (G1-G18)
 *
 * \details Executes stored macro commands for current mode and G-key.
 * Supports type:TEXT, key:KEYNAME, and delay:MS command formats.
 * Returns silently if no macro defined.
 */
static void play_macro(const char *g_key)
{
	int mode_idx = get_mode_index(macro_state.current_mode);
	int gkey_idx = get_gkey_index(g_key);

	if (gkey_idx < 0) {
		report(RPT_WARNING, "G-Key Macro: Invalid G-key: %s", g_key);
		return;
	}

	macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];
	if (macro->command_count == 0) {
		report(RPT_INFO, "G-Key Macro: No macro defined for %s in mode %s", g_key,
		       macro_state.current_mode);
		return;
	}

	report(RPT_DEBUG, "G-Key Macro: Playing macro for %s in mode %s (%d commands)", g_key,
	       macro_state.current_mode, macro->command_count);

	// Execute commands: type:TEXT, key:KEYNAME, delay:MS
	for (int i = 0; i < macro->command_count; i++) {
		const char *cmd = macro->commands[i];

		if (strncmp(cmd, "type:", 5) == 0) {
			char cmd_buffer[256];
			snprintf(cmd_buffer, sizeof(cmd_buffer), "type \"%s\"", cmd + 5);
			execute_ydotool_command(cmd_buffer);

		} else if (strncmp(cmd, "key:", 4) == 0) {
			char cmd_buffer[256];
			snprintf(cmd_buffer, sizeof(cmd_buffer), "key %s", cmd + 4);
			execute_ydotool_command(cmd_buffer);
			report(RPT_DEBUG, "G-Key Macro: Executing ydotool key %s", cmd + 4);

		} else if (strncmp(cmd, "delay:", 6) == 0) {
			int delay_ms = atoi(cmd + 6);
			if (delay_ms > 0 && delay_ms < 5000) {
				usleep(delay_ms * 1000);
			}
		}

		usleep(50000);
	}
}

/**
 * \brief Begin recording new macro for G-key
 * \param g_key G-key name to record macro for (G1-G18)
 *
 * \details Clears existing macro for this G-key and starts input recording
 * thread. Recording continues until stop_recording() called (MR key press).
 */
static void start_recording(const char *g_key)
{
	strncpy(macro_state.recording_target, g_key, sizeof(macro_state.recording_target) - 1);
	macro_state.recording_target[sizeof(macro_state.recording_target) - 1] = '\0';

	// Clear existing macro for this G-key
	int mode_idx = get_mode_index(macro_state.current_mode);
	int gkey_idx = get_gkey_index(g_key);

	if (gkey_idx >= 0) {
		macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];
		macro->command_count = 0;
		macro->created = time(NULL);
	}

	if (start_input_recording(g_key) == 0) {
		report(RPT_DEBUG, "G-Key Macro: Recording started for %s in mode %s", g_key,
		       macro_state.current_mode);
		report(RPT_DEBUG, "G-Key Macro: Press MR again to stop recording");

	} else {
		report(RPT_ERR, "G-Key Macro: Failed to start recording for %s", g_key);
		macro_state.recording = 0;
		gkey_macro_update_leds();
	}
}

/**
 * \brief Stop current macro recording
 *
 * \details Stops input recording thread, converts recorded events to ydotool
 * commands, and saves macro to configuration file.
 */
static void stop_recording(void)
{
	if (!macro_state.recording) {
		return;
	}

	macro_state.recording = 0;
	gkey_macro_update_leds();

	if (macro_state.recorder.recording) {
		stop_input_recording();
		convert_ydotool_recording();
	}

	save_macros();
	report(RPT_DEBUG, "G-Key Macro: Recording stopped");
}

/**
 * \brief Check if device is a keyboard
 * \param device_path Path to /dev/input/eventX device
 * \retval 1 Device is a keyboard
 * \retval 0 Device is not a keyboard or check failed
 *
 * \details Tests for EV_KEY support and typical keyboard keys (Q/A/Z)
 * to filter out mice and touchpads.
 */
static int is_keyboard_device(const char *device_path)
{
	int fd = open(device_path, O_RDONLY);
	if (fd < 0)
		return 0;

	unsigned long evbit = 0;
	unsigned long keybit[NLONGS(KEY_MAX)] = {0};

	// Check if device supports EV_KEY events
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), &evbit) < 0) {
		close(fd);
		return 0;
	}

	if (!(evbit & (1 << EV_KEY))) {
		close(fd);
		return 0;
	}

	if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit) < 0) {
		close(fd);
		return 0;
	}

	close(fd);

	// Check for typical keyboard keys (Q, A, Z) to filter out mice/touchpads
	return (test_bit(KEY_Q, keybit) || test_bit(KEY_A, keybit) || test_bit(KEY_Z, keybit));
}

/**
 * \brief Open input devices
 * \return Return value
 */
static int open_input_devices(void)
{
	DIR *dir = opendir("/dev/input");
	if (!dir) {
		report(RPT_ERR, "G-Key Macro: Cannot open /dev/input directory");
		return -1;
	}

	struct dirent *entry;
	macro_state.recorder.event_fd_count = 0;

	// Scan /dev/input for keyboard event devices (up to 32 max)
	// readdir() is thread-safe on Linux (glibc uses per-DIR lock for different directory
	// streams)
	while ((entry = safe_readdir(dir)) != NULL && macro_state.recorder.event_fd_count < 32) {
		if (strncmp(entry->d_name, "event", 5) != 0)
			continue;

		char device_path[256];
		snprintf(device_path, sizeof(device_path), "/dev/input/%s", entry->d_name);

		if (!is_keyboard_device(device_path)) {
			continue;
		}

		int fd = open(device_path, O_RDONLY | O_NONBLOCK);
		if (fd >= 0) {
			macro_state.recorder.event_fds[macro_state.recorder.event_fd_count] = fd;
			macro_state.recorder.event_fd_count++;
			report(RPT_DEBUG, "G-Key Macro: Opened input device %s (fd=%d)",
			       device_path, fd);
		} else {
			char err_buf[256];
			strerror_r(errno, err_buf, sizeof(err_buf));
			report(RPT_WARNING, "G-Key Macro: Failed to open %s: %s", device_path,
			       err_buf);
		}
	}

	closedir(dir);

	if (macro_state.recorder.event_fd_count == 0) {
		report(RPT_ERR,
		       "G-Key Macro: No input devices accessible - may need root privileges");
		return -1;
	}

	report(RPT_INFO, "G-Key Macro: Opened %d input devices for recording",
	       macro_state.recorder.event_fd_count);
	return 0;
}

/**
 * \brief Close all input devices
 *
 * \details Closes all file descriptors in macro_state.recorder.event_fds.
 * Resets event_fd_count to 0.
 */
static void close_input_devices(void)
{
	for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
		if (macro_state.recorder.event_fds[i] >= 0) {
			close(macro_state.recorder.event_fds[i]);
			macro_state.recorder.event_fds[i] = -1;
		}
	}

	macro_state.recorder.event_fd_count = 0;
}

/**
 * \brief Key code to ydotool name mapping entry
 * \details Maps Linux input event key codes to ydotool key names.
 * Used for translating recorded input events into ydotool replay commands.
 */
typedef struct {
	int code;	  ///< Linux input event key code (e.g., KEY_A)
	const char *name; ///< Corresponding ydotool key name (e.g., "a")
} key_mapping_t;

/** \brief Lookup table mapping Linux input key codes to ydotool key names
 *
 * \details Static array containing all supported key translations for macro replay.
 * Used by translate_key_code() to convert recorded input events into ydotool commands.
 */
static const key_mapping_t key_mappings[] = {
    // Alphabetic keys (A-Z)
    {KEY_A, "a"},
    {KEY_B, "b"},
    {KEY_C, "c"},
    {KEY_D, "d"},
    {KEY_E, "e"},
    {KEY_F, "f"},
    {KEY_G, "g"},
    {KEY_H, "h"},
    {KEY_I, "i"},
    {KEY_J, "j"},
    {KEY_K, "k"},
    {KEY_L, "l"},
    {KEY_M, "m"},
    {KEY_N, "n"},
    {KEY_O, "o"},
    {KEY_P, "p"},
    {KEY_Q, "q"},
    {KEY_R, "r"},
    {KEY_S, "s"},
    {KEY_T, "t"},
    {KEY_U, "u"},
    {KEY_V, "v"},
    {KEY_W, "w"},
    {KEY_X, "x"},
    {KEY_Y, "y"},
    {KEY_Z, "z"},

    // Numeric keys (0-9)
    {KEY_1, "1"},
    {KEY_2, "2"},
    {KEY_3, "3"},
    {KEY_4, "4"},
    {KEY_5, "5"},
    {KEY_6, "6"},
    {KEY_7, "7"},
    {KEY_8, "8"},
    {KEY_9, "9"},
    {KEY_0, "0"},

    // Special keys
    {KEY_SPACE, "space"},
    {KEY_ENTER, "Return"},
    {KEY_TAB, "Tab"},
    {KEY_BACKSPACE, "BackSpace"},
    {KEY_DELETE, "Delete"},
    {KEY_ESC, "Escape"},

    // Modifier keys
    {KEY_LEFTSHIFT, "shift"},
    {KEY_RIGHTSHIFT, "shift"},
    {KEY_LEFTCTRL, "ctrl"},
    {KEY_RIGHTCTRL, "ctrl"},
    {KEY_LEFTALT, "alt"},
    {KEY_RIGHTALT, "altgr"},

    // Arrow keys
    {KEY_UP, "Up"},
    {KEY_DOWN, "Down"},
    {KEY_LEFT, "Left"},
    {KEY_RIGHT, "Right"},

    // Function keys (F1-F12)
    {KEY_F1, "F1"},
    {KEY_F2, "F2"},
    {KEY_F3, "F3"},
    {KEY_F4, "F4"},
    {KEY_F5, "F5"},
    {KEY_F6, "F6"},
    {KEY_F7, "F7"},
    {KEY_F8, "F8"},
    {KEY_F9, "F9"},
    {KEY_F10, "F10"},
    {KEY_F11, "F11"},
    {KEY_F12, "F12"},

    // Punctuation
    {KEY_BACKSLASH, "backslash"},

    // Terminator
    {0, NULL}};

/**
 * \brief Translate Linux key code to ydotool name
 * \param code Linux input key code (KEY_* constant)
 * \retval non-NULL ydotool key name string
 * \retval NULL Key code not recognized
 *
 * \details Linear search through key_mappings lookup table.
 * Returns ydotool-compatible key names for macro recording.
 */
static const char *translate_key_code(int code)
{
	// Linear search through lookup table
	for (const key_mapping_t *m = key_mappings; m->name != NULL; m++) {
		if (m->code == code)
			return m->name;
	}
	return NULL;
}

/**
 * \brief Input recording thread
 * \param arg void *arg
 * \return Return value
 */
static void *input_recording_thread(void *arg)
{
	(void)arg;

	FILE *record_file = fopen(macro_state.recorder.record_file, "w");

	if (!record_file) {
		report(RPT_ERR, "G-Key Macro: Cannot create recording file %s",
		       macro_state.recorder.record_file);
		return NULL;
	}

	report(RPT_DEBUG, "G-Key Macro: Recording thread started, writing to %s",
	       macro_state.recorder.record_file);

	struct input_event ev;
	fd_set readfds;
	int max_fd = 0;

	// Find highest file descriptor for select()
	for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
		if (macro_state.recorder.event_fds[i] > max_fd) {
			max_fd = macro_state.recorder.event_fds[i];
		}
	}

	time_t last_event_time = time(NULL);
	int recorded_events = 0;

	// Main recording loop (limit: 1000 events to prevent infinite macros)
	while (!macro_state.recorder.stop_recording && recorded_events < 1000) {
		FD_ZERO(&readfds);

		for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
			FD_SET(macro_state.recorder.event_fds[i], &readfds);
		}

		struct timeval timeout = {0, 100000};
		int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

		if (ready < 0) {
			if (errno == EINTR)
				continue;
			// strerror_l() is thread-safe (POSIX.1-2008) and uses C locale for
			// consistent messages
			report(RPT_ERR, "G-Key Macro: select() error: %s",
			       strerror_l(errno, LC_GLOBAL_LOCALE));
			break;
		}

		if (ready == 0)
			continue;

		// Check each file descriptor for input events
		for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
			int fd = macro_state.recorder.event_fds[i];
			if (!FD_ISSET(fd, &readfds))
				continue;

			ssize_t bytes = read(fd, &ev, sizeof(ev));
			if (bytes != sizeof(ev))
				continue;

			time_t current_time = time(NULL);

			// Record timing delays between keypresses (minimum 100ms)
			if (recorded_events > 0 && (current_time - last_event_time) > 0) {
				int delay_ms = (current_time - last_event_time) * 1000;
				if (delay_ms >= 100) {
					fprintf(record_file, "delay:%d\n", delay_ms);
					recorded_events++;
				}
			}

			// Only record key presses (value=1), ignore releases (value=0)
			if (ev.type == EV_KEY && ev.value == 1) {
				const char *key_name = translate_key_code(ev.code);
				if (key_name) {
					fprintf(record_file, "key:%s\n", key_name);
					recorded_events++;
					last_event_time = current_time;
					report(RPT_DEBUG, "G-Key Macro: Recorded key press: %s",
					       key_name);
				}
			} else if (ev.type == EV_REL) {
				// Skip mouse movement events
			}
		}
	}

	fclose(record_file);
	report(RPT_DEBUG, "G-Key Macro: Recording finished, captured %d events", recorded_events);
	macro_state.recorder.recording = 0;

	return NULL;
}

/**
 * \brief Convert recording to ydotool commands
 *
 * \details Parses recorded input events, optimizes consecutive text into
 * type: commands, converts key events to key: commands, adds delay: commands.
 * Stores result in current mode/G-key macro slot.
 */
static void convert_ydotool_recording(void)
{
	FILE *file = fopen(macro_state.recorder.record_file, "r");
	if (!file) {
		report(RPT_WARNING, "G-Key Macro: Could not read recording file %s",
		       macro_state.recorder.record_file);
		return;
	}

	int mode_idx = get_mode_index(macro_state.current_mode);
	int gkey_idx = get_gkey_index(macro_state.recording_target);

	if (gkey_idx < 0) {
		fclose(file);
		return;
	}

	macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];
	macro->command_count = 0;
	macro->created = time(NULL);

	char line[512];
	char text_buffer[256] = "";
	int collecting_text = 0;

	while (fgets(line, sizeof(line), file) && macro->command_count < 10) {
		char *newline = strchr(line, '\n');
		if (newline)
			*newline = '\0';

		if (strncmp(line, "key:", 4) == 0) {
			const char *key = line + 4;

			// Check if it's a regular character (a-z, 0-9, space)
			if (strlen(key) == 1 &&
			    ((key[0] >= 'a' && key[0] <= 'z') || (key[0] >= '0' && key[0] <= '9') ||
			     key[0] == ' ')) {

				// Collect consecutive characters for text typing
				if (!collecting_text) {
					collecting_text = 1;
					text_buffer[0] = '\0';
				}
				strncat(text_buffer, key, 1);

				// Special key - flush text buffer first, then add special key
			} else {
				if (collecting_text && strlen(text_buffer) > 0) {
					snprintf(macro->commands[macro->command_count], 256,
						 "type:%s", text_buffer);
					macro->command_count++;
					text_buffer[0] = '\0';
					collecting_text = 0;
				}

				if (macro->command_count < 10) {
					strncpy(macro->commands[macro->command_count], line, 255);
					macro->commands[macro->command_count][255] = '\0';
					macro->command_count++;
				}
			}

			// Non-key command (delay) - flush text buffer first
		} else {
			if (collecting_text && strlen(text_buffer) > 0) {
				snprintf(macro->commands[macro->command_count], 256, "type:%s",
					 text_buffer);
				macro->command_count++;
				text_buffer[0] = '\0';
				collecting_text = 0;
			}

			if (macro->command_count < 10) {
				strncpy(macro->commands[macro->command_count], line, 255);
				macro->commands[macro->command_count][255] = '\0';
				macro->command_count++;
			}
		}
	}

	// Flush remaining text buffer
	if (collecting_text && strlen(text_buffer) > 0 && macro->command_count < 10) {
		snprintf(macro->commands[macro->command_count], 256, "type:%s", text_buffer);
		macro->command_count++;
	}

	fclose(file);

	if (unlink(macro_state.recorder.record_file) != 0) {
		report(RPT_WARNING, "G-Key Macro: Failed to delete temp file %s",
		       macro_state.recorder.record_file);
	}

	if (macro->command_count == 0) {
		report(RPT_WARNING, "G-Key Macro: No actions recorded for %s",
		       macro_state.recording_target);

	} else {
		report(RPT_INFO, "G-Key Macro: Converted %d actions for %s", macro->command_count,
		       macro_state.recording_target);
	}
}

// Start input event recording from /dev/input/event* devices
int start_input_recording(const char *target_gkey)
{
	if (macro_state.recorder.recording) {
		report(RPT_WARNING, "G-Key Macro: Recording already in progress");
		return -1;
	}

	if (open_input_devices() != 0) {
		return -1;
	}

	// Set up temp file path: ~/.config/lcdproc/recording_MODE_GKEY.tmp
	// Use thread-safe cached environment variable
	const char *home = env_get_home();

	if (home) {
		snprintf(macro_state.recorder.record_file, sizeof(macro_state.recorder.record_file),
			 "%s/.config/lcdproc/recording_%s_%s.tmp", home, macro_state.current_mode,
			 target_gkey);

	} else {
		snprintf(macro_state.recorder.record_file, sizeof(macro_state.recorder.record_file),
			 "/tmp/lcdproc_recording_%s_%s.tmp", macro_state.current_mode, target_gkey);
	}

	macro_state.recorder.recording = 1;
	macro_state.recorder.stop_recording = 0;
	macro_state.recorder.record_start_time = time(NULL);

	// Create background pthread for input event monitoring
	if (pthread_create(&macro_state.recorder.record_thread, NULL, input_recording_thread,
			   NULL) != 0) {
		report(RPT_ERR, "G-Key Macro: Failed to create recording thread");
		close_input_devices();
		macro_state.recorder.recording = 0;
		return -1;
	}

	return 0;
}

// Stop input event recording and convert to ydotool commands
int stop_input_recording(void)
{
	if (!macro_state.recorder.recording) {
		return 0;
	}

	// Signal thread to stop
	macro_state.recorder.stop_recording = 1;

	// Wait for thread completion
	if (macro_state.recorder.record_thread != 0) {
		pthread_join(macro_state.recorder.record_thread, NULL);
		macro_state.recorder.record_thread = 0;
	}

	close_input_devices();

	macro_state.recorder.recording = 0;
	macro_state.recorder.stop_recording = 0;

	return 0;
}
