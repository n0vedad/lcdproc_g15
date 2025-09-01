# LCDproc Fork for G15 Devices

This is a specialized fork from the [official LCDproc repository](https://github.com/lcdproc/lcdproc) optimized specifically for Logitech G15 keyboards under Arch with Wayland.

## Introduction

LCDproc G15 is a client/server suite designed exclusively for Logitech G15 keyboards with integrated LCD displays.
The G15 driver supports the 160x43 pixel monochrome LCD display found on Logitech G15 keyboards, providing system information display capabilities.

Various clients are available that display things like CPU load, system load, memory usage, uptime, and more directly on your G15's LCD screen.

The client and server communicate via TCP connection, allowing remote monitoring capabilities.

## Installation

Refer to [INSTALL](INSTALL.md) (included with this archive) for installation
instructions (including how to connect an LCD to your system) and Prerequesites.

## Changes

This fork has been stripped down to include only the G15 driver and essential components.

## G-Key Macro System

This fork includes an advanced G-Key macro recording and playback system with the following capabilities:

- **Keyboard and mouse event parsing** with Linux input subsystem integration
- **Intelligent device detection** - automatically identifies and opens keyboard/mouse devices
- **Wayland compatibility** through ydotool integration
- **Real-time keyboard input capture** - records actual keystrokes as you type
- **Mouse event recording** - captures clicks and relative mouse movements
- **Multi-mode support** - separate macro sets for M1/M2/M3 modes
- **Persistent storage** - macros saved in JSON format at `~/.config/lcdproc/g15_macros.json`

## Code Formatting

This project supports **optioal automatic code formatting** for developers to maintain consistent code style. For more information please refer to [CODE FORMATTING](installation.md#code-formatting)

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
