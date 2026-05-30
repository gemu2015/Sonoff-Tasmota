#!/bin/sh
# Device Configurator launcher (Linux / any Unix). Double-click in your file
# manager (mark executable) or run from a terminal. Opens the configurator
# at http://127.0.0.1:8125/.
#
# Needs Python 3 (stdlib only — no extra packages).
cd "$(dirname "$0")" || exit 1
exec python3 device_configurator_server.py
