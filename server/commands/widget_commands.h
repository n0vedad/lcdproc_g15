// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/commands/widget_commands.h
 * \brief Widget command handlers for LCDd server
 * \author William Ferrell, Selene Scriven, Joris Robijn
 * \date 1999-2025
 *
 * \features
 * - **widget_add_func()**: Create widgets and add them to screens with optional container support
 * - **widget_del_func()**: Remove widgets and clean up associated resources
 * - **widget_set_func()**: Configure widget properties, position, size, and content
 * - Support for 8+ widget types (string, hbar, vbar, pbar, icon, title, scroller, frame, num)
 * - Optional container placement using "-in" flag for frame widgets
 * - Widget-specific configuration parameters and validation
 * - Coordinate-based positioning with 1-based coordinate system
 * - Progress bar support with 0-1000 promille value range
 * - Icon name to icon mapping for status indicators
 * - Scrolling text with direction and speed control
 * - Frame widgets for hierarchical widget containers
 * - Memory management for widget text content and labels
 *
 * \usage
 * - Include this header in LCDd server command processing modules
 * - Functions are called by the command parser for widget-related commands
 * - Used for LCD display widget management in LCDd server
 * - Provides client widget lifecycle management functionality
 * - Used by protocol parser to handle widget protocol commands
 *
 * \details
 * Function declarations for client commands concerning widget
 * management within the LCDd server. These functions handle widget
 * creation, deletion, and configuration operations.
 */

#ifndef COMMANDS_WIDGET_H
#define COMMANDS_WIDGET_H

#include "client.h"

/**
 * \brief Add a widget to a screen
 * \param c Client making the request
 * \param argc Number of command arguments
 * \param argv Command argument array
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Creates and adds a new widget to the specified screen. Supports
 * various widget types and optional container placement using the "-in" flag.
 * The widget is created without initial content - use widget_set to configure
 * the widget's properties and content.
 */
int widget_add_func(Client *c, int argc, char **argv);

/**
 * \brief Delete a widget from a screen
 * \param c Client making the request
 * \param argc Number of command arguments
 * \param argv Command argument array
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Removes the specified widget from its screen and deallocates all
 * associated resources. The widget ID becomes available for reuse after deletion.
 * If the widget is a frame widget, all contained widgets are also removed.
 */
int widget_del_func(Client *c, int argc, char **argv);

/**
 * \brief Configure widget properties
 * \param c Client making the request
 * \param argc Number of command arguments
 * \param argv Command argument array containing widget-specific data
 * \retval 0 Success
 * \retval 1 Client not active
 *
 * \details Sets widget-specific properties including position, size, content,
 * speed, and other display characteristics. The exact parameters depend on
 * the widget type. Coordinates are 1-based, progress values range 0-1000.
 */
int widget_set_func(Client *c, int argc, char **argv);

#endif
