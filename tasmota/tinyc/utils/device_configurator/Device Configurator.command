#!/bin/bash
# Double-clickable launcher for the Device Configurator.
# Opens a browser SPA to pick a `#define device_*` from user_config_override.h,
# edit its defines block + its companion [env:...] block, and compile that env.
cd "$(dirname "$0")"
exec /usr/bin/env python3 device_configurator_server.py
