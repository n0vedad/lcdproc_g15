# PREREQUISITES

## Quick Build for G15 Devices

First read the [README](README.md) if you haven't already.

In order to compile/install LCDproc, you'll need the following programs:

- A C compiler which supports C99, we recommend GCC. Most Linux or BSD systems
  come with GCC.

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

**Note:** You can use optional automatic code formatting. This requires:

- `clang-format` for C code formatting
- `npm` for Markdown/JSON/Shell formatting (includes Node.js)

#

### Install Required Dependencies:

Build:

```
sudo pacman -S gcc make autoconf automake
```

G15 Device:

```
sudo pacman -S libg15 libg15render libusb libftdi-compat
```

G-Key Macro System:

```
sudo pacman -S ydotool
```

Code formatting:

```
sudo pacman -S clang npm
```

## Installation

### Install from PKGBUILD (Recommended)

```
# Clone this repository
git clone https://github.com/n0vedad/lcdproc-g15.git
cd lcdproc-g15

# Build and install package
makepkg -si
```

**Note:** PKGBUILD installation runs in **non-interactive mode** - code formatting setup is automatically skipped. After installation, developers can optionally enable formatting with:

```
# Enable code formatting for development
./setup-hooks.sh install
```

### Manual Build

If you prefer to build manually:

```
./autogen.sh
./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input
make
make install (if you're root)
```

**Interactive Code Formatting Setup:** `autogen.sh` runs in **interactive mode** and will ask if you want to enable automatic code formatting. This requires:

If you skip formatting setup or dependencies are missing, the build will continue normally. You can enable formatting later with `./setup-hooks.sh install`.

### Clean Build

If you need to rebuild or reset the build environment:

```
# Clean compiled files only (normal rebuild)
make clean
make

# Complete reset (after configure/autotools changes)
make distclean
```

and run same steps from above. For reconfigure code formatting please see [CODE FORMATTING](#code-formatting)

#

If you want to know more read from here:

### Preparing a Git distro

If you retrieved these files from the Git, you will first need to run:

```
./autogen.sh
```

### Configuration

The simplest way of doing it is with:

```
./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input
```

### Configure Options Explained

The usual configure commands to uses this G15-specific build:

```
./configure \
    --prefix=/usr \                         # Install base directory
    --sbindir=/usr/bin \                    # Server binaries to /usr/bin (not /usr/sbin)
    --sysconfdir=/etc \                     # Configuration files to /etc
    --enable-libusb \                       # Enable libusb support for G15 device communication
    --enable-lcdproc-menus \                # Enable menu support in lcdproc client
    --enable-stat-smbfs \                   # Enable Samba filesystem statistics
    --enable-debug                          # Enable Debug if needed
    --enable-drivers=g15,linux_input,debug  # Build available drivers
```

For more options like setting backlights color please refer to the specific config file (lcdexec.conf, lcdproc.conf, LCDd.conf)

But it may not do what you want, so please take a few seconds to type:

```
./configure --help
```

### Compilation

Run make to build the server and all clients

```
make
```

If you only want to compile the clients, you can omit to compile the
server:

```
make clients
```

Similarly, if you only want to compile the server, you can omit to
compile the clients:

```
make server
```

Depending on your system, LCDproc will build in a few seconds to a
few minutes. It's not very big.

If you want to, you can install it (if you're root) by typing:

```
make install
```

This will install the binaries and the man pages in the directory you
specified in configure.

## Uninstall

### Remove PKGBUILD Installation

If you installed via `makepkg -si`:

```
# Remove the package (keeps configuration files)
sudo pacman -R lcdproc-g15

# Remove package including configuration files
sudo pacman -Rns lcdproc-g15
```

### Remove Manual Installation

If you installed via `make install`:

```
# Uninstall (from the build directory)
cd lcdproc-g15
sudo make uninstall
```

**Note:** `make uninstall` only works if you still have the original build directory with the same configure options.

# INPUT/OUTPUT

Only one Display/output device (g15.so) and input device (linux_input.so) is supported in this fork.

For LCDd (the server) to use the device, it needs to load a driver. The
drivers are so called 'shared modules', that usually have an extension
of `.so`. The drivers to be loaded should be specified in the config file
(by one or more `Driver=` lines), or on the command line. The command line
should only be used to override things in the config file. The drivers
should be in a directory that is indicated by the `DriverPath=` line in
the configfile.

# RUNNING LCDPROC

## Prerequisites for G-Key Macro System

### Setup ydotool daemon (Wayland compatibility)

The installation process automatically installs, enables and starts ydotoold.service as a system service. No manual configuration required.

```
# Verify it's running (should be automatic)
systemctl status ydotoold.service
```

**Note:** The ydotoold.service is automatically installed as a system service to `/usr/lib/systemd/system/` and started during installation. G-Key macros work immediately after installation.

### Root Privileges for Input Recording

The G-Key macro system requires root privileges to access `/dev/input/event*` devices for real-time input recording. You have two options:

#### Option 1: Run with sudo (Recommended for testing)

```
sudo lcdproc -f -c /etc/lcdproc.conf
```

#### Option 2: Add user to input group (Permanent solution)

```
# Add your user to the input group
sudo usermod -a -G input $USER

# Log out and back in, then verify group membership
groups | grep input

# Now you can run without sudo
lcdproc -f -c /etc/lcdproc.conf
```

## Systemd Service Management

After installation, manage the services with systemd:

```
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

## Running Manually

### Starting The Server

If you're in the LCDproc source directory, and have just built it, run:

```
server/LCDd -c path/to/config/file
```

For security reasons, LCDd by default only accepts connections from
localhost (`127.0.0.1`), it will not accept connections from other computers on
your network / the Internet. You can change this behaviour in the
configuration file.

### Starting The Client(s)

Then, you'll need some clients. LCDproc comes with a few, of which the
`lcdproc` client is the main client:

```
clients/lcdproc/lcdproc -f  C M T L
```

This will run the LCDproc client, with the [C]pu, [M]emory,
[T]ime, and [L]oad screens. The option `-f` causes it not to daemonize,
but run in the foreground.

By default, the client tries to connect to a server located on localhost
and listening to port 13666. To change this, use the -s and -p options.

## G-KEY MACRO SYSTEM

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
- The system preserves timing between actions based on your recording speed
- Fast typing is consolidated into efficient `type:` commands
- Pauses and special keys are preserved as separate actions

### Example Workflow

```
# Start the system
sudo lcdproc -f -c /etc/lcdproc.conf

# In the terminal, you'll see:
# "KEY EVENT RECEIVED: MR" when you press MR
# "G-Key Macro: Recording started for G1 in mode M1"
# "G-Key Macro: Recorded key press: h" for each key
# "G-Key Macro: Recording stopped" when done
```

The macro system captures real keyboard and mouse input events directly from the Linux input subsystem, providing precise timing and comprehensive input recording for professional macro automation.

# CODE FORMATTING

## Automatic vs Manual Setup

You can choose about two installation methods - whatever fits your need!

#### 1. **PKGBUILD Installation** (Non-Interactive)

```
makepkg -si
```

- **Automatically skips** formatting setup
- **No user interaction** required
- **Minimal dependencies** - only installs runtime requirements
- **Optional dependencies** available for developers: `clang` and `npm`

#### 2. **Manual Build** (Interactive)

```
./autogen.sh
```

- **Asks interactively** about code formatting
- **Checks dependencies** and provides installation instructions
- **Graceful fallback** if dependencies are missing
- **Build continues** regardless of formatting choice

### Post-Installation Setup

After any installation method, developers can enable code formatting:

```
# Install formatting dependencies
sudo pacman -S clang npm

# Enable git hooks for automatic formatting
./setup-hooks.sh install

# Check status
./setup-hooks.sh status

# Format code manually
make format

# Check format status
make format-check
```
