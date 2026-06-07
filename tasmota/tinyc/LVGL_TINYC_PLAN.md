# LVGL-in-TinyC — phased implementation plan (P4-first)

Status: **PLAN / finalized (gemu approved scope) — ready to start Phase 0.**

**Dev/test target = S3 NOW, P4 later.** `USE_TINYC_LVGL` is board-agnostic (any `USE_UNIVERSAL_DISPLAY`
build), so we develop and verify *on screen* on gemu's existing hardware before the P4 arrives:
- **Primary dev board:** gemu's ESP32-S3 + **ILI9488 16-bit parallel** display @ **192.168.188.135**
  (16 MB + QSPI PSRAM). Built from the `device_ILI9488p16` config block (top-of-file default toggle)
  via `[env:tinyc32s3-p16]`. That block already has `USE_TINYC`+`USE_UNIVERSAL_DISPLAY`+
  `USE_UNIVERSAL_TOUCH` — add `USE_TINYC_LVGL`. ILI9488 = 320×480, 16bpp (tiny partial buffer).
  Already runs TinyC display programs (analog_clock.tcb) → renderer flush path proven on this panel.
- **Later:** `device_p4_full` (10.1" JD9365 800×1280 DSI + GT911) — same gate, bigger panel/PSRAM.

Resolved scope (gemu): (1) the ~11-widget set is enough — NO textarea/keyboard/dropdown/roller/list/
table for now; (2) NO coexistence — LVGL takes the framebuffer exclusively when active (suspend the
tc_ui immediate-mode drawing + `/tc_display` mirror while LVGL is up); (3) defaults OK (partial+PSRAM
buffer, built-in Montserrat fonts, `lvgl*` naming).

**Dev loop on .135:** build `tinyc32s3-p16` (with `device_ILI9488p16` default + `USE_TINYC_LVGL`) →
OTA (device-pull `OtaUrl`+`Upgrade 1`, or POST `/u2`) → upload a test `.tc` (`/tc_upload`) → run →
look at the screen + `/tc_api?cmd=status`. App-only OTA preserves creds/FS.

## 1. Goal & scope

Expose a **curated subset** of LVGL 9.5 to TinyC programs as `lvgl*` syscalls, all behind a new
`USE_TINYC_LVGL` build gate (off everywhere except the P4-full env initially). Give TinyC a
**retained-mode, touch-interactive on-device GUI** (buttons, sliders, charts, lists, …) on the big
panel — complementing, not replacing, the existing immediate-mode drawing (`drawPixel/drawLine`),
the web-tile UI, and the `/tc_display` mirror.

Non-goals: the Berry `lv_binding_berry` auto-binding, haspmota, full LVGL API parity, animations
engine exposure (beyond a simple anim flag), custom-font tooling (reuse built-in fonts first).

## 2. Architecture — what's reused vs new

**Already exists, Berry-FREE (reuse as-is):**
- `xdrv_54_lvgl.ino` (gated `USE_LVGL && USE_UNIVERSAL_DISPLAY`, **not** Berry):
  - `start_lvgl(uconfig)` → `lv_init()` + display + draw buffer
  - `lv_flush_callback` → `renderer->` (the uDisplay abstraction already wired to the P4 JD9365)
  - `FUNC_LOOP` → `lv_timer_handler`; `lv_tick_handler` → `lv_tick_inc`
  - touch indev: `lvgl_touchscreen_read` → `Touch_Status()` (= our GT911 via uDisplay universal touch)
- LVGL 9.5 core `lib/libesp32_lvgl/lvgl/` — pure C, **0 Berry refs**, standalone MIT lib.

**New (this plan):**
1. Build wiring: `USE_TINYC_LVGL` gate; compile the `lvgl` core but NOT `lv_binding_berry`/`lv_haspmota`;
   a TinyC-tuned `lv_conf.h`; a non-Berry caller for `start_lvgl()`.
2. `xdrv_124_lvgl_glue` (new `.ino`, plain-C `tc_lv_*` surface — mirrors the BLE-glue split so no LVGL
   type crosses into the VM file; sorts AFTER xdrv_124 so the VM can forward-declare `tc_lv_*`).
3. The `lvgl*` syscall block in `xdrv_124_tinyc_vm.h` + IDE `opcodes.js`/`codegen.js` + rebundle.
4. A **handle table** (int↔`lv_obj_t*`) and an **event ring** (poll model — no VM reentrancy).
5. An **LVGL lock** (single mutex around `lv_timer_handler` and every `lvgl*` syscall).

## 3. Concurrency / safety design (the #1 risk)

LVGL is NOT reentrant. The TinyC VM may run on its own task while `lv_timer_handler` runs on the
main task (`FUNC_LOOP`). Therefore:
- One recursive `lvgl_lock` (FreeRTOS mutex). `xdrv_54` FUNC_LOOP takes it around `lv_timer_handler`;
  every `lvgl*` syscall takes it for the duration of the LVGL call(s).
- Event callbacks (`lv_event_cb`) run inside `lv_timer_handler` (main task, lock already held) →
  they only **push** `{handle, code}` into a ring (publish-after-fill) + set a count. The VM **polls**
  `lvglEvent()` on its task (BLE precedent: `bleNext()`/`bleDone()`/`bleResult()`). **No C→VM call.**
- Keep syscall critical sections short; never hold `lvgl_lock` across a blocking call.
- This honors the established `vm_mutex`/matter/BLE/ufsu task-safety lessons.

## 4. Handle model

- `static lv_obj_t* lv_handles[N];` (N≈128, in PSRAM). Int handle = index+1; 0 = invalid/NULL.
- Every created object gets an `LV_EVENT_DELETE` cb that nulls its slot → no dangling handles when
  LVGL auto-deletes children (`lvglClean`/screen swap/parent delete).
- Chart series use a parallel small table (`lv_chart_series_t*`), or pack series into the same space
  with a type tag. Syscalls validate range + non-null before every use → bad bytecode can't crash LVGL.

## 5. Syscall surface (subset, ~45) — new SYS block 450+

`lvglInit()`/`lvglActive()` plus screen mgmt; object create (parent→child handle); common props;
widget-specific; events. Examples (final names/numbers fixed in Phase 1):

```
// lifecycle / screen
lvglInit()                         -> 1     idempotent start_lvgl()
lvglActive()                       -> bool
lvglScreenActive()                 -> handle
lvglScreenCreate()                 -> handle
lvglScreenLoad(h)                  -> 1
lvglClean(h) / lvglDelete(h)       -> 1
// create (parent handle, 0 = active screen)
lvglObj(p) lvglLabel(p) lvglButton(p) lvglSlider(p) lvglBar(p)
lvglArc(p) lvglSwitch(p) lvglCheckbox(p) lvglChart(p) lvglImage(p) lvglLine(p)  -> handle
// common props
lvglSetPos(h,x,y)  lvglSetSize(h,w,ht)  lvglAlign(h,align,dx,dy)
lvglSetText(h,str)          [strArg]     lvglSetValue(h,v,anim)  lvglGetValue(h)->int
lvglSetRange(h,min,max)     lvglSetBgColor(h,rgb888[,part])      lvglSetTextColor(h,rgb888)
lvglSetHidden(h,b)          lvglAddFlag(h,f) lvglClearFlag(h,f)  lvglSetStyleInt(h,prop,val[,part])
// widget-specific
lvglChartAddSeries(h,rgb)->series    lvglChartSetNext(h,series,v)  lvglChartSetCount(h,n)
lvglChartSetRange(h,axis,min,max)    lvglImageSrc(h,path)  [strArg]
// events (poll)
lvglEventEnable(h,filter)            lvglEvent()->1/0 (pops into current)  lvglEventObj()->handle  lvglEventCode()->int
```

Each syscall = 1 line in `opcodes.js` (number+comment) + 1 in `codegen.js` BUILTINS
(`{syscall, args, returns[, returnFloat][, strArgs:[i]]}`) + 1 `case` in `xdrv_124_tinyc_vm.h`,
then `bundle.py` → `tinyc_ide.html(.gz)`.

## 6. Build wiring

- Add `USE_TINYC_LVGL` (implies `USE_LVGL` + requires `USE_UNIVERSAL_DISPLAY`); `#error` if missing,
  like the BLE `USE_TINYC_BLE→USE_BLE_ESP32` guard.
- platformio envs (`tinyc32s3-p16` first, `tasmota32p4-full` later): **remove `libesp32_lvgl` from
  lib_ignore**, but lib_ignore the Berry-coupled sub-libs **by name**: `Berry mapping to LVGL`,
  `lv_haspmota` (and `freetype`/`LVGL assets` unless a font/asset is needed). Verify the LDF only
  pulls the `lvgl` core.
- `lv_conf.h`: enable ONLY the subset widgets (bounds flash); RGB565; partial draw buffer in PSRAM
  (ILI9488 320×480: e.g. 320×40×2B ×2 ≈ 50 KB; P4 800×1280 ≈ 128 KB); confirm `LV_COLOR_DEPTH 16`
  matches the panel `:H ...,16,...`. (lv_conf is shared, so widget enables must suit both boards.)
- user_config_override `device_ILI9488p16` (now) + `device_p4_full` (later): `#define USE_TINYC_LVGL`
  (gitignored, like the BLE flags).

## 7. Phases

- **Phase 0 — build wiring + skeleton:** `USE_TINYC_LVGL` gate; compile lvgl core into `tinyc32s3-p16`;
  `lvgl_lock` around `lv_timer_handler`; a non-Berry `start_lvgl()` caller; `lvglInit()`/`lvglActive()`
  syscalls only. **Gate: green build, flash delta logged.**
- **Phase 1 — core objects + events (FIRST ON-SCREEN RESULT on .135):** handle table + `LV_EVENT_DELETE`
  cleanup; `lvglObj/Label/Button`; `SetPos/Size/Align/Text/BgColor/TextColor`; event ring +
  `lvglEventEnable/Event/EventObj/EventCode`. Deliverable: a `.tc` where a button toggles a label —
  OTA to .135, **tap it on the real ILI9488 panel**, confirm the label changes. **Gate: visible + green.**
- **Phase 2 — input widgets:** `Slider/Bar/Arc/Switch/Checkbox`; `SetValue/GetValue/SetRange`; a few
  `SetStyleInt` props (radius/border/opa/pad). **Gate: drag a slider on .135 → value updates.**
- **Phase 3 — data widgets:** `Chart` (+series/next/count/range), `Image` (FS src via ufilesys),
  `Line`. **Gate: live chart on .135.**
- **Phase 4 — DX:** example `.tc` programs (e.g. `lvgl_demo.tc`, a live SML/power dashboard);
  sync `TinyC.tmLanguage` + `opcodes.js`/`codegen.js`; `TinyC_Reference.md` (EN) + `_DE`; rebundle IDE.
- **Phase 5 — P4 bring-up (when board arrives):** enable the gate in `device_p4_full`, retune the
  shared `lv_conf` draw buffer for 800×1280, GT911 hit-testing, FUNC_LOOP refresh-budget on the big
  panel, memory soak.

## 8. Cost
Flash ~+150–250 KB (core + subset); now at 46.3% of ~4 MB → ample. RAM: partial draw buffer in PSRAM
(~128 KB) — trivial on P4 16–32 MB.

## 9. Resolved (gemu)
1. Widget scope: ~11 widgets enough; no text-input widgets for now. ✓
2. Coexistence: not needed — LVGL exclusive (suspend tc_ui drawing + `/tc_display` mirror while active). ✓
3. Defaults OK: partial+PSRAM draw buffer, built-in Montserrat fonts, `lvgl*` naming. ✓
4. Panels: develop/test on S3 (ILI9488 @ .135) now; P4 later. ✓ (USE_TINYC_LVGL is board-agnostic.)
