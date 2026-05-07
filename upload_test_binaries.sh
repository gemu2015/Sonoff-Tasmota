#!/bin/bash
#
# Upload test firmware binaries to GitHub Release (rolling "testing" tag)
# Replaces previous release — only the latest binaries are kept.
#
# Usage:
#   ./upload_test_binaries.sh                    # upload all standard targets
#   ./upload_test_binaries.sh tasmota32-DE       # upload specific target(s)
#   ./upload_test_binaries.sh --list             # show available binaries
#

REPO="gemu2015/Sonoff-Tasmota"
TAG="testing"
TITLE="TinyC Test Build"
FW_DIR="$(cd "$(dirname "$0")" && pwd)/build_output/firmware"

# Standard TinyC test targets (without .bin extension)
DEFAULT_TARGETS=(
  tinyc8266-4M
  tinyc32-4M
  tinyc32s3
  tinyc32c3
)

# --- helpers ---
die()  { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

# --- list mode ---
if [[ "$1" == "--list" ]]; then
  echo "Available firmware binaries in $FW_DIR:"
  echo ""
  ls -lh "$FW_DIR"/*.bin 2>/dev/null | awk '{printf "  %-40s %s\n", $NF, $5}'
  exit 0
fi

# --- check prerequisites ---
command -v gh >/dev/null 2>&1 || die "gh CLI not installed. Run: brew install gh"
gh auth status >/dev/null 2>&1 || die "Not logged in. Run: gh auth login"
[[ -d "$FW_DIR" ]] || die "Firmware directory not found: $FW_DIR"

# --- determine targets ---
if [[ $# -gt 0 ]]; then
  TARGETS=("$@")
else
  TARGETS=("${DEFAULT_TARGETS[@]}")
fi

# --- collect files to upload ---
FILES=()
for target in "${TARGETS[@]}"; do
  bin="$FW_DIR/${target}.bin"
  bingz="$FW_DIR/${target}.bin.gz"
  factory="$FW_DIR/${target}.factory.bin"
  if [[ "$target" == *8266* ]] && [[ -f "$bingz" ]]; then
    # ESP8266: use compressed .bin.gz (uncompressed may be too large for OTA)
    FILES+=("$bingz")
  elif [[ -f "$bin" ]]; then
    FILES+=("$bin")
    # include .factory.bin if it exists
    [[ -f "$factory" ]] && FILES+=("$factory")
  else
    echo "WARNING: $bin not found, skipping"
  fi
done

[[ ${#FILES[@]} -eq 0 ]] && die "No firmware files found to upload"

# --- include TinyC IDE + docs from the repo (single source of truth) ---
# Previously read from /Volumes/vp_dev/TinyC/ which was a manually-synced
# staging area — easy to forget to sync before release. Reading directly
# from the repo guarantees published artifacts match what's committed.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TINYC_DIR="$SCRIPT_DIR/tasmota/tinyc"

IDE_FILE="$TINYC_DIR/tinyc_ide.html.gz"
[[ -f "$IDE_FILE" ]] && FILES+=("$IDE_FILE") || echo "WARNING: TinyC IDE not found at $IDE_FILE"

for doc in README.md TinyC_Reference.md TinyC_Reference_DE.md; do
  [[ -f "$TINYC_DIR/$doc" ]] && FILES+=("$TINYC_DIR/$doc") || echo "WARNING: $doc not found"
done

# --- show what will be uploaded ---
info "Files to upload:"
for f in "${FILES[@]}"; do
  size=$(ls -lh "$f" | awk '{print $5}')
  echo "  $(basename "$f")  ($size)"
done
echo ""

# --- build release notes ---
DATE=$(date "+%Y-%m-%d %H:%M")
NOTES_HEADER="## TinyC Test Firmware — $DATE

**For testers only** — may contain experimental features.

### Included targets:
$(for f in "${FILES[@]}"; do echo "- \`$(basename "$f")\`"; done)
"

NOTES_BODY=$(cat <<'NOTES_BODY_EOF'

### How to flash:
- OTA: Firmware Upgrade → Upload `.bin` file
- Factory install: Use `.factory.bin` with esptool or web installer
- Upload `tinyc_ide.html.gz` via Tasmota file manager (Consoles → Manage File System)

### Changes since last release:
- **🛑 Critical fix #1 — AES syscall stack overflow corrupting WiFi heap (May 7)** —
  The 1.3.20 `add aes` commit introduced four large fixed-size local arrays
  inside `tc_vm_step()`'s switch-case handlers: `stackbuf[4096]` in
  SYS_AES_CBC, `kbuf[1024]+dbuf[4096]` in SYS_HMAC_SHA256, `dbuf[4096]` in
  SYS_SHA256, `src[1024]` in SYS_HEX2BIN. GCC at `-Os` reserves the largest
  unified switch-case frame **at function entry, regardless of which case
  fires** — so every TinyC callback dispatch inflated `tc_vm_step` by ~5 KB.
  Loop task (8 KB stack) and web-server task (4–8 KB) callbacks overflowed
  into adjacent heap, surfacing as `StoreProhibited` in WiFi RX
  (`esf_buf_alloc` / `wDev_IndicateFrame` / `ppTask`). Reproducible on a
  4 MB ESP32 device running multi-slot scripts (`epaper42_v3.tcb` slot 0
  with `sensorGet` + `sht31.tcb` slot 5 with `JsonCall`) — crashed within
  seconds. **Fix**: heap-allocate all four buffers via `special_malloc`
  (PSRAM-preferring on equipped devices). `tc_vm_step` frame returns to
  baseline. AES is never on a hot path, so per-call malloc cost is
  negligible.
- **🛑 Critical fix #2 — multi-slot deadlock in TinyCShow JsonCall+WebCall fan-out (May 7)** —
  The `drop racy halted pre-check` change (Apr 30) removed the
  `s->vm.halted && s->vm.error == TC_OK` pre-check before
  `tc_slot_callback(s, JsonCall/WebCall)` to fix a cosmetic spawnTask
  UI-disappear case. But the pre-check was real safety: without it,
  every fan-out unconditionally takes each slot's mutex with
  `portMAX_DELAY`, deadlocking when a slot is already running
  (mid-callback delay, mid-syscall in spawnTask worker, etc.).
  Multi-slot configs hung permanently on slot startup or first
  nav-button click. Bisect confirmed against the C3 baseline (commit
  63a7e6535, Apr 20) which ran the same pattern stably for weeks.
  **Fix**: pre-check restored. The spawnTask UI-disappear case will
  need a different cleaner fix (likely `xSemaphoreTake` with timeout
  in `tc_slot_callback`) — open as a follow-up.
- **`/tc` repo fetch cap 2 s → 5 s + better error logging** — the 2-second
  cap was too tight for github TLS handshake on RSSI ~-66 dBm devices,
  silently dropping the Repository section from /tc. Bumped to 5 s
  (still well under 5-s task watchdog), and added explicit error logs
  for fetch failures so the next time the section disappears, the
  serial log says why.
- **Binary array file I/O syscalls (TC_RELEASE 1.3.37)** —
  `fileReadBin(handle, arr, count)` and `fileWriteBin(handle, arr, count)`
  move int32/float arrays between memory and flash as raw 4-byte
  little-endian. Same syscall serves int[] and float[] alike. Motivating
  use case: chart-history files that survive `persist`'s layout-hash
  invalidation. New IDs 286/287.
- **`/tc` page no longer hangs the device on weak WiFi** — every visit to
  the TinyC Console page used to do a synchronous HTTPS GET to GitHub
  for the precompiled-bytecode index. With `http.setTimeout(5000)` only
  and no connect-timeout cap, this could block the loop task for **50+
  seconds** on RSSI worse than ~-75 dBm — long enough to starve
  FUNC_EVERY_50_MSECOND and trigger the RTC watchdog. Now: cache
  index.txt for 60 s (force-refresh button on the page), cap any single
  fetch at 5 s via both setConnectTimeout() and setTimeout(). Override
  via `-DTC_REPO_CACHE_MS=N -DTC_REPO_FETCH_MS=N`.
- **Race-free autoexec start (TC_RELEASE 1.3.36)** — removes the historical
  `delay(15000)` workaround at the top of `main()`. Autoexec slot main()
  now spawns on the first FUNC_LOOP iteration once
  `TasmotaGlobal.uptime ≥ TC_AUTOEXEC_MIN_UPTIME` (default 3 s).
  Plain `serialBegin` in `main()` now works without any delay or
  BootInit hook. Override via `-DTC_AUTOEXEC_MIN_UPTIME=N`.
- **Optional `BootInit()` callback** — fires once after main() returns.
  Opt-in convenience for users who prefer hardware init separated from
  main(); not required for correctness now that 1.3.36 fixed the
  underlying race.
- **`sprintf` `%%` escape fix in float path (1.3.22)** — a format string
  with `%%` adjacent to a float spec used to render the literal pair
  unescaped. Both copy loops in tc_sprintf_float now collapse `%%`
  pairs to a single `%`. Int-only paths were already correct.
- **Latency-watchdog idiom in `heatpump_map.tc`** — template pattern for
  catching multi-second loop-task blockers from outside the script.
  Records max gap between consecutive Every50ms ticks plus per-callback
  durations; surfaces them via WebUI row and console commands `MBUSLAT`
  / `MBUSLATRESET`. TaskLoop-driven backstop forces `Restart 1` if loop
  task goes silent for 60 s. Persist counters survive the forced reboot.
- **`utils/` directory for desktop helpers** — `utils/sml_emulator/` and
  `utils/udp_monitor/` ship ready-to-run macOS .app + Linux/Win bundles,
  Python sources, full README per tool. SML Emulator covers SML,
  encrypted DLMS push, OBIS ASCII, VBus, EBus, M-Bus, Kamstrup
  OMNIPOWER, Modbus RTU+TCP, T510 IEC 62056-21. UDP Monitor decodes
  TinyC `global` + Scripter multicast variables on
  `239.255.255.250:1999` with CSV export.
- **`watch` + `webButton`/`webSlider` work end-to-end** — historically
  broken combo. URL handler now mirrors STORE_WATCH side effects so
  `written(var)` / `changed(var)` fire correctly. The natural pattern
  `if (written(slider_var)) { dispatch(); snapshot(slider_var); }` is
  the correct idiom — drop any `prev_X` shadow workarounds.
- **Symmetric crypto syscalls (ESP32)** — `aesEcb`, `aesCbc`,
  `hmacSha256`, `sha256`, `hex2bin` / `bin2hex`. Backed by mbedtls
  (no extra flash — already linked for HTTPS / MQTT-TLS). ESP8266 stubs
  return 0. Limits: HMAC/SHA bounded at 1024 B key / 4 KB data.
  AES-GCM / ECDH not exposed yet. Motivating use case: Tuya v3.3
  local protocol — see new `examples/pool_pump.tc`. **All buffers now
  heap-allocated** as of May 7 (see Critical fix #1 above).
- **`examples/pool_pump.tc`** — Tuya local-protocol client for
  swimming-pool heat pumps. v3.3 (AES-128-ECB + CRC32) + v3.4
  (AES-128-ECB + HMAC-SHA256 + 3-step session handshake) with
  auto-detect. Credentials loaded from `/pool_pump.cfg`. Web UI: 4
  sensor rows + watch-driven button & slider. Background TCP work on
  spawned PoolWorker task.
- **VM task stack** raised 8 KB → 12 KB — fixes `StoreProhibited` deep
  in fwrite/littlefs when scripts call `saveVars()` early.
- **Cross-VM `share*` API (1.3.19)** — `shareSet/Get` for Int/Float/Str,
  plus `shareHas`/`shareDelete`. Driver-global named key/value table
  (mutex-protected, 32 entries) for splitting large programs across
  multiple VM slots without going through MQTT or filesystem.
- **PSRAM-backed bytecode (1.3.19)** — `TC_MAX_PROGRAM` 64 KB → 128 KB.
  Code and constant pool allocate from internal DRAM first, only spill
  to PSRAM on OOM.
- **Tesla Powerwall API** — `pwlRequest`/`pwlGet` with nth-occurrence
  support. Per-phase grid readings from `/api/meters/readings`.
  Complete example: `powerwall.tc`.
- **1-Wire bus support** — native GPIO + DS2480B serial bridge modes.
  `owSetPin`, `owReset`, `owWrite`, `owRead`, `owSearch` syscalls.
  DS18B20/DS18S20/DS1822 temperature sensors. DS2406/DS2413/DS2408
  switch devices. Complete example: `onewire.tc`.
- `sensorGet("SensorName#Key")` — read Tasmota sensor JSON values from
  TinyC scripts.
- `udpSend("name", value)` — explicit UDP global variable sending.
- Dual Y-axis `WebChart()` — auto-detected when series have different
  Y ranges. `WebChartSize(width, height)` configurable dimensions.
- **Tight heap allocation** — VM heap sized to actual declared arrays.
  Growable runtime heap via `realloc` (up to 32 KB per slot).
- `sensorGet` re-entry guard — slot-specific, other slots' JsonCall
  still runs.
- `exp()`, `log()`, `intBitsToFloat()` — math + IEEE 754 reinterpret.
- Multi-VM expanded to **6 concurrent slots** with dynamic memory
  allocation. Lazy loading. Staggered autoexec.
- `addCommand()` / `responseCmnd()` — custom console command callbacks.
- `Command(char cmd[])` callback — scripts handle Tasmota console
  commands. `OnExit()` / `Event()` callbacks.
- `webPulldown(var, "label", "options")` — with label parameter and
  `@getfreepins` dynamic GPIO pin picker.
- I2C sensor examples: BMX280, SCD30, SPS30, SGP30, VL53L0X,
  MLX90614, TCS34725, VEML6075. EPaper 2.9″ display controller with
  dual Y-axis charts and UDP globals. DY-SV17F MP3 player driver.
- `WebChart()` — automatic Google Charts rendering with Y-axis
  min/max range control.
- Computed goto VM dispatch — ~10% faster bytecode execution.
- Pin safety: forbidden pins halt VM immediately.
- Boot loop protection: autoexec disabled after 4 rapid reboots.
- Deep sleep support: `deepSleep()`, `deepSleepGpio()`, `wakeupCause()`.
- Email with file attachments: `mailSend()`, `mailBody()`,
  `mailAttach()`.
- Portable bytecode — compile once in the browser, run on any ESP target.
- Full documentation included (README + Reference EN/DE).
NOTES_BODY_EOF
)

NOTES="${NOTES_HEADER}${NOTES_BODY}"

# --- write notes to a file (--notes-file is bulletproof; --notes "$NOTES"
#     is fragile when the body contains backticks + apostrophes + multi-line
#     content, which can leave the release with an empty body and let GitHub
#     fall back to rendering the tagged commit's message instead) ---
NOTES_FILE=$(mktemp /tmp/release_notes.XXXXXX.md)
printf '%s' "$NOTES" > "$NOTES_FILE"
trap "rm -f $NOTES_FILE" EXIT
info "Notes written to $NOTES_FILE ($(wc -c < "$NOTES_FILE") bytes)"

# --- delete old release if exists ---
info "Removing old '$TAG' release (if any)..."
gh release delete "$TAG" --repo "$REPO" --yes --cleanup-tag 2>/dev/null || true

# --- create new release ---
info "Creating release '$TAG' with ${#FILES[@]} files..."
gh release create "$TAG" \
  --repo "$REPO" \
  --title "$TITLE" \
  --notes-file "$NOTES_FILE" \
  --prerelease \
  "${FILES[@]}"

if [[ $? -eq 0 ]]; then
  URL="https://github.com/$REPO/releases/tag/$TAG"
  echo ""
  info "Done! Release URL:"
  echo "  $URL"
else
  die "Failed to create release"
fi
