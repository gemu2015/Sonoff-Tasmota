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

- **`bcall("name", buf, len)` syscall** — invoke native functions exported
  by a `MODULE_TYPE_BLIB` binary library plugin. Phase-1 ABI is locked to
  `(BUF, INT) → INT` — matches the CRC primitives in the first reference
  blib (`xblib_01_crc.cpp`: `mb_crc16` / `crc32` / `crc8_dallas`). The
  blib is uploaded as a relocatable `.bin` to the device's plugin
  partition and `iniz`'d once; from then on the names are callable from
  any TinyC slot at full native speed (no per-blib firmware glue).
  String literal name → `constArgs[0]`; runtime char[] buffer → `strArgs[1]`.
  Sim stub returns `-1` so standalone-IDE runs don't crash. See
  `examples/blib_crc_demo.tc` for a working three-CRC demo.
  Use it for: tight inner loops (CRC, FFT, DSP), crypto we don't already
  wrap, codec primitives, large LUTs that don't belong in script bytecode.
  Don't use it for: anything that talks to Tasmota lifecycle (`Command`,
  `WebUI`, `OnMqttData`, `persist`, `watch`/`share`) or that you'd want
  to iterate without a flash cycle. Phase-1 dispatcher is in
  `xdrv_124_tinyc_vm.h`; widening to other arg shapes (float, multi-arg,
  `TC_ARG_REF` out-params) is a small surgical patch when the next blib
  needs them.
- **String ops** (1.5.0) — 7 new built-ins for in-place char[] manipulation
  with literal-needle args:
    `int n = strReplace(buf, "old", "new")` — find/replace all, returns count
    `if (strStartsWith(buf, "MBUS"))` / `strEndsWith(buf, ".tcb")` /
    `strContains(buf, "<error>")` — 1/0 prefix/suffix/substring tests
    `strToUpper(buf)` / `strToLower(buf)` — in-place ASCII case
    `int n = strTrim(buf)` — in-place whitespace strip, returns new length
  All literal-needle (string-literal arg). Runtime-needle variants
  intentionally not in v1 — uncommon in practice. See
  `examples/test_strings_v15.tc`.
- **Reference parameters** (1.4.3) — `void swap(int& a, int& b)` declares
  scalar pass-by-reference params (int, float, char). Callee reads/writes
  through the ref, mutations visible to the caller. Multi-out
  (`void parse(int x, int& lo, int& hi)`), in-place compound (`n += 5` on
  a ref param), and globals-as-ref-args all work. Caller arg must be a
  plain Identifier of a local or global; array elements / struct fields
  / heap-arrays yield a clear compile error. **Zero new VM opcodes** —
  reuses ADDR_LOCAL/ADDR_GLOBAL + LOAD_REF_ARR/STORE_REF_ARR (already in
  the VM for `int arr[]` array refs); scalar refs are "array refs always
  at index 0". See `examples/test_refparams_v1.tc`.
- **Function pointers** (1.4.1, struct fields in 1.4.2) —
  `typedef int (*cmp_fn)(int, int);` declares a fn-ptr type;
  `cmp_fn fn;` is a variable; `fn = my_function;` assigns the bare
  function name (no `&`); `fn(a, b)` calls indirectly. Works for locals,
  globals, function parameters, **and struct fields** — `cmds[i].handler(args)`
  routes through `OP_CALL_INDIRECT 0x56` cleanly. The dispatch-table-as-array-of-
  struct pattern works fully. Out of scope: inline `void (*p)(int)` without
  typedef, fn-ptr comparison `==/!=`, returning fn-ptrs from functions. See
  `examples/test_fnptrs_v1.tc` and `test_fnptrs_v2.tc`.
- **Structs by value** (1.4.0) — `struct Tag { int x; float y; char name[16]; }`
  defines a record type. Field access via `.` (`p.x`, `s.label`, `r.tl.x` for
  nested), positional initializer (`Point p = {1, 2}`), array of struct
  (`WriteLog wlog[16]`), whole-struct assignment (`b = a` between matching
  tags) including array↔var both directions, struct as function parameter
  (by value, the callee gets a copy), struct as return value, `sizeof(Tag)`
  returns slot count at compile time. Uses existing 1D-array opcodes — VM
  unchanged. Heap-promotion follows existing rules: structs of ≤16 slots
  stack, arrays of struct >16 total slots auto-heap. Char-array field
  patterns work: `strcpy(s.label, "x")`, `sprintf(buf, "%s", s.label)`,
  `strcat`, `strcmp`. Nested struct field offsets correctly count inner
  struct slots (no off-by-N). Persist'd struct globals work but the v1 hash
  doesn't include field-name lists — silently reordering fields within a
  struct decl after persist data exists won't invalidate `.pvs`. Workaround:
  add+remove a field (which DOES invalidate) or delete `.pvs` manually.
  Out of v1: self-referential structs (needs pointers), 2D-array fields,
  struct equality `a==b`, designated initializers, function-pointer fields.
- **2D arrays** (1.3.38) — `char buf[N][M]`, `int grid[R][C]`, `float coef[R][C]`.
  Element access `arr[i][j]`, write `arr[i][j] = …`, row passing `func(arr[i])`
  to 1D array params, `strcpy/strcat/strcmp` on rows, `sprintf("%s", arr[i])`
  for 2D char. Pure compiler change — VM unchanged (flattens to existing
  1D heap-array opcodes via `i*cols + j`; row refs use `ADDR_HEAP_OFF`).
  Row passing requires heap storage (auto-promoted at >16 total elements,
  so any practical 2D qualifies). 3D+ not supported. Initialisers for 2D
  literals (`int m[2][3] = {{1,2,3},{4,5,6}}`) not accepted yet — initialise
  in `main()` instead.
- **Race-free autoexec start** (1.3.36) — autoexec slot main() now spawns
  on a FUNC_LOOP iteration once `TasmotaGlobal.uptime ≥ 3 s` (override:
  `-DTC_AUTOEXEC_MIN_UPTIME=N`). Previously main() spawned during FUNC_INIT
  and raced Tasmota's own driver init + Wi-Fi/RF coex bring-up — `serialBegin`
  there would silently fail to receive bytes. With the deferral, **plain
  `serialBegin` in `main()` works with no delay**, the same as in any
  in-tree Tasmota driver. Removes the historical `delay(15000)` workaround.
  An optional `BootInit()` callback also exists (fires after main returns)
  for users who prefer to keep main lean — but it's no longer needed for
  correctness.
- **Variadic `sprintf`** — `sprintf(buf, "x=%d t=%.1f s=%s", x, t, name)` works
  (compiler splits the format at compile time). Do **not** copy the older
  `sprintfInt + strcat` chaining idiom into new code.
- **`char name[] = "literal"`** — size auto-inferred from the literal; no
  explicit size, no separate `strcpy` init. Older examples use
  `char name[32]; strcpy(name, "...");` — that still works but isn't needed.
- **`persist`** — layout auto-resets on flash rebuild (v2 `.pvs` format,
  FNV-1a hash). Users no longer need `UfsDelete /*.pvs` after adding/removing
  persist vars. If advice says they do, it's stale.
  - ⚠️ **"auto-resets" means ALL persist values revert to declaration
    defaults, not just the changed ones.** Adding/removing/reordering ANY
    `persist` variable invalidates the *entire* `.pvs` (the FNV-1a hash is
    over the whole layout). On next boot every persisted value — IPs,
    calibration, months of accumulated stats — is back to its code default.
    Treat any persist-layout change as a **full-restore event**: back up
    `.pvs` / all relevant settings (Tasmota config backup) *before* the
    flash. The graceful no-`UfsDelete` behaviour is about not corrupting,
    not about preserving old values.
- **`spawnTask("Name")` / `killTask` / `taskRunning`** — user-defined functions
  can run as dedicated FreeRTOS tasks (ESP32, max 4). Task name must be a
  string literal matching a user function in the same `.tc`.
- **Canvas API** (`imgCreate` / `imgBeginDraw` / `imgEndDraw`) — offscreen
  RGB565 buffer in PSRAM; all `dsp*` primitives redirect to the canvas when
  active. See `examples/voltmeter.tc`.
- **`delay()` inside the mini-scripter** `>F`/`>S` sections is supported
  with a 1 s cap — different subsystem, not relevant to most users.
- **Cross-VM `share*` API** — `shareSetInt/GetInt/SetFloat/GetFloat/SetStr/GetStr/Has/Delete`
  let multiple TinyC slots share named scalars/strings (driver-global table,
  mutex-protected). Use this when a single program outgrows
  `TC_MAX_PROGRAM` (128 KB) and needs to be split across two slots without
  going through MQTT or the filesystem. Keys are short string literals;
  missing-key reads return `0` / `0.0` / `""`. Caps: 32 keys, 16 char key,
  64 char string value (override via `TC_SHARE_*` defines).
  - **`TC_SHARE_MAX` is a global cap across ALL slots combined**, not
    per-slot. In multi-slot setups (4-6 slots × 5-10 shares each) the
    default 32 is reached fast. As of 1.6.6 a full table no longer fails
    silently: each rejected `shareSet*` logs at INFO
    `TCC: shareSetInt("key")=val failed — table full (N/32). Raise
    TC_SHARE_MAX in user_config_override.h.` (one line per failing call).
    From outside, a silently-capped share looks like `shareGetInt("key")`
    constantly returning `0`. Workaround: `#define TC_SHARE_MAX 48` in
    `user_config_override.h` + rebuild. DRAM cost per added slot ≈
    `1 + TC_SHARE_STR_LEN` bytes.
- **PSRAM-backed bytecode** — `.tcb` and the constant string pool are
  allocated in internal DRAM first; on OOM they automatically spill to
  PSRAM (ESP32 only). Small programs stay fast; very large ones still load.
  `TC_MAX_PROGRAM` raised from 64 KB → 128 KB.
- **Symmetric crypto syscalls** — `aesEcb` (AES-128-ECB single block, in-place),
  `aesCbc` (AES-128-CBC, key+iv 16 B, len multiple of 16), `hmacSha256`,
  `sha256`, `hex2bin` / `bin2hex`. ESP32-only via mbedtls (no extra flash
  cost — already linked for HTTPS / MQTT-TLS); ESP8266 stubs return 0. All
  operate in-place on TinyC `char[]` buffers (1 byte per int32 slot).
  Motivating use case: Tuya v3.3 local protocol → `examples/pool_pump.tc`.
  Limits: AES-CBC stack-allocates ≤ 4 KB; HMAC/SHA bounded at 1024 B key /
  4 KB data per call. AES-GCM / ECDH not exposed yet (Tuya v3.4 needs both).
- **Watch + webButton/webSlider works end-to-end** — historically broken:
  `watch int x; webSlider(x, ...);` updated `x` on slider drag but
  `written(x)` stayed false because the URL handler raw-wrote `globals[idx]`,
  bypassing `STORE_WATCH`. Fixed: VM scans bytecode at load for
  `STORE_WATCH` opcodes, builds `watch_indices[]`; `?sv=N_V` (and any
  future out-of-band bridge) goes through `tc_global_write_with_watch()`
  which mirrors the shadow + written-flag updates. So the natural pattern
  `if (written(slider_var)) { dispatch(); snapshot(slider_var); }` now fires
  correctly. `TC_MAX_WATCH = 16` watched globals per VM slot.

- **`webButton` is a momentary action button, not a toggle** — it no
  longer appends `: ON`/`: OFF` to the label (that suffix was confusing
  for the dominant "do something" use case). On click it still pulses
  the bound var (toggle scripts keep working) and briefly shows a
  confirmation on the button itself for ~2.5 s, then reverts. Opt into
  custom confirmation text with an `"Idle|Active"` label:
  `webButton(do_init, "Zaehler initialisieren|initialisiert")`. No `|`
  → generic `✓`. The `|` split is purely server-side rendering — no API
  / compiler change, fully backward compatible (no existing label uses
  `|`). True-toggle UIs that need a visible ON/OFF state should show it
  via a separate `webText`/label — or use `webToggle` (below).
- **`webToggle(var, "Label")` — latching on/off button** (syscall 394).
  The stateful counterpart to the now-momentary `webButton`: a
  full-width button that is **green when the bound var != 0, grey when
  0**, no ON/OFF text suffix. Click flips the var 0↔1. Use this for
  lights, pump-enable, mode switches — anything where the user must see
  current state at a glance. Optional per-state label/emoji via an
  `"On text|Off text"` label: `webToggle(light, "💡 An|🌙 Aus")` shows
  "💡 An" (green) when on, "🌙 Aus" (grey) when off. No `|` → same text
  both states, colour only. `webCheckbox` still exists for a classic
  checkbox; `webToggle` is the button-styled equivalent that matches the
  push-button aesthetic.

Features documented in `TinyC_Reference.md` VM-limits table may understate:
on ESP32, constant pool is **1024**, heap is **64 KB** (`TC_MAX_HEAP`
16384 slots × 4 B — default; overridable in `user_config_override.h`),
code size is **128 KB** (the table section in the reference is behind).

- **Firmware flash footprint of the TinyC subsystem ≈ 196 KB** (hand-measured
  on `tinyc32-4M`, 1.6.8). This is the *whole* `xdrv_124` driver (VM + ~70
  syscalls + web widgets + browser-IDE serving + HW/HomeKit/camera bridges),
  NOT the bare interpreter loop — the old README "~12 KB" claim was the
  interpreter only and was corrected. On the 4 MB ESP32 (`app1856k`, 1856 KB
  ceiling) TinyC is the single largest optional subsystem, bigger than
  HomeKit (≈152 KB). Consequence: **HomeKit ships only in S3/16 MB builds**;
  4 MB envs force-drop `-DTINYC_HOMEKIT`. `USE_TINYC` is currently *not*
  cleanly removable (unguarded couplings in xdrv_55_touch — fixed — and
  xdrv_01_2_webserver_esp32_mail `WcGetPicstore` — open). See
  `TinyC_Custom_Builds.md` → Flash Budget for the full measured table.

---

## 5. Callbacks — recognised names only

Define a function with one of these exact names and Tasmota will call it —
no registration needed. Full table in `TinyC_Reference.md §Callback Functions`.

Common:
`EveryLoop`, `Every50ms`, `Every100ms`, `EverySecond` — periodic ticks.
`BootInit` — optional convenience callback, fires once after main() returns. Since 1.3.36 the runtime defers autoexec spawn until Tasmota is fully up, so plain `serialBegin` in `main()` works race-free — `BootInit` is not required for correctness, just a place to put hardware init separately from main if you prefer that style. For "WiFi got an IP" semantics use `OnWifiConnect` / `OnInit`.
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
| Output | `addLog("fmt %d %s", a, b)` (variadic), `sprintf`, `responseCmnd`, `responseAppend`, `webSend` |
| GPIO | `pinMode`, `digitalRead`, `digitalWrite`, `analogRead`, `dacWrite` |
| Time | `millis`, `delay`, `tasm_hour/minute/second/…`, `time()`, `strftime` |
| Timers | `timerSet`, `timerCancel` (software) |
| Serial | `serialBegin(rx, tx, baud, cfg, buf)` returns slot 0..2; `serialRead(slot)`, `serialWrite(slot, str)`, `serialAvailable(slot)` (multi-port API; slot is first arg) |
| I²C | `i2cRead8`, `i2cWrite8`, `i2cReadBuf`, `i2cWriteBuf`, `i2cScan` |
| SPI | `spiInit`, `spiTransfer`, `spiSetCs` |
| 1-Wire | `owReset`, `owRead`, `owWrite`, `owSearch` |
| Files | `fileOpen`, `fileRead`, `fileWrite`, `fileClose`, `fileDelete`, `fileSize`, `fileReadBin`/`fileWriteBin` |
| HTTP | `httpGet`, `httpPost`, `httpHeader` |
| TCP | `tcpConnect`, `tcpRead`, `tcpWrite`, `tcpAvailable`, `tcpSelect` (slot 0..3); tuning: `tcpKeepalive`, `tcpNoDelay`, `tcpTransact`, `tcpDisconnectReason` |
| UDP (Scripter globalvars) | `udpSend(name, val)`, `udpRecv(name)`, `udpReady(name)`, `udpRecvArray(name, arr, max)` |
| UDP (general) | `udp(N, args…)` dispatcher: 0=open, 1=read, 2=reply, 3=send-to-url, 9=join-mcast, 10=igmp-leave, etc. (see Reference.md §General-Purpose UDP) |
| MQTT | `mqttSubscribe`, `mqttUnsubscribe`, `mqttPublish` (const-pool string args; ESP32 USE_MQTT) |
| mDNS | `mdnsAdvertise` |
| Display | `dspText`, `dspPixel`, `dspLine`, `dspRect`, `dspCircle`, `dspColor`, `dspUpdate` (e-paper only) |
| Image bridge | `dspLoadImageFromCam(cam) → img_slot`, `dspImgTextBurn(slot, x, y, color, w, align, text)`, `dspImageToCam(img, cam, quality)` |
| Canvas | `imgCreate`, `imgBeginDraw`, `imgEndDraw`, `imgClear`, `imgBlit`, `imgFlush`, `dspPushImageRect` |
| TinyUI widgets | `uiTheme`, `uiScreen`, `uiClearScreen`, `uiLabel`, `uiLabelSet`, `uiProgress`, `uiProgressSet`, `uiGauge`, `uiCheckbox`, `uiButton`, `uiIcon` |
| WebChart (Google) | `WebChart`, `WebChartSize`, `WebChartTimeBase` |
| Web raw HTTP | `webRawMode`, `webRawWrite`, `webKeepAlive` (for clients that expect specific headers / persistent sockets — EcoTracker, Jackery, etc.) |
| Audio | `i2sBegin`, `i2sWrite`, `i2sStop`, `fileReadPCM16` |
| Camera | `camControl(sel, p1, p2)` (multiplexed: 0=init, 10=capture, 11=save-to-file, 12=free, 13=deinit, …) |
| SML | `smlGet`, `smlGetStr`, `smlWrite`, `smlRead`, `smlSetOpt`, `smlApplyPins`, `smlScripterLoad` |
| Sensor JSON | `sensorGet("ENERGY#Power")` parses live Tasmota SensorJSON |
| WS2812 LEDs | `setPixels(arr, count, pin)` |
| HomeKit | `hkAdd`, `hkVar`, `hkReady`, `hkStart`, `hkInit`, `hkStop`, `hkReset`, `hkSetCode`, `HomeKitWrite` callback |
| Tasks (ESP32) | `spawnTask("Name"[, stackKB])`, `killTask`, `taskRunning` |
| Crypto (ESP32) | `aesEcb` (AES-128-ECB, 1 block), `aesCbc`, `hmacSha256`, `sha256`, `hex2bin` / `bin2hex` |
| TWAI / CAN (ESP32) | `twaiBegin(rx, tx, kbits, mode)`, `twaiSend`, `twaiRecv`, `twaiAvailable`, `twaiStatus`, `twaiFilter`, `twaiEnd` |
| Persist | `persist` decl, `saveVars()` |
| Watch | `watch` decl, `changed`, `delta`, `written`, `snapshot` |
| Cross-VM share | `shareSetInt/GetInt`, `shareSetFloat/GetFloat`, `shareSetStr/GetStr`, `shareHas`, `shareDelete`, `shareDump` (diagnostic) — string-literal keys |
| Tasmota commands | `tasmCmd("Cmd")`, `tasmDefer("Cmd")` (queue for main task), `tasmInfo(sel, buf)` |
| Diagnostics | `dumpVM()`, `vmStackDepth()` |
| Binary libs (BLIB) | `bcall("name", buf, len)` → int — call a native function exported by a `MODULE_TYPE_BLIB` plugin (e.g. `xblib_01_crc`'s `mb_crc16`, `crc32`, `crc8_dallas`). Name is a string literal; phase-1 ABI is `(BUF, INT) → INT` only. |

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
11. **Enabling SML on a device that still has a Scripter `>S` section** —
    Rule1 bit 0 gates BOTH the SML driver AND Scripter execution, so the
    moment you flip SML on, any leftover ottelo-style chart script will
    start emitting its own `setOnLoadCallback` / chart HTML alongside your
    TinyC `WebPage()`. Result: collided chart targets, last `drawChart`
    wins, page renders as a mess. When porting Scripter → TinyC, clear the
    Scripter source via IDE *Tools → Edit Script* before activating SML.
    See `TinyC_Reference.md §Smart Meter (SML)` for the full callout.

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

### Slot-restart cleanup (close TCP/UDP sockets cleanly before main() reruns)

When a slot is stopped and restarted (via `TinyCStop`/`TinyCRun`, or via IDE
"Run" after edit), any persistent TCP clients or UDP sockets the script
opened are still held by lwIP at the moment main() runs again. The script
will happily call `tcpConnect()` / `udpBegin()` again, but the peer (e.g.
a Modbus-TCP slave at a battery BMU) may still see a half-open session
from the previous run and refuse the new one until its own timeout fires —
typically 30 s of pointless errors at slot startup.

`CleanUp()` is a recognised callback that fires once when the slot is
asked to stop, before the VM tears down. Use it to release sockets:

```c
void CleanUp() {
    // Close any TCP-client slot you opened
    if (mb_slot >= 0) {
        tcpSelect(mb_slot);
        tcpDisconnect();
    }
    // Drop UDP-multicast membership and close the socket cleanly
    udp(0, 9522);
}
```

After this lands, the next slot run sees a clean socket table and can
reconnect immediately. Verified live on Andreas's Bat3 setup (BMU + SMA
inverter Modbus-TCP, 11.5 h with `byd_err = 0`) on 14.05./15.05.

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
**Cross-VM share (ESP32)** — `share_writer.tc` (slot 0 EverySecond writer) + `share_reader.tc` (slot 1 Command reader)
**Binary libs (BLIB)** — `blib_crc_demo.tc` (calls `mb_crc16` / `crc32` / `crc8_dallas` exported by the `xblib_01_crc` plugin)
**HomeKit (ESP32)** — `homekit_demo.tc`, `homekit_office.tc`
**Strings / sort** — `strings.tc`, `sort.tc`, `file_io.tc`
**2D arrays** — `test_2d.tc` (char 2D), `test_2d_phase2.tc` (int + float 2D + sprintf %s)
**Structs** — `structs_demo.tc` (real-world wlog-ring + sizeof + struct return), `test_structs_v1.tc` (smoke tests across all v1 patterns)
**Function pointers** — `test_fnptrs_v1.tc` (typedef'd fn-ptrs as locals, globals, parameters, dispatch-by-id), `test_fnptrs_v2.tc` (fn-ptrs as struct fields → dispatch table)
**Reference parameters** — `test_refparams_v1.tc` (swap, multi-out, compound `n += 5`, globals as refs)
**String ops** — `test_strings_v15.tc` (replace/starts/ends/contains/upper/lower/trim with grow+shrink edge cases)
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

**Reclaim flash for FS on safeboot devices**: `TinyCChkpt` shows the
partition table; `TinyCChkpt p` repacks — shrinks `app0` to fit the
current sketch + 192 KB headroom, gives all freed flash to `spiffs`.
Common use: 16 MB device flashed with a generic `app0=3008 KB` slot
where the sketch is actually only 1.5 MB — packing returns the rest
to the filesystem. Refuses if there's no `safeboot` partition (no
recovery path) or if the requested app size is smaller than the
running sketch (would brick on next OTA). **Wipes LittleFS** as a
side effect — back up `Settings.json` and any user files first.
`TinyCChkpt p 2880` to set an explicit KB size instead of auto.

---

## 11. Known gaps / "don't retry"

- **`SYS_FILE_WRITE_STR`** — compiler emits the opcode but VM has no handler
  yet (`TC_ERR_BAD_SYSCALL` at runtime). Track upstream; don't generate code
  that relies on it.
- **`SYS_STRCONCAT` (259)** — same shape: IDE emits the opcode when a script
  uses `"a" + "b"` (string-`+` concat), but firmware has no case-handler for
  ID 259 → `TC_ERR_BAD_SYSCALL`. Avoid string-`+`; use `strcpy` + `strcat`
  (or `sprintf` for variadic builds) instead.
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
