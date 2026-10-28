# Frequently Asked Questions

## Device Compatibility

**Q: Which devices are supported?**  
A: See the complete device compatibility list in [README.md](README.md#supported-devices) with USB IDs and feature details.

**Q: Is this optimized for a specific device?**  
A: Yes, specifically optimized for the **Logitech G510s**. Other supported devices may require configuration adjustments.

## Installation & Build

**Q: Which installation method should I use?**  
A: See [INSTALL.md](INSTALL.md) for detailed installation instructions. Use PKGBUILD for regular users, manual build for development.

**Q: What dependencies do I need?**  
A: See [INSTALL.md](INSTALL.md#install-required-dependencies) for complete dependency information.

## G-Key macro system

**Q: Do I need root privileges or the `input` group for macros?**  
A: Not with the services: `lcdproc.service` runs the client with the group `input`, which it needs to read `/dev/input/event*` for recording. Only when you run the `lcdproc` client manually as your own user, add that user to `input` (see [INSTALL.md](INSTALL.md#input-device-access-for-recording)).

**Q: Do G-Key macros work on Wayland?**  
A: Yes! The system uses `ydotool` for Wayland compatibility. The `ydotoold.service` is automatically installed and started.

**Q: How many macros can I record?**  
A: 54 total macros (18 G-keys × 3 modes: M1, M2, M3).

**Q: Where are macros stored?**
A: In `/var/lib/lcdproc/.config/lcdproc/g15_macros.json` with automatic saving (system-wide storage).

**Q: Are G-Key macros device-specific?**  
A: Yes! This macro implementation is optimized for **G510s** key layout. Other supported devices (G15/G15v2/G510) may have different key mappings and require manual adjustment.

**Q: What gets recorded in macros?**  
A: Due the limitations of Wayland only keyboard input will be recorded.

## RGB Backlight Control

**Q: What RGB control methods are available?**  
A: Two methods: `hid_reports` (temporary) and `led_subsystem` (persistent). Configure via `RGBMethod` in `/etc/LCDd.conf`.

**Q: Which RGB method should I choose?**  
A: Use `led_subsystem` (default) for persistent colors. Use `hid_reports` for temporary colors. See [INSTALL.md](INSTALL.md#rgb-backlight-configuration) for technical details.

**Q: How do I change RGB colors?**  
A: Set `BacklightRed`, `BacklightGreen`, `BacklightBlue` values (0-255) in `/etc/LCDd.conf`, then restart services or reboot.

## System Services

**Q: How do I start LCDproc services?**  
A: They should start automatically. But for manual testing see [INSTALL.md](INSTALL.md#running-automatically)

**Q: Can I run LCDproc manually without services?**  
A: Yes! Please refer to [INSTALL.md](INSTALL.md#running-manually)

**Q: Why isn't my G-Series LCD working?**  
A: First verify your device is supported (`lsusb | grep -i logitech`). Then check dependencies are installed, `lcdd.service` is running, and device is connected properly. Check logs: `journalctl -u lcdd.service lcdproc.service`.

**Q: Does LCDd run as root?**  
A: No. `lcdd.service` starts it as the system user `lcdd`, which gets access to the keyboard through a udev rule. See [INSTALL.md](INSTALL.md#permissions).

**Q: LCDd stops with `Refusing to run with root privileges, abort.` What now?**  
A: LCDd was started as root and could not switch to the user set by `User=` in `LCDd.conf`, e.g. because the user does not exist or a systemd drop-in removed `CAP_SETUID`/`CAP_SETGID` (`CapabilityBoundingSet=`). Older versions silently kept running as root in this case. Start LCDd through `lcdd.service`, which does not need to switch users, or fix the user in `LCDd.conf`.

## Development Workflow

**Q: Is there developer support for this project**
A: Of course it is! Please check [CONTRIBUTING.md](CONTRIBUTING.md) and [tests/README.md](tests/README.md) for details.

## Troubleshooting

**Q: How do I clean/rebuild the project?**  
A: For normal rebuild: `make clean && make`. For complete reset after configure changes: `make distclean` then re-run `make`.

**Q: Can I uninstall LCDproc?**  
A: PKGBUILD: `sudo pacman -R lcdproc-g15` (or `-Rns` to include configs). Manual: `sudo make uninstall` from the original build directory.

**Q: Where are the configuration files?**
A: Main configs in `/etc/`: `LCDd.conf` (server), `lcdproc.conf` (client), `lcdexec.conf` (executor). Macros: `/var/lib/lcdproc/.config/lcdproc/g15_macros.json`.
