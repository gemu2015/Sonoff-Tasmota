# TinyC fuer Tasmota

**TinyC** ist ein C-Subset-Compiler mit virtueller Maschine, der auf der ESP32-Familie (ESP32, S3, C3, C6, P4) und, mit kleinerem Funktionsumfang, auf dem ESP8266 als Tasmota-Treiber `XDRV_124` laeuft. C-Code wird im Browser-IDE geschrieben, zu portablem Bytecode kompiliert, auf das Geraet hochgeladen und ausgefuehrt — ohne Firmware-Neubau, ohne Compiler auf dem Geraet.

![TinyC Browser-IDE](images/Tinyc_ide.png){ loading=lazy }

## Warum TinyC

- **Portabler Bytecode** — einmal kompilieren, dasselbe Binaerformat laeuft auf ESP32, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-P4 oder ESP8266. Ein Programm braucht eine Firmware mit gleicher oder neuerer Syscall-ABI: die Firmware weist neueren Bytecode beim Laden ab, und die IDE uebersetzt passend zur ABI des verbundenen Geraets.
- **Kein Compiler auf dem Geraet** — kompiliert wird im Browser, in der Firmware steckt weder Parser noch Codegenerator. Der Interpreter selbst ist winzig; der gesamte TinyC-Treiber (VM, Systemaufrufe, Web-Widgets, IDE-Auslieferung, Hardware-Anbindungen) belegt aber etwa 200 KB Flash — das Flash-Budget steht unter [Eigene Builds](custom-builds.md).
- **Bekannte C-Syntax** — `int`, `float`, `char[]`, Strukturen, 2D-Arrays, Funktionszeiger, Referenzparameter, gepackte `byte[]`- / `int16[]`-Arrays, `#include` und `#if` — keine neue Sprache.
- **Viel schneller als Skript-Interpreter** — direkt gekoppelter Bytecode-Dispatch mit Superinstruktionen ohne Neu-Parsen des Quelltexts; die Benchmark-Suite laeuft schneller als Berry auf demselben Chip.
- **Echte Hintergrundtasks** — bis zu sechs VM-Slots auf dem ESP32 (einer auf dem ESP8266), und `TaskLoop()` laeuft in einem eigenen FreeRTOS-Task mit voller `delay()`-Unterstuetzung.
- **Tiefe Tasmota-Integration** — direkter Zugriff auf SML-Zaehler, I2C, SPI, 1-Wire, seriell, Display-Treiber, UDP-Multicast, MQTT, HTTP/TLS, Mail und Webseiten.
- **Mehr als Sensoren** — **Matter**-Geraete definieren (Apple Home, Google Home, Alexa), **Bluetooth LE** und Bluetooth Classic, **Kamera** mit Bewegungs- und Personenerkennung, ein **FTP-Client** (z. B. ein Logger auf dem FRITZ!NAS), FTDI-Geraete ueber den **USB-Host** des ESP32-S3, **LVGL**-Touch-Oberflaechen und Sprachausgabe ueber das **TTS-Plugin**.

## Hier beginnen

<div class="grid cards" markdown>

- :material-rocket-launch:{ .lg .middle } __[Erste Schritte](getting-started.md)__

    ---
    `USE_TINYC` im Build aktivieren, IDE hochladen, erstes Programm schreiben.

- :material-book-open-variant:{ .lg .middle } __[Referenz](reference.md)__

    ---
    Vollstaendige Funktionsreferenz — GPIO, I2C, SML, Display, HomeKit, Netzwerk.

- :material-code-braces:{ .lg .middle } __[Beispiele](examples/index.md)__

    ---
    Direkt lauffaehige Programme fuer Sensoren, Displays, Zaehler und mehr.

- :material-image-multiple:{ .lg .middle } __[Galerie](gallery/index.md)__

    ---
    Screenshots realer Projekte auf Tasmota-Hardware.

- :material-package-variant:{ .lg .middle } __[Versionen](releases.md)__

    ---
    Vorgebaute Firmware-Binaries fuers Testen.

- :material-tools:{ .lg .middle } __[Eigene Builds](custom-builds.md)__

    ---
    Feature-Flags fuer abgespeckte oder erweiterte Firmware-Varianten.

</div>

## Neueste Version

Die aktuellste Test-Firmware, das IDE-Paket, die Plugins und die Dokumentation liegen immer am [testing](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing)-Release-Tag; jeder Bau hat ausserdem ein eigenes `v<Version>`-Release. Was sich in welcher Version geaendert hat und welche Firmware ein uebersetztes Programm braucht (die Syscall-ABI-Tabelle), steht im [Changelog](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/CHANGELOG.md).

---

!!! note "Nicht mit TCC / TinyCC verwandt"
    *TinyC fuer Tasmota* ist ein eigenstaendiges Projekt ohne Verbindung zu Fabrice Bellards
    [Tiny C Compiler (TCC / TinyCC)](https://bellard.org/tcc/). Die beiden Projekte teilen
    einen aehnlichen Namen, haben aber verschiedenen Umfang: TCC ist ein nativer
    x86/ARM-C-Compiler, waehrend TinyC fuer Tasmota ein C-Subset-Compiler ist, der auf
    eine portable Bytecode-VM innerhalb von Tasmota auf ESP32/ESP8266 abzielt.
