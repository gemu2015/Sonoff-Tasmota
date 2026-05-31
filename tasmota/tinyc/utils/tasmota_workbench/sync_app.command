#!/bin/bash
# Copy the canonical tasmota_workbench_server.py into the .app bundle so the
# Finder-launchable "Tasmota Workbench.app" runs the latest code.
# Needed because macOS TCC blocks a Finder-launched app from reading the repo
# on ~/Desktop, so the .app runs its own bundled copy.
# Double-click after editing tasmota_workbench_server.py. (Runs in Terminal,
# which already has Desktop access.)
cd "$(dirname "$0")" || exit 1
SRC="tasmota_workbench_server.py"
DST="Tasmota Workbench.app/Contents/Resources/tasmota_workbench_server.py"
mkdir -p "Tasmota Workbench.app/Contents/Resources"
cp -v "$SRC" "$DST" && echo "Synced. 'Tasmota Workbench.app' now runs the latest server."
