#!/bin/sh
# Tasmota Workbench launcher (Linux / any Unix). Double-click in your file
# manager (mark executable) or run from a terminal. Opens a browser-based SPA
# at http://127.0.0.1:8124/ with three panes:
#   • Monitor — serial console (USB) OR UDP-syslog console (LAN devices)
#   • Devices — LAN scan, OTA flash, inline rename
#   • Shares  — Tasmota multicast (239.255.255.250:1999) share-protocol monitor
#
# Needs Python 3 + pyserial:   pip3 install --user pyserial
# Serial port access on Linux usually needs the 'dialout' group:
#     sudo usermod -aG dialout "$USER"   (then re-login)
# Syslog on UDP <1024 (e.g. 514) needs root; use a high port such as 5514 and
# set Tasmota 'LogPort 5514' (no sudo needed).
cd "$(dirname "$0")" || exit 1
exec python3 tasmota_workbench_server.py
