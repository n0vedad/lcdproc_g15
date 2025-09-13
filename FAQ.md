# Frequently Asked Questions

## Device Compatibility

**Q: Which devices are supported?**  
A: See the complete device compatibility list in [README.md](README.md#supported-devices) with USB IDs and feature details.

**Q: Is this optimized for a specific device?**  
A: Yes, specifically optimized for the **Logitech G510s**. Other supported devices may require configuration adjustments.

## Installation & Build

**Q: Why does makepkg skip the interactive setup?**  
A: Package builds run in isolated environments without terminal access. Interactive prompts would cause the build to fail.

**Q: Which installation method should I use?**  
A: See [INSTALL.md](INSTALL.md) for detailed installation instructions. Use PKGBUILD for regular users, manual build for development.

**Q: What dependencies do I need?**  
A: See [INSTALL.md](INSTALL.md#install-required-dependencies) for complete dependency information.

**Q: Can I build without all dependencies?**  
A: Yes! The build system gracefully handles missing optional dependencies like `clang` and `npm` - the project will build successfully without them.

## Code Formatting

**Q: Can I use code formatting without git hooks?**  
A: Yes! Use `make format` to format code manually anytime.

**Q: What happens if I don't have clang or npm?**  
A: The build will continue normally. Formatting is completely optional for building the project.

**Q: How do I enable code formatting after installation?**  
A: Install dependencies (`sudo pacman -S clang npm`) then run `make setup-hooks-install`. This will automatically install prettier dependencies and git hooks.

**Q: How do I check if code formatting is active?**  
A: Run `make setup-hooks-status` or `make format-check`.

**Q: How do I run static analysis on the code?**  
A: Use `make lint` to run clang-tidy static analysis. Use `make lint-fix` to auto-fix issues where possible, or `make lint-check` for analysis without changes.

**Q: What static analysis tools are used?**  
A: clang-tidy for comprehensive C code analysis including security, performance, and modernization checks. Configuration is in `.clang-tidy`.

## G-Key macro system

**Q: Why do I need root privileges for macros?**  
A: The macro system requires access to `/dev/input/event*` devices for real-time input recording. Add your user to the `input` group: `sudo usermod -a -G input $USER` (requires logout/login).

**Q: Do G-Key macros work on Wayland?**  
A: Yes! The system uses `ydotool` for Wayland compatibility. The `ydotoold.service` is automatically installed and started.

**Q: How many macros can I record?**  
A: 54 total macros (18 G-keys × 3 modes: M1, M2, M3).

**Q: Where are macros stored?**  
A: In `~/.config/lcdproc/g15_macros.json` with automatic saving.

**Q: Are G-Key macros device-specific?**  
A: Yes! This macro implementation is optimized for **G510s** key layout. Other supported devices (G15/G15v2/G510) may have different key mappings and require manual adjustment.

**Q: What gets recorded in macros?**  
A: Keyboard input, mouse clicks, mouse movements, and timing between actions. Fast typing is automatically consolidated into efficient commands.

## RGB Backlight Control

**Q: What RGB control methods are available?**  
A: Two methods: `hid_reports` (temporary) and `led_subsystem` (persistent). Configure via `RGBMethod` in `/etc/LCDd.conf`.

**Q: Which RGB method should I choose?**  
A: Use `led_subsystem` (default) for persistent colors. Use `hid_reports` for temporary colors. See [INSTALL.md](INSTALL.md#rgb-backlight-configuration) for technical details.

**Q: How do I change RGB colors?**  
A: Set `BacklightRed`, `BacklightGreen`, `BacklightBlue` values (0-255) in `/etc/LCDd.conf`, then restart `lcdd.service`.

**Q: How can I test device detection without physical hardware?**  
A: Run the included unit tests: `make check` (basic) or `cd tests && make test-full` (comprehensive). The test system simulates all supported devices (G15/G510/G510s) and validates RGB functionality. See [`tests/README.md`](tests/README.md) for details.

## System Services

**Q: How do I start LCDproc services?**  
A: Use systemd: `sudo systemctl start lcdd.service` (server) and `sudo systemctl start lcdproc.service` (client). Enable for auto-start: `sudo systemctl enable lcdd.service lcdproc.service`.

**Q: Can I run LCDproc manually?**  
A: Yes! Start server: `sudo /usr/bin/LCDd -c /etc/LCDd.conf`. Start client: `lcdproc -f C M T L` (CPU, Memory, Time, Load screens).

**Q: Why isn't my G-Series LCD working?**  
A: First verify your device is supported (`lsusb | grep -i logitech`). Then check dependencies are installed, `lcdd.service` is running, and device is connected properly. Check logs: `journalctl -u lcdd.service`.

## Troubleshooting

**Q: How do I clean/rebuild the project?**  
A: For normal rebuild: `make clean && make`. For complete reset after configure changes: `make distclean` then re-run `./configure` and `make`.

**Q: Can I uninstall LCDproc?**  
A: PKGBUILD: `sudo pacman -R lcdproc-g15` (or `-Rns` to include configs). Manual: `sudo make uninstall` from the original build directory.

**Q: Where are the configuration files?**  
A: Main configs in `/etc/`: `LCDd.conf` (server), `lcdproc.conf` (client), `lcdexec.conf` (executor). User macros: `~/.config/lcdproc/g15_macros.json`.
