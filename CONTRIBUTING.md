# Contributing to LCDproc G15

Thank you for your interest in contributing to this project! This document provides guidelines and technical explanations for developers working on the codebase.

## Development Setup

### Prerequisites

For development work, you'll need these additional tools beyond the runtime requirements:

- `clang-format and clang-tidy` for comprehensive toolchain (formatting and static analysis)
- `gcc` for fallback and multi-compiler testing
- `npm` for Markdown/JSON/Shell formatting (includes Node.js)
- `bear` for generating compile database
- `valgrind` for detecting memory leaks
- `act` for testing GitHub workflow locally (optional)
- `python-evdev` for for reading raw input events from /dev/input/event\*
- `gcovr` for code coverage analysis and reporting

```bash
# Install all optional dependencies
sudo pacman -S clang-format clang-tidy gcc npm bear valgrind act python-evdev gcovr
```

### Build for Development

Use the development build for complete setup with test support:

```bash
# Complete development setup in one command (recommended)
make dev

# Or manual step-by-step setup:
make setup-autotools
make setup-hooks-install (for local git hooks)

# Development configuration (includes debug driver)
./configure \
  --prefix=/usr \                      # Install base directory
--sbindir=/usr/bin \                   # Server binaries to /usr/bin (not /usr/sbin)
--sysconfdir=/etc \                    # Configuration files to /etc
--enable-libusb \                      # Enable libusb support for G15 device communication
--enable-lcdproc-menus \               # Enable menu support in lcdproc client
--enable-stat-smbfs \                  # Enable Samba filesystem statistics
--enable-debug \                       # Required for debug driver
--enable-drivers=g15,linux_input,debug # Build all drivers including debug

# Build
make
```

But it may not do what you want, so please take a few seconds to type:

```bash
./configure --help
```

**Important:** Without `--enable-debug`, the debug driver will be silently ignored even if specified in `--enable-drivers`.

Now for debugging you just need to uncomment the debug driver at `/etc/LCDd.conf`:

## Debug Driver Properties

| Property         | Value     | Description                       |
| ---------------- | --------- | --------------------------------- |
| **Display Size** | 20x4      | Standard LCD character dimensions |
| **Cell Size**    | 5x8       | Character cell pixel dimensions   |
| **Backlight**    | Simulated | Software brightness control       |
| **Contrast**     | Simulated | Software contrast control         |
| **Frame Buffer** | Memory    | Single buffer implementation      |

## Integration with Test Suite

The debug driver is used in automated testing to:

- **Validate screen output** - Verify client display content
- **Test driver operations** - Ensure all API functions work
- **Monitor performance** - Measure update frequencies
- **Debug test failures** - Trace exact driver behavior

See `tests/` directory for debug driver test integration.

### Git Hooks

Two automated workflows can run on every commit/push:

- **Code Quality**: Formatting, linting, static analysis for all code changes
- **Device Tests**: Hardware simulation tests for driver/test changes only

They are implemented two-way, local (via `make setup-hooks-install`) and via GitHub workflows.

Please see [tests/README.md](tests/README.md) for more details.

## Testing

### Running Tests

The project includes a comprehensive testing system with unit- and integration tests and Python debug scripts for real hardware analysis. Further it includes code formatting & static analysis and test coverage reports. Please refer to [tests/README.md](tests/README.md) for available tests and more.

## Submitting Changes

### Minimal Steps Before Submitting

1. Ensure all tests pass: `make test-full`
2. Verify code formatting: `make format-check`
3. Run static analysis: `make lint`
4. Test GitHub workflow locally (optional): `act push`

### Commit Messages

Follow conventional commit format:

- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation changes
- `refactor:` for code refactoring
- `test:` for test improvements

### Pull Requests

- If available eference related issues
- Describe the changes and their impact
- Include test results if applicable
- Ensure CI/CD checks pass

## Getting Help

- Check existing issues in the repository
- Review the [FAQ.md](FAQ.md) for common questions
- Reference the [INSTALL.md](INSTALL.md) for setup issues
- See [tests/README.md](tests/README.md) for testing
