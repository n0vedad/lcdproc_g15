# Changelog

All notable changes to LCDproc G15 are listed here, newest first. Known open issues are not
listed here: they are kept as `\todo` entries in the source code and collected by priority in
the Doxygen documentation (`docs/html/todo.html`, see [CONTRIBUTING.md](CONTRIBUTING.md#documentation)).

LCDproc G15 is a fork of [LCDproc](https://github.com/lcdproc/lcdproc) 0.5dev (upstream commit
`71877ee0`, 2024-12-07). Each version starts with the change that set its number in the
`PKGBUILD` and ends before the next one; `configure.ac` carries the same number.

## [0.0.6] - 2026-10-28

### Upgrade notes

- pacman keeps a locally changed `/etc/LCDd.conf` and installs the new template as
  `/etc/LCDd.conf.pacnew`; merge it by hand (e.g. with `pacdiff`).
- Set `User=lcdd` in `/etc/LCDd.conf`. It is ignored by `lcdd.service`, but a manual
  `sudo LCDd` would otherwise switch to `nobody`, which has no access to the keyboard.

### Security

- **LCDd no longer runs as root.** `lcdd.service` starts it as the new system user `lcdd`
  (`sysusers.d/lcdproc-g15.conf`). A udev rule (`60-lcdproc-g15.rules`) gives `lcdd` access to
  the keyboard's hidraw device and the `g15::*` LED files, also when logind has already set an
  ACL on the device.
- **Privilege drop fixed** (bug inherited from upstream LCDproc): when started as root, LCDd
  only called `setuid()`, so it kept gid 0 and root's supplementary groups, and it silently kept
  running as root when switching users failed (e.g. without `CAP_SETUID`). It now sets
  supplementary groups, gid and uid in this order and stops with
  `Refusing to run with root privileges, abort.` if any step fails.
- The default user is `lcdd` instead of `nobody`.

### Fixed

- **Widget replies:** since 0.0.4 all replies of `widget_add`, `widget_set` and `widget_del`
  ended with a literal `\n` instead of a newline, so clients received them merged with the
  next server message.
- **G-keys lost while LCDd shows its own screen:** the lcdproc client reserved the G-keys and
  M1/M2/M3/MR as shared keys, which LCDd only passes on while one of the client's screens is
  visible. While the server screen, the LCDd menu or another client's screen was shown, the
  key presses went to LCDd and the macros did not fire. They are now reserved exclusively.
- **Messages split across two reads** were dropped by the lcdproc client, including key
  events; the unterminated rest of a read is now carried over to the next one.
- **Strict number parsing:** protocol arguments, command line options, server replies and
  `/proc` values are parsed with overflow and trailing-character checks instead of `atoi()` and
  `sscanf()`. For example, `widget_set … 12abc 1 …` is rejected as invalid coordinates instead of
  being read as 12.
- Out-of-bounds read when a `connect` reply ends with a keyword without value (lcdproc,
  lcdexec).
- Unterminated read buffer when parsing `/proc/apm`, and a possible NULL dereference for
  `/proc/net/dev` lines without `:`.
- Link order of the server libraries (`libLCDcommands.a` before `libLCDstuff.a`).
- Doxygen showed the version as "0.6.0-g15"; it now uses the version from `configure`.

### Changed

- Source files include their own headers and declare shared globals there instead of in other
  `.c` files; file-local functions and variables are `static`. The include cycle between
  `widget.h` and `screen.h` is resolved.
- `make lint` (clang-tidy), the compiler and Doxygen report no warnings.
- CI: the containers create the user `lcdd` and install Doxygen and graphviz; lock files are no
  longer checked by Prettier; `test-compilers` prints the build log on failure.
- CI: memory leak detection runs again in the device tests (it was skipped since 2025-09) and
  covers all unit tests; the key table in `gkey_macro.c` is laid out so that clang-format 22 and
  23 agree.
- `make check-dev-deps` / `make install-dev-deps` include `doxygen` and `graphviz`.

### Added

- Unit tests `test_unit_privs` (privilege drop) and `test_unit_str` (number conversions).
- Integration tests: one line per reply, invalid coordinates, G-key delivery to the lcdproc
  client (exclusive reservation, split messages). The tests now skip LCDd's asynchronous
  `listen`/`ignore` messages, which made them fail at random.

## [0.0.5] - 2026-09-14

### Fixed

- G510 backlight colour via the multicolor LED API on Linux 6.15+ (the legacy `color` attribute
  is used as a fallback on older kernels).

### Changed

- `make debug` targets for the debug workflow in the autotools build; `make distclean` also
  removes generated documentation.
- clang-tidy internal-linkage findings in the g15 driver and unit tests resolved.
- Prettier 3.9.6 and prettier-plugin-sh 0.18.1.

### Added

- Unit tests for the LED sysfs colour writes, including flush errors.

### Upstream

- Merged LCDproc master of 2026-08-30 without adopting changes.

## [0.0.4] - 2025-12-28

### Added

- G510 macro LEDs (M1/M2/M3/MR) set by the macro system.
- SysInfo screen with system metrics, symmetric block rendering for title widgets.
- Debug workflow (`make debug`, `tests/debug/`).
- Doxygen documentation for the whole code base; TODOs are Doxygen `\todo` entries grouped
  by priority (they replace `TODO.md`).
- CI: integration tests, ThreadSanitizer and coverage analysis.

### Changed

- Code standardization: SPDX headers, extended `.clang-tidy`, thread-safe platform helpers
  (`shared/environment.c`, `shared/posix_wrappers.h`), `machine_Linux.c` merged into
  `machine.c`, popt instead of the bundled getopt.
- Macros record keyboard input only (mouse support removed).
- Windows-only checks removed.
- Doxygen documentation is generated in development builds only.
- `configure.ac` carries the fork version (0.0.4) instead of upstream's 0.5dev.

### Fixed

- Several security issues and buffer overflows in clients and server, compiler and
  clang-tidy warnings (array bounds, buffer safety).
- Wrong port configuration, development build, testing framework.
- TitleSpeed range validation and timing of the GPL notice.
- G510 hardware logo is cleared on initialization.
- CI: pacman/ICU breakage in GitHub Actions; Prettier optional in the pre-commit hook.

## [0.0.3] - 2025-09-18

### Added

- Device detection by USB product ID (G15, G15 v2, G510, G510s); RGB features only on RGB
  devices.
- RGB control method selectable: HID reports or LED subsystem (`RGBMethod`).
- clang-tidy linting (`make lint`) and clang as the default compiler.
- Test framework: unit tests with a mock hidraw device, integration tests with a mock
  server, local coverage analysis; debug tools moved to `tests/debug/`.
- GitHub workflows for code quality and device tests.

### Changed

- Heartbeat off by default.
- Build: git hook detection, tool checks, build order; `autogen.sh` and `setup-hooks.sh`
  replaced by `GNUmakefile` targets.

### Fixed

- Multiple buffer overflows (lcdexec menu, report, iface and others).
- Formatting edge cases and typos.

## [0.0.2] - 2025-09-01

### Added

- G-key macro system: recording and playback of G1-G18 in modes M1-M3 via `ydotool`
  (`ydotoold.service`).
- Debug tools for keys and RGB (`debug/`).
- Code formatting with clang-format and Prettier, copyright notices, pre-commit hook.
- GitHub workflow for code quality.

### Changed

- RGB backlight via the LED subsystem instead of HID reports.
- `LCDd.conf` lists only the drivers of this fork (g15, linux_input, debug).
- Non-Linux platform support removed (Darwin, FreeBSD, NetBSD, OpenBSD, SunOS).

### Fixed

- Config option to disable RGB; config explanations removed by mistake; missing includes for
  clang 20.

## [0.0.1] - 2025-08-31

### Added

- RGB backlight for G510/G510s.
- systemd services for LCDd and the lcdproc client.
- Arch Linux packaging (`PKGBUILD`, install script).

### Changed

- Stripped down to the G15 driver and essentials: all other drivers, clients (lcdvc, metar,
  examples), contrib, scripts, docbook documentation and CVS leftovers removed; g15daemon
  support removed in favour of hidraw.
- Build system trimmed (`acinclude.m4`, `configure.ac`); `LCDd.conf` moved to `server/` and
  reduced to the G15 settings; config files standardized; default title scroll speed.
- README and INSTALL rewritten for the G15 fork.

### Fixed

- Idle time on the time screen.
