// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/configfile.h
 * \brief Configuration file parsing interface for INI-style files
 * \author Joris Robijn, Peter Marschall
 * \date 2001-2007
 *
 * \features
 * - INI-style configuration file parsing with sections and key-value pairs
 * - Multiple data type accessors (bool, tristate, int, float, string)
 * - Default value handling when keys are missing
 * - Multiple values for the same key with skip parameter
 * - Section and key existence checking
 * - Memory management with config_clear()
 * - Optional string-based configuration parsing
 *
 * \usage
 * - Include this header to access configuration parsing functions
 * - Use config_read_file() to load configuration from files
 * - Access values with type-specific getter functions
 * - Check for section/key existence before accessing values
 *
 * \details This header file declares the interface for LCDproc's configuration
 * file parsing system. It provides functions to read and parse INI-style
 * configuration files, supporting sections, key-value pairs, and various data
 * types including booleans, integers, floats, and strings.
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/**
 * \brief Opens the specified file and reads everything into memory.
 * \param filename Path to the configuration file to read
 * \retval 0 Config file was successfully parsed
 * \retval <0 Error occurred during parsing
 */
int config_read_file(const char *filename);

/**
 * \brief Reads everything in the string into memory.
 * \param sectionname Name of the section to parse the string into
 * \param str Configuration string to parse
 * \retval 0 Config string was successfully parsed
 * \retval <0 Error occurred during parsing
 */
#if defined(LCDPROC_CONFIG_READ_STRING)
int config_read_string(const char *sectionname, const char *str);
#endif

/**
 * \brief Tries to interpret a value in the config file as a boolean.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \param skip Skip value for iterating over multiple values (0=first, 1=second, -1=last)
 * \param default_value Default value to return if key is not found
 * \return Boolean value: false for (0, false, off, no, n), true for (1, true, on, yes, y)
 *
 * \details If the key is not found or cannot be interpreted, the given default value is
 * returned. The skip value can be used to iterate over multiple values with the same
 * key. Should be 0 to get the first one, 1 for the second etc. and -1 for the last.
 */
short config_get_bool(const char *sectionname, const char *keyname, int skip, short default_value);

/**
 * \brief Tries to interpret a value in the config file as a tristate.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \param skip Skip value for iterating over multiple values (0=first, 1=second, -1=last)
 * \param name3rd Name for the third state value
 * \param default_value Default value to return if key is not found
 * \return Tristate value: 0 for (0, false, off, no, n), 1 for (1, true, on, yes, y), 2 for name3rd
 *
 * \details If the key is not found or cannot be interpreted, the given default value is
 * returned. The skip value can be used to iterate over multiple values with the same
 * key. Should be 0 to get the first one, 1 for the second etc. and -1 for the last.
 */
short config_get_tristate(const char *sectionname, const char *keyname, int skip,
			  const char *name3rd, short default_value);

/**
 * \brief Tries to interpret a value in the config file as an integer.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \param skip Skip value for iterating over multiple values (0=first, 1=second, -1=last)
 * \param default_value Default value to return if key is not found
 * \return Integer value parsed from the configuration file
 */
long int config_get_int(const char *sectionname, const char *keyname, int skip,
			long int default_value);

/**
 * \brief Tries to interpret a value in the config file as a float.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \param skip Skip value for iterating over multiple values (0=first, 1=second, -1=last)
 * \param default_value Default value to return if key is not found
 * \return Floating point value parsed from the configuration file
 */
double config_get_float(const char *sectionname, const char *keyname, int skip,
			double default_value);

/**
 * \brief Returns a pointer to the string associated with the specified key.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \param skip Skip value for iterating over multiple values (0=first, 1=second, -1=last)
 * \param default_value Default value to return if key is not found
 * \return Pointer to the string value (always NUL-terminated)
 * \warning The string should never be modified, and used only short-term.
 * \warning In successive calls this function can re-use the data space!
 *
 * \details You can do some things with the returned string:
 * \code
 * // 1. Scan or parse it:
 * s = config_get_string(...);
 * sscanf( s, "%dx%d", &w, &h );  // scan format like: 20x4
 *
 * // 2. Copy it to a preallocated buffer like device[256]:
 * s = config_get_string(...);
 * strncpy( device, s, sizeof(device));
 * device[sizeof(device)-1] = 0;
 *
 * // 3. Copy it to a newly allocated space in char *device:
 * s = config_get_string(...);
 * device = malloc(strlen(s)+1);
 * if( device == NULL ) return -5; // or whatever < 0
 * strcpy( device, s );
 * \endcode
 */
const char *config_get_string(const char *sectionname, const char *keyname, int skip,
			      const char *default_value);

/**
 * \brief Checks if a specified section exists.
 * \param sectionname Name of the section to check
 * \return Non-zero if section exists, zero otherwise
 */
int config_has_section(const char *sectionname);

/**
 * \brief Checks if a specified key within the specified section exists.
 * \param sectionname Name of the configuration section
 * \param keyname Name of the key within the section
 * \return Number of times the key exists in the section
 */
int config_has_key(const char *sectionname, const char *keyname);

/**
 * \brief Clears all data stored by the config_read_* functions.
 *
 * \details Should be called if the config should be reread.
 * This function frees all memory allocated for configuration data.
 */
void config_clear(void);

#endif
