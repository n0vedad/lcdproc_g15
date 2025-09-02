# Frequently Asked Questions

## Installation & Build

**Q: Why does makepkg skip the interactive setup?**  
A: Package builds run in isolated environments without terminal access. Interactive prompts would cause the build to fail.

**Q: What's the difference between PKGBUILD and manual installation?**  
A: PKGBUILD installation (`makepkg -si`) is **non-interactive** and automatically skips code formatting setup. Manual build (`./autogen.sh`) runs **interactively** and asks about code formatting preferences.

**Q: Which installation method should I use?**  
A: Use **PKGBUILD** (`makepkg -si`) for regular users. Use **manual build** for developers who want code formatting setup.

**Q: What dependencies do I need for G15 support?**  
A: Essential: `libg15`, `libg15render`, `libusb`, `libftdi-compat`. For G-Key macros: `ydotool`. For development: `clang`, `npm`.

**Q: Can I build without all dependencies?**  
A: Yes! The build system gracefully handles missing optional dependencies like `clang` and `npm` - the project will build successfully without them.

## Code Formatting

**Q: Can I use code formatting without git hooks?**  
A: Yes! Use `make format` to format code manually anytime.

**Q: What happens if I don't have clang or npm?**  
A: The build will continue normally. Formatting is completely optional for building the project.

**Q: How do I enable code formatting after installation?**  
A: Install dependencies (`sudo pacman -S clang npm`) then run `./setup-hooks.sh install`. This will automatically install prettier dependencies and git hooks.

**Q: How do I check if code formatting is active?**  
A: Run `./setup-hooks.sh status` or `make format-check`.

**Q: How do I run static analysis on the code?**  
A: Use `make lint` to run clang-tidy static analysis. Use `make lint-fix` to auto-fix issues where possible, or `make lint-check` for analysis without changes.

**Q: What static analysis tools are used?**  
A: clang-tidy for comprehensive C code analysis including security, performance, and modernization checks. Configuration is in `.clang-tidy`.

## G-Key Macro System

**Q: Why do I need root privileges for macros?**  
A: The macro system requires access to `/dev/input/event*` devices for real-time input recording. Add your user to the `input` group: `sudo usermod -a -G input $USER` (requires logout/login).

**Q: Do G-Key macros work on Wayland?**  
A: Yes! The system uses `ydotool` for Wayland compatibility. The `ydotoold.service` is automatically installed and started.

**Q: How many macros can I record?**  
A: 54 total macros (18 G-keys × 3 modes: M1, M2, M3).

**Q: Where are macros stored?**  
A: In `~/.config/lcdproc/g15_macros.json` with automatic saving.

**Q: What gets recorded in macros?**  
A: Keyboard input, mouse clicks, mouse movements, and timing between actions. Fast typing is automatically consolidated into efficient commands.

## RGB Backlight Control

**Q: What RGB control methods are available?**  
A: Two methods: `hid_reports` (temporary) and `led_subsystem` (persistent). Configure via `RGBMethod` in `/etc/LCDd.conf`.

**Q: What's the difference between HID reports and LED subsystem?**  
A: **HID reports** (`RGBMethod=hid_reports`): Temporary RGB colors, lost on reboot/power cycle. **LED subsystem** (`RGBMethod=led_subsystem`): Persistent RGB colors stored in hardware, survive reboots and OS changes.

**Q: Which RGB method should I choose?**  
A: Use `led_subsystem` (default) for persistent colors that survive reboots. Use `hid_reports` for temporary colors or if you want manual hardware control.

**Q: Can hardware buttons override software RGB settings?**  
A: With `hid_reports`: Yes, hardware buttons can easily override software colors. With `led_subsystem`: Hardware settings are synchronized with software settings.

**Q: How do I change RGB colors?**  
A: Set `BacklightRed`, `BacklightGreen`, `BacklightBlue` values (0-255) in `/etc/LCDd.conf`, then restart `lcdd.service`.

## System Services

**Q: How do I start LCDproc services?**  
A: Use systemd: `sudo systemctl start lcdd.service` (server) and `sudo systemctl start lcdproc.service` (client). Enable for auto-start: `sudo systemctl enable lcdd.service lcdproc.service`.

**Q: Can I run LCDproc manually?**  
A: Yes! Start server: `sudo /usr/bin/LCDd -c /etc/LCDd.conf`. Start client: `lcdproc -f C M T L` (CPU, Memory, Time, Load screens).

**Q: Why isn't my G15 LCD working?**  
A: Verify dependencies are installed, check that `lcdd.service` is running, and ensure your G15 is connected properly. Check logs: `journalctl -u lcdd.service`.

## Troubleshooting

**Q: How do I clean/rebuild the project?**  
A: For normal rebuild: `make clean && make`. For complete reset after configure changes: `make distclean` then re-run `./configure` and `make`.

**Q: Can I uninstall LCDproc?**  
A: PKGBUILD: `sudo pacman -R lcdproc-g15` (or `-Rns` to include configs). Manual: `sudo make uninstall` from the original build directory.

**Q: Where are the configuration files?**  
A: Main configs in `/etc/`: `LCDd.conf` (server), `lcdproc.conf` (client), `lcdexec.conf` (executor). User macros: `~/.config/lcdproc/g15_macros.json`.
