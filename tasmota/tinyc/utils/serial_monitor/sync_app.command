#!/bin/bash
# Copy the canonical serial_monitor_server.py into the .app bundle so
# the Finder-launchable "Serial Monitor.app" runs the latest code.
# Needed because macOS TCC blocks a Finder-launched app from reading
# the repo on ~/Desktop, so the .app runs its own bundled copy.
# Double-click after editing serial_monitor_server.py. (Runs in
# Terminal, which already has Desktop access.)
cd "$(dirname "$0")" || exit 1
SRC="serial_monitor_server.py"
DST="Serial Monitor.app/Contents/Resources/serial_monitor_server.py"
mkdir -p "Serial Monitor.app/Contents/Resources"
cp -v "$SRC" "$DST" && echo "Synced. 'Serial Monitor.app' now runs the latest server."
