# Sprachausgabe als Plugin (PICOTTS)

*English: [PICOTTS_PLUGIN.md](PICOTTS_PLUGIN.md)*

Die Sprach-Engine (SVOX Pico, sechs Sprachen) steckte bisher in der Firmware (`-DTINYC_TTS`, rund 125 KB
Flash), jede Firmware, die sprechen sollte, brauchte also einen eigenen Bau. Seit 1.6.70 gibt es sie auch als
Plugin: **eine Standard-Firmware, darauf `PICOTTS_32.bin`**. Der Befehl bleibt derselbe — `I2STTS <Text>` des
Audio-Plugins (`I2SAUDIO`), das du ohnehin brauchst.

⚠️ Getestet auf einem ESP32-S3 (16 MB, 8 MB PSRAM). Nur Xtensa-Chips mit PSRAM (`_32` = S3 / ESP32); die Engine
braucht rund 1,1 MB PSRAM plus die Stimme (etwa 1 MB).

## Was du brauchst

| | |
|---|---|
| Firmware | die Standard-S3-Firmware (ohne `-DTINYC_TTS`; hat die Firmware die Engine eingebaut, wird diese benutzt und das Plugin ignoriert) |
| Plugins | `I2SAUDIO_32.bin` **und** `PICOTTS_32.bin` vom [Testing-Release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) |
| Plugin-Partition | I2SAUDIO (102 KB) + PICOTTS (144 KB) passen in 256 KB: `chkpt a4`. Mit dem Matter-Plugin (61 KB) dazu `chkpt a8` (512 KB, Firmware 1.6.70 oder neuer) |
| Stimmdateien | je Sprache zwei Dateien aus `lib/libesp32_div/pico/lang/` im Repository, **umbenannt** (unten) |

## Schritt für Schritt

1. **Plugin-Partition**, einmalig: `chkpt a4` (oder `a8`). ⚠️ Das formatiert das Dateisystem, was dabei verloren
   geht, steht in [MATTER_PLUGIN_DE.md](MATTER_PLUGIN_DE.md).
2. **Beide Plugins hochladen:** Werkzeuge → Plugins directory → `I2SAUDIO_32.bin`, dann `PICOTTS_32.bin`.
3. **Pins des Audio-Plugins** (`I2SAUDIO`, DOUT / BCK / WS …): auf der Plugin-Seite setzen, oder eine
   `I2SAUDIO.cfg` ins Hauptverzeichnis des Dateisystems legen.
4. ⚠️ **„Autostart plugins at boot“ anhaken** — sonst startet das Audio-Plugin nicht.
5. **Neu starten.** Ohne Neustart ist das Plugin geladen, aber nicht gestartet (`PTT: no PICOTTS plugin and no
   built-in engine` im Log). Dasselbe nach jedem Austausch eines Plugins.
6. **Stimmdateien** ins Hauptverzeichnis des Dateisystems (die SD-Karte, wenn eine steckt, sonst der Flash):

   | Im Repository | Name auf dem Gerät |
   |---|---|
   | `de-DE_ta.bin` | `/picotts_de-DE_ta.bin` |
   | `de-DE_gl0_sg.bin` | `/picotts_de-DE_sg.bin` |
   | `en-US_ta.bin` | `/picotts_en-US_ta.bin` |
   | `en-US_lh0_sg.bin` | `/picotts_en-US_sg.bin` |
   | `en-GB_ta.bin`, `en-GB_kh0_sg.bin` | `/picotts_en-GB_ta.bin`, `/picotts_en-GB_sg.bin` |
   | `fr-FR_ta.bin`, `fr-FR_nk0_sg.bin` | `/picotts_fr-FR_ta.bin`, `/picotts_fr-FR_sg.bin` |
   | `it-IT_ta.bin`, `it-IT_cm0_sg.bin` | `/picotts_it-IT_ta.bin`, `/picotts_it-IT_sg.bin` |
   | `es-ES_ta.bin`, `es-ES_zl0_sg.bin` | `/picotts_es-ES_ta.bin`, `/picotts_es-ES_sg.bin` |

   Dateien über 256 KB laufen in *Dateisystem verwalten* automatisch über Port 83.

## Benutzen

```
I2STTS Guten Tag, hier spricht die Kamera
I2STTSLang              die geladene Sprache (Standard de-DE)
I2STTSLang en-US        umschalten; die Stimme wird beim nächsten I2STTS neu geladen
```

Beim ersten `I2STTS` steht im Log `PTT: loaded /picotts_de-DE_ta.bin … -> PSRAM`, `PTT: plugin self check ok` und
`PTT: ready`. Nach jedem Satz eine Zeile `PTT: utterance N samples, peak …, rms …; engine … ms, output callback
… ms` — praktisch, um zu sehen, wie lange die Engine wirklich rechnet (auf einem S3 etwa 0,6 s je Sekunde
Sprache).

Speicher: rund 2,5 MB PSRAM, solange eine Stimme geladen ist (Arena 1,1 MB + Stimme 1–1,4 MB); Hin- und
Herschalten verliert nichts (gemessen: 3200 KB frei mit Deutsch, 2848 KB mit Englisch, über vier Runden
konstant).

## Wenn es nicht klappt

| Anzeichen | Ursache |
|---|---|
| `Unknown` bei `I2STTS` | das Audio-Plugin läuft nicht: Autostart, Pins, Neustart |
| `PTT: no PICOTTS plugin and no built-in engine` | `PICOTTS_32.bin` fehlt, oder nach dem Hochladen nicht neu gestartet |
| `PTT: voice load failed` | eine Stimmdatei fehlt oder liegt im falschen Dateisystem (SD steckt: das Plugin sucht dort) |
| `PTT: plugin self check FAILED (mask …)` | Plugin und Firmware passen nicht zusammen (Sprungtabelle); das Plugin desselben Releases nehmen |
| Beim Hochladen `MEM` / `flash slot memory error` | die Partition ist voll oder zerstückelt; Firmware vor 1.6.70 konnte ein Plugin nicht durch ein gleich großes ersetzen — zuerst ein anderes Plugin entfernen, oder `chkpt a8` |

## Für Entwickler

Das Plugin wird aus dem unveränderten `lib/libesp32_div/pico` erzeugt; Werkzeuge, Host-Tests und das eine
Compiler-Problem, das umgangen werden musste (`acphAccentuation` wird mit `-O0` gebaut), stehen in
`tasmota/Plugins/picotts/test/README.md`.
