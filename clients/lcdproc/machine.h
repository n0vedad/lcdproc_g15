// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/machine.h
 * \brief Common data types and function declarations for OS-specific system information collection
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - OS-specific system information collection interface
 * - CPU load monitoring (single and SMP systems)
 * - Memory usage statistics collection
 * - Filesystem information and disk usage
 * - Battery status monitoring
 * - Process information and memory usage
 * - Network interface statistics
 * - System uptime and load averages
 * - Cross-platform data structure definitions
 *
 * \usage
 * - Implement all declared functions in platform-specific machine_XXX.c files
 * - Call machine_init() before using any other functions
 * - Use provided data structures for consistent information exchange
 * - Reference existing machine_*.c files for implementation examples
 * - Call machine_close() during program shutdown for resource cleanup
 *
 * \details This header file provides common data types and function declarations
 * for OS-specific system information collection modules in lcdproc. It defines
 * the interface that must be implemented by platform-specific files such as
 * machine_Linux.c, machine_Darwin.c, etc. To port lcdproc to a new operating
 * system, create a machine_XXX.c file implementing all functions declared here.
 */

#ifndef _lcdproc_machine_h_
#define _lcdproc_machine_h_

#include "shared/LL.h"
#include <time.h>

/** \brief Number of load average statistics returned by getloadavg() */
#ifndef LOADAVG_NSTATS
#define LOADAVG_NSTATS 3
#endif

/** \brief Index in loadavg[] parameter to getloadavg() for 1 minute load average */
#ifndef LOADAVG_1MIN
#define LOADAVG_1MIN 0 ///< 1-minute load average index
#endif

/** \brief Index in loadavg[] parameter to getloadavg() for 5 minute load average */
#ifndef LOADAVG_5MIN
#define LOADAVG_5MIN 1 ///< 5-minute load average index
#endif

/** \brief Index in loadavg[] parameter to getloadavg() for 15 minute load average */
#ifndef LOADAVG_15MIN
#define LOADAVG_15MIN 2 ///< 15-minute load average index
#endif

/** \brief Maximal number of CPUs for which load history is kept */
#ifndef MAX_CPUS
#define MAX_CPUS 16 ///< Maximum CPU count for monitoring
#endif

/**
 * \brief Information about CPU load statistics.
 *
 * \details Contains CPU time statistics measured in USER_HZ since the last call.
 * Used for calculating CPU usage percentages and load monitoring.
 */
typedef struct {
	unsigned long total;  // Total time (in USER_HZ; since last call)
	unsigned long user;   // Time in user mode (in USER_HZ; since last call)
	unsigned long system; // Time in kernel mode (in USER_HZ; since last call)
	unsigned long nice;   // Time in 'niced' user mode (in USER_HZ; since last call)
	unsigned long idle;   // Time idling (in USER_HZ; since last call)
} load_type;

/**
 * \brief Information about mounted filesystems.
 *
 * \details Contains filesystem statistics including device information,
 * mount points, and space usage. Used for disk usage monitoring.
 */
typedef struct {
	char dev[256];	  // Device name
	char type[64];	  // Filesystem type (as string)
	char mpoint[256]; // Mount point name
	long bsize;	  // Transfer block size
	long blocks;	  // Total data blocks in filesystem
	long bfree;	  // Free blocks in filesystem
	long files;	  // Total file nodes in filesystem
	long ffree;	  // Free file nodes in filesystem
} mounts_type;

/**
 * \brief Information about system memory status.
 *
 * \details Contains memory usage statistics in kilobytes.
 * Used for memory monitoring and display.
 */
typedef struct {
	long total;   // Total memory (in kB)
	long cache;   // Memory in page cache (in kB)
	long buffers; // Memory in buffer cache (in kB)
	long free;    // Free memory (in kB)
	long shared;  // Shared memory (in kB)
} meminfo_type;

/**
 * \brief Information about processes and their memory usage.
 *
 * \details Contains process information for memory usage monitoring.
 * Multiple instances of the same process are aggregated.
 */
typedef struct {
	char name[16]; // Process name
	long totl;     // Process memory usage (in kB)
	int number;    // Number of instances of the process
} procinfo_type;

/**
 * \brief Status definitions for network interfaces.
 *
 * \details Enumeration defining the operational status of network interfaces.
 */
typedef enum {
	down = 0, // Interface is down/inactive
	up = 1,	  // Interface is up/active
} IfaceStatus;

/**
 * \brief Network interface information and statistics.
 *
 * \details Contains comprehensive information about a network interface
 * including names, status, and traffic statistics. Used for network
 * monitoring and throughput calculations.
 */
typedef struct iface_info {
	char *name;  // Physical interface name
	char *alias; // Displayed name of interface

	IfaceStatus status; // Status of the interface

	time_t last_online; // Time when interface was last online

	double rc_byte;	    // Currently received bytes
	double rc_byte_old; // Previously received bytes

	double tr_byte;	    // Currently sent bytes
	double tr_byte_old; // Previously sent bytes

	double rc_pkt;	   // Currently received packets
	double rc_pkt_old; // Previously received packets

	double tr_pkt;	   // Currently sent packets
	double tr_pkt_old; // Previously sent packets
} IfaceInfo;

/**
 * \brief Initialize machine-specific subsystems.
 * \retval FALSE Initialization failed
 * \retval TRUE Initialization successful
 *
 * \details Sets up OS-specific functions and opens necessary system
 * files or resources for monitoring. Must be called before any other
 * machine_* functions.
 */
int machine_init(void);

/**
 * \brief Clean up machine-specific resources.
 * \retval FALSE Cleanup failed
 * \retval TRUE Cleanup successful
 *
 * \details Tears down OS-specific functions and closes any open
 * system files or resources. Should be called during program shutdown.
 */
int machine_close(void);

/**
 * \brief Get battery status information.
 * \param acstat Pointer to store AC adapter status
 * \param battflag Pointer to store battery status flags
 * \param percent Pointer to store battery charge percentage
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointers contain valid data
 *
 * \details Retrieves battery and AC adapter information from the system.
 * If no battery is present, should return appropriate status indicating
 * AC power operation.
 */
int machine_get_battstat(int *acstat, int *battflag, int *percent);

/**
 * \brief Get information about mounted filesystems.
 * \param fs Array to store filesystem information
 * \param cnt Pointer to store the number of filesystems found
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointers contain valid data
 *
 * \details Retrieves information about all mounted filesystems including
 * device names, mount points, and usage statistics. Filters out virtual
 * filesystems as appropriate.
 */
int machine_get_fs(mounts_type fs[], int *cnt);

/**
 * \brief Get total CPU load statistics.
 * \param cur_load Pointer to store current load information
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointer contains valid data
 *
 * \details Retrieves CPU load statistics aggregated across all CPUs.
 * Returns the difference since the last call for calculating usage
 * percentages.
 */
int machine_get_load(load_type *cur_load);

/**
 * \brief Get 1-minute load average.
 * \param load Pointer to store the current 1-minute load average
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointer contains valid data
 *
 * \details Retrieves the system's 1-minute load average, which represents
 * the average number of processes that are either running or waiting for
 * resources over the last minute.
 */
int machine_get_loadavg(double *load);

/**
 * \brief Get system memory information.
 * \param result Pointer to store memory statistics
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointer contains valid data
 *
 * \details Retrieves system memory usage information including total,
 * free, cached, and buffered memory. Values are typically in kilobytes.
 */
int machine_get_meminfo(meminfo_type *result);

/**
 * \brief Get list of processes and their memory usage.
 * \param procs Pointer to linked list for storing process information
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointer contains valid data
 *
 * \details Retrieves information about running processes including their
 * names and memory usage. Multiple instances of the same process are
 * typically aggregated.
 */
int machine_get_procs(LinkedList *procs);

/**
 * \brief Get per-CPU load statistics for SMP systems.
 * \param result Array to store per-CPU load information
 * \param numcpus Pointer to store the number of CPUs found
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointers contain valid data
 *
 * \details Retrieves CPU load statistics for each individual CPU in
 * multi-processor systems. Useful for displaying per-CPU usage.
 */
int machine_get_smpload(load_type *result, int *numcpus);

/**
 * \brief Get system uptime and idle percentage.
 * \param up Pointer to store uptime in seconds
 * \param idle Pointer to store CPU idle percentage since boot
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointers contain valid data
 *
 * \details Retrieves system uptime and the percentage of time the CPUs
 * have been idle since system boot.
 */
int machine_get_uptime(double *up, double *idle);

/**
 * \brief Get network interface statistics.
 * \param interface Pointer to interface info structure to populate
 * \retval FALSE Error, parameter contents are invalid
 * \retval TRUE Success, parameter pointer contains valid data
 *
 * \details Reads network interface statistics for a single interface
 * and stores the results in the IfaceInfo structure. This includes
 * byte and packet counts for both transmitted and received traffic.
 */
int machine_get_iface_stats(IfaceInfo *interface);

#endif
