// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/defines.h
 * \brief Common macro definitions for LCDproc components
 * \author LCDproc Development Team
 * \date Various years
 *
 * \features
 * - min() and max() for value comparison
 * - bool, true, false for boolean operations in C
 * - Platform-independent type definitions
 *
 * \usage
 * Include this header in any LCDproc component that needs common macro definitions.
 *
 * \details This header file provides common macro definitions that are used
 * throughout the LCDproc codebase. It includes utility macros for mathematical
 * operations (min/max), boolean type definitions for C compatibility, and other
 * fundamental definitions that ensure consistent behavior across different
 * LCDproc components.
 */

#ifndef SHARED_DEFINES_H
#define SHARED_DEFINES_H

/**
 * \brief Return the minimum of two values
 * \param a First value
 * \param b Second value
 * \retval minimum The smaller of the two values
 *
 * \details Safe macro that evaluates to the smaller of two values.
 * Uses ternary operator to avoid double evaluation side effects.
 */
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * \brief Return the maximum of two values
 * \param a First value
 * \param b Second value
 * \retval maximum The larger of the two values
 *
 * \details Safe macro that evaluates to the larger of two values.
 * Uses ternary operator to avoid double evaluation side effects.
 */
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

/**
 * \brief Boolean type definition for C compatibility
 *
 * \details Defines boolean type and constants for older C standards
 * that don't have built-in boolean support. Uses short for efficiency.
 */
#ifndef bool

/**
 * \brief Boolean type definition
 *
 * \details Uses short integer type for boolean values
 */
#define bool short

/**
 * \brief Boolean true value
 *
 * \details Defines true as integer value 1
 */
#define true 1

/**
 * \brief Boolean false value
 *
 * \details Defines false as integer value 0
 */
#define false 0
#endif

#endif
