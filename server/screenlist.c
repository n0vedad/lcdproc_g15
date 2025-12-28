// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/screenlist.c
 * \brief Screen list management and rotation implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \date 1999-2003
 *
 * \features
 * - Global screen list management
 * - Priority-based screen ordering
 * - Automatic screen rotation
 * - Screen switching with client notification
 * - Screen timeout handling
 * - Current screen tracking
 *
 * \usage
 * - Screen list initialization and cleanup
 * - Screen addition and removal operations
 * - Automatic screen processing and rotation
 * - Manual screen navigation functions
 * - Priority-based screen sorting
 *
 * \details All actions that can be performed on the list of screens.
 * This file also manages the rotation of screens and priority-based
 * scheduling of screen display. Uses linked list for screen storage,
 * sorts screens by priority before processing, handles client notification
 * on screen switches, manages screen timeouts and expiration, and supports
 * manual navigation (next/previous).
 */

#include <stdio.h>
#include <stdlib.h>

#include "shared/LL.h"
#include "shared/report.h"
#include "shared/sockets.h"

#include "client.h"
#include "main.h"
#include "screen.h"
#include "screenlist.h"

int compare_priority(void *one, void *two);

/** \name Global Screen Rotation State
 * Screen list, current screen tracking, and autorotate configuration
 */
///@{
int autorotate = UNSET_INT;		///< Auto-rotation enabled flag (see render.h)
LinkedList *screenlist = NULL;		///< Priority-sorted list of all screens
Screen *current_screen = NULL;		///< Currently displayed screen
long int current_screen_start_time = 0; ///< Frame counter when current screen started
///@}

// Initialize screenlist and prepare screen management
int screenlist_init(void)
{
	report(RPT_DEBUG, "%s()", __FUNCTION__);

	screenlist = LL_new();
	if (!screenlist) {
		report(RPT_ERR, "%s: Error allocating", __FUNCTION__);
		return -1;
	}
	return 0;
}

// Shutdown screenlist and cleanup resources
int screenlist_shutdown(void)
{
	report(RPT_DEBUG, "%s()", __FUNCTION__);

	if (!screenlist) {
		return -1;
	}
	LL_Destroy(screenlist);

	return 0;
}

// Add screen to global screenlist
int screenlist_add(Screen *s)
{
	if (!screenlist)
		return -1;

	return LL_Push(screenlist, s);
}

// Remove screen from global screenlist (switches away if current)
int screenlist_remove(Screen *s)
{
	debug(RPT_DEBUG, "%s(s=[%.40s])", __FUNCTION__, s->id);

	if (!screenlist)
		return -1;

	if (s == current_screen) {
		screenlist_goto_next();
		if (s == current_screen) {
			void *res = LL_Remove(screenlist, s, NEXT);
			screenlist_goto_next();
			return (res == NULL) ? -1 : 0;
		}
	}

	return (LL_Remove(screenlist, s, NEXT) == NULL) ? -1 : 0;
}

// Process screenlist and handle screen switching logic
void screenlist_process(void)
{
	Screen *s;
	Screen *f;

	report(RPT_DEBUG, "%s()", __FUNCTION__);

	if (!screenlist)
		return;

	LL_Sort(screenlist, compare_priority);
	f = LL_GetFirst(screenlist);
	s = screenlist_current();

	// Screen scheduling logic: initialize if no current screen, handle timeout expiration,
	// switch on high priority, and autorotate based on duration
	if (!s) {
		s = f;

		if (!s) {
			return;
		}

		screenlist_switch(s);
		return;

	} else {
		if (s->timeout != -1) {
			--(s->timeout);
			report(RPT_DEBUG, "Active screen [%.40s] has timeout->%d", s->id,
			       s->timeout);

			if (s->timeout <= 0) {
				report(RPT_DEBUG, "Removing expired screen [%.40s]", s->id);
				client_remove_screen(s->client, s);
				screen_destroy(s);
			}
		}
	}

	if (f->priority > s->priority) {
		report(RPT_DEBUG, "%s: High priority screen [%.40s] selected", __FUNCTION__, f->id);
		screenlist_switch(f);
		return;
	}

	if (autorotate && (timer - current_screen_start_time >= s->duration) &&
	    s->priority > PRI_BACKGROUND && s->priority <= PRI_FOREGROUND) {
		screenlist_goto_next();
	}
}

// Switch to another screen with client notification
void screenlist_switch(Screen *s)
{
	Client *c;
	char str[256];

	if (!s)
		return;

	report(RPT_DEBUG, "%s(s=[%.40s])", __FUNCTION__, s->id);

	// Client notification protocol: send "ignore" to previous screen's client, then "listen" to
	// new screen's client for screen activation events
	if (s == current_screen) {
		return;
	}

	if (current_screen) {
		c = current_screen->client;

		if (c) {
			snprintf(str, sizeof(str), "ignore %s\n", current_screen->id);
			sock_send_string(c->sock, str);
		}
	}

	c = s->client;

	if (c) {
		snprintf(str, sizeof(str), "listen %s\n", s->id);
		report(RPT_INFO, "%s: Sending 'listen %s' to client [%d] on socket %d",
		       __FUNCTION__, s->id, c->sock, c->sock);
		sock_send_string(c->sock, str);
		report(RPT_DEBUG, "%s: 'listen %s' message sent successfully", __FUNCTION__, s->id);
	} else {
		report(RPT_DEBUG, "%s: No client for screen [%.40s] - listen message NOT sent",
		       __FUNCTION__, s->id);
	}

	report(RPT_INFO, "%s: switched to screen [%.40s]", __FUNCTION__, s->id);
	current_screen = s;
	current_screen_start_time = timer;
}

// Return currently active screen
Screen *screenlist_current(void) { return current_screen; }

// Move to next screen in rotation order
int screenlist_goto_next(void)
{
	Screen *s;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	// Navigate to next screen in priority-sorted list with wraparound, respecting priority
	// boundaries to stay within same priority tier
	if (!current_screen)
		return -1;

	for (s = LL_GetFirst(screenlist); s && s != current_screen; s = LL_GetNext(screenlist))
		;

	s = LL_GetNext(screenlist);

	if (!s || s->priority < current_screen->priority) {
		s = LL_GetFirst(screenlist);
	}

	screenlist_switch(s);
	return 0;
}

// Move to previous screen in rotation order
int screenlist_goto_prev(void)
{
	Screen *s;

	debug(RPT_DEBUG, "%s()", __FUNCTION__);

	// Navigate to previous screen with wraparound to last screen of same priority tier when
	// reaching list beginning
	if (!current_screen)
		return -1;

	for (s = LL_GetFirst(screenlist); s && s != current_screen; s = LL_GetNext(screenlist))
		;

	s = LL_GetPrev(screenlist);

	if (!s) {
		Screen *f = LL_GetFirst(screenlist);
		Screen *n;

		s = f;
		while ((n = LL_GetNext(screenlist)) && n->priority == f->priority) {
			s = n;
		}
	}

	screenlist_switch(s);
	return 0;
}

/**
 * \brief Compare priority
 * \param one void *one
 * \param two void *two
 * \return Return value
 */
int compare_priority(void *one, void *two)
{
	Screen *a, *b;

	debug(RPT_DEBUG, "compare_priority: %8x %8x", one, two);

	if (!one)
		return 0;
	if (!two)
		return 0;

	a = (Screen *)one;
	b = (Screen *)two;

	debug(RPT_DEBUG, "compare_priority: done?");

	return (b->priority - a->priority);
}
