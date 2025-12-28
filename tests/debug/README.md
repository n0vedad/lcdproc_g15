# Debug Tools for LCDproc-G15

This directory contains debugging and testing utilities for LCDproc-G15 development.

## Available Tools

### `lcdproc_debug.sh`

**Purpose:** Automated development workflow for testing LCDd and lcdproc with different report levels.

**Features:**

- Builds and installs debug binaries
- Restarts systemd services (LCDd, lcdproc)
- Monitors combined log output in real-time
- Supports multiple report/verbosity levels
- Shows GPL copyright banners at INFO and DEBUG levels
- Clean shutdown with Ctrl+C

**Usage:**

```bash
# From repository root
make debug              # Default: DEBUG level (all messages)
make debug-critical     # CRITICAL only
make debug-error        # ERROR and CRITICAL
make debug-warning      # WARNING, ERROR and CRITICAL
make debug-info         # INFO and above (shows GPL banner)

# Direct script usage
./tests/debug/lcdproc_debug.sh [OPTIONS]

Options:
  --critical, --crit, -c    Only critical errors (report level 0)
  --error, --errors, -e     Errors and critical (report level 1)
  --warning, --warnings, -w Warnings and above (report level 2)
  --info, -i                Info and above (report level 4, shows GPL banner)
  --debug, -d, --all, -a    All debug messages (report level 5, default)
```

**Report Levels:**

- `0` = CRITICAL - Only critical errors that cause program exit
- `1` = ERROR - Error messages and critical
- `2` = WARNING - Warnings, errors and critical
- `3` = NOTICE - Major events (driver loading, etc.)
- `4` = INFO - Minor events, configuration details, GPL banner
- `5` = DEBUG - Detailed function tracing and all messages

**Requirements:**

- Development build (configure with `--enable-debug`)
- Use `make dev` to set up development environment
- Root/sudo access for systemd service control

**Output:**

- Live monitoring in terminal
- Combined log file: `/tmp/lcdproc-dev-test.log`
- Individual logs: `/tmp/lcdd.log`, `/tmp/lcdproc.log`

---

### `debug_keys.py`

**Purpose:** Monitor and capture input events from G15 keyboard for debugging key mappings.

**Features:**

- Lists all available input devices
- Monitors G15 keyboard events in real-time
- Shows key codes, scan codes, and event types
- Helps identify correct device paths and key mappings

**Usage:**

```bash
sudo python3 tests/debug/debug_keys.py
```

**Requirements:**

- Python 3
- `python-evdev` package (`pip install evdev`)
- Root access (needed for `/dev/input/` access)

**Use Cases:**

- Verify G-key mappings
- Debug input event handling
- Identify device paths for configuration
- Test keyboard input driver

---

### `debug_rgb.py`

**Purpose:** Monitor RGB backlight changes and LED states on G15 keyboard.

**Features:**

- Real-time monitoring of RGB backlight values
- Tracks keyboard backlight brightness and color
- Monitors power LED brightness and color
- Displays changes during button presses
- Shows HID raw events

**Usage:**

```bash
sudo python3 tests/debug/debug_rgb.py
```

**Requirements:**

- Python 3
- Root access (needed for sysfs LED control)
- G15 keyboard with RGB backlight support

**Monitored Paths:**

- `/sys/class/leds/g15::kbd_backlight/brightness`
- `/sys/class/leds/g15::kbd_backlight/color`
- `/sys/class/leds/g15::power_on_backlight_val/brightness`
- `/sys/class/leds/g15::power_on_backlight_val/color`

**Use Cases:**

- Debug RGB backlight driver
- Verify LED control functionality
- Test color and brightness changes
- Monitor backlight state during operation

---

## Development Workflow

**Typical debugging session:**

1. **Set up development environment:**

   ```bash
   make dev
   ```

2. **Start debugging at desired level:**

   ```bash
   make debug-info # Shows GPL banner and important events
   ```

3. **Monitor specific hardware features:**

   ```bash
   # In another terminal
   sudo python3 tests/debug/debug_keys.py # Monitor key events
   sudo python3 tests/debug/debug_rgb.py  # Monitor RGB changes
   ```

4. **Stop debugging:**
   - Press `Ctrl+C` in the debug session terminal
   - Processes are stopped cleanly
   - Development mode remains active

5. **Resume debugging:**

   ```bash
   make debug # Resumes with same debug build
   ```

6. **Return to production:**
   ```bash
   make distclean && make
   ```

---

## Tips

- **Log Files:** All output is saved to `/tmp/lcdproc-dev-test.log` for later analysis
- **Clean Logs:** The debug script clears old logs before starting
- **Background Monitoring:** You can tail the individual log files while debugging:
  ```bash
  tail -f /tmp/lcdd.log
  tail -f /tmp/lcdproc.log
  ```
- **Python Scripts:** Both Python scripts require root for hardware access
- **Report Levels:** Start with `--info` to see important messages without debug spam
- **GPL Banner:** Only visible at INFO (level 4) and DEBUG (level 5)

---

## Troubleshooting

**"Development build required" error:**

- Run `make distclean && make dev` to enable debug mode

**No log output visible:**

- Check `/tmp/lcdd.log` and `/tmp/lcdproc.log` exist
- Verify processes are running: `ps aux | grep -E 'LCDd|lcdproc'`

**Python scripts fail:**

- Install `python-evdev`: `pip install evdev`
- Ensure running with `sudo` for hardware access
- Check G15 kernel driver is loaded: `lsmod | grep lg4ff`

**Services won't restart:**

- Check systemd status: `systemctl status LCDd lcdproc`
- Verify service files exist in `/etc/systemd/system/`
- Run `sudo systemctl daemon-reload`

---

## Author

Debug tools maintained by n0vedad and contributors.

See individual files for copyright and license information (GPL-2.0+).
