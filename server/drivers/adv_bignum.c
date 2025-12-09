// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/adv_bignum.c
 * \brief Advanced big number display library for LCD drivers
 * \author Stefan Herdler, Peter Marschall
 * \date 2006-2025
 *
 *
 * \features
 * - Implementation of library to generate big numbers on displays with different numbers of custom
 * characters
 * - Optimized big number rendering based on display capabilities and available custom character
 * slots
 * - Cell width: 5 pixels support (also works with 6, but with gaps)
 * - Cell height: 7 or 8 pixels support
 * - Required driver functions integration: get_free_chars(), set_char(), chr(), height()
 * - Custom characters placement at offset+0, offset+1, ..., offset+n-1
 * - 2-line displays: 0, 1, 2-4, 5, 6-27, 28+ custom characters support
 * - 4-line displays: 0, 3-7, 8+ custom characters support
 * - Automatic optimization based on available resources
 * - Multiple big number rendering algorithms for different display configurations
 * - Memory-efficient character pattern storage and management
 *
 * \usage
 * - Used by LCD drivers to implement big number display functionality
 * - Include adv_bignum.h in your driver implementation
 * - Check if custom character mode needs initialization
 * - Call lib_adv_bignum() with appropriate parameters
 * - Library handles character setup and number rendering automatically
 * - Integration in driver num() function for numeric widget display
 *
 * \details Implementation of library to generate big numbers on displays with
 * different numbers of custom characters. This library provides optimized
 * big number rendering based on display capabilities and available custom
 * character slots.
 */

#include "adv_bignum.h"
#include "lcd.h"

// Internal functions for 2- or 3-line displays
static void adv_bignum_num_2_0(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_2_1(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_2_2(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_2_5(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_2_6(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_2_28(Driver *drvthis, int x, int num, int height, int offset,
				int do_init);

// Internal functions for displays with at least 4 lines
static void adv_bignum_num_4_0(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_4_3(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);
static void adv_bignum_num_4_8(Driver *drvthis, int x, int num, int height, int offset,
			       int do_init);

// Internal function to write a big number to the display
static void adv_bignum_write_num(Driver *drvthis, char num_map[][4][3], int x, int num, int height,
				 int offset);

// Draw a big number to the display
void lib_adv_bignum(Driver *drvthis, int x, int num, int offset, int do_init)
{
	int height = drvthis->height(drvthis);
	int customchars = drvthis->get_free_chars(drvthis);

	// 4-line rendering (starts at line 1)
	if (height >= 4) {
		height = 4;

		if (customchars == 0) {
			adv_bignum_num_4_0(drvthis, x, num, height, offset, do_init);
		} else if (customchars < 8) {
			adv_bignum_num_4_3(drvthis, x, num, height, offset, do_init);
		} else {
			adv_bignum_num_4_8(drvthis, x, num, height, offset, do_init);
		}

		// 2-line rendering (also works for 3-line displays)
	} else if (height >= 2) {
		height = 2;

		if (customchars == 0) {
			adv_bignum_num_2_0(drvthis, x, num, height, offset, do_init);
		} else if (customchars == 1) {
			adv_bignum_num_2_1(drvthis, x, num, height, offset, do_init);
		} else if (customchars < 5) {
			adv_bignum_num_2_2(drvthis, x, num, height, offset, do_init);
		} else if (customchars < 6) {
			adv_bignum_num_2_5(drvthis, x, num, height, offset, do_init);
		} else if (customchars < 28) {
			adv_bignum_num_2_6(drvthis, x, num, height, offset, do_init);
		} else {
			adv_bignum_num_2_28(drvthis, x, num, height, offset, do_init);
		}
	}

	return;
}

/**
 * \brief Write big number to display using character map
 * \param drvthis Driver instance
 * \param num_map Character map for number patterns
 * \param x Horizontal position for number display
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height in lines
 * \param offset Custom character offset in CGRAM
 *
 * \details Renders big number by writing characters from num_map to display.
 * Handles colon (num=10) as 1-char wide, regular numbers as 3-char wide.
 * Adds offset to custom character codes (values < ASCII space).
 */
static void adv_bignum_write_num(Driver *drvthis, char num_map[][4][3], int x, int num, int height,
				 int offset)
{
	int y, dx;

	for (y = 0; y < height; y++) {
		if (num == 10) {
			// Colon character is only 1 character wide
			unsigned char c = num_map[num][y][0];

			// Add offset for custom characters (values < ASCII space)
			if (c < ' ')
				c += offset;

			drvthis->chr(drvthis, x, y + 1, c);

		} else {
			// Regular numbers are 3 characters wide
			for (dx = 0; dx < 3; dx++) {
				unsigned char c = num_map[num][y][dx];

				// Add offset for custom characters
				if (c < ' ')
					c += offset;

				drvthis->chr(drvthis, x + dx, y + 1, c);
			}
		}
	}
}

/**
 * \brief Render big number on 2-line display without custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset (unused here)
 * \param do_init Initialize custom characters flag (unused here)
 *
 * \details Uses only ASCII characters to form big numbers. No custom
 * character initialization needed. Quality is lower than custom char versions.
 */
static void adv_bignum_num_2_0(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {" ||", " ||", "   ", "   "},
				       // 1
				       {"  |", "  |", "   ", "   "},
				       // 2
				       {"  ]", " [ ", "   ", "   "},
				       // 3
				       {"  ]", "  ]", "   ", "   "},
				       // 4
				       {" L|", "  |", "   ", "   "},
				       // 5
				       {" [ ", "  ]", "   ", "   "},
				       // 6
				       {" [ ", " []", "   ", "   "},
				       // 7
				       {"  7", "  |", "   ", "   "},
				       // 8
				       {" []", " []", "   ", "   "},
				       // 9
				       {" []", "  ]", "   ", "   "},
				       // colon
				       {".", ".", " ", " "}};

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 2-line display with 1 custom character
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom character if non-zero
 *
 * \details Defines 1 custom character (top bar) to improve number appearance.
 * Better quality than ASCII-only version with minimal CGRAM usage.
 */
static void adv_bignum_num_2_1(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{'|', 0, '|'}, {"|_|"}, {"   "}, {"   "}},
				       // 1
				       {{"  |"}, {"  |"}, {"   "}, {"   "}},
				       // 2
				       {{' ', 0, ']'}, {" [_"}, {"   "}, {"   "}},
				       // 3
				       {{' ', 0, ']'}, {" _]"}, {"   "}, {"   "}},
				       // 4
				       {{" L|"}, {"  |"}, {"   "}, {"   "}},
				       // 5
				       {{' ', '[', 0}, {" _]"}, {"   "}, {"   "}},
				       // 6
				       {{' ', '[', 0}, {" []"}, {"   "}, {"   "}},
				       // 7
				       {{' ', 0, '|'}, {"  |"}, {"   "}, {"   "}},
				       // 8
				       {{" []"}, {" []"}, {"   "}, {"   "}},
				       // 9
				       {{" []"}, {" _]"}, {"   "}, {"   "}},
				       // colon
				       {{"."}, {"."}, {" "}, {" "}}};

	if (do_init) {
		static unsigned char bignum[1][8] = {[0] = {b__XXXXX, b_______, b_______, b_______,
							    b_______, b_______, b_______,
							    b_______}};
		drvthis->set_char(drvthis, offset + 0, bignum[0]);
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 2-line display with 2-4 custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 2 custom characters (top bar, top+bottom bar) for
 * improved number rendering with better segment definition.
 */
static void adv_bignum_num_2_2(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{'|', 0, '|'}, "|_|", "   ", "   "},
				       // 1
				       {"  |", "  |", "   ", "   "},
				       // 2
				       {{' ', 1, '|'}, "|_ ", "   ", "   "},
				       // 3
				       {{' ', 1, '|'}, " _|", "   ", "   "},
				       // 4
				       {"|_|", "  |", "   ", "   "},
				       // 5
				       {{'|', 1, ' '}, " _|", "   ", "   "},
				       // 6
				       {{'|', 0, ' '}, {'|', 1, '|'}, "   ", "   "},
				       // 7
				       {{' ', 0, '|'}, "  |", "   ", "   "},
				       // 8
				       {{'|', 1, '|'}, "|_|", "   ", "   "},
				       // 9
				       {{'|', 1, '|'}, " _|", "   ", "   "},
				       // colon
				       {".", ".", " ", " "}};

	if (do_init) {
		int i;
		static unsigned char bignum[2][8] = {[0] = {b__XXXXX, b_______, b_______, b_______,
							    b_______, b_______, b_______, b_______},
						     [1] = {b__XXXXX, b_______, b_______, b_______,
							    b_______, b_______, b__XXXXX,
							    b__XXXXX}};

		for (i = 0; i < 2; i++) {
			drvthis->set_char(drvthis, offset + i, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 2-line display with 5 custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 5 custom characters for high-quality 2-line numbers.
 * Uses left/right segments and various bar combinations for clean appearance.
 */
static void adv_bignum_num_2_5(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{3, 0, 2}, {3, 1, 2}, {"   "}, {"   "}},
				       // 1
				       {{' ', ' ', 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 2
				       {{' ', 4, 2}, {3, 1, ' '}, {"   "}, {"   "}},
				       // 3
				       {{' ', 4, 2}, {' ', 1, 2}, {"   "}, {"   "}},
				       // 4
				       {{3, 1, 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 5
				       {{3, 4, ' '}, {' ', 1, 2}, {"   "}, {"   "}},
				       // 6
				       {{3, 0, ' '}, {3, 4, 2}, {"   "}, {"   "}},
				       // 7
				       {{' ', 0, 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 8
				       {{3, 4, 2}, {3, 1, 2}, {"   "}, {"   "}},
				       // 9
				       {{3, 4, 2}, {' ', 1, 2}, {"   "}, {"   "}},
				       // colon
				       {{'.'}, {'.'}, {"   "}, {"   "}}};

	if (do_init) {
		int i;
		static unsigned char bignum[5][8] = {[0] = {b__XXXXX, b__XXXXX, b_______, b_______,
							    b_______, b_______, b_______, b_______},
						     [1] = {b_______, b_______, b_______, b_______,
							    b_______, b__XXXXX, b__XXXXX, b__XXXXX},
						     [2] = {b__XXX__, b__XXX__, b__XXX__, b__XXX__,
							    b__XXX__, b__XXX__, b__XXX__, b__XXX__},
						     [3] = {b____XXX, b____XXX, b____XXX, b____XXX,
							    b____XXX, b____XXX, b____XXX, b____XXX},
						     [4] = {b__XXXXX, b__XXXXX, b_______, b_______,
							    b_______, b__XXXXX, b__XXXXX,
							    b__XXXXX}};

		for (i = 0; i < 5; i++) {
			drvthis->set_char(drvthis, offset + i, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 2-line display with 6-27 custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 6 custom characters for optimal 2-line number quality.
 * All segments properly defined for best visual appearance with moderate CGRAM usage.
 */
static void adv_bignum_num_2_6(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{3, 0, 2}, {3, 1, 2}, {"   "}, {"   "}},
				       // 1
				       {{' ', ' ', 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 2
				       {{' ', 5, 2}, {3, 4, ' '}, {"   "}, {"   "}},
				       // 3
				       {{' ', 5, 2}, {' ', 4, 2}, {"   "}, {"   "}},
				       // 4
				       {{3, 1, 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 5
				       {{3, 5, ' '}, {' ', 4, 2}, {"   "}, {"   "}},
				       // 6
				       {{3, 5, ' '}, {3, 4, 2}, {"   "}, {"   "}},
				       // 7
				       {{' ', 0, 2}, {' ', ' ', 2}, {"   "}, {"   "}},
				       // 8
				       {{3, 5, 2}, {3, 4, 2}, {"   "}, {"   "}},
				       // 9
				       {{3, 5, 2}, {' ', 4, 2}, {"   "}, {"   "}},
				       // colon
				       {{'.'}, {'.'}, {"   "}, {"   "}}};

	// One-time initialization: define 6 custom LCD characters with pixel patterns for building
	// large numbers, upload to display's character generator RAM
	if (do_init) {
		int i;
		static unsigned char bignum[6][8] = {[0] = {b__XXXXX, b__XXXXX, b_______, b_______,
							    b_______, b_______, b_______, b_______},
						     [1] = {b_______, b_______, b_______, b_______,
							    b_______, b__XXXXX, b__XXXXX, b__XXXXX},
						     [2] = {b__XXX__, b__XXX__, b__XXX__, b__XXX__,
							    b__XXX__, b__XXX__, b__XXX__, b__XXX__},
						     [3] = {b____XXX, b____XXX, b____XXX, b____XXX,
							    b____XXX, b____XXX, b____XXX, b____XXX},
						     [4] = {b__XXXXX, b_______, b_______, b_______,
							    b_______, b__XXXXX, b__XXXXX, b__XXXXX},
						     [5] = {b__XXXXX, b__XXXXX, b_______, b_______,
							    b_______, b_______, b__XXXXX,
							    b__XXXXX}};

		for (i = 0; i < 6; i++) {
			drvthis->set_char(drvthis, offset + i, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 2-line display with 28+ custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (2 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 28 custom characters for maximum quality 2-line numbers.
 * Each number has unique, highly detailed pixel patterns. Highest quality
 * but requires extensive CGRAM (all 8 slots per number).
 */
static void adv_bignum_num_2_28(Driver *drvthis, int x, int num, int height, int offset,
				int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{15, 6, 2}, {14, 4, 5}, {"   "}, {"   "}},
				       // 1
				       {{' ', 26, ' '}, {' ', 10, ' '}, {"   "}, {"   "}},
				       // 2
				       {{1, 6, 2}, {7, 8, 9}, {"   "}, {"   "}},
				       // 3
				       {{0, 11, 2}, {3, 13, 5}, {"   "}, {"   "}},
				       // 4
				       {{25, 21, 23}, {17, 22, 24}, {"   "}, {"   "}},
				       // 5
				       {{10, 11, 12}, {3, 13, 5}, {"   "}, {"   "}},
				       // 6
				       {{15, 11, 16}, {14, 13, 5}, {"   "}, {"   "}},
				       // 7
				       {{17, 18, 19}, {' ', 20, ' '}, {"   "}, {"   "}},
				       // 8
				       {{15, 11, 2}, {14, 13, 5}, {"   "}, {"   "}},
				       // 9
				       {{15, 11, 2}, {3, 13, 5}, {"   "}, {"   "}},
				       // colon
				       {{27}, {27}, {"   "}, {"   "}}};

	// One-time initialization for 4-line displays: define 28 custom LCD characters with
	// detailed pixel patterns for high-resolution large numbers, upload to display's CGRAM
	if (do_init) {
		int i;
		static unsigned char bignum[28][8] = {
		    [0] = {b_____XX, b____XXX, b____XXX, b_______, b_______, b_______, b_______,
			   b_______},
		    [1] = {b_____XX, b____XXX, b____XXX, b____XXX, b_______, b_______, b_______,
			   b_______},
		    [2] = {b__XX___, b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XXX__,
			   b__XXX__},
		    [3] = {b_______, b_______, b_______, b_______, b____XXX, b____XXX, b_____XX,
			   b_____XX},
		    [4] = {b_______, b_______, b_______, b_______, b__XXXXX, b__XXXXX, b__XXXXX,
			   b__XXXXX},
		    [5] = {b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XX___,
			   b__X____},
		    [6] = {b__XXXXX, b__XXXXX, b__XXXXX, b_______, b_______, b_______, b_______,
			   b_______},
		    [7] = {b_______, b_______, b_______, b______X, b____XXX, b____XXX, b____XXX,
			   b____XXX},
		    [8] = {b____XXX, b___XXXX, b__XXXX_, b__XXX__, b__XXXXX, b__XXXXX, b__XXXXX,
			   b__XXXXX},
		    [9] = {b__X____, b_______, b_______, b_______, b__XXX__, b__XXX__, b__XXX__,
			   b__XXX__},
		    [10] = {b____XXX, b____XXX, b____XXX, b____XXX, b____XXX, b____XXX, b____XXX,
			    b____XXX},
		    [11] = {b__XXXXX, b__XXXXX, b__XXXXX, b_______, b_______, b_______, b__XXXXX,
			    b__XXXXX},
		    [12] = {b__XXX__, b__XXX__, b__XXX__, b_______, b_______, b_______, b_______,
			    b_______},
		    [13] = {b__XXXXX, b_______, b_______, b_______, b__XXXXX, b__XXXXX, b__XXXXX,
			    b__XXXXX},
		    [14] = {b____XXX, b____XXX, b____XXX, b____XXX, b____XXX, b____XXX, b_____XX,
			    b_____XX},
		    [15] = {b_____XX, b____XXX, b____XXX, b____XXX, b____XXX, b____XXX, b____XXX,
			    b____XXX},
		    [16] = {b__XX___, b__XXX__, b__XXX__, b_______, b_______, b_______, b_______,
			    b_______},
		    [17] = {b____XXX, b____XXX, b____XXX, b_______, b_______, b_______, b_______,
			    b_______},
		    [18] = {b__XXXXX, b__XXXXX, b__XXXXX, b_______, b_____XX, b_____XX, b____XXX,
			    b____XXX},
		    [19] = {b__XXX__, b__XXX__, b__XXX__, b__XXX__, b__XX___, b__X____, b_______,
			    b_______},
		    [20] = {b___XXX_, b___XXX_, b__XXXX_, b__XXX__, b__XXX__, b__XXX__, b__XXX__,
			    b__XXX__},
		    [21] = {b______X, b_____XX, b____XXX, b___XXXX, b__XXXXX, b__XXX_X, b__XX__X,
			    b__XX__X},
		    [22] = {b__XXXXX, b__XXXXX, b__XXXXX, b______X, b______X, b______X, b______X,
			    b______X},
		    [23] = {b__X____, b__X____, b__X____, b__X____, b__X____, b__X____, b__X____,
			    b__X____},
		    [24] = {b__XXX__, b__XXX__, b__XXX__, b__X____, b__X____, b__X____, b__X____,
			    b__X____},
		    [25] = {b_______, b_______, b_______, b_______, b_______, b_______, b______X,
			    b______X},
		    [26] = {b____XXX, b____XXX, b___XXXX, b__XXXXX, b____XXX, b____XXX, b____XXX,
			    b____XXX},
		    [27] = {b_______, b_______, b_______, b____XX_, b____XX_, b_______, b_______,
			    b_______}};

		for (i = 0; i < 28; i++) {
			drvthis->set_char(drvthis, offset + i, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 4-line display without custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (4 lines)
 * \param offset Custom character offset (unused here)
 * \param do_init Initialize custom characters flag (unused here)
 *
 * \details Uses only ASCII characters for 4-line tall numbers. Based on
 * lcdm001.c by David GLAUDE. No CGRAM needed but lower visual quality.
 */
static void adv_bignum_num_4_0(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	// Originally from lcdm001.c by David GLAUDE
	static char num_map[][4][3] = {// 0
				       {" _ ", "| |", "|_|", "   "},
				       // 1
				       {"   ", "  |", "  |", "   "},
				       // 2
				       {" _ ", " _|", "|_ ", "   "},
				       // 3
				       {" _ ", " _|", " _|", "   "},
				       // 4
				       {"   ", "|_|", "  |", "   "},
				       // 5
				       {" _ ", "|_ ", " _|", "   "},
				       // 6
				       {" _ ", "|_ ", "|_|", "   "},
				       // 7
				       {" _ ", "  |", "  |", "   "},
				       // 8
				       {" _ ", "|_|", "|_|", "   "},
				       // 9
				       {" _ ", "|_|", " _|", "   "},
				       // colon
				       {" ", ".", ".", " "}};

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 4-line display with 3-7 custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (4 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 3 custom characters uploaded with offset+1 to reserve
 * slot 0 for special use. Provides good 4-line number quality with minimal CGRAM.
 */
static void adv_bignum_num_4_3(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{1, 2, 1}, {1, ' ', 1}, {1, ' ', 1}, {1, 3, 1}},
				       // 1
				       {{' ', ' ', 1}, {' ', ' ', 1}, {' ', ' ', 1}, {' ', ' ', 1}},
				       // 2
				       {{' ', 2, 1}, {' ', 3, 1}, {1, ' ', ' '}, {1, 3, ' '}},
				       // 3
				       {{' ', 2, 1}, {' ', 3, 1}, {' ', ' ', 1}, {' ', 3, 1}},
				       // 4
				       {{1, ' ', 1}, {1, 3, 1}, {' ', ' ', 1}, {' ', ' ', 1}},
				       // 5
				       {{1, 2, ' '}, {1, 3, ' '}, {' ', ' ', 1}, {' ', 3, 1}},
				       // 6
				       {{1, 2, ' '}, {1, 3, ' '}, {1, ' ', 1}, {1, 3, 1}},
				       // 7
				       {{' ', 2, 1}, {' ', ' ', 1}, {' ', ' ', 1}, {' ', ' ', 1}},
				       // 8
				       {{1, 2, 1}, {1, 3, 1}, {1, ' ', 1}, {1, 3, 1}},
				       // 9
				       {{1, 2, 1}, {1, 3, 1}, {' ', ' ', 1}, {' ', 3, 1}},
				       // colon
				       {{" "}, {'.'}, {'.'}, {" "}}};

	// One-time initialization for 3-line displays: define 3 custom LCD characters for minimal
	// big numbers, upload with offset+1 to reserve slot 0 for special use
	if (do_init) {
		int i;
		static unsigned char bignum[3][8] = {[0] = {b__XXXXX, b__XXXXX, b__XXXXX, b_______,
							    b_______, b_______, b_______, b_______},
						     [1] = {b_______, b_______, b_______, b_______,
							    b__XXXXX, b__XXXXX, b__XXXXX, b__XXXXX},
						     [2] = {b___XXX_, b___XXX_, b___XXX_, b___XXX_,
							    b___XXX_, b___XXX_, b___XXX_,
							    b___XXX_}};

		// Upload custom characters with offset by 1
		for (i = 0; i < 3; i++) {
			drvthis->set_char(drvthis, offset + i + 1, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}

/**
 * \brief Render big number on 4-line display with 8+ custom characters
 * \param drvthis Driver instance
 * \param x Horizontal position
 * \param num Number to display (0-9 or 10 for colon)
 * \param height Display height (4 lines)
 * \param offset Custom character offset in CGRAM
 * \param do_init Initialize custom characters if non-zero
 *
 * \details Defines 8 custom characters using all available CGRAM slots
 * for maximum quality 4-line numbers. Best visual appearance for 4-line displays.
 */
static void adv_bignum_num_4_8(Driver *drvthis, int x, int num, int height, int offset, int do_init)
{
	static char num_map[][4][3] = {// 0
				       {{1, 2, 3}, {6, 32, 6}, {6, 32, 6}, {7, 2, 32}},
				       // 1
				       {{7, 6, 32}, {32, 6, 32}, {32, 6, 32}, {7, 2, 32}},
				       // 2
				       {{1, 2, 3}, {32, 5, 0}, {1, 32, 32}, {2, 2, 0}},
				       // 3
				       {{1, 2, 3}, {32, 5, 0}, {3, 32, 6}, {7, 2, 32}},
				       // 4
				       {{32, 3, 6}, {1, 32, 6}, {2, 2, 6}, {32, 32, 0}},
				       // 5
				       {{1, 2, 0}, {2, 2, 3}, {3, 32, 6}, {7, 2, 32}},
				       // 6
				       {{1, 2, 32}, {6, 5, 32}, {6, 32, 6}, {7, 2, 32}},
				       // 7
				       {{2, 2, 6}, {32, 1, 32}, {32, 6, 32}, {32, 0, 32}},
				       // 8
				       {{1, 2, 3}, {4, 5, 0}, {6, 32, 6}, {7, 2, 32}},
				       // 9
				       {{1, 2, 3}, {4, 3, 6}, {32, 1, 32}, {7, 32, 32}},
				       // colon
				       {{32, 32, 32}, {0, 32, 32}, {0, 32, 32}, {32, 32, 32}}};

	// One-time initialization for 2-line displays: define 8 custom LCD characters using all
	// available CGRAM slots for compact big numbers, upload to display
	if (do_init) {
		int i;
		static unsigned char bignum[8][8] = {[0] = {b__XX___, b__XX___, b__XX___, b__XX___,
							    b_______, b_______, b_______, b_______},
						     [1] = {b_____XX, b_____XX, b_____XX, b_____XX,
							    b__XX___, b__XX___, b__XX___, b__XX___},
						     [2] = {b__XX_XX, b__XX_XX, b__XX_XX, b__XX_XX,
							    b_______, b_______, b_______, b_______},
						     [3] = {b_______, b_______, b_______, b_______,
							    b__XX___, b__XX___, b__XX___, b__XX___},
						     [4] = {b__XX___, b__XX___, b__XX___, b__XX___,
							    b_____XX, b_____XX, b_____XX, b_____XX},
						     [5] = {b_______, b_______, b_______, b_______,
							    b__XX_XX, b__XX_XX, b__XX_XX, b__XX_XX},
						     [6] = {b__XX___, b__XX___, b__XX___, b__XX___,
							    b__XX___, b__XX___, b__XX___, b__XX___},
						     [7] = {b_____XX, b_____XX, b_____XX, b_____XX,
							    b_______, b_______, b_______,
							    b_______}};

		for (i = 0; i < 8; i++) {
			drvthis->set_char(drvthis, offset + i, bignum[i]);
		}
	}

	adv_bignum_write_num(drvthis, num_map, x, num, height, offset);
}
