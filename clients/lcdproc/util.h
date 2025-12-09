// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/util.h
 * \brief Utility function declarations for numerical formatting and widget management
 * \author Peter Marschall
 * \date 2005-2006
 *
 * \features
 * - **sprintf_memory()**: Memory size formatting with unit conversion
 * - **sprintf_percent()**: Percentage value formatting with precision control
 * - **convert_double()**: Generic unit conversion for various bases
 * - **pbar_widget_add()**: Progress bar widget creation
 * - **pbar_widget_set()**: Progress bar configuration
 * - Automatic unit scaling (B, kB, MB, GB, etc.)
 * - Configurable precision based on value magnitude
 * - Binary (1024) and decimal (1000) base support
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Use sprintf_memory() for human-readable memory formatting
 * - Use sprintf_percent() for consistent percentage display
 * - Use pbar_widget functions for progress bar management
 *
 * \details
 * Header file providing utility function declarations for the lcdproc
 * client. These functions handle standardized formatting of numerical values
 * and widget management.
 */

#ifndef LCDPROC_UTIL_H
#define LCDPROC_UTIL_H

#include <stdio.h>
#include <string.h>

/**
 * \brief Format memory value with appropriate unit suffix.
 * \param dst Destination string buffer (minimum 12 characters)
 * \param value Memory value in bytes
 * \param roundlimit Precision threshold (< 1.0 for limited input precision)
 * \retval dst Pointer to formatted string
 *
 * \details Formats a memory value with the most appropriate unit suffix
 * using binary scaling (1024-based). Automatically adjusts precision
 * based on value magnitude for optimal readability.
 */
char *sprintf_memory(char *dst, double value, double roundlimit);

/**
 * \brief Format percentage value with appropriate precision.
 * \param dst Destination string buffer (minimum 12 characters)
 * \param percent Percentage value (0.0 to 100.0+)
 * \retval dst Pointer to formatted string
 *
 * \details Formats percentage values with edge case handling.
 * Values > 99.9% display as "100%", negative values are clamped to 0%.
 */
char *sprintf_percent(char *dst, double percent);

/**
 * \brief Convert numerical value to appropriate unit scale.
 * \param value Pointer to value to be scaled (modified in-place)
 * \param base Scaling base (1000 for decimal, 1024 for binary units)
 * \param roundlimit Precision threshold for scaling decisions (0.0-1.0)
 * \retval unit Unit prefix string (empty, "k", "M", "G", "T", "P", "E", "Z", "Y")
 *
 * \details Scales numerical values and returns appropriate unit prefixes.
 * Supports both decimal (SI) and binary (IEC) unit systems.
 */
char *convert_double(double *value, int base, double roundlimit);

/**
 * \brief Add progress bar widget.
 * \param screen Name of the screen to add the widget to
 * \param name Name of the widget
 *
 * \details Creates a native progress bar (pbar) widget.
 */
void pbar_widget_add(const char *screen, const char *name);

/**
 * \brief Configure progress bar widget parameters.
 * \param screen Name of the screen containing the widget
 * \param name Name of the widget
 * \param x Horizontal character position (column) of starting point
 * \param y Vertical character position (row) of starting point
 * \param width Widget width in characters (including labels)
 * \param promille Current progress level in promille (0-1000)
 * \param begin_label Optional text at bar beginning (may be NULL)
 * \param end_label Optional text at bar end (may be NULL)
 *
 * \details Sets progress bar parameters including optional begin/end labels.
 */
void pbar_widget_set(const char *screen, const char *name, int x, int y, int width, int promille,
		     char *begin_label, char *end_label);

#endif
