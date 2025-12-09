// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/render.c
 * \brief Screen rendering and display output implementation
 * \author William Ferrell
 * \author Selene Scriven
 * \author Joris Robijn
 * \author Peter Marschall
 * \date 1999-2007
 *
 * \features
 * - Screen data generation and formatting
 * - Widget rendering and positioning
 * - Frame handling with recursive support
 * - Display hardware abstraction
 * - Heartbeat and cursor management
 * - Server message display
 *
 * \usage
 * - Supports various widget types (strings, bars, icons, etc.)
 * - Handles screen transitions and animations
 * - Manages display timing and refresh rates
 * - Provides abstraction for different display sizes
 *
 * \details This file contains code that actually generates the full screen data to
 * send to the LCD. render_screen() takes a screen definition and calls
 * render_frame() which in turn builds the screen according to the definition.
 * It may recursively call itself (for nested frames). THIS FILE IS MESSY!
 * Anyone care to rewrite it nicely? Please?? Multiple screen sizes? Multiple
 * simultaneous screens? Horrors of horrors... next thing you know it'll be
 * making coffee... Better believe it'll take a while to do...
 *
 * \todo This needs to be greatly expanded and redone for greater flexibility.
 * \ingroup ToDo_critical
 *
 * The entire rendering system needs a major overhaul for greater flexibility and robustness.
 * This is a core system for display output, so improvements here significantly enhance
 * functionality.
 *
 * Required improvements:
 * - Multiple screen sizes: Support displays of varying dimensions dynamically
 * - More flexible widgets: Widget system needs extensibility for custom widget types
 * - Multiple simultaneous screens: Render different content on multiple displays concurrently
 * - render_string correctness: Review for buffer overflows and text rendering bugs
 * - Better bounds checking: Prevent rendering outside display area
 * - Performance optimizations: Reduce redundant rendering operations
 *
 * Current limitations:
 * - Fixed screen size assumptions throughout code
 * - Widget types are hardcoded and inflexible
 * - Single-display only architecture
 * - render_string modifies w->x permanently (see separate ToDo)
 *
 * \see render_string (line 395: "Review render_string for correctness - w->x is permanently
 * modified")
 *
 * Impact: Display quality, text rendering, multi-display support, extensibility
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/LL.h"
#include "shared/defines.h"
#include "shared/report.h"

#include "client.h"
#include "drivers.h"
#include "render.h"
#include "screen.h"
#include "screenlist.h"
#include "widget.h"

/** \brief Buffer size for string formatting operations */
#define BUFSIZE 1024

/** \name Global Rendering State Variables
 * LCD hardware control and display settings
 */
///@{
int heartbeat = HEARTBEAT_OPEN;		      ///< Heartbeat display mode (see render.h)
static int heartbeat_fallback = HEARTBEAT_ON; ///< Fallback heartbeat mode when client doesn't set
int backlight = BACKLIGHT_OPEN;		      ///< Backlight control mode (see render.h)
static int backlight_fallback = BACKLIGHT_ON; ///< Fallback backlight mode when client doesn't set
int titlespeed = 1;			      ///< Title scroll speed setting
int output_state = 0;			      ///< Hardware output state bitmask
char *server_msg_text;			      ///< Server message text to display
int server_msg_expire = 0;		      ///< Frame count when server message expires
///@}

/**
 * \brief Renders frame containers with nested widgets
 * \param list Widget list to render
 * \param left Left boundary of frame
 * \param top Top boundary of frame
 * \param right Right boundary of frame
 * \param bottom Bottom boundary of frame
 * \param fwid Frame width
 * \param fhgt Frame height
 * \param fscroll Frame scroll direction
 * \param fspeed Frame scroll speed
 * \param timer Current timer value for animations
 *
 * \details Supports recursion and scrolling for nested frame widgets.
 */
static void render_frame(LinkedList *list, int left, int top, int right, int bottom, int fwid,
			 int fhgt, char fscroll, int fspeed, long timer);

/**
 * \brief Render string widget
 * \param w Widget to render
 * \param left Left boundary
 * \param top Top boundary
 * \param right Right boundary
 * \param bottom Bottom boundary
 * \param fy Frame Y offset
 *
 * \details Renders text strings at specified position with frame offset support.
 */
static void render_string(Widget *w, int left, int top, int right, int bottom, int fy);

/**
 * \brief Render horizontal bar widget
 * \param w Widget to render
 * \param left Left boundary
 * \param top Top boundary
 * \param right Right boundary
 * \param bottom Bottom boundary
 * \param fy Frame Y offset
 *
 * \details Renders horizontal progress/status bars.
 */
static void render_hbar(Widget *w, int left, int top, int right, int bottom, int fy);

/**
 * \brief Render scrolling text widget
 * \param w Widget to render
 * \param left Left boundary
 * \param top Top boundary
 * \param right Right boundary
 * \param bottom Bottom boundary
 * \param timer Current timer value for animation
 *
 * \details Renders scrolling text with animation support.
 */
static void render_scroller(Widget *w, int left, int top, int right, int bottom, long timer);

static void render_vbar(Widget *w, int left, int top, int right, int bottom);
static void render_pbar(Widget *w, int left, int top, int right, int bottom);
static void render_title(Widget *w, int left, int top, int right, int bottom, long timer);
static void render_num(Widget *w, int left, int top, int right, int bottom);

// Render complete screen with backlight, heartbeat, and display effects
int render_screen(Screen *s, long timer)
{
	int tmp_state = 0;

	debug(RPT_DEBUG, "%s(screen=[%.40s], timer=%ld)  ==== START RENDERING ====", __FUNCTION__,
	      s->id, timer);

	if (s == NULL)
		return -1;

	drivers_clear();

	// Determine backlight priority: server > client > screen > fallback
	if (backlight != BACKLIGHT_OPEN) {
		tmp_state = backlight;
	} else if ((s->client != NULL) && (s->client->backlight != BACKLIGHT_OPEN)) {
		tmp_state = s->client->backlight;
	} else if (s->backlight != BACKLIGHT_OPEN) {
		tmp_state = s->backlight;
	} else {
		tmp_state = backlight_fallback;
	}

	// Apply backlight effect based on mode
	if (tmp_state & BACKLIGHT_FLASH) {
		drivers_backlight(((tmp_state & BACKLIGHT_ON) ^ ((timer & 7) == 7))
				      ? BACKLIGHT_ON
				      : BACKLIGHT_OFF);
	} else if (tmp_state & BACKLIGHT_BLINK) {
		drivers_backlight(((tmp_state & BACKLIGHT_ON) ^ ((timer & 14) == 14))
				      ? BACKLIGHT_ON
				      : BACKLIGHT_OFF);
	} else {
		drivers_backlight(tmp_state & BACKLIGHT_ON);
	}

	drivers_output(output_state);

	render_frame(s->widgetlist, 0, 0, display_props->width, display_props->height, s->width,
		     s->height, 'v', max(s->duration / s->height, 1), timer);

	drivers_cursor(s->cursor_x, s->cursor_y, s->cursor);

	// Determine heartbeat priority: server > client > screen > fallback
	if (heartbeat != HEARTBEAT_OPEN) {
		tmp_state = heartbeat;
	} else if ((s->client != NULL) && (s->client->heartbeat != HEARTBEAT_OPEN)) {
		tmp_state = s->client->heartbeat;
	} else if (s->heartbeat != HEARTBEAT_OPEN) {
		tmp_state = s->heartbeat;
	} else {
		tmp_state = heartbeat_fallback;
	}

	drivers_heartbeat(tmp_state);

	// Display server message if not expired
	if (server_msg_expire > 0) {
		drivers_string(display_props->width - strlen(server_msg_text) + 1,
			       display_props->height, server_msg_text);
		server_msg_expire--;
		if (server_msg_expire == 0) {
			free(server_msg_text);
		}
	}

	drivers_flush();

	debug(RPT_DEBUG, "==== END RENDERING ====");

	return 0;
}

// Render frame container with nested widgets (supports recursion and scrolling)
static void render_frame(LinkedList *list, int left, int top, int right, int bottom, int fwid,
			 int fhgt, char fscroll, int fspeed, long timer)
{
	int fy = 0;

	debug(RPT_DEBUG,
	      "%s(list=%p, left=%d, top=%d, "
	      "right=%d, bottom=%d, fwid=%d, fhgt=%d, "
	      "fscroll='%c', fspeed=%d, timer=%ld)",
	      __FUNCTION__, list, left, top, right, bottom, fwid, fhgt, fscroll, fspeed, timer);

	if ((list == NULL) || (fhgt <= 0))
		return;

	// Calculate vertical scroll offset if enabled
	if (fscroll == 'v') {
		if ((fspeed != 0) && (fhgt > bottom - top)) {
			int fy_max = fhgt - (bottom - top) + 1;

			fy = (fspeed > 0) ? (timer / fspeed) % fy_max : (-fspeed * timer) % fy_max;
			fy = max(fy, 0);

			debug(RPT_DEBUG, "%s: fy=%d", __FUNCTION__, fy);
		}

	} else if (fscroll == 'h') {

		/**
		 * \todo Frames don't scroll horizontally yet!
		 *
		 * Horizontal scrolling in frames is not implemented. Only vertical scrolling
		 * (fscroll == 'v') is currently supported with frame offset calculation (fy).
		 *
		 * Required implementation:
		 * - Add horizontal offset variable (fx) similar to vertical fy
		 * - Calculate fx from frame left/width and widget position
		 * - Pass fx to render functions for horizontal clipping
		 * - Update all widget render functions to handle horizontal offset
		 *
		 * Impact: Feature completeness, wide content display in frames
		 *
		 * \ingroup ToDo_medium
		 */
	}

	LL_Rewind(list);

	// Iterate through all widgets and render each by type
	do {
		Widget *w = (Widget *)LL_Get(list);

		if (w == NULL)
			return;

		switch (w->type) {

		// Text string widget
		case WID_STRING:
			render_string(w, left, top - fy, right, bottom, fy);
			break;

		// Horizontal bar widget
		case WID_HBAR:
			render_hbar(w, left, top - fy, right, bottom, fy);
			break;

		// Vertical bar widget
		case WID_VBAR:
			/**
			 * \todo Vbars don't work in frames!
			 *
			 * Vertical bars have incomplete frame support, causing rendering issues
			 * when placed inside frames. Frame offset (fy) is not properly applied to
			 * vbar rendering.
			 *
			 * Required fixes:
			 * - Apply frame vertical offset (fy) to vbar position calculations
			 * - Handle clipping when vbar extends beyond frame boundaries
			 * - Test with different vbar heights and frame scroll positions
			 *
			 * Impact: Widget functionality, frame completeness, display correctness
			 *
			 * \ingroup ToDo_medium
			 */
			render_vbar(w, left, top, right, bottom);
			break;

		// Progress bar widget
		case WID_PBAR:
			render_pbar(w, left, top - fy, right, bottom);
			break;

		// Icon widget
		case WID_ICON:
			drivers_icon(w->x, w->y, w->length);
			break;

		// Title widget with scrolling
		case WID_TITLE:
			render_title(w, left, top, right, bottom, timer);
			break;

		// Scroller widget with multiple modes
		case WID_SCROLLER:
			/**
			 * \todo Scrollers don't work in frames!
			 *
			 * Scroller widgets have incomplete frame support, causing rendering issues
			 * when placed inside frames. Frame offset (fy) is not properly applied to
			 * scroller rendering.
			 *
			 * Required fixes:
			 * - Apply frame vertical offset (fy) to scroller position calculations
			 * - Handle clipping when scroller text extends beyond frame boundaries
			 * - Support frame-relative scrolling for both horizontal and vertical
			 * scrollers
			 * - Test with different scroller directions and frame scroll positions
			 *
			 * Impact: Widget functionality, frame completeness, text display
			 * correctness
			 *
			 * \ingroup ToDo_medium
			 */
			render_scroller(w, left, top, right, bottom, timer);
			break;

		// Nested frame widget (recursive rendering)
		case WID_FRAME: {
			int new_left = left + w->left - 1;
			int new_top = top + w->top - 1;
			int new_right = min(left + w->right, right);
			int new_bottom = min(top + w->bottom, bottom);

			if ((new_left < right) && (new_top < bottom)) {
				render_frame(w->frame_screen->widgetlist, new_left, new_top,
					     new_right, new_bottom, w->width, w->height, w->length,
					     w->speed, timer);
			}
		} break;

		// Numeric display widget (digits 0-9 or colon)
		case WID_NUM:
			if ((w->x > 0) && (w->y >= 0) && (w->y <= 10)) {
				drivers_num(w->x + left, w->y);
			}
			break;

		// Empty or unknown widget type
		case WID_NONE:
		default:
			break;
		}

	} while (LL_Next(list) == 0);
}

// Render text string widget at specified position
static void render_string(Widget *w, int left, int top, int right, int bottom, int fy)
{
	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d, fy=%d)", __FUNCTION__, w,
	      left, top, right, bottom, fy);

	/**
	 * \todo Review render_string for correctness - w->x is permanently modified
	 *
	 * The render_string() function modifies w->x permanently during rendering, which can cause
	 * unexpected behavior on subsequent renders. The widget's x position should not be changed
	 * by rendering functions.
	 *
	 * Problem: w->x is modified during text rendering and alignment calculations
	 * Effect: Widget position changes persist between render cycles
	 * Correct: Use local variable for position calculations, leave w->x unchanged
	 *
	 * Additional review needed:
	 * - Check for buffer overflows in string copying
	 * - Verify text alignment calculations (left/center/right)
	 * - Test with various string lengths and display widths
	 * - Validate Unicode/special character handling
	 *
	 * Impact: Widget position stability, rendering correctness, text display bugs
	 *
	 * \ingroup ToDo_medium
	 */

	if ((w->text != NULL) && (w->x > 0) && (w->y > 0) && (w->y > fy) &&
	    (w->y <= bottom - top)) {
		w->x = min(w->x, right - left);
		drivers_string(w->x + left, w->y + top, w->text);
	}
}

// Render horizontal bar widget with proportional length
static void render_hbar(Widget *w, int left, int top, int right, int bottom, int fy)
{
	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d, fy=%d)", __FUNCTION__, w,
	      left, top, right, bottom, fy);

	if (!((w->x > 0) && (w->y > 0) && (w->y > fy) && (w->y <= bottom - top)))
		return;

	if (w->length > 0) {
		int len = display_props->width - w->x - left + 1;
		int promille = 1000;

		if ((w->length / display_props->cellwidth) < right - left - w->x + 1) {
			len = w->length / display_props->cellwidth +
			      (w->length % display_props->cellwidth ? 1 : 0);
			promille = (long)1000 * w->length / (display_props->cellwidth * len);
		}

		drivers_hbar(w->x + left, w->y + top, len, promille, BAR_PATTERN_FILLED);

		/**
		 * \todo Left-extending horizontal bars not yet implemented
		 *
		 * Horizontal bars that extend to the left (instead of right) are not implemented.
		 * This affects layout options for progress bars and level indicators that should
		 * grow from right to left.
		 *
		 * Current: Only right-extending bars (w->length > 0) are supported
		 * Missing: Left-extending bars (w->length < 0) have empty implementation
		 *
		 * Use cases:
		 * - Right-aligned progress bars
		 * - Level meters that fill right-to-left
		 * - Bidirectional indicators
		 *
		 * Impact: Widget layout flexibility, bar widget completeness
		 *
		 * \ingroup ToDo_critical
		 */

	} else if (w->length < 0) {
	}
}

/**
 * \brief Render vertical bar widget
 * \param w Widget to render
 * \param left Left boundary offset
 * \param top Top boundary offset
 * \param right Right boundary offset
 * \param bottom Bottom boundary offset
 *
 * \details Draws vertical bar filling proportional height based on widget length value.
 */
static void render_vbar(Widget *w, int left, int top, int right, int bottom)
{
	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d)", __FUNCTION__, w, left,
	      top, right, bottom);

	if (!((w->x > 0) && (w->y > 0)))
		return;

	if (w->length > 0) {
		int full_len = display_props->height;
		int promille = (long)1000 * w->length / (display_props->cellheight * full_len);

		drivers_vbar(w->x + left, w->y + top, full_len, promille, BAR_PATTERN_FILLED);

	} else if (w->length < 0) {
	}
}

/**
 * \brief Render progress bar widget
 * \param w Widget to render
 * \param left Left boundary offset
 * \param top Top boundary offset
 * \param right Right boundary offset
 * \param bottom Bottom boundary offset
 *
 * \details Draws horizontal progress bar with optional begin/end labels.
 */
static void render_pbar(Widget *w, int left, int top, int right, int bottom)
{
	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d)", __FUNCTION__, w, left,
	      top, right, bottom);

	if (!((w->x > 0) && (w->y > 0) && (w->width > 0)))
		return;

	drivers_pbar(w->x + left, w->y + top, w->width, w->promille, w->begin_label, w->end_label);
}

/**
 * \brief Render title widget with scrolling text
 * \param w Widget to render
 * \param left Left boundary offset
 * \param top Top boundary offset
 * \param right Right boundary offset
 * \param bottom Bottom boundary offset
 * \param timer Current timer value for scroll animation
 *
 * \details Displays title between block icons with horizontal scrolling if text too long.
 */
static void render_title(Widget *w, int left, int top, int right, int bottom, long timer)
{
	int vis_width = right - left;
	char str[BUFSIZE];
	int x, width = vis_width - 6, length, delay;

	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d, timer=%ld)", __FUNCTION__,
	      w, left, top, right, bottom, timer);

	if ((w->text == NULL) || (vis_width < 8))
		return;

	length = strlen(w->text);

	delay = (titlespeed <= TITLESPEED_NO) ? TITLESPEED_NO
					      : max(TITLESPEED_MIN, TITLESPEED_MAX - titlespeed);

	drivers_icon(w->x + left, w->y + top, ICON_BLOCK_FILLED);
	drivers_icon(w->x + left + 1, w->y + top, ICON_BLOCK_FILLED);

	length = min(length, sizeof(str) - 1);

	// Text scrolling logic: static display for short text or no delay, bidirectional scrolling
	// animation with configurable delay for long text
	if ((length <= width) || (delay == 0)) {
		length = min(length, width);
		strncpy(str, w->text, length);
		str[length] = '\0';
		x = length + 4;

	} else {
		int offset = timer;
		int reverse;

		if ((delay != 0) && (delay < length / (length - width)))
			offset /= delay;

		reverse = (offset / length) & 1;

		offset %= length;
		offset = max(offset, 0);

		if ((delay != 0) && (delay >= length / (length - width)))
			offset /= delay;

		offset = min(offset, length - width);

		if (reverse)
			offset = (length - width) - offset;

		length = min(width, sizeof(str) - 1);
		strncpy(str, w->text + offset, length);
		str[length] = '\0';

		x = vis_width - 2;
	}

	drivers_string(w->x + 3 + left, w->y + top, str);

	for (; x < vis_width; x++) {
		drivers_icon(w->x + x + left, w->y + top, ICON_BLOCK_FILLED);
	}
}

// Render scroller widget with three modes (marquee, horizontal, vertical)
static void render_scroller(Widget *w, int left, int top, int right, int bottom, long timer)
{
	char str[BUFSIZE];
	int length;
	int offset, gap;
	int screen_width;
	int necessaryTimeUnits = 0;

	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d, timer=%ld)", __FUNCTION__,
	      w, left, top, right, bottom, timer);

	if ((w->text == NULL) || (w->right < w->left))
		return;

	screen_width = abs(w->right - w->left + 1);
	screen_width = min(screen_width, sizeof(str) - 1);

	switch (w->length) {

	// Marquee mode: continuous horizontal scrolling with gap
	case 'm':
		length = strlen(w->text);

		// Multi-line text rendering with three modes: static if fits in width, line-wrapped
		// if fits in height, or bidirectional vertical scrolling with speed control
		if (length <= screen_width) {
			drivers_string(w->left, w->top, w->text);
			break;
		}

		gap = screen_width / 2;
		length += gap;

		if (w->speed > 0) {
			necessaryTimeUnits = length * w->speed;
			offset = (timer % necessaryTimeUnits) / w->speed;
		} else if (w->speed < 0) {
			necessaryTimeUnits = length / (w->speed * -1);
			offset = (timer % necessaryTimeUnits) * w->speed * -1;
		} else {
			offset = 0;
		}

		if (offset <= length) {
			if (gap > offset) {
				memset(str, ' ', gap - offset);
				strncpy(&str[gap - offset], w->text, screen_width);
			} else {
				int room = screen_width - (length - offset);

				strncpy(str, &w->text[offset - gap], screen_width);
				if (room > 0) {
					memset(&str[length - offset], ' ', min(room, gap));
					room -= gap;
					if (room > 0)
						strncpy(&str[length - offset + gap], w->text, room);
				}
			}
			str[screen_width] = '\0';
			drivers_string(w->left, w->top, str);
		}
		break;

	// Horizontal mode: oscillating back-and-forth scrolling
	case 'h':
		length = strlen(w->text) + 1;

		if (length <= screen_width) {
			drivers_string(w->left, w->top, w->text);

		} else {
			int effLength = length - screen_width;

			if (w->speed > 0) {
				necessaryTimeUnits = effLength * w->speed;
				if (((timer / necessaryTimeUnits) % 2) == 0) {
					offset = (timer % (effLength * w->speed)) / w->speed;

				} else {
					offset = (((timer % (effLength * w->speed)) -
						   (effLength * w->speed) + 1) /
						  w->speed) *
						 -1;
				}

			} else if (w->speed < 0) {
				necessaryTimeUnits = effLength / (w->speed * -1);
				if (((timer / necessaryTimeUnits) % 2) == 0) {
					offset =
					    (timer % (effLength / (w->speed * -1))) * w->speed * -1;
				} else {
					offset = (((timer % (effLength / (w->speed * -1))) *
						   w->speed * -1) -
						  effLength + 1) *
						 -1;
				}

			} else {
				offset = 0;
			}

			if (offset <= length) {
				strncpy(str, &((w->text)[offset]), screen_width);
				str[screen_width] = '\0';
				drivers_string(w->left, w->top, str);

				debug(RPT_DEBUG, "scroller %s : %d", str, length - offset);
			}
		}
		break;

	// Vertical mode: multi-line oscillating with line wrapping
	case 'v':
		/**
		 * \todo Vertical scrollers sometimes jump to top instead of scrolling back up
		 *
		 * Vertical scrollers have a bug where they jump to the top instead of smoothly
		 * scrolling back up after reaching the bottom. This affects user experience,
		 * especially with long multi-line text content.
		 *
		 * Expected behavior: Smooth scroll down, then smooth scroll back up (oscillating)
		 * Actual behavior: Scroll down correctly, but jump to top instead of scrolling up
		 *
		 * Problem likely in:
		 * - Direction reversal logic in vertical scroller code
		 * - Scroll position calculation when direction changes
		 * - Timer/speed handling during upward scroll
		 *
		 * Impact: UI behavior, user experience, text display smoothness
		 *
		 * \ingroup ToDo_medium
		 */
		length = strlen(w->text);

		// Multi-line text rendering with three modes: static if fits in width, line-wrapped
		// if fits in height, or bidirectional vertical scrolling with speed control
		if (length <= screen_width) {
			drivers_string(w->left, w->top, w->text);

		} else {
			int lines_required =
			    (length / screen_width) + (length % screen_width ? 1 : 0);
			int available_lines = (w->bottom - w->top + 1);

			if (lines_required <= available_lines) {
				int i;

				for (i = 0; i < lines_required; i++) {
					strncpy(str, &((w->text)[i * screen_width]), screen_width);
					str[screen_width] = '\0';
					drivers_string(w->left, w->top + i, str);
				}

			} else {
				int effLines = lines_required - available_lines + 1;
				int begin = 0;
				int i = 0;

				debug(
				    RPT_DEBUG,
				    "length: %d sw: %d lines req: %d  avail lines: %d effLines: %d",
				    length, screen_width, lines_required, available_lines,
				    effLines);

				if (w->speed > 0) {
					necessaryTimeUnits = effLines * w->speed;
					if (((timer / necessaryTimeUnits) % 2) == 0) {
						debug(RPT_DEBUG, "up ");
						begin = (timer % (necessaryTimeUnits)) / w->speed;
					} else {
						debug(RPT_DEBUG, "down ");
						begin = (((timer % necessaryTimeUnits) -
							  necessaryTimeUnits + 1) /
							 w->speed) *
							-1;
					}

				} else if (w->speed < 0) {
					necessaryTimeUnits = effLines / (w->speed * -1);
					if (((timer / necessaryTimeUnits) % 2) == 0) {
						begin =
						    (timer % necessaryTimeUnits) * w->speed * -1;
					} else {
						begin = (((timer % necessaryTimeUnits) * w->speed *
							  -1) -
							 effLines + 1) *
							-1;
					}

				} else {
					begin = 0;
				}

				debug(RPT_DEBUG, "rendering begin: %d  timer: %d effLines: %d",
				      begin, timer, effLines);

				for (i = begin; i < begin + available_lines; i++) {
					strncpy(str, &((w->text)[i * (screen_width)]),
						screen_width);
					str[screen_width] = '\0';
					debug(RPT_DEBUG, "rendering: '%s' of %s", str, w->text);
					drivers_string(w->left, w->top + (i - begin), str);
				}
			}
		}
		break;

	// Handle invalid scroll modes
	default:
		break;
	}
}

/**
 * \brief Render large numeric digit widget
 * \param w Widget to render
 * \param left Left boundary offset
 * \param top Top boundary offset
 * \param right Right boundary offset
 * \param bottom Bottom boundary offset
 *
 * \details Displays large digit (0-9) or colon using driver's num() function.
 */
static void render_num(Widget *w, int left, int top, int right, int bottom)
{
	debug(RPT_DEBUG, "%s(w=%p, left=%d, top=%d, right=%d, bottom=%d)", __FUNCTION__, w, left,
	      top, right, bottom);

	if ((w->x > 0) && (w->y >= 0) && (w->y <= 10)) {
		drivers_num(w->x + left, w->y);
	}
}

// Display short server message in screen corner
int server_msg(const char *text, int expire)
{
	debug(RPT_DEBUG, "%s(text=\"%.40s\", expire=%d)", __FUNCTION__, text, expire);

	if (strlen(text) > 15 || expire <= 0) {
		return -1;
	}

	if (server_msg_expire > 0) {
		free(server_msg_text);
	}

	size_t msg_size = strlen(text) + 3;
	server_msg_text = malloc(msg_size);
	strncpy(server_msg_text, "| ", msg_size - 1);
	server_msg_text[msg_size - 1] = '\0';
	strncat(server_msg_text, text, msg_size - strlen(server_msg_text) - 1);

	server_msg_expire = expire;

	return 0;
}
