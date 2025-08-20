# PREREQUISITES

## Quick Build for G15 Devices

First read the [README](README.md) if you haven't already.

In order to compile/install LCDproc, you'll need the following programs:

* A C compiler which supports C99, we recommend GCC. Most Linux or BSD systems
 come with GCC.

* GNU Make. It is available for all major distributions. If you want to
 compile it yourself, see http://www.gnu.org/software/make/make.html .

* The GNU autotools, that is automake and autoconf. They are only required
 if you want to build LCdproc directory from Git.
  The GNU autotools are available for all major distributions. If you want
  to compile them yourself, see
  http://www.gnu.org/software/autoconf/ and
  http://www.gnu.org/software/automake/.

* libftdi-compat which is a library to talk to FTDI chip, see https://www.intra2net.com/en/developer/libftdi/download.php

* libusb, see http://libusb.sourceforge.net/

* libg15 and libg15render (>= 1.1.1) for use with the g15 driver,
  see http://www.g15tools.com/

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

## Installation

### Install from PKGBUILD (Recommended)

```
# Clone this repository
git clone https://github.com/n0vedad/lcdproc-g15-arch.git
cd lcdproc-g15-arch

# Build and install package
makepkg -si
```
### Manual Build

If you prefer to build manually:
```
sh ./autogen.sh
./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input
make
make install (if you're root) 
```

### Clean Build

If you need to rebuild or reset the build environment:

```
# Clean compiled files only (normal rebuild)
make clean
make

# Complete reset (after configure/autotools changes)
make distclean
```

and run same steps from above.

#

If you want to know more read from here:

### Preparing a Git distro
If you retrieved these files from the Git, you will first need to run:
```
sh ./autogen.sh
```
### Configuration
The simplest way of doing it is with:
```
./configure --prefix=/usr --sbindir=/usr/bin --sysconfdir=/etc --enable-libusb --enable-lcdproc-menus --enable-stat-smbfs --enable-drivers=g15,linux_input
```
### Configure Options Explained

The configure command uses these G15-specific options:

```
./configure \
    --prefix=/usr \                   # Install to /usr
    --sbindir=/usr/bin \              # Server binaries to /usr/bin
    --sysconfdir=/etc \               # Configuration files to /etc
    --enable-lcdproc-menus \          # Enable menu support in lcdproc client
    --enable-drivers=g15,linux_input  # Build only G15 and linux_input drivers
```

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
cd lcdproc-g15-arch
sudo make uninstall
```

**Note:** `make uninstall` only works if you still have the original build directory with the same configure options.

# INPUT/OUTPUT

Only one Display/output device (g15.so) and input device (linux_input.so) is supported in this fork.

For LCDd (the server) to use the device, it needs to load a driver. The
drivers are so called 'shared modules', that usually have an extension
of ```.so```. The drivers to be loaded should be specified in the config file
(by one or more ```Driver=``` lines), or on the command line. The command line
should only be used to override things in the config file. The drivers
should be in a directory that is indicated by the ```DriverPath=``` line in
the configfile.

# RUNNING LCDPROC

## Systemd Service Management

After installation, manage the services with systemd:

```
# Start LCDd server (required)
sudo systemctl start lcdd.service
sudo systemctl enable lcdd.service

# Start system monitor client (required)
sudo systemctl start lcdproc.service
sudo systemctl enable lcdproc.service
```

## Running Manually

### Starting The Server

If you're in the LCDproc source directory, and have just built it, run:
```
server/LCDd -c path/to/config/file
```
For security reasons, LCDd by default only accepts connections from
localhost (```127.0.0.1```), it will not accept connections from other computers on
your network / the Internet. You can change this behaviour in the
configuration file.

### Starting The Client(s)

Then, you'll need some clients. LCDproc comes with a few, of which the
```lcdproc``` client is the main client:
```
clients/lcdproc/lcdproc -f  C M T L
```

This will run the LCDproc client, with the [C]pu, [M]emory,
[T]ime, and [L]oad screens.  The option ```-f``` causes it not to daemonize,
but run in the foreground.

By default, the client tries to connect to a server located on localhost
and listening to port 13666. To change this, use the -s and -p options.
