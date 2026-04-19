# Merging upstream Tasmota into this fork

This fork carries ~3,000 commits of fork-specific work on top of
[arendst/Tasmota](https://github.com/arendst/Tasmota). This file documents how
to absorb upstream changes without losing fork modifications.

## One-time setup

```bash
git remote add upstream https://github.com/arendst/Tasmota.git
git config merge.ours.driver true
```

The second command activates the `merge=ours` driver referenced in
`.gitattributes`. It is built into git but must be enabled per clone.

## Routine merge

```bash
./scripts/merge_upstream.sh
```

The script:

1. Fetches `upstream/development`.
2. Creates a throwaway branch `merge-upstream-YYYY-MM-DD` off `universal`.
3. Runs `git merge`. Files marked `merge=ours` keep the fork's version
   automatically.
4. Lists remaining conflicts grouped by directory.

After resolving conflicts, build and test, then:

```bash
git commit                              # writes the merge commit
git checkout universal
git merge --ff-only merge-upstream-…    # promote
```

To abort cleanly:

```bash
git merge --abort
git checkout universal
git branch -D merge-upstream-…
```

## What's auto-resolved (`merge=ours` in `.gitattributes`)

| Path | Reason |
|------|--------|
| `tasmota/tinyc/**` | Entire TinyC subsystem — only this fork has it |
| `tasmota/include/xdrv_124_tinyc*.h` | TinyC VM headers |
| `tasmota/tasmota_xdrv_driver/xdrv_124_tinyc.ino` | TinyC driver entry point |
| `tinyc_docs/**`, `mkdocs.yml`, `requirements-docs.txt` | Docs site |
| `scripts/build_tinyc_docs.py` | Docs pre-build |
| `.github/workflows/tinyc_docs.yml` | Pages deploy |
| `lib/lib_display/Display_Renderer-gemu-1.0/**` | Fork-branded display renderer |
| `README.md` | Contains the TinyC banner |

If upstream ever adds a file at one of these paths, the fork wins. That's the
intended behaviour — these paths are wholly owned.

## Files that will conflict — the manual-review zone

These files contain both upstream evolution and fork-specific changes; they
must be merged by hand each time. Categorized by risk.

### High-traffic hotspots (review first)

Heavy fork modifications, also frequently touched by upstream:

| File | Fork +/- | Notes |
|------|---------:|-------|
| `lib/default/TasmotaWire/src/core_esp8266_si2c.cpp` | +1066/-230 | Bit-banged I2C — fork has extensive additions |
| `lib/default/TasmotaWire/src/Wire.cpp` | +246/-104 | Companion to above |
| `lib/default/TasmotaWire/src/Wire.h` | +53/-38 | API surface |
| `lib/default/TasmotaWire/src/twi.h` | +16/-0 | Pure additions, low conflict risk |
| `lib/default/TasmotaWire/src/twi_class.h` | +67/-3 | Mostly additive |
| `tasmota/tasmota_xdrv_driver/xdrv_10_scripter.ino` | +199/-123 | Scripter — actively evolved on both sides |
| `tasmota/tasmota_xsns_sensor/xsns_53_sml.ino` | +175/-24 | SML driver — fork has Modbus TCP write + bus options |
| `tasmota/tasmota_xdrv_driver/xdrv_15_pca9685_v2.ino` | +314/-234 | Fork v2 variant of PCA9685 |
| `tasmota/tasmota_xdrv_driver/xdrv_15_pca9685.ino` | +67/-110 | Original v1 also modified |
| `tasmota/tasmota_xdrv_driver/xdrv_56_rtc_chips.ino` | +110/-333 | Pruned + extended |
| `tasmota/tasmota_xdrv_driver/xdrv_81_esp32_webcam_task.ino` | +177/-114 | Webcam TaskLoop |
| `tasmota/tasmota_xdrv_driver/xdrv_83_esp32_watch.ino` | +454/-2 | Almost entirely additive |
| `lib/lib_display/UDisplay/**` | varies | uDisplay diverged from upstream |
| `lib/lib_display/LiquidCrystal_I2C-1.1.3/**` | varies | I2C LCD lib variant |

**Recommendation when conflict hits**: open the file with three-way diff
(`git mergetool` or VSCode), keep your blocks where they don't overlap with
upstream additions, accept upstream where they're untouched by you.

### Berry / LVGL territory (mostly upstream-canonical)

The Berry interpreter and LVGL files are usually safer to take from upstream
unless you remember a specific local tweak. Common conflicts:

- `lib/libesp32/berry/src/be_vm.c` (+33/-66)
- `lib/libesp32/berry/src/be_lexer.c` (+11/-12)
- `lib/libesp32/berry/src/be_strlib.c`, `be_introspectlib.c`
- `lib/libesp32/berry_int64/src/be_int64_class.c` (+306/-371) — **fork has int64 changes**, review before accepting upstream
- `lib/libesp32/berry_matter/src/solidify/solidified_Matter_z_Commissioning.h` — generated file, regenerate after merge
- `tasmota/berry/extensions/LVGL_Panel/**`
- `tasmota/berry/extensions/LoRaWan_Decoders/**`
- `lib/libesp32/berry_tasmota/src/solidify/solidified_lv_tasmota.h` — generated

### Build / config (review carefully)

These touch every build; conflicts here are common and consequential:

- `platformio.ini`, `platformio_tasmota32.ini`, `platformio_tasmota_env32.ini`
- `platformio_override_sample.ini`, `platformio_tasmota_cenv_sample.ini`
- `tasmota/include/tasmota_options.h` — feature flag bits, reorder-sensitive
- `tasmota/include/tasmota.h`, `tasmota_globals.h`, `tasmota_template.h`
- `tasmota/my_user_config.h`
- `pio-tools/post_esp32.py`, `port-vsc.py`, `tasmotapiolib.py`

For `tasmota_options.h` in particular: be careful with bit-flag reordering.
Upstream may have added new flags between yours; preserve the bit numbers.

### Trivial (53 files with ≤3 line changes)

Mostly minor sensor driver tweaks — `xsns_08_htu21.ino`, `xsns_12_ads1115.ino`,
`xsns_14_sht3x.ino`, etc. Usually merge cleanly without manual help. If they
do conflict, accept upstream unless you remember why you changed them.

## After the merge

1. **Build all four test targets**:
   ```
   tinyc32-4M, tinyc32s3, tinyc32c3, tinyc8266-4M
   ```
2. **Smoke-test on hardware** — at minimum: TinyC IDE loads, an example
   compiles + runs, SML descriptor parses.
3. **Re-generate solidified Berry files** if any `.be` source changed.
4. **Push** — the gh-pages workflow auto-rebuilds the docs site if anything
   in `tinyc_docs/`, `tasmota/tinyc/`, or the docs config changed.

## Tips

- **Merge often, in small batches.** Once a quarter is brutal; once a month
  is manageable; once a week is trivial. Pick a rhythm.
- **Cherry-pick specific upstream fixes** when you don't want a full merge:
  ```bash
  git cherry-pick <sha-from-upstream>
  ```
- **If a `merge=ours` file genuinely needs an upstream change** (e.g.
  upstream fixes a real bug in `xdrv_124_tinyc.ino`), temporarily disable
  the rule:
  ```bash
  git checkout merge-upstream-… upstream/development -- path/to/file
  # then manually merge in your changes
  ```
- **Generated files** (Berry solidify outputs) should be regenerated post-merge
  rather than merged hunk-by-hunk.
