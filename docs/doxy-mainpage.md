# The LCDproc-G15 Documentation {#mainpage}

Welcome to the LCDproc-G15 documentation! This page serves as the central hub for all project documentation.

## ToDo Categories

Browse ToDos organized by priority:

- \ref ToDo_critical "🔴 Critical Priority ToDos" - High-priority issues requiring immediate attention
- \ref ToDo_medium "🟡 Medium Priority ToDos" - Medium-priority improvements and missing features
- \ref ToDo_low "🟢 Low Priority ToDos" - Low-priority enhancements and nice-to-have features

## Documentation Types

### API Reference (This Site)

This Doxygen-generated documentation provides **complete API reference** for developers:

- **Functions**: All public and static functions with parameters and return values
- **Data Structures**: Structs, enums, and type definitions
- **Source Code Browser**: Cross-referenced source code with syntax highlighting
- **Call Graphs**: Function dependencies and relationships
- **File Documentation**: Purpose and features of each source file

Use the navigation menu above to browse by **Files**, **Data Structures**, or **Functions**.

### User Documentation

For installation, configuration, and usage instructions, see these markdown files in the repository root:

- **README.md**: Project overview, features, and quick start guide
- **INSTALL.md**: Detailed installation instructions for Arch Linux and other systems
- **CONTRIBUTING.md**: Development setup, coding standards, testing, and contribution guidelines
- **FAQ.md**: Frequently asked questions and troubleshooting
- **tests/README.md** - Detailed testing documentation and usage guide

## Project Overview

LCDproc-G15 is a specialized fork of LCDproc that provides enhanced support for
Logitech G15 keyboards with LCD displays and macro keys:

- **G15 Driver Support**: Native support for G15/G510/G510s keyboard LCD displays
- **Macro Key Integration**: Full G-Key functionality with recording and playback
- **RGB LED Control**: Support for G15 keyboard RGB backlight control
- **Enhanced Input Handling**: Improved keyboard input processing with ydotool integration
- **Thread-Safe Design**: Modern C code with pthread support for macro recording

## Key Components

### Client Applications (`clients/`)

- **lcdproc**: System monitor client (CPU, memory, disk, network, G-Key macros)
- **lcdexec**: Execute commands from LCD menu selections

### Server (`server/`)

- **LCDd**: Core daemon managing LCD displays and client connections
- **G15 Driver**: Specialized driver for G15 keyboard hardware
- **Input Driver**: Linux input event handling for keyboard events
- **Debug Driver**: Virtual LCD for testing without hardware

### Shared Libraries (`shared/`)

- **Sockets**: Network communication utilities
- **Configuration**: Config file parsing
- **Reporting**: Logging and error reporting
- **Environment**: Thread-safe environment variable caching

### Tests (`tests/`)

- **Unit Tests**: Component testing with mock hardware
- **Integration Tests**: Full system testing
- **Debug Tools**: Python scripts for hardware analysis

## Links

- **Original LCDproc**: http://www.lcdproc.org/
- **G15 Fork Repository**: https://github.com/n0vedad/lcdproc-g15/
- **Issue Tracker**: https://github.com/n0vedad/lcdproc-g15/issues
