#!/bin/bash
# Double-clickable launcher for the Serial Monitor.
# Opens a browser-based serial console (large scrollback) for ESP devices.
cd "$(dirname "$0")"
exec /usr/bin/env python3 serial_monitor_server.py
