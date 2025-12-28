// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/str.c
 * \brief Command and argument parsing utilities for LCDproc clients
 * \author LCDproc Development Team
 * \date Various years
 *
 * \features
 * - String tokenization using whitespace delimiters
 * - Safe argument array population with bounds checking
 * - Support for parsing command-style input strings
 * - Integration with the LCDproc reporting system
 *
 * \usage
 * - Include str.h in your source files
 * - Use get_args() to split command strings into argument arrays
 * - Useful for parsing network commands and configuration input
 *
 * \details This file provides string parsing utilities primarily used by
 * LCDproc clients for command line argument processing and string tokenization.
 * It includes functions for splitting strings into argument arrays, which is
 * useful for parsing commands received over network connections or from
 * configuration files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "report.h"
#include "str.h"

// Split elements of a string into an array of strings for argument parsing
int get_args(char **argv, char *str, int max_args)
{
	char *delimiters = " \n";
	char *item;
	int i = 0;

	if (!argv)
		return -1;
	if (!str)
		return 0;
	if (max_args < 1)
		return 0;

	debug(RPT_DEBUG, "get_args(%i): string=%s", max_args, str);

	// Tokenize string using strtok_r with whitespace delimiters
	char *saveptr;
	item = strtok_r(str, delimiters, &saveptr);
	while (item != NULL) {
		debug(RPT_DEBUG, "get_args: item=%s", item);

		if (i < max_args) {
			argv[i] = item;
			i++;
		} else {
			return i;
		}

		item = strtok_r(NULL, delimiters, &saveptr);
	}

	return i;
}
