// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/environment.h
 * \brief Thread-safe environment variable caching
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - Thread-safe cached access to environment variables
 * - Eliminates need for getenv() which uses static buffers
 * - One-time initialization at program startup
 * - Immutable after initialization (no runtime changes)
 *
 * \usage
 * - Call env_cache_init() once at program start before any threads
 * - Use env_get_home() and env_get_shell() instead of getenv()
 * - Thread-safe for both single-threaded and multi-threaded programs
 *
 * \details
 * This module provides thread-safe access to commonly-used environment
 * variables by caching them at program startup. This eliminates the need
 * for getenv() which returns pointers to static buffers and is not
 * thread-safe. The cached values are immutable after initialization,
 * which is the desired behavior for most applications (environment
 * variables should not change during program execution).
 */

#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

/**
 * \brief Initialize environment variable cache
 *
 * \details
 * Must be called once at program startup before any threads are created.
 * Reads common environment variables (HOME, SHELL) and caches them in
 * thread-safe storage. Subsequent calls are ignored (idempotent).
 *
 * This function should be called early in main() before any other
 * initialization that might spawn threads or call env_get_*() functions.
 */
void env_cache_init(void);

/**
 * \brief Get cached HOME directory path
 * \retval Non-NULL Pointer to cached HOME directory path
 * \retval NULL HOME not set or env_cache_init() not called
 *
 * \details
 * Returns thread-safe pointer to cached HOME directory. The returned
 * pointer is valid for the lifetime of the program and never changes.
 * Unlike getenv("HOME"), this function is completely thread-safe.
 */
const char *env_get_home(void);

/**
 * \brief Get cached SHELL path
 * \retval Non-NULL Pointer to cached SHELL path (may be fallback "/bin/sh")
 * \retval NULL env_cache_init() not called
 *
 * \details
 * Returns thread-safe pointer to cached SHELL path. If SHELL environment
 * variable was not set at initialization, returns "/bin/sh" as fallback.
 * The returned pointer is valid for the lifetime of the program.
 */
const char *env_get_shell(void);

#endif
