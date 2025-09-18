# G-Series Device Tests

This directory contains comprehensive unit tests, integration tests, mock systems, and Python debug scripts for G15/G510 keyboards. It includes code formatting, static analysis, and test coverage reports.

## Test System Overview

The test suite consists of two complementary systems:

1. **Unit Tests** (`test_unit_g15`) - Mock-based component testing
2. **Integration Tests** (`test_integration_g15`) - End-to-end system testing

### **Unit Test System (Mock-Based)**

The unit test system uses a mock hidraw interface that simulates different USB devices:

```c
struct mock_device_info test_devices[] = {
    {0x046d, 0xc222, "Logitech G15 (Original)", 0, 0},   // No RGB
    {0x046d, 0xc227, "Logitech G15 v2", 0, 0},           // No RGB
    {0x046d, 0xc22d, "Logitech G510", 1, 0},             // RGB support
    {0x046d, 0xc22e, "Logitech G510s", 1, 0},            // RGB support
    {0x046d, 0xc221, "Unknown Logitech Device", 0, 0},   // Unknown
};
```

### **Integration Test System**

The integration test system validates end-to-end functionality: LCDd server startup, TCP protocol communication, client connections, and multi-driver support.

Unlike unit tests that use mocks, integration tests run real LCDd processes with actual drivers:

```
LCDd Server (debug/g15/input) ←→ TCP Protocol ←→ lcdproc Client
```

**Supported Drivers:**

- **debug** - 20x4 virtual display (safe, default)
- **g15** - Real G15/G510 hardware (requires keyboard)
- **linux_input** - Generic input driver

## Test Matrix & Coverage

Type `make help` for further informations.

### **Device Coverage**

- ✅ **G15 Original (046d:c222)**: Device detection, No RGB support
- ✅ **G15 v2 (046d:c227)**: Device detection, No RGB support
- ✅ **G510 (046d:c22d)**: Device detection, RGB support enabled
- ✅ **G510s (046d:c22e)**: Device detection, RGB support enabled
- ✅ **Unknown devices**: Safe defaults fallback

### **Functional Coverage**

- ✅ **USB device detection**: All supported device IDs validated
- ✅ **RGB feature detection**: Automatic based on device type
- ✅ **RGB command validation**: HID reports + LED subsystem methods
- ✅ **G-Key macro system**: 18 G-keys, M1/M2/M3 modes
- ✅ **Debug driver**: Virtual display functionality
- ✅ **Error handling**: Device failures, connection issues, memory management

## Running Tests

### **Local Execution**

#### **Development Workflow (Recommended)**

```bash
# Daily development (fast feedback)
make check # Basic unit tests (~3 seconds)

# Before commits (thorough validation)
make test-full # Unit tests + memory leak detection (~30-60 seconds)
# Non-destructive, safe for daily use

# Before releases (complete validation)
make test-ci # Full + multi-compiler testing (~2-5 minutes)
# ⚠️ DESTRUCTIVE: Rebuilds entire project!
```

#### **Integration Testing**

```bash
# Basic integration tests (debug driver)
make test-integration # Safe virtual testing

# Hardware-specific tests (requires hardware)
make test-integration-g15   # G15/G510 hardware required
make test-integration-input # Linux input driver
make test-integration-all   # All drivers comprehensive

# Component testing
make test-mock    # Mock server standalone
make test-server  # LCDd server only
make test-clients # Client functionality only
make test-e2e     # End-to-end workflow
```

#### **Unit Test Categories**

```bash
# Device-specific testing
make test-g15  # Test only G15 devices (no RGB support)
make test-g510 # Test only G510 devices (with RGB support)

# Test scenarios (for debugging)
make test-scenarios # All 4 specialized tests combined

# Individual test scenarios
make test-scenario-detection # Only device detection (USB device IDs)
make test-scenario-rgb       # Only RGB color testing (HID + LED)
make test-scenario-macros    # Only macro system (18 G-keys, M1/M2/M3)
make test-scenario-failures  # Only error handling (device failures)

# Advanced diagnostics
make test-verbose   # Run tests with detailed output
make test-memcheck  # Memory leak detection only (requires valgrind)
make test-compilers # Multi-compiler build testing only
# ⚠️ DESTRUCTIVE: Rebuilds with clang + gcc
```

## Code Formatting

- **C/C++**: clang-format with project-specific configuration
- **Markdown/JSON/Shell**: prettier for consistent formatting

Rules are specified at `.clang-format` and `.prettierrc/.prettierignore`.

## Static Analysis

This project uses **clang-tidy** for comprehensive static analysis of C code.

### Configuration

Static analysis rules are configured in `.clang-tidy` with focus on:

- System programming patterns (USB, threading, memory management)
- Security-critical code paths
- Performance optimizations for real-time applications
- The compile database is automatically generated when needed by `make lint` or `make dev`

```bash
# Code Quality & Formatting (requires 'make dev')
make format       # Format all C code with clang-format + prettier
make format-check # Check if formatting is needed (non-destructive)
make lint         # Run clang-tidy static analysis
make lint-fix     # Run clang-tidy with automatic fixes
```

## Coverage Analysis

**Coverage Output Files:**

- `coverage.html` - Interactive HTML report with line-by-line coverage
- `coverage.xml` - Machine-readable XML format (Cobertura)
- `*.gcov` - Individual file coverage reports
- `*.gcno` / `*.gcda` - GCC coverage data files

### **Coverage Details**

- **mock_hidraw_lib.c**: 89% (67/75 lines)
- **test_g15.c**: 90% (568/627 lines)

**Uncovered Code (10%):**

- Memory allocation failures (`calloc()` returns `NULL`)
- Debug driver stub functions with unused parameters
- Error handling paths that are difficult to simulate

```bash
# Test Coverage Analysis
make test-coverage # Generate detailed test coverage analysis report
```

## Git Hooks

### **Two Different Workflows**

**Code Quality Workflow** (`.github/workflows/code-quality.yml`):

- Triggers on: All C/H files, build configs, formatting configs
- Does: Code formatting checks, static analysis, linting
- Fast execution: ~2-3 minutes

**Device Tests Workflow** (`.github/workflows/device-tests.yml`):

- Triggers on: Driver files, test files only
- Does: Full device simulation testing across all supported hardware
- Longer execution: ~5-8 minutes

### **How Tests Work**

Tests use mock hidraw devices to simulate different G15 keyboards without requiring actual hardware. Each test validates device detection, RGB capability detection, and core functionality across all supported device types and compilers.

## Debug Scripts for Real Hardware

The `tests/debug/` directory contains Python scripts for debugging real G15/G510 keyboards:

- **debug_keys.py**: Captures and displays input events from G-keys and special keys
- **debug_rgb.py**: Monitors RGB backlight changes and HID raw events

Both require `sudo` and `python-evdev` package.
