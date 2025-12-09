// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drivers/g15.h
 * \brief Header file for Logitech G15/G510 LCD driver
 * \author Anthony J. Mirabella
 * \author n0vedad
 * \date 2006-2025
 *
 *
 * \features
 * - Header file for LCDproc driver supporting Logitech G-Series keyboards
 * - 160x43 monochrome LCD display support with pixel-level control
 * - USB HID communication through hidraw interface
 * - RGB backlight control for G510 keyboards with zone support
 * - Macro LED control for G510 keyboards (M1, M2, M3, MR)
 * - Big number display support with 32x32 pixel bitmaps
 * - Icon rendering capabilities with predefined icon set
 * - Double buffering with canvas and backing store
 * - Font rendering support through libg15render
 * - Core driver functions: init, close, width, height, clear, flush
 * - Graphics functions: string, chr, icon, hbar, vbar, num
 * - Display control: backlight, RGB backlight, macro LEDs
 * - Key input handling and event processing
 *
 * \usage
 * - Include this header in LCDd server for G15/G510 driver support
 * - Used for Logitech G15 (Original, USB ID: 046d:c222)
 * - Used for Logitech G15 v2 (USB ID: 046d:c227)
 * - Used for Logitech G510 (USB ID: 046d:c22d/046d:c22e)
 * - Used for Logitech G510s keyboards
 * - Driver automatically selected when "g15" is specified in configuration
 * - Requires hidraw interface and libg15render library
 * - Provides complete LCD and LED control functionality
 *
 * \details Header file for the LCDproc driver supporting Logitech G-Series
 * keyboards with 160x43 monochrome LCD displays. Provides support for G15,
 * G15 v2, and G510 keyboards with additional RGB backlight control for G510.
 */

#ifndef G15_H_
#define G15_H_

#include "hidraw_lib.h"
#include "lcd.h"
#include <libg15render.h>

/**
 * \brief Private data structure for the G15 driver
 *
 * \details Contains all the state information needed by the G15 driver
 * including device handles, display buffers, and device capabilities.
 */
typedef struct g15_private_data {
	// HID raw handle for USB communication
	struct lib_hidraw_handle *hidraw_handle;

	// Primary LCD canvas buffer
	g15canvas canvas;

	// Backing store for double buffering
	g15canvas backingstore;

	// Font handle for text rendering
	g15font *font;

	// Current backlight state
	int backlight_state;

	// Device RGB backlight capability
	int has_rgb_backlight;

	// Current red component (0-255)
	unsigned char rgb_red;

	// Current green component (0-255)
	unsigned char rgb_green;

	// Current blue component (0-255)
	unsigned char rgb_blue;

	// RGB control method (1=HID, 0=LED)
	int rgb_method_hid;

	// Macro LED bitmask (M1,M2,M3,MR)
	unsigned char macro_leds;
} PrivateData;

/** \name G15 Display Geometry
 * Display dimensions and layout constants for G15 LCD
 */
///@{
#define G15_OFFSET 32	  ///< Display offset for positioning
#define G15_PX_WIDTH 160  ///< LCD pixel width
#define G15_PX_HEIGHT 43  ///< LCD pixel height
#define G15_CHAR_WIDTH 20 ///< Character display width
#define G15_CHAR_HEIGHT 5 ///< Character display height
#define G15_CELL_WIDTH 8  ///< Character cell width in pixels
#define G15_CELL_HEIGHT 8 ///< Character cell height in pixels
///@}

/** \name G15 USB Communication
 * USB protocol constants for G15 keyboard
 */
///@{
#define G15_LCD_WRITE_CMD 0x03 ///< LCD write command
#define G15_USB_ENDPT 2	       ///< USB endpoint for communication
///@}

/** \name G15 Icon Mappings
 * G15-specific character codes for standard icons
 */
///@{
#define G15_ICON_HEART_FILLED 3	  ///< Filled heart icon
#define G15_ICON_HEART_OPEN 3	  ///< Open heart icon
#define G15_ICON_ARROW_UP 24	  ///< Up arrow icon
#define G15_ICON_ARROW_DOWN 25	  ///< Down arrow icon
#define G15_ICON_ARROW_RIGHT 26	  ///< Right arrow icon
#define G15_ICON_ARROW_LEFT 27	  ///< Left arrow icon
#define G15_ICON_CHECKBOX_ON 7	  ///< Checked checkbox icon
#define G15_ICON_CHECKBOX_OFF 9	  ///< Unchecked checkbox icon
#define G15_ICON_CHECKBOX_GRAY 10 ///< Gray checkbox icon
#define G15_ICON_STOP 254	  ///< Stop icon
#define G15_ICON_PAUSE 186	  ///< Pause icon
#define G15_ICON_PLAY 16	  ///< Play icon
#define G15_ICON_PLAYR 17	  ///< Play reverse icon
#define G15_ICON_FF 175		  ///< Fast forward icon
#define G15_ICON_FR 174		  ///< Fast reverse icon
#define G15_ICON_NEXT 242	  ///< Next track icon
#define G15_ICON_PREV 243	  ///< Previous track icon
#define G15_ICON_REC 7		  ///< Record icon
///@}

/** \name Big Number Display
 * Constants for big number rendering
 */
///@{
#define G15_BIGNUM_LEN 1032 ///< Big number bitmap length in bytes
///@}

/** \name G510 RGB Backlight Control
 * Constants for G510 RGB backlight feature
 */
///@{
#define G510_FEATURE_RGB_ZONE0 0x05 ///< RGB zone 0 feature report
#define G510_FEATURE_RGB_ZONE1 0x06 ///< RGB zone 1 feature report
#define G510_RGB_REPORT_SIZE 4	    ///< RGB report size in bytes
///@}

/** \name G510 Macro LED Control
 * Constants for G510 macro LED feature
 */
///@{
#define G510_FEATURE_MACRO_LEDS 0x04 ///< Macro LED feature report
#define G510_MACRO_LED_REPORT_SIZE 2 ///< Macro LED report size
///@}

/** \name G510 Macro LED Bitmasks
 * Bitmask values for individual macro LEDs
 */
///@{
#define G510_LED_M1 0x80 ///< M1 macro key LED
#define G510_LED_M2 0x40 ///< M2 macro key LED
#define G510_LED_M3 0x20 ///< M3 macro key LED
#define G510_LED_MR 0x10 ///< MR macro record LED
///@}

/** \name External Data
 * External data declarations for G15 driver
 */
///@{
extern short g15_bignum_data[11][G15_BIGNUM_LEN]; ///< Big number bitmap data (digits 0-9 and colon)
///@}

/**
 * \brief Initialize the G15 driver
 * \param drvthis Pointer to driver structure
 * \retval 0 Success
 * \retval -1 Error during initialization
 *
 * \details Initializes the G15 driver by allocating private data,
 * configuring device settings, and establishing USB communication.
 * Detects device capabilities and sets up RGB backlight if supported.
 */
MODULE_EXPORT int g15_init(Driver *drvthis);

/**
 * \brief Close the G15 driver and clean up resources
 * \param drvthis Pointer to driver structure
 *
 * \details Performs cleanup by closing USB connection, freeing fonts,
 * and releasing allocated memory.
 */
MODULE_EXPORT void g15_close(Driver *drvthis);

/**
 * \brief Return the display width in characters
 * \param drvthis Pointer to driver structure
 * \return Display width in characters
 *
 * \details Returns the number of character columns available on the display.
 */
MODULE_EXPORT int g15_width(Driver *drvthis);

/**
 * \brief Return the display height in characters
 * \param drvthis Pointer to driver structure
 * \return Display height in characters
 *
 * \details Returns the number of character rows available on the display.
 */
MODULE_EXPORT int g15_height(Driver *drvthis);

/**
 * \brief Return the width of a character cell in pixels
 * \param drvthis Pointer to driver structure
 * \return Character cell width in pixels
 *
 * \details Returns the pixel width of a single character cell.
 */
MODULE_EXPORT int g15_cellwidth(Driver *drvthis);

/**
 * \brief Return the height of a character cell in pixels
 * \param drvthis Pointer to driver structure
 * \return Character cell height in pixels
 *
 * \details Returns the pixel height of a single character cell.
 */
MODULE_EXPORT int g15_cellheight(Driver *drvthis);

/**
 * \brief Clear the LCD screen
 * \param drvthis Pointer to driver structure
 *
 * \details Clears both the main canvas and backing store buffers.
 */
MODULE_EXPORT void g15_clear(Driver *drvthis);

/**
 * \brief Flush the frame buffer to the LCD display
 * \param drvthis Pointer to driver structure
 *
 * \details Converts the canvas buffer to LCD format and sends it
 * to the device via USB. Updates the display with current content.
 */
MODULE_EXPORT void g15_flush(Driver *drvthis);

/**
 * \brief Print a string on the LCD display at specified position
 * \param drvthis Pointer to driver structure
 * \param x Horizontal starting position (1-20)
 * \param y Vertical position (1-5)
 * \param string String to display
 *
 * \details Prints a string starting at position (x,y). The upper-left
 * corner is (1,1) and the lower-right is (20,5).
 */
MODULE_EXPORT void g15_string(Driver *drvthis, int x, int y, const char string[]);

/**
 * \brief Place a single character on the LCD at specified position
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position (1-20)
 * \param y Vertical position (1-5)
 * \param c Character to display
 *
 * \details Renders a single character at the specified position using
 * the libg15render font system.
 */
MODULE_EXPORT void g15_chr(Driver *drvthis, int x, int y, char c);

/**
 * \brief Draw an icon on the LCD display
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Vertical position
 * \param icon Icon identifier
 * \retval 0 Icon handled by driver
 * \retval -1 Let core handle icon
 *
 * \details Draws predefined icons at the specified position. Some icons
 * are handled directly, others are delegated to the core.
 */
MODULE_EXPORT int g15_icon(Driver *drvthis, int x, int y, int icon);

/**
 * \brief Draw a horizontal bar growing to the right
 * \param drvthis Pointer to driver structure
 * \param x Starting horizontal position
 * \param y Vertical position
 * \param len Maximum length of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options
 *
 * \details Draws a horizontal progress bar that grows from left to right
 * based on the promille value.
 */
MODULE_EXPORT void g15_hbar(Driver *drvthis, int x, int y, int len, int promille, int options);

/**
 * \brief Draw a vertical bar growing upward
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position
 * \param y Starting vertical position (bottom)
 * \param len Maximum height of the bar in characters
 * \param promille Fill level in promille (0-1000)
 * \param options Bar rendering options
 *
 * \details Draws a vertical progress bar that grows from bottom to top
 * based on the promille value.
 */
MODULE_EXPORT void g15_vbar(Driver *drvthis, int x, int y, int len, int promille, int options);

/**
 * \brief Get key input from the G15 keyboard
 * \param drvthis Pointer to driver structure
 * \return Key string or NULL if no key pressed
 *
 * \details Returns key input from the G15 keyboard. Currently not
 * implemented for direct hidraw access.
 */
MODULE_EXPORT const char *g15_get_key(Driver *drvthis);

/**
 * \brief Control the LCD backlight
 * \param drvthis Pointer to driver structure
 * \param on Backlight state (BACKLIGHT_ON/BACKLIGHT_OFF)
 *
 * \details Controls the LCD backlight. For RGB-capable devices,
 * restores saved RGB values when turning on, or sets all colors
 * to 0 when turning off.
 */
MODULE_EXPORT void g15_backlight(Driver *drvthis, int on);

/**
 * \brief Set RGB backlight colors
 * \param drvthis Pointer to driver structure
 * \param red Red component (0-255)
 * \param green Green component (0-255)
 * \param blue Blue component (0-255)
 * \retval 0 Success
 * \retval -1 Error or device doesn't support RGB
 *
 * \details Sets the RGB backlight colors for G510/G510s keyboards.
 * Uses either HID reports or LED subsystem based on configuration.
 */
MODULE_EXPORT int g15_set_rgb_backlight(Driver *drvthis, int red, int green, int blue);

/**
 * \brief Set macro LED status
 * \param drvthis Pointer to driver structure
 * \param m1 M1 key LED state (0=off, 1=on)
 * \param m2 M2 key LED state (0=off, 1=on)
 * \param m3 M3 key LED state (0=off, 1=on)
 * \param mr MR (Macro Record) LED state (0=off, 1=on)
 * \retval 0 Success
 * \retval -1 Error or device doesn't support macro LEDs
 *
 * \details Controls the macro key LEDs on G510/G510s keyboards.
 * Only available on RGB-capable devices.
 */
MODULE_EXPORT int g15_set_macro_leds(Driver *drvthis, int m1, int m2, int m3, int mr);

/**
 * \brief Display a large number on the LCD
 * \param drvthis Pointer to driver structure
 * \param x Horizontal position for the number
 * \param num Number to display (0-9) or 10 for colon
 *
 * \details Renders a large number using the big number bitmap data.
 * Used for clock displays and large numeric indicators.
 */
MODULE_EXPORT void g15_num(Driver *drvthis, int x, int num);

#endif
