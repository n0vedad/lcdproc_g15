// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/widget.h
 * \brief Widget management interface for LCDproc server
 * \author William Ferrell
 * \author Selene Scriven
 * \date 1999
 *
 * \features
 * - Widget creation and destruction
 * - Widget type management and conversion
 * - Widget positioning and sizing
 * - Text and data display widgets
 * - Progress bars and scrollers
 * - Icon display support
 * - Frame widgets for containers
 * - Widget search and lookup
 *
 * \usage
 * - Widget structure definitions and enumerations
 * - Function prototypes for widget operations
 * - Widget type conversion utilities
 * - Icon name and number mapping
 * - Subwidget search functionality
 *
 * \details Public interface to the widget methods. Provides functionality
 * for creating, managing, and manipulating widget objects that are used
 * to display content on LCD screens. Supports string widgets for text display,
 * horizontal and vertical bars for progress, icons for graphical elements,
 * scrolling text widgets, title widgets for screen headers, and frame widgets
 * for grouping. Allows creating widgets with specific types and properties,
 * positioning widgets on screen coordinates, updating widget content dynamically,
 * and searching for widgets by ID within screens.
 */

#ifndef WIDGET_H
#define WIDGET_H

/** \brief Include only type definitions from screen.h
 *
 * \details Prevents circular dependencies by including only struct/typedef
 * declarations from screen.h, not full function prototypes.
 */
#define INC_TYPES_ONLY 1
#include "screen.h"
#undef INC_TYPES_ONLY

/**
 * \brief Widget type enumeration
 *
 * \details Defines all available widget types for LCD display. Values correspond
 * to indices in the typenames[] array. WID_NUM represents the total count.
 */
typedef enum WidgetType {
	WID_NONE = 0, ///< No widget type (placeholder)
	WID_STRING,   ///< Text string widget
	WID_HBAR,     ///< Horizontal bar widget
	WID_VBAR,     ///< Vertical bar widget
	WID_PBAR,     ///< Progress bar widget
	WID_ICON,     ///< Icon display widget
	WID_TITLE,    ///< Title text widget
	WID_SCROLLER, ///< Scrolling text widget
	WID_FRAME,    ///< Container frame widget
	WID_NUM	      ///< Total number of widget types
} WidgetType;

/**
 * \brief Widget structure
 * \details Core widget data structure containing all properties
 * and data needed to display widgets on LCD screens
 */
typedef struct Widget {
	char *id;		      // The widget's unique identifier name
	WidgetType type;	      // The widget's type (string, bar, icon, etc.)
	Screen *screen;		      // What screen is this widget in?
	int x, y;		      // Position coordinates on screen
	int width, height;	      // Visible size dimensions
	int left, top, right, bottom; // Bounding rectangle coordinates
	int length;		      // Size or direction parameter
	int speed;		      // Speed setting for scroller widgets
	int promille;		      // For percentage/progress bars (0-1000)
	char *text;		      // Text content or binary data
	char *begin_label;	      // Label in front of progress bars; or NULL
	char *end_label;	      // Label at end of progress bars; or NULL
	struct Screen *frame_screen;  // Frame widgets get an associated screen

} Widget;

/** \brief Maximum direction value for bar widgets
 *
 * \details Maximum value for widget direction parameter used by horizontal/vertical
 * bar widgets to indicate orientation.
 */
#define WID_MAX_DIR 4

/**
 * \brief Creates a new widget
 * \param id Widget identifier string
 * \param type Widget type from WidgetType enum
 * \param screen Parent screen for the widget
 * \retval Widget* Pointer to new widget
 * \retval NULL Widget creation failed
 *
 * \details Creates and initializes a new widget of the specified type.
 */
Widget *widget_create(char *id, WidgetType type, Screen *screen);

/**
 * \brief Destroys a widget
 * \param w Widget to destroy
 *
 * \details Frees all memory associated with the widget and
 * removes it from its parent screen.
 */
void widget_destroy(Widget *w);

/**
 * \brief Converts a widget typename string to a widget type
 * \param typename String name of widget type
 * \retval WidgetType Corresponding widget type enum value
 * \retval WID_NONE Invalid or unknown typename
 *
 * \details Converts string representations like "string", "hbar", etc.
 * to their corresponding WidgetType enum values.
 */
WidgetType widget_typename_to_type(char *typename);

/**
 * \brief Converts a widget type to its typename string
 * \param t Widget type enum value
 * \retval char* String representation of widget type
 * \retval NULL Invalid widget type
 *
 * \details Converts WidgetType enum values to their string
 * representations for display or configuration purposes.
 */
char *widget_type_to_typename(WidgetType t);

/**
 * \brief Searches for subwidgets within a widget
 * \param w Parent widget to search in
 * \param id Widget ID to search for
 * \retval Widget* Found widget
 * \retval NULL Widget not found
 *
 * \details Recursively searches for a widget with the given ID
 * within the subwidgets of the specified parent widget.
 */
Widget *widget_search_subs(Widget *w, char *id);

/**
 * \brief Converts icon number to icon name
 * \param icon Icon number
 * \retval char* Icon name string
 * \retval NULL Invalid icon number
 *
 * \details Converts numeric icon identifiers to their
 * corresponding string names for display purposes.
 */
char *widget_icon_to_iconname(int icon);

/**
 * \brief Converts icon name to icon number
 * \param iconname Icon name string
 * \retval int Icon number
 * \retval -1 Invalid or unknown icon name
 *
 * \details Converts string icon names to their corresponding
 * numeric identifiers for internal processing.
 */
int widget_iconname_to_icon(char *iconname);

#endif
