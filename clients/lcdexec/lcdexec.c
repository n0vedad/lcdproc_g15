// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdexec/lcdexec.c
 * \brief Main file for lcdexec, the program starter in the LCDproc suite
 * \author Joris Robijn, Peter Marschall
 * \date 2002-2008
 *
 * \features
 * - Menu-driven program execution interface
 * - Process monitoring and status feedback
 * - Configuration file support
 * - Background/foreground execution modes
 * - Integration with LCDd server menus
 * - Process lifecycle management with PID tracking
 * - Signal handling for child process cleanup
 *
 * \usage
 * - Configure programs in the configuration file
 * - Connect to LCDd server for menu display
 * - Navigate menus using LCD input devices
 * - Execute programs through menu selections
 * - Monitor process status and lifecycle
 *
 * \details This file implements lcdexec, an LCDproc client that provides
 * a menu-driven interface for executing programs and scripts. It connects
 * to the LCDd server and creates interactive menus that allow users to
 * launch predefined commands through the LCD display.
 */

/** \brief Enable POSIX.1-2008 functions (strdup, etc.) */
#define _POSIX_C_SOURCE 200809L
/** \brief Enable BSD and SVID functions */
#define _DEFAULT_SOURCE

#include <errno.h>
#include <locale.h>
#include <popt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "menu.h"
#include "shared/configfile.h"
#include "shared/environment.h"
#include "shared/report.h"
#include "shared/sockets.h"
#include "shared/str.h"

/** \brief System configuration directory - overridable at compile time */
#if !defined(SYSCONFDIR)
#define SYSCONFDIR "/etc"
#endif

/** \brief PID file directory - overridable at compile time */
#if !defined(PIDFILEDIR)
#define PIDFILEDIR "/var/run"
#endif

/** \brief Default configuration file path */
#define DEFAULT_CONFIGFILE SYSCONFDIR "/lcdexec.conf"
/** \brief Default PID file path */
#define DEFAULT_PIDFILE PIDFILEDIR "/lcdexec.pid"
/** \brief Sentinel value for uninitialized integer config options */
#define UNSET_INT (-1)
/** \brief Sentinel value for uninitialized string config options */
#define UNSET_STR "\01"

/**
 * \brief Information about a process started by lcdexec
 * \details Tracks execution state of commands launched from LCD menu.
 * Processes are stored in a linked list (proc_queue) for status monitoring.
 */
typedef struct ProcInfo {
	struct ProcInfo *next; ///< Next process in linked list
	const MenuEntry *cmd;  ///< Menu entry that started this process
	pid_t pid;	       ///< Process ID
	time_t starttime;      ///< When process started
	time_t endtime;	       ///< When process ended (0 if still running)
	int status;	       ///< Exit status from waitpid()
	int feedback;	       ///< Feedback type (on/off/to_menu)
	int shown;	       ///< Whether status was already displayed
} ProcInfo;

/** \brief Help text displayed with -h option */
char *help_text = "lcdexec - LCDproc client to execute commands from the LCDd menu\n"
		  "\n"
		  "Copyright (c) 2002, Joris Robijn, 2006-2008 Peter Marschall.\n"
		  "This program is released under the terms of the GNU General Public License.\n"
		  "\n"
		  "Usage: lcdexec [<options>]\n"
		  "  where <options> are:\n"
		  "    -c <file>           Specify configuration file [" DEFAULT_CONFIGFILE "]\n"
		  "    -a <address>        DNS name or IP address of the LCDd server [localhost]\n"
		  "    -p <port>           port of the LCDd server [13666]\n"
		  "    -f                  Run in foreground\n"
		  "    -r <level>          Set reporting level (0-5) [2: errors and warnings]\n"
		  "    -s <0|1>            Report to syslog (1) or stderr (0, default)\n"
		  "    -h                  Show this help\n";

/** \brief Program name for error messages */
char *progname = "lcdexec";

/** \brief Configuration file path */
char *configfile = NULL;
/** \brief Server address (hostname or IP) */
char *address = NULL;
/** \brief Server port number */
int port = UNSET_INT;
/** \brief Foreground mode flag (1=foreground, 0=daemon) */
int foreground = FALSE;
/** \brief Logging report level */
static int report_level = UNSET_INT;
/** \brief Logging destination (syslog or stderr) */
static int report_dest = UNSET_INT;
/** \brief PID file path */
char *pidfile = NULL;
/** \brief Flag indicating if PID file was written */
int pidfile_written = FALSE;
/** \brief Display name for menu title */
char *displayname = NULL;
/** \brief Default shell for command execution */
char *default_shell = NULL;
/** \brief Root of the menu tree */
MenuEntry *main_menu = NULL;
/** \brief Queue of running/completed processes */
ProcInfo *proc_queue = NULL;

/** \brief LCD display width in characters */
int lcd_wid = 0;
/** \brief LCD display height in characters */
int lcd_hgt = 0;
/** \brief Server socket file descriptor */
int sock = -1;
/** \brief Program termination flag */
int Quit = 0;

// Function prototypes
static void exit_program(int val);
static void sigchld_handler(int signal);
static int process_command_line(int argc, char **argv);
static int process_configfile(char *configfile);
static int connect_and_setup(void);
static int process_response(char *str);
static int exec_command(MenuEntry *cmd);
static int show_procinfo_msg(ProcInfo *p);
static int main_loop(void);
static int update_menu_string_value(char **value_ptr, const char *new_value);
static void format_env_int(char *buf, size_t buf_size, const char *name, int value);
static void format_env_string(char *buf, size_t buf_size, const char *name, const char *value);
static void add_status_widgets(ProcInfo *p, int is_multiline);
static void display_exit_status(ProcInfo *p, int status_y);

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
 * \brief Terminate program if any error occurred in function chain
 * \details Checks error code and exits if negative, used at end of CHAIN sequences.
 */
#define CHAIN_END(e)                                                                               \
	{                                                                                          \
		if ((e) < 0) {                                                                     \
			report(RPT_CRIT, "Critical error, abort");                                 \
			exit(e);                                                                   \
		}                                                                                  \
	}
/**
 * \brief Initialize lcdexec client and enter main event loop
 * \param argc Argument count
 * \param argv Argument vector
 * \retval 0 Success
 * \retval -1 Initialization or runtime error
 *
 * \details Processes command line arguments, reads configuration file,
 * connects to LCDd server, and enters the main event loop handling
 * menu events and executing commands.
 */
int main(int argc, char **argv)
{
	int error = 0;
	struct sigaction sa;

	// Initialize environment variable cache (must be first for thread safety)
	env_cache_init();

	CHAIN(error, process_command_line(argc, argv));
	if (configfile == NULL)
		configfile = DEFAULT_CONFIGFILE;
	CHAIN(error, process_configfile(configfile));

	// Set up reporting system with defaults if not configured
	if (report_dest == UNSET_INT || report_level == UNSET_INT) {
		report_dest = RPT_DEST_STDERR;
		report_level = RPT_ERR;
	}
	set_reporting(progname, report_level, report_dest);
	CHAIN_END(error);

	CHAIN(error, connect_and_setup());
	CHAIN_END(error);

	// Daemonize if not running in foreground
	if (foreground != TRUE) {
		if (daemon(1, 1) != 0) {
			report(RPT_ERR, "Error: daemonize failed");
		}

		if (pidfile != NULL) {
			FILE *pidf = fopen(pidfile, "w");

			if (pidf) {
				int ret = fprintf(pidf, "%d\n", (int)getpid());
				(void)ret;
				fclose(pidf);
				pidfile_written = TRUE;
			} else {
				// strerror_l() is thread-safe (POSIX.1-2008) and uses C locale for
				// consistent messages
				const char *err_msg = strerror_l(errno, LC_GLOBAL_LOCALE);
				int ret = fprintf(stderr, "Error creating pidfile %s: %s\n",
						  pidfile, err_msg);
				(void)ret;
				return (EXIT_FAILURE);
			}
		}
	}

	sigemptyset(&sa.sa_mask);

// Register exit_program handler for termination signals with syscall restart
#ifdef HAVE_SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
	sa.sa_handler = exit_program;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGKILL, &sa, NULL);

	// Setup signal handler for children to avoid zombies
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sa.sa_handler = sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);

	main_loop();

	exit_program(EXIT_SUCCESS);

	// NOTREACHED
	return EXIT_SUCCESS;
}

/**
 * \brief Update menu string value with reallocation
 * \param value_ptr Pointer to string pointer to update
 * \param new_value New string value to copy
 * \retval 0 Success
 * \retval -1 Memory allocation failed
 */
static int update_menu_string_value(char **value_ptr, const char *new_value)
{
	*value_ptr = realloc(*value_ptr, strlen(new_value) + 1);
	if (*value_ptr != NULL) {
		size_t len = strlen(new_value);
		memcpy(*value_ptr, new_value, len);
		(*value_ptr)[len] = '\0';
		return 0;
	}

	return -1;
}

/**
 * \brief Format integer as environment variable string
 * \param buf Output buffer
 * \param buf_size Buffer size
 * \param name Variable name
 * \param value Integer value
 */
static void format_env_int(char *buf, size_t buf_size, const char *name, int value)
{
	int ret = snprintf(buf, buf_size - 1, "%s=%d", name, value);
	(void)ret;
}

/**
 * \brief Format string as environment variable string
 * \param buf Output buffer
 * \param buf_size Buffer size
 * \param name Variable name
 * \param value String value
 */
static void format_env_string(char *buf, size_t buf_size, const char *name, const char *value)
{
	int ret = snprintf(buf, buf_size - 1, "%s=%s", name, value);
	(void)ret;
}

/**
 * \brief Clean exit with cleanup
 * \param val Exit status code
 */
static void exit_program(int val)
{
	printf("exit program now\n");
	Quit = 1;
	sock_close(sock);

	// Clean up PID file if running as daemon
	if ((foreground != TRUE) && (pidfile != NULL) && (pidfile_written == TRUE))
		unlink(pidfile);

	exit(val);
}

/**
 * \brief SIGCHLD signal handler
 * \param signal Signal number (unused)
 */
static void sigchld_handler(int signal)
{
	pid_t pid;
	int status;

	// Wait for the child that was signalled as finished
	if ((pid = wait(&status)) != -1) {
		ProcInfo *p;

		for (p = proc_queue; p != NULL; p = p->next) {
			if (p->pid == pid) {
				p->status = status;
				p->endtime = time(NULL);
			}
		}
	}
}

/**
 * \brief Process command line arguments
 * \param argc Argument count
 * \param argv Argument vector
 * \retval 0 Success
 * \retval -1 Error in arguments
 */
static int process_command_line(int argc, char **argv)
{
	int error = 0;
	int help = 0;
	char *config_arg = NULL;
	char *addr_arg = NULL;
	int port_arg = 0;
	int level_arg = -1;
	int syslog_arg = -1;

	// Define popt option table for thread-safe argument parsing
	struct poptOption optionsTable[] = {
	    {"help", 'h', POPT_ARG_NONE, &help, 0, "Show this help", NULL},
	    {"config", 'c', POPT_ARG_STRING, (void *)&config_arg, 0,
	     "Specify configuration file [" DEFAULT_CONFIGFILE "]", "FILE"},
	    {"address", 'a', POPT_ARG_STRING, (void *)&addr_arg, 0,
	     "DNS name or IP address of the LCDd server [localhost]", "ADDRESS"},
	    {"port", 'p', POPT_ARG_INT, &port_arg, 0, "Port of the LCDd server [13666]", "PORT"},
	    {"foreground", 'f', POPT_ARG_NONE, &foreground, 0, "Run in foreground", NULL},
	    {"reportlevel", 'r', POPT_ARG_INT, &level_arg, 0,
	     "Set reporting level (0-5) [2: errors and warnings]", "LEVEL"},
	    {"syslog", 's', POPT_ARG_INT, &syslog_arg, 0,
	     "Report to syslog (1) or stderr (0, default)", "0|1"},
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
		error = -1;
		goto cleanup;
	}

	// Check for leftover arguments
	const char **leftover = poptGetArgs(optcon);
	if (leftover != NULL && leftover[0] != NULL) {
		report(RPT_ERR, "Non-option arguments on the command line");
		error = -1;
		goto cleanup;
	}

	// Process help flag
	if (help) {
		int ret = fprintf(stderr, "%s", help_text);
		(void)ret;
		poptFreeContext(optcon);
		exit(EXIT_SUCCESS);
	}

	// Process config file argument
	if (config_arg != NULL) {
		configfile = strdup(config_arg);
	}

	// Process server address argument
	if (addr_arg != NULL) {
		address = strdup(addr_arg);
	}

	// Process port argument with validation
	if (port_arg > 0) {
		if (port_arg <= 0xFFFF) {
			port = port_arg;
		} else {
			report(RPT_ERR, "Illegal port value %d", port_arg);
			error = -1;
			goto cleanup;
		}
	}

	// Process report level argument
	if (level_arg >= 0) {
		report_level = level_arg;
	}

	// Process syslog argument
	if (syslog_arg >= 0) {
		report_dest = (syslog_arg ? RPT_DEST_SYSLOG : RPT_DEST_STDERR);
	}

cleanup:
	poptFreeContext(optcon);
	return error;
}
/**
 * \brief Process configfile
 * \param configfile char *configfile
 * \return Return value
 */
static int process_configfile(char *configfile)
{
	const char *tmp;

	if (configfile == NULL)
		configfile = DEFAULT_CONFIGFILE;

	if (config_read_file(configfile) < 0) {
		report(RPT_WARNING, "Could not read config file: %s", configfile);
	}

	if (address == NULL) {
		address = strdup(config_get_string(progname, "Address", 0, "localhost"));
	}

	if (port == UNSET_INT) {
		port = config_get_int(progname, "Port", 0, 13666);
	}

	if (report_level == UNSET_INT) {
		report_level = config_get_int(progname, "ReportLevel", 0, RPT_WARNING);
	}

	if (report_dest == UNSET_INT) {
		report_dest = (config_get_bool(progname, "ReportToSyslog", 0, 0)) ? RPT_DEST_SYSLOG
										  : RPT_DEST_STDERR;
	}
	if (foreground != TRUE) {
		foreground = config_get_bool(progname, "Foreground", 0, FALSE);
	}

	if (pidfile == NULL) {
		pidfile = strdup(config_get_string(progname, "PidFile", 0, DEFAULT_PIDFILE));
	}

	if ((tmp = config_get_string(progname, "DisplayName", 0, NULL)) != NULL)
		displayname = strdup(tmp);

	if ((tmp = config_get_string(progname, "Shell", 0, NULL)) != NULL)
		default_shell = strdup(tmp);

	else {
		report(RPT_WARNING,
		       "Shell not set in configuration, falling back to variable SHELL");
		// Use thread-safe cached environment variable, strdup for consistent ownership
		default_shell = strdup(env_get_shell());
	}

	main_menu = menu_read(NULL, "MainMenu");

// Output menu structure for debugging purposes
#if defined(DEBUG)
	menu_dump(main_menu);
#endif

	if (main_menu == NULL) {
		report(RPT_ERR, "no main menu found in configuration");
		return -1;
	}

	return 0;
}

/**
 * \brief Connect and setup
 * \return Return value
 */
static int connect_and_setup(void)
{
	report(RPT_INFO, "Connecting to %s:%d", address, port);

	sock = sock_connect(address, port);
	if (sock < 0) {
		return -1;
	}

	sock_send_string(sock, "hello\n");

	if (displayname != NULL) {
		sock_printf(sock, "client_set -name {%s}\n", displayname);

		// Use program name + hostname as default display name
	} else {
		struct utsname unamebuf;

		if (uname(&unamebuf) == 0)
			sock_printf(sock, "client_set -name {%s %s}\n", progname,
				    unamebuf.nodename);
		else
			sock_printf(sock, "client_set -name {%s}\n", progname);
	}

	if (menu_sock_send(main_menu, NULL, sock) < 0) {
		return -1;
	}

	return 0;
}

/**
 * \brief Process response
 * \param str char *str
 * \return Return value
 */
static int process_response(char *str)
{
	char *argv[20];
	int argc;

	debug(RPT_DEBUG, "Server said: \"%s\"", str);

	argc = get_args(argv, str, sizeof(argv) / sizeof(argv[0]));
	if (argc < 1)
		return 0;

	if (strcmp(argv[0], "menuevent") == 0) {

		if (argc < 2)
			goto err_invalid;

		// Handle select/leave events that trigger command execution
		if ((strcmp(argv[1], "select") == 0) || (strcmp(argv[1], "leave") == 0)) {
			MenuEntry *entry;

			if (argc < 3)
				goto err_invalid;

			entry = menu_find_by_id(main_menu, atoi(argv[2]));
			if (entry == NULL) {
				report(RPT_WARNING,
				       "Could not find the item id given by the server");
				return -1;
			}

			// Trigger on command entries without args or last arg of command entries
			if (((entry->type == MT_EXEC) && (entry->children == NULL)) ||
			    ((entry->type & MT_ARGUMENT) && (entry->next == NULL))) {

				// last arg => get parent entry
				if ((entry->type & MT_ARGUMENT) && (entry->next == NULL))
					entry = entry->parent;

				if (entry->type == MT_EXEC)
					exec_command(entry);
			}

			// Handle parameter value changes (plus/minus/update events)
		} else if ((strcmp(argv[1], "plus") == 0) || (strcmp(argv[1], "minus") == 0) ||
			   (strcmp(argv[1], "update") == 0)) {
			MenuEntry *entry;

			if (argc < 4)
				goto err_invalid;

			entry = menu_find_by_id(main_menu, atoi(argv[2]));
			if (entry == NULL) {
				report(RPT_WARNING,
				       "Could not find the item id given by the server");
				return -1;
			}

			switch (entry->type) {

			// Handle slider value update
			case MT_ARG_SLIDER:
				entry->data.slider.value = atoi(argv[3]);
				break;

			// Handle ring value update
			case MT_ARG_RING:
				entry->data.ring.value = atoi(argv[3]);
				break;

			// Handle numeric value update
			case MT_ARG_NUMERIC:
				entry->data.numeric.value = atoi(argv[3]);
				break;

			// Handle alpha string value update
			case MT_ARG_ALPHA:
				update_menu_string_value(&entry->data.alpha.value, argv[3]);
				break;

			// Handle IP address value update
			case MT_ARG_IP:
				update_menu_string_value(&entry->data.ip.value, argv[3]);
				break;

			// Handle checkbox value update
			case MT_ARG_CHECKBOX:
				if ((entry->data.checkbox.allow_gray) &&
				    (strcasecmp(argv[3], "gray") == 0))
					entry->data.checkbox.value = 2;
				else if (strcasecmp(argv[3], "on") == 0)
					entry->data.checkbox.value = 1;
				else
					entry->data.checkbox.value = 0;
				break;

			// Handle unsupported menu entry types
			default:
				report(RPT_WARNING, "Illegal menu entry type for event");
				return -1;
			}
		} else {
			; // Ignore other menuevents
		}
	} else if (strcmp(argv[0], "connect") == 0) {
		int a;

		// Extract LCD dimensions from connect response
		for (a = 1; a < argc; a++) {
			if (strcmp(argv[a], "wid") == 0)
				lcd_wid = atoi(argv[++a]);
			else if (strcmp(argv[a], "hgt") == 0)
				lcd_hgt = atoi(argv[++a]);
		}

	} else if (strcmp(argv[0], "bye") == 0) {
		report(RPT_INFO, "Server disconnected: %s", str);
		exit_program(EXIT_SUCCESS);

	} else if (strcmp(argv[0], "huh?") == 0) {
		report(RPT_WARNING, "Server error: %s", str);

	} else {
		debug(RPT_DEBUG, "Ignoring unknown server response: \"%s\"", str);
	}

	return 0;

err_invalid:
	report(RPT_WARNING, "Server gave invalid response");
	return -1;
}

/**
 * \brief Exec command
 * \param cmd MenuEntry *cmd
 * \return Return value
 */
static int exec_command(MenuEntry *cmd)
{
	if ((cmd != NULL) && (menu_command(cmd) != NULL)) {
		const char *command = menu_command(cmd);
		const char *argv[4];
		pid_t pid;
		ProcInfo *p;
		char *envp[cmd->numChildren + 1];
		MenuEntry *arg;
		int i;

		argv[0] = default_shell;
		argv[1] = "-c";
		argv[2] = command;
		argv[3] = NULL;

		// Convert menu parameters to environment variables
		for (arg = cmd->children, i = 0; arg != NULL; arg = arg->next, i++) {
			char buf[1025];

			switch (arg->type) {

			// Format slider value as environment variable
			case MT_ARG_SLIDER:
				format_env_int(buf, sizeof(buf), arg->name, arg->data.slider.value);
				break;

			// Format ring value as environment variable
			case MT_ARG_RING:
				format_env_string(buf, sizeof(buf), arg->name,
						  arg->data.ring.strings[arg->data.ring.value]);
				break;

			// Format numeric value as environment variable
			case MT_ARG_NUMERIC:
				format_env_int(buf, sizeof(buf), arg->name,
					       arg->data.numeric.value);
				break;

			// Format alpha string value as environment variable
			case MT_ARG_ALPHA:
				format_env_string(buf, sizeof(buf), arg->name,
						  arg->data.alpha.value);
				break;

			// Format IP address value as environment variable
			case MT_ARG_IP:
				format_env_string(buf, sizeof(buf), arg->name, arg->data.ip.value);
				break;

			// Format checkbox value as environment variable
			case MT_ARG_CHECKBOX:
				if (arg->data.checkbox.map[arg->data.checkbox.value] != NULL) {
					size_t len = strlen(
					    arg->data.checkbox.map[arg->data.checkbox.value]);
					if (len > sizeof(buf) - 1)
						len = sizeof(buf) - 1;
					memcpy(buf,
					       arg->data.checkbox.map[arg->data.checkbox.value],
					       len);
					buf[len] = '\0';
				} else {
					format_env_int(buf, sizeof(buf), arg->name,
						       arg->data.checkbox.value);
				}
				break;

			// Handle unknown argument types
			default:
				break;
			}

			buf[sizeof(buf) - 1] = '\0';
			envp[i] = strdup(buf);
			debug(RPT_DEBUG, "Environment: %s", envp[i]);
		}

		envp[cmd->numChildren] = NULL;
		debug(RPT_DEBUG, "Executing '%s' via Shell %s", command, default_shell);

		switch (pid = fork()) {

		// Child process: execute the command
		case 0:
			execve(argv[0], (char **)argv, envp);
			exit(EXIT_SUCCESS);
			break;

		// Parent process: setup ProcInfo structure
		default:
			p = calloc(1, sizeof(ProcInfo));
			if (p != NULL) {
				p->cmd = cmd;
				p->pid = pid;
				p->starttime = time(NULL);
				p->feedback = cmd->data.exec.feedback;
				p->next = proc_queue;
				proc_queue = p;
			}
			break;

		// Fork error: report failure and return
		case -1:
			report(RPT_ERR, "Could not fork");
			return -1;
		}

		for (i = 0; envp[i] != NULL; i++)
			free(envp[i]);

		return 0;
	}

	return -1;
}

/**
 * \brief Add status widgets to process screen
 * \param p Process information structure
 * \param is_multiline Add third status widget if non-zero
 *
 * \details Creates string widgets s1, s2 (and s3 for multiline displays)
 * to show process status information on the LCD screen.
 */
static void add_status_widgets(ProcInfo *p, int is_multiline)
{
	sock_printf(sock, "widget_add [%u] s1 string\n", p->pid);
	sock_printf(sock, "widget_add [%u] s2 string\n", p->pid);

	if (is_multiline)
		sock_printf(sock, "widget_add [%u] s3 string\n", p->pid);
}

/**
 * \brief Display process exit status on LCD
 * \param p Process information structure
 * \param status_y Y position for status message
 *
 * \details Shows success message, exit code (hex) for failures, or signal
 * number for terminations. Adapts formatting to display size.
 */
static void display_exit_status(ProcInfo *p, int status_y)
{
	int multiline = (lcd_hgt > 2);

	// Display process exit status on LCD: success message, exit code in hex for failures, or
	// signal number for terminations with display-size-adaptive formatting
	if (WIFEXITED(p->status)) {
		if (WEXITSTATUS(p->status) == EXIT_SUCCESS) {
			sock_printf(sock, "widget_set [%u] s2 1 %d {%s}\n", p->pid, status_y,
				    multiline ? "successfully." : "succeeded");
		} else {
			sock_printf(sock, "widget_set [%u] s2 1 %d {%s0x%02X%s}\n", p->pid,
				    status_y, multiline ? "with code " : "finished (",
				    WEXITSTATUS(p->status), multiline ? "." : ")");
		}
	} else if (WIFSIGNALED(p->status)) {
		sock_printf(sock, "widget_set [%u] s2 1 %d {killed by SIG %d%s}\n", p->pid,
			    status_y, WTERMSIG(p->status), multiline ? "." : "");
	}
}

/**
 * \brief Show procinfo msg
 * \param p ProcInfo *p
 * \return Return value
 */
static int show_procinfo_msg(ProcInfo *p)
{
	// Create alert screen for completed process: validate LCD dimensions, check completion and
	// feedback flags, calculate layout positions, send screen/widget setup commands
	if ((p != NULL) && (lcd_wid > 0) && (lcd_hgt > 0)) {
		if (p->endtime > 0) {
			if ((p->shown) || (!p->feedback))
				return 1;

			int is_multiline = (lcd_hgt > 2);
			int status_y = is_multiline ? 3 : 2;

			sock_printf(sock, "screen_add [%u]\n", p->pid);
			sock_printf(sock,
				    "screen_set [%u] -name {lcdexec [%u]}"
				    " -priority alert -timeout %d"
				    " -heartbeat off\n",
				    p->pid, p->pid, 6 * 8);

			// Add widgets for multi-line displays
			if (is_multiline) {
				sock_printf(sock, "widget_add [%u] t title\n", p->pid);
				sock_printf(sock, "widget_set [%u] t {%s}\n", p->pid,
					    p->cmd->displayname);
			}

			add_status_widgets(p, is_multiline);

			// Set first line content based on display type
			if (is_multiline) {
				sock_printf(sock, "widget_set [%u] s1 1 2 {[%u] finished%s}\n",
					    p->pid, p->pid, (WIFSIGNALED(p->status) ? "," : ""));
			} else {
				sock_printf(sock, "widget_set [%u] s1 1 1 {%s}\n", p->pid,
					    p->cmd->displayname);
			}

			display_exit_status(p, status_y);

			if (lcd_hgt > 3) {
				sock_printf(sock, "widget_set [%u] s3 1 4 {Exec time: %lds}\n",
					    p->pid, p->endtime - p->starttime);
			}

			return 1;
		}
	}

	return 0;
}

/**
 * \brief Main loop
 * \return Return value
 */
static int main_loop(void)
{
	int num_bytes;
	char buf[100];
	int keepalive_delay = 0;
	int status_delay = 0;

	// Main event loop: receive server messages, send keepalive every 3 seconds when idle, check
	// process status every second, handle server commands
	while (!Quit && ((num_bytes = sock_recv_string(sock, buf, sizeof(buf) - 1)) >= 0)) {
		if (num_bytes == 0) {
			ProcInfo *p;

			usleep(100000);

			if (keepalive_delay++ >= 30) {
				keepalive_delay = 0;
				if (sock_send_string(sock, "\n") < 0) {
					break;
				}
			}

			// Check for process status updates every second
			if (status_delay++ >= 10) {
				status_delay = 0;

				for (p = proc_queue; p != NULL; p = p->next) {
					ProcInfo *pn = p->next;

					if ((pn != NULL) && (pn->shown)) {
						p->next = pn->next;
						free(pn);
					}
				}

				// Deleting queue head is special
				if ((proc_queue != NULL) && (proc_queue->shown)) {
					p = proc_queue;
					proc_queue = proc_queue->next;
					free(p);
				}

				// Display process completion status
				for (p = proc_queue; p != NULL; p = p->next) {
					p->shown |= show_procinfo_msg(p);
				}
			}

		} else {
			process_response(buf);
		}
	}

	if (!Quit)
		report(RPT_ERR, "Server disconnected (or connection error)");
	return 0;
}
