// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/screen.h
 * \brief Screen management interface and priority definitions
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 * \features
 * - Screen creation, destruction, and management
 * - Widget list management within screens
 * - Priority system for screen scheduling
 * - Cursor and display property control
 * - Key handling and client association
 * - Conditional compilation support
 *
 * \usage
 * - Screen structure definitions and enumerations
 * - Function prototypes for screen operations
 * - Inline helper functions for widget iteration
 * - Priority conversion utilities
 * - Optional types-only inclusion mode
 *
 * \details Public interface to the screen management methods, providing
 * structure definitions and function prototypes for screen operations.
 * Supports conditional compilation to include only type definitions when needed.
 * If you only need 'struct Screen' to work with, you should use the
 * following code (which does not create an indirect dependency on
 * 'struct Widget'):
 *
 * \code
 * #define INC_TYPES_ONLY 1
 * #include "screen.h"
 * #undef INC_TYPES_ONLY
 * \endcode
 *
 */

// Header guard for type definitions
#ifndef SCREEN_H_TYPES
#define SCREEN_H_TYPES

// Include linked list implementation
#include "shared/LL.h"

// Forward declaration of Client to avoid circular dependency
struct Client;

/**
 * \brief Screen priority levels
 * \details Defines the priority levels for screen scheduling and display ordering.
 * Higher priority screens are displayed more frequently and take precedence.
 */
typedef enum {
	PRI_HIDDEN,	// Screen is hidden from display
	PRI_BACKGROUND, // Background priority (lowest visible)
	PRI_INFO,	// Information display priority
	PRI_FOREGROUND, // Normal foreground priority
	PRI_ALERT,	// Alert priority (high visibility)
	PRI_INPUT	// Input priority (highest, for interactive screens)
} Priority;

/**
 * \brief Screen structure definition
 * \details Represents a screen that can be displayed on the LCD.
 * Contains all properties and widgets associated with the screen.
 */
typedef struct Screen {
	char *id;		// Unique screen identifier
	char *name;		// Human-readable screen name
	int width, height;	// Screen dimensions
	int duration;		// Display duration in deciseconds
	int timeout;		// Screen timeout value
	Priority priority;	// Screen display priority
	short int heartbeat;	// Heartbeat indicator setting
	short int backlight;	// Backlight setting
	short int cursor;	// Cursor type setting
	short int cursor_x;	// Cursor X position
	short int cursor_y;	// Cursor Y position
	char *keys;		// Reserved key list
	int keys_size;		// Size of keys buffer
	LinkedList *widgetlist; // List of widgets on this screen
	struct Client *client;	// Client that owns this screen
} Screen;

/** \brief Default screen duration in deciseconds
 *
 * \details Global default for how long screens are displayed (0 = infinite).
 * Can be overridden per-screen.
 */
extern int default_duration;

/** \brief Default screen priority level
 *
 * \details Global default priority for new screens. See Priority enum.
 */
extern int default_priority;

#endif

/** \brief Conditional inclusion guard for function declarations
 *
 * \details When INC_TYPES_ONLY is defined, only type definitions are included.
 * This prevents circular dependencies during header inclusion.
 */
#ifndef INC_TYPES_ONLY

/** \brief Function declarations section guard
 *
 * \details Ensures screen function prototypes are only included once.
 */
#ifndef SCREEN_H_FNCS
#define SCREEN_H_FNCS

#include <stddef.h>

/**
 * \brief Include widget types for function declarations
 * \details Prevents circular dependencies by including only struct/typedef
 * declarations from widget.h, not full function prototypes.
 */
#define INC_TYPES_ONLY 1
#include "widget.h"
#undef INC_TYPES_ONLY

// Forward declaration of Client for function parameters
typedef struct Client Client;

/**
 * \brief Creates a new screen
 * \param id Unique screen identifier
 * \param client Client that owns the screen
 * \retval Screen* Pointer to new screen
 * \retval NULL Creation failed
 *
 * \details Creates and initializes a new screen with default properties.
 */
Screen *screen_create(char *id, Client *client);

/**
 * \brief Destroys a screen
 * \param s Screen to destroy
 *
 * \details Frees all memory associated with the screen including
 * widgets and internal structures.
 */
void screen_destroy(Screen *s);

/**
 * \brief Add a widget to a screen
 * \param s Target screen
 * \param w Widget to add
 * \retval 0 Success
 * \retval <0 Addition failed
 *
 * \details Adds a widget to the screen's widget list.
 */
int screen_add_widget(Screen *s, Widget *w);

/**
 * \brief Remove a widget from a screen
 * \param s Source screen
 * \param w Widget to remove
 * \retval 0 Success
 * \retval <0 Removal failed
 *
 * \details Removes a widget from the screen's widget list.
 * Does not destroy the widget itself.
 */
int screen_remove_widget(Screen *s, Widget *w);

/**
 * \brief Get first widget from screen
 * \param s Screen to query
 * \retval Widget* First widget
 * \retval NULL No widgets or invalid screen
 *
 * \details Returns the first widget in the screen's widget list.
 */
static inline Widget *screen_getfirst_widget(Screen *s)
{
	return (Widget *)((s != NULL) ? LL_GetFirst(s->widgetlist) : NULL);
}

/**
 * \brief Get next widget from screen
 * \param s Screen to query
 * \retval Widget* Next widget
 * \retval NULL No more widgets or invalid screen
 *
 * \details Returns the next widget in the screen's widget list.
 * Must be called after screen_getfirst_widget().
 */
static inline Widget *screen_getnext_widget(Screen *s)
{
	return (Widget *)((s != NULL) ? LL_GetNext(s->widgetlist) : NULL);
}

/**
 * \brief Find a widget in a screen by ID
 * \param s Screen to search
 * \param id Widget identifier to find
 * \retval Widget* Found widget
 * \retval NULL Widget not found
 *
 * \details Searches the screen's widget list for a widget with the given ID.
 */
Widget *screen_find_widget(Screen *s, char *id);

/**
 * \brief Test if key is reserved by screen
 * \param s Screen to check
 * \param key Key string to test
 * \retval char* Key found in screen's key list
 * \retval NULL Key not reserved by screen
 *
 * \details Checks if the given key is in the screen's reserved key list.
 */
char *screen_find_key(Screen *s, const char *key);

/**
 * \brief Convert priority name to priority value
 * \param pri_name Priority name string
 * \retval Priority Priority enumeration value
 *
 * \details Converts string priority names to Priority enumeration values.
 */
Priority screen_pri_name_to_pri(char *pri_name);

/**
 * \brief Convert priority value to priority name
 * \param pri Priority enumeration value
 * \retval char* Priority name string
 * \retval NULL Invalid priority
 *
 * \details Converts Priority enumeration values to string names.
 */
char *screen_pri_to_pri_name(Priority pri);

#endif

#endif
