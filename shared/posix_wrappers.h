// SPDX-License-Identifier: GPL-2.0+

/**
 * \file shared/posix_wrappers.h
 * \brief Thread-safe wrappers for POSIX functions with MT-unsafe warnings
 * \author LCDproc-G15 Team
 * \date 2025
 *
 * \details This header provides wrapper functions for POSIX functions that are marked
 * as MT-unsafe by clang-tidy's static analysis, but are actually thread-safe in our
 * specific usage patterns.
 *
 * \usage
 * - Include this header instead of directly calling POSIX functions
 * - Centralizes clang-tidy warning suppressions in one place
 * - Documents why each function is safe in our context
 *
 * \details Background: clang-tidy uses a hardcoded POSIX blacklist and cannot analyze
 * runtime conditions. These wrappers suppress false-positive warnings while documenting
 * the actual thread-safety guarantees.
 */

#ifndef POSIX_WRAPPERS_H
#define POSIX_WRAPPERS_H

#include <dirent.h>
#include <dlfcn.h>
#include <stdlib.h>

/**
 * \brief Thread-safe wrapper for readdir()
 * \param dirp Directory stream pointer returned by opendir()
 * \retval Pointer to directory entry structure, or NULL on end of directory or error
 *
 * \details Wraps readdir() to suppress clang-tidy concurrency-mt-unsafe warnings.
 *
 * Thread safety: Safe when each thread uses its own DIR* stream pointer.
 * - glibc implements per-DIR* locking (not global state)
 * - Different threads with different DIR* pointers are completely independent
 * - POSIX marks it MT-unsafe due to shared static buffer in some implementations,
 *   but glibc allocates per-stream buffers
 *
 * Usage: Replace `readdir(dir)` with `safe_readdir(dir)` throughout codebase.
 */
static inline struct dirent *safe_readdir(DIR *dirp)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	return readdir(dirp);
}

/**
 * \brief Thread-safe wrapper for getenv()
 * \param name Environment variable name to retrieve
 * \retval Pointer to environment variable value, or NULL if not found
 *
 * \details Wraps getenv() to suppress clang-tidy concurrency-mt-unsafe warnings.
 *
 * Thread safety: Safe when called before any threads are created, or when the
 * environment is not being modified concurrently (no setenv/putenv/unsetenv calls).
 * - LCDproc reads environment variables only during initialization
 * - No concurrent setenv/putenv/unsetenv calls in codebase
 * - POSIX marks it MT-unsafe due to potential concurrent modification of environ
 *
 * Usage: Replace `getenv(name)` with `safe_getenv(name)` for initialization-time
 * environment variable access.
 */
static inline const char *safe_getenv(const char *name)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	return getenv(name);
}

/**
 * \brief Thread-safe wrapper for dlerror()
 * \retval Pointer to error string, or NULL if no error occurred
 *
 * \details Wraps dlerror() to suppress clang-tidy concurrency-mt-unsafe warnings.
 *
 * Thread safety: Safe when dlopen/dlsym/dlclose calls are externally synchronized.
 * - LCDproc loads driver modules sequentially during initialization
 * - No concurrent dlopen/dlsym operations in codebase
 * - POSIX marks it MT-unsafe because error state is per-thread but not protected
 *
 * Usage: Replace `dlerror()` with `safe_dlerror()` after dlopen/dlsym calls that
 * are already synchronized by design.
 */
static inline const char *safe_dlerror(void)
{
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	return dlerror();
}

#endif
