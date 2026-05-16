#!/bin/bash
# Double-clickable launcher for the tc2plugin PoC.
# Opens the TinyC->plugin translator UI in your browser.
cd "$(dirname "$0")"
exec /usr/bin/env python3 tc2plugin_app.py
