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

#include <errno.h>
#include <limits.h>
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

// Convert a decimal string to an int, rejecting trailing characters and overflow
int str_to_int(const char *s, int *value)
{
	char *end;
	long v;

	if (s == NULL || *s == '\0') {
		return -1;
	}

	errno = 0;
	v = strtol(s, &end, 10);
	if (errno != 0 || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) {
		return -1;
	}

	*value = (int)v;
	return 0;
}

// Read up to count whitespace-separated unsigned integers
int str_to_ulongs(const char *s, unsigned long *values, int count)
{
	int n = 0;

	while (s != NULL && n < count) {
		char *end;

		while (*s == ' ' || *s == '\t') {
			s++;
		}
		// strtoul() would accept and negate a leading minus sign
		if (*s == '-') {
			break;
		}

		errno = 0;
		unsigned long v = strtoul(s, &end, 10);
		if (errno != 0 || end == s) {
			break;
		}

		values[n++] = v;
		s = end;
	}

	return n;
}

// Read up to count whitespace-separated floating point numbers
int str_to_doubles(const char *s, double *values, int count)
{
	int n = 0;

	while (s != NULL && n < count) {
		char *end;

		errno = 0;
		double v = strtod(s, &end);
		if (errno != 0 || end == s) {
			break;
		}

		values[n++] = v;
		s = end;
	}

	return n;
}
