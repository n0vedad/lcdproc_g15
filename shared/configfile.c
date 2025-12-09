// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/configfile.c
 * \brief Configuration file parser implementation for INI-style files
 * \author Joris Robijn, Rene Wagner, Peter Marschall
 * \date 2001-2007
 *
 * \features
 * - INI-style parsing with [sections] and key=value pairs
 * - Support for boolean, integer, float, and string values
 * - Tristate values (0/1/2 or false/true/custom)
 * - Multi-valued keys (multiple values for the same key)
 * - Quoted strings with escape sequence support
 * - Comment support (# and ; characters)
 * - Flexible data type retrieval with default values
 * - Memory-based configuration storage
 *
 * \usage
 * - Include configfile.h in your source files
 * - Use config_read_file() to parse configuration files
 * - Retrieve values with config_get_string(), config_get_int(), etc.
 * - Free resources with config_clear()
 *
 * \details This file provides a complete configuration file parsing system
 * for INI-style configuration files. It supports sections, key-value pairs,
 * comments, quoted strings with escape sequences, and various data type
 * conversions. The implementation uses linked lists for sections and keys
 * with a finite state machine parser for character-by-character processing.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "shared/report.h"

/**
 * \brief Configuration key-value pair structure
 * \details Represents a single key=value entry in a configuration file section
 */
typedef struct _config_key {
	char *name;		      ///< Key name
	char *value;		      ///< Key value
	struct _config_key *next_key; ///< Next key in linked list
} ConfigKey;

/**
 * \brief Configuration section structure
 * \details Represents a [section] in a configuration file with its associated keys
 */
typedef struct _config_section {
	char *name;			      ///< Section name
	ConfigKey *first_key;		      ///< First key in this section
	struct _config_section *next_section; ///< Next section in linked list
} ConfigSection;

/** \brief Head of global configuration section linked list
 *
 * \details Pointer to first section in the parsed configuration file.
 * NULL if no configuration file has been loaded.
 */
static ConfigSection *first_section = NULL;

// Internal function declarations
static ConfigSection *find_section(const char *sectionname);
static ConfigSection *add_section(const char *sectionname);
static ConfigKey *find_key(ConfigSection *s, const char *keyname, int skip);
static ConfigKey *add_key(ConfigSection *s, const char *keyname, const char *value);

// String parsing mode functions
#if defined(LCDPROC_CONFIG_READ_STRING)
static char get_next_char_f(FILE *f);
static int process_config(ConfigSection **current_section, char (*get_next_char)(),
			  const char *source_descr, FILE *f);
#else
static int process_config(ConfigSection **current_section, const char *source_descr, FILE *f);
#endif

// Parse configuration from INI-file style config file into memory
int config_read_file(const char *filename)
{
	FILE *f;
	ConfigSection *curr_section = NULL;
	int result = 0;

	report(RPT_NOTICE, "Using Configuration File: %s", filename);

	f = fopen(filename, "r");
	if (f == NULL) {
		return -1;
	}

#if defined(LCDPROC_CONFIG_READ_STRING)
	result = process_config(&curr_section, get_next_char_f, filename, f);
#else
	result = process_config(&curr_section, filename, f);
#endif

	fclose(f);

	return result;
}

// Parse configuration from memory string into specified section
#if defined(LCDPROC_CONFIG_READ_STRING)
int config_read_string(const char *sectionname, const char *str)
{
	int pos = 0;
	ConfigSection *s;

	char get_next_char() { return str[pos++]; }

	if ((s = find_section(sectionname)) == NULL)
		s = add_section(sectionname);

	return process_config(&s, get_next_char, "command line", NULL);
}
#endif

// Get string value from configuration in memory
const char *config_get_string(const char *sectionname, const char *keyname, int skip,
			      const char *default_value)
{
	ConfigKey *k = find_key(find_section(sectionname), keyname, skip);

	if (k == NULL)
		return default_value;

	return k->value;
}

/**
 * \brief Parse string as boolean value
 * \param value String to parse
 * \retval 0 False-like value ("0", "false", "n", "no", "off")
 * \retval 1 True-like value ("1", "true", "y", "yes", "on")
 * \retval -1 Not recognized as boolean
 *
 * \details Case-insensitive parsing of common boolean string representations.
 */
static short parse_bool_value(const char *value)
{
	// Check for false-like values (case insensitive)
	if ((strcasecmp(value, "0") == 0) || (strcasecmp(value, "false") == 0) ||
	    (strcasecmp(value, "n") == 0) || (strcasecmp(value, "no") == 0) ||
	    (strcasecmp(value, "off") == 0)) {
		return 0;
	}

	// Check for true-like values (case insensitive)
	if ((strcasecmp(value, "1") == 0) || (strcasecmp(value, "true") == 0) ||
	    (strcasecmp(value, "y") == 0) || (strcasecmp(value, "yes") == 0) ||
	    (strcasecmp(value, "on") == 0)) {
		return 1;
	}

	return -1;
}

// Get boolean value from configuration in memory
short config_get_bool(const char *sectionname, const char *keyname, int skip, short default_value)
{
	ConfigKey *k = find_key(find_section(sectionname), keyname, skip);
	short result;

	if (k == NULL)
		return default_value;

	result = parse_bool_value(k->value);
	return (result >= 0) ? result : default_value;
}

// Get tristate value from configuration in memory
short config_get_tristate(const char *sectionname, const char *keyname, int skip,
			  const char *name3rd, short default_value)
{
	ConfigKey *k = find_key(find_section(sectionname), keyname, skip);
	short result;

	if (k == NULL)
		return default_value;

	// Try parsing as boolean first
	result = parse_bool_value(k->value);
	if (result >= 0)
		return result;

	// Check for third state values - numeric "2" or custom name
	if ((strcasecmp(k->value, "2") == 0) ||
	    ((name3rd != NULL) && (strcasecmp(k->value, name3rd) == 0))) {
		return 2;
	}

	return default_value;
}

/**
 * \brief Check if strtol/strtod conversion was valid
 * \param original Original input string
 * \param end End pointer from strtol/strtod
 * \retval 1 Valid conversion (entire string consumed)
 * \retval 0 Invalid conversion (partial or no conversion)
 *
 * \details Validates that conversion functions consumed entire input string.
 */
static int is_valid_conversion(const char *original, const char *end)
{
	return (end != NULL) && (end != original) && (*end == '\0');
}

// Get integer from configuration in memory
long int config_get_int(const char *sectionname, const char *keyname, int skip,
			long int default_value)
{
	ConfigKey *k = find_key(find_section(sectionname), keyname, skip);

	// Parse integer from config value with validation: convert string to long, verify valid
	// conversion, return parsed value or default
	if (k != NULL) {
		char *end;
		long int v = strtol(k->value, &end, 0);

		if (is_valid_conversion(k->value, end))
			return v;
	}

	return default_value;
}

// Get floating point number from configuration in memory
double config_get_float(const char *sectionname, const char *keyname, int skip,
			double default_value)
{
	ConfigKey *k = find_key(find_section(sectionname), keyname, skip);

	// Parse floating point from config value with validation: convert string to double, verify
	// valid conversion, return parsed value or default
	if (k != NULL) {
		char *end;
		double v = strtod(k->value, &end);

		if (is_valid_conversion(k->value, end))
			return v;
	}

	return default_value;
}

// Test whether the configuration contains a specific section
int config_has_section(const char *sectionname)
{
	return (find_section(sectionname) != NULL) ? 1 : 0;
}

// Test whether the configuration contains a specific key in a specific section
int config_has_key(const char *sectionname, const char *keyname)
{
	ConfigSection *s = find_section(sectionname);
	int count = 0;

	// Count occurrences of key name in section using case-insensitive comparison for
	// multi-value config keys
	if (s != NULL) {
		ConfigKey *k;

		for (k = s->first_key; k != NULL; k = k->next_key) {
			if (strcasecmp(k->name, keyname) == 0)
				count++;
		}
	}

	return count;
}

// Clear configuration
void config_clear(void)
{
	ConfigSection *s;
	ConfigSection *next_s;

	// Deep cleanup of config structure: free all keys within each section, then free sections,
	// preserving next pointers before deallocation
	for (s = first_section; s != NULL; s = next_s) {
		ConfigKey *k;
		ConfigKey *next_k;

		for (k = s->first_key; k != NULL; k = next_k) {
			next_k = k->next_key;
			free(k->name);
			free(k->value);
			free(k);
		}

		next_s = s->next_section;
		free(s->name);
		free(s);
	}

	first_section = NULL;
}

/**
 * \brief Find configuration section by name
 * \param sectionname Section name to search for
 * \return Pointer to section, or NULL if not found
 *
 * \details Case-insensitive search through linked list of sections.
 */
static ConfigSection *find_section(const char *sectionname)
{
	ConfigSection *s;

	for (s = first_section; s != NULL; s = s->next_section) {
		if (strcasecmp(s->name, sectionname) == 0) {
			return s;
		}
	}

	return NULL;
}

/**
 * \brief Create and add new configuration section
 * \param sectionname Name for new section
 * \return Pointer to new section, or NULL on allocation failure
 *
 * \details Allocates new section at end of linked list.
 */
static ConfigSection *add_section(const char *sectionname)
{
	ConfigSection *s;
	ConfigSection **place = &first_section;

	// Traverse section list to find insertion point and initialize at the end
	for (s = first_section; s != NULL; s = s->next_section)
		place = &(s->next_section);

	*place = (ConfigSection *)malloc(sizeof(ConfigSection));
	if (*place != NULL) {
		(*place)->name = strdup(sectionname);
		(*place)->first_key = NULL;
		(*place)->next_section = NULL;
	}

	return (*place);
}

/**
 * \brief Find configuration key in section
 * \param s Section to search in
 * \param keyname Key name to search for
 * \param skip Number of matches to skip (0=first, -1=last)
 * \return Pointer to key, or NULL if not found
 *
 * \details Case-insensitive search. skip=-1 returns last match,
 * useful for overriding values. skip=N returns Nth occurrence.
 */
static ConfigKey *find_key(ConfigSection *s, const char *keyname, int skip)
{
	ConfigKey *k;
	int count = 0;
	ConfigKey *last_key = NULL;

	if (s == NULL)
		return NULL;

	for (k = s->first_key; k != NULL; k = k->next_key) {
		if (strcasecmp(k->name, keyname) == 0) {
			if (count == skip)
				return k;

			count++;
			last_key = k;
		}
	}

	// Special case: skip=-1 means return last matching key
	if (skip == -1)
		return last_key;

	return NULL;
}

/**
 * \brief Add new key-value pair to section
 * \param s Section to add key to
 * \param keyname Key name
 * \param value Key value
 * \return Pointer to new key, or NULL on failure
 *
 * \details Allocates and appends key at end of section's key list.
 */
static ConfigKey *add_key(ConfigSection *s, const char *keyname, const char *value)
{
	if (s != NULL) {
		ConfigKey *k;
		ConfigKey **place = &(s->first_key);

		// Traverse key list, allocate new key-value pair at end, and initialize with
		// duplicated strings
		for (k = s->first_key; k != NULL; k = k->next_key)
			place = &(k->next_key);

		*place = (ConfigKey *)malloc(sizeof(ConfigKey));
		if (*place != NULL) {
			(*place)->name = strdup(keyname);
			(*place)->value = strdup(value);
			(*place)->next_key = NULL;
		}

		return (*place);
	}

	return NULL;
}

/**
 * \brief Read next character from file stream
 * \param f File stream to read from
 * \return Next character or '\0' on EOF
 *
 * \details Helper function for config file parsing in string mode.
 * Converts EOF to null terminator for unified parsing logic.
 */
#if defined(LCDPROC_CONFIG_READ_STRING)
static char get_next_char_f(FILE *f)
{
	int c = fgetc(f);
	return ((c == EOF) ? '\0' : c);
}
#endif

/** \brief Initial parser state */
#define ST_INITIAL 0
/** \brief Parser state: inside comment */
#define ST_COMMENT 257
/** \brief Parser state: reading section label */
#define ST_SECTIONLABEL 258
/** \brief Parser state: reading key name */
#define ST_KEYNAME 259
/** \brief Parser state: reading assignment operator */
#define ST_ASSIGNMENT 260
/** \brief Parser state: reading value */
#define ST_VALUE 261
/** \brief Parser state: reading quoted value */
#define ST_QUOTEDVALUE 262
/** \brief Parser state: section label completed */
#define ST_SECTIONLABEL_DONE 263
/** \brief Parser state: value completed */
#define ST_VALUE_DONE 264
/** \brief Parser state: invalid section label encountered */
#define ST_INVALID_SECTIONLABEL 265
/** \brief Parser state: invalid key name encountered */
#define ST_INVALID_KEYNAME 266
/** \brief Parser state: invalid assignment encountered */
#define ST_INVALID_ASSIGNMENT 267
/** \brief Parser state: invalid value encountered */
#define ST_INVALID_VALUE 268
/** \brief Parser state: end of parsing */
#define ST_END 999

/** \brief Maximum length of section label in config file */
#define MAXSECTIONLABELLENGTH 40
/** \brief Maximum length of key name in config file */
#define MAXKEYNAMELENGTH 40
/** \brief Maximum length of value in config file */
#define MAXVALUELENGTH 200

/**
 * \brief Parse INI-format configuration file with finite state machine
 * \param current_section Pointer to current section pointer
 * \param get_next_char Function pointer to character reading function (string mode only)
 * \param source_descr Source description for error messages
 * \param f File handle to read from (or string context in string mode)
 * \retval 0 Success
 * \retval <0 Parse error occurred
 *
 * \details Implements a finite state machine parser for INI-format configuration
 * files. Handles sections [name], key=value pairs, comments, and escape sequences.
 * Two versions exist: one for file mode and one for string mode with custom
 * character reader function.
 */
#if defined(LCDPROC_CONFIG_READ_STRING)
static int process_config(ConfigSection **current_section, char (*get_next_char)(),
			  const char *source_descr, FILE *f)
#else
static int process_config(ConfigSection **current_section, const char *source_descr, FILE *f)
#endif
{
	int state = ST_INITIAL;
	int ch;
	char sectionname[MAXSECTIONLABELLENGTH + 1];
	int sectionname_pos = 0;
	char keyname[MAXKEYNAMELENGTH + 1];
	int keyname_pos = 0;
	char value[MAXVALUELENGTH + 1];
	int value_pos = 0;
	int escape = 0;
	int line_nr = 1;
	int error = 0;

#if !defined(LCDPROC_CONFIG_READ_STRING)
	if (f == NULL)
		return (0);
#endif

	// Main parsing loop - continue until ST_END state reached
	while (state != ST_END) {

#if defined(LCDPROC_CONFIG_READ_STRING)
		ch = (f != NULL) ? get_next_char(f) : get_next_char();
#else
		ch = fgetc(f);
		if (ch == EOF)
			ch = '\0';
#endif

		if (ch == '\n')
			line_nr++;

		// Finite state machine - process character based on current state
		switch (state) {

		// Initial parsing state - waiting for content to parse
		case ST_INITIAL:

			switch (ch) {

			// Handle comment start characters
			case '#':
			case ';':
				state = ST_COMMENT;

			// Ignore all forms of whitespace and line terminators
			case '\0':
			case '\n':
			case '\r':
			case '\t':
			case ' ':
				break;

			// Start section label parsing
			case '[':
				state = ST_SECTIONLABEL;
				sectionname_pos = 0;
				sectionname[sectionname_pos] = '\0';
				break;

			// Any other character starts a key name
			default:
				state = ST_KEYNAME;
				keyname_pos = 0;
				keyname[keyname_pos++] = ch;
				keyname[keyname_pos] = '\0';
			}
			break;

		// Parse section label between brackets
		case ST_SECTIONLABEL:
			switch (ch) {

			// Handle unterminated section label
			case '\0':
			case '\n':
				report(RPT_WARNING,
				       "Unterminated section label on line %d of %s: %s", line_nr,
				       source_descr, sectionname);
				error = 1;
				state = ST_INITIAL;
				break;

			// Complete section label parsing
			case ']':
				if (!(*current_section = find_section(sectionname))) {
					*current_section = add_section(sectionname);
				}
				state = ST_SECTIONLABEL_DONE;
				break;

			// Append character to section name
			default:
				if (sectionname_pos < MAXSECTIONLABELLENGTH) {
					sectionname[sectionname_pos++] = ch;
					sectionname[sectionname_pos] = '\0';
					break;
				}
				report(RPT_WARNING, "Section name too long on line %d of %s: %s",
				       line_nr, source_descr, sectionname);
				error = 1;
				state = ST_INVALID_SECTIONLABEL;
			}
			break;

		// Parse key name
		case ST_KEYNAME:
			switch (ch) {

			// Handle whitespace after key name
			case '\r':
			case '\t':
			case ' ':
				if (keyname_pos != 0)
					state = ST_ASSIGNMENT;
				break;

			// Handle incomplete key name
			case '\0':
			case '\n':
				report(RPT_WARNING, "Loose word found on line %d of %s: %s",
				       line_nr, source_descr, keyname);
				error = 1;
				state = ST_INITIAL;
				break;

			// Handle assignment operator
			case '=':
				state = ST_VALUE;
				value[0] = '\0';
				value_pos = 0;
				break;

			// Append character to key name
			default:
				if (keyname_pos < MAXKEYNAMELENGTH) {
					keyname[keyname_pos++] = ch;
					keyname[keyname_pos] = '\0';
					break;
				}
				report(RPT_WARNING, "Key name too long on line %d of %s: %s",
				       line_nr, source_descr, keyname);
				error = 1;
				state = ST_INVALID_KEYNAME;
			}
			break;

		// Parse assignment operator
		case ST_ASSIGNMENT:
			switch (ch) {

			// Ignore leading spaces before assignment
			case '\t':
			case ' ':
				break;

			// Found assignment operator
			case '=':
				state = ST_VALUE;
				value[0] = '\0';
				value_pos = 0;
				break;

			// Handle missing assignment operator
			default:
				report(RPT_WARNING, "Assignment expected on line %d of %s: %s",
				       line_nr, source_descr, keyname);
				error = 1;
				state = ST_INVALID_ASSIGNMENT;
			}
			break;

		// Parse value
		case ST_VALUE:
			switch (ch) {
			// Comment after value only
			case '#':
			case ';':
				if (value_pos > 0) {
					state = ST_COMMENT;
					break;
				}

			// Reserved characters forbidden
			case '[':
			case ']':
			case '=':
				report(
				    RPT_WARNING,
				    "Invalid character '%c' in value on line %d of %s, at key: %s",
				    ch, line_nr, source_descr, keyname);
				error = 1;
				state = ST_INVALID_VALUE;
				break;

			// Handle whitespace in values
			case '\t':
			case ' ':
				if (value_pos == 0)
					break;

			// Handle value termination characters
			case '\0':
			case '\n':
			case '\r':
				if (!*current_section) {
					report(
					    RPT_WARNING,
					    "Data outside sections on line %d of %s with key: %s",
					    line_nr, source_descr, keyname);
					error = 1;
				} else {
					(void)add_key(*current_section, keyname, value);
				}
				state = ((ch == ' ') || (ch == '\t')) ? ST_VALUE_DONE : ST_INITIAL;
				break;

			// Begin quoted string parsing
			case '"':
				state = ST_QUOTEDVALUE;
				break;

			// Append regular characters to value
			default:
				if (value_pos < MAXVALUELENGTH) {
					value[value_pos++] = ch;
					value[value_pos] = '\0';
					break;
				}
				report(RPT_WARNING, "Value too long on line %d of %s, at key: %s",
				       line_nr, source_descr, keyname);
				error = 1;
				state = ST_INVALID_VALUE;
			}
			break;

		// Parse quoted strings with escape sequences
		case ST_QUOTEDVALUE:
			switch (ch) {
			// Unterminated quoted string
			case '\0':
			case '\n':
				report(RPT_WARNING,
				       "Premature end of quoted string on line %d of %s: %s",
				       line_nr, source_descr, keyname);
				error = 1;
				state = ST_INITIAL;
				break;

			// Handle escape sequences
			case '\\':
				if (!escape) {
					escape = 1;
					break;
				}

			// Handle end of quoted string
			case '"':
				if (!escape) {
					state = ST_VALUE;
					break;
				}

			// Handle regular characters
			default:
				if (escape) {
					// Convert escape sequences
					switch (ch) {
					case 'a':
						ch = '\a';
						break;
					case 'b':
						ch = '\b';
						break;
					case 'f':
						ch = '\f';
						break;
					case 'n':
						ch = '\n';
						break;
					case 'r':
						ch = '\r';
						break;
					case 't':
						ch = '\t';
						break;
					case 'v':
						ch = '\v';
						break;

					// Keep character as-is for unknown escape sequences
					default:
						break;
					}
					escape = 0;
				}
				value[value_pos++] = ch;
				value[value_pos] = '\0';
			}
			break;

		// Handle post-section-label and post-value states
		case ST_SECTIONLABEL_DONE:
		case ST_VALUE_DONE:
			switch (ch) {

			// Handle comment start
			case ';':
			case '#':
				state = ST_COMMENT;
				break;

			// Handle line termination
			case '\0':
			case '\n':
				state = ST_INITIAL;
				break;

			// Ignore trailing whitespace
			case '\t':
			case ' ':
				break;

			// Handle invalid characters
			default:
				report(RPT_WARNING, "Invalid character '%c' on line %d of %s", ch,
				       line_nr, source_descr);
				error = 1;
				state = ST_INVALID_VALUE;
			}

		// Handle invalid states - resync to next line for error recovery
		case ST_INVALID_SECTIONLABEL:
			if (ch == ']')
				state = ST_INITIAL;

		case ST_INVALID_ASSIGNMENT:
		case ST_INVALID_KEYNAME:
		case ST_INVALID_VALUE:

		// Handle comment parsing - consume characters until end of line
		case ST_COMMENT:
			if (ch == '\n')
				state = ST_INITIAL;
			break;

		// Handle unexpected states
		default:
			break;
		}

		// Check for end of input (null terminator)
		if (ch == '\0') {
			if ((!error) && (state != ST_INITIAL) && (state != ST_COMMENT) &&
			    (state != ST_SECTIONLABEL_DONE) && (state != ST_VALUE_DONE)) {
				report(RPT_WARNING,
				       "Premature end of configuration on line %d of %s: %d",
				       line_nr, source_descr, state);
				error = 1;
			}
			state = ST_END;
		}
	}

	return (error) ? -1 : 0;
}

// Debug dump of all sections and keys
#if CONFIGFILE_DEBUGTEST
void config_dump(void)
{
	ConfigSection *s;

	for (s = first_section; s != NULL; s = s->next_section) {
		ConfigKey *k;

		fprintf(stderr, "[%s]\n", s->name);

		for (k = s->first_key; k != NULL; k = k->next_key)
			fprintf(stderr, "%s = \"%s\"\n", k->name, k->value);

		fprintf(stderr, "\n");
	}
}

// Test main function
int main(int argc, char *argv[])
{
	if (argc > 0)
		config_read_file(argv[1]);
	config_dump();
}
#endif
