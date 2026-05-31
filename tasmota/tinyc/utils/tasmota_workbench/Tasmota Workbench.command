#!/bin/bash
# Double-clickable launcher for Tasmota Workbench.
# Opens a browser SPA with: serial/syslog console · LAN device scan + OTA ·
# Tasmota UDP-share-protocol monitor (multicast 239.255.255.250:1999).
cd "$(dirname "$0")"
exec /usr/bin/env python3 tasmota_workbench_server.py
