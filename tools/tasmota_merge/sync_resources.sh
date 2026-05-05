#!/bin/bash
# Sync the source-of-truth files (top-level .py + .html) into the .app
# bundle's Resources/ directory.
#
# Why copies and not symlinks: when Finder / Launch Services launches a
# .app, macOS treats reads through symlinks that point OUTSIDE the
# bundle as a sandbox / TCC violation, so Python's open() fails with
# "Operation not permitted" before the script can even start. Real
# copies inside Resources sidestep that entirely.
#
# Run this after editing tasmota_merge_server.py or tasmota_merge.html
# if you want the .app launch to pick up your changes. (Running the
# server directly via `python3 tasmota_merge_server.py` doesn't need
# this — that uses the top-level files directly.)

set -e
cd "$(dirname "$0")"
SRC_PY="tasmota_merge_server.py"
SRC_HTML="tasmota_merge.html"
DEST="Tasmota Merge.app/Contents/Resources"

if [ ! -d "$DEST" ]; then
    echo "ERROR: $DEST not found — run from tools/tasmota_merge/" >&2
    exit 1
fi
cp -p "$SRC_PY"    "$DEST/"
cp -p "$SRC_HTML"  "$DEST/"
echo "Synced:"
echo "  $SRC_PY    → $DEST/"
echo "  $SRC_HTML  → $DEST/"
