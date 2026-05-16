#!/bin/bash
# Double-clickable launcher for the native2dual PoC.
# Opens the native-driver -> BinPlugin scaffolder UI in your browser.
cd "$(dirname "$0")"
exec /usr/bin/env python3 native2dual_app.py
