// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/main.c
 * \brief Main program entry point and core functionality for lcdproc client
 * \author William Ferrell, Selene Scriven, n0vedad
 * \date 1999-2025
 *
 * \features
 * - Command-line argument processing
 * - Configuration file parsing and validation
 * - LCDd server connection and protocol handling
 * - Screen mode management and scheduling
 * - G-Key macro system integration
 * - Signal handling and cleanup
 * - Daemon mode support with PID file management
 * - Dynamic screen enable/disable functionality
 * - Protocol version compatibility checking
 *
 * \details This file implements the main program entry point, command-line
 * argument processing, configuration file handling, and the main execution
 * loop for the lcdproc client. It coordinates all screen modules and manages
 * communication with the LCDd server.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#include <unistd.h>

#include "shared/configfile.h"
#include "shared/environment.h"
#include "shared/report.h"
#include "shared/sockets.h"

#include <popt.h>

#include "batt.h"
#include "chrono.h"
#include "cpu.h"
#include "cpu_smp.h"
#include "disk.h"
#include "iface.h"
#include "load.h"
#include "machine.h"
#include "main.h"
#include "mem.h"
#include "mode.h"

#ifdef LCDPROC_EYEBOXONE
#include "eyebox.h"
#endif

// Global client state: quit flag, server socket, version string, LCD dimensions, protocol version,
// system info, and internal function declarations
int Quit = 0;
int sock = -1;
char *version = VERSION;

int lcd_wid = 0;
int lcd_hgt = 0;
int lcd_cellwid = 0;
int lcd_cellhgt = 0;

static int protocol_major_version = 0; ///< LCDd server protocol major version
static int protocol_minor_version = 0; ///< LCDd server protocol minor version
static struct utsname unamebuf;	       ///< System information from uname()

static void HelpScreen(int exit_state);
static void exit_program(int val);
static void main_loop(void);
static int process_configfile(char *cfgfile);

/** \brief Time unit for screen updates in microseconds (1/8 second = 125ms) */
#define TIME_UNIT 125000

/** \brief System configuration directory (defaults to /etc if not set by build) */
#if !defined(SYSCONFDIR)
#define SYSCONFDIR "/etc"
#endif

/** \brief PID file directory (defaults to /var/run if not set by build) */
#if !defined(PIDFILEDIR)
#define PIDFILEDIR "/var/run"
#endif

/** \brief Sentinel value for unset integer configuration options */
#define UNSET_INT (-1)

/** \brief Sentinel value for unset string configuration options */
#define UNSET_STR "\01"

/** \brief Default LCDd server address */
#define DEFAULT_SERVER "127.0.0.1"

/** \brief Default configuration file path */
#define DEFAULT_CONFIGFILE SYSCONFDIR "/lcdproc.conf"

/** \brief Default PID file path */
#define DEFAULT_PIDFILE PIDFILEDIR "/lcdproc.pid"

/** \brief Default report destination (stderr) */
#define DEFAULT_REPORTDEST RPT_DEST_STDERR

/** \brief Default report level (warnings and above) */
#define DEFAULT_REPORTLEVEL RPT_WARNING

/** \brief Available screen modes and their configurations
 *
 * \details Array of screen mode definitions. Screens with ACTIVE flag run by default.
 */
ScreenMode sequence[] = {
    /**
     * \note Fields: name (display name), which (CLI switch char), on (on-interval in time units),
     * off (off-interval in time units), inv (invert flag), timer (update counter),
     * flags (ACTIVE=enabled by default), function (screen render callback)
     */
    {"CPU", 'C', 1, 2, 0, 0xffff, ACTIVE, cpu_screen},
    {"Iface", 'I', 1, 2, 0, 0xffff, 0, iface_screen},
    {"Memory", 'M', 4, 16, 0, 0xffff, ACTIVE, mem_screen},
    {"Load", 'L', 64, 128, 1, 0xffff, ACTIVE, xload_screen},
    {"TimeDate", 'T', 4, 64, 0, 0xffff, ACTIVE, time_screen},
    {"About", 'A', 999, 9999, 0, 0xffff, ACTIVE, credit_screen},
    {"SMP-CPU", 'P', 1, 2, 0, 0xffff, 0, cpu_smp_screen},
    {"OldTime", 'O', 4, 64, 0, 0xffff, 0, clock_screen},
    {"BigClock", 'K', 4, 64, 0, 0xffff, 0, big_clock_screen},
    {"Uptime", 'U', 4, 128, 0, 0xffff, 0, uptime_screen},
    {"Battery", 'B', 32, 256, 1, 0xffff, 0, battery_screen},
    {"CPUGraph", 'G', 1, 2, 0, 0xffff, 0, cpu_graph_screen},
    {"ProcSize", 'S', 16, 256, 1, 0xffff, 0, mem_top_screen},
    {"Disk", 'D', 256, 256, 1, 0xffff, 0, disk_screen},
    {"MiniClock", 'N', 4, 64, 0, 0xffff, 0, mini_clock_screen},
    {NULL, 0, 0, 0, 0, 0, 0, NULL},
};

/** \name Runtime Configuration Variables
 * Global configuration settings loaded from config file and command line
 */
///@{
static int islow = -1;		     ///< Low priority mode flag (-1=unset)
char *progname = "lcdproc";	     ///< Program name for display/logging
char *server = NULL;		     ///< LCDd server address
int port = UNSET_INT;		     ///< LCDd server port number
int foreground = FALSE;		     ///< Run in foreground (don't daemonize)
static int report_level = UNSET_INT; ///< Logging verbosity level
static int report_dest = UNSET_INT;  ///< Logging destination (stderr/syslog)
char *configfile = NULL;	     ///< Configuration file path
char *pidfile = NULL;		     ///< PID file path
int pidfile_written = FALSE;	     ///< Whether PID file has been created
char *displayname = NULL;	     ///< Custom display name override
char *hostname = "";		     ///< Hostname of this machine
///@}

// Get the hostname of this machine
const char *get_hostname(void) { return hostname; }

// Get the operating system name
const char *get_sysname(void) { return (unamebuf.sysname); }

// Get the operating system release version
const char *get_sysrelease(void) { return (unamebuf.release); }

/**
 * \brief Enable or disable screen mode by name or shortcut
 * \param shortname Single-character screen shortcut (e.g., 'C' for CPU)
 * \param longname Long screen name (e.g., "CPU")
 * \param state 1 to enable, 0 to disable
 * \retval 1 Screen mode found and modified
 * \retval 0 Screen mode not found
 *
 * \details Searches mode registry and updates ACTIVE flag. Disabled screens
 * are deleted from server with screen_del command.
 */
static int set_mode(int shortname, const char *longname, int state)
{
	int k;

	// Search screen mode registry by case-insensitive long name or uppercase short name match
	for (k = 0; sequence[k].which != 0; k++) {
		if (((sequence[k].longname != NULL) &&
		     (0 == strcasecmp(longname, sequence[k].longname))) ||
		    (toupper(shortname) == sequence[k].which)) {

			// Clear both active and initialized flags since we delete the
			// screen
			if (!state) {
				sequence[k].flags &= (~ACTIVE & ~INITIALIZED);
				if (sock >= 0) {
					sock_printf(sock, "screen_del %c\n", sequence[k].which);
				}
			} else
				sequence[k].flags |= ACTIVE;

			return 1;
		}
	}
	return 0;
}

/**
 * \brief Disable all screen modes
 *
 * \details Clears ACTIVE flag for all registered screen modes.
 * Screens are not deleted from server, just marked inactive.
 */
static void clear_modes(void)
{
	int k;

	for (k = 0; sequence[k].which != 0; k++) {
		sequence[k].flags &= (~ACTIVE);
	}
}

/**
 * \brief Initialize lcdproc client and enter main loop
 * \param argc Argument count
 * \param argv Argument vector
 * \retval EXIT_SUCCESS Normal exit
 * \retval EXIT_FAILURE Initialization error
 *
 * \details Processes configuration, connects to LCDd server, registers screens,
 * and enters main event loop to update active screen displays.
 */
int main(int argc, char **argv)
{
	int cfgresult;

	// Initialize environment variable cache (must be first for thread safety)
	env_cache_init();

	// Set locale for date & time formatting in chrono.c
	setlocale(LC_TIME, "");

	if (uname(&unamebuf) == -1) {
		perror("uname");
		return (EXIT_FAILURE);
	}

	signal(SIGINT, exit_program);
	signal(SIGTERM, exit_program);
	signal(SIGHUP, exit_program);
	signal(SIGPIPE, exit_program);
	signal(SIGKILL, exit_program);

	// Command-line argument parsing using popt for thread-safe option handling
	int help = 0;
	int show_version = 0;
	char *config_arg = NULL;
	char *server_arg = NULL;
	int port_arg = 0;
	int delay_arg = -1;

	struct poptOption optionsTable[] = {
	    {"help", 'h', POPT_ARG_NONE, &help, 0, "Display help information", NULL},
	    {"version", 'v', POPT_ARG_NONE, &show_version, 0, "Display version information", NULL},
	    {"config", 'c', POPT_ARG_STRING, (void *)&config_arg, 0, "Specify configuration file",
	     "FILE"},
	    {"server", 's', POPT_ARG_STRING, (void *)&server_arg, 0,
	     "Set LCDd server hostname or IP address", "HOST"},
	    {"port", 'p', POPT_ARG_INT, &port_arg, 0, "Set LCDd server port number", "PORT"},
	    {"delay", 'e', POPT_ARG_INT, &delay_arg, 0, "Set update delay between screen refreshes",
	     "SECONDS"},
	    {"foreground", 'f', POPT_ARG_NONE, &foreground, 0, "Run in foreground", NULL},
	    POPT_AUTOHELP POPT_TABLEEND};

	poptContext optcon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);

	int rc;
	while ((rc = poptGetNextOpt(optcon)) > 0) {
		// All options are handled by popt automatically via arg pointers
	}

	if (rc < -1) {
		fprintf(stderr, "%s: %s\n", poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		poptFreeContext(optcon);
		exit(EXIT_FAILURE);
	}

	if (help) {
		poptFreeContext(optcon);
		HelpScreen(EXIT_SUCCESS);
	}

	if (show_version) {
		fprintf(stderr, "LCDproc %s\n", version);
		poptFreeContext(optcon);
		exit(EXIT_SUCCESS);
	}

	if (config_arg != NULL) {
		configfile = config_arg;
	}

	if (server_arg != NULL) {
		server = server_arg;
	}

	if (port_arg > 0) {
		if (port_arg < 0xFFFF) {
			port = port_arg;
		} else {
			fprintf(stderr, "Illegal port value %d\n", port_arg);
			poptFreeContext(optcon);
			exit(EXIT_FAILURE);
		}
	}

	if (delay_arg >= 0) {
		islow = delay_arg;
	}

	// Get remaining arguments for mode selection
	const char **leftover_args = poptGetArgs(optcon);
	int leftover_count = 0;
	if (leftover_args != NULL) {
		while (leftover_args[leftover_count] != NULL) {
			leftover_count++;
		}
	}

	// Parse configuration file and optionally prepare hostname for display: load settings, exit
	// on error, allocate and format hostname with leading space if ShowHostname is enabled
	cfgresult = process_configfile(configfile);
	if (cfgresult < 0) {
		fprintf(stderr, "Error reading config file\n");
		exit(EXIT_FAILURE);
	}
	if (config_get_bool(progname, "ShowHostname", 0, TRUE)) {
		hostname = malloc(strlen(unamebuf.nodename) + 2);
		if (hostname != NULL) {
			hostname[0] = ' ';
			strncpy(hostname + 1, unamebuf.nodename, strlen(unamebuf.nodename));
			hostname[strlen(unamebuf.nodename) + 1] = '\0';
		}
	}

	if (report_dest == UNSET_INT)
		report_dest = DEFAULT_REPORTDEST;
	if (report_level == UNSET_INT)
		report_level = DEFAULT_REPORTLEVEL;

	set_reporting("lcdproc", report_level, report_dest);

	if (leftover_count > 0) {
		int i;

		// If no config file was read, clear hardcoded default modes
		if (cfgresult == 0)
			clear_modes();

		for (i = 0; i < leftover_count; i++) {
			// Determine state: '!' prefix means disable, otherwise enable
			int state = (*leftover_args[i] == '!') ? 0 : 1;
			const char *name = (state) ? leftover_args[i] : leftover_args[i] + 1;
			int shortname = (strlen(name) == 1) ? name[0] : '\0';
			int found = set_mode(shortname, name, state);

			if (!found) {
				fprintf(stderr, "Invalid Screen: %s\n", name);
				poptFreeContext(optcon);
				return (EXIT_FAILURE);
			}
		}
	}

	poptFreeContext(optcon);

	if (server == NULL)
		server = DEFAULT_SERVER;

	// Daemonize BEFORE connecting to server to avoid closing the socket
	if (foreground != TRUE) {
		if (daemon(1, 0) != 0) {
			fprintf(stderr, "Error: daemonize failed\n");
			return (EXIT_FAILURE);
		}

		if (pidfile != NULL) {
			FILE *pidf = fopen(pidfile, "w");

			if (pidf) {
				fprintf(pidf, "%d\n", (int)getpid());
				fclose(pidf);
				pidfile_written = TRUE;
			} else {
				// strerror_l() is thread-safe (POSIX.1-2008) and uses C locale for
				// consistent messages
				const char *err_msg = strerror_l(errno, LC_GLOBAL_LOCALE);
				fprintf(stderr, "Error creating pidfile %s: %s\n", pidfile,
					err_msg);
				return (EXIT_FAILURE);
			}
		}
	}

	sock = sock_connect(server, port);
	if (sock < 0) {
		fprintf(stderr,
			"Error connecting to LCD server %s on port %d.\n"
			"Check to see that the server is running and operating normally.\n",
			server, port);
		return (EXIT_FAILURE);
	}

	report(RPT_INFO, "Sending 'hello' to server");
	sock_send_string(sock, "hello\n");
	report(RPT_DEBUG, "Sleeping 500ms to allow server initialization");
	usleep(500000);

	// Set temporary LCD dimensions (real values come from "connect" response)
	lcd_wid = 20;
	lcd_hgt = 4;
	lcd_cellwid = 5;
	lcd_cellhgt = 8;

	report(RPT_INFO, "Initializing mode subsystems");
	mode_init();

	if (gkey_macro_init() != 0) {
		report(RPT_WARNING, "Failed to initialize G-Key macro system");
	}

	// Reserve all G-Keys and macro keys for the macro system
	const char *gkeys[] = {"G1",  "G2",  "G3",  "G4",  "G5",  "G6",	 "G7",	"G8",
			       "G9",  "G10", "G11", "G12", "G13", "G14", "G15", "G16",
			       "G17", "G18", "M1",  "M2",  "M3",  "MR",	 NULL};
	for (int i = 0; gkeys[i] != NULL; i++) {
		sock_printf(sock, "client_add_key %s\n", gkeys[i]);
		report(RPT_DEBUG, "Reserved G-Key: %s", gkeys[i]);
	}

	report(RPT_INFO, "Client initialization complete - starting main_loop");
	main_loop();
	exit_program(EXIT_SUCCESS);

	return EXIT_SUCCESS;
}

/**
 * \brief Process configuration file
 * \param configfile Path to configuration file (NULL to use default)
 * \retval 0 Success
 * \retval <0 Error reading or parsing configuration
 *
 * \details Reads and parses the lcdproc configuration file, applying all settings
 * to global configuration variables.
 */
static int process_configfile(char *configfile)
{
	int k;
	const char *tmp;

	debug(RPT_DEBUG, "%s(%s)", __FUNCTION__, (configfile) ? configfile : "<null>");

	if (configfile == NULL) {
		struct stat statbuf;

		if ((lstat(DEFAULT_CONFIGFILE, &statbuf) == -1) && (errno = ENOENT))
			return 0;

		configfile = DEFAULT_CONFIGFILE;
	}

	if (config_read_file(configfile) != 0) {
		report(RPT_CRIT, "Could not read config file: %s", configfile);
		return -1;
	}

	// Configure server connection parameters (command line takes precedence)
	if (server == NULL) {
		server = strdup(config_get_string(progname, "Server", 0, DEFAULT_SERVER));
	}

	if (port == UNSET_INT) {
		port = config_get_int(progname, "Port", 0, LCDPORT);
	}

	if (report_level == UNSET_INT) {
		report_level = config_get_int(progname, "ReportLevel", 0, RPT_WARNING);
	}

	if (report_dest == UNSET_INT) {
		if (config_get_bool(progname, "ReportToSyslog", 0, 0)) {
			report_dest = RPT_DEST_SYSLOG;
		} else {
			report_dest = RPT_DEST_STDERR;
		}
	}

	if (foreground != TRUE) {
		foreground = config_get_bool(progname, "Foreground", 0, FALSE);
	}

	if (pidfile == NULL) {
		pidfile = strdup(config_get_string(progname, "PidFile", 0, DEFAULT_PIDFILE));
	}

	if (islow < 0) {
		islow = config_get_int(progname, "Delay", 0, -1);
	}

	if ((tmp = config_get_string(progname, "DisplayName", 0, NULL)) != NULL) {
		displayname = strdup(tmp);
	}

	// Apply configuration file settings to all screen modes
	for (k = 0; sequence[k].which != 0; k++) {
		if (sequence[k].longname != NULL) {
			sequence[k].on_time =
			    config_get_int(sequence[k].longname, "OnTime", 0, sequence[k].on_time);
			sequence[k].off_time = config_get_int(sequence[k].longname, "OffTime", 0,
							      sequence[k].off_time);
			sequence[k].show_invisible = config_get_bool(
			    sequence[k].longname, "ShowInvisible", 0, sequence[k].show_invisible);
			if (config_get_bool(sequence[k].longname, "Active", 0,
					    sequence[k].flags & ACTIVE))
				sequence[k].flags |= ACTIVE;
			else
				sequence[k].flags &= (~ACTIVE);
		}
	}

	return 1;
}

/**
 * \brief Display help screen and exit program
 * \param exit_state Exit code (EXIT_SUCCESS or EXIT_FAILURE)
 *
 * \details Prints usage information including command-line options and
 * available screen modes, then exits with specified status code.
 */
void HelpScreen(int exit_state)
{
	fprintf(stderr,
		"lcdproc - LCDproc system status information viewer\n"
		"\n"
		"Copyright (c) 1999-2017 Selene Scriven, William Ferrell, and misc. contributors.\n"
		"This program is released under the terms of the GNU General Public License.\n"
		"\n"
		"Usage: lcdproc [<options>] [<screens> ...]\n"
		"  where <options> are\n"
		"    -s <host>           connect to LCDd daemon on <host>\n"
		"    -p <port>           connect to LCDd daemon using <port>\n"
		"    -f                  run in foreground\n"
		"    -e <delay>          slow down initial announcement of screens (in 1/100s)\n"
		"    -c <config>         use a configuration file other than %s\n"
		"    -h                  show this help screen\n"
		"    -v                  display program version\n"
		"  and <screens> are\n"
		"    C CPU               detailed CPU usage\n"
		"    P SMP-CPU           CPU usage overview (one line per CPU)\n"
		"    G CPUGraph          CPU usage histogram\n"
		"    L Load              load histogram\n"
		"    M Memory            memory & swap usage\n"
		"    S ProcSize          biggest processes size\n"
		"    D Disk              filling level of mounted file systems\n"
		"    I Iface             network interface usage\n"
		"    B Battery           battery status\n"
		"    T TimeDate          time & date information\n"
		"    O OldTime           old time screen\n"
		"    U Uptime            uptime screen\n"
		"    K BigClock          big clock\n"
		"    N MiniClock         minimal clock\n"
		"    A About             credits page\n"
		"\n"
		"Example:\n"
		"    lcdproc -s my.lcdproc.server.com -p 13666 C M L\n",
		DEFAULT_CONFIGFILE);

	exit(exit_state);
}

/**
 * \brief Signal handler to request program termination
 * \param val Signal number (unused)
 *
 * \details Sets global Quit flag to trigger graceful shutdown in main loop.
 * Async-signal-safe - actual cleanup happens in main loop, not in handler.
 */
void exit_program(int val)
{
	// Only async-safe operations allowed in signal handlers
	// Set flag to trigger graceful shutdown in main loop
	(void)val; // Suppress unused parameter warning
	Quit = 1;
}

#ifdef LCDPROC_MENUS
// Initialize menu system with screen mode checkboxes
int menus_init()
{
	int k;

	for (k = 0; sequence[k].which; k++) {
		if (sequence[k].longname) {
			sock_printf(sock, "menu_add_item {} %c checkbox {%s} -value %s\n",
				    sequence[k].which, sequence[k].longname,
				    (sequence[k].flags & ACTIVE) ? "on" : "off");
		}
	}

#ifdef LCDPROC_CLIENT_TESTMENUS
	// Test menu structure for debugging menu functionality
	sock_send_string(sock, "menu_add_item {} ask menu {Leave menus?} -is_hidden true\n");
	sock_send_string(sock, "menu_add_item {ask} ask_yes action {Yes} -next _quit_\n");
	sock_send_string(sock, "menu_add_item {ask} ask_no action {No} -next _close_\n");
	sock_send_string(sock, "menu_add_item {} test menu {Test}\n");
	sock_send_string(sock, "menu_add_item {test} test_action action {Action}\n");
	sock_send_string(sock, "menu_add_item {test} test_checkbox checkbox {Checkbox}\n");
	sock_send_string(sock,
			 "menu_add_item {test} test_ring ring {Ring} -strings {one\ttwo\tthree}\n");
	sock_send_string(
	    sock,
	    "menu_add_item {test} test_slider slider {Slider} -mintext < -maxtext > -value 50\n");
	sock_send_string(sock, "menu_add_item {test} test_numeric numeric {Numeric} -value 42\n");
	sock_send_string(sock, "menu_add_item {test} test_alpha alpha {Alpha} -value abc\n");
	sock_send_string(sock,
			 "menu_add_item {test} test_ip ip {IP} -v6 false -value 192.168.1.1\n");
	sock_send_string(sock, "menu_add_item {test} test_menu menu {Menu}\n");
	sock_send_string(sock,
			 "menu_add_item {test_menu} test_menu_action action {Submenu's action}\n");
	sock_send_string(sock, "menu_set_item {} test -prev {ask}\n");

	sock_send_string(sock, "menu_set_item {} test_action -next {test_checkbox}\n");
	sock_send_string(sock,
			 "menu_set_item {} test_checkbox -next {test_ring} -prev test_action\n");
	sock_send_string(sock,
			 "menu_set_item {} test_ring -next {test_slider} -prev {test_checkbox}\n");
	sock_send_string(sock,
			 "menu_set_item {} test_slider -next {test_numeric} -prev {test_ring}\n");
	sock_send_string(sock,
			 "menu_set_item {} test_numeric -next {test_alpha} -prev {test_slider}\n");
	sock_send_string(sock,
			 "menu_set_item {} test_alpha -next {test_ip} -prev {test_numeric}\n");
	sock_send_string(sock, "menu_set_item {} test_ip -next {test_menu} -prev {test_alpha}\n");
	sock_send_string(sock, "menu_set_item {} test_menu_action -next {_close_}\n");
#endif

	return 0;
}
#endif

/**
 * \brief Main program execution loop
 *
 * \details Handles server connection, command parsing, screen updates,
 * and user input processing. Runs until termination signal received.
 */
void main_loop(void)
{
	int i = 0, j;
	int connected = 0;
	char buf[8192];
	char *argv[256];
	int argc, newtoken;
	int len;
	static int loop_count = 0;

	report(RPT_INFO, "Entering main_loop - starting message processing");

	while (!Quit) {
		len = sock_recv(sock, buf, 8000);

		if (len > 0 && loop_count < 5) {
			loop_count++;
			report(RPT_DEBUG, "main_loop: Received %d bytes (iteration #%d)", len,
			       loop_count);
		}

		// Tokenize received string into command arguments
		while (len > 0) {
			argc = 0;
			newtoken = 1;

			for (i = 0; i < len; i++) {
				switch (buf[i]) {

				// Handle space character - marks end of current token
				case ' ':
					newtoken = 1;
					buf[i] = 0;
					break;

				// Handle regular characters - continue building current token
				default:
					if (newtoken)
						argv[argc++] = buf + i;
					newtoken = 0;
					break;

				// Handle line termination - process complete command
				case '\0':
				case '\n':
					buf[i] = 0;
					if (argc > 0) {

						// Handle "listen" command - server tells us screen
						// is now visible
						if ((0 == strcmp(argv[0], "listen")) &&
						    (argc >= 2)) {
							for (j = 0; sequence[j].which; j++) {
								if (sequence[j].which ==
								    argv[1][0]) {
									sequence[j].flags |=
									    VISIBLE;
									report(
									    RPT_INFO,
									    "Received LISTEN for "
									    "screen '%s' - setting "
									    "VISIBLE flag",
									    argv[1]);
								}
							}

							// Handle "ignore" command - server tells us
							// screen is no longer visible
						} else if ((0 == strcmp(argv[0], "ignore")) &&
							   (argc >= 2)) {
							for (j = 0; sequence[j].which; j++) {
								if (sequence[j].which ==
								    argv[1][0]) {
									sequence[j].flags &=
									    ~VISIBLE;
									report(
									    RPT_INFO,
									    "Received IGNORE for "
									    "screen '%s' - "
									    "clearing VISIBLE flag",
									    argv[1]);
								}
							}

							// Handle "key" command - key press events
							// from server
						} else if ((0 == strcmp(argv[0], "key")) &&
							   (argc >= 2)) {
							report(RPT_INFO, "KEY EVENT RECEIVED: %s",
							       argv[1]);
							gkey_macro_handle_key(argv[1]);
						}
#ifdef LCDPROC_MENUS
						// Process menu update events to enable/disable
						// screens
						else if (0 == strcmp(argv[0], "menuevent")) {
							if (argc == 4 &&
							    (0 == strcmp(argv[1], "update"))) {
								// State: on if not "off"
								set_mode(argv[2][0], "",
									 strcmp(argv[3], "off"));
							}
						}
#else
						else if (0 == strcmp(argv[0], "menu")) {
						}
#endif
						// Handle connection acknowledgment from LCDd server
						else if (0 == strcmp(argv[0], "connect")) {
							int a;
							report(RPT_NOTICE,
							       "Received CONNECT from server");
							for (a = 1; a < argc; a++) {
								if (0 == strcmp(argv[a], "wid"))
									lcd_wid = atoi(argv[++a]);
								else if (0 ==
									 strcmp(argv[a], "hgt"))
									lcd_hgt = atoi(argv[++a]);
								else if (0 ==
									 strcmp(argv[a], "cellwid"))
									lcd_cellwid =
									    atoi(argv[++a]);
								else if (0 ==
									 strcmp(argv[a], "cellhgt"))
									lcd_cellhgt =
									    atoi(argv[++a]);

								// Extract major.minor version
								// numbers
								else if (0 == strcmp(argv[a],
										     "protocol"))
									sscanf(
									    argv[++a], "%d.%d",
									    &protocol_major_version,
									    &protocol_minor_version);
							}
							connected = 1;
							report(RPT_NOTICE,
							       "Connection established - "
							       "lcd_wid=%d, lcd_hgt=%d",
							       lcd_wid, lcd_hgt);

							// Set client name - use configured name or
							// hostname
							if (displayname != NULL)
								sock_printf(
								    sock,
								    "client_set -name \"%s\"\n",
								    displayname);
							else
								sock_printf(sock,
									    "client_set -name "
									    "{LCDproc %s}\n",
									    get_hostname());
#ifdef LCDPROC_MENUS
							menus_init();
#endif
							// CRITICAL: After connection, immediately
							// drain all buffered messages from the
							// socket to ensure "listen" commands are
							// processed before the first screen update.
							// This prevents the race condition where
							// the server sends "listen T" in a separate
							// TCP packet that arrives after we've
							// already updated the screen with
							// display=0.
							report(RPT_DEBUG,
							       "Draining socket buffer for pending "
							       "messages after connection");
							goto drain_socket;
						} else if (0 == strcmp(argv[0], "bye")) {
							exit_program(EXIT_SUCCESS);
						}

						else if (0 == strcmp(argv[0], "success")) {
						} else {
							debug(RPT_DEBUG,
							      "Unknown server message: argc=%d",
							      argc);
							for (int j = 0; j < argc; j++)
								debug(RPT_DEBUG, "  arg[%d]: %s", j,
								      argv[j]);
						}
					}

					argc = 0;
					newtoken = 1;
					break;
				}
			}

		drain_socket:
			len = sock_recv(sock, buf, 8000);
		}

		// Update all active screens based on timing
		if (connected) {
			for (i = 0; sequence[i].which > 0; i++) {
				sequence[i].timer++;

				if (!(sequence[i].flags & ACTIVE))
					continue;

				if (sequence[i].flags & VISIBLE) {
					if (sequence[i].timer >= sequence[i].on_time) {
						sequence[i].timer = 0;
						debug(RPT_NOTICE,
						      "Updating VISIBLE screen '%c' with display=1",
						      sequence[i].which);
						update_screen(&sequence[i], 1);
					}
				} else {
					if (sequence[i].timer >= sequence[i].off_time) {
						sequence[i].timer = 0;
						debug(RPT_NOTICE,
						      "Updating INVISIBLE screen '%c' with "
						      "display=%d",
						      sequence[i].which,
						      sequence[i].show_invisible);
						update_screen(&sequence[i],
							      sequence[i].show_invisible);
					}
				}
				if (islow > 0)
					usleep(islow * 10000);
			}
		}

		usleep(TIME_UNIT);
	}

	// Cleanup when exiting main loop (triggered by Quit flag from signal handler)
	gkey_macro_cleanup();
	sock_close(sock);
	mode_close();
	if ((foreground != TRUE) && (pidfile != NULL) && (pidfile_written == TRUE))
		unlink(pidfile);
}
