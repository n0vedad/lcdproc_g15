// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/report.h
 * \brief Contains reporting functions.
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2001
 *
 * \features
 * - Multiple reporting levels (CRIT, ERR, WARNING, NOTICE, INFO, DEBUG)
 * - Configurable output destinations (stderr, syslog, memory store)
 * - Global and per-file DEBUG macro support
 * - Printf-style formatted output functions
 * - Runtime verbosity configuration
 *
 * \usage
 * To enable debug() function on all software: ./configure --enable-debug
 * To enable debug() function only in specific files:
 * 1) Configure without enabling debug (without --enable-debug)
 * 2) Edit source file and add `#define DEBUG` before `#include "report.h"`
 * 3) Recompile with 'make'
 *
 * This enables local debugging while keeping global DEBUG macro off.
 *
 * \details Header file providing reporting and debugging functions with multiple
 * severity levels and output destinations. Supports both regular reporting and
 * conditional debug output that can be enabled globally or per-file.
 */

#ifndef REPORT_H
#define REPORT_H

/**
 * \name Reporting Severity Levels
 * \brief Severity levels for logging and reporting messages
 *
 * \details These constants define the severity levels for the reporting system.
 * They are compatible with syslog severity levels and should not be modified.
 * Use these levels with the report() function to control message importance.
 * @{
 */

/** Critical conditions: the program stops right after this.
 * Only use this if the program is actually exited from the current function. */
#define RPT_CRIT 0

/** Error conditions: serious problem, program continues.
 * Use this just before you return -1 from a function. */
#define RPT_ERR 1

/** Warning conditions: Something that the user should fix, but the
 * program can continue without a real problem.
 * Example: Protocol errors from a client. */
#define RPT_WARNING 2

/** Major event in the program.
 * Example: (un)loading of driver, client (dis)connect. */
#define RPT_NOTICE 3

/** Minor event in the program: the activation of a setting, details of a
 * loaded driver, a key reservation, a keypress, a screen switch. */
#define RPT_INFO 4

/** Insignificant event.
 * Example: What function has been called, what subpart of a function is being
 * executed, what was received and sent over the socket, etc. */
#define RPT_DEBUG 5

/** @} */

/** \name Reporting Destinations
 * Output destinations for log messages
 */
///@{
#define RPT_DEST_STDERR 0 ///< Send messages to standard error
#define RPT_DEST_SYSLOG 1 ///< Send messages to syslog
#define RPT_DEST_STORE 2  ///< Store messages in internal buffer
///@}

/**
 * \brief Sets reporting level and message destination
 * \param application_name Name of the application for syslog identification
 * \param new_level New reporting level threshold (RPT_CRIT to RPT_DEBUG)
 * \param new_dest New destination (RPT_DEST_STDERR, RPT_DEST_SYSLOG, RPT_DEST_STORE)
 * \retval 0 Success: reporting parameters updated
 * \retval -1 Error: invalid reporting level provided
 *
 * \details Configures the reporting system with a new verbosity level and output
 * destination. When switching to/from syslog, the appropriate open/close operations
 * are performed. Stored messages are flushed when switching away from storage mode.
 */
int set_reporting(char *application_name, int new_level, int new_dest);

/**
 * \brief Report a message to the selected destination if important enough
 * \param level Message priority level (RPT_CRIT, RPT_ERR, RPT_WARNING, RPT_NOTICE, RPT_INFO,
 * RPT_DEBUG)
 * \param format Printf-style format string
 * \param ... Arguments for the format string
 *
 * \details This is the main reporting function that formats and outputs messages
 * based on the current reporting level and destination. Messages are only output
 * if their level is at or below the current reporting threshold, except when
 * storing to memory where all messages are captured.
 */
void report(const int level, const char *format, ...);

/**
 * \brief No-operation function for disabled debug output
 * \param level Message priority level (ignored)
 * \param format Printf-style format string (ignored)
 * \param ... Arguments for the format string (ignored)
 *
 * \details The code that this function generates will not be in the executable when
 * compiled without debugging. This way memory and CPU cycles are saved.
 */
static inline void dont_report(const int level, const char *format, ... /*args*/) {}

/**
 * \brief Debug output macro that conditionally compiles to report() or dont_report()
 *
 * \details Consider the debug function to be exactly the same as the report function.
 * The only difference is that it is only compiled in if DEBUG is defined.
 * When DEBUG is not defined, debug() calls are optimized away completely.
 */
#ifdef DEBUG
#define debug report
#else
#define debug dont_report
#endif

#endif
