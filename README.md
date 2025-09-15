# LCDproc Fork for Logitech G-Series Keyboards

This is a specialized fork from the [official LCDproc repository](https://github.com/lcdproc/lcdproc) optimized specifically for **supported Logitech G-Series keyboards** under Arch with Wayland.

## Supported Devices

### ✅ **Fully Supported:**

| Device                      | LCD Display | RGB Backlight | G-Keys | Configuration Status  |
| --------------------------- | ----------- | ------------- | ------ | --------------------- |
| **Logitech G15** (Original) | 160x43 mono | ❌            | ✅     | ✅ Tested             |
| **Logitech G15 v2**         | 160x43 mono | ❌            | ✅     | ✅ Tested             |
| **Logitech G510**           | 160x43 mono | ✅ RGB        | ✅     | ✅ Tested             |
| **Logitech G510s**          | 160x43 mono | ✅ RGB        | ✅     | ✅ **Primary Config** |

### ❌ **NOT Supported:**

| Device           | Reason                                                           | Alternative                  |
| ---------------- | ---------------------------------------------------------------- | ---------------------------- |
| **Logitech G19** | 320x240 **color** LCD requires completely different architecture | Use Logitech Gaming Software |
| **Logitech G13** | Different USB protocol and display specs                         | Community drivers available  |

### ⚠️ **Important Note:**

**This configuration is specifically optimized for the Logitech G510s.** Key mappings, RGB settings, and macro configurations may need adjustment for other supported devices.

## Introduction

LCDproc G15 is a client/server suite designed for **Logitech G-Series keyboards with 160x43 monochrome LCD displays**.
The G15 driver supports system information display and advanced RGB backlight control (G510/G510s only).

Various clients are available that display things like CPU load, system load, memory usage, uptime, and more directly on your G15's LCD screen.

The client and server communicate via TCP connection, allowing remote monitoring capabilities.

## Quick Start

```bash
# Install dependencies
sudo pacman -S clang make autoconf automake libg15 libg15render libusb libftdi-compat ydotool

# Build and compile in one command
make

# Install (optional)
sudo make install
```

Refer to [INSTALL.md](INSTALL.md) for detailed installation instructions, prerequisites and configuration

## Changes

This fork has been stripped down to include only the G15 driver and essential components.

## G-Key macro system

This fork includes an advanced G-Key macro recording and playback system. See [INSTALL.md](INSTALL.md#g-key-macro-system)

## RGB Backlight Control

Advanced RGB backlight management with dual control methods. See [INSTALL.md](INSTALL.md#rgb-backlight-configuration)

## Code Quality & Analysis

This project uses a **Clang-based toolchain** for consistent development experience.

For development setup see [CONTRIBUTING.md](CONTRIBUTING.md).

## Testing & Debugging

This implementation includes comprehensive unit tests and debugging tools for device handling, G-Key macro system and RGB functionality.

See [tests/README.md](tests/README.md) for detailed testing documentation.

## Code Coverage

**Current Coverage:** ~90% (635/702 lines)

For detailed coverage setup and analysis, see [tests/README.md](tests/README.md#coverage-analysis) section below.

## FAQ Section

You can find frequently asked questions [here](FAQ.md)

## Legal Stuff

LCDproc is Copyright (C) 1998-2016 William Ferrell, Selene Scriven and many
other contributors.

Changes in this fork are Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

The file COPYING contains the GNU General Public License. You should have
received a copy of the GNU General Public License along with this program; if
not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
Floor, Boston, MA 02110-1301, USA.
