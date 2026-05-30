# Device Configurator

A small browser-based GUI for picking a Tasmota target out of
`tasmota/user_config_override.h`, editing its defines and PlatformIO env
in two side-by-side panels, and compiling that one env with a button click.

Same pattern as `serial_monitor`: a single Python HTTP server with an
embedded SPA, double-clickable launcher, no extra dependencies.

## Run

- **macOS:** double-click `Device Configurator.command` (or:
  `python3 device_configurator_server.py` from a terminal).
- **Linux / any Unix:** `./device_configurator.sh`.

Listens on `http://127.0.0.1:8125/` and opens your browser automatically.

## What it does

A device variant in this repo lives in two places:

1. **`user_config_override.h`** has, near the top, a list of selector lines —
   ```c
   //#define device_ILI9488
     #define device_ILI9488p16     // <- exactly ONE is uncommented = ACTIVE
   //#define device_sunton_800_480
   ```
   and further down, an `#ifdef device_X ... #endif` block per device with
   that variant's defines (display driver, sensors, feature flags, …).

2. **`platformio_override.ini`** has an `[env:NAME]` block per build target
   (board, build flags, lib_extra_dirs, etc.).

The configurator surfaces both side-by-side:

- Left pane: every `#define device_*` selector found in the override header,
  one per row. A green dot marks the **ACTIVE** one. Selectors whose
  `#ifdef` block doesn't exist are shown struck through.
- Right pane, three tabs for the selected device:
  - **Defines** — the verbatim `#ifdef device_X … #endif` block, editable as
    a textarea. **Save** writes it back to `user_config_override.h`
    (a one-deep `.bak` is taken before each save).
  - **Env** — a dropdown of every `[env:NAME]` in `platformio_override.ini`.
    Pick the env this device is normally built with; the choice is
    remembered as a single comment line right after the `#ifdef`:
    `// device_config_env: <env-name>`. Below the dropdown is the
    `[env:…]` block as an editable textarea. **Save** writes the env back.
  - **Build** — shows the mapped env name and the running `pio run -e <env>`
    output live. **Compile** runs it; **Stop** terminates it.

    Below the build header is an **OTA targets** input — one or more device
    IPs (comma-separated) that should receive this variant. They're persisted
    as a marker comment right after the env mapping inside the device block:
    ```c
    #ifdef device_M5EPD47
    // device_config_env: tasmota32-core2
    // device_config_targets: 192.168.188.20, 192.168.188.21
    ```
    The title row shows a `→ N targets` pill. After a successful compile, the
    **Flash N ▸** button enables. Click it to OTA-flash `.pio/build/<env>/firmware.bin`
    to every listed device **sequentially**, with safeboot escalation if a
    target rejects the direct upload with "nicht genug Speicherplatz" (single-app
    boards): trigger `/u4?u4=fct`, wait for safeboot, retry `/u2`. Each device
    is verified by polling Status 2 until uptime resets into the `(tasmota)` app.
    Per-device progress lines (`[flash:<ip>] …`) appear in the same Build log.
  - **Common ⌑** — a single global section of `#undef` / `#define` lines that
    applies to **every** build, regardless of which `device_*` selector is
    active. The region is demarcated in `user_config_override.h` by two
    marker comments:
    ```c
    // ===== device_config_common: BEGIN =====
    #undef USE_RULES        // (example) every build drops Rules
    #undef USE_TIMERS
    #define USE_TINYC_BLE
    // ===== device_config_common: END =====
    ```
    On first use the section doesn't exist yet — click **Create section** and
    the markers get inserted right before the first `#ifdef device_*` block
    (so they run between the selector list at the top and the per-device
    `#ifdef` blocks below, letting a device block re-`#define` something the
    common section `#undef`'d). The tab is marked in amber to remind you the
    edit is global, not per-device.

Top-right of the title row:

- **Activate** — make this device the only uncommented selector in the
  override header (every other `#define device_*` is commented). You must
  confirm. Greyed out if the device is already active.
- **Compile** — kicks off `pio run -e <mapped-env>`. If the device is
  *not* the active selector, you'll be asked to confirm (the build still
  uses the env, but the device-specific `#ifdef` block won't be picked up
  unless that selector is the active one).

## Files this tool reads / writes

- `tasmota/user_config_override.h` (selector list, `#ifdef` blocks, env-mapping
  comment).
- `platformio_override.ini` (`[env:…]` blocks).

Both files are **user-owned and gitignored**. A one-deep rotating backup
(`.bak` next to the file) is taken before every write.

## Endpoints (if you want to script it)

| route | method | body | what |
|---|---|---|---|
| `/api/devices` | GET | — | list of all devices + active flag + mapped env |
| `/api/device/<name>` | GET | — | block + env_block + dropdown of envs |
| `/api/device/save_block` | POST | `{name, block}` | save the `#ifdef…#endif` text |
| `/api/device/save_env_mapping` | POST | `{name, env}` | write the marker comment |
| `/api/device/activate` | POST | `{name}` | activate exactly this selector |
| `/api/env/save` | POST | `{env, text}` | save an `[env:…]` block |
| `/api/common` | GET | — | `{exists, text}` of the common section |
| `/api/common/save` | POST | `{text}` | replace the common-section inner content |
| `/api/common/create` | POST | `{text?}` | insert BEGIN/END markers + initial text |
| `/api/device/save_targets` | POST | `{name, targets:[ip,…]}` | write the OTA targets marker |
| `/api/flash/start` | POST | `{env, targets}` | OTA-flash the env's firmware.bin to all targets |
| `/api/flash/status` | GET | — | `{running, current, results: {ip:OK\|FAILED}}` |
| `/api/compile/start` | POST | `{env}` | start `pio run -e <env>` |
| `/api/compile/stop` | POST | — | terminate a running compile |
| `/api/compile/status` | GET | — | `{running, env, rc, tail[…]}` |
| `/quit` | GET | — | exit the server |

## Limitations / non-goals

- Does **not** lint or syntax-check what you type; it's a free-form text edit.
- Does **not** discover envs outside `platformio_override.ini`
  (no `platformio.ini`, no other override files).
- Single open device at a time — close one tab before opening another.
- Compile output is captured to an in-memory ring buffer (~4000 lines).
  Reload the page during a build = log gets restored from server state.
