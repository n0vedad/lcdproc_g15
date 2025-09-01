/** \file clients/lcdproc/gkey_macro.h
 * G-Key macro support for LCDproc client
 */

#ifndef GKEY_MACRO_H
#define GKEY_MACRO_H

/**
 * Initialize G-Key macro system
 */
int gkey_macro_init(void);

/**
 * Cleanup G-Key macro system
 */
void gkey_macro_cleanup(void);

/**
 * Handle G-Key press event
 * \param key_name  Name of pressed key (G1-G18, M1-M3, MR)
 */
void gkey_macro_handle_key(const char *key_name);

/**
 * Process macro recording/playback
 * Called regularly from main loop
 */
void gkey_macro_process(void);

/**
 * Get current macro mode (M1, M2, M3)
 */
const char *gkey_macro_get_mode(void);

/**
 * Check if currently recording
 */
int gkey_macro_is_recording(void);

/**
 * Start input event recording from /dev/input/event* devices
 * \param target_gkey  G-key to record macro for
 */
int start_input_recording(const char *target_gkey);

/**
 * Stop input event recording and convert to ydotool commands
 */
int stop_input_recording(void);

#endif /* GKEY_MACRO_H */