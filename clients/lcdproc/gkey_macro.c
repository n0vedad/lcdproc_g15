/** \file clients/lcdproc/gkey_macro.c
 * G-Key macro support for LCDproc client
 * Integrates with ydotool for Wayland compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/input.h>
#include <dirent.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>

#include "gkey_macro.h"

/* Bit manipulation macros for input device capabilities */
#define NLONGS(x) (((x) + 8 * sizeof(long) - 1) / (8 * sizeof(long)))
#define test_bit(bit, array)                                                                       \
	((array)[(bit) / (8 * sizeof(long))] & (1UL << ((bit) % (8 * sizeof(long)))))
#include "main.h"
#include "shared/report.h"

/* Simple macro storage structure */
typedef struct {
	char commands[10][256]; /* Up to 10 commands per macro */
	int command_count;
	time_t created;
} macro_t;

/* Input recording thread data */
typedef struct {
	int recording;
	int stop_recording;
	char record_file[256];
	pthread_t record_thread;
	int event_fds[32]; /* File descriptors for /dev/input/event* */
	int event_fd_count;
	time_t record_start_time;
} input_recorder_t;

/* Macro system state */
static struct {
	char current_mode[8];	   /* M1, M2, M3 */
	int recording;		   /* Recording flag */
	char recording_target[8];  /* Target G-key for recording */
	macro_t macros[3][18];	   /* 3 modes x 18 G-keys */
	char config_file[256];	   /* Config file path */
	time_t last_key_time;	   /* For timing in recordings */
	pid_t ydotool_record_pid;  /* PID of ydotool recording process */
	char record_file[256];	   /* Temporary recording file */
	input_recorder_t recorder; /* Input event recorder */
} macro_state = {.current_mode = "M1",
		 .recording = 0,
		 .recording_target = "",
		 .macros = {{{0}}},
		 .config_file = "", /* Will be set in init */
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

/* Forward declarations */
static void
save_macros(void);
static void
load_macros(void);
static int
execute_ydotool_command(const char *command);
static void
play_macro(const char *g_key);
static void
start_recording(const char *g_key);
static void
stop_recording(void);
static void
convert_ydotool_recording(void);
static void *
input_recording_thread(void *arg);
static int
open_input_devices(void);
static void
close_input_devices(void);
static const char *
translate_key_code(int code);
static int
is_keyboard_device(const char *device_path);
static int
is_mouse_device(const char *device_path);

int
gkey_macro_init(void)
{
	/* Initialize macro storage */
	memset(&macro_state.macros, 0, sizeof(macro_state.macros));

	/* Set config file path to ~/.config/lcdproc/g15_macros.json */
	const char *home = getenv("HOME");
	if (home) {
		snprintf(macro_state.config_file, sizeof(macro_state.config_file),
			 "%s/.config/lcdproc/g15_macros.json", home);

		/* Create directory if it doesn't exist */
		char dir_path[256];
		snprintf(dir_path, sizeof(dir_path), "%s/.config/lcdproc", home);
		mkdir(dir_path, 0755); /* Create if needed, ignore errors */
	} else {
		strcpy(macro_state.config_file, "/tmp/lcdproc_g15_macros.json");
	}

	/* Load existing macros */
	load_macros();

	report(RPT_INFO, "G-Key Macro: Initialized (Mode: %s)", macro_state.current_mode);
	return 0;
}

void
gkey_macro_cleanup(void)
{
	/* Stop any active recording */
	if (macro_state.recorder.recording) {
		stop_input_recording();
	}

	/* Save macros before cleanup */
	save_macros();
}

void
gkey_macro_handle_key(const char *key_name)
{
	if (!key_name)
		return;

	report(RPT_DEBUG, "G-Key Macro: Key pressed: %s", key_name);

	if (strcmp(key_name, "MR") == 0) {
		if (macro_state.recording) {
			stop_recording();
		} else {
			macro_state.recording = 1;
			report(RPT_INFO, "G-Key Macro: Recording mode active - press a G-key to "
					 "start recording");
		}
	} else if (strncmp(key_name, "M", 1) == 0 && strlen(key_name) == 2) {
		/* Mode switch: M1, M2, M3 */
		strncpy(macro_state.current_mode, key_name, sizeof(macro_state.current_mode) - 1);
		macro_state.current_mode[sizeof(macro_state.current_mode) - 1] = '\0';
		report(RPT_INFO, "G-Key Macro: Switched to mode %s", macro_state.current_mode);
	} else if (strncmp(key_name, "G", 1) == 0 && strlen(key_name) > 1) {
		/* G-key press: G1-G18 */
		if (macro_state.recording) {
			start_recording(key_name);
		} else {
			play_macro(key_name);
		}
	}

	macro_state.last_key_time = time(NULL);
}

void
gkey_macro_process(void)
{
	/* Process any pending macro operations */
	/* This could be extended for timing-sensitive operations */
}

const char *
gkey_macro_get_mode(void)
{
	return macro_state.current_mode;
}

int
gkey_macro_is_recording(void)
{
	return macro_state.recording;
}

static int
get_mode_index(const char *mode)
{
	if (strcmp(mode, "M1") == 0)
		return 0;
	if (strcmp(mode, "M2") == 0)
		return 1;
	if (strcmp(mode, "M3") == 0)
		return 2;
	return 0; /* Default to M1 */
}

static int
get_gkey_index(const char *gkey)
{
	if (strncmp(gkey, "G", 1) == 0) {
		int num = atoi(gkey + 1);
		if (num >= 1 && num <= 18) {
			return num - 1; /* 0-based index */
		}
	}
	return -1;
}

static void
load_macros(void)
{
	FILE *file = fopen(macro_state.config_file, "r");
	if (!file) {
		report(RPT_DEBUG, "G-Key Macro: No existing config file, using defaults");
		return;
	}

	char line[512];
	while (fgets(line, sizeof(line), file)) {
		/* Simple format: MODE GKEY COMMAND_COUNT COMMAND1 COMMAND2 ... */
		char mode[8], gkey[8];
		int cmd_count;

		if (sscanf(line, "%7s %7s %d", mode, gkey, &cmd_count) == 3) {
			int mode_idx = get_mode_index(mode);
			int gkey_idx = get_gkey_index(gkey);

			if (gkey_idx >= 0 && cmd_count > 0 && cmd_count <= 10) {
				macro_state.macros[mode_idx][gkey_idx].command_count = cmd_count;

				/* Read commands from rest of line */
				char *ptr = line;
				/* Skip mode, gkey, cmd_count */
				for (int i = 0; i < 3; i++) {
					ptr = strchr(ptr, ' ');
					if (ptr)
						ptr++;
				}

				/* Read commands separated by | */
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
						/* Last command */
						strncpy(macro_state.macros[mode_idx][gkey_idx]
							    .commands[i],
							ptr, 255);
						macro_state.macros[mode_idx][gkey_idx]
						    .commands[i][255] = '\0';
						/* Remove newline */
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

static void
save_macros(void)
{
	FILE *file = fopen(macro_state.config_file, "w");
	if (!file) {
		report(RPT_ERR, "G-Key Macro: Failed to save macros to %s",
		       macro_state.config_file);
		return;
	}

	const char *modes[] = {"M1", "M2", "M3"};

	for (int mode_idx = 0; mode_idx < 3; mode_idx++) {
		for (int gkey_idx = 0; gkey_idx < 18; gkey_idx++) {
			macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];
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

static int
execute_ydotool_command(const char *command)
{
	char cmd_buffer[512];
	snprintf(cmd_buffer, sizeof(cmd_buffer), "YDOTOOL_SOCKET=/tmp/.ydotool_socket ydotool %s",
		 command);

	int result = system(cmd_buffer);
	if (result != 0) {
		report(RPT_WARNING, "G-Key Macro: ydotool command failed: %s", command);
		return -1;
	}

	return 0;
}

static void
play_macro(const char *g_key)
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

	report(RPT_INFO, "G-Key Macro: Playing macro for %s in mode %s (%d commands)", g_key,
	       macro_state.current_mode, macro->command_count);

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
		} else if (strncmp(cmd, "mousemove:", 10) == 0) {
			char cmd_buffer[256];
			snprintf(cmd_buffer, sizeof(cmd_buffer), "mousemove %s", cmd + 10);
			execute_ydotool_command(cmd_buffer);
		} else if (strncmp(cmd, "mousemove_rel:", 14) == 0) {
			char cmd_buffer[256];
			snprintf(cmd_buffer, sizeof(cmd_buffer), "mousemove_relative %s", cmd + 14);
			execute_ydotool_command(cmd_buffer);
		} else if (strncmp(cmd, "click:", 6) == 0) {
			char cmd_buffer[256];
			snprintf(cmd_buffer, sizeof(cmd_buffer), "click %s", cmd + 6);
			execute_ydotool_command(cmd_buffer);
		}

		/* Small delay between commands */
		usleep(50000); /* 50ms */
	}
}

static void
start_recording(const char *g_key)
{
	strncpy(macro_state.recording_target, g_key, sizeof(macro_state.recording_target) - 1);
	macro_state.recording_target[sizeof(macro_state.recording_target) - 1] = '\0';

	/* Clear any existing macro first */
	int mode_idx = get_mode_index(macro_state.current_mode);
	int gkey_idx = get_gkey_index(g_key);

	if (gkey_idx >= 0) {
		macro_t *macro = &macro_state.macros[mode_idx][gkey_idx];
		macro->command_count = 0;
		macro->created = time(NULL);
	}

	/* Start real input event recording */
	if (start_input_recording(g_key) == 0) {
		report(RPT_INFO, "G-Key Macro: Recording started for %s in mode %s", g_key,
		       macro_state.current_mode);
		report(RPT_INFO, "G-Key Macro: Press MR again to stop recording");
	} else {
		report(RPT_ERR, "G-Key Macro: Failed to start recording for %s", g_key);
		macro_state.recording = 0;
	}
}

static void
stop_recording(void)
{
	if (!macro_state.recording) {
		return;
	}

	macro_state.recording = 0;

	/* Stop input recording if active */
	if (macro_state.recorder.recording) {
		stop_input_recording();
		convert_ydotool_recording();
	}

	save_macros();
	report(RPT_INFO, "G-Key Macro: Recording stopped");
}

static int
is_keyboard_device(const char *device_path)
{
	int fd = open(device_path, O_RDONLY);
	if (fd < 0)
		return 0;

	unsigned long evbit = 0;
	unsigned long keybit[NLONGS(KEY_MAX)] = {0};

	/* Check if device supports key events */
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), &evbit) < 0) {
		close(fd);
		return 0;
	}

	if (!(evbit & (1 << EV_KEY))) {
		close(fd);
		return 0;
	}

	/* Check if it has keyboard keys (not just mouse buttons) */
	if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit) < 0) {
		close(fd);
		return 0;
	}

	close(fd);

	/* Check for typical keyboard keys */
	return (test_bit(KEY_Q, keybit) || test_bit(KEY_A, keybit) || test_bit(KEY_Z, keybit));
}

static int
is_mouse_device(const char *device_path)
{
	int fd = open(device_path, O_RDONLY);
	if (fd < 0)
		return 0;

	unsigned long evbit = 0;
	unsigned long keybit[NLONGS(KEY_MAX)] = {0};
	unsigned long relbit[NLONGS(REL_MAX)] = {0};

	/* Check if device supports button and relative events */
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), &evbit) < 0) {
		close(fd);
		return 0;
	}

	int has_key = evbit & (1 << EV_KEY);
	int has_rel = evbit & (1 << EV_REL);

	if (has_key) {
		ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit);
	}
	if (has_rel) {
		ioctl(fd, EVIOCGBIT(EV_REL, REL_MAX), relbit);
	}

	close(fd);

	/* Mouse should have mouse buttons and relative movement */
	return (has_key && test_bit(BTN_LEFT, keybit)) ||
	       (has_rel && (test_bit(REL_X, relbit) || test_bit(REL_Y, relbit)));
}

static int
open_input_devices(void)
{
	DIR *dir = opendir("/dev/input");
	if (!dir) {
		report(RPT_ERR, "G-Key Macro: Cannot open /dev/input directory");
		return -1;
	}

	struct dirent *entry;
	macro_state.recorder.event_fd_count = 0;

	while ((entry = readdir(dir)) != NULL && macro_state.recorder.event_fd_count < 32) {
		if (strncmp(entry->d_name, "event", 5) != 0)
			continue;

		char device_path[256];
		snprintf(device_path, sizeof(device_path), "/dev/input/%s", entry->d_name);

		/* Only open keyboard and mouse devices */
		if (!is_keyboard_device(device_path) && !is_mouse_device(device_path)) {
			continue;
		}

		int fd = open(device_path, O_RDONLY | O_NONBLOCK);
		if (fd >= 0) {
			macro_state.recorder.event_fds[macro_state.recorder.event_fd_count] = fd;
			macro_state.recorder.event_fd_count++;
			report(RPT_DEBUG, "G-Key Macro: Opened input device %s (fd=%d)",
			       device_path, fd);
		} else {
			report(RPT_WARNING, "G-Key Macro: Failed to open %s: %s", device_path,
			       strerror(errno));
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

static void
close_input_devices(void)
{
	for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
		if (macro_state.recorder.event_fds[i] >= 0) {
			close(macro_state.recorder.event_fds[i]);
			macro_state.recorder.event_fds[i] = -1;
		}
	}
	macro_state.recorder.event_fd_count = 0;
}

static const char *
translate_key_code(int code)
{
	/* Translate Linux input key codes to ydotool key names */
	switch (code) {
	case KEY_A:
		return "a";
	case KEY_B:
		return "b";
	case KEY_C:
		return "c";
	case KEY_D:
		return "d";
	case KEY_E:
		return "e";
	case KEY_F:
		return "f";
	case KEY_G:
		return "g";
	case KEY_H:
		return "h";
	case KEY_I:
		return "i";
	case KEY_J:
		return "j";
	case KEY_K:
		return "k";
	case KEY_L:
		return "l";
	case KEY_M:
		return "m";
	case KEY_N:
		return "n";
	case KEY_O:
		return "o";
	case KEY_P:
		return "p";
	case KEY_Q:
		return "q";
	case KEY_R:
		return "r";
	case KEY_S:
		return "s";
	case KEY_T:
		return "t";
	case KEY_U:
		return "u";
	case KEY_V:
		return "v";
	case KEY_W:
		return "w";
	case KEY_X:
		return "x";
	case KEY_Y:
		return "y";
	case KEY_Z:
		return "z";
	case KEY_1:
		return "1";
	case KEY_2:
		return "2";
	case KEY_3:
		return "3";
	case KEY_4:
		return "4";
	case KEY_5:
		return "5";
	case KEY_6:
		return "6";
	case KEY_7:
		return "7";
	case KEY_8:
		return "8";
	case KEY_9:
		return "9";
	case KEY_0:
		return "0";
	case KEY_SPACE:
		return "space";
	case KEY_ENTER:
		return "Return";
	case KEY_TAB:
		return "Tab";
	case KEY_BACKSPACE:
		return "BackSpace";
	case KEY_DELETE:
		return "Delete";
	case KEY_ESC:
		return "Escape";
	case KEY_LEFTSHIFT:
		return "shift";
	case KEY_RIGHTSHIFT:
		return "shift";
	case KEY_LEFTCTRL:
		return "ctrl";
	case KEY_RIGHTCTRL:
		return "ctrl";
	case KEY_LEFTALT:
		return "alt";
	case KEY_RIGHTALT:
		return "alt";
	case KEY_UP:
		return "Up";
	case KEY_DOWN:
		return "Down";
	case KEY_LEFT:
		return "Left";
	case KEY_RIGHT:
		return "Right";
	case KEY_F1:
		return "F1";
	case KEY_F2:
		return "F2";
	case KEY_F3:
		return "F3";
	case KEY_F4:
		return "F4";
	case KEY_F5:
		return "F5";
	case KEY_F6:
		return "F6";
	case KEY_F7:
		return "F7";
	case KEY_F8:
		return "F8";
	case KEY_F9:
		return "F9";
	case KEY_F10:
		return "F10";
	case KEY_F11:
		return "F11";
	case KEY_F12:
		return "F12";
	default:
		return NULL;
	}
}

static void *
input_recording_thread(void *arg)
{
	(void)arg; /* Unused parameter */

	FILE *record_file = fopen(macro_state.recorder.record_file, "w");
	if (!record_file) {
		report(RPT_ERR, "G-Key Macro: Cannot create recording file %s",
		       macro_state.recorder.record_file);
		return NULL;
	}

	report(RPT_INFO, "G-Key Macro: Recording thread started, writing to %s",
	       macro_state.recorder.record_file);

	struct input_event ev;
	fd_set readfds;
	int max_fd = 0;

	/* Find maximum file descriptor for select() */
	for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
		if (macro_state.recorder.event_fds[i] > max_fd) {
			max_fd = macro_state.recorder.event_fds[i];
		}
	}

	time_t last_event_time = time(NULL);
	int recorded_events = 0;

	while (!macro_state.recorder.stop_recording && recorded_events < 1000) {
		FD_ZERO(&readfds);

		/* Add all event file descriptors to set */
		for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
			FD_SET(macro_state.recorder.event_fds[i], &readfds);
		}

		struct timeval timeout = {0, 100000}; /* 100ms timeout */
		int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

		if (ready < 0) {
			if (errno == EINTR)
				continue;
			report(RPT_ERR, "G-Key Macro: select() error: %s", strerror(errno));
			break;
		}

		if (ready == 0)
			continue; /* Timeout */

		/* Check each file descriptor */
		for (int i = 0; i < macro_state.recorder.event_fd_count; i++) {
			int fd = macro_state.recorder.event_fds[i];
			if (!FD_ISSET(fd, &readfds))
				continue;

			ssize_t bytes = read(fd, &ev, sizeof(ev));
			if (bytes != sizeof(ev))
				continue;

			time_t current_time = time(NULL);

			/* Add delay if significant time passed */
			if (recorded_events > 0 && (current_time - last_event_time) > 0) {
				int delay_ms = (current_time - last_event_time) * 1000;
				fprintf(record_file, "delay:%d\n", delay_ms);
				recorded_events++;
			}

			if (ev.type == EV_KEY && ev.value == 1) { /* Key press */
				const char *key_name = translate_key_code(ev.code);
				if (key_name) {
					fprintf(record_file, "key:%s\n", key_name);
					recorded_events++;
					last_event_time = current_time;
					report(RPT_DEBUG, "G-Key Macro: Recorded key press: %s",
					       key_name);
				}
			} else if (ev.type == EV_KEY &&
				   ev.value == 0) {	/* Key release - ignore for now */
							/* Could add key release events if needed */
			} else if (ev.type == EV_REL) { /* Mouse movement */
				if (ev.code == REL_X || ev.code == REL_Y) {
					static int rel_x = 0, rel_y = 0;
					if (ev.code == REL_X)
						rel_x += ev.value;
					if (ev.code == REL_Y)
						rel_y += ev.value;

					/* Only record significant movements */
					if (abs(rel_x) > 5 || abs(rel_y) > 5) {
						fprintf(record_file, "mousemove_rel:%d %d\n", rel_x,
							rel_y);
						recorded_events++;
						last_event_time = current_time;
						rel_x = rel_y = 0;
						report(
						    RPT_DEBUG,
						    "G-Key Macro: Recorded mouse movement: %d %d",
						    rel_x, rel_y);
					}
				}
			} else if (ev.type == EV_KEY &&
				   (ev.code == BTN_LEFT || ev.code == BTN_RIGHT ||
				    ev.code == BTN_MIDDLE)) {
				/* Mouse button */
				if (ev.value == 1) { /* Press */
					const char *button = (ev.code == BTN_LEFT)    ? "1"
							     : (ev.code == BTN_RIGHT) ? "3"
										      : "2";
					fprintf(record_file, "click:%s\n", button);
					recorded_events++;
					last_event_time = current_time;
					report(RPT_DEBUG,
					       "G-Key Macro: Recorded mouse click: button %s",
					       button);
				}
			}
		}
	}

	fclose(record_file);

	report(RPT_INFO, "G-Key Macro: Recording finished, captured %d events", recorded_events);
	macro_state.recorder.recording = 0;
	return NULL;
}

static void
convert_ydotool_recording(void)
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
		/* Remove newline */
		char *newline = strchr(line, '\n');
		if (newline)
			*newline = '\0';

		if (strncmp(line, "key:", 4) == 0) {
			/* Check if it's a regular character key */
			const char *key = line + 4;
			if (strlen(key) == 1 &&
			    ((key[0] >= 'a' && key[0] <= 'z') || (key[0] >= '0' && key[0] <= '9') ||
			     key[0] == ' ')) {
				/* Collect characters for text typing */
				if (!collecting_text) {
					collecting_text = 1;
					text_buffer[0] = '\0';
				}
				strncat(text_buffer, key, 1);
			} else {
				/* Non-text key - flush collected text first */
				if (collecting_text && strlen(text_buffer) > 0) {
					snprintf(macro->commands[macro->command_count], 256,
						 "type:%s", text_buffer);
					macro->command_count++;
					text_buffer[0] = '\0';
					collecting_text = 0;
				}
				/* Then add the special key */
				if (macro->command_count < 10) {
					strncpy(macro->commands[macro->command_count], line, 255);
					macro->commands[macro->command_count][255] = '\0';
					macro->command_count++;
				}
			}
		} else {
			/* Non-key command - flush collected text first */
			if (collecting_text && strlen(text_buffer) > 0) {
				snprintf(macro->commands[macro->command_count], 256, "type:%s",
					 text_buffer);
				macro->command_count++;
				text_buffer[0] = '\0';
				collecting_text = 0;
			}
			/* Add the command */
			if (macro->command_count < 10) {
				strncpy(macro->commands[macro->command_count], line, 255);
				macro->commands[macro->command_count][255] = '\0';
				macro->command_count++;
			}
		}
	}

	/* Flush any remaining text */
	if (collecting_text && strlen(text_buffer) > 0 && macro->command_count < 10) {
		snprintf(macro->commands[macro->command_count], 256, "type:%s", text_buffer);
		macro->command_count++;
	}

	fclose(file);

	if (macro->command_count == 0) {
		report(RPT_WARNING, "G-Key Macro: No actions recorded for %s",
		       macro_state.recording_target);
	} else {
		report(RPT_INFO, "G-Key Macro: Converted %d actions for %s", macro->command_count,
		       macro_state.recording_target);
	}
}

int
start_input_recording(const char *target_gkey)
{
	if (macro_state.recorder.recording) {
		report(RPT_WARNING, "G-Key Macro: Recording already in progress");
		return -1;
	}

	/* Open input devices */
	if (open_input_devices() != 0) {
		return -1;
	}

	/* Set up recording file */
	const char *home = getenv("HOME");
	if (home) {
		snprintf(macro_state.recorder.record_file, sizeof(macro_state.recorder.record_file),
			 "%s/.config/lcdproc/recording_%s_%s.tmp", home, macro_state.current_mode,
			 target_gkey);
	} else {
		snprintf(macro_state.recorder.record_file, sizeof(macro_state.recorder.record_file),
			 "/tmp/lcdproc_recording_%s_%s.tmp", macro_state.current_mode, target_gkey);
	}

	/* Start recording thread */
	macro_state.recorder.recording = 1;
	macro_state.recorder.stop_recording = 0;
	macro_state.recorder.record_start_time = time(NULL);

	if (pthread_create(&macro_state.recorder.record_thread, NULL, input_recording_thread,
			   NULL) != 0) {
		report(RPT_ERR, "G-Key Macro: Failed to create recording thread");
		close_input_devices();
		macro_state.recorder.recording = 0;
		return -1;
	}

	return 0;
}

int
stop_input_recording(void)
{
	if (!macro_state.recorder.recording) {
		return 0;
	}

	/* Signal thread to stop */
	macro_state.recorder.stop_recording = 1;

	/* Wait for thread to finish */
	if (macro_state.recorder.record_thread != 0) {
		pthread_join(macro_state.recorder.record_thread, NULL);
		macro_state.recorder.record_thread = 0;
	}

	/* Close input devices */
	close_input_devices();

	macro_state.recorder.recording = 0;
	macro_state.recorder.stop_recording = 0;

	return 0;
}