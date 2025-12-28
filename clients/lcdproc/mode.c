// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/mode.c
 * \brief Screen mode management and credits display for lcdproc client
 * \author William Ferrell, Selene Scriven
 * \date 1999-2006
 *
 * \features
 * - Screen mode initialization and cleanup
 * - Backlight state management based on screen return values
 * - Credits screen with scrolling contributor list
 * - EyeboxOne integration support
 * - Machine-dependent function wrappers
 * - Adaptive layout for different LCD sizes
 * - Contributor list synchronized with CREDITS file
 *
 * \usage
 * - Called by the main lcdproc client for mode management
 * - Provides initialization and cleanup wrappers
 * - Handles screen updates and backlight control
 * - Displays project credits and contributor information
 * - Integrates with EyeboxOne devices when enabled
 *
 * \details
 * This file implements screen mode management functionality and
 * the credits screen for the lcdproc client. It provides wrappers for
 * machine-dependent initialization and cleanup, as well as screen update
 * coordination with optional EyeboxOne support.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef LCDPROC_EYEBOXONE
#include "eyebox.h"
#endif

#include "shared/sockets.h"

#include "machine.h"
#include "main.h"
#include "mode.h"

// Initialize mode-specific subsystems
int mode_init(void)
{
	machine_init();
	return (0);
}

// Clean up mode subsystems on exit
void mode_close(void) { machine_close(); }

// Update screen display and manage backlight state
int update_screen(ScreenMode *m, int display)
{
	static int status = -1;
	int old_status = status;

	if (m && m->func) {
#ifdef LCDPROC_EYEBOXONE
		int init_flag = (m->flags & INITIALIZED);
#endif
		status = m->func(m->timer, display, &(m->flags));
#ifdef LCDPROC_EYEBOXONE
		if (init_flag == 0)
			eyebox_screen(m->which, 0);
		eyebox_screen(m->which, 1);
#endif
	}

	// Update backlight state only when it changes
	if (status != old_status) {
		if (status == BACKLIGHT_OFF)
			sock_send_string(sock, "backlight off\n");
		if (status == BACKLIGHT_ON)
			sock_send_string(sock, "backlight on\n");
		if (status == BLINK_ON)
			sock_send_string(sock, "backlight blink\n");
	}

	return (status);
}

// Display credits screen with contributor list
int credit_screen(int rep, int display, int *flags_ptr)
{
	// CREDITS
	const char *contributors[] = {
	    "William Ferrell",	  "Selene Scriven",	  "Gareth Watts",
	    "Lorand Bruhacs",	  "Benjamin Tse",	  "Matthias Prinke",
	    "Richard Rognlie",	  "Tom Wheeley",	  "Bjoern Andersson",
	    "Andrew McMeikan",	  "David Glaude",	  "Todd Porter",
	    "Bjoern Andersson",	  "Jason Dale Woodward",  "Ethan Dicks",
	    "Michael Reinelt",	  "Simon Harrison",	  "Charles Steinkuehler",
	    "Harald Klein",	  "Philip Pokorny",	  "Glen Gray",
	    "David Douthitt",	  "Eddie Sheldrake",	  "Rene Wagner",
	    "Andre Breiler",	  "Joris Robijn",	  "Guillaume Filion",
	    "Chris Debenham",	  "Mark Haemmerling",	  "Robin Adams",
	    "Manuel Stahl",	  "Mike Patnode",	  "Peter Marschall",
	    "Markus Dolze",	  "Volker Boerchers",	  "Lucian Muresan",
	    "Matteo Pillon",	  "Laurent Arnal",	  "Simon Funke",
	    "Matthias Goebl",	  "Stefan Herdler",	  "Bernhard Walle",
	    "Andrew Foss",	  "Anthony J. Mirabella", "Cedric Tessier",
	    "John Sanders",	  "Eric Pooch",		  "Benjamin Wiedmann",
	    "Frank Jepsen",	  "Karsten Festag",	  "Gatewood Green",
	    "Dave Platt",	  "Nicu Pavel",		  "Daryl Fonseca-Holt",
	    "Thien Vu",		  "Thomas Jarosch",	  "Christian Jodar",
	    "Mariusz Bialonczyk", "Jack Cleaver",	  "Aron Parsons",
	    "Malte Poeggel",	  "Dean Harding",	  "Christian Leuschen",
	    "Jonathan Kyler",	  "Sam Bingner",	  NULL};

	int contr_num = 0;
	int i;

	if ((*flags_ptr & INITIALIZED) == 0) {
		*flags_ptr |= INITIALIZED;

		for (contr_num = 0; contributors[contr_num] != NULL; contr_num++)
			;

		sock_send_string(sock, "screen_add A\n");
		sock_send_string(sock, "screen_set A -name {Credits for LCDproc}\n");
		sock_send_string(sock, "widget_add A title title\n");
		sock_printf(sock, "widget_set A title {LCDPROC %s}\n", version);

		// Descriptive text scroller for tall displays
		if (lcd_hgt >= 4) {
			sock_send_string(sock, "widget_add A text scroller\n");
			sock_printf(sock, "widget_set A text 1 2 %d 2 h 8 {%s}\n", lcd_wid,
				    "LCDproc was brought to you by:");
		}

		sock_send_string(sock, "widget_add A f frame\n");

		// Frame from (line 3 or 2, left) to (last line, right)
		// Scroll rate: 1 line every X ticks (8 for tall, 12 for short displays)
		sock_printf(sock, "widget_set A f 1 %i %i %i %i %i v %i\n",
			    ((lcd_hgt >= 4) ? 3 : 2), lcd_wid, lcd_hgt, lcd_wid, contr_num,
			    ((lcd_hgt >= 4) ? 8 : 12));

		for (i = 1; i < contr_num; i++) {
			sock_printf(sock, "widget_add A c%i string -in f\n", i);
			sock_printf(sock, "widget_set A c%i 1 %i {%s}\n", i, i, contributors[i]);
		}
	}

	return 0;
}
