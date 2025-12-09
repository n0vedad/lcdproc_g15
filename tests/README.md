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
make check # Basic unit tests

# Before commits (thorough validation)
make test-full # Unit tests + memory leak detection
# Non-destructive, safe for daily use

# Before releases (complete validation)
make test-ci # Full + multi-compiler testing
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
make test-memcheck  # Sanitizer checks (ASan, LSan, UBSan)
make test-compilers # Multi-compiler build testing only
# ⚠️ DESTRUCTIVE: Rebuilds with clang + gcc
```

**Understanding Test Targets:**

All unit test targets execute the same binary (`test_unit_g15`) with different command-line flags:

- **`test-g15`/`test-g510`**: Filter tests by hardware type (useful for hardware-specific debugging)
- **`test-scenario-*`**: Run isolated test categories (device detection, RGB, macros, failures)
- **`test-scenarios`**: Convenience wrapper that runs all four `test-scenario-*` targets sequentially
- **`test-verbose`**: Runs all tests with detailed progress output (same coverage as `make check`)
- **Coverage**: Full test suite (`make check`) achieves 91%+ code coverage (19 tests)

**Sanitizers Active:**

Tests are compiled with three sanitizers for comprehensive error detection:

- **ASan** (AddressSanitizer): Detects memory errors (buffer overflows, use-after-free, double-free)
- **LSan** (LeakSanitizer): Detects memory leaks
- **UBSan** (Undefined Behavior Sanitizer): Detects undefined behavior (integer overflow, null deref, division by zero)

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

## ThreadSanitizer (TSan) - Race Condition Detection

ThreadSanitizer is a dynamic analysis tool that detects data races at runtime. It's particularly useful for the G-Key macro system which uses pthread for input recording.

**When to use TSan:**

- Testing the G-Key macro recording functionality
- Debugging potential race conditions in multi-threaded code
- Verifying thread safety of shared data structures
- Before releasing new versions with threading changes

**Running TSan:**

```bash
# Full TSan build and test (requires rebuild)
make test-tsan

# Quick test on existing TSan build
make test-tsan-quick

# Manual testing with TSan-enabled binary
./clients/lcdproc/lcdproc -f # Run in foreground to see TSan output
```

**Important Notes:**

- Reports race conditions to stderr with detailed stack traces
- Requires full rebuild with `-fsanitize=thread` flag
- Only detects actual races that occur during execution (not potential ones)

## GitHub Actions CI/CD Workflows

### **Three Comprehensive Workflows**

**1. Code Quality Workflow** (`.github/workflows/code-quality.yml`):

- **Triggers on**: All C/H files, build configs, formatting configs
- **Jobs**:
  - `formatting`: Code formatting checks (clang-format, prettier optional)
  - `static-analysis`: clang-tidy static analysis
  - `arch-build-test`: Production build + comprehensive CI tests
  - `coverage-analysis`: Code coverage reports with artifact upload ✨ NEW
- **Execution time**: ~3-5 minutes
- **Artifacts**: Coverage reports (HTML, XML, gcov) - 30 days retention

**2. Device Tests Workflow** (`.github/workflows/device-tests.yml`):

- **Triggers on**: Driver files, test files only
- **Jobs**:
  - `device-simulation-tests`: Matrix testing (5 devices × 2 compilers)
  - `integration-test`: Scenarios + multi-compiler validation
- **Execution time**: ~5-8 minutes
- **Coverage**: All G15/G510 device variants with RGB validation

**3. Integration Tests Workflow** (`.github/workflows/integration-tests.yml`): ✨ NEW

- **Triggers on**: Server, client, and test code changes
- **Jobs**:
  - `integration-basic`: LCDd server, mock, clients, e2e tests
  - `thread-safety`: ThreadSanitizer for race condition detection
  - `scenario-matrix`: Individual test scenarios (detection, RGB, macros, failures)
- **Execution time**: ~8-12 minutes
- **Coverage**: End-to-end workflows, thread safety, isolated scenarios

### **Complete CI Test Matrix**

| Workflow | Test Type | Coverage |
|----------|-----------|----------|
| Code Quality | Formatting | ✅ clang-format, prettier (optional) |
| Code Quality | Static Analysis | ✅ clang-tidy (100+ checks) |
| Code Quality | Coverage Reports | ✅ HTML/XML with artifacts |
| Device Tests | Unit Tests | ✅ 5 devices × 2 compilers |
| Device Tests | RGB Validation | ✅ Device-specific RGB support |
| Integration Tests | Server Tests | ✅ LCDd, mock, clients, e2e |
| Integration Tests | Thread Safety | ✅ TSan for macro system |
| Integration Tests | Scenarios | ✅ Detection, RGB, macros, failures |

### **How Tests Work**

Tests use mock hidraw devices to simulate different G15 keyboards without requiring actual hardware. Each test validates device detection, RGB capability detection, and core functionality across all supported device types and compilers.

## Debug Scripts for Real Hardware

The `tests/debug/` directory contains Python scripts for debugging real G15/G510 keyboards:

- **debug_keys.py**: Captures and displays input events from G-keys and special keys
- **debug_rgb.py**: Monitors RGB backlight changes and HID raw events

Both require `sudo` and `python-evdev` package.
