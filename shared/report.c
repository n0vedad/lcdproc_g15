// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/report.c
 * \brief Logging and reporting system implementation
 * \author William Ferrell, Selene Scriven, Joris Robijn, Peter Marschall
 * \date 1999-2005
 *
 * \features
 * - Multiple reporting levels (CRIT, ERR, WARNING, NOTICE, INFO, DEBUG)
 * - Multiple output destinations (stderr, syslog, memory store)
 * - Message buffering and delayed output
 * - Configurable verbosity levels
 * - Printf-style formatted output
 * - Automatic message flushing when destination changes
 *
 * \usage
 * - Include report.h in your source files
 * - Use report(level, format, ...) for logging messages
 * - Configure output destination with set_reporting_destination()
 * - Set verbosity with set_reporting_verbosity()
 *
 * \details This file provides a flexible logging and reporting system that supports
 * multiple output destinations (stderr, syslog, memory storage) and different
 * reporting levels. The system includes message buffering capabilities for delayed
 * output and configurable verbosity levels.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "report.h"

/** \brief Maximum number of messages that can be stored in memory */
#define MAX_STORED_MSGS 200

/** \name Global Report System State
 * Logging level, destination, and message storage
 */
///@{
static int report_level = RPT_INFO;	   ///< Current logging level threshold
static int report_dest = RPT_DEST_STORE;   ///< Current logging destination
static char *stored_msgs[MAX_STORED_MSGS]; ///< Array of stored message strings
static int stored_levels[MAX_STORED_MSGS]; ///< Array of levels for stored messages
static int num_stored_msgs = 0;		   ///< Count of messages currently stored
///@}

/**
 * \brief Store a report message in memory buffer
 * \param level Report level (RPT_DEBUG, RPT_INFO, etc.)
 * \param message Message string to store
 *
 * \details Stores messages in circular buffer until flush_messages() is called.
 * Used during initialization before logging destination is configured.
 */
static void store_report_message(int level, const char *message);

// Flush stored messages to configured destination
static void flush_messages();

// Report a message to the selected destination if important enough
void report(const int level, const char *format, ...)
{
	if (level <= report_level || report_dest == RPT_DEST_STORE) {
		char buf[1024];
		va_list ap;

		va_start(ap, format);

		switch (report_dest) {

		// Output to stderr with newline
		case RPT_DEST_STDERR:
			vfprintf(stderr, format, ap);
			fprintf(stderr, "\n");
			break;

		// Output to syslog with adjusted priority
		case RPT_DEST_SYSLOG:
			vsyslog(LOG_USER | (level + 2), format, ap);
			break;

		default:
			break;

		// Store message in memory buffer
		case RPT_DEST_STORE:
			vsnprintf(buf, sizeof(buf), format, ap);
			buf[sizeof(buf) - 1] = 0;
			store_report_message(level, buf);
			break;
		}

		va_end(ap);
	}
}

// Sets reporting level and message destination
int set_reporting(char *application_name, int new_level, int new_dest)
{
	if (new_level < RPT_CRIT || new_level > RPT_DEBUG) {
		report(RPT_ERR, "report level invalid: %d", new_level);
		return -1;
	}

	// Handle syslog lifecycle
	if (report_dest != RPT_DEST_SYSLOG && new_dest == RPT_DEST_SYSLOG) {
		openlog(application_name, 0, LOG_USER);
	} else if (report_dest == RPT_DEST_SYSLOG && new_dest != RPT_DEST_SYSLOG) {
		closelog();
	}

	report_level = new_level;
	report_dest = new_dest;

	if (report_dest != RPT_DEST_STORE)
		flush_messages();

	return 0;
}

// Store a message in the internal message buffer
static void store_report_message(int level, const char *message)
{
	if (num_stored_msgs < MAX_STORED_MSGS) {
		size_t msg_len = strlen(message);

		stored_msgs[num_stored_msgs] = malloc(msg_len + 1);
		if (stored_msgs[num_stored_msgs] != NULL) {
			strncpy(stored_msgs[num_stored_msgs], message, msg_len);
			stored_msgs[num_stored_msgs][msg_len] = '\0';
			stored_levels[num_stored_msgs] = level;
			num_stored_msgs++;
		}
	}

	/**
	 * \note If buffer is full (200 messages), additional messages are silently
	 * discarded without warning. This can result in loss of critical error messages
	 * during startup phase before set_reporting() switches to final destination.
	 */
}

/**
 * \brief Output all stored messages and clear buffer
 *
 * \details Sends all buffered messages to current report destination,
 * frees message memory, and resets stored message count.
 */
static void flush_messages()
{
	int i;

	for (i = 0; i < num_stored_msgs; i++) {
		report(stored_levels[i], "%s", stored_msgs[i]);
		free(stored_msgs[i]);
	}

	num_stored_msgs = 0;
}
