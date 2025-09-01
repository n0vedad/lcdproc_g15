#!/usr/bin/env python3
# Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
"""
Debug script to monitor RGB backlight changes during button press
Also monitors HID raw events
Run with: sudo python3 debug_rgb.py
"""

import time
import os
import sys

LED_BRIGHTNESS = "/sys/class/leds/g15::kbd_backlight/brightness"
LED_COLOR = "/sys/class/leds/g15::kbd_backlight/color"
POWER_BRIGHTNESS = "/sys/class/leds/g15::power_on_backlight_val/brightness"
POWER_COLOR = "/sys/class/leds/g15::power_on_backlight_val/color"

def read_file(path):
    """Read file content safely"""
    try:
        with open(path, 'r') as f:
            return f.read().strip()
    except:
        return "ERROR"

def get_rgb_state():
    """Get current RGB state from all sources"""
    return {
        'kbd_brightness': read_file(LED_BRIGHTNESS),
        'kbd_color': read_file(LED_COLOR),
        'power_brightness': read_file(POWER_BRIGHTNESS),
        'power_color': read_file(POWER_COLOR),
        'timestamp': time.strftime('%H:%M:%S.%f')[:-3]
    }

def check_hidraw_devices():
    """Check for G15 HID raw devices"""
    hidraw_devices = []
    try:
        for device in os.listdir("/dev"):
            if device.startswith("hidraw"):
                device_path = f"/dev/{device}"
                hidraw_devices.append(device_path)
        return hidraw_devices
    except:
        return []

def monitor_rgb_changes():
    """Monitor RGB changes continuously"""
    print("RGB Backlight Monitor")
    print("====================")
    
    # Check HID devices
    hidraw_devs = check_hidraw_devices()
    print(f"Found HID raw devices: {hidraw_devs}")
    
    print("Press the RGB backlight button on your G15 keyboard")
    print("Press Ctrl+C to exit")
    print()
    
    # Get initial state
    last_state = get_rgb_state()
    print(f"Initial state at {last_state['timestamp']}:")
    for key, value in last_state.items():
        if key != 'timestamp':
            print(f"  {key}: {value}")
    print("-" * 50)
    
    try:
        while True:
            current_state = get_rgb_state()
            
            # Check for changes
            changes = []
            for key in ['kbd_brightness', 'kbd_color', 'power_brightness', 'power_color']:
                if current_state[key] != last_state[key]:
                    changes.append({
                        'param': key,
                        'old': last_state[key],
                        'new': current_state[key]
                    })
            
            if changes:
                print(f"CHANGE DETECTED at {current_state['timestamp']}:")
                for change in changes:
                    print(f"  {change['param']}: {change['old']} -> {change['new']}")
                print("-" * 50)
                last_state = current_state
            
            time.sleep(0.1)  # Check 10 times per second
            
    except KeyboardInterrupt:
        print("\nExiting...")

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("This script needs to be run as root (sudo)")
        sys.exit(1)
        
    monitor_rgb_changes()