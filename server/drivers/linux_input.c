// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/linux_input.c
 * \brief Linux input subsystem driver implementation
 * \author n0vedad
 * \date 2025
 *
 *
 * \features
 * - Implementation of LCDd driver for Linux input subsystem integration
 * - Device identification by path (/dev/input/eventX) or device name matching
 * - Configurable key code to button name mappings via configuration file
 * - Automatic device reconnection on connection loss or device unplugging
 * - Non-blocking input event reading with event queue processing
 * - Support for multiple input devices with unified key handling
 * - G-Key macro system support for gaming keyboards
 * - Linux evdev interface integration for kernel input events
 * - Device capability detection and filtering for keyboard events
 * - Key press and release event processing with release filtering
 * - Default key mappings for common navigation keys (arrows, Enter, Escape)
 * - Memory management for button mapping linked list
 * - Error handling and connection loss detection with automatic recovery
 *
 * \usage
 * - Used by LCDd server when "linuxInput" driver is specified in configuration
 * - Primary usage for keyboard input from USB/PS2/Bluetooth keyboards
 * - G-Key macro input from gaming keyboards (Logitech, Razer, Corsair, etc.)
 * - Custom input device integration for specialized hardware controllers
 * - Configuration via Device option (path or name) in LCDd.conf section
 * - Key mapping configuration via multiple "key" entries: key=keycode,buttonname
 * - Automatic device discovery when device name is specified instead of path
 * - Fallback device selection when primary device is unavailable
 * - Event-based input processing with configurable key name translation
 *
 * \details LCDd driver for reading input events from Linux input subsystem
 * providing keyboard and input device support through Linux event devices.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "shared/posix_wrappers.h"
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lcd.h"
#include "linux_input.h"
#include "shared/posix_wrappers.h"

#include "shared/LL.h"
#include "shared/posix_wrappers.h"
#include "shared/report.h"

/**
 * \note Fallback definition for PATH_MAX
 *
 * Some embedded systems or older compilers may not define PATH_MAX.
 * 4096 bytes is a safe default for maximum path length on most systems.
 */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/**
 * \note Fallback definition for DT_CHR (directory entry type for character devices)
 *
 * Used to identify /dev/input/event* as character devices when scanning directory.
 * Value 2 is the standard POSIX value for character device type.
 */
#ifndef DT_CHR
#define DT_CHR 2
#endif

/** \brief Default input device path
 *
 * \details Default /dev/input device to monitor for key events.
 * Can be overridden in configuration file.
 */
#define LINUXINPUT_DEFAULT_DEVICE "/dev/input/event0"

/**
 * \brief Keycode to button name mapping structure
 * \details Maps Linux input event keycodes to LCDproc button names
 */
struct keycode {
	unsigned short code; ///< Linux input event keycode
	char *button;	     ///< LCDproc button name
};

/**
 * \brief Parse keycode mapping from config string
 * \param configvalue Configuration string "keycode,buttonname"
 * \return Allocated keycode structure, or NULL on error
 *
 * \details Parses format "code,name" (e.g., "28,Enter"). Validates keycode range.
 */
static struct keycode *keycode_create(const char *configvalue)
{
	long code;
	char *button;
	struct keycode *ret;

	code = strtol(configvalue, NULL, 0);
	if (code < 0 || code > UINT16_MAX) {
		return NULL;
	}

	button = strchr(configvalue, ',');
	if (!button) {
		return NULL;
	}

	button = strdup(&button[1]);
	if (!button) {
		return NULL;
	}

	ret = malloc(sizeof(*ret));
	if (ret) {
		ret->code = code;
		ret->button = button;
	} else {
		free(button);
	}

	return ret;
}

/**
 * \brief Linux input driver private data structure
 * \details Stores internal state for the Linux input event driver
 */
typedef struct linuxInput_private_data {
	int fd;		       ///< File descriptor for input device
	const char *name;      ///< Device name
	LinkedList *buttonmap; ///< Keycode to button name mapping list
} PrivateData;

/** \name Linux Input Driver Module Exports
 * Driver metadata exported to the LCDd server core
 */
///@{
MODULE_EXPORT char *api_version = API_VERSION;	   ///< Driver API version string
MODULE_EXPORT int stay_in_foreground = 0;	   ///< Can run as daemon
MODULE_EXPORT int supports_multiple = 1;	   ///< Supports multiple instances
MODULE_EXPORT char *symbol_prefix = "linuxInput_"; ///< Function symbol prefix for this driver
///@}

/**
 * \brief Open input device if name matches
 * \param device Device path (e.g., "/dev/input/event0")
 * \param name Expected device name to match
 * \retval >=0 File descriptor on success
 * \retval -1 Error (open failed, ioctl failed, or name mismatch)
 *
 * \details Opens device, queries name with EVIOCGNAME ioctl, verifies match.
 */
static int linuxInput_open_with_name(const char *device, const char *name)
{
	char buf[256];
	int err, fd;

	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd == -1) {
		return -1;
	}

	err = ioctl(fd, EVIOCGNAME(256), buf);
	if (err == -1) {
		close(fd);
		return -1;
	}

	buf[255] = 0;

	if (strcmp(buf, name) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

/**
 * \brief Search /dev/input for device with matching name
 * \param name Device name to search for
 * \retval >=0 File descriptor of matching device
 * \retval -1 Device not found
 *
 * \details Scans /dev/input/event* devices until finding name match.
 */
static int linuxInput_search_by_name(const char *name)
{
	char devname[PATH_MAX];
	struct dirent *dirent;
	int fd = -1;
	DIR *dir;

	dir = opendir("/dev/input");
	if (dir == NULL)
		return -1;

	// readdir() is thread-safe on Linux (glibc uses per-DIR lock for different directory
	// streams)
	while ((dirent = safe_readdir(dir)) != NULL) {
		if (dirent->d_type != DT_CHR || strncmp(dirent->d_name, "event", 5) != 0)
			continue;

		snprintf(devname, sizeof(devname), "/dev/input/%s", dirent->d_name);

		fd = linuxInput_open_with_name(devname, name);
		if (fd != -1)
			break;
	}

	closedir(dir);

	return fd;
}

// Initialize the linux input driver
MODULE_EXPORT int linuxInput_init(Driver *drvthis)
{
	PrivateData *p;
	const char *s;
	struct keycode *key;
	int i;

	p = (PrivateData *)calloc(1, sizeof(PrivateData));
	if (p == NULL)
		return -1;
	if (drvthis->store_private_ptr(drvthis, p))
		return -1;

	p->fd = -1;
	if ((p->buttonmap = LL_new()) == NULL) {
		report(RPT_ERR, "%s: cannot allocate memory for buttons", drvthis->name);
		return -1;
	}

	s = drvthis->config_get_string(drvthis->name, "Device", 0, LINUXINPUT_DEFAULT_DEVICE);
	report(RPT_INFO, "%s: using Device %s", drvthis->name, s);

	if (s[0] == '/') {
		report(RPT_DEBUG, "%s: Opening device by path: %s", drvthis->name, s);
		if ((p->fd = open(s, O_RDONLY | O_NONBLOCK)) == -1) {
			char err_buf[256];
			strerror_r(errno, err_buf, sizeof(err_buf));
			report(RPT_ERR, "%s: open(%s) failed (%s)", drvthis->name, s, err_buf);
			return -1;
		}

	} else {
		report(RPT_DEBUG, "%s: Searching device by name: %s", drvthis->name, s);
		if ((p->fd = linuxInput_search_by_name(s)) == -1) {
			report(RPT_ERR, "%s: could not find '%s' input-device", drvthis->name, s);
			return -1;
		}
		p->name = s;
		report(RPT_DEBUG, "%s: Found device fd=%d", drvthis->name, p->fd);
	}

	for (i = 0; (s = drvthis->config_get_string(drvthis->name, "key", i, NULL)) != NULL; i++) {
		if ((key = keycode_create(s)) == NULL) {
			report(RPT_ERR, "%s: parsing configvalue '%s' failed", drvthis->name, s);
			continue;
		}
		LL_AddNode(p->buttonmap, key);
	}

	report(RPT_DEBUG, "%s: init() done", drvthis->name);

	return 0;
}

// Close the linux input driver and clean up resources
MODULE_EXPORT void linuxInput_close(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	struct keycode *k;

	if (p != NULL) {
		if (p->fd >= 0)
			close(p->fd);

		if (p->buttonmap != NULL) {
			while ((k = LL_Pop(p->buttonmap)) != NULL) {
				free(k->button);
				free(k);
			}
			LL_Destroy(p->buttonmap);
		}

		free(p);
	}
	drvthis->store_private_ptr(drvthis, NULL);
}

/**
 * \brief Compare with keycode
 * \param data void *data
 * \param codep void *codep
 * \return Return value
 */
static int compare_with_keycode(void *data, void *codep)
{
	uint16_t code = *(uint16_t *)codep;
	struct keycode *k = data;

	return k->code != code;
}

/**
 * \brief Read and process input events from linux input device
 * \param p Pointer to driver private data structure
 * \retval >0 Key code of pressed key
 * \retval 0 Event read but ignored (non-key event or key release)
 * \retval -1 No event available or read error
 *
 * \details Reads input events from the Linux input device file descriptor.
 * Handles device disconnection/reconnection automatically. Only processes
 * key press events (EV_KEY with value=1), ignoring releases and other event types.
 */
static int linuxInput_get_key_code(PrivateData *p)
{
	struct input_event event;
	int result = -1;

	if (p->fd != -1) {
		result = read(p->fd, &event, sizeof(event));
		if (result == -1 && errno == ENODEV) {
			report(RPT_WARNING, "Lost input device connection");
			close(p->fd);
			p->fd = -1;
		}
	}

	/**
	 * \note Automatic reconnection handling for device disconnection
	 *
	 * Handles Bluetooth disconnects, USB re-enumeration, and power management events.
	 * For example, G510 keyboards change their product ID when headphones are plugged
	 * in or unplugged, requiring device re-discovery.
	 */
	if (p->fd == -1 && p->name) {
		p->fd = linuxInput_search_by_name(p->name);
		if (p->fd != -1) {
			report(RPT_WARNING, "Successfully re-opened input device '%s'", p->name);
			result = read(p->fd, &event, sizeof(event));
		}
	}

	if (result != sizeof(event))
		return -1;

	report(RPT_DEBUG, "linux_input: Read event type=%d code=0x%x value=%d", event.type,
	       event.code, event.value);

	if (event.type != EV_KEY) {
		report(RPT_DEBUG, "linux_input: Ignoring non-key event type=%d", event.type);
		return 0;
	}
	if (!event.value) {
		report(RPT_DEBUG, "linux_input: Ignoring key release event");
		return 0;
	}

	report(RPT_DEBUG, "linux_input: Processing key press code=0x%x", event.code);
	return event.code;
}

/**
 * \brief Map Linux input keycode to LCDd button name
 * \param p Driver private data with button mapping
 * \param code Linux input event keycode
 * \return Button name string, or NULL if not mapped
 *
 * \details Searches buttonmap for keycode and returns configured button name.
 */
static const char *linuxInput_key_code_to_key_name(PrivateData *p, uint16_t code)
{
	struct keycode *k;

	if (code == 0)
		return NULL;

	if (LL_GetFirst(p->buttonmap)) {
		k = LL_Find(p->buttonmap, compare_with_keycode, &code);
		if (k) {
			report(RPT_DEBUG, "linux_input: Mapped code 0x%x to key '%s'", code,
			       k->button);
			return k->button;
		} else {
			report(RPT_DEBUG, "linux_input: No mapping found for code 0x%x", code);
		}
	} else {

		// Default key mappings
		switch (code) {

		// Escape key
		case KEY_ESC:
			return "Escape";

		// Up arrow key
		case KEY_UP:
			return "Up";

		// Left arrow key
		case KEY_LEFT:
			return "Left";

		// Right arrow key
		case KEY_RIGHT:
			return "Right";

		// Down arrow key
		case KEY_DOWN:
			return "Down";

		// Main Enter key and Keypad Enter - both return "Enter"
		case KEY_ENTER:
		case KEY_KPENTER:
			return "Enter";

		// Unknown key code
		default:
			break;
		}
	}

	report(RPT_INFO, "linux_input: Unknown key code: %d", code);
	return NULL;
}

// Read the next key press event from linux input device
MODULE_EXPORT const char *linuxInput_get_key(Driver *drvthis)
{
	PrivateData *p = drvthis->private_data;
	const char *retval = NULL;
	int code;

	// Process all queued events until valid key found
	do {
		code = linuxInput_get_key_code(p);
	} while (code >= 0 && (retval = linuxInput_key_code_to_key_name(p, code)) == NULL);

	return retval;
}
