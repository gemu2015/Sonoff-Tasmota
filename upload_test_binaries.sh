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
NOTES="## TinyC Test Firmware — $DATE

**For testers only** — may contain experimental features.

### Included targets:
$(for f in "${FILES[@]}"; do echo "- \`$(basename "$f")\`"; done)

### How to flash:
- OTA: Firmware Upgrade → Upload \`.bin\` file
- Factory install: Use \`.factory.bin\` with esptool or web installer
- Upload \`tinyc_ide.html.gz\` via Tasmota file manager (Consoles → Manage File System)

### Changes since last release:
- **Binary array file I/O syscalls (TC_RELEASE 1.3.37)** —
  \`fileReadBin(handle, arr, count)\` and \`fileWriteBin(handle, arr, count)\`
  move int32/float arrays between memory and flash as raw 4-byte
  little-endian. Same syscall serves int[] and float[] alike (both
  are int32 in memory; on-disk bit pattern is identical). Motivating
  use case: chart-history files that survive \`persist\`'s layout-hash
  invalidation. The .pvs file gets discarded on every persist-var
  add/remove, blowing away accumulated chart history; with these
  syscalls a script can keep its chart arrays in regular globals and
  save/load them to a dedicated named file via simple subroutines,
  independent of persist. Pattern is reusable for any binary blob —
  calibration tables, lookup tables, time series. New IDs 286/287.
- **\`/tc\` page no longer hangs the device on weak WiFi** — every visit to
  the TinyC Console page (or the "TinyC Console" button in Tasmota's
  Tools menu) used to do a synchronous HTTPS GET to GitHub for the
  precompiled-bytecode index. With \`http.setTimeout(5000)\` only and no
  connect-timeout cap, this could block the loop task for **50+ seconds**
  on RSSI worse than ~-75 dBm — long enough to starve
  FUNC_EVERY_50_MSECOND and trigger the RTC watchdog. Now: cache index.txt
  for 60 s (force-refresh button on the page), cap any single fetch at
  2 s via both setConnectTimeout() and setTimeout(). Worst-case loop-task
  block per /tc visit dropped from ~50 s to ~2 s. Override at build time
  via \`-DTC_REPO_CACHE_MS=N -DTC_REPO_FETCH_MS=N\`.
- **Race-free autoexec start (TC_RELEASE 1.3.36)** — removes the historical
  \`delay(15000)\` workaround at the top of \`main()\`. Autoexec slot main()
  now spawns on the first FUNC_LOOP iteration once
  \`TasmotaGlobal.uptime ≥ TC_AUTOEXEC_MIN_UPTIME\` (default 3 s). Previously
  main() spawned during FUNC_INIT and raced Tasmota's own driver init
  + Wi-Fi/RF coex bring-up — \`serialBegin\` there silently failed to
  receive bytes. Plain \`serialBegin\` in \`main()\` now works without any
  delay or BootInit hook, same as in any in-tree Tasmota driver.
  Override via \`-DTC_AUTOEXEC_MIN_UPTIME=N\` for slow networks.
- **Optional \`BootInit()\` callback** — fires once after main() returns. Kept
  as opt-in convenience for users who prefer hardware init separated
  from main(); not required for correctness now that 1.3.36 fixed the
  underlying race.
- **\`sprintf\` \`%%\` escape fix in float path (1.3.22)** — a format string
  with \`%%\` adjacent to a float spec used to render the literal pair
  unescaped (e.g. \`85 %% (12.98 kWh)\` instead of \`85 % (12.98 kWh)\`).
  The byte-walk loop in tc_sprintf_float copies prefix/suffix bytes
  verbatim around the float-spec replacement; both copy loops now
  collapse \`%%\` pairs to a single \`%\`. Int-only paths were already
  correct (they use real snprintf).
- **WebCall / JsonCall no longer skipped during spawnTask busy windows** —
  TinyCShow had a racy \`s->vm.halted\` pre-check that returned without
  invoking the callback when a worker was mid-syscall (e.g. tcpConnect
  blocking for ~500 ms). The script's sensor rows would visibly
  disappear from the WebUI for 1-3 s during query cycles, and MQTT
  telemetry would emit SENSOR JSON without the slot's keys. The pre-
  check is gone — \`tc_slot_callback()\` already handles the locking
  and halted check correctly, so the response now waits briefly for
  the worker's next \`delay()\` to release the mutex and renders all
  rows / keys.
- **Latency-watchdog idiom in \`heatpump_map.tc\`** — new template pattern
  for catching multi-second loop-task blockers from outside the script.
  Records max gap between consecutive Every50ms ticks plus per-callback
  durations; surfaces them via a "WDT-hunt" row in the WebUI and via
  console commands \`MBUSLAT\` / \`MBUSLATRESET\`. TaskLoop-driven backstop
  forces \`Restart 1\` if loop task goes silent for 60 s — turns a
  multi-minute hang into a 70 s reboot. Persist counters survive the
  forced reboot for trip statistics. Copy this pattern to any
  long-running script that's susceptible to firmware-side hangs.
- **\`utils/\` directory for desktop helpers** — \`utils/sml_emulator/\` and
  \`utils/udp_monitor/\` each ship a ready-to-run macOS .app + Linux/Win
  folder bundle (.zip), their canonical Python sources, and full
  README per tool. SML Emulator now covers SML, encrypted DLMS push,
  OBIS ASCII, VBus, EBus, M-Bus, **Kamstrup OMNIPOWER (poll/response)**,
  Modbus RTU+TCP, T510 IEC 62056-21. UDP Monitor decodes TinyC \`global\`
  + Scripter \`=>\` multicast variables on \`239.255.255.250:1999\` with
  CSV export.
- **\`watch\` + \`webButton\`/\`webSlider\` work end-to-end** — historically broken
  combo. URL handler (\`?sv=N_V\`) now mirrors STORE_WATCH side effects so
  \`written(var)\` / \`changed(var)\` fire correctly. VM scans bytecode at load
  for STORE_WATCH ops and records each watched var-slot. The natural pattern
  \`if (written(slider_var)) { dispatch(); snapshot(slider_var); }\` is now
  the correct idiom — drop any \`prev_X\` shadow workarounds.
- **Symmetric crypto syscalls (ESP32)** — \`aesEcb\` (AES-128-ECB single block,
  in-place), \`aesCbc\` (key+iv 16 B, len multiple of 16), \`hmacSha256\`,
  \`sha256\`, \`hex2bin\` / \`bin2hex\`. Backed by mbedtls (no extra flash —
  already linked for HTTPS / MQTT-TLS). ESP8266 stubs return 0. Limits:
  AES-CBC ≤ 4 KB stack-alloc; HMAC/SHA bounded at 1024 B key / 4 KB data.
  AES-GCM / ECDH not exposed yet. Motivating use case: Tuya v3.3 local
  protocol — see new \`examples/pool_pump.tc\`.
- **\`examples/pool_pump.tc\`** — Tuya local-protocol client for swimming-pool
  heat pumps. v3.3 (AES-128-ECB + CRC32) + v3.4 (AES-128-ECB + HMAC-SHA256
  + 3-step session handshake) with auto-detect. Credentials loaded from
  \`/pool_pump.cfg\` (3 lines: IP / 16-char key / 22-char devId — never in
  source). Web UI: 4 sensor rows + watch-driven button & slider (target
  20-28 °C). Background TCP work on a spawned PoolWorker task.
- **Scripter null-deref guard** — \`?sv=N_V\` URLs from another driver no
  longer reboot the device when no Scripter program is loaded. Was
  null-derefing in \`Run_script_sub\` / \`Script_Check_HTML_Setvars\`.
- **VM task stack** raised 8 KB → 12 KB — fixes \`StoreProhibited\` deep in
  fwrite/littlefs when scripts call \`saveVars()\` early, after the 1.3.20
  mbedtls headers shifted code layout.
- **Cross-VM \`share*\` API (1.3.19)** — \`shareSetInt\`/\`shareGetInt\` /
  \`shareSetFloat\`/\`shareGetFloat\` / \`shareSetStr\`/\`shareGetStr\` /
  \`shareHas\` / \`shareDelete\`. Driver-global named key/value table
  (mutex-protected, 32 entries × 16-char key × 64-char string value, 2.6 KB
  DRAM) for splitting large programs across multiple VM slots without
  going through MQTT or the filesystem.
- **PSRAM-backed bytecode (1.3.19)** — \`TC_MAX_PROGRAM\` 64 KB → 128 KB.
  Code and constant pool allocate from internal DRAM first, only spill to
  PSRAM on OOM. Small/normal programs stay fast.
- **IDE emitByte syscall-id truncation fix (1.3.19)** — \`strcmp(arr,"lit")\`
  was silently calling \`pow()\` because syscall ids ≥ 256 got \`& 0xFF\`-ed.
  Same path hit FILE_WRITE_STR, LOG_LEVEL, LOG_LEVEL_STR. All four sites
  now use \`emitSyscall(id)\` which auto-picks SYSCALL2 (u16) for big ids.
- **IDE inferType float-builtin fix (1.3.19)** — \`sprintf(buf,"%.2f",
  shareGetFloat(...))\` printed garbage because \`inferType\` ignored the
  symbol-table \`returnFloat: true\` flag. Now respects the flag — future
  float-returning builtins work automatically.
- **Tesla Powerwall API** — \`pwlRequest\`/\`pwlGet\` with nth-occurrence support
  - \`pwlGet("key[N]")\` extracts Nth occurrence of a repeated JSON key
  - Per-phase grid readings from \`/api/meters/readings\`
  - Increased PWL buffer to 8192 bytes for large API responses
  - Complete example: \`powerwall.tc\` — battery %, power flow, 3-phase grid, capacity
- **1-Wire bus support** — native GPIO + DS2480B serial bridge modes
  - \`owSetPin\`, \`owReset\`, \`owWrite\`, \`owRead\`, \`owSearch\` syscalls
  - DS18B20/DS18S20/DS1822 temperature sensors
  - DS2406/DS2413/DS2408 switch devices with WebUI buttons
  - ROM-based device aliases (\`persist\`), \`OW NAME\` command
  - Mode/pin selection via WebUI pulldowns with auto-reinit
  - Complete example: \`onewire.tc\` (~8 KB bytecode)
- \`sensorGet("SensorName#Key")\` — read Tasmota sensor JSON values from TinyC scripts
- \`udpSend("name", value)\` — explicit UDP global variable sending
- Dual Y-axis \`WebChart()\` — auto-detected when series have different Y ranges
- \`WebChartSize(width, height)\` — configurable chart dimensions
- **Tight heap allocation** — VM heap sized to actual declared arrays (saves ~90KB with 6 slots)
- Growable runtime heap via \`realloc\` (up to 32KB limit per slot)
- \`sensorGet\` re-entry guard — slot-specific, other slots' JsonCall still runs
- \`exp()\`, \`log()\` — exponential and natural logarithm math functions
- \`intBitsToFloat()\` — reinterpret raw int bits as IEEE 754 float (for I2C sensors like SCD30, SPS30)
- Multi-VM expanded to **6 concurrent slots** with dynamic memory allocation
- Lazy loading: non-autoexec slots store only filename until first run
- Staggered autoexec: 100ms delay between VM starts (prevents heap exhaustion on single-core ESP32-C3)
- Compiler fix: local variables now correctly shadow single-letter defines
- \`addCommand()\` / \`responseCmnd()\` — custom console command callbacks
- \`Command(char cmd[])\` callback — scripts can handle Tasmota console commands
- \`OnExit()\` / \`Event()\` callbacks — script stop cleanup and rule event handling
- \`webPulldown(var, "label", "options")\` — now with label parameter
- \`@getfreepins\` — dynamic GPIO pin picker in webPulldown
- \`strFind()\` / \`responseCmnd()\` with string literals — auto-detected const variants
- \`strSub()\` fix — \`len=0\` now correctly copies to end of string
- I2C sensor examples: BMX280 (BMP/BME), SCD30 (CO2), SPS30 (PM2.5), SGP30 (eCO2/TVOC)
- EPaper 2.9\" display controller example with dual Y-axis charts and UDP globals
- DY-SV17F MP3 player driver example with serial TX and console commands
- VL53L0X, MLX90614, TCS34725, VEML6075 sensor examples
- \`i2cReadRS()\` — repeated START I2C read for sensors that require it
- \`WebChart()\` — automatic Google Charts rendering with Y-axis min/max range control
- Computed goto VM dispatch — ~10% faster bytecode execution
- Pin safety: forbidden pins halt VM immediately
- Boot loop protection: autoexec disabled after 4 rapid reboots
- Deep sleep support: \`deepSleep()\`, \`deepSleepGpio()\`, \`wakeupCause()\`
- Email with file attachments: \`mailSend()\`, \`mailBody()\`, \`mailAttach()\`
- Portable bytecode — compile once in the browser, run on any ESP target
- Full documentation included (README + Reference EN/DE)
"

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
