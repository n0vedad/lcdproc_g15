// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/mem.h
 * \brief Memory usage and process monitoring screen functions for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - **mem_screen()**: Memory usage overview (RAM and swap utilization)
 * - **mem_top_screen()**: Process memory usage ranking (top memory consumers)
 * - Memory and swap usage display with progress bars
 * - Adaptive layout for different LCD sizes
 * - Process memory usage ranking
 * - Real-time memory statistics monitoring
 * - Memory values formatted in human-readable units
 * - Scrolling display for process lists
 * - Title alternation between hostname and memory info
 *
 * \usage
 * - Include this header in lcdproc client source files
 * - Call mem_screen() to display memory and swap usage
 * - Call mem_top_screen() to display top memory-consuming processes
 * - Used by the main lcdproc screen rotation system
 *
 * \details
 * Header file for memory-related monitoring screens in the lcdproc
 * client. Provides function declarations for memory usage displays and process
 * memory ranking screens.
 */

#ifndef MEM_H
#define MEM_H

/**
 * \brief Display memory and swap usage information screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Always returns success
 *
 * \features
 * - Memory and swap progress bars
 * - Human-readable memory units
 * - Alternating title display
 * - Adaptive layout scaling
 *
 * \details Displays memory and swap usage information with progress bars
 * and usage statistics. Supports both detailed and compact layouts based
 * on LCD dimensions. Shows total, free, and used memory with visual
 * progress indicators.
 *
 * **Display layouts:**
 *
 * **4+ line LCD (detailed view):**
 * \verbatim
 * +--------------------+
 * |## MEM #### SWAP #@|
 * | 758.3M Totl 1.884G |
 * | 490.8M Free 1.882G |
 * |E---    F  E       F|
 * +--------------------+
 * \endverbatim
 *
 * **2-3 line LCD (compact view):**
 * \verbatim
 * +--------------------+
 * |M 758.3M [- ] 35.3%@|
 * |S 1.884G [  ]  0.1% |
 * +--------------------+
 * \endverbatim
 */
int mem_screen(int rep, int display, int *flags_ptr);

/**
 * \brief Display top memory-consuming processes screen.
 * \param rep Time since last screen update (in tenths of seconds)
 * \param display Flag indicating if screen should be updated (1=update, 0=skip)
 * \param flags_ptr Pointer to mode flags for screen state tracking
 * \retval 0 Success
 * \retval -1 Error (memory allocation failure)
 *
 * \features
 * - Process memory usage ranking
 * - Memory values in human-readable format
 * - Process instance counting
 * - Scrolling display for many processes
 * - Hostname integration in title
 * - Adaptive line count based on LCD height
 *
 * \details Displays a ranked list of processes sorted by memory usage.
 * Shows process names, memory consumption, and instance counts. Supports
 * scrolling for long process lists and adapts to different LCD sizes.
 *
 * **Display layout:**
 * \verbatim
 * +--------------------+
 * |## TOP MEM: myhos #@|
 * |1 110.4M mysqld     |
 * |2 35.38M konqueror(2|
 * |3 29.21M XFree86    |
 * +--------------------+
 * \endverbatim
 */
int mem_top_screen(int rep, int display, int *flags_ptr);

#endif
