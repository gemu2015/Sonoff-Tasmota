# Text to speech as a plugin (PICOTTS)

*Deutsch: [PICOTTS_PLUGIN_DE.md](PICOTTS_PLUGIN_DE.md)*

The speech engine (SVOX Pico, six languages) used to be part of the firmware (`-DTINYC_TTS`, about
125 KB of flash), so every firmware that should speak needed its own build. Since 1.6.70 it is also
available as a plugin: **one standard firmware, `PICOTTS_32.bin` on top**. The command is the same as
before — `I2STTS <text>` of the audio plugin (`I2SAUDIO`), which you need in any case.

⚠️ Tested on an ESP32-S3 (16 MB, 8 MB PSRAM). Xtensa chips with PSRAM only (`_32` = S3 / ESP32); the
engine needs about 1.1 MB of PSRAM plus the voice (about 1 MB).

## What you need

| | |
|---|---|
| Firmware | the standard S3 firmware (no `-DTINYC_TTS`; if the firmware has the engine built in, that one is used and the plugin is ignored) |
| Plugins | `I2SAUDIO_32.bin` **and** `PICOTTS_32.bin` from the [testing release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) |
| Plugin partition | I2SAUDIO (102 KB) + PICOTTS (144 KB) fit into 256 KB: `chkpt a4`. With the Matter plugin as well (61 KB) use `chkpt a8` (512 KB, firmware 1.6.70 or newer) |
| Voice files | two files per language, from `lib/libesp32_div/pico/lang/` of the repository, **renamed** (below) |

## Step by step

1. **Plugin partition**, once: `chkpt a4` (or `a8`). ⚠️ This formats the file system, see
   [MATTER_PLUGIN.md](MATTER_PLUGIN.md) for what is lost.
2. **Upload both plugins:** Tools → Plugins directory → `I2SAUDIO_32.bin`, then `PICOTTS_32.bin`.
3. **Pins of the audio plugin** (`I2SAUDIO`, DOUT / BCK / WS …): set them on the plugin page, or put an
   `I2SAUDIO.cfg` into the root of the file system.
4. ⚠️ **Tick "Autostart plugins at boot"** on the plugin page — the audio plugin is not started otherwise.
5. **Restart.** Without the restart the plugin is loaded but not started (`PTT: no PICOTTS plugin and no
   built-in engine` in the log). The same after every replacement of a plugin.
6. **Voice files** into the root of the file system (the SD card if there is one, else the flash):

   | In the repository | Name on the device |
   |---|---|
   | `de-DE_ta.bin` | `/picotts_de-DE_ta.bin` |
   | `de-DE_gl0_sg.bin` | `/picotts_de-DE_sg.bin` |
   | `en-US_ta.bin` | `/picotts_en-US_ta.bin` |
   | `en-US_lh0_sg.bin` | `/picotts_en-US_sg.bin` |
   | `en-GB_ta.bin`, `en-GB_kh0_sg.bin` | `/picotts_en-GB_ta.bin`, `/picotts_en-GB_sg.bin` |
   | `fr-FR_ta.bin`, `fr-FR_nk0_sg.bin` | `/picotts_fr-FR_ta.bin`, `/picotts_fr-FR_sg.bin` |
   | `it-IT_ta.bin`, `it-IT_cm0_sg.bin` | `/picotts_it-IT_ta.bin`, `/picotts_it-IT_sg.bin` |
   | `es-ES_ta.bin`, `es-ES_zl0_sg.bin` | `/picotts_es-ES_ta.bin`, `/picotts_es-ES_sg.bin` |

   Files above 256 KB go through port 83 automatically in *Manage File System*.

## Use

```
I2STTS Guten Tag, hier spricht die Kamera
I2STTSLang              the language that is loaded (default de-DE)
I2STTSLang en-US        switch; the voice is reloaded at the next I2STTS
```

At the first `I2STTS` the log shows `PTT: loaded /picotts_de-DE_ta.bin … -> PSRAM`, `PTT: plugin self check ok`
and `PTT: ready`. After every sentence one line `PTT: utterance N samples, peak …, rms …; engine … ms,
output callback … ms` — handy to see how long the engine really needs (about 0.6 s of computing per
second of speech on an S3).

Memory: about 2.5 MB of PSRAM while a voice is loaded (arena 1.1 MB + voice 1–1.4 MB); switching back and
forth does not leak (measured: 3200 KB free with German, 2848 KB with English, constant over four rounds).

## When it does not work

| Symptom | Cause |
|---|---|
| `Unknown` for `I2STTS` | the audio plugin is not running: autostart, pins, restart |
| `PTT: no PICOTTS plugin and no built-in engine` | `PICOTTS_32.bin` missing, or not restarted after the upload |
| `PTT: voice load failed` | a voice file is missing or in the wrong file system (SD present: the plugin looks on the SD) |
| `PTT: plugin self check FAILED (mask …)` | the plugin does not match the firmware (jump table); use the plugin of the same release |
| Upload says `MEM` / `flash slot memory error` | the partition is full or fragmented; firmware before 1.6.70 could not replace a plugin by one of the same size — remove another plugin first, or `chkpt a8` |

## For developers

The plugin is generated from the untouched `lib/libesp32_div/pico`; see `tasmota/Plugins/picotts/test/README.md`
for the tools, the host tests and the one compiler problem that had to be worked around
(`acphAccentuation` is built at `-O0`).
