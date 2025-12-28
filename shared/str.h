// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/str.h
 * \brief Command and argument parsing functions for LCDproc clients
 * \author LCDproc Development Team
 * \date Various years
 *
 * \features
 * - Command string tokenization and argument parsing
 * - Safe argument array population with bounds checking
 * - Whitespace-based string splitting functionality
 *
 * \usage
 * - Include this header to access string parsing functions
 * - Use get_args() to parse command strings into argument arrays
 * - Useful for network protocol command parsing
 *
 * \details Header file for string parsing utilities used by LCDproc clients
 * for command line argument processing and string tokenization.
 * Provides safe argument parsing with bounds checking.
 */

#ifndef STR_H
#define STR_H

/**
 * \brief Split elements of a string into an array of strings
 * \param argv Pointer to array that will store pointers to the parsed arguments
 * \param str String to be parsed (will be modified by strtok)
 * \param max_args Maximum number of arguments to parse (size of argv array)
 * \retval -1 Error: NULL argv pointer provided
 * \retval >=0 Number of arguments successfully parsed
 * \warning The input string is modified during parsing - use a copy if needed
 * \see strtok
 *
 * \details Parses a string by splitting it on whitespace delimiters (space and newline)
 * and stores pointers to the resulting tokens in the provided array. This function
 * modifies the input string by inserting null terminators at delimiter positions.
 * Commonly used for parsing command lines and argument lists in LCDproc clients.
 */
int get_args(char **argv, char *str, int max_args);

#endif
