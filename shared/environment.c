// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/environment.c
 * \brief Thread-safe environment variable caching implementation
 * \author n0vedad
 * \date 2025
 *
 * \features
 * - Caches HOME and SHELL environment variables at program startup
 * - Thread-safe immutable storage using static buffers
 * - Idempotent initialization (safe to call multiple times)
 * - Automatic fallback values for missing variables
 *
 * \usage
 * - Internal implementation of environment caching
 * - Used by both lcdproc client and lcdexec
 * - Eliminates concurrency-mt-unsafe warnings from clang-tidy
 *
 * \details
 * This implementation uses static buffers to cache environment variables
 * at program startup. Since the buffers are only written once (during
 * initialization before any threads exist) and then become read-only,
 * this approach is completely thread-safe without requiring locks.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "posix_wrappers.h"
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "environment.h"
#include "posix_wrappers.h"

/** \brief Maximum path length fallback if not defined by system */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/** \brief Cached environment variables
 *
 * \details Write-once, read-many cache for HOME and SHELL environment variables.
 * Initialized once at startup and then accessed without syscalls.
 */
static struct {
	char home[PATH_MAX];  ///< Cached HOME directory path
	char shell[PATH_MAX]; ///< Cached SHELL path
	bool initialized;     ///< Cache initialization flag
} env_cache = {0};

// Initialize environment variable cache from actual environment
void env_cache_init(void)
{
	const char *tmp;

	// Idempotent: only initialize once
	if (env_cache.initialized) {
		return;
	}

	// Cache HOME directory
	// safe_getenv() is safe here: called once during initialization before any threads start
	tmp = safe_getenv("HOME");
	if (tmp) {
		strncpy(env_cache.home, tmp, sizeof(env_cache.home) - 1);
		env_cache.home[sizeof(env_cache.home) - 1] = '\0';
	} else {
		env_cache.home[0] = '\0';
	}

	// Cache SHELL with fallback to /bin/sh
	// safe_getenv() is safe here: called once during initialization before any threads start
	tmp = safe_getenv("SHELL");
	if (tmp) {
		strncpy(env_cache.shell, tmp, sizeof(env_cache.shell) - 1);
		env_cache.shell[sizeof(env_cache.shell) - 1] = '\0';
	} else {
		strncpy(env_cache.shell, "/bin/sh", sizeof(env_cache.shell) - 1);
		env_cache.shell[sizeof(env_cache.shell) - 1] = '\0';
	}

	env_cache.initialized = true;
}

// Get cached HOME directory path (thread-safe)
const char *env_get_home(void)
{
	if (!env_cache.initialized) {
		return NULL;
	}
	return env_cache.home[0] != '\0' ? env_cache.home : NULL;
}

// Get cached SHELL path (thread-safe, never NULL after init)
const char *env_get_shell(void)
{
	if (!env_cache.initialized) {
		return NULL;
	}
	return env_cache.shell;
}
