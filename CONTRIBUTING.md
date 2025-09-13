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

```bash
# Install formatting and static analysis dependencies
sudo pacman -S clang-format clang-tidy gcc npm bear valgrind act
```

### Build for Development

Use the development build for complete setup and IDE support:

```bash
# Complete development setup in one command (recommended)
make dev

# Or manual step-by-step setup:
make setup-autotools
make setup-hooks-install

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

```ini
# Enable debug driver
Driver=debug
```

## Driver Properties

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

## Code Formatting

- **C/C++**: clang-format with project-specific configuration
- **Markdown/JSON/Shell**: prettier for consistent formatting

### Usage

```bash
# Format code manually
make format

# Check format status
make format-check
```

### Git Hooks

Two automated workflows can run on every commit/push:

- **Code Quality**: Formatting, linting, static analysis for all code changes
- **Device Tests**: Hardware simulation tests for driver/test changes only

Please see [tests/README.md](tests/README.md) for more details.

## Static Analysis

This project uses **clang-tidy** for comprehensive static analysis of C code.

### Configuration

Static analysis rules are configured in `.clang-tidy` with focus on:

- System programming patterns (USB, threading, memory management)
- Security-critical code paths
- Performance optimizations for real-time applications
- The compile database is automatically generated when needed by `make lint` or `make dev`

### Usage

```bash
# Run complete static analysis
make lint

# Auto-fix issues where possible
make lint-fix
```

## Testing

### Running Tests

The project includes a comprehensive mock testing system and Python debug scripts for real hardware analysis. Please refer to [tests/README.md](tests/README.md) for more details.

## INPUT/OUTPUT

Only one Display/output device (g15.so) and input device (linux_input.so) is supported in this fork.

For LCDd (the server) to use the device, it needs to load a driver. The
drivers are so called 'shared modules', that usually have an extension
of `.so`. The drivers to be loaded should be specified in the config file
(by one or more `Driver=` lines), or on the command line. The command line
should only be used to override things in the config file. The drivers
should be in a directory that is indicated by the `DriverPath=` line in
the configfile.

## Submitting Changes

### Before Submitting

1. Ensure all tests pass: `cd tests && make test-full`
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
