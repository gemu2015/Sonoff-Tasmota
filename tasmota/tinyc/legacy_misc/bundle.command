#!/bin/bash
# Double-click this file in Finder to rebuild tinyc_ide.html.gz
set -e
cd "$(dirname "$0")"
echo "=== Building TinyC IDE bundle ==="
python3 bundle.py --gzip
echo ""
echo "=== Done! ==="
echo "Press any key to close..."
read -n 1
