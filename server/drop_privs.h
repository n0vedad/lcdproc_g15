// SPDX-License-Identifier: GPL-2.0+

/**
 * \file server/drop_privs.h
 * \brief Root privilege drop for LCDd
 * \author n0vedad
 * \date 2026
 *
 *
 * \features
 * - Switches a root process to the configured user
 * - Sets supplementary groups and primary group before the uid
 * - Leaves unprivileged processes untouched
 *
 * \usage
 * - Included by the LCDd server (main.c)
 * - Included by the unit tests, which replace the credential calls with mocks
 *   through the linker (-Wl,--wrap)
 *
 * \details Header-only so that the unit tests test the same code the server
 * runs, without linking the server itself.
 */

#ifndef DROP_PRIVS_H
#define DROP_PRIVS_H

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared/report.h"

/**
 * \brief Drop root privileges to the configured user
 * \param user User name to switch to
 * \retval 0 Privileges dropped, or not running as root
 * \retval -1 User unknown or switching failed
 *
 * \details Sets the user's supplementary groups and primary group before the
 * uid, since setgid()/initgroups() need root (CERT POS36-C). Without them the
 * process would keep gid 0 and root's supplementary groups.
 */
static inline int drop_privs(const char *user)
{
	struct passwd pwd;
	struct passwd *pwent;
	char buffer[4096];

	if (getuid() != 0 && geteuid() != 0) {
		return 0;
	}

	// getpwnam_r() is thread-safe (POSIX.1-2001), uses caller-provided buffer
	int result = getpwnam_r(user, &pwd, buffer, sizeof(buffer), &pwent);
	if (result != 0 || pwent == NULL) {
		report(RPT_ERR, "User %.40s not a valid user!", user);
		return -1;
	}

	if (initgroups(pwent->pw_name, pwent->pw_gid) < 0 || setgid(pwent->pw_gid) < 0 ||
	    setuid(pwent->pw_uid) < 0) {
		char err_buf[256];
		strerror_r(errno, err_buf, sizeof(err_buf));
		report(RPT_ERR, "Unable to switch to user %.40s: %s", user, err_buf);
		return -1;
	}

	return 0;
}

#endif
