# DFRobot AI CAM (DFR1154) selbst bauen — mit allen Optionen

*Für Hans, 02.10.2026. Ersetzt die Schritte 1–3 der älteren Notiz
[`docs/ESP-DL_Bauanleitung.md`](../docs/ESP-DL_Bauanleitung.md); Modell und Benutzung stehen
dort weiter.*

Hallo Hans,

diese Anleitung bringt dich von einem frischen `git clone` zu einer Firmware für deine
**DFRobot FireBeetle 2 ESP32-S3 AI CAM (DFR1154, OV3660)** mit:

| Option | Was du davon hast | Kostet |
|---|---|---|
| **TinyC** + Kamera-Treiber | Webcam-Skript `webcam_tinyc.tc`, Bewegungserkennung, Nachtsicht (IR-LEDs), Mail | – |
| **Personenerkennung** (ESP-DL) | das Netz bestätigt „Mensch“, bevor der Alarm losgeht | +~800 kB Flash, 76 kB RAM zur Laufzeit |
| **Audio-Plugin** (`I2SAUDIO`) | Audio-Ein-/Ausgabe: Mikrofon, Lautsprecher, Anbindung an die CamViewer-App | ~100 kB Plugin-Partition |
| **Sprachausgabe** (PicoTTS, `I2STTS`) | Text sprechen, Deutsch und fünf weitere Sprachen; braucht das Audio-Plugin | +~125 kB Flash, ~1 MB PSRAM beim Sprechen; Stimmdateien (je ~1 MB) auf der SD-Karte |
| **Matter** (nur wenn du magst) | Kamera als Matter-Gerät | eingebaut +33 kB RAM, als Plugin 0 |

**Das Wichtigste vorweg — zwei Dateien kommen nicht mit dem `git pull`:**
`platformio_override.ini` und `tasmota/user_config_override.h` stehen beide in der
`.gitignore`. Die TinyC-Schalter (`USE_TINYC` & Co.) stehen ausschliesslich in der zweiten;
ohne sie baut PlatformIO eine Tasmota-Firmware **ohne TinyC**, und du merkst es erst, wenn
die IDE fehlt. Beide Dateien liegen fertig im Ordner `tasmota/tinyc/dfrobot/`.

---

## 1. Voraussetzungen

* **PlatformIO** (Kommandozeile oder VS-Code-Erweiterung), Python 3, `git`.
* **Node.js** (ab 18) für `tc_deploy.mjs`, das TinyC-Skripte übersetzt und hochlädt.
* Rund 1,5 GB Platz für den Baum und die Werkzeuge, beim ersten Bau lädt PlatformIO die
  Plattform nach.
* Hardware: ESP32-S3 mit **16 MB Flash und 8 MB PSRAM** — das hat die DFR1154.

Zwei Stolpersteine, die jeder erste Bau hat:

* `ModuleNotFoundError: No module named 'yaml'` im ersten Lauf: einfach **noch einmal**
  starten. PlatformIO richtet die Plattform erst während des ersten Laufs fertig ein.
* `No module named SCons.Tool.FortranCommon`: PlatformIO-Core 6.2.0 verträgt sich nicht mit
  der Tasmota-Plattform. Abhilfe: `pip install` der Fassung **6.1.19**
  (`https://github.com/platformio/platformio-core/archive/refs/tags/v6.1.19.zip`).

## 2. Quellen holen

```bash
git clone https://github.com/gemu2015/Sonoff-Tasmota.git
cd Sonoff-Tasmota          # Zweig universal
```

Ob alles da ist, siehst du an `lib/libesp32_dl/` (ESP-DL) und `tasmota/tinyc/dfrobot/`
(diese Dateien). Später reicht `git pull`.

## 3. Die zwei Dateien an ihren Platz

```bash
cp tasmota/tinyc/dfrobot/platformio_override_dfrobot.ini  platformio_override.ini
cp tasmota/tinyc/dfrobot/user_config_override_dfrobot.h   tasmota/user_config_override.h
```

⚠️ Hast du schon eine eigene `platformio_override.ini` oder `user_config_override.h`:
**sichern**, bei der `.ini` die Abschnitte nur **anhängen**. Die alte Umgebung `dfrobot-cam`
aus der Notiz vom 16.09. brauchst du nicht mehr — sie baute mit Tasmotas voller
Standardausstattung statt mit dem TinyC-Block.

In der Kopfdatei steht **nichts Persönliches**: kein WLAN, kein MQTT. Nach dem Flashen
meldet sich das Gerät als Access Point, dort richtest du dein WLAN ein.

Was die Umgebung `dfrobot-ai-cam` einschaltet (alles steht in der `.ini`, kommentiert):

* `-DTINYC_TESTING` — aktiviert den TinyC-Block in der Kopfdatei.
* `-DTINYC_CAMERA` — Kamera-Treiber (esp32-camera).
* `-DUSE_TINYC_ESPDL` samt den Include-Pfaden, `-DCONFIG_PIX_CVT_…`, `-lfbs_model` und
  `lib_deps = symlink://lib/libesp32_dl/esp-dl` — **die Personenerkennung**. Diesen Block
  nicht kürzen: ohne `lib_deps` wird die Bibliothek wortlos nicht gebaut (Fehler erst beim
  Binden: `undefined reference to dl::Model::…`), ohne `vision/image/isa` fehlt eine Kopfdatei,
  ohne die `CONFIG_PIX_CVT_…` fehlen Farbumwandlungen.
* `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DUSE_USB_CDC_CONSOLE` — die DFR1154 hat
  **nur nativen USB**, keinen UART-Chip. Ohne das hast du keine serielle Konsole.
* `-DTINYC_NO_LVGL` — die Kamera hat kein Display, spart Flash.

⚠️ **Zwischen `build_flags` und seiner Fortsetzungszeile darf kein Kommentar stehen.** Das
bricht in INI-Dateien die Fortsetzung, und die Schalter fallen **still** weg.

## 4. Bauen

```bash
pio run -e dfrobot-ai-cam
```

Der erste Lauf dauert 3–5 Minuten (Plattform laden), spätere um 2 Minuten. Es kommen
rund **2,6 MB** heraus:

```
.pio/build/dfrobot-ai-cam/firmware.bin            ← zum Aktualisieren (OTA)
.pio/build/dfrobot-ai-cam/firmware.factory.bin    ← fürs erste Aufspielen per USB
```

Varianten mit Matter: `dfrobot-ai-cam-matter` (Engine eingebaut, **+33 kB RAM, immer** —
auch wenn Matter nie startet) oder `dfrobot-ai-cam-mtrplugin` (Firmware ohne die Engine,
Matter kommt als Plugin, siehe [`docs/MATTER_PLUGIN_DE.md`](../docs/MATTER_PLUGIN_DE.md)).
Für die Kamera würde ich Matter weglassen: das RAM braucht die Personenerkennung.

## 5. Aufspielen

**Erstes Mal, per USB.** Die DFR1154 meldet sich als USB-Gerät. Reagiert der Port nicht:
**BOOT** gedrückt halten, **RESET** drücken, beide loslassen (Download-Modus). Dann:

```bash
pio run -e dfrobot-ai-cam -t upload
# oder, mit der .factory.bin (Bootloader + Partitionen + Safeboot + Firmware in einem):
esptool.py --chip esp32s3 -p <port> write_flash 0x0 .pio/build/dfrobot-ai-cam/firmware.factory.bin
```

Dann WLAN einrichten (Access Point des Geräts) und die **Vorlage** setzen (Abschnitt 6).

**Danach per WLAN (OTA):**

```bash
python3 tasmota/tinyc/utils/ota_flash.py <ip> --env dfrobot-ai-cam
```

⚠️ **Nicht** über den Upload der Weboberfläche (`/u2`): der scheitert bei dieser
Partitionsaufteilung immer mit „Nicht genug Speicherplatz“. Das Skript lässt das Gerät die
Datei selbst abholen.

⚠️ Bleibt das Gerät danach im **Safeboot** hängen (Seite zeigt „13.4.0(safeboot)“, auch nach
einem Neustart): das Skript greift dann nicht mehr. Von Hand, aus dem Ordner mit
`firmware.bin`:

```bash
python3 -m http.server 8765 &
curl "http://<ip>/cm?cmnd=OtaUrl%20http://<deine-rechner-ip>:8765/firmware.bin"
curl "http://<ip>/cm?cmnd=Upgrade%201"
```

⚠️ **Nach dem ersten Flash eines selbst gebauten Images können die Einstellungen weg sein.** Bei uns kam
die Kamera nach dem OTA-Flash mit Standardwerten zurück: das WLAN war vergessen (sie meldete sich als
Access Point), und die Vorlage mit den Kamera-Pins, der Gerätename und der Hostname waren zurückgesetzt.
Die Ursache haben wir nicht geklärt. Rechne also damit, das WLAN neu einzutragen und die Vorlage aus
Abschnitt 6 neu zu setzen. Dateien auf der SD-Karte und das Plugin samt Autostart blieben erhalten.
Das Image enthält absichtlich keine WLAN-Zugangsdaten.

## 6. Vorlage (Pins der Kamera)

Das sind die Pins unserer DFR1154, so wie sie auf unserer Kamera laufen. In der Konsole:

```
Template {"NAME":"DFRWEBCAM","ARCH":"ESP32S3","GPIO":[1,0,0,0,0,0,0,0,641,609,6720,704,736,672,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,224,1],"FLAG":0,"BASE":1}
Module 0
```

Das Gerät startet neu. `224` ist der Relais-Ausgang an **GPIO47: das sind die IR-LEDs** der
Nachtsicht.

## 7. TinyC-Skript und IDE

Die IDE liegt als Datei im Gerät und ist **nicht** Teil der Firmware. Einmal in der Konsole
(das Gerät braucht dafür Internet):

```
TinyCIde
```

Dann das Kameraskript, `webcam_tinyc.tc`, in **Slot 0**:

```bash
node tasmota/tinyc/tc_deploy.mjs tasmota/tinyc/examples/webcam_tinyc.tc <ip> --slot 0
```

* `--slot 0` **nicht weglassen**: ohne Angabe geht alles nach Slot 0.
* Das Skript trägt `// @defines: -DBOARD_DFROBOT` — die Nachtsicht und die Personenerkennung
  hängen daran. Die Vorgabe greift von selbst.
* Kommt `node` bei dir nicht ins LAN (`EHOSTUNREACH`, auf dem Mac die fehlende Erlaubnis
  „Lokales Netzwerk“): mit `--no-upload` nur übersetzen und mit `curl` hochladen — siehe
  [`docs/ESP-DL_Bauanleitung.md`](../docs/ESP-DL_Bauanleitung.md), Schritt 5.
* ⚠️ **Der Übersetzer muss zur Firmware passen.** `tc_deploy.mjs` nimmt die IDE aus dem
  Quellbaum. Ist die Firmware auf dem Gerät älter, erzeugt es Opcodes, die sie nicht kennt
  (`"Error":"Unknown opcode"`), und der Bau war dabei grün. Wenn du **selbst** baust und
  flashst, passt es.

Nach jedem Aufspielen einmal `TinyC` in die Konsole: im Slot-JSON muss `"Error":"OK"` stehen.

## 8. Personenerkennung

Das Modell liegt **nicht** in der Firmware, es wird zur Laufzeit als Datei geladen:

```bash
git clone --depth 1 https://github.com/espressif/esp-dl.git
# 425 kB, unbedingt die s3-Fassung:
#   esp-dl/models/pedestrian_detect/models/s3/pedestrian_detect_pico_s8_v1.espdl
```

Hochladen als `ped.espdl` (Dateiverwaltung der Weboberfläche). ⚠️ **Steckt eine SD-Karte
im Gerät, liegt die Datei auf der Karte, und der Pfad für den Treiber ist `/sd/ped.espdl`** —
die Weboberfläche zeigt dir trotzdem `/ped.espdl`. Der Treiber ist auf `/sd/ped.espdl`
vorbelegt. Ohne Karte musst du `TC_DL_PERSON_MODEL` in `xdrv_124_tinyc.ino` ändern. Prüfen
mit `UfsType` (`[1,3]` = Karte und Flash, `[3]` = nur Flash).

Erster Test: `TinyCDl /sd/ped.espdl`. `"Test":"FAIL"` ist dabei normal. Dann auf der
Hauptseite → **Camera Setup** → *Confirm with the neural net (person detection)* anhaken
(Schwelle 50 %). Gemessen: 216 ms für das Netz, ~470 ms je Bild mit JPEG-Entpacken. Mehr
steht in der älteren Notiz.

## 9. Audio-Plugin — nötig für Audio-Ein-/Ausgabe

In dieser Firmware ist das eingebaute I2S-Audio **abgeschaltet** (die Plugins ersetzen es).
**Ohne das Plugin passiert bei Audio gar nichts**, und es gibt keine Fehlermeldung. Das
Plugin musst du **nicht selbst bauen**, es liegt fertig am Test-Release:

> **https://github.com/gemu2015/Sonoff-Tasmota/releases/download/testing/I2SAUDIO_32.bin**
> (die Release-Seite: https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing)

`_32` ist die Fassung für Xtensa-Chips, also auch für deinen S3.

1. **Plugin-Partition anlegen**, einmalig, in der Konsole: `chkpt a`
   ⚠️ **Das formatiert das Dateisystem.** Skripte, die IDE und die Dateien auf dem Flash
   sind danach weg (eine SD-Karte nicht). Vorher sichern. Das Gerät startet von selbst neu;
   danach `TinyCIde` und das Skript wieder hochladen.
2. **Plugin hochladen:** Weboberfläche → *Werkzeuge* → **Plugins directory** →
   `I2SAUDIO_32.bin` wählen → Start. Der Eintrag erscheint in der Tabelle.
3. ⚠️ **Autostart einschalten:** oben auf der Plugin-Seite **„Autostart plugins at boot“**
   anhaken. **Ohne den Haken wird das Plugin beim Start nicht gestartet** — Matter holt sich
   sein Plugin trotzdem selbst, das Audio-Plugin nicht. (Das war bei uns die Ursache,
   warum auf der Kamera kein Audio ging.) Der Haken wird mit der Firmware vom 29.09.2026 oder
   neuer in `/plugins.auto` gespeichert; auf älterer Firmware geht er beim Neustart verloren.
   Wer selbst baut (diese Anleitung), hat die neue Fassung.
4. **Neu starten.** Kontrolle in der Konsole mit `mdir`: der Eintrag `I2SAUDIO` steht da,
   mit einer Zahl in der RAM-Spalte (das Plugin läuft, sonst steht dort 0).

### Die Pins: `I2SAUDIO.cfg` einfach übernehmen

Hans hat dieselbe Platine, also dieselben Pins. Sie stehen fertig in
[`I2SAUDIO.cfg`](I2SAUDIO.cfg) (liegt neben dieser Anleitung) — es ist die Datei, die das
Audio-Plugin auf unserer Kamera selbst geschrieben hat (am 02.10.2026 von dort geholt):

| Feld | Pin | Bedeutung |
|---|---|---|
| `DOUT` | 42 | Ausgabe zum Verstärker |
| `DIN/PDD` | 39 | Eingabe vom (PDM-)Mikrofon |
| `PDC` | 38 | Takt des PDM-Mikrofons |
| `BCK` | 45 | Bit-Takt |
| `WS` | 46 | Wort-Takt |
| `MC` | 49 = „-1“ | kein Master-Takt |
| `APWR` | 49 = „-1“ | kein Schaltpin für den Verstärker |
| `MODE`, `CODEC` | 16777728 | Auswahlfelder (Wert 0 in einer Auswahl 0–2); die große Zahl enthält die Auswahlgrenzen — **nicht ändern** |

(„49“ ist, wie das Plugin „kein Pin“ speichert; in der Auswahlliste steht dafür „-1“.)

So kommt die Datei auf die Kamera:

1. **Werkzeuge → Dateisystem verwalten**, dort auf das **Flash-Dateisystem** umschalten
   (nicht die SD-Karte; hat das Gerät keine SD-Karte, gibt es nur dieses eine).
2. `I2SAUDIO.cfg` hochladen. Sie muss direkt im Hauptverzeichnis liegen: **`/I2SAUDIO.cfg`**.
3. Neu starten. Das Plugin liest die Datei beim Start und übernimmt die Pins; weichen die Pins vom Standard
   des Plugins ab, steht im Log `Plugin: pin config applied /I2SAUDIO.cfg`.

⚠️ `chkpt a` (Schritt 1 oben) formatiert das Flash-Dateisystem — die Datei also **erst danach**
hochladen, sonst ist sie wieder weg. Wer die Pins lieber von Hand setzt: auf der Plugin-Seite
stehen sie als Auswahlfelder an der Zeile `I2SAUDIO`; jede Änderung dort schreibt die Datei von
selbst neu.

Ein TinyC-Skript, das den Kanal selbst öffnet, setzt seine Pins im Skript (`i2sBegin`,
`i2sMicBegin`); die Datei betrifft nur das Plugin. Das Plugin selbst ist auch an anderen
S3-Geräten (WM8960) erprobt, siehe `examples/audio_io.tc`.

### Sprachausgabe (TTS)

Das Plugin spricht Text mit **SVOX PicoTTS**. Die Engine (rund 125 KB) steckt **nicht im Plugin,
sondern in der Firmware** — das Plugin steuert sie nur. Darum gilt:

- **Die Firmware muss mit TTS gebaut sein.** Die Umgebung `dfrobot-ai-cam` aus
  `platformio_override_dfrobot.ini` tut das (`-DTINYC_TTS -DUSE_PICOTTS`). In einer Standard-Firmware
  ohne diese Schalter ist PicoTTS abgeschaltet: `I2STTS` scheitert dann mit `PTT: picotts_init
  failed`, obwohl das Plugin läuft.
- **Die Stimmen sind Dateien**, keine Firmware. Sie liegen im Baum unter
  `lib/libesp32_div/pico/lang/`. Für Deutsch brauchst du zwei Stück und musst sie **umbenennen**:

  | Datei im Baum | Name auf der Kamera | Größe |
  |---|---|---|
  | `de-DE_ta.bin` | `/picotts_de-DE_ta.bin` | 441 KB |
  | `de-DE_gl0_sg.bin` | `/picotts_de-DE_sg.bin` | 635 KB |

  Sie gehören in das Hauptverzeichnis des **Dateisystems, in dem das Plugin sucht**: ist eine SD-Karte
  eingesteckt, ist das die SD-Karte (dieselbe, auf der `/sd/ped.espdl` liegt), sonst der Flash.
  Hochladen unter **Werkzeuge → Dateisystem verwalten** (Dateien über 256 KB laufen dabei automatisch
  über Port 83). Andere Sprachen genauso: `en-US`, `en-GB`, `fr-FR`, `it-IT`, `es-ES`.
- Beim ersten `I2STTS` lädt das Plugin beide Dateien in den PSRAM (zusammen rund 1 MB, dazu der
  Arbeitsspeicher der Engine). Der DFR1154 hat 8 MB PSRAM, das reicht.

Probe in der Konsole:

```
I2STTS Guten Tag, hier spricht die Kamera
I2STTSLang          zeigt die geladene Sprache
I2STTSLang en-US    wechselt die Sprache (die beiden Dateien dazu müssen da sein)
```

Im Log steht beim ersten Mal `PTT: loaded /picotts_de-DE_ta.bin …` und `PTT: ready (lang=de-DE …)`.
Standardsprache ist `de-DE`.

## 10. Wenn etwas nicht geht

| Anzeichen | Ursache |
|---|---|
| Keine TinyC-IDE, kein `TinyC`-Befehl | `user_config_override.h` fehlt oder liegt am falschen Ort (Abschnitt 3) |
| `berry.h: No such file` | alte Fassung der Kopfdatei: neu aus `tasmota/tinyc/dfrobot/` kopieren (dort steht jetzt `#undef USE_AUTOCONF`) |
| `undefined reference to dl::Model::…` | `lib_deps = symlink://lib/libesp32_dl/esp-dl` fehlt in der Umgebung |
| `camControl(21,…)` liefert -1 | Firmware ohne ESP-DL, oder das Modell liegt nicht unter `/sd/ped.espdl` |
| `"Error":"Unknown opcode"` im Slot | die Firmware ist älter als der Übersetzer (Abschnitt 7) |
| Gerät nach OTA im Safeboot | Abschnitt 5, der Weg von Hand |
| Kein Audio | Plugin fehlt, nicht gestartet (Autostart!) oder falsche Pins (`I2SAUDIO.cfg`, Abschnitt 9) |
| `PTT: picotts_init failed` / `PTT: voice load failed` | Firmware ohne TTS (`-DTINYC_TTS` fehlt), oder die Stimmdateien `/picotts_de-DE_ta.bin` und `_sg.bin` fehlen bzw. liegen im falschen Dateisystem (Abschnitt 9, Sprachausgabe) |
| Bild nur halb / Streifen bei wenig Licht | IR-LED-Welligkeit bei hoher Verstärkung — `webcam_tinyc.tc` begrenzt sie, nicht verändern |

Wenn du den Baum aktualisierst: `git pull`, die beiden Dateien aus `tasmota/tinyc/dfrobot/`
**neu** nach Abschnitt 3 kopieren (sie ändern sich mit dem Baum), `pio run -e dfrobot-ai-cam`.

Viel Erfolg — melde dich, wenn etwas klemmt.
Gerhard
