# TinyC desktop utilities

Companion tools that run on your laptop (not on the device) and help
develop / debug TinyC and Scripter setups.

| Tool                                  | What it does                                                                  |
| ------------------------------------- | ----------------------------------------------------------------------------- |
| **[`sml_emulator/`](sml_emulator/)** | Emulate real smart meters over a USB-serial dongle so you can test Tasmota's SML descriptors without owning the meter. Supports SML, encrypted DLMS, OBIS ASCII, VBus, EBus, M-Bus, Kamstrup, Modbus RTU/TCP, T510 IEC 62056-21. |
| **[`udp_monitor/`](udp_monitor/)**   | Sniff the local network for TinyC `global` and Scripter `=>` variables broadcast on `239.255.255.250:1999`, decode them, and show who's publishing what. CSV export + clash detection. |

Both tools follow the same architecture: a small Python HTTP server +
a browser UI page, no external dependencies beyond `python3` (and
`pyserial` for the SML Emulator's USB-serial side). Each ships as
three platform copies — a macOS `.app` bundle, a Linux folder, and a
Windows folder — bundled in a `.zip` you can hand to a non-developer.

See each tool's own `README.md` for full details, distribution
layout, and implementation notes.
