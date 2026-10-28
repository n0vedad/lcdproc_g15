// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/test_unit_str.c
 * \brief Unit tests for the checked number conversions in shared/str.c.
 * \author Copyright (C) 2026 n0vedad <https://github.com/n0vedad/>
 * \date 2026
 *
 * \features
 * - str_to_int(): valid numbers, trailing characters, empty input, int overflow
 * - str_to_ulongs(): /proc style lines, early stop, negative numbers
 * - str_to_doubles(): /proc/uptime and /proc/net/dev style lines
 *
 * \details These helpers replace atoi() and sscanf() for protocol arguments,
 * command line options and /proc parsing, so they must reject what atoi()
 * silently accepted.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "shared/str.h"

// str_to_int() accepts whole decimal numbers only
static void test_str_to_int(void)
{
	int v = 42;

	assert(str_to_int("0", &v) == 0 && v == 0);
	assert(str_to_int("128", &v) == 0 && v == 128);
	assert(str_to_int("-7", &v) == 0 && v == -7);
	assert(str_to_int("2147483647", &v) == 0 && v == INT_MAX);

	// Rejected input leaves the value unchanged
	v = 42;
	assert(str_to_int("", &v) == -1 && v == 42);
	assert(str_to_int("abc", &v) == -1 && v == 42);
	assert(str_to_int("12abc", &v) == -1 && v == 42);
	assert(str_to_int("1.5", &v) == -1 && v == 42);
	assert(str_to_int("2147483648", &v) == -1 && v == 42);
	assert(str_to_int("99999999999999999999", &v) == -1 && v == 42);
	assert(str_to_int(NULL, &v) == -1 && v == 42);

	printf("✅ str_to_int() accepts numbers and rejects garbage and overflow\n");
}

// str_to_ulongs() reads /proc/stat style fields and stops at the first non-number
static void test_str_to_ulongs(void)
{
	unsigned long v[7] = {0};

	assert(str_to_ulongs(" 10 20\t30 40 50 60 70", v, 7) == 7);
	assert(v[0] == 10 && v[3] == 40 && v[6] == 70);

	// Older kernels have only four fields; count limits the result
	assert(str_to_ulongs("1 2 3 4\n", v, 7) == 4);
	assert(str_to_ulongs("1 2 3 4 5", v, 2) == 2);

	assert(str_to_ulongs("1 x 3", v, 7) == 1);
	assert(str_to_ulongs("-1", v, 7) == 0);
	assert(str_to_ulongs("", v, 7) == 0);

	// "VmRSS:	    1234 kB": the unit after the number ends the list
	assert(str_to_ulongs("	    1234 kB", v, 1) == 1 && v[0] == 1234);

	printf("✅ str_to_ulongs() reads /proc style fields\n");
}

// str_to_doubles() reads /proc/uptime and /proc/net/dev style lines
static void test_str_to_doubles(void)
{
	double v[10] = {0};

	assert(str_to_doubles("12345.67 54321.00\n", v, 2) == 2);
	assert(v[0] > 12345.6 && v[0] < 12345.7 && v[1] > 54320.9);

	assert(str_to_doubles(" 1 2 0 0 0 0 0 0 3 4", v, 10) == 10);
	assert(v[0] == 1 && v[1] == 2 && v[8] == 3 && v[9] == 4);

	assert(str_to_doubles("1 2 x", v, 10) == 2);
	assert(str_to_doubles("", v, 10) == 0);

	printf("✅ str_to_doubles() reads /proc style lines\n");
}

int main(void)
{
	printf("🚀 Starting number conversion tests\n");
	printf("==================================\n");

	test_str_to_int();
	test_str_to_ulongs();
	test_str_to_doubles();

	printf("\n🎉 All number conversion tests passed\n");
	return 0;
}
