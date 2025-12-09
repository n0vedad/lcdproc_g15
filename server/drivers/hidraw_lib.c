// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/hidraw_lib.c
 * \brief Implementation of HID raw device utility library
 * \author Hans de Goede
 * \author n0vedad
 * \date 2017-2025
 *
 *
 * \features
 * - Implementation of HID raw device utility library for Linux hidraw subsystem
 * - Automatic device discovery by scanning /dev/hidraw* devices
 * - Device identification by USB vendor/product ID matching
 * - Optional descriptor matching for multi-interface devices
 * - Output report transmission for display data and device control
 * - Feature report transmission for advanced device configuration
 * - Connection recovery and automatic device re-opening on disconnection
 * - Handle-based device management with opaque structures
 * - Cross-device compatibility with unified API
 * - Error handling and connection loss detection
 * - Directory scanning and character device filtering
 * - USB product ID retrieval for device capability detection
 *
 * \usage
 * - Used by LCDd drivers requiring HID raw device communication
 * - Primary backend for Logitech G15/G510 keyboard LCD drivers
 * - Supports any HID device accessible via Linux hidraw interface
 * - Device identification through lib_hidraw_id structure arrays
 * - Handle creation via lib_hidraw_open() with device ID matching
 * - Data transmission via lib_hidraw_send_output_report()
 * - Feature control via lib_hidraw_send_feature_report()
 * - Resource cleanup via lib_hidraw_close()
 *
 * \details Implementation of utility functions for using hidraw devices
 * in LCDd drivers providing device discovery, connection management,
 * and communication functions.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "hidraw_lib.h"
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
 * Used to identify /dev/hidraw* as character devices when scanning directory.
 * Value 2 is the standard POSIX value for character device type.
 */
#ifndef DT_CHR
#define DT_CHR 2
#endif

/**
 * \brief HID raw device handle structure
 * \details Represents an open HID device connection with device identification
 */
struct lib_hidraw_handle {
	const struct lib_hidraw_id *ids; ///< Device ID specification
	int fd;				 ///< File descriptor for open device
};

/**
 * \brief Open and verify a specific HID raw device
 * \param device Path to HID raw device (e.g., "/dev/hidraw0")
 * \param ids Array of device IDs to match against (null-terminated)
 * \retval >=0 File descriptor of opened device on success
 * \retval -1 Device not found, not supported, or failed to open
 *
 * \details Opens the specified HID raw device and verifies it matches one of the
 * supported device IDs. Checks both USB vendor/product ID and optionally the HID
 * descriptor header for multi-interface devices.
 */
static int lib_hidraw_open_device(const char *device, const struct lib_hidraw_id *ids)
{
	struct hidraw_report_descriptor descriptor;
	struct hidraw_devinfo devinfo;
	int i, err, fd;

	fd = open(device, O_RDWR);
	if (fd == -1) {
		return -1;
	}

	err = ioctl(fd, HIDIOCGRAWINFO, &devinfo);
	if (err == -1) {
		close(fd);
		return -1;
	}

	// Read HID descriptor header for interface matching
	descriptor.size = LIB_HIDRAW_DESC_HDR_SZ;
	err = ioctl(fd, HIDIOCGRDESC, &descriptor);
	if (err == -1) {
		close(fd);
		return -1;
	}

	// Search through device ID list for a match
	for (i = 0; ids[i].devinfo.bustype; i++) {
		if (memcmp(&devinfo, &ids[i].devinfo, sizeof(devinfo)) != 0) {
			continue;
		}

		/**
		 * \note Optional descriptor header check for multi-interface devices
		 *
		 * If descriptor_header[0] is 0, skip descriptor check (match any interface).
		 * Otherwise, verify descriptor matches exactly (all 16 bytes must match).
		 */
		if (ids[i].descriptor_header[0] == 0 ||
		    (descriptor.size >= LIB_HIDRAW_DESC_HDR_SZ &&
		     memcmp(descriptor.value, ids[i].descriptor_header, LIB_HIDRAW_DESC_HDR_SZ) ==
			 0)) {
			break;
		}
	}

	// Verify we found a match (bustype is non-zero for valid entries)
	if (!ids[i].devinfo.bustype) {
		close(fd);
		return -1;
	}

	return fd;
}

/**
 * \brief Lib hidraw find device
 * \param ids const struct lib_hidraw_id *ids
 * \return Return value
 */
static int lib_hidraw_find_device(const struct lib_hidraw_id *ids)
{
	char devname[PATH_MAX];
	struct dirent *dirent;
	int fd = -1;
	DIR *dir;

	dir = opendir("/dev");
	if (dir == NULL) {
		return -1;
	}

	// readdir() is thread-safe on Linux (glibc uses per-DIR lock for different directory
	// streams)
	while ((dirent = safe_readdir(dir)) != NULL) {
		if (dirent->d_type != DT_CHR || strncmp(dirent->d_name, "hidraw", 6) != 0) {
			continue;
		}

		snprintf(devname, sizeof(devname), "/dev/%s", dirent->d_name);

		fd = lib_hidraw_open_device(devname, ids);
		if (fd != -1) {
			break;
		}
	}

	closedir(dir);

	return fd;
}

// Open a HID raw device matching the provided IDs
struct lib_hidraw_handle *lib_hidraw_open(const struct lib_hidraw_id *ids)
{
	struct lib_hidraw_handle *handle;
	int fd;

	fd = lib_hidraw_find_device(ids);
	if (fd == -1) {
		return NULL;
	}

	handle = calloc(1, sizeof(*handle));
	if (!handle) {
		close(fd);
		return NULL;
	}

	handle->fd = fd;
	handle->ids = ids;
	return handle;
}

// Send an output report to the HID device
void lib_hidraw_send_output_report(struct lib_hidraw_handle *handle, unsigned char *data, int count)
{
	int result;

	if (handle->fd != -1) {
		result = write(handle->fd, data, count);

		if (result == -1 && errno == ENODEV) {
			report(RPT_WARNING, "Lost hidraw device connection");
			close(handle->fd);
			handle->fd = -1;
		}
	}

	/**
	 * \note Automatic reconnection handling for device disconnection
	 *
	 * This handles Bluetooth disconnects and USB re-enumeration scenarios.
	 * For example, G510 keyboards change their product ID when headphones
	 * are plugged in or unplugged, requiring device re-discovery.
	 */
	if (handle->fd == -1) {
		handle->fd = lib_hidraw_find_device(handle->ids);
		if (handle->fd != -1) {
			report(RPT_WARNING, "Successfully re-opened hidraw device");
			write(handle->fd, data, count);
		}
	}
}

// Send a feature report to the HID device
int lib_hidraw_send_feature_report(struct lib_hidraw_handle *handle, unsigned char *data, int count)
{
	int result = -1;

	if (handle->fd != -1) {
		result = ioctl(handle->fd, HIDIOCSFEATURE(count), data);

		if (result == -1 && errno == ENODEV) {
			report(RPT_WARNING, "Lost hidraw device connection");
			close(handle->fd);
			handle->fd = -1;
		}
	}

	/**
	 * \note Automatic reconnection handling for device disconnection
	 *
	 * This handles Bluetooth disconnects and USB re-enumeration scenarios.
	 * For example, G510 keyboards change their product ID when headphones
	 * are plugged in or unplugged, requiring device re-discovery.
	 */
	if (handle->fd == -1) {
		handle->fd = lib_hidraw_find_device(handle->ids);
		if (handle->fd != -1) {
			report(RPT_WARNING, "Successfully re-opened hidraw device");
			result = ioctl(handle->fd, HIDIOCSFEATURE(count), data);
		}
	}

	return result;
}

// Close a HID raw device handle
void lib_hidraw_close(struct lib_hidraw_handle *handle)
{
	if (handle->fd != -1) {
		close(handle->fd);
	}

	free(handle);
}

// Get the USB product ID of the device
unsigned short lib_hidraw_get_product_id(struct lib_hidraw_handle *handle)
{
	struct hidraw_devinfo devinfo;
	int err;

	if (!handle || handle->fd == -1) {
		return 0;
	}

	err = ioctl(handle->fd, HIDIOCGRAWINFO, &devinfo);
	if (err == -1) {
		return 0;
	}

	return devinfo.product;
}
