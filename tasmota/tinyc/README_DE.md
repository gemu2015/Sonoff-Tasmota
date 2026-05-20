# TinyC — C-Scripting fuer Tasmota

TinyC ist ein C-Subset-Compiler und eine VM, die auf ESP32/ESP8266 als Tasmota-Treiber `XDRV_124` laeuft. C-Code im Browser-IDE schreiben, zu Bytecode kompilieren, hochladen und ausfuehren — kein Firmware-Rebuild noetig.

> **Aktuelle Firmware: v1.6.14** — vorgebaute `.bin` / `.factory.bin` fuer ESP32 / ESP32-S3 / ESP32-C3 / ESP8266 sowie das passende `tinyc_ide.html.gz` haengen am [`testing`-Release auf GitHub](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing).

## Was ist neu in v1.6.14

- **DMX512-TX via RMT** — `dmxInit(gpio)` (ein GPIO, kein UART belegt). Der Frame wird als `rmt_symbol_word_t[]` aufgebaut und ueber `rmt_new_tx_channel` + Copy-Encoder mit 1 MHz hardware-getaktet ausgegeben (BREAK/MAB/250 kBaud, kein Busy-Wait). Alle UARTs bleiben frei fuer SML/Modbus/`tc_serial`. 30-Sekunden-Watchdog zwingt auf Null, damit ein Heizstab-Dimmer sicher abschalten kann. Siehe `examples/dmx_dimmer_panel.tc`.
- **Boot-Loop-False-Positive-Fix (1.6.11)** — schnelle `Restart 1`-Folgen markieren die vier Slots nicht mehr faelschlich als `autoexec=0`. Echte PANIC/WDT/BROWNOUT-Loops loesen die Schutzlogik weiterhin aus.
- **Persist-`.pvs.bak`-Sicherheitsnetz (1.6.10)** — jeder erfolgreiche Persist-Schreibvorgang behaelt den vorherigen Stand als `slot-N_<name>.pvs.bak`; ein einfaches `cp` stellt einen layout-hash-geloeschten Zustand ohne JSON-Restore wieder her.
- **WebOn/WebUI-Halted-Wait-Fix (1.6.10)** — `webOn()`-Callbacks liefern keinen spuriosen `503` mehr, wenn die VM waehrend des Page-Pollings auf dem anderen Core kurz angehalten ist.
- **IDE injiziert `TC_RELEASE` + `TC_MAX_HEAP` automatisch** — `bundle.py` liest beide beim Bundeln aus `xdrv_124_tinyc_vm.h`, sodass Banner-Version und Heap-Warnschwelle immer zur Firmware passen. Das Heap-Budget ist jetzt eine *Warnung*, kein harter Block — das Geraet entscheidet.
- **Neues Beispiel: `persist_array_file`** — Script-verwaltete Binaer-Persistenz via `fileReadArray` / `fileWriteArray` fuer Puffer, die groesser sind als das `persist`-Slot-Budget.
- **`serial_monitor`-Utility — grosse Ueberarbeitung** — Windows- und Linux-Unterstuetzung, OTA-Flasher (LAN-Scan -> `OtaUrl` + `Upgrade`), serieller esptool-Flasher mit `--no-stub`-Fallback, ESP32-Partitionstabelle in der Geraetekarte, isolierte esptool-Installation. Liegt unter `tasmota/tinyc/utils/serial_monitor/`.

## Tasmota mit TinyC bauen

In `user_config_override.h` hinzufuegen:

```c
#define USE_TINYC           // TinyC VM aktivieren (XDRV_124)
#define USE_TINYC_IDE       // Browser-IDE aktivieren (benoetigt USE_UFILESYS)
```

## Schnellstart

1. `tinyc_ide.html.gz` auf das Flash-Dateisystem des Geraets hochladen
2. `http://<geraete-ip>/tinyc_ide.html` im Browser oeffnen
3. Code schreiben, **Ctrl+Enter** zum Kompilieren, **Ctrl+Shift+Enter** zum Ausfuehren

## Sprache

Standard-C-Subset: `int`, `float`, `char`, `void`, `bool` Datentypen. Kontrollfluss mit `if/else`, `while`, `for`, `switch/case`, `break`, `continue`. Funktionen, 1D- und 2D-Arrays (`char buf[N][M]`, `int grid[R][C]`, `float coef[R][C]` — automatische Heap-Promotion ueber 16 Elementen), Structs, `#define`-Praeprozessor, `// Zeile` und `/* Block */` Kommentare. Keine Pointer.

## Tasmota-Integration

Callbacks werden automatisch aus Tasmotas Hauptschleife aufgerufen:

| Callback | Wann | Verwendung |
|---|---|---|
| `EveryLoop()` | Jede Hauptschleife (~1-5ms) | Ultraschnelles Polling, Bit-Banging |
| `Every50ms()` | Alle 50ms | Schnelle I/O, Funk-Polling |
| `EverySecond()` | Jede Sekunde | Sensor-Abfrage, Status-Updates |
| `JsonCall()` | MQTT-Telemetrie | JSON anfuegen via `responseAppend()` |
| `WebCall()` | Web-Seiten-Aktualisierung | Sensor-Zeilen via `webSend()` |
| `WebPage()` | Seitenaufruf (einmalig) | Charts, eigenes HTML |
| `UdpCall()` | UDP-Paket empfangen | Geraetekommunikation |
| `TaskLoop()` | FreeRTOS-Task (ESP32) | Hintergrundschleife mit `delay()` |

`main()` wird zuerst ausgefuehrt (in einem FreeRTOS-Task auf ESP32, mit voller `delay()`-Unterstuetzung). Nach Rueckkehr von `main()` bleiben Globale erhalten und Callbacks werden aktiviert. Wenn `TaskLoop()` definiert ist, laeuft es unabhaengig vom Haupt-Thread weiter.

## Eingebaute Funktionen

**GPIO:** `pinMode`, `digitalWrite`, `digitalRead`, `analogRead`, `analogWrite`, `gpioInit`
**Timing:** `delay`, `delayMicroseconds`, `millis`, `micros`
**Timer:** `timerStart`, `timerDone`, `timerStop`, `timerRemaining`
**Seriell:** `serialBegin`, `serialPrint`, `serialPrintInt`, `serialPrintFloat`, `serialPrintln`, `serialRead`, `serialAvailable`
**Mathe:** `abs`, `min`, `max`, `map`, `random`, `sqrt`, `sin`, `cos`
**Strings:** `strlen`, `strcpy`, `strcat`, `strcmp`, `printString`, `printStr`, `strToken`, `strSub`, `strFind`
**Format:** `sprintf`, `sprintfAppend` (Typ automatisch erkannt; alt: `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr`)
**I2C:** `i2cRead8`, `i2cWrite8`, `i2cRead`, `i2cWrite`, `i2cExists`, `i2cRead0`, `i2cWrite0`
**SPI:** `spiInit`, `spiSetCS`, `spiTransfer`
**Dateien:** `fileOpen`, `fileClose`, `fileRead`, `fileWrite`, `fileExists`, `fileDelete`, `fileSize`
**Tasmota:** `tasmCmd`, `sensorGet`, `responseAppend`, `webSend`, `webFlush`, `addLog`, `addLogLevel`, `addCommand`, `responseCmnd`
**HTTP:** `httpGet`, `httpPost`, `httpHeader`
**UDP:** `udpRecv`, `udpReady`, `udpSendArray`, `udpRecvArray`, `udp` (allgemein, Modi 0-7) — skalare `global` Floats senden automatisch bei Zuweisung
**Display:** `dspText`, `dspClear`, `dspPos`, `dspFont`, `dspSize`, `dspColor`, `dspDraw`, `dspPad`, `dspPixel`, `dspLine`, `dspRect`, `dspFillRect`, `dspCircle`, `dspFillCircle`, `dspHLine`, `dspVLine`, `dspRoundRect`, `dspFillRoundRect`, `dspTriangle`, `dspFillTriangle`, `dspDim`, `dspOnOff`, `dspUpdate`, `dspPicture`, `dspWidth`, `dspHeight`, `dspTextWidth`, `dspTextHeight`
**Bildspeicher:** `dspLoadImage`, `dspPushImageRect`, `dspImageWidth`, `dspImageHeight`, `dspImgText`, `dspLoadImageFromCam`, `dspImgTextBurn`, `dspImageToCam` — PSRAM-Bildslots fuer flimmerfreies Compositing + Cam ↔ Bild Bruecke zum Einbrennen von Zeitstempeln/Labels in JPEG-Aufnahmen
**Touch-Buttons:** `dspButton`, `dspTButton`, `dspPButton`, `dspSlider`, `dspButtonState`, `touchButton`
**Audio:** `audioVol`, `audioPlay`, `audioSay`
**WebUI:** `webButton`, `webToggle`, `webSlider`, `webCheckbox`, `webText`, `webNumber`, `webPulldown`, `webRadio`, `webTime`, `webPageLabel`, `webPage`, `webSendFile`, `webOn`, `webHandler`, `webArg`
**SML:** `smlGet`, `smlGetStr`, `smlWrite`, `smlRead`, `smlSetBaud`, `smlSetWStr`, `smlSetOpt`, `smlGetV`
**mDNS:** `mdnsRegister`
**System:** `tasm_wifi`, `tasm_mqttcon`, `tasm_teleperiod`, `tasm_uptime`, `tasm_heap`, `tasm_pheap`, `tasm_maxblock`, `tasm_frag`, `tasm_power`, `tasm_dimmer`, `tasm_temp`, `tasm_hum`, `tasm_hour`, `tasm_minute`, `tasm_second`, `tasm_year`, `tasm_month`, `tasm_day`, `tasm_wday`, `tasm_cw`, `tasm_sunrise`, `tasm_sunset`, `tasm_time`
**HomeKit:** `hkSetCode`, `hkAdd`, `hkVar`, `hkReady`, `hkStart`, `hkReset`, `hkStop` + Callback `HomeKitWrite(dev, var, val)`
**LED-Streifen:** `setPixels(array, len, offset)` — WS2812/NeoPixel-Steuerung
**DMX512 (ESP32 via RMT):** `dmxInit(gpio)`, `dmxWrite(channel, value)` — TX-only, hardware-getakteter BREAK/MAB/250 kBaud 8N2, 16 Slots (`TC_DMX_SLOTS`), 30 s All-Zero-Watchdog. Kein UART belegt
**Debug:** `print`, `dumpVM`

## Vordefinierte Konstanten

**Farben (RGB565):** 16 Farben ohne `#define` verfuegbar: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `YELLOW`, `CYAN`, `MAGENTA`, `ORANGE`, `PURPLE`, `GREY`, `DARKGREY`, `LIGHTGREY`, `DARKGREEN`, `NAVY`, `MAROON`, `OLIVE`

**HomeKit-Typen:** `HK_TEMPERATURE`, `HK_HUMIDITY`, `HK_LIGHT_SENSOR`, `HK_BATTERY`, `HK_CONTACT`, `HK_SWITCH`, `HK_OUTLET`, `HK_LIGHT`

**Datei-Modi:** `r` (Lesen), `w` (Schreiben), `a` (Anhaengen) — fuer `fileOpen()`

**1-Wire:** `owSetPin`, `owReset`, `owWrite`, `owRead`, `owWriteBit`, `owReadBit`, `owSearchReset`, `owSearch`
**TCP:** `tcpServer`, `tcpClose`, `tcpAvailable`, `tcpRead`, `tcpWrite`, `tcpReadArray`, `tcpWriteArray`, `tcpConnect`, `tcpDisconnect`, `tcpConnected`, `tcpSelect` (4 parallele Client-Slots)
**MQTT:** `mqttSubscribe`, `mqttUnsubscribe`, `mqttPublish` + `OnMqttData(topic, payload)` Callback (10 Abos, `#` Praefix-Wildcard)
**Dynamische Tasks (ESP32):** `spawnTask`, `killTask`, `taskRunning` — bis zu 4 parallele benannte FreeRTOS-Tasks, die den VM-Zustand des Aufrufers teilen (einmalige verzoegerte Jobs, parallele Downloader, toetbare Worker)
**Cross-VM Share (ESP32):** `shareSetInt`/`shareGetInt`, `shareSetFloat`/`shareGetFloat`, `shareSetStr`/`shareGetStr`, `shareHas`, `shareDelete` — 32-Eintraege-Tabelle (treiber-global, Mutex-geschuetzt) zum Austausch von Skalaren/Strings zwischen TinyC-Slots, wenn ein Programm auf zwei Slots aufgeteilt werden muss. Lesezugriff auf fehlende Schluessel liefert `0`/`0.0`/`""` ohne Fehler
**Tiefschlaf:** `deepSleep`, `deepSleepGpio`, `wakeupCause`
**E-Mail:** `mailBody`, `mailAttach`, `mailSend`
**Persist:** `persist` Schluesselwort fuer automatisch gespeicherte Variablen, `saveVars` fuer manuelles Speichern
**Watch:** `watch` Schluesselwort fuer Aenderungserkennung, `changed`, `delta`, `written`, `snapshot`
**HomeKit:** `hkSetCode`, `hkAdd`, `hkVar`, `hkReady`, `hkStart`, `hkReset`, `hkStop` + Callback `HomeKitWrite(dev, var, val)`
**LED-Streifen:** `setPixels(array, len, offset)` — WS2812/NeoPixel-Steuerung
**DMX512 (ESP32 via RMT):** `dmxInit(gpio)`, `dmxWrite(channel, value)` — TX-only, hardware-getakteter BREAK/MAB/250 kBaud 8N2, 16 Slots (`TC_DMX_SLOTS`), 30 s All-Zero-Watchdog. Kein UART belegt
**Debug:** `print`, `dumpVM`

## Tasmota-Befehle

| Befehl | Beschreibung |
|---|---|
| `TinyC` | VM-Status anzeigen (alle Slots) |
| `TinyCRun [s] [/f]` | Slot s starten (Standard 0), optional /f laden |
| `TinyCStop [s]` | Slot s stoppen (Standard 0) |
| `TinyCReset [s]` | Slot s zuruecksetzen (Standard 0) |
| `TinyCExec <n>` | Instruktionen pro Tick setzen (Standard 1000) |
| `TinyCInfo 0\|1` | VM-Statuszeilen auf Hauptseite ein-/ausblenden |
| `TinyCChkpt` | Partitionstabelle anzeigen (nur ESP32) |
| `TinyCChkpt p` | App-Partition automatisch verkleinern, Dateisystem vergroessern. **Achtung: Dateisystem wird formatiert!** |
| `TinyCChkpt p <KB>` | App-Partition auf bestimmte Groesse in KB setzen |

REST-API: `http://<ip>/tc_api?cmd=run`, `cmd=stop`, `cmd=status` (mit `slot=` Parameter)

## VM-Limits

| Ressource | ESP8266 | ESP32 |
|---|---|---|
| Stack-Tiefe | 64 | 256 |
| Aufruf-Frames | 8 | 32 |
| Globale | 64 | dynamisch |
| Konstanten | 32 | 1024 |
| Konst.-Daten | 512 B | dynamisch (DRAM, faellt auf PSRAM zurueck) |
| Code-Groesse | 4 KB | 128 KB (DRAM, faellt auf PSRAM zurueck) |
| Heap | 8 KB | 32 KB |
| Heap-Handles | 8 | 32 |

> **ESP8266-Einschraenkung:** Der ESP8266 hat sehr wenig RAM (~40 KB freier Heap). TinyC funktioniert fuer einfache Skripte (Sensoren lesen, MQTT, einfache Automatisierung), aber Programme mit Heap-Arrays, WS2812-LED-Streifen oder IR zusammen mit der Tasmota-Web-Oberflaeche fuehren zu Instabilitaet wegen Speicherknappheit. Fuer alles ueber triviale Skripte hinaus ESP32, ESP32-S3 oder ESP32-C3 verwenden.

## Beispiele

Siehe [`examples/`](examples/) fuer 60+ vollstaendige Programme. Highlights:

**I2C-Sensoren:**
- **bme280** / **bmp280** / **bmx280** — Bosch Umweltsensoren (Temperatur, Feuchte, Druck)
- **sht31** — Temperatur/Feuchte (Dual-Bus-Scan, Adress-Claiming)
- **scd30** — CO2-Sensor mit Auto-Kalibrierung
- **sgp30** — VOC/eCO2 Luftqualitaet
- **sps30** — Feinstaubsensor
- **vl53l0x** — Laser Time-of-Flight Abstandsmessung (VLMode/VLBudget/VLStatus Befehle)
- **mlx90614** — Infrarot-Thermometer (beruehrungslos)
- **tcs34725** — RGB-Farbsensor
- **veml6075** — UV-Index-Sensor
- **ltr308** — Umgebungslichtsensor
- **ads1115** — 16-Bit ADC
- **lcd_i2c** — HD44780 LCD mit Konsolenbefehlen

**Display:**
- **display_demo** — Sensor-Dashboard fuer ILI9488
- **sunton_display** / **guiton_display** — Sunton/Guition Board-Demos
- **epaper29** — E-Paper 2.9" Treiber
- **text_on_image** — Flimmerfreies Text-Compositing auf JPEG-Hintergruenden
- **analog_clock** — Analoguhr mit Datumsanzeige
- **lcd_chart** / **live_chart** / **chart_types** — Verschiedene Chart-Stile
- **touch_buttons** — GFX Touch-Button/Slider Demo

**Protokolle & Hardware:**
- **bresser** / **bresser_chart** — CC1101 868 MHz Wetterstation-Empfaenger
- **onewire** — 1-Wire Bus: DS18B20 + DS2406/DS2413/DS2408, GPIO und DS2480B Modi
- **max31855** — SPI Thermoelement-Leser
- **dysv17f** — DY-SV17F MP3-Player (seriell TX, Konsolenbefehle)
- **camera** / **webcam** — ESP32-Kamera-Treiber

**Energie & Automatisierung:**
- **core2_energy** — M5Stack Core2 Energiemonitor mit Shelly 3EM
- **ecotracker** — Energieerfassung mit Tageszaehlern
- **powerwall** — Tesla Powerwall API

**Web & Kommunikation:**
- **web_buttons** / **web_handler** / **webui_demo** — Eigene Web-Oberflaechen
- **homekit_demo** / **homekit_office** — Apple HomeKit Integration
- **udp** — Multicast-Datenaustausch zwischen Geraeten

**Grundlagen:**
- **blink**, **callbacks**, **benchmark**, **fibonacci**, **sort**, **strings**, **file_io**, **sensor_read**, **editor**, **watch_demo**

## VS Code Unterstuetzung

Die `vscode-tinyc` Erweiterung fuer `.tc` Syntax-Highlighting installieren mit voller TinyC-Schluesselwort-, Builtin- und Callback-Faerbung. Nach `~/.vscode/extensions/tinyc-1.0.0` kopieren oder verlinken.

## Referenz

Vollstaendige Sprachspezifikation: [TinyC_Reference.md](TinyC_Reference.md) | [Deutsche Version](TinyC_Reference_DE.md)
