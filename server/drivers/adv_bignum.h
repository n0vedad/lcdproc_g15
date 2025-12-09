// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/adv_bignum.h
 * \brief Advanced big number display library for LCD drivers
 * \author Stefan Herdler
 * \date 2006-2025
 *
 *
 * \features
 * - Library to generate big numbers on displays with different numbers of custom characters
 * - Optimized big number rendering based on display capabilities and available custom character
 * slots
 * - 2-line displays with 0 to 28+ custom characters support
 * - 4-line displays with 0 to 8+ custom characters support
 * - Automatic optimization based on available resources
 * - Bit pattern macros for custom character creation
 * - Optimized patterns for different display configurations
 * - Support for colon character in time displays
 * - Cell width: 5 pixels (works with 6 but with gaps)
 * - Cell height: 7 or 8 pixels support
 *
 * \usage
 * - Include this header in LCD drivers that need big number display functionality
 * - Use lib_adv_bignum() function to render numbers 0-9 and colon on LCD
 * - Requires driver functions: get_free_chars(), set_char(), chr(), height()
 * - Custom characters placed at offset+0, offset+1, ..., offset+n-1
 * - Set do_init=1 on first call to initialize custom characters
 *
 * \details Library to generate big numbers on displays with different numbers
 * of custom characters. This library provides optimized big number rendering
 * based on display capabilities and available custom character slots.
 */

#ifndef ADV_BIGNUM_H
#define ADV_BIGNUM_H

#include "lcd.h"

/**
 * \name Bit pattern macros for custom character creation
 * \brief Defines bit patterns for creating custom LCD characters
 * \details These macros provide a visual way to define 5x8 pixel patterns
 * for custom characters. Each macro represents a row of pixels where
 * X indicates a lit pixel and _ indicates an unlit pixel.
 * @{
 */
#define b_______ 0x00 ///< Empty row (no pixels)
#define b______X 0x01 ///< Single rightmost pixel
#define b_____X_ 0x02 ///< Single pixel at position 1
#define b_____XX 0x03 ///< Two rightmost pixels
#define b____X__ 0x04 ///< Single pixel at position 2
#define b____X_X 0x05 ///< Pixels at positions 0 and 2
#define b____XX_ 0x06 ///< Two pixels at positions 1-2
#define b____XXX 0x07 ///< Three rightmost pixels
#define b___X___ 0x08 ///< Single pixel at position 3
#define b___X__X 0x09 ///< Pixels at positions 0 and 3
#define b___X_X_ 0x0A ///< Pixels at positions 1 and 3
#define b___X_XX 0x0B ///< Pixels at positions 0-1 and 3
#define b___XX__ 0x0C ///< Two pixels at positions 2-3
#define b___XX_X 0x0D ///< Pixels at positions 0, 2-3
#define b___XXX_ 0x0E ///< Three pixels at positions 1-3
#define b___XXXX 0x0F ///< Four rightmost pixels

#define b__X____ 0x10 ///< Single pixel at position 4
#define b__X___X 0x11 ///< Pixels at positions 0 and 4
#define b__X__X_ 0x12 ///< Pixels at positions 1 and 4
#define b__X__XX 0x13 ///< Pixels at positions 0-1 and 4
#define b__X_X__ 0x14 ///< Pixels at positions 2 and 4
#define b__X_X_X 0x15 ///< Pixels at positions 0, 2, and 4
#define b__X_XX_ 0x16 ///< Pixels at positions 1-2 and 4
#define b__X_XXX 0x17 ///< Pixels at positions 0-2 and 4
#define b__XX___ 0x18 ///< Two pixels at positions 3-4
#define b__XX__X 0x19 ///< Pixels at positions 0, 3-4
#define b__XX_X_ 0x1A ///< Pixels at positions 1, 3-4
#define b__XX_XX 0x1B ///< Pixels at positions 0-1, 3-4
#define b__XXX__ 0x1C ///< Three pixels at positions 2-4
#define b__XXX_X 0x1D ///< Pixels at positions 0, 2-4
#define b__XXXX_ 0x1E ///< Four pixels at positions 1-4
#define b__XXXXX 0x1F ///< All five pixels (full row)
#define b_XXX___ 0x38 ///< Three pixels at positions 3-5 (extended)
#define b_XXXXXX 0x3F ///< Six pixels (extended pattern)

/** @} */

/**
 * \brief Generate big numbers on LCD display
 * \param drvthis Pointer to driver structure
 * \param x Position at which the big number starts (leftmost column)
 * \param num The number to draw (0-9 or 10 for colon ':')
 * \param offset Offset at which custom characters can be placed in CGRAM
 * \param do_init Set to 1 to initialize custom characters, 0 if already set
 *
 * \details This function determines the best possible big number type for the
 * display based on the display's height and number of available custom
 * characters. It automatically selects the most appropriate rendering method
 * and calls the corresponding internal function.
 */
void lib_adv_bignum(Driver *drvthis, int x, int num, int offset, int do_init);

#endif
