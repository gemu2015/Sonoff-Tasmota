# TinyC desktop utilities

Companion tools that run on your laptop (not on the device) and help
develop / debug TinyC, Matter, and Scripter setups.

## Browser-GUI tools (Python server + single-page UI, no dependencies)

| Tool | What it does |
| ---- | ------------ |
| **[`tasmota_workbench/`](tasmota_workbench/)** | ESP/Tasmota Swiss-army knife. **Monitor:** serial console **and** remote UDP-syslog viewer sharing one 200k-line scrollback (hex mode, command input). **Devices · Scan & OTA:** LAN scan of Tasmota devices (CPU / firmware / partitions / heap) with **per-device sensor + relay/lamp detection** (the "Sensors / Outputs" column), inline `DeviceName`/`Hostname` rename, and an OTA flasher (serial via esptool with auto-offset / Tasmota OTA with a pre-flash device card). **Shares:** multicast monitor for TinyC `global` / Scripter `=>` variables on `239.255.255.250:1999`, with clash detection + CSV export. *Supersedes the old `serial_monitor` + `udp_monitor`, which were merged into it.* |
| **[`sml_emulator/`](sml_emulator/)** | Emulate real smart meters over a USB-serial dongle so you can test Tasmota's SML descriptors without owning the meter. Supports SML, encrypted DLMS, OBIS ASCII, VBus, eBUS, M-Bus, Kamstrup, Modbus RTU/TCP, T510 IEC 62056-21, and a CAN-bus request/response device mode. |
| **[`shelly_tester/`](shelly_tester/)** | GUI for poking at Shelly and EcoTracker devices over UDP-RPC (port 1010), HTTP GET, or ICMP ping. Colour-coded log + auto-formatted JSON + a persistent UDP listener for unsolicited push packets. Cross-platform port of ottelo's Windows-only PowerShell GUI. |
| **[`device_configurator/`](device_configurator/)** | Pick a Tasmota target out of `tasmota/user_config_override.h`, edit its `#define`s and PlatformIO env in two side-by-side panels, and compile that one env with a button click. |

## Code-generation tools (native ⇄ TinyC ⇄ BinPlugin)

| Tool | What it does |
| ---- | ------------ |
| **[`native2dual/`](native2dual/)** | Transpiler/scaffolder that turns a native `xsns_`/`xdrv_` driver into a dual-format **loadable BinPlugin** (`.bin` consumed by `xdrv_123_plugins`), so a driver ships as a runtime-loadable module instead of bloating the firmware image. Browser GUI + macOS `.app`; see [`native2dual/FINDINGS.md`](native2dual/FINDINGS.md). |
| **[`tc2plugin/`](tc2plugin/)** | Browser app: edit TinyC on the left, **Translate**, and get the generated dual-format Tasmota plugin C++ (`xsns_*_dual.cpp` shape) on the right. Proof of concept. |

---

Most of these follow the same architecture: a small Python HTTP server +
a browser UI page, no external dependencies beyond `python3` (and
`pyserial` for the serial side of the Workbench / SML Emulator). The
user-facing tools (Workbench, SML Emulator, Shelly Tester) ship as three
platform copies — a macOS `.app` bundle, a Linux folder, and a Windows
folder — bundled in a `.zip` you can hand to a non-developer.

> macOS note: a Finder-launched `.app` may need **System Settings →
> Privacy & Security → Local Network** enabled for it to reach your
> devices; otherwise run the `.command` from Terminal (which already has
> the grant). After editing a tool's `*_server.py`, run its
> `sync_app.command` to copy the change into the `.app` bundle(s).

See each tool's own `README.md` for full details, distribution layout,
and implementation notes.
