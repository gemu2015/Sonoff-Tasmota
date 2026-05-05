#!/bin/bash
# Tasmota Merge — Terminal launcher.
#
# Use this INSTEAD of the .app bundle when your fork lives in
# ~/Desktop, ~/Documents or ~/Downloads. macOS TCC sandboxes
# Finder-launched .app bundles and silently denies file reads in
# those folders; Terminal-launched processes inherit Terminal.app's
# permissions (which the user typically grants once via Full Disk
# Access for development work).
#
# Double-click this file in Finder → opens Terminal → runs the
# server → opens the browser at http://127.0.0.1:8500/
#
# To stop: Ctrl-C in the Terminal window, or use Quit in the UI.

cd "$(dirname "$0")"
echo "Tasmota Merge — starting on http://127.0.0.1:8500/"
echo "Press Ctrl-C to stop."
echo ""
exec python3 tasmota_merge_server.py
