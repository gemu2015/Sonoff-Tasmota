# TinyC — AI Assistant Primer

**Read this file first when helping a user write `.tc` code.**
It's a compressed entry point; the deep language spec is
[`TinyC_Reference.md`](./TinyC_Reference.md) (~4 200 lines), and real code
examples live in [`examples/`](./examples/).

This document is maintained for AI assistants. It deliberately omits tutorials,
install steps and marketing — just the sharp edges, current idioms and what not
to do. Keep it under ~300 lines; index-style entries, detail elsewhere.

---

## 1. Mental model (read once)

TinyC is a C-subset compiled to bytecode that runs on a stack VM inside the
Tasmota firmware (driver `XDRV_124`). ESP32 supports up to 6 concurrent VM slots;
ESP8266 is a subset with tighter limits.

- `int main()` runs once in a FreeRTOS task; `delay()` blocks for real here.
- After `main()` returns (or halts), **globals and heap persist**.
- Tasmota then calls your **callbacks** (`EverySecond`, `Command`, `WebUI`, …)
  on its own schedule. Callbacks run synchronously with an instruction budget;
  **`delay()` in a callback is wrong** — use `TaskLoop()` or `spawnTask()`.
- No preprocessor beyond `#define`, no pointers, no dynamic linking.
  Strings are `char[]` buffers you `strcpy`/`strcmp`/`strcat` by name.

## 2. Minimum working skeleton

```c
int counter = 0;

void EverySecond() {
    counter = counter + 1;
}

void Command(char cmd[]) {
    char resp[64];
    if (strcmp(cmd, "SHOW") == 0) {
        sprintf(resp, "counter=%d", counter);
        responseCmnd(resp);
    } else {
        responseCmnd("unknown");   // ← every branch must call responseCmnd
    }
}

int main() {
    addCommand("MY");              // registers "MYSHOW" console command
    return 0;
}
```

Anything that omits `responseCmnd()` on a `Command()` branch (including the
fallthrough `else`) causes Tasmota to emit `{"Command":"Error",...}` instead of
the intended response. This is the #1 mistake; always check.

---

## 3. Where things live

| Purpose | File |
|---|---|
| Language reference (types, operators, syntax, every built-in) | `TinyC_Reference.md` |
| Short examples by topic | `examples/*.tc` |
| IDE (browser-based compiler + flasher) | `tinyc_ide.html.gz` (served from device) |
| VM + syscall implementations (contributors only) | `../include/xdrv_124_tinyc_vm.h` |
| Driver glue: task, file, HTTP handlers | `../tasmota_xdrv_driver/xdrv_124_tinyc.ino` |

For users, everything happens in the IDE. `.tc` → compile → `.tcb` → upload →
`TinyCRun /file.tcb` in the console. Autoexec: write filename to `/tinyc.cfg`;
clear with `UfsDelete /tinyc.cfg`.

---

## 4. Current feature baseline (as of 2026-04)

Things that changed recently and invalidate older examples or forum advice:

- **Variadic `sprintf`** — `sprintf(buf, "x=%d t=%.1f s=%s", x, t, name)` works
  (compiler splits the format at compile time). Do **not** copy the older
  `sprintfInt + strcat` chaining idiom into new code.
- **`char name[] = "literal"`** — size auto-inferred from the literal; no
  explicit size, no separate `strcpy` init. Older examples use
  `char name[32]; strcpy(name, "...");` — that still works but isn't needed.
- **`persist`** — layout auto-resets on flash rebuild (v2 `.pvs` format,
  FNV-1a hash). Users no longer need `UfsDelete /*.pvs` after adding/removing
  persist vars. If advice says they do, it's stale.
- **`spawnTask("Name")` / `killTask` / `taskRunning`** — user-defined functions
  can run as dedicated FreeRTOS tasks (ESP32, max 4). Task name must be a
  string literal matching a user function in the same `.tc`.
- **Canvas API** (`imgCreate` / `imgBeginDraw` / `imgEndDraw`) — offscreen
  RGB565 buffer in PSRAM; all `dsp*` primitives redirect to the canvas when
  active. See `examples/voltmeter.tc`.
- **`delay()` inside the mini-scripter** `>F`/`>S` sections is supported
  with a 1 s cap — different subsystem, not relevant to most users.

Features documented in `TinyC_Reference.md` VM-limits table may understate:
on ESP32, constant pool is **512**, heap is **32 KB**, code size is **64 KB**
(the table section in the reference is behind).

---

## 5. Callbacks — recognised names only

Define a function with one of these exact names and Tasmota will call it —
no registration needed. Full table in `TinyC_Reference.md §Callback Functions`.

Common:
`EveryLoop`, `Every50ms`, `Every100ms`, `EverySecond` — periodic ticks.
`Command(char cmd[])` — custom console command; requires `addCommand("PFX")` in `main`.
`JsonCall` — append to MQTT telemetry (use `responseAppend`).
`WebUI`, `WebCall`, `WebPage`, `WebOn` — custom web UI / REST endpoints.
`TaskLoop` — background loop in own task; `delay()` works here.
`OnMqttConnect`, `OnMqttData(topic, payload)` — MQTT.
`TouchButton(btn, val)`, `HomeKitWrite(dev, var, val)` — UI / HomeKit events.
`OnExit`, `CleanUp` — teardown.

Rule of thumb: **ticks don't `delay()`**. If you need to wait, set state in
a tick and act on it next tick, or use `spawnTask`.

---

## 6. Syscall index by domain

One-liner per group — full signatures in `TinyC_Reference.md §Built-in Functions`.

| Domain | Key calls |
|---|---|
| Output | `addLog`, `addLogF`, `sprintf`, `responseCmnd`, `responseAppend`, `webSend` |
| GPIO | `pinMode`, `digitalRead`, `digitalWrite`, `analogRead`, `dacWrite` |
| Time | `millis`, `delay`, `tasm_secs/mins/hours/…`, `time()`, `strftime` |
| Timers | `timerSet`, `timerCancel` (software) |
| Serial | `serialBegin`, `serialRead`, `serialWrite`, `serialAvailable` |
| I²C | `i2cBegin`, `i2cRead`, `i2cWrite`, `i2cExists`, `i2cScan` |
| SPI | `spiBegin`, `spiTransfer`, `spiCS` |
| 1-Wire | `oneWireReset`, `oneWireRead`, `oneWireWrite`, `oneWireSearch` |
| Files | `fileOpen`, `fileRead`, `fileWrite`, `fileClose`, `fileDelete`, `fileSize` |
| HTTP | `httpGet`, `httpPost`, `httpGetStream` |
| TCP | `tcpListen`, `tcpAccept`, `tcpRead`, `tcpWrite` (client + server) |
| UDP | `udpSend`, `udpReceive`, `udpMulticast` |
| MQTT | `mqttSubscribe`, `mqttPublish` |
| mDNS | `mdnsAdvertise` |
| Display | `dspText`, `dspPixel`, `dspLine`, `dspRect`, `dspCircle`, `dspColor`, `dspUpdate` |
| Canvas | `imgCreate`, `imgBeginDraw`, `imgEndDraw`, `imgClear`, `dspPushImageRect` |
| Touch | `btnAdd`, `btnState`, `btnEnable`, `TouchButton` callback |
| TinyUI | `uiLabel`, `uiButton`, `uiSlider`, `uiRefresh` |
| Audio | `i2sBegin`, `i2sWrite`, `i2sStop`, `fileReadPCM16` |
| Camera | `cameraInit`, `camControl`, `camCapture` |
| SML | `smlGet`, `smlWrite`, `smlDesc` |
| Sensor JSON | `sensorGet("ENERGY#Power")` parses live Tasmota SensorJSON |
| WS2812 LEDs | `ws2812Begin`, `ws2812SetPixel`, `ws2812Show` |
| HomeKit | `hkAdd`, `hkSet`, `hkReady`, `HomeKitWrite` callback |
| Tasks (ESP32) | `spawnTask("Name"[, stackKB])`, `killTask`, `taskRunning` |
| Persist | `persist` decl, `saveVars()` |
| Watch | `watch` decl, `changed`, `delta`, `written`, `snapshot` |

---

## 7. Anti-patterns (silent breaks — AI, do not do these)

1. **`delay()` in a tick callback** — stalls the Tasmota main loop. Ticks run
   with an instruction budget; if you need to wait, use state + next tick, or
   `TaskLoop`/`spawnTask`.
2. **Missing `responseCmnd()` in a `Command()` branch** — including the
   `else`/unknown-subcommand path. Tasmota will emit
   `{"Command":"Error",...}` even though your code ran.
3. **Comparing strings with `==`** — use `strcmp(a, b) == 0`. `a == b` compares
   buffer addresses, which are rarely equal.
4. **`dspUpdate()` on an RGB / TFT panel** — only e-paper needs an explicit
   flush. On RGB panels it wastes time or corrupts the display.
5. **Separate `dspPos` + `dspPad` + `dspDraw`** when rendering a padded value —
   pad bleeds over adjacent labels. Use inline
   `sprintfFloat(buf, "[Ci%dx%dy%dp-7]%.1f", color, x, y, val); dspText(buf);`.
6. **Task stack too small** — default 5 KB handles `addLog`-only workers; HTTP
   clients need 6–8 KB; TLS + JSON parsing needs 16 KB. Below 3 KB crashes in
   `vprintf`. `spawnTask("Name", 8)` to size up.
7. **Using a non-literal task name** — `spawnTask(someVar)` won't compile;
   the name is resolved at compile time to a function in the same `.tc`.
8. **Telling the user to `UfsDelete /x.pvs`** after a persist-layout change —
   obsolete since v2 `.pvs` (auto-resets on mismatch). Only legacy `PV`-magic
   files need it (once, on first boot of new firmware).
9. **Assuming a 240-element array is stack-allocated** — the auto-heap
   threshold is **16 elements**. Arrays larger than 16 silently move to a
   heap handle. Fine for correctness; just be aware that ~128 such live heap
   arrays (`TC_MAX_HEAP_HANDLES`, ESP32) is the ceiling.
10. **`TinyCRun file.tcb` without leading slash** — needs `/file.tcb`.

---

## 8. Idiom cookbook

### Read a sensor and display a value (RGB panel, inline positioning)
```c
void EverySecond() {
    float t = sensorGet("ANALOG#Temperature1");
    char buf[64];
    sprintfFloat(buf, "[f2s1Ci3x350y300p6]%.1f C", t);
    dspText(buf);
}
```
*E-paper variant:* wrap with `dspUpdate()` at the end. See `examples/epaper29.tc`.

### HTTP GET in a background task (reports back via shared global)
```c
char url[]  = "https://example.com/api/status";
char body[2048];
int  dl_state = 0;          // 0 idle, 1 ok, -1 fail

void Downloader() {
    int rc = httpGet(url, body);
    dl_state = (rc > 0) ? 1 : -1;
}

void EverySecond() {
    if (dl_state == 1) { addLog("got body"); dl_state = 0; }
}

void Command(char cmd[]) {
    if (strcmp(cmd, "GO") == 0) {
        if (taskRunning("Downloader")) { responseCmnd("busy"); return; }
        spawnTask("Downloader", 8);      // 8 KB for TLS
        responseCmnd("fetching");
    } else {
        responseCmnd("?");
    }
}
```
See `examples/spawn_tasks.tc` for the full pattern (blink / download / heartbeat).

### Persist a counter across reboots
```c
persist int  bootCount = 0;
persist float totalKwh = 0.0;

int main() {
    bootCount = bootCount + 1;   // auto-loaded from /yourfile.pvs
    saveVars();                  // auto on TinyCStop; manual for safety
    return 0;
}
```
Remove `persist` from a var and re-flash → old file is silently discarded
(FNV-1a layout hash mismatch). No user action needed.

### Receive a Tasmota console command with arguments
```c
void Command(char cmd[]) {
    // cmd is the suffix after your registered prefix, e.g. "MY" prefix +
    // user types "MYSET 42"  → cmd = "SET 42"
    if (strncmp(cmd, "SET ", 4) == 0) {
        int v = atoi(cmd + 4);
        // …
        responseCmnd("ok");
    } else {
        responseCmnd("unknown");
    }
}
int main() { addCommand("MY"); return 0; }
```

### Change detection (watch)
```c
watch float power;
void EverySecond() {
    power = sensorGet("ENERGY#Power");
    if (changed(power)) {
        addLogF("power changed by %.1f", delta(power));
        snapshot(power);
    }
}
```

### MQTT: subscribe and react
```c
int main() {
    mqttSubscribe("cmnd/room/light");
    return 0;
}
void OnMqttData(char topic[], char payload[]) {
    if (strcmp(payload, "ON") == 0)  digitalWrite(12, 1);
    if (strcmp(payload, "OFF") == 0) digitalWrite(12, 0);
}
```

---

## 9. Example index (grouped)

**GPIO / basic** — `blink.tc`, `sensor_read.tc`, `fibonacci.tc`
**Sensors (I²C)** — `bme280.tc`, `bmp280.tc`, `scd30.tc`, `sht31.tc`, `ccs811.tc`, `sgp30.tc`, `ltr308.tc`, `veml6075.tc`, `tcs34725.tc`, `vl53l0x.tc`, `ads1115.tc`, `max31855.tc`, `mlx90614.tc`, `sps30.tc`, `ld2410.tc`
**Sensors (other)** — `onewire.tc` (DS18B20), `bresser.tc` (weather), `ecotracker.tc` (BLE)
**Display** — `display_demo.tc`, `lcd_i2c.tc`, `lcd_chart.tc`, `chart.tc`, `chart_types.tc`, `epaper29.tc`, `sunton_display.tc`, `guiton_display.tc`, `analog_clock.tc`, `voltmeter.tc` (canvas), `text_on_image.tc`, `watch_demo.tc`
**LEDs** — `ledbar.tc`
**Touch / UI** — `touch_buttons.tc`, `tinyui_demo.tc`, `tinyui_dashboard.tc`, `multipage_demo.tc`
**Web** — `web_buttons.tc`, `web_handler.tc`, `webcall_demo.tc`, `webui_demo.tc`
**Network** — `udp.tc`, `live_chart.tc`
**Audio** — `wav_player.tc` (I²S + WM8960)
**Camera (ESP32)** — `camera.tc`, `webcam.tc`, `webcam_tinyc.tc`, `snap_with_timestamp.tc`
**Power / energy** — `powerwall.tc` (Tesla), `sma_speedwire.tc`, `core2_energy.tc`
**SML smart meter** — `sml_ebus.tc`
**Tasks / concurrency (ESP32)** — `spawn_tasks.tc`, `callbacks.tc`, `callback_test.tc`
**HomeKit (ESP32)** — `homekit_demo.tc`, `homekit_office.tc`
**Strings / sort** — `strings.tc`, `sort.tc`, `file_io.tc`
**Benchmarks / diag** — `benchmark.tc`, `crash_test.tc`, `sizeof_demo.tc`

When a user asks for X, prefer to start from the closest example and adapt —
faster and less bug-prone than synthesising from the reference.

---

## 10. Upload / iterate

IDE workflow (most users): write `.tc` → Compile → Upload → Run. Log window
shows compile errors and runtime `addLog` output.

CLI / direct (less common):
- Upload `.tcb` with multipart form: `curl -F "file=@my.tcb" http://<ip>/upload`.
  **Not** `--data-binary` — that path crashes on the raw handler (camera regression).
- Console: `TinyCStop 0` to stop slot 0, `TinyCRun /my.tcb` to start, `TinyCStatus`.
- Autoexec on boot: write filename to `/tinyc.cfg` (IDE does this via checkbox).

Persist files live next to the `.tcb` as `<name>.pvs`.

---

## 11. Known gaps / "don't retry"

- **`SYS_FILE_WRITE_STR`** — compiler emits the opcode but VM has no handler
  yet (`TC_ERR_BAD_SYSCALL` at runtime). Track upstream; don't generate code
  that relies on it.
- **DFRobot OV3660 camera horizontal stripes** — unsolved, 10+ hrs debug. If a
  user has this exact board, acknowledge the known issue rather than guessing.
- **Callback-dispatch integer cache** — attempted and reverted (broke PAUSED
  handler); if tempted to optimise, read `MEMORY.md §TinyC Performance
  Optimization` in the maintainer's notes first.

---

## 12. Meta

- Update rule: when a feature ships that invalidates older examples or forum
  advice, add a one-liner in §4 and, if it's an anti-pattern-shaped change,
  in §7. Move detail to topic docs, keep this file ≤ ~300 lines.
- This file complements `TinyC_Reference.md` — do not duplicate tables; point
  into it. Only put here what belongs in an AI assistant's working memory.
