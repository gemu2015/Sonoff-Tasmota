#!/bin/bash
# Plugin Builder — double-click launcher for tasmota/Plugins/build_plugin.py --gui
#
# Behaviour: cd's into the Tasmota source tree, fires the Tk GUI, and
# closes the Terminal window when the builder exits cleanly. If Python
# or the script can't be found you get a visible error instead of a
# silent flash.

set -e

REPO="/Users/gerhardmutz1/Desktop/Smart-Home/Tasmota/Development/Sonoff-Tasmota"
SCRIPT="$REPO/tasmota/Plugins/build_plugin.py"

if [ ! -f "$SCRIPT" ]; then
  echo "ERROR: build_plugin.py not found at:"
  echo "  $SCRIPT"
  echo "Repo path may have moved — edit this .command file and update REPO=…"
  read -p "Press Enter to close…"
  exit 1
fi

cd "$REPO"
exec /usr/bin/env python3 "$SCRIPT" --gui
