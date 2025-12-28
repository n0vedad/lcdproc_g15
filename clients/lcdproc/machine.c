// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/machine.c
 * \brief System information collection implementation for lcdproc client
 * \author William Ferrell, Selene Scriven, n0vedad
 * \date 1999-2025
 *
 * \features
 * - CPU load monitoring (single and SMP)
 * - Memory and swap usage statistics
 * - Filesystem and mount point information
 * - Battery status monitoring via APM
 * - Process information and memory usage
 * - Network interface statistics
 * - System uptime and load average
 * - Support for both traditional and modern Linux features
 * - Uses /proc filesystem for most system information
 * - Supports both getloadavg() and /proc/loadavg for load averages
 * - Handles various filesystem types and statistics methods
 * - Provides fallback mechanisms for missing features
 *
 * \usage
 * - Called by lcdproc client for system information retrieval
 * - Provides abstraction layer for Linux system monitoring
 * - Supports both single and SMP CPU configurations
 * - Handles filesystem statistics and battery monitoring
 * - Used internally by various lcdproc display screens
 *
 * \details
 * This file implements Linux-specific system information collection
 * functions for the lcdproc client. It interfaces with the Linux /proc filesystem
 * to gather various system metrics including CPU load, memory usage, filesystem
 * statistics, process information, and network interface data.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "shared/posix_wrappers.h"
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

/** \brief Enable getloadavg() if available on this platform */
#ifdef HAVE_GETLOADAVG
#define USE_GETLOADAVG
#endif

#ifdef USE_GETLOADAVG
#ifdef HAVE_SYS_LOADAVG_H
#include <sys/loadavg.h>
#endif
#endif

#ifdef HAVE_PROCFS_H
#include <procfs.h>
#endif

#ifdef HAVE_SYS_PROCFS_H
#include <sys/procfs.h>
#endif

#include "machine.h"
#include "main.h"
#include "mode.h"
#include "shared/posix_wrappers.h"

#include "shared/LL.h"
#include "shared/posix_wrappers.h"
#include "shared/report.h"

/** \name /proc File Descriptors
 * Cached file descriptors for reading system statistics from /proc
 */
///@{
static int batt_fd;    ///< Battery status file descriptor
static int load_fd;    ///< CPU load file descriptor
static int loadavg_fd; ///< Load average file descriptor
static int meminfo_fd; ///< Memory info file descriptor
static int uptime_fd;  ///< Uptime file descriptor
///@}

/**
 * \todo Replace procbuf with dynamic allocation or per-function buffers.
 *
 * Static global buffer procbuf[1024] is shared across multiple functions for /proc file parsing.
 * This is an unclean solution that can lead to:
 * - Buffer overflows if /proc files exceed 1024 bytes
 * - Reentrancy issues if functions using procbuf are called concurrently
 * - Code coupling - multiple functions depend on the same global buffer
 *
 * Recommended fix: Replace with either dynamic allocation (malloc/free) per function call
 * or per-function local buffers sized appropriately.
 *
 * Impact: Security, thread safety, code quality
 *
 * \ingroup ToDo_medium
 */
static char procbuf[1024]; ///< Shared buffer for /proc file parsing (ugly hack!)
static FILE *mtab_fd;	   ///< Mount table file handle

// Initialize machine-specific subsystems and open proc files
int machine_init(void)
{
	uptime_fd = -1;
	batt_fd = -1;
	load_fd = -1;
	loadavg_fd = -1;
	meminfo_fd = -1;

	if (uptime_fd < 0) {
		uptime_fd = open("/proc/uptime", O_RDONLY);
		if (uptime_fd < 0) {
			perror("open /proc/uptime");
			return (FALSE);
		}
	}

	if (load_fd < 0) {
		load_fd = open("/proc/stat", O_RDONLY);
		if (load_fd < 0) {
			perror("open /proc/stat");
			return (FALSE);
		}
	}

#ifndef USE_GETLOADAVG
	if (loadavg_fd < 0) {
		loadavg_fd = open("/proc/loadavg", O_RDONLY);
		if (loadavg_fd < 0) {
			perror("open /proc/loadavg");
			return (FALSE);
		}
	}
#endif

	if (meminfo_fd < 0) {
		meminfo_fd = open("/proc/meminfo", O_RDONLY);
		if (meminfo_fd < 0) {
			perror("open /proc/meminfo");
			return (FALSE);
		}
	}

	if (batt_fd < 0) {
		batt_fd = open("/proc/apm", O_RDONLY);
		if (batt_fd < 0) {
			batt_fd = -1;
		}
	}

	return (TRUE);
}

// Clean up machine-specific resources and close open files
int machine_close(void)
{
	if (batt_fd >= 0)
		close(batt_fd);
	batt_fd = -1;

	if (load_fd >= 0)
		close(load_fd);
	load_fd = -1;

#ifndef USE_GETLOADAVG
	if (loadavg_fd >= 0)
		close(loadavg_fd);
	loadavg_fd = -1;
#endif

	if (meminfo_fd >= 0)
		close(meminfo_fd);
	meminfo_fd = -1;

	if (uptime_fd >= 0)
		close(uptime_fd);
	uptime_fd = -1;

	return (TRUE);
}

/**
 * \brief Reread data from /proc file into buffer
 * \param f File descriptor of open /proc file
 * \param errmsg Error message to display on failure
 *
 * \details Seeks to beginning and reads entire file into procbuf.
 * Exits program with perror() if seek or read fails.
 */
static void reread(int f, char *errmsg)
{
	if (lseek(f, 0L, 0) == 0 && read(f, procbuf, sizeof(procbuf) - 1) > 0)
		return;
	perror(errmsg);
	exit(1);
}

/**
 * \brief Extract tagged numeric value from /proc buffer
 * \param tag Tag name to search for (e.g., "MemTotal:")
 * \param bufptr Buffer containing /proc file contents
 * \param value Output pointer for parsed numeric value
 * \retval 1 Tag found and value extracted
 * \retval 0 Tag not found
 *
 * \details Searches for tag in buffer and parses following numeric value using strtol().
 */
static int getentry(const char *tag, const char *bufptr, long *value)
{
	char *tail;
	int len = strlen(tag);
	long val;

	// Parse /proc file line by line: find tag prefix, extract integer value with validation,
	// return success or failure
	while (bufptr != NULL) {
		if (*bufptr == '\n')
			bufptr++;
		if (!strncmp(tag, bufptr, len)) {
			errno = 0;
			val = strtol(bufptr + len, &tail, 10);
			if ((errno == 0) && (tail != bufptr + len)) {
				*value = val;
				return TRUE;
			} else
				return FALSE;
		}
		bufptr = strchr(bufptr, '\n');
	}

	return FALSE;
}

// Get battery status information from APM
int machine_get_battstat(int *acstat, int *battflag, int *percent)
{
	char str[64];
	int battstat;

	if (batt_fd < 0) {
		*acstat = LCDP_AC_ON;
		*battflag = LCDP_BATT_ABSENT;
		*percent = 100;
		return (TRUE);
	}

	// Rewind /proc/apm file to beginning for fresh read
	if (lseek(batt_fd, 0, 0) != 0)
		return (FALSE);

	// Read APM battery status from /proc/apm into buffer
	if (read(batt_fd, str, sizeof(str) - 1) < 0)
		return (FALSE);

	// Parse APM status: skip first 13 chars, read hex values and percentage
	if (3 > sscanf(str + 13, "0x%x 0x%x 0x%x %d", acstat, &battstat, battflag, percent))
		return (FALSE);

	// Translate APM battery flags to lcdproc constants
	if (*battflag == 0xff)
		*battflag = LCDP_BATT_UNKNOWN;
	else {
		if (*battflag & 1)
			*battflag = LCDP_BATT_HIGH;
		if (*battflag & 2)
			*battflag = LCDP_BATT_LOW;
		if (*battflag & 4)
			*battflag = LCDP_BATT_CRITICAL;
		if (*battflag & 8 || battstat == 3)
			*battflag = LCDP_BATT_CHARGING;
		if (*battflag & 128)
			*battflag = LCDP_BATT_ABSENT;
	}

	// Translate APM AC adapter status to lcdproc constants
	switch (*acstat) {

	// APM reports no AC power
	case 0:
		*acstat = LCDP_AC_OFF;
		break;

	// APM reports AC power connected
	case 1:
		*acstat = LCDP_AC_ON;
		break;

	// APM reports backup power source
	case 2:
		*acstat = LCDP_AC_BACKUP;
		break;

	// APM reports unknown or invalid AC status
	default:
		*acstat = LCDP_AC_UNKNOWN;
		break;
	}

	return (TRUE);
}

// Get filesystem statistics for all mounted filesystems
int machine_get_fs(mounts_type fs[], int *cnt)
{
#ifdef STAT_STATVFS
	struct statvfs fsinfo;
#else
	struct statfs fsinfo;
#endif
	char line[256];
	int x = 0;
	int err;

#ifdef MTAB_FILE
	mtab_fd = fopen(MTAB_FILE, "r");
#else
	mtab_fd = fopen("/proc/mounts", "r");
#endif

	memset(fs, 0, sizeof(mounts_type) * 256);

	if (mtab_fd == NULL) {
		return -1;
	}

	while (x < 256) {
		if (fgets(line, 256, mtab_fd) == NULL)
			break;

		sscanf(line, "%s %s %s", fs[x].dev, fs[x].mpoint, fs[x].type);

		if (strcmp(fs[x].type, "proc") != 0 && strcmp(fs[x].type, "tmpfs") != 0

#ifndef STAT_NFS
		    && strcmp(fs[x].type, "nfs") != 0
#endif

#ifndef STAT_SMBFS
		    && strcmp(fs[x].type, "smbfs") != 0
#endif
		) {

#ifdef STAT_STATVFS
			err = statvfs(fs[x].mpoint, &fsinfo);
#elif STAT_STATFS2_BSIZE
			err = statfs(fs[x].mpoint, &fsinfo);
#elif STAT_STATFS4
			err = statfs(fs[x].mpoint, &fsinfo, sizeof(fsinfo), 0);
#else
			err = statfs(fs[x].mpoint, &fsinfo);
#endif

			if (err < 0) {
				char err_buf[256];
				strerror_r(errno, err_buf, sizeof(err_buf));
				debug(RPT_INFO, "statvfs(%s): %s", fs[x].mpoint, err_buf);
				continue;
			}

			fs[x].blocks = fsinfo.f_blocks;

			if (fs[x].blocks > 0) {
				fs[x].bsize = fsinfo.f_bsize;
				fs[x].bfree = fsinfo.f_bfree;
				fs[x].files = fsinfo.f_files;
				fs[x].ffree = fsinfo.f_ffree;
				x++;
			}
		}
	}

	fclose(mtab_fd);
	*cnt = x;
	return (TRUE);
}

// Get CPU load statistics for single-processor systems
int machine_get_load(load_type *curr_load)
{
	static load_type last_load = {0, 0, 0, 0, 0};
	load_type load;
	int ret;
	unsigned long load_iowait, load_irq, load_softirq;

	reread(load_fd, "get_load");

	// Parse CPU line: "cpu user nice system idle iowait irq softirq"
	ret = sscanf(procbuf, "cpu %lu %lu %lu %lu %lu %lu %lu", &load.user, &load.nice,
		     &load.system, &load.idle, &load_iowait, &load_irq, &load_softirq);

	// Merge modern kernel extensions into existing categories
	if (ret >= 5)
		load.idle += load_iowait;
	if (ret >= 6)
		load.system += load_irq;
	if (ret >= 7)
		load.system += load_softirq;

	load.total = load.user + load.nice + load.system + load.idle;

	// Calculate deltas for percentage calculation
	curr_load->user = load.user - last_load.user;
	curr_load->nice = load.nice - last_load.nice;
	curr_load->system = load.system - last_load.system;
	curr_load->idle = load.idle - last_load.idle;
	curr_load->total = load.total - last_load.total;

	last_load = load;

	return (TRUE);
}

// Get system load average (1-minute average)
int machine_get_loadavg(double *load)
{
#ifdef USE_GETLOADAVG
	double loadavg[LOADAVG_NSTATS];

	if (getloadavg(loadavg, LOADAVG_NSTATS) <= LOADAVG_1MIN) {

		/**
		 * \todo Replace perror() with report() for consistent error handling.
		 *
		 * Current implementation uses primitive perror("getloadavg") which outputs
		 * directly to stderr. Should use LCDproc's report(RPT_ERR, ...) function
		 * for consistency with the rest of the codebase and proper log routing.
		 *
		 * Impact: Error handling consistency, logging uniformity
		 *
		 * \ingroup ToDo_medium
		 */

		perror("getloadavg");
		return (FALSE);
	}
	*load = loadavg[LOADAVG_1MIN];
#else
	reread(loadavg_fd, "get_loadavg");
	sscanf(procbuf, "%lf", load);
#endif
	return (TRUE);
}

// Get memory and swap usage statistics
int machine_get_meminfo(meminfo_type *result)
{
	long tmp;

	reread(meminfo_fd, "get_meminfo");

	// Extract RAM memory statistics (index 0 = RAM)
	result[0].total = (getentry("MemTotal:", procbuf, &tmp) == TRUE) ? tmp : 0L;
	result[0].free = (getentry("MemFree:", procbuf, &tmp) == TRUE) ? tmp : 0L;
	result[0].shared = (getentry("MemShared:", procbuf, &tmp) == TRUE) ? tmp : 0L;
	result[0].buffers = (getentry("Buffers:", procbuf, &tmp) == TRUE) ? tmp : 0L;
	result[0].cache = (getentry("Cached:", procbuf, &tmp) == TRUE) ? tmp : 0L;

	// Extract swap space statistics (index 1 = swap)
	result[1].total = (getentry("SwapTotal:", procbuf, &tmp) == TRUE) ? tmp : 0L;
	result[1].free = (getentry("SwapFree:", procbuf, &tmp) == TRUE) ? tmp : 0L;

	return (TRUE);
}

// Get process memory usage information for top memory consumers
int machine_get_procs(LinkedList *procs)
{
	DIR *proc;
	FILE *StatusFile;
	struct dirent *procdir;

	char procName[16];
	int procSize, procRSS, procData, procStk, procExe;
	const char *NameLine = "Name:", *VmSizeLine = "VmSize:", *VmRSSLine = "VmRSS",
		   *VmDataLine = "VmData", *VmStkLine = "VmStk", *VmExeLine = "VmExe";
	const int NameLineLen = strlen(NameLine), VmSizeLineLen = strlen(VmSizeLine),
		  VmDataLineLen = strlen(VmDataLine), VmStkLineLen = strlen(VmStkLine),
		  VmExeLineLen = strlen(VmExeLine), VmRSSLineLen = strlen(VmRSSLine);

	// Memory threshold: only track processes using >400KB
	int threshold = 400, unique;

	if ((proc = opendir("/proc")) == NULL) {

		/**
		 * \todo Replace perror() with report() for consistent error handling in
		 * mem_top_screen
		 *
		 * Current implementation uses primitive perror("mem_top_screen: unable to open
		 * /proc") which outputs directly to stderr. Should use LCDproc's report(RPT_ERR,
		 * ...) function for consistency with the rest of the codebase and proper log
		 * routing.
		 *
		 * Impact: Error handling consistency, logging uniformity
		 *
		 * \ingroup ToDo_medium
		 */

		perror("mem_top_screen: unable to open /proc");
		return (FALSE);
	}

	// Iterate /proc directory entries: filter numeric PIDs, parse /proc/[pid]/status for memory
	// stats, aggregate process memory usage
	// readdir() is thread-safe on Linux (glibc uses per-DIR lock for different directory
	// streams)
	while ((procdir = safe_readdir(proc))) {
		char buf[128];

		if (!strchr("1234567890", procdir->d_name[0]))
			continue;

		sprintf(buf, "/proc/%s/status", procdir->d_name);
		if ((StatusFile = fopen(buf, "r")) == NULL) {
			continue;
		}

		procRSS = procSize = procData = procStk = procExe = 0;
		while (fgets(buf, sizeof(buf), StatusFile)) {
			if (!strncmp(buf, NameLine, NameLineLen)) {
				sscanf(buf, "%*s %s", procName);
			} else if (!strncmp(buf, VmSizeLine, VmSizeLineLen)) {
				sscanf(buf, "%*s %d", &procSize);
			} else if (!strncmp(buf, VmRSSLine, VmRSSLineLen)) {
				sscanf(buf, "%*s %d", &procRSS);
			} else if (!strncmp(buf, VmDataLine, VmDataLineLen)) {
				sscanf(buf, "%*s %d", &procData);
			} else if (!strncmp(buf, VmStkLine, VmStkLineLen)) {
				sscanf(buf, "%*s %d", &procStk);
			} else if (!strncmp(buf, VmExeLine, VmExeLineLen)) {
				sscanf(buf, "%*s %d", &procExe);
			}
		}
		fclose(StatusFile);

		// Add process to list if above threshold: merge with existing entry or create new
		// one
		if (procSize > threshold) {
			unique = 1;
			LL_Rewind(procs);
			do {
				procinfo_type *p = LL_Get(procs);

				if ((p != NULL) && (0 == strcmp(p->name, procName))) {
					unique = 0;
					p->number++;
					p->totl += procData + procStk + procExe;
				}
			} while (LL_Next(procs) == 0);

			if (unique) {
				procinfo_type *p = malloc(sizeof(procinfo_type));
				if (p == NULL) {
					perror("mem_top_screen: Error allocating process entry");
					break;
				}
				strncpy(p->name, procName, sizeof(p->name) - 1);
				p->name[sizeof(p->name) - 1] = '\0';
				p->totl = procData + procStk + procExe;
				p->number = 1;

				/**
				 * \todo Check for errors when pushing processes to linked list
				 *
				 * Missing error handling when pushing processes to linked list in
				 * Linux-specific code. The LL_Push() operation may fail, but errors
				 * are currently not checked or handled.
				 *
				 * Impact: Robustness
				 *
				 * \ingroup ToDo_medium
				 */

				LL_Push(procs, (void *)p);
			}
		}
	}
	closedir(proc);

	return (TRUE);
}

// Get CPU load statistics for multi-processor (SMP) systems
int machine_get_smpload(load_type *result, int *numcpus)
{
	char *token;
	static load_type last_load[MAX_CPUS];
	load_type curr_load[MAX_CPUS];
	int ncpu = 0;
	int ret;
	unsigned long load_iowait, load_irq, load_softirq;

	reread(load_fd, "get_load");

	// Parse per-CPU lines: "cpu0", "cpu1", "cpu2", etc.
	char *saveptr;
	token = strtok_r(procbuf, "\n", &saveptr);
	while (token != NULL) {
		if ((strlen(token) > 3) && (!strncmp(token, "cpu", 3)) && isdigit(token[3])) {
			ret = sscanf(token, "cpu%*d %lu %lu %lu %lu %lu %lu %lu",
				     &curr_load[ncpu].user, &curr_load[ncpu].nice,
				     &curr_load[ncpu].system, &curr_load[ncpu].idle, &load_iowait,
				     &load_irq, &load_softirq);

			// Merge modern kernel extensions
			if (ret >= 5)
				curr_load[ncpu].idle += load_iowait;
			if (ret >= 6)
				curr_load[ncpu].system += load_irq;
			if (ret >= 7)
				curr_load[ncpu].system += load_softirq;

			curr_load[ncpu].total = curr_load[ncpu].user + curr_load[ncpu].nice +
						curr_load[ncpu].system + curr_load[ncpu].idle;

			// Calculate deltas for percentage calculation
			result[ncpu].total = curr_load[ncpu].total - last_load[ncpu].total;
			result[ncpu].user = curr_load[ncpu].user - last_load[ncpu].user;
			result[ncpu].nice = curr_load[ncpu].nice - last_load[ncpu].nice;
			result[ncpu].system = curr_load[ncpu].system - last_load[ncpu].system;
			result[ncpu].idle = curr_load[ncpu].idle - last_load[ncpu].idle;

			last_load[ncpu] = curr_load[ncpu];

			ncpu++;
			if ((ncpu >= *numcpus) || (ncpu >= MAX_CPUS))
				break;
		}
		token = strtok_r(NULL, "\n", &saveptr);
	}
	*numcpus = ncpu;

	return (TRUE);
}

// Get system uptime and idle time statistics
int machine_get_uptime(double *up, double *idle)
{
	double local_up, local_idle;

	reread(uptime_fd, "get_uptime");
	sscanf(procbuf, "%lf %lf", &local_up, &local_idle);

	if (up != NULL)
		*up = local_up;

	if (idle != NULL)
		*idle = (local_up != 0) ? 100 * local_idle / local_up : 100;

	return (TRUE);
}

// Get network interface statistics
int machine_get_iface_stats(IfaceInfo *interface)
{
	FILE *file;
	char buffer[1024];
	static int first_time = 1;
	char *ch_pointer = NULL;

	if ((file = fopen("/proc/net/dev", "r")) != NULL) {
		if (fgets(buffer, sizeof(buffer), file) == NULL) {
			fclose(file);
			return -1;
		}
		if (fgets(buffer, sizeof(buffer), file) == NULL) {
			fclose(file);
			return -1;
		}

		interface->status = down;

		// Parse /proc/net/dev for interface stats: find matching interface line, extract
		// rx/tx bytes and packets, initialize baseline on first call
		while ((fgets(buffer, sizeof(buffer), file) != NULL)) {
			if (strstr(buffer, interface->name)) {
				interface->status = up;
				interface->last_online = time(NULL);

				ch_pointer = strchr(buffer, ':');
				ch_pointer++;

				// Parse: rx_bytes rx_packets ... tx_bytes tx_packets
				sscanf(ch_pointer, "%lf %lf %*s %*s %*s %*s %*s %*s %lf %lf",
				       &interface->rc_byte, &interface->rc_pkt, &interface->tr_byte,
				       &interface->tr_pkt);

				// Initialize old values on first call to prevent spikes
				if (first_time) {
					interface->rc_byte_old = interface->rc_byte;
					interface->tr_byte_old = interface->tr_byte;
					interface->rc_pkt_old = interface->rc_pkt;
					interface->tr_pkt_old = interface->tr_pkt;
					first_time = 0;
				}
			}
		}

		fclose(file);
		return (TRUE);

	} else {
		perror("Error: Could not open DEVFILE");
		return (FALSE);
	}
}
