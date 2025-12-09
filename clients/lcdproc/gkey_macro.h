// SPDX-License-Identifier: GPL-2.0+

/**
 * \file clients/lcdproc/gkey_macro.h
 * \brief G-Key macro system interface for Logitech G15 keyboards
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - **gkey_macro_init()**: Initialize macro system with persistent storage
 * - **gkey_macro_cleanup()**: Clean shutdown with macro saving
 * - **gkey_macro_handle_key()**: Process G-key, mode, and record key events
 * - **gkey_macro_get_mode()**: Query current active mode (M1/M2/M3)
 * - **gkey_macro_is_recording()**: Check recording status
 * - **gkey_macro_update_leds()**: Update keyboard LED indicators
 * - **start_input_recording()**: Begin input event capture
 * - **stop_input_recording()**: End recording and process events
 * - Real-time input event recording from /dev/input/event* devices
 * - Macro playback using ydotool for Wayland compatibility
 * - Three independent macro modes with 18 G-keys each
 * - Persistent macro storage in text format
 * - Thread-safe recording system with pthread
 *
 * \usage
 * - Include this header in lcdproc client modules that need macro support
 * - Call gkey_macro_init() during program startup
 * - Use gkey_macro_handle_key() to process keyboard events from G15 driver
 * - Call gkey_macro_cleanup() during program shutdown
 * - Used for G15 keyboard macro recording and playback functionality
 * - Requires ydotool daemon for macro execution
 * \code
 * // Initialize the macro system
 * if (gkey_macro_init() == 0) {
 *     // Handle key events
 *     gkey_macro_handle_key("G1");  // Execute macro or start recording
 *     gkey_macro_handle_key("MR");  // Toggle recording mode
 *     gkey_macro_handle_key("M2");  // Switch to mode 2
 *
 *     // Cleanup when done
 *     gkey_macro_cleanup();
 * }
 * \endcode
 *
 * \details Public interface for the G-Key macro system that provides
 * recording, playback, and management of macros for Logitech G15 keyboards.
 * Supports 18 G-keys across 3 modes (M1, M2, M3) with persistent storage.
 */

#ifndef GKEY_MACRO_H
#define GKEY_MACRO_H

/**
 * \brief Initialize the G-Key macro system.
 * \retval 0 Initialization successful
 * \retval -1 Initialization failed
 * \note Requires write access to ~/.config/lcdproc/ or /tmp/ for macro storage.

 * \usage
 * - Creates ~/.config/lcdproc/ directory if needed
 * - Loads existing macros from g15_macros.json
 * - Sets up initial macro storage arrays
 * - Configures LED status on G15 keyboard
 *
 * \details Initializes the macro system by setting up storage structures,
 * creating configuration directories, loading existing macros, and setting
 * initial LED states. Must be called before using any other macro functions.
 */
int gkey_macro_init(void);

/**
 * \brief Clean up macro system resources.
 * \note Safe to call multiple times or when system is not initialized.
 *
 * \usage
 * - Stops active input recording threads
 * - Saves all macros to storage file
 * - Releases system resources
 * - Closes input device file descriptors
 *
 * \details Stops any active recording sessions and saves all macros
 * to persistent storage. Should be called during program shutdown.
 */
void gkey_macro_cleanup(void);

/**
 * \brief Handle G-Key and mode key press events.
 * \param key_name Name of the pressed key (e.g., "G1", "M1", "MR")
 * \note Invalid key names are ignored. Thread-safe for concurrent calls.
 *
 * \usage
 * - **G1-G18**: Execute stored macro or start recording if in record mode
 * - **M1-M3**: Switch between macro modes (affects which macros are active)
 * - **MR**: Toggle recording mode on/off
 * - First MR press: Activates recording mode (LED indicator turns on)
 * - G-key press: Starts recording input events for that specific key
 * - Second MR press: Stops recording and saves the macro
 * - G-key press: Executes the stored macro for current mode
 * - M-key press: Changes active mode and updates LED indicators

 * \details Processes key press events for G-keys (G1-G18), mode keys
 * (M1-M3), and the macro record key (MR). Handles mode switching,
 * recording start/stop, and macro playback.
 */
void gkey_macro_handle_key(const char *key_name);

/**
 * \brief Get the current macro mode.
 * \retval mode Current mode string ("M1", "M2", or "M3")
 * \note The returned string is statically allocated and does not
 * need to be freed. Default mode is "M1" if not explicitly changed.
 *
 * \details Returns the currently active macro mode which determines
 * which set of macros will be executed when G-keys are pressed.
 * The mode affects both playback and recording operations.
 */
const char *gkey_macro_get_mode(void);

/**
 * \brief Check if macro recording is currently active.
 * \retval 1 Recording is active
 * \retval 0 Recording is not active
 * \note This function is thread-safe and can be called from any context.
 *
 * \details Returns the current recording state. When recording is active,
 * the next G-key press will start recording input events for that key.
 * The MR LED indicator on the keyboard will be lit during recording mode.
 */
int gkey_macro_is_recording(void);

/**
 * \brief Update macro LED status on the keyboard.
 * \note This function communicates with the LCDd server via socket
 * commands. LED updates may fail if server connection is not available.
 *
 * \usage
 * - **M1/M2/M3**: One LED lit to indicate current active mode
 * - **MR**: LED lit when recording mode is active
 *
 * \details Sends LED control commands to the LCDd server to update the
 * mode indicators (M1, M2, M3) and recording indicator (MR) on the
 * G15 keyboard. Should be called whenever the mode changes or recording
 * status changes.
 */
void gkey_macro_update_leds(void);

/**
 * \brief Start input event recording from /dev/input/event* devices.
 * \param target_gkey G-key to record macro for ("G1" to "G18")
 * \retval 0 Recording started successfully
 * \retval -1 Failed to start recording
 * \note Requires read access to /dev/input/event* devices (may need
 * root privileges or membership in input group).
 *
 * \usage
 * - Scans /dev/input/ for keyboard devices
 * - Opens devices in non-blocking mode
 * - Starts background recording thread
 * - Captures key events to temporary file
 * - Automatically adds timing delays between events
 *
 * \details Begins capturing input events from all available keyboard
 * devices for creating a new macro. Opens all /dev/input/event* devices
 * that represent keyboards and starts a background thread to monitor
 * key press events.
 */
int start_input_recording(const char *target_gkey);

/**
 * \brief Stop input event recording and convert to ydotool commands.
 * \retval 0 Recording stopped and processed successfully
 * \retval -1 Error stopping recording or no active recording
 * \note Safe to call even if no recording is active. The function
 * will process any pending events before returning.
 *
 * \usage
 * - Signals recording thread to stop
 * - Waits for thread completion (pthread_join)
 * - Closes all input device file descriptors
 * - Converts raw events to ydotool commands
 * - Groups consecutive characters into text typing commands
 * - Preserves special keys as individual key press commands
 * - Saves optimized macro to storage
 * - Removes temporary recording files
 *
 * \details Stops the active input recording session, waits for the
 * background recording thread to finish, and processes the captured
 * events into optimized macro commands.
 */
int stop_input_recording(void);

#endif
