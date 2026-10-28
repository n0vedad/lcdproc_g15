// SPDX-License-Identifier: GPL-2.0+

/**
 * \file tests/test_unit_privs.c
 * \brief Unit tests for the LCDd root privilege drop.
 * \author Copyright (C) 2026 n0vedad <https://github.com/n0vedad/>
 * \date 2026
 *
 * \features
 * - Order of initgroups(), setgid() and setuid() (groups before uid)
 * - Failure of any step returns -1 and skips the remaining steps
 * - Unknown users are rejected
 * - Unprivileged processes are left untouched
 *
 * \details Tests drop_privs() from server/drop_privs.h, the code LCDd runs. The
 * credential calls are replaced through the linker (-Wl,--wrap=...), so the
 * tests need no root privileges and change nothing on the system.
 */

// initgroups(), getpwnam_r() and strerror_r() are not part of strict C11
#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "server/drop_privs.h"

/** \brief Fake uid/gid of the target user */
#define TEST_UID 963
#define TEST_GID 963

// Mock report function to suppress output during tests.
// Must stay external: replaces report() from libLCDstuff.a (shared/report.c) at link time.
// NOLINTNEXTLINE(misc-use-internal-linkage)
void report(int level, const char *format, ...)
{
	(void)level;
	(void)format;
}

// Mock state: current credentials, which call fails, and the calls made
static uid_t mock_uid;
static uid_t mock_euid;
static int mock_user_exists;
static const char *mock_fail; // name of the call that fails, or NULL
static char mock_calls[64];   // call order, e.g. "initgroups,setgid,setuid,"
static gid_t mock_set_gid;
static uid_t mock_set_uid;

static void mock_reset(uid_t uid, uid_t euid)
{
	mock_uid = uid;
	mock_euid = euid;
	mock_user_exists = 1;
	mock_fail = NULL;
	mock_calls[0] = '\0';
	mock_set_gid = (gid_t)-1;
	mock_set_uid = (uid_t)-1;
}

static int mock_call(const char *name)
{
	size_t len = strlen(mock_calls);
	snprintf(mock_calls + len, sizeof(mock_calls) - len, "%s,", name);
	if (mock_fail != NULL && strcmp(mock_fail, name) == 0) {
		errno = EPERM;
		return -1;
	}
	return 0;
}

// Wrapped libc functions (-Wl,--wrap=...), must stay external
// NOLINTBEGIN(misc-use-internal-linkage)
uid_t __wrap_getuid(void) { return mock_uid; }

uid_t __wrap_geteuid(void) { return mock_euid; }

int __wrap_getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen,
		      struct passwd **result)
{
	*result = NULL;
	if (!mock_user_exists) {
		return 0;
	}
	snprintf(buf, buflen, "%s", name);
	memset(pwd, 0, sizeof(*pwd));
	pwd->pw_name = buf;
	pwd->pw_uid = TEST_UID;
	pwd->pw_gid = TEST_GID;
	*result = pwd;
	return 0;
}

int __wrap_initgroups(const char *user, gid_t group)
{
	assert(strcmp(user, "lcdd") == 0);
	assert(group == TEST_GID);
	return mock_call("initgroups");
}

int __wrap_setgid(gid_t gid)
{
	mock_set_gid = gid;
	return mock_call("setgid");
}

int __wrap_setuid(uid_t uid)
{
	mock_set_uid = uid;
	return mock_call("setuid");
}
// NOLINTEND(misc-use-internal-linkage)

// Root: groups, gid and uid are set in this order
static void test_drop_as_root(void)
{
	mock_reset(0, 0);
	assert(drop_privs("lcdd") == 0);
	assert(strcmp(mock_calls, "initgroups,setgid,setuid,") == 0);
	assert(mock_set_gid == TEST_GID);
	assert(mock_set_uid == TEST_UID);
	printf("✅ Root drops supplementary groups, gid and uid in order\n");
}

// Set-uid root (only the effective uid is 0) drops as well
static void test_drop_as_setuid_root(void)
{
	mock_reset(1000, 0);
	assert(drop_privs("lcdd") == 0);
	assert(strcmp(mock_calls, "initgroups,setgid,setuid,") == 0);
	printf("✅ Effective uid 0 drops privileges\n");
}

// Unprivileged start (e.g. lcdd.service with User=): nothing to drop
static void test_unprivileged_untouched(void)
{
	mock_reset(963, 963);
	assert(drop_privs("nobody") == 0);
	assert(mock_calls[0] == '\0');
	printf("✅ Unprivileged process is left untouched\n");
}

// Unknown user: rejected before any credential change
static void test_unknown_user(void)
{
	mock_reset(0, 0);
	mock_user_exists = 0;
	assert(drop_privs("lcdd") == -1);
	assert(mock_calls[0] == '\0');
	printf("✅ Unknown user is rejected\n");
}

// Each failing step returns -1 and stops before the next one, so a failed
// setgid() never leaves a process with the new uid but gid 0
static void test_failing_steps(void)
{
	static const struct {
		const char *fail;
		const char *calls;
	} cases[] = {
	    {"initgroups", "initgroups,"},
	    {"setgid", "initgroups,setgid,"},
	    {"setuid", "initgroups,setgid,setuid,"},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		mock_reset(0, 0);
		mock_fail = cases[i].fail;
		assert(drop_privs("lcdd") == -1);
		assert(strcmp(mock_calls, cases[i].calls) == 0);
		printf("✅ Failing %s returns -1 and stops\n", cases[i].fail);
	}
}

int main(void)
{
	printf("🚀 Starting LCDd privilege drop tests\n");
	printf("=====================================\n");

	test_drop_as_root();
	test_drop_as_setuid_root();
	test_unprivileged_untouched();
	test_unknown_user();
	test_failing_steps();

	printf("\n🎉 All privilege drop tests passed\n");
	return 0;
}
