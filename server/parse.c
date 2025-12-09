// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/parse.c
 * \brief Client message parsing and command dispatching implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Peter Marschall
 * \date 1999-2008
 *
 * \features
 * - Client message tokenization and parsing
 * - Command argument extraction and validation
 * - Protocol command dispatching
 * - Quote handling for string arguments
 * - Multi-client message processing
 *
 * \usage
 * - State machine based parser for robust tokenization
 * - Support for quoted strings and escape sequences
 * - Command lookup and handler dispatch
 * - Error handling and client notification
 * - Maximum argument limits for security
 *
 * \details Handles input commands from clients by splitting strings into tokens
 * and passing arguments to the appropriate handler. The parser works much like
 * a command line interface where only the first token is used to determine
 * what function to call.
 */

#include "parse.h"
#include "clients.h"
#include "sock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands/command_list.h"
#include "shared/LL.h"
#include "shared/report.h"
#include "shared/sockets.h"

/** \brief Maximum number of arguments allowed in a single command
 *
 * \details Maximum number of space-separated arguments that can be parsed
 * from a single client command line. Exceeding this limit will result in
 * argument truncation.
 */
#define MAX_ARGUMENTS 40

/**
 * \brief Check if character is whitespace
 * \param x Character to test
 * \retval 1 Character is space, tab, or carriage return
 * \retval 0 Character is not whitespace
 */
static inline int is_whitespace(char x) { return ((x == ' ') || (x == '\t') || (x == '\r')); }

/**
 * \brief Check if character terminates input
 * \param x Character to test
 * \retval 1 Character is newline or null terminator
 * \retval 0 Character is not a terminator
 */
static inline int is_final(char x) { return ((x == '\n') || (x == '\0')); }

/**
 * \brief Check if character opens quoted string
 * \param x Character to test
 * \param q Current quote state ('\0' if not in quote)
 * \retval 1 Character opens quote (\" or {) when not already quoted
 * \retval 0 Not an opening quote
 */
static inline int is_opening_quote(char x, char q)
{
	return ((q == '\0') && ((x == '\"') || (x == '{')));
}

/**
 * \brief Check if character closes current quoted string
 * \param x Character to test
 * \param q Current quote character ('{' or '\"')
 * \retval 1 Character closes current quote (} closes {, \" closes \")
 * \retval 0 Not a closing quote
 */
static inline int is_closing_quote(char x, char q)
{
	return (((q == '{') && (x == '}')) || ((q == '\"') && (x == '\"')));
}

/**
 * \brief Parse a single client message and dispatch command
 * \param str Message string to parse
 * \param c Client that sent the message
 *
 * \details Parses client protocol messages, tokenizes arguments, and dispatches
 * to appropriate command handlers. Supports quoted strings and escape sequences.
 */
static void parse_message(const char *str, Client *c)
{
	typedef enum { ST_INITIAL, ST_WHITESPACE, ST_ARGUMENT, ST_FINAL } State;
	State state = ST_INITIAL;

	int error = 0;
	char quote = '\0';
	int pos = 0;
	char arg_space[strlen(str) + 1];
	int argc = 0;
	char *argv[MAX_ARGUMENTS];
	int argpos = 0;
	CommandFunc function = NULL;

	debug(RPT_DEBUG, "%s(str=\"%.120s\", client=[%d])", __FUNCTION__, str, c->sock);

	// Initialize argv[0] to point to start of argument buffer
	argv[0] = arg_space;

	// State machine loop processes each character until final state or error
	while ((state != ST_FINAL) && !error) {
		char ch = str[pos++];

		switch (state) {

		// Initial state
		case ST_INITIAL:
		// Whitespace between arguments
		case ST_WHITESPACE:
			if (is_whitespace(ch))
				break;
			if (is_final(ch)) {
				state = ST_FINAL;
				break;
			}

			state = ST_ARGUMENT;

		// Argument parsing
		case ST_ARGUMENT:
			if (is_final(ch)) {
				// Finalize current argument and transition to final state
				if (quote)
					error = 2;
				if (argc >= MAX_ARGUMENTS - 1) {
					error = 1;
				} else {
					argv[argc][argpos] = '\0';
					argv[argc + 1] = argv[argc] + argpos + 1;
					argc++;
					argpos = 0;
				}
				state = ST_FINAL;

			} else if (ch == '\\') {
				// Process escape sequences (\n, \r, \t, or literal character)
				if (str[pos]) {
					const char escape_chars[] = "nrt";
					const char escape_trans[] = "\n\r\t";
					char *p = strchr(escape_chars, str[pos]);

					/**
					 * \todo Is it wise to have the characters \\n, \\r & \\t
					 * expanded? Can the displays deal with them?
					 *
					 * The parser currently expands escape sequences (\\n, \\r,
					 * \\t) in client commands, but it's unclear whether all LCD
					 * displays can handle these special characters correctly.
					 * Some displays may interpret them as control codes,
					 * causing rendering issues or unexpected behavior.
					 *
					 * Affected areas:
					 * - String widget content
					 * - Menu item text
					 * - Title text
					 * - Any client-supplied text content
					 *
					 * Should verify display driver compatibility and possibly
					 * add escaping option or filtering.
					 *
					 * Impact: Display compatibility, text rendering
					 * reliability, protocol design
					 *
					 * \ingroup ToDo_low
					 */

					// Translate escape sequence or copy literal character
					if (p != NULL) {
						argv[argc][argpos++] =
						    escape_trans[p - escape_chars];
					} else {
						argv[argc][argpos++] = str[pos];
					}
					pos++;

				} else {
					// Backslash at end of string is an error
					error = 2;

					if (argc >= MAX_ARGUMENTS - 1) {
						error = 1;
					} else {
						argv[argc][argpos] = '\0';
						argv[argc + 1] = argv[argc] + argpos + 1;
						argc++;
						argpos = 0;
					}
					state = ST_FINAL;
				}

			} else if (is_opening_quote(ch, quote)) {
				// Start quoted section (don't include quote character)
				quote = ch;

			} else if (is_closing_quote(ch, quote)) {
				// End quoted section and finalize argument
				quote = '\0';
				if (argc >= MAX_ARGUMENTS - 1) {
					error = 1;
				} else {
					argv[argc][argpos] = '\0';
					argv[argc + 1] = argv[argc] + argpos + 1;
					argc++;
					argpos = 0;
				}
				state = ST_WHITESPACE;

			} else if (is_whitespace(ch) && (quote == '\0')) {
				// Whitespace outside quotes finalizes current argument
				if (argc >= MAX_ARGUMENTS - 1) {
					error = 1;
				} else {
					argv[argc][argpos] = '\0';
					argv[argc + 1] = argv[argc] + argpos + 1;
					argc++;
					argpos = 0;
				}
				state = ST_WHITESPACE;

			} else {
				// Regular character - append to current argument
				argv[argc][argpos++] = ch;
			}
			break;

		// Final state
		case ST_FINAL:
			break;
		}
	}

	// Null-terminate argv array for safe iteration
	if (argc < MAX_ARGUMENTS)
		argv[argc] = NULL;
	else
		error = 1;

	// Send parse error to client and abort processing
	if (error) {
		sock_send_error(c->sock, "Could not parse command\n");
		return;
	}

	// Look up command handler function by first argument
	function = get_command_function(argv[0]);

	if (function != NULL) {
		// Execute command handler and report any errors
		error = function(c, argc, argv);
		if (error) {
			sock_printf_error(c->sock, "Function returned error \"%.40s\"\n", argv[0]);
			report(RPT_WARNING,
			       "Command function returned an error after command from client on "
			       "socket %d: %.40s",
			       c->sock, str);
		}
	} else {
		// Unknown command - send error response
		sock_printf_error(c->sock, "Invalid command \"%.40s\"\n", argv[0]);
		report(RPT_WARNING, "Invalid command from client on socket %d: %.40s", c->sock,
		       str);
	}
}

// Parse and process all pending client messages
void parse_all_client_messages(void)
{
	Client *c;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	// Iterate through all connected clients
	for (c = clients_getfirst(); c != NULL; c = clients_getnext()) {
		char *str;

		// Process all queued messages for this client and stop processing if client
		// disconnected
		for (str = client_get_message(c); str != NULL; str = client_get_message(c)) {
			parse_message(str, c);
			free(str);

			if (c->state == GONE) {
				sock_destroy_client_socket(c);
				break;
			}
		}
	}
}
