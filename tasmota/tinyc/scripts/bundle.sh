#!/bin/bash
# Bundle TinyC IDE into a single HTML file for ESP32 device upload
# Usage: ./bundle.sh [--gzip]
set -e
cd "$(dirname "$0")"
python3 bundle.py "$@"
