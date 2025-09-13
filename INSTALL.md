# PREREQUISITES

## Device Compatibility Check

**Before installation, verify your device is supported.** See [README.md](README.md#supported-devices) for the complete device compatibility list and USB IDs.

## Quick Build for Supported Devices

First read the [README.md](README.md) if you haven't already.

In order to compile/install LCDproc, you'll need the following programs:

- **Clang compiler** - This project defaults to Clang for toolchain consistency with
  code formatting and static analysis tools. GCC is also supported as fallback.

- GNU Make. It is available for all major distributions. If you want to
  compile it yourself, see http://www.gnu.org/software/make/make.html .

- The GNU autotools, that is automake and autoconf. They are only required
  if you want to build LCdproc directory from Git.
  The GNU autotools are available for all major distributions. If you want
  to compile them yourself, see
  http://www.gnu.org/software/autoconf/ and
  http://www.gnu.org/software/automake/.

- libftdi-compat which is a library to talk to FTDI chip, see https://www.intra2net.com/en/developer/libftdi/download.php

- libusb, see http://libusb.sourceforge.net/

- libg15 and libg15render (>= 1.1.1) for use with the g15 driver,
  see http://www.g15tools.com/

**Note:** For development workflow see [CONTRIBUTING.md](CONTRIBUTING.md) and [tests/README.md](tests/README.md).

### Install Required Dependencies:

**Essential Dependencies:**

```bash
# Core build tools
sudo pacman -S clang make autoconf automake

# G15 hardware support
sudo pacman -S libg15 libg15render libusb libftdi-compat

# G-Key macro system
sudo pacman -S ydotool
```

## Installation

### Install from PKGBUILD (Recommended)

```bash
# Clone this repository
git clone https://github.com/n0vedad/lcdproc-g15.git
cd lcdproc-g15

# Build and install package
makepkg -si
```

### Manual Build

#### **Standard Build Process**

```bash
# Simple one-command build (recommended)
make

# Or manual step-by-step:
make setup-autotools

# Standard configuration (production use)
./configure \
  --prefix=/usr \                # Install base directory
--sbindir=/usr/bin \             # Server binaries to /usr/bin (not /usr/sbin)
--sysconfdir=/etc \              # Configuration files to /etc
--enable-libusb \                # Enable libusb support for G15 device communication
--enable-lcdproc-menus \         # Enable menu support in lcdproc client
--enable-stat-smbfs \            # Enable Samba filesystem statistics
--enable-drivers=g15,linux_input # Build G15 and input drivers

# Build
make
```

If you only want to compile the clients, you can omit to compile the
server:

```bash
make clients
```

Similarly, if you only want to compile the server, you can omit to
compile the clients:

```bash
make server
```

#### **Installation**

```bash
make install # (if you're root)
```

### Clean Build

If you need to rebuild or reset the build environment:

```bash
# Clean compiled files only (normal rebuild)
make clean

# Complete reset (after configure/autotools changes)
make distclean
```

## Uninstall

### Remove PKGBUILD Installation

If you installed via `makepkg -si`:

```bash
# Remove the package (keeps configuration files)
sudo pacman -R lcdproc-g15

# Remove package including configuration files
sudo pacman -Rns lcdproc-g15
```

### Remove Manual Installation

If you installed via `make install`:

```bash
# Uninstall (from the build directory)
cd lcdproc-g15
sudo make uninstall
```

**Note:** `make uninstall` only works if you still have the original build directory with the same configure options.

# G-KEY MACRO SYSTEM

## Prerequisites for G-Key macro system

### Setup ydotool daemon (Wayland compatibility)

The installation process automatically installs, enables and starts ydotoold.service as a system service. No manual configuration required.

```bash
# Verify it's running (should be automatic)
systemctl status ydotoold.service
```

### Root Privileges for Input Recording

The G-Key macro system requires root privileges to access `/dev/input/event*` devices for real-time input recording. Besides to run with sudo privileges you have to add your current user to input group:

```bash
# Add your user to the input group, log out and back in or restart
sudo usermod -a -G input $USER
```

Once both LCDd server and lcdproc client are running with proper permissions:

### Recording a Macro

1. **Enter Recording Mode**: Press the `MR` key on your G15 keyboard
2. **Select Target G-Key**: Press any G-key (G1-G18) to start recording for that key
3. **Record Actions**: Type text, use mouse, or press keys - all input will be captured in real-time
4. **Stop Recording**: Press `MR` again to stop and save the macro

### Playing a Macro

- Simply press any G-key that has a recorded macro
- The system will replay your recorded actions with original timing

### Mode Switching

- Press `M1`, `M2`, or `M3` to switch between macro modes
- Each mode can store different macros for all 18 G-keys (54 total macros)

### Macro Storage

- Macros are automatically saved to `~/.config/lcdproc/g15_macros.json`
- Pauses and special keys are preserved as separate actions

# RGB BACKLIGHT CONFIGURATION

## Overview

The keyboard supports RGB backlight control through two different methods:

#### 1. **HID Reports Method** (Temporary)

- **Configuration**: `RGBMethod=hid_reports` in `/etc/LCDd.conf` writes to `/sys/class/leds/g15::kbd_backlight/`
- **Behavior**: RGB colors are written directly to keyboard RAM
- **Persistence**: **Non-persistent** - colors are lost on reboot/power cycle
- **Hardware Override**: Hardware buttons can easily override software colors

#### 2. **LED Subsystem Method** (Persistent) - **Default**

- **Configuration**: `RGBMethod=led_subsystem` in `/etc/LCDd.conf` writes to `/sys/class/leds/g15::power_on_backlight_val/`
- **Behavior**: RGB colors are written to keyboard firmware/EEPROM
- **Persistence**: **Persistent** - colors survive reboots and OS changes
- **Hardware Override**: Hardware settings are synchronized with software

# RUNNING LCDPROC

## Running Manually

### Starting The Server

If you're in the LCDproc source directory, and have just built it, run:

```bash
server/LCDd -c /etc/LCDd.conf
```

For security reasons, LCDd by default only accepts connections from localhost (`127.0.0.1`), it will not accept connections from other computers on your network / the Internet and the client tries to connect to a server located on localhost listening to port 13666. Fore more options use the `--help` flag.

You can change this behaviour in the configuration files.

### Starting The Client(s)

Then, you'll need some clients. LCDproc comes with a few, of which the `lcdproc` client is the main client:

```bash
clients/lcdproc/lcdproc -f C M T L
```

This will run the LCDproc client, for example with the [C]pu, [M]emory, [T]ime, and [L]oad screens.
The option `-f` causes it not to daemonize, but run in the foreground. Fore more options use the `--help` flag.

## Running automatically

### Systemd Service Management

After installation, manage the services with systemd:

```bash
# Start LCDd server (required)
sudo systemctl start lcdd.service
sudo systemctl enable lcdd.service

# Start system monitor client (required)
sudo systemctl start lcdproc.service
sudo systemctl enable lcdproc.service

# Start ydotool daemon (required for G-Key macros)
sudo systemctl start ydotoold.service
sudo systemctl enable ydotoold.service
```
