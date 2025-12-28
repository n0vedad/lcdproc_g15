// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/main.c
 * \brief Main server entry point and initialization
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author Rene Wagner
 * \author Mike Patnode
 * \author Guillaume Filion
 * \author Peter Marschall
 * \date 1999-2006
 *
 * \features
 * - Program initialization and main event loop
 * - Command-line argument processing
 * - Configuration file parsing
 * - Signal handling for clean shutdown and reload
 * - Daemon mode with parent-child communication
 * - Privilege dropping for security
 * - Driver initialization and management
 * - Network socket initialization
 * - Screen rendering and client polling
 * - Server screen rotation and timing control
 *
 * \usage
 * - Entry point for LCDd server daemon
 * - Processes command-line options and config file
 * - Initializes all server subsystems in correct order
 * - Runs main loop for rendering and input processing
 * - Handles reload signals (SIGHUP) for config changes
 * - Implements clean shutdown on termination signals
 *
 * \details Contains main(), plus signal callback functions and a help screen.
 * Program init, command-line handling, and the main loop are implemented here.
 * Also contains minimal data about the program such as the revision number.
 */

/** \brief Enable POSIX.1-2008 functions */
#define _POSIX_C_SOURCE 200809L
/** \brief Enable BSD and SVID functions */
#define _DEFAULT_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <popt.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/**
 * \todo Fill in what to include for systems without sys/time.h
 *
 * Incomplete include statements for platforms where sys/time.h is not available.
 * Platform-specific missing header files need to be determined for systems like
 * Windows, legacy Unix systems, or embedded platforms that don't provide sys/time.h.
 *
 * Need to add conditional includes based on configure-detected availability.
 *
 * Impact: Portability, compilability on non-POSIX systems
 *
 * \ingroup ToDo_critical
 */

#include "shared/defines.h"
#include "shared/environment.h"
#include "shared/report.h"

#include "clients.h"
#include "drivers.h"
#include "input.h"
#include "main.h"
#include "menuscreens.h"
#include "parse.h"
#include "render.h"
#include "screen.h"
#include "screenlist.h"
#include "serverscreens.h"
#include "shared/configfile.h"
#include "sock.h"

#if !defined(SYSCONFDIR)
#define SYSCONFDIR "/etc"
#endif

/** \brief Default server bind address */
#define DEFAULT_BIND_ADDR "127.0.0.1"
/** \brief Default server bind port */
#define DEFAULT_BIND_PORT LCDPORT
/** \brief Default configuration file path */
#define DEFAULT_CONFIGFILE SYSCONFDIR "/LCDd.conf"
/** \brief Default user for privilege dropping */
#define DEFAULT_USER "nobody"
/** \brief Default LCD driver */
#define DEFAULT_DRIVER "curses"
/** \brief Default driver search path */
#define DEFAULT_DRIVER_PATH ""
/** \brief Maximum number of drivers that can be loaded */
#define MAX_DRIVERS 8
/** \brief Default foreground mode (0 = daemon, 1 = foreground) */
#define DEFAULT_FOREGROUND_MODE 0
/** \brief Default server screen rotation setting */
#define DEFAULT_ROTATE_SERVER_SCREEN SERVERSCREEN_ON
/** \brief Default report destination */
#define DEFAULT_REPORTDEST RPT_DEST_STDERR
/** \brief Default report level */
#define DEFAULT_REPORTLEVEL RPT_WARNING

/** \brief Default frame refresh interval in microseconds (125ms) */
#define DEFAULT_FRAME_INTERVAL 125000
/** \brief Default screen duration in frame intervals */
#define DEFAULT_SCREEN_DURATION 32
/** \brief Default backlight setting */
#define DEFAULT_BACKLIGHT BACKLIGHT_OPEN
/** \brief Default heartbeat setting */
#define DEFAULT_HEARTBEAT HEARTBEAT_OPEN
/** \brief Default title scroll speed */
#define DEFAULT_TITLESPEED TITLESPEED_MAX
/** \brief Default auto-rotation setting */
#define DEFAULT_AUTOROTATE AUTOROTATE_ON

/** \name Version Information
 * Version strings for server, protocol, and API
 */
///@{
char *version = VERSION;		   ///< LCDd server version string
char *protocol_version = PROTOCOL_VERSION; ///< Client-server protocol version
char *api_version = API_VERSION;	   ///< Driver API version
///@}

/** \name Network Configuration
 * Server network binding settings
 */
///@{
unsigned int bind_port = UNSET_INT; ///< TCP port for client connections
char bind_addr[64];		    ///< Bind address (IP or hostname)
///@}

/** \name File Paths and User
 * Configuration file path and privilege dropping user
 */
///@{
char configfile[256]; ///< Path to LCDd.conf configuration file
char user[64];	      ///< User name for privilege dropping
///@}

int frame_interval = DEFAULT_FRAME_INTERVAL; ///< Frame refresh interval in microseconds

/** \name Driver Management
 * Driver loading and initialization state
 */
///@{
char *drivernames[MAX_DRIVERS]; ///< Array of driver names to load
int num_drivers = 0;		///< Number of drivers configured
///@}

/** \name Runtime State Variables
 * Internal state for daemon operation and signal handling
 */
///@{
static int foreground_mode = UNSET_INT;	     ///< Run in foreground (1) or daemon (0)
static int report_dest = UNSET_INT;	     ///< Logging destination (stderr/syslog)
static int report_level = UNSET_INT;	     ///< Logging verbosity level
static int stored_argc;			     ///< Stored argc for config reload
static char **stored_argv;		     ///< Stored argv for config reload
static volatile short got_reload_signal = 0; ///< SIGHUP reload signal received flag
///@}

long timer = 0; ///< Main loop timer counter (incremented each frame)

// Internal function declarations for initialization, signal handling, daemon mode, and runtime
// control
static void clear_settings(void);
static int process_command_line(int argc, char **argv);
static int process_configfile(char *cfgfile);
static void set_default_settings(void);
static void install_signal_handlers(int allow_reload);
static void child_ok_func(int signal);
static pid_t daemonize(void);
static int wave_to_parent(pid_t parent_pid);
static int init_drivers(void);
static int drop_privs(char *user);
static void do_reload(void);
static void do_mainloop(void);
static void exit_program(int val);
static void catch_reload_signal(int val);
static int interpret_boolean_arg(char *s);
static void output_help_screen(void);
static void output_GPL_notice(void);

/**
 * \brief Chain function calls with error checking
 * \details Only execute function if no previous error occurred (e >= 0).
 * Allows clean error propagation through initialization chains.
 */
#define CHAIN(e, f)                                                                                \
	{                                                                                          \
		if ((e) >= 0) {                                                                    \
			(e) = (f);                                                                 \
		}                                                                                  \
	}

/**
 * \brief Terminate initialization chain on error
 * \details Check if error occurred (e < 0), report critical error and exit.
 * Used as final step in CHAIN sequences to abort on failure.
 */
#define CHAIN_END(e, msg)                                                                          \
	{                                                                                          \
		if ((e) < 0) {                                                                     \
			report(RPT_CRIT, (msg));                                                   \
			exit(EXIT_FAILURE);                                                        \
		}                                                                                  \
	}

/**
 * \brief LCDd server main entry point
 * \param argc Argument count
 * \param argv Argument vector
 * \retval 0 Normal exit
 * \retval !=0 Error exit
 *
 * \details Initializes server, processes config, optionally daemonizes,
 * initializes drivers and enters main event loop.
 */
int main(int argc, char **argv)
{
	int e = 0;
	pid_t parent_pid = 0;

	stored_argc = argc;
	stored_argv = argv;

	// Settings priority: 1) Command line  2) Config file  3) Defaults
	// This order requires reading command line first, then config, then applying defaults

	env_cache_init();

	report(RPT_NOTICE, "LCDd version %s starting", version);
	report(RPT_INFO, "Protocol version %s, API version %s", protocol_version, api_version);

	clear_settings();

	CHAIN(e, process_command_line(argc, argv));

	if (strcmp(configfile, UNSET_STR) == 0)
		strncpy(configfile, DEFAULT_CONFIGFILE, sizeof(configfile));
	CHAIN(e, process_configfile(configfile));

	set_default_settings();

	set_reporting("LCDd", report_level, report_dest);
	report(RPT_INFO, "Set report level to %d, output to %s", report_level,
	       ((report_dest == RPT_DEST_SYSLOG) ? "syslog" : "stderr"));

	// Show GPL notice early (before CHAIN_END) if in foreground mode with INFO+ level
	if (foreground_mode && report_level >= RPT_INFO) {
		output_GPL_notice();
	}

	CHAIN_END(e, "Critical error while processing settings, abort.");

	// Daemon mode requires forking before driver init to preserve LPT port access
	if (!foreground_mode) {
		report(RPT_INFO, "Server forking to background");
		CHAIN(e, parent_pid = daemonize());
	} else {
		report(RPT_INFO, "Server running in foreground");
	}

	install_signal_handlers(!foreground_mode);

	CHAIN(e, sock_init(bind_addr, bind_port));
	CHAIN(e, screenlist_init());
	CHAIN(e, init_drivers());
	CHAIN(e, clients_init());
	CHAIN(e, input_init());
	CHAIN(e, menuscreens_init());
	CHAIN(e, server_screen_init());
	CHAIN_END(e, "Critical error while initializing, abort.");

	if (!foreground_mode) {
		wave_to_parent(parent_pid);
	}

	drop_privs(user);
	do_mainloop();

	return 0;
}

/**
 * \brief Reset all configuration variables to UNSET state
 *
 * \details Sets all server configuration variables (ports, paths, drivers, etc.)
 * to UNSET_INT or UNSET_STR for reinitialization.
 */
static void clear_settings(void)
{
	int i;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	bind_port = UNSET_INT;
	strncpy(bind_addr, UNSET_STR, sizeof(bind_addr));
	strncpy(configfile, UNSET_STR, sizeof(configfile));
	strncpy(user, UNSET_STR, sizeof(user));
	foreground_mode = UNSET_INT;
	rotate_server_screen = UNSET_INT;
	backlight = UNSET_INT;
	heartbeat = UNSET_INT;
	titlespeed = UNSET_INT;

	default_duration = UNSET_INT;
	report_dest = UNSET_INT;
	report_level = UNSET_INT;

	for (i = 0; i < num_drivers; i++) {
		free(drivernames[i]);
		drivernames[i] = NULL;
	}
	num_drivers = 0;
}

/**
 * \brief Parse command-line arguments
 * \param argc Argument count
 * \param argv Argument vector
 * \retval 0 Success
 * \retval -1 Error or help requested
 *
 * \details Parses options like -c config, -d driver, -f foreground, etc.
 */
static int process_command_line(int argc, char **argv)
{
	int e = 0;
	int help = 0;
	char *config_arg = NULL;
	char *driver_arg = NULL;
	char *addr_arg = NULL;
	int port_arg = 0;
	char *user_arg = NULL;
	double wait_arg = 0.0;
	char *syslog_arg = NULL;
	int level_arg = -1;
	char *rotate_arg = NULL;

	debug(RPT_DEBUG, "%s(argc=%d, argv=...)", __FUNCTION__, argc);

	// Define popt option table for thread-safe argument parsing
	struct poptOption optionsTable[] = {
	    {"help", 'h', POPT_ARG_NONE, &help, 0, "Display this help screen", NULL},
	    {"config", 'c', POPT_ARG_STRING, (void *)&config_arg, 0,
	     "Use a configuration file other than " DEFAULT_CONFIGFILE, "FILE"},
	    {"driver", 'd', POPT_ARG_STRING, (void *)&driver_arg, 0,
	     "Add a driver to use (overrides drivers in config file)", "DRIVER"},
	    {"foreground", 'f', POPT_ARG_NONE, &foreground_mode, 0, "Run in the foreground", NULL},
	    {"addr", 'a', POPT_ARG_STRING, (void *)&addr_arg, 0,
	     "Network (IP) address to bind to [" DEFAULT_BIND_ADDR "]", "ADDRESS"},
	    {"port", 'p', POPT_ARG_INT, &port_arg, 0, "Network port to listen for connections on",
	     "PORT"},
	    {"user", 'u', POPT_ARG_STRING, (void *)&user_arg, 0, "User to run as", "USER"},
	    {"waittime", 'w', POPT_ARG_DOUBLE, &wait_arg, 0,
	     "Time to pause at each screen (in seconds)", "SECONDS"},
	    {"syslog", 's', POPT_ARG_STRING, (void *)&syslog_arg, 0,
	     "If set, reporting will be done using syslog", "BOOL"},
	    {"reportlevel", 'r', POPT_ARG_INT, &level_arg, 0, "Report level (0-5)", "LEVEL"},
	    {"rotate", 'i', POPT_ARG_STRING, (void *)&rotate_arg, 0,
	     "Whether to rotate the server info screen", "BOOL"},
	    POPT_AUTOHELP POPT_TABLEEND};

	poptContext optcon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);

	// Parse all options
	int rc;
	while ((rc = poptGetNextOpt(optcon)) > 0) {
		// All options are handled by popt automatically via arg pointers
	}

	// Check for parsing errors
	if (rc < -1) {
		report(RPT_ERR, "%s: %s", poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
		       poptStrerror(rc));
		e = -1;
		goto cleanup;
	}

	// Check for leftover arguments
	const char **leftover = poptGetArgs(optcon);
	if (leftover != NULL && leftover[0] != NULL) {
		report(RPT_ERR, "Non-option arguments on the command line !");
		e = -1;
		goto cleanup;
	}

	// Process help flag
	if (help) {
		output_help_screen();
		e = -1;
		goto cleanup;
	}

	// Process config file argument
	if (config_arg != NULL) {
		strncpy(configfile, config_arg, sizeof(configfile));
		configfile[sizeof(configfile) - 1] = '\0';
	}

	// Process driver argument (can be specified multiple times via leftover handling)
	if (driver_arg != NULL) {
		if (num_drivers < MAX_DRIVERS) {
			drivernames[num_drivers] = strdup(driver_arg);
			if (drivernames[num_drivers] != NULL) {
				num_drivers++;
			} else {
				report(RPT_ERR, "alloc error storing driver name: %s", driver_arg);
				e = -1;
				goto cleanup;
			}
		} else {
			report(RPT_ERR, "Too many drivers!");
			e = -1;
			goto cleanup;
		}
	}

	// Process bind address argument
	if (addr_arg != NULL) {
		strncpy(bind_addr, addr_arg, sizeof(bind_addr));
		bind_addr[sizeof(bind_addr) - 1] = '\0';
	}

	// Process port argument
	if (port_arg > 0) {
		bind_port = port_arg;
	}

	// Process user argument
	if (user_arg != NULL) {
		strncpy(user, user_arg, sizeof(user));
		user[sizeof(user) - 1] = '\0';
	}

	// Process wait time argument
	if (wait_arg > 0.0) {
		default_duration = (int)(wait_arg * 1e6 / frame_interval);
		if (default_duration * frame_interval < 2e6) {
			report(RPT_ERR, "Waittime should be at least 2 (seconds), not %f",
			       wait_arg);
			e = -1;
			goto cleanup;
		}
	}

	// Process syslog argument
	if (syslog_arg != NULL) {
		int b = interpret_boolean_arg(syslog_arg);
		if (b == -1) {
			report(RPT_ERR, "Not a boolean value: '%s'", syslog_arg);
			e = -1;
			goto cleanup;
		}
		report_dest = (b) ? RPT_DEST_SYSLOG : RPT_DEST_STDERR;
	}

	// Process report level argument
	if (level_arg >= 0) {
		report_level = level_arg;
	}

	// Process server screen rotation argument
	if (rotate_arg != NULL) {
		int b = interpret_boolean_arg(rotate_arg);
		if (b == -1) {
			report(RPT_ERR, "Not a boolean value: '%s'", rotate_arg);
			e = -1;
			goto cleanup;
		}
		rotate_server_screen = b;
	}

cleanup:
	poptFreeContext(optcon);
	return e;
}

/**
 * \brief Read and parse configuration file
 * \param configfile Path to LCDd.conf configuration file
 * \retval 0 Configuration file processed successfully
 * \retval -1 Error reading or parsing configuration file
 *
 * \details Reads configuration file and applies settings that were not set on
 * command line. Configures network binding, user privileges, driver settings,
 * and display parameters.
 */
static int process_configfile(char *configfile)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	if (config_read_file(configfile) != 0) {
		report(RPT_CRIT, "Could not read config file: %s", configfile);
		return -1;
	}

	if (bind_port == UNSET_INT)
		bind_port = config_get_int("Server", "Port", 0, UNSET_INT);

	if (strcmp(bind_addr, UNSET_STR) == 0)
		strncpy(bind_addr, config_get_string("Server", "Bind", 0, UNSET_STR),
			sizeof(bind_addr));

	if (strcmp(user, UNSET_STR) == 0)
		strncpy(user, config_get_string("Server", "User", 0, UNSET_STR), sizeof(user));

	if (default_duration == UNSET_INT) {
		default_duration =
		    (config_get_float("Server", "WaitTime", 0, 0) * 1e6 / frame_interval);
		if (default_duration == 0)
			default_duration = UNSET_INT;
		else if (default_duration * frame_interval < 2e6) {
			report(RPT_WARNING,
			       "Waittime should be at least 2 (seconds). Set to 2 seconds.");
			default_duration = 2e6 / frame_interval;
		}
	}

	if (foreground_mode == UNSET_INT) {
		int fg = config_get_bool("Server", "Foreground", 0, UNSET_INT);

		if (fg != UNSET_INT)
			foreground_mode = fg;
	}

	if (rotate_server_screen == UNSET_INT) {
		rotate_server_screen =
		    config_get_tristate("Server", "ServerScreen", 0, "blank", UNSET_INT);
	}

	if (backlight == UNSET_INT) {
		backlight = config_get_tristate("Server", "Backlight", 0, "open", UNSET_INT);
	}

	if (heartbeat == UNSET_INT) {
		heartbeat = config_get_tristate("Server", "Heartbeat", 0, "open", UNSET_INT);
	}

	if (autorotate == UNSET_INT) {
		autorotate = config_get_bool("Server", "AutoRotate", 0, DEFAULT_AUTOROTATE);
	}

	if (titlespeed == UNSET_INT) {
		int speed = config_get_int("Server", "TitleSpeed", 0, DEFAULT_TITLESPEED);

		titlespeed = (speed < TITLESPEED_MIN) ? TITLESPEED_MIN : min(speed, TITLESPEED_MAX);
	}

	frame_interval = config_get_int("Server", "FrameInterval", 0, DEFAULT_FRAME_INTERVAL);

	if (report_dest == UNSET_INT) {
		int rs = config_get_bool("Server", "ReportToSyslog", 0, UNSET_INT);

		if (rs != UNSET_INT)
			report_dest = (rs) ? RPT_DEST_SYSLOG : RPT_DEST_STDERR;
	}
	if (report_level == UNSET_INT) {
		report_level = config_get_int("Server", "ReportLevel", 0, UNSET_INT);
	}

	// If drivers specified on command line, skip config file drivers
	if (num_drivers == 0) {
		while (1) {
			const char *s = config_get_string("Server", "Driver", num_drivers, NULL);
			if (s == NULL)
				break;
			if (s[0] != '\0') {
				drivernames[num_drivers] = strdup(s);
				if (drivernames[num_drivers] == NULL) {
					report(RPT_ERR, "alloc error storing driver name: %s", s);
					exit(EXIT_FAILURE);
				}
				num_drivers++;
			}
		}
	}

	return 0;
}

/**
 * \brief Apply defaults to unset configuration variables
 *
 * \details Fills in default values for server configuration variables
 * that weren't set via config file or command line.
 */
static void set_default_settings(void)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	if (bind_port == UNSET_INT)
		bind_port = DEFAULT_BIND_PORT;
	if (strcmp(bind_addr, UNSET_STR) == 0)
		strncpy(bind_addr, DEFAULT_BIND_ADDR, sizeof(bind_addr));
	if (strcmp(user, UNSET_STR) == 0)
		strncpy(user, DEFAULT_USER, sizeof(user));

	if (foreground_mode == UNSET_INT)
		foreground_mode = DEFAULT_FOREGROUND_MODE;
	if (rotate_server_screen == UNSET_INT)
		rotate_server_screen = DEFAULT_ROTATE_SERVER_SCREEN;

	if (default_duration == UNSET_INT)
		default_duration = DEFAULT_SCREEN_DURATION;
	if (backlight == UNSET_INT)
		backlight = DEFAULT_BACKLIGHT;
	if (heartbeat == UNSET_INT)
		heartbeat = DEFAULT_HEARTBEAT;
	if (titlespeed == UNSET_INT)
		titlespeed = DEFAULT_TITLESPEED;

	if (report_dest == UNSET_INT)
		report_dest = DEFAULT_REPORTDEST;
	if (report_level == UNSET_INT)
		report_level = DEFAULT_REPORTLEVEL;

	if (num_drivers == 0) {
		drivernames[0] = strdup(DEFAULT_DRIVER);
		if (drivernames[0] == NULL) {
			report(RPT_ERR, "alloc error storing driver name: %s", DEFAULT_DRIVER);
			exit(EXIT_FAILURE);
		}
		num_drivers = 1;
	}
}

/**
 * \brief Install signal handlers for server lifecycle
 * \param allow_reload Enable SIGHUP reload handler (1=yes, 0=no)
 *
 * \details Sets handlers for SIGINT/SIGTERM (exit), SIGPIPE (ignore),
 * and optionally SIGHUP (reload).
 */
static void install_signal_handlers(int allow_reload)
{
	struct sigaction sa;

	debug(RPT_DEBUG, "%s(allow_reload=%d)", __FUNCTION__, allow_reload);

	sigemptyset(&(sa.sa_mask));

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);

	sa.sa_handler = exit_program;

#ifdef HAVE_SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (allow_reload) {
		sa.sa_handler = catch_reload_signal;
	}
	sigaction(SIGHUP, &sa, NULL);
}

/**
 * \brief Signal handler for parent process on child success
 * \param signal Signal number (SIGUSR1)
 *
 * \details Parent process exits with SUCCESS when child signals readiness.
 */
static void child_ok_func(int signal)
{
	debug(RPT_INFO, "%s(signal=%d)", __FUNCTION__, signal);

	_exit(EXIT_SUCCESS);
}

/**
 * \brief Daemonize
 * \return Return value
 */
static pid_t daemonize(void)
{
	pid_t child;
	pid_t parent;
	int child_status;
	struct sigaction sa;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	parent = getpid();
	debug(RPT_INFO, "parent = %d", parent);

	sa.sa_handler = child_ok_func;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);

	switch ((child = fork())) {

	// Fork failed
	case -1:
		report(RPT_ERR, "Could not fork");
		return -1;

	// Child process continues execution
	case 0:
		break;

	// Parent process waits for child signal
	default:
		debug(RPT_INFO, "child = %d", child);
		wait(&child_status);

		if (WIFEXITED(child_status)) {
			debug(RPT_INFO, "Child has terminated!");
			exit(WEXITSTATUS(child_status));
		}
		debug(RPT_INFO, "Got OK signal from child.");
		exit(EXIT_SUCCESS);
	}

	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &sa, NULL);

	setsid();

	return parent;
}

/**
 * \brief Signal parent process that child initialized successfully
 * \param parent_pid Parent process ID
 * \retval 0 Always returns 0
 *
 * \details Sends SIGUSR1 to parent, allowing it to exit cleanly.
 */
static int wave_to_parent(pid_t parent_pid)
{
	debug(RPT_DEBUG, "%s(parent_pid=%d)", __FUNCTION__, parent_pid);

	kill(parent_pid, SIGUSR1);

	return 0;
}

/**
 * \brief Initialize drivers
 * \return Return value
 */
static int init_drivers(void)
{
	int i, res;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	for (i = 0; i < num_drivers; i++) {
		res = drivers_load_driver(drivernames[i]);
		if (res >= 0) {
			if (res == 2)
				foreground_mode = 1;
		} else {
			report(RPT_ERR, "Could not load driver %.40s", drivernames[i]);
		}
	}

	if (output_driver)
		return 0;

	report(RPT_ERR, "There is no output driver");

	return -1;
}

/**
 * \brief Drop privs
 * \param user char *user
 * \return Return value
 */
static int drop_privs(char *user)
{
	struct passwd pwd;
	struct passwd *pwent;
	char buffer[4096];

	debug(RPT_DEBUG, "%s(user=\"%.40s\")", __FUNCTION__, user);

	if (getuid() == 0 || geteuid() == 0) {
		// getpwnam_r() is thread-safe (POSIX.1-2001), uses caller-provided buffer
		int result = getpwnam_r(user, &pwd, buffer, sizeof(buffer), &pwent);
		if (result != 0 || pwent == NULL) {
			report(RPT_ERR, "User %.40s not a valid user!", user);
			return -1;
		} else {
			if (setuid(pwent->pw_uid) < 0) {
				report(RPT_ERR, "Unable to switch to user %.40s", user);
				return -1;
			}
		}
	}

	return 0;
}

/**
 * \brief Reload configuration and restart drivers
 *
 * \details Called on SIGHUP signal. Unloads drivers, rereads config,
 * and reinitializes drivers with new configuration.
 */
static void do_reload(void)
{
	int e = 0;

	drivers_unload_all();
	config_clear();
	clear_settings();

	CHAIN(e, process_command_line(stored_argc, stored_argv));

	if (strcmp(configfile, UNSET_STR) == 0)
		strncpy(configfile, DEFAULT_CONFIGFILE, sizeof(configfile));
	CHAIN(e, process_configfile(configfile));

	CHAIN(e, (set_default_settings(), 0));

	CHAIN(e, set_reporting("LCDd", report_level, report_dest));
	CHAIN(e, (report(RPT_INFO, "Set report level to %d, output to %s", report_level,
			 ((report_dest == RPT_DEST_SYSLOG) ? "syslog" : "stderr")),
		  0));

	CHAIN(e, init_drivers());
	CHAIN_END(e, "Critical error while reloading, abort.");
}

// Main loop: process clients/input at PROCESS_FREQ Hz, render at frame_interval
static void do_mainloop(void)
{
	Screen *s;
	struct timeval t;
	struct timeval last_t;
	int sleeptime;
	long int process_lag = 0;
	long int render_lag = 0;
	long int t_diff;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	gettimeofday(&t, NULL);

	// Main event loop: calculate time delta, process clients/input when due, render screens
	// when due, sleep remainder
	while (1) {
		last_t = t;
		gettimeofday(&t, NULL);
		t_diff = t.tv_sec - last_t.tv_sec;
		if (((t_diff + 1) > ((double)LONG_MAX / 1e6)) || (t_diff < 0)) {
			t_diff = 0;
			process_lag = 1;
			render_lag = frame_interval;
		} else {
			t_diff *= 1e6;
			t_diff += t.tv_usec - last_t.tv_usec;
		}
		process_lag += t_diff;
		if (process_lag > 0) {
			sock_poll_clients();
			parse_all_client_messages();
			handle_input();

			process_lag = 0 - (1e6 / PROCESS_FREQ);
		}

		render_lag += t_diff;
		if (render_lag > 0) {
			timer++;
			screenlist_process();
			s = screenlist_current();

			/**
			 * \todo Move this call to every client connection and every screen add
			 *
			 * Critical threading issue: update_server_screen() should be called per
			 * client connection and every screen add, but is currently only called once
			 * globally when rendering the server screen. This can lead to race
			 * conditions and inconsistent behavior with multiple simultaneous clients.
			 *
			 * Current behavior: Only updates when server_screen is the active screen
			 * Desired behavior: Update on each client connect/disconnect and screen
			 * add/remove
			 *
			 * Impact: Thread-safety, multi-client support, display accuracy
			 *
			 * \ingroup ToDo_critical
			 */
			if (s == server_screen) {
				update_server_screen();
			}
			render_screen(s, timer);

			if (render_lag > frame_interval * MAX_RENDER_LAG_FRAMES) {
				render_lag = frame_interval * MAX_RENDER_LAG_FRAMES;
			}
			render_lag -= frame_interval;
		}

		sleeptime = min(0 - process_lag, 0 - render_lag);
		if (sleeptime > 0) {
			usleep(sleeptime);
		}

		if (got_reload_signal) {
			got_reload_signal = 0;
			do_reload();
		}
	}

	exit_program(0);
}

/**
 * \brief Perform clean shutdown of all server subsystems and exit
 * \param val Exit signal number (0 for normal exit, >0 for signal-triggered exit)
 *
 * \details Shuts down all subsystems in correct order: goodbye screen, drivers,
 * clients, menu screens, screen list, input, and sockets. Reports shutdown reason
 * based on signal number. Never returns - calls _exit(EXIT_SUCCESS).
 */
static void exit_program(int val)
{
	char buf[64];

	debug(RPT_DEBUG, "%s(val=%d)", __FUNCTION__, val);

	/**
	 * \todo These things shouldn't be so interdependent. Shutdown order shouldn't matter.
	 *
	 * Architecture problem with circular or fragile dependencies between components.
	 * The shutdown order is critical and error-prone - changing the order can cause
	 * crashes, deadlocks, or resource leaks. Components should be independently
	 * shutdownable in any order.
	 *
	 * Current issues:
	 * - drivers depend on clients being closed first
	 * - screen list depends on menu screens
	 * - sockets must be last but this is fragile
	 *
	 * Desired: Each component handles its own cleanup independently with proper
	 * reference counting or dependency tracking.
	 *
	 * Impact: Code maintainability, refactoring resistance, shutdown robustness
	 *
	 * \ingroup ToDo_critical
	 */

	if (val > 0) {

		switch (val) {

		// SIGHUP signal (reload request)
		case 1:
			snprintf(buf, sizeof(buf), "Server shutting down on SIGHUP");
			break;

		// SIGINT signal (Ctrl-C)
		case 2:
			snprintf(buf, sizeof(buf), "Server shutting down on SIGINT");
			break;

		// SIGTERM signal (termination request)
		case 15:
			snprintf(buf, sizeof(buf), "Server shutting down on SIGTERM");
			break;

		// Other signals
		default:
			snprintf(buf, sizeof(buf), "Server shutting down on signal %d", val);
			break;
		}

		report(RPT_NOTICE, buf);
	}

	if (report_level == UNSET_INT)
		report_level = DEFAULT_REPORTLEVEL;
	if (report_dest == UNSET_INT)
		report_dest = DEFAULT_REPORTDEST;
	set_reporting("LCDd", report_level, report_dest);

	goodbye_screen();
	drivers_unload_all();

	clients_shutdown();
	menuscreens_shutdown();
	screenlist_shutdown();
	input_shutdown();
	sock_shutdown();

	report(RPT_INFO, "Exiting.");
	_exit(EXIT_SUCCESS);
}

/**
 * \brief Signal handler for SIGHUP reload request
 * \param val Signal number (unused)
 *
 * \details Sets reload_flag which triggers config reload in main loop.
 * Async-signal-safe.
 */
static void catch_reload_signal(int val)
{
	debug(RPT_DEBUG, "%s(val=%d)", __FUNCTION__, val);

	got_reload_signal = 1;
}

/**
 * \brief Interpret boolean arg
 * \param s char *s
 * \retval 0 Success
 * \retval -1 Error
 */
static int interpret_boolean_arg(char *s)
{
	if (strcasecmp(s, "0") == 0 || strcasecmp(s, "false") == 0 || strcasecmp(s, "n") == 0 ||
	    strcasecmp(s, "no") == 0 || strcasecmp(s, "off") == 0) {
		return 0;
	}

	if (strcasecmp(s, "1") == 0 || strcasecmp(s, "true") == 0 || strcasecmp(s, "y") == 0 ||
	    strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0) {
		return 1;
	}

	return -1;
}

/**
 * \brief Print GPL license notice to stderr
 *
 * \details Displays LCDd version, copyright, and GPL license terms
 * when server runs in foreground mode.
 */
static void output_GPL_notice(void)
{
	fprintf(stderr, "LCDd %s, LCDproc Protocol %s\n", VERSION, PROTOCOL_VERSION);
	fprintf(stderr, "Copyright (C) 1998-2017 William Ferrell, Selene Scriven\n"
			"                        and many other contributors\n\n");

	fprintf(stderr, "This program is free software; you can redistribute it and/or\n"
			"modify it under the terms of the GNU General Public License\n"
			"as published by the Free Software Foundation; either version 2\n"
			"of the License, or (at your option) any later version.\n\n");
}

/**
 * \brief Display command-line usage help
 *
 * \details Prints LCDd command-line options, syntax, and examples
 * to stdout and exits.
 */
static void output_help_screen(void)
{
	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	fprintf(stdout, "LCDd - LCDproc Server Daemon, %s\n\n", version);
	fprintf(
	    stdout,
	    "Copyright (c) 1998-2017 Selene Scriven, William Ferrell, and misc. contributors.\n");

	fprintf(stdout,
		"This program is released under the terms of the GNU General Public License.\n\n");

	fprintf(stdout, "Usage: LCDd [<options>]\n");
	fprintf(stdout, "  where <options> are:\n");
	fprintf(stdout, "    -h                  Display this help screen\n");
	fprintf(stdout, "    -c <config>         Use a configuration file other than %s\n",
		DEFAULT_CONFIGFILE);

	fprintf(
	    stdout,
	    "    -d <driver>         Add a driver to use (overrides drivers in config file) [%s]\n",
	    DEFAULT_DRIVER);

	fprintf(stdout, "    -f                  Run in the foreground\n");
	fprintf(stdout, "    -a <addr>           Network (IP) address to bind to [%s]\n",
		DEFAULT_BIND_ADDR);

	fprintf(stdout, "    -p <port>           Network port to listen for connections on [%i]\n",
		DEFAULT_BIND_PORT);

	fprintf(stdout, "    -u <user>           User to run as [%s]\n", DEFAULT_USER);
	fprintf(stdout, "    -w <waittime>       Time to pause at each screen (in seconds) [%d]\n",
		(int)((DEFAULT_SCREEN_DURATION * frame_interval) / 1e6));

	fprintf(stdout, "    -s <bool>           If set, reporting will be done using syslog\n");
	fprintf(stdout, "    -r <level>          Report level [%d]\n", DEFAULT_REPORTLEVEL);
	fprintf(stdout, "    -i <bool>           Whether to rotate the server info screen\n");
}
