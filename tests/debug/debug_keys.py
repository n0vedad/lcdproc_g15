#!/usr/bin/env python3
# Copyright (C) 2025 n0vedad <https://github.com/n0vedad/>
"""
Debug script to capture and display input events from G15 keyboard
Run with: sudo python3 debug_keys.py
"""

import evdev
import sys
import time
import select

def find_all_devices():
    """Find ALL input devices for comprehensive monitoring"""
    devices = []
    print("Available input devices:")
    print("=" * 50)
    
    for path in evdev.list_devices():
        device = evdev.InputDevice(path)
        print(f"Device: {device.name}")
        print(f"  Path: {device.path}")
        print(f"  Vendor: 0x{device.info.vendor:04x}")
        print(f"  Product: 0x{device.info.product:04x}")
        print(f"  Capabilities: {device.capabilities(verbose=True)}")
        print("-" * 30)
        
        # Add all devices that have key capabilities
        if evdev.ecodes.EV_KEY in device.capabilities():
            devices.append(device)
            print(f"  -> MONITORING: {device.name}")
        
        print()
    
    print(f"Total devices to monitor: {len(devices)}")
    return devices

def monitor_events(devices):
    """Monitor input events from multiple devices"""
    print("\nMonitoring G15 input events...")
    print("Press keys on your G15 keyboard (Ctrl+C to exit)")
    print("-" * 60)
    
    try:
        while True:
            # Use select to monitor multiple devices
            r, w, x = select.select(devices, [], [])
            for device in r:
                try:
                    for event in device.read():
                        if event.type == evdev.ecodes.EV_KEY:
                            key_event = evdev.categorize(event)
                            if key_event.keystate == evdev.KeyEvent.key_down:
                                print(f"Device: {device.name}")
                                print(f"Key: {key_event.keycode} = {hex(event.code)} ({event.code})")
                                print(f"Time: {time.strftime('%H:%M:%S')}")
                                print("-" * 40)
                except BlockingIOError:
                    pass
                            
    except KeyboardInterrupt:
        print("\nExiting...")

def main():
    if len(sys.argv) > 1 and sys.argv[1] in ['-h', '--help']:
        print(__doc__)
        return
        
    print("G15 Keyboard Key Code Debug Tool")
    print("=" * 40)
    
    devices = find_all_devices()
    if not devices:
        print("No input devices found!")
        print("Make sure you have permissions to access input devices.")
        return
        
    monitor_events(devices)

if __name__ == "__main__":
    main()