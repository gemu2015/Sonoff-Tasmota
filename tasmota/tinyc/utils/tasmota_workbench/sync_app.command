#!/bin/bash
# Copy the canonical tasmota_workbench_server.py into the .app bundle(s) so the
# Finder-launchable "Tasmota Workbench.app" runs the latest code.
# Needed because macOS TCC blocks a Finder-launched app from reading the repo
# on ~/Desktop, so the .app runs its own bundled copy.
#
# Syncs both:
#   1. The in-tree .app (this directory)  — always.
#   2. ~/Desktop/Tasmota Workbench.app    — if it exists (the convenient
#      double-click location next to your other Tasmota tools).
#
# Double-click after editing tasmota_workbench_server.py. (Runs in Terminal,
# which already has Desktop access.)
cd "$(dirname "$0")" || exit 1
SRC="tasmota_workbench_server.py"
sync_to() {
  local app="$1"
  [ -d "$app" ] || return 0
  local dst="$app/Contents/Resources/tasmota_workbench_server.py"
  mkdir -p "$app/Contents/Resources"
  cp -v "$SRC" "$dst" && echo "  → synced into '$app'"
}
sync_to "Tasmota Workbench.app"
sync_to "$HOME/Desktop/Tasmota Workbench.app"
echo "Done."
