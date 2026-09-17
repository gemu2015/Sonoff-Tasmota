# Personenerkennung auf der DFRobot AI CAM — Bauanleitung

*Notiz an Hans, 16.09.2026*

Hallo Hans,

du hast ja jetzt dieselbe Kamera wie unsere .124 — die **DFRobot FireBeetle 2
ESP32-S3 AI CAM (DFR1154, OV3660)**. Wir haben gestern und heute ESP-DL in
Tasmota gebracht, und sie erkennt jetzt Personen auf dem Kamerabild. Hier steht
alles, was du zum Nachbauen brauchst.

Zur Erwartungshaltung vorweg: DFRobot bewirbt die Kamera als „AI CAM", aber der
grösste Teil der beworbenen KI läuft gar nicht auf dem Chip — OpenCV und YOLOv5
laufen im PC, das ChatGPT-Beispiel in der Cloud. Was **wirklich auf dem S3**
läuft, ist das hier, und das funktioniert gut.

---

## Was dabei herauskommt

Alles an der Hardware gemessen, nicht geschätzt:

| | |
|---|---|
| Personenerkennung (224×224) | **216 ms** |
| JPEG eines 640×480-Bildes entpacken | 252 ms |
| zusammen je Bild | **~470 ms** |
| Modell laden (einmalig, bleibt geladen) | 1,1 s |
| Firmware wächst um | **+759 kB** (1429 → 2232 kB) |
| PSRAM-Bedarf des Modells | 2,29 MB von 8 MB |
| internes RAM | 76 kB |

Erkannte Scores im Betrieb: 61, 80, 83, 85, 89, 91, **92 %**.

---

## Voraussetzungen

* **ESP32-S3 mit 16 MB Flash und 8 MB PSRAM.** Auf 4 MB oder mit 2 MB PSRAM
  lohnt es nicht.
* Der Baum muss die drei Commits von heute haben — such nach
  `lib/libesp32_dl/`. Ist der Ordner da, passt es.

---

## Schritt 1: Bauumgebung anlegen

⚠️ **`platformio_override.ini` steht in `.gitignore`** — die Umgebung kommt also
**nicht** mit dem `git pull`. Du musst sie selbst anlegen. Diesen Block
hineinkopieren:

```ini
[env:dfrobot-cam]
  extends                 = env:tasmota32
  board_build.f_cpu       = 240000000L
  board_build.mcu         = esp32s3
  board                   = esp32s3-qio_opi

  build_flags             = ${env:tasmota32.build_flags} -DARDUINO_USB_MODE=1 -DUSE_USB_CDC_CONSOLE
                            -DUSE_TINYC_ESPDL
                            -Ilib/libesp32_dl/esp-dl/dl
                            -Ilib/libesp32_dl/esp-dl/dl/tool/include
                            -Ilib/libesp32_dl/esp-dl/dl/tensor/include
                            -Ilib/libesp32_dl/esp-dl/dl/base
                            -Ilib/libesp32_dl/esp-dl/dl/base/isa
                            -Ilib/libesp32_dl/esp-dl/dl/base/isa/tie728
                            -Ilib/libesp32_dl/esp-dl/dl/base/isa/xtensa
                            -Ilib/libesp32_dl/esp-dl/dl/math/include
                            -Ilib/libesp32_dl/esp-dl/dl/model/include
                            -Ilib/libesp32_dl/esp-dl/dl/module/include
                            -Ilib/libesp32_dl/esp-dl/fbs_loader/include
                            -Ilib/libesp32_dl/esp-dl/vision/detect
                            -Ilib/libesp32_dl/esp-dl/vision/image
                            -Ilib/libesp32_dl/esp-dl/vision/image/isa
                            -DCONFIG_PIX_CVT_RGB888_TO_RGB888_SUPPORT=1
                            -DCONFIG_PIX_CVT_RGB888_TO_GRAY_SUPPORT=1
                            -DCONFIG_PIX_CVT_RGB565_TO_RGB888_SUPPORT=1
                            -Llib/libesp32_dl/esp-dl/fbs_loader/lib/esp32s3
                            -lfbs_model

  lib_extra_dirs          = ${common.lib_extra_dirs}
                            lib/libesp32
                            lib/libesp32_audio
                            lib/lib_deprecated

  lib_deps                = symlink://lib/libesp32_dl/esp-dl
```

Drei Zeilen darin sind nicht optional, und jede hat mich heute Zeit gekostet:

1. **`lib_deps = symlink://…`** — die Bibliothek liegt zwei Ebenen tief, und
   `lib_compat_mode = strict` verwirft **wortlos** alles, was in `library.json`
   nicht `platforms` und `frameworks` deklariert. Ohne diese Zeile wird sie
   einfach nicht gebaut, und du merkst es erst beim Binden an
   `undefined reference to dl::Model::…`.

2. **`-Ilib/…/vision/image/isa`** — das sieht nach P4-Assembler aus, enthält
   aber auch `dl_image_color_isa.hpp`, das jede Farbdatei einbindet.

3. **`-DCONFIG_PIX_CVT_…`** — ESP-DL schaltet seine Farbumwandlungen per
   Kconfig zu. Ohne Kconfig sind nur die drei Dateien mitkopiert, die wir
   brauchen; die Schalter müssen dazu passen.

⚠️ **Kein Kommentar zwischen `build_flags` und seiner Fortsetzungszeile.** Das
bricht in INI-Dateien die Fortsetzung, und die Schalter fallen still weg.

---

## Schritt 2: Bauen

```bash
pio run -e dfrobot-cam
```

Sollte ~80 s dauern und mit rund **2232 kB** enden.

⚠️ Falls der erste Lauf mit `ModuleNotFoundError: No module named 'yaml'`
abbricht: einfach **noch einmal** starten. PlatformIO installiert die Plattform
während des ersten Laufs und ist dann noch nicht fertig eingerichtet.

---

## Schritt 3: Aufspielen

```bash
python3 tasmota/tinyc/utils/ota_flash.py <ip> --env dfrobot-cam
```

⚠️ **Nicht** über `POST /u2` in der Weboberfläche — das scheitert bei dieser
Partitionsaufteilung immer mit „Nicht genug Speicherplatz". Das Skript lässt das
Gerät die Datei selbst abholen, das geht zuverlässig.

---

## Schritt 4: Modell aufs Gerät

Das Modell ist **nicht** im Baum — es wird zur Laufzeit als Datei geladen. Das
ist Absicht: so bleibt die OTA klein, und ein Modellwechsel ist ein Upload statt
eines Neuflashens.

```bash
git clone --depth 1 https://github.com/espressif/esp-dl.git
# 425 kB, unbedingt die s3-Fassung:
#   esp-dl/models/pedestrian_detect/models/s3/pedestrian_detect_pico_s8_v1.espdl
```

Hochladen über die Dateiverwaltung der Weboberfläche, oder:

```bash
curl -F "ufsu=@pedestrian_detect_pico_s8_v1.espdl;filename=ped.espdl" \
     "http://<ip>/ufsu?fsz=435328"
```

⚠️⚠️ **Der Pfad ist die Stelle, an der ich am längsten gesucht habe.** Wenn im
Gerät eine SD-Karte steckt, landet die Datei **auf der Karte**, und die hängt
unter `/sd`. Die Weboberfläche zeigt dir `/ped.espdl`, das C-`fopen` braucht
aber `/sd/ped.espdl`. Prüf mit `UfsType`:

* `{"UfsType":[1,3]}` → SD-Karte **und** Flash vorhanden, deine Datei liegt
  unter **`/sd/ped.espdl`**
* `{"UfsType":[3]}` → nur Flash, dann ist es **`/ped.espdl`**

Der Pfad steht im Treiber als `TC_DL_PERSON_MODEL` und ist auf `/sd/ped.espdl`
vorbelegt. Ohne Karte musst du ihn in `xdrv_124_tinyc.ino` ändern.

**Erster Test, noch ohne Skript:**

```
TinyCDl /sd/ped.espdl
```

Antwortet mit Speicherplan und Laufzeiten. `"Test":"FAIL"` ist dabei **normal** —
die Modelle aus dem Zoo bringen keine eingebetteten Sollwerte mit, nur
Espressifs Lehrbeispiel tut das.

---

## Schritt 5: Das Kameraskript

`tasmota/tinyc/examples/webcam_tinyc.tc`, übersetzen und in **Slot 0**:

```bash
node tasmota/tinyc/tc_deploy.mjs tasmota/tinyc/examples/webcam_tinyc.tc <ip> --slot 0
```

⚠️ `--slot 0` **nicht weglassen** — ohne Slot-Angabe geht alles nach Slot 0, was
auf einem Gerät mit mehreren belegten Slots den falschen hinauswirft.

Falls node bei dir nicht ins LAN kommt (`EHOSTUNREACH` — auf dem Mac ist das die
fehlende Erlaubnis „Lokales Netzwerk", die es kopflos nicht erfragen kann), dann
nur übersetzen und mit curl hochladen:

```bash
node tasmota/tinyc/tc_deploy.mjs tasmota/tinyc/examples/webcam_tinyc.tc --no-upload
SZ=$(stat -f%z tasmota/tinyc/bytecode/webcam_tinyc.tcb)
curl -F "file=@tasmota/tinyc/bytecode/webcam_tinyc.tcb;filename=webcam_tinyc.tcb" \
     "http://<ip>/tc_upload?api=1&fsz=$SZ&slot=0"
curl --get --data-urlencode "cmnd=TinyCStop 0"                     "http://<ip>/cm"
curl --get --data-urlencode "cmnd=TinyCRun 0 /webcam_tinyc.tcb"    "http://<ip>/cm"
```

⚠️⚠️ **Der Übersetzer muss zur Firmware passen.** `tc_deploy.mjs` benutzt die IDE
**aus dem Quellbaum**. Ist die Firmware auf dem Gerät älter als der Baum,
erzeugt sie Opcodes, die die VM nicht kennt — der Slot hält mit
`Error: "Unknown opcode"` an, und der **Bau war dabei grün**. Wenn du also nach
Schritt 3 selbst geflasht hast, passt es. Falls nicht: die IDE vom Gerät holen
(`http://<ip>/ufsd?download=/tinyc_ide.html.gz`) und damit bauen.

**Nach jedem Aufspielen einmal prüfen:**

```
TinyC
```

Im Slot-JSON muss `"Error":"OK"` stehen. Das ist der einzige Beweis.

---

## Benutzen

Auf der Hauptseite → **Camera Setup**:

* **Confirm with the neural net (person detection)** — Häkchen setzen
* **Person score threshold (%)** — Vorgabe 50. Die Modellvorgabe von 70 ist für
  eine Hauskamera zu streng.

Der Aufbau ist bewusst zweistufig: die **billige Bewegungserkennung** (~40 ms)
ist der Auslöser, das **Netz** (~470 ms) die Bestätigung. Erst wenn das Netz
wirklich einen Menschen findet, geht der Alarm los. Genau das stellt die Mail
bei jeder vorbeiziehenden Wolke ab.

Auf der Hauptseite steht dann rot:

```
🚶 PERSON     12 s ago  89%  256x330@232,148
```

Von aussen über `Status 10`: `Person`, `PersonScore`, `PersonTotal`.

---

## Drei Dinge, die du wissen solltest

**Der Block passt auch in deine eigene S3-Umgebung.** Seit dem 17.09.2026
steht er ausser in `dfrobot-cam` auch in `tinyc32s3` (Matter + Kamera + LVGL)
und baut dort — mit `${env:tinyc_base.build_flags}` statt `env:tasmota32`,
sonst wortgleich. Der erste Bau dort scheiterte an einem Typnamen: der
Testbuild decodiert JPEG mit ESP32_JPDEC (`jpg_scale_t`), die DFRobot-Umgebung
mit esp32-camera (`esp_jpeg_image_scale_t`). Seit `efed6d38d` nennt der
Treiber den Typ nicht mehr — wenn dein Bau vorher mit „cannot convert
'jpg_scale_t'" abbrach, ist das der Grund, und ein `git pull` genügt.
Kostet +820 kB Flash.


**Es kostet.** LoadAvg der Hauptschleife steigt von 66 auf 140, der Heap fällt
von 184 auf 156 kB. Wenn dir das zu viel ist: Häkchen weg, der Rest läuft
weiter.

**`max_internal_size` nicht anfassen.** Der Parameter verspricht 22 % mehr
Tempo, ist aber unbenutzbar: ESP-DL prüft eine fehlgeschlagene Speicherbelegung
nicht und läuft in einen Nullzeiger (`LoadProhibited`, `EXCVADDR 00000000`).
64 kB lief bei mir **zweimal** sauber mit 152 ms und warf das Gerät beim dritten
identischen Aufruf um — es hängt an der Heap-Fragmentierung im Moment des
Aufrufs, ist also keine berechenbare Schwelle. Im Treiber steht er deshalb fest
auf 0.

---

## Wenn du weiterspielen willst

Im Modellzoo liegen fertig und vorquantisiert für den S3:

| Modell | Grösse |
|---|---|
| Hand Detect | 486 kB |
| Hand Gesture (8+ Gesten) | 768 kB |
| Gesicht finden (zweistufig) | 187 kB |
| Gesicht wiedererkennen | 1265 kB |

Die Erkennung selbst steckt in `TcPersonen` in `xdrv_124_tinyc.ino` und ist
zwanzig Zeilen lang — ein anderes Modell ist im Wesentlichen ein anderer
Nachbearbeiter.

Mehr Einzelheiten stehen in `TinyC_Reference_DE.md` unter „Personenerkennung"
und in `lib/libesp32_dl/README.md`.

Viel Erfolg — melde dich, wenn etwas klemmt.
