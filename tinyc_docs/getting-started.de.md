# Erste Schritte

## 1. Eine Firmware mit TinyC besorgen

Am einfachsten ist ein vorkompiliertes Binary aus dem [Testing-Release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) (was jede Datei ist, steht unter [Versionen](releases.md)):

| Dein Geraet | Datei |
|---|---|
| ESP32, 4 MB (klassisches WROOM) | `tinyc32-4M-plain` |
| ESP32-S3, 16 MB (Matter eingebaut, Kamera, LVGL) | `tinyc32s3` |
| ESP32-C3 | `tinyc32c3` |
| ESP32-C6 | `tinyc32c6` |
| ESP32-P4 (Display, Kamera, Audio) | `tinyc32-p4-full` |
| ESP8266, 4 MB (schlank, ein VM-Slot) | `tinyc8266-4M` |

- **Neuinstallation per Kabel:** die `.factory.bin` mit esptool oder einem Web-Installer flashen. Sie enthaelt Bootloader, Partitionstabelle und App und **ersetzt das Dateisystem**.
- **Laufendes Geraet aktualisieren:** die `.bin` nehmen. Entweder ueber die Seite **Firmware-Upgrade**, oder in der Konsole `OtaUrl <url-der-bin>` und danach `Upgrade 1` — das Geraet holt die Datei dann selbst; das ist bei Geraeten mit Safeboot-Partitionslayout der zuverlaessige Weg. Eine `.factory.bin` wird nie auf diese Weise hochgeladen.
- **Flashen aktualisiert die IDE nicht** (Schritt 2), und ein fuer eine neuere Firmware uebersetztes Programm wird von einer aelteren abgewiesen. Also: erst die Firmware, dann die IDE, dann neu uebersetzen.

Wer selbst baut, traegt Folgendes in `user_config_override.h` ein (die vielen optionalen Teile stehen unter [Eigene Builds](custom-builds.md)):

```c
#define USE_TINYC         // TinyC-VM aktivieren (XDRV_124)
#define USE_TINYC_IDE     // Eingebaute Browser-IDE (benoetigt USE_UFILESYS)
```

`USE_TINYC_IDE` fuegt den Endpunkt `/tinyc_ide.html` hinzu. Erfordert einen Build mit
Dateisystem (`USE_UFILESYS`).

## 2. Die IDE auf das Geraet bringen

Die IDE ist eine **Datei im Dateisystem des Geraets** (`/tinyc_ide.html.gz`), kein Teil der Firmware.

- **Aus der Konsole (am einfachsten):** `TinyCIde` eingeben oder auf der TinyC-Konsolenseite (`/tc`) **Update IDE** druecken. Das Geraet holt die neueste IDE aus dem Repository und ersetzt seine eigene Kopie. Das einmal nach jedem Firmware-Update tun und die Browserseite hart neu laden.
- **Von Hand:** `tinyc_ide.html.gz` aus dem [Testing-Release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) laden, **Konsolen → Dateisystem verwalten** oeffnen (oder per POST an `http://<geraet>/ufsu`) und in das Wurzelverzeichnis des Dateisystems hochladen.

Danach im Browser `http://<geraet-ip>/tinyc_ide.html` aufrufen. Eine IDE, die aelter ist als die Firmware, meldet `Undefined function: <name>` fuer eine Funktion, die die Firmware durchaus hat — dann die IDE aktualisieren.

Der TinyC-Treiber erweitert die Tasmota-Weboberflaeche um eine eigene Konsolenseite —
eine Zeile pro VM-Slot, Programm-Upload, Bytecode-Repository sowie ein Shortcut zum
Oeffnen der IDE:

![TinyC-Geraetekonsole](images/tinyc_console.png){ loading=lazy }

### Elemente der Geraetekonsole

**TinyC VM Slots** — bis zu sechs unabhaengige VM-Instanzen (0–5). Jede Zeile zeigt
den Zustand (ein farbiger Punkt und `Rdy` / `Run` / ein Fehler), die geladene `.tcb`-Datei samt Groesse, gefolgt von fuenf Aktionsknoepfen:

| Knopf | Wirkung |
|-------|---------|
| :material-play: gruen | Programm im Slot starten / fortsetzen |
| :material-stop: dunkel | Ausfuehrung stoppen und Heap-Speicher freigeben |
| :material-refresh: blau | Dieselbe `.tcb` erneut aus Flash laden und neu starten |
| :material-eject: blau | **Auswerfen** — Bytecode und VM-Speicher zurueckgeben und den Dateinamen vergessen; der Slot ist danach leer |
| **A** blau | Autoexec umschalten — dieser Slot startet bei jedem Boot |

**Load Program** — eine bereits auf dem Geraet liegende `.tcb` auswaehlen und in
den gewuenschten Slot laden. Der Knopf **i** neben der Liste oeffnet den zum gewaehlten Programm hinterlegten Info-Link (abgeblendet, wenn es keinen gibt). **Delete All .tcb** entfernt jeden kompilierten
Bytecode aus dem Flash (die Quelldateien `.tc` auf dem PC bleiben unberuehrt).

**Repository** — Online-Bytecode-Bibliothek. Das Auswahlmenue listet vorkompilierte
Beispiele (**Refresh list** laedt sie neu); **Download & Load** holt die Datei auf das Geraet und laedt sie
zugleich in den gewaehlten Slot.

**Upload Program** — eine lokal kompilierte `.tcb` direkt in einen Slot senden.
Nuetzlich beim Entwickeln ausserhalb der IDE.

**TinyC IDE** — **Open IDE** oeffnet `/tinyc_ide.html` in einem neuen Tab (direkt aus dem
Geraete-Dateisystem; keine Cloud, kein externer Host). **Update IDE** holt die neueste IDE aus dem Repository und ersetzt die Kopie auf dem Geraet — das einmal nach jedem Firmware-Update tun. **Run IDE from repo** oeffnet die vollstaendige IDE aus dem Repository im Browser (der Browser braucht Internet, das Geraet nicht), ganz ohne IDE-Datei auf dem Geraet; sie fragt das Geraet nach seiner ABI und uebersetzt passend dazu, und man kann ein Beispiel und einen Slot waehlen und auf Run druecken (`/tcrepo`).

**Display Mirror** — Live-Browseransicht des angeschlossenen Displays bei Geraeten
mit TFT / OLED / E-Paper.

**Werkzeuge / Tools** — zurueck zum Standard-Menue **Werkzeuge** von Tasmota.

## 3. Das erste Programm

```c
int main() {
    addLog("Hallo aus TinyC!");
    return 0;                       // main() muss zurueckkehren: danach starten die Callbacks unten
}

void EverySecond() {
    float t = tasm_temp;            // Temperatur des ersten Tasmota-Sensors (0, falls es keinen gibt)
    char buf[64];
    sprintf(buf, "temp=%.1f C, Laufzeit %d s", t, tasm_uptime);
    addLog(buf);
}
```

- **Ctrl+Enter** kompiliert.
- **Ctrl+Shift+Enter** laedt hoch und startet.
- **Stop** bricht die Ausfuehrung ab.

`main()` laeuft einmal; nach der Rueckkehr ruft Tasmota `EverySecond()` und die anderen [Callbacks](reference.md) nach ihrem Takt auf.

Konsolenausgaben erscheinen im Tasmota-Tab **Konsole**.

![TinyC-IDE](images/Tinyc_ide.png){ loading=lazy }

### Elemente der Browser-IDE

**Obere Werkzeugleiste** (von links nach rechts):

| Knopf | Zweck |
|-------|-------|
| **New** | Leerer Editor + neuer Dateiname |
| **Open** | `.tc`-Quelldatei vom PC laden |
| **Save** | Aktuelle Quelle auf PC speichern |
| **Load Example…** | Eines der 54 mitgelieferten Programme waehlen (Sensoren, Displays, Charts, Matter, BLE, Netzwerk) |
| **Repo Examples…** | Das gesamte Online-Beispiel-Repository (ueber 200 Programme; die Liste zeigt die aktuelle Zahl) durchsuchen und direkt laden |
| **Incl** | Einen Ordner mit `.tc` / `.h` / `.c`-Dateien als `#include`-Quellen ins Programm holen |
| **Compile** | Parsen + Bytecode erzeugen (Ausgabe im linken Bereich) |
| **Save .tcb** | Den kompilierten Bytecode (`.tcb`) auf den PC herunterladen |
| **Run** | Bytecode in der Browser-VM ausfuehren (ohne Geraet) |
| **Slot** | Ziel-VM-Slot (0–5) fuer die folgenden Geraete-Aktionen |
| **Device IP** | Adresse des Tasmota-Geraets (wird automatisch gesetzt, wenn die IDE vom Geraet ausgeliefert wird) |
| **Upload** | `.tcb` an das Dateisystem des verbundenen Geraets senden |
| **Run on Device** | Hochladen + in den gewaehlten Slot laden + starten, alles in einem Klick |
| **Device Files…** | Dateien auf dem Geraet auflisten / herunterladen / loeschen |
| **Save File** | Quelltext und Bytecode gemeinsam auf dem Geraet ablegen |
| **✕ Close** | Aktuellen Tab schliessen |
| **EN / DE** | Sprachumschaltung der Oberflaeche |

**Linker Bereich — Tabs** (Compiler-Einblicke):

- **Output** — Compiler-Meldungen, VM-Ausgabe, Fehlerorte
- **Disassembly** — lesbare Bytecode-Auflistung mit Opcode-Offsets
- **AST** — geparster Syntaxbaum (hilfreich, wenn ein Konstrukt nicht kompiliert)
- **Hex** — roher `.tcb`-Bytedump zum Pruefen oder manuellen Hochladen
- **VM State** — Live-Register, Stack, Heap und globale Variablen waehrend der Ausfuehrung

**Rechter Bereich — Tabs** (Quellen):

- **editor.tc** — der TinyC-Quelltext im Editor
- **SML Descriptor** — separater Textpuffer fuer Smart-Meter-Descriptor-Zeilen;
  wird bei Bedarf zusammen mit dem Programm zum Geraet geschickt

**Statusleiste (unten)** — zeigt `Ready` / Statusmeldungen; sobald eine IP eingetragen
ist und das Geraet antwortet, auch die Anzahl der Dateien im Dateisystem des Geraets
(aktualisiert sich bei jedem Upload oder Loeschen).

## 4. Wohin als Naechstes

- Die [Funktionsreferenz](reference.md) durchblaettern — jeder Syscall mit Signatur und Beispiel.
- Die [Beispiele](examples/index.md) ansehen — lauffaehiger Code fuer typische Sensoren, Displays und Protokolle.
- In die [Galerie](gallery/index.md) schauen — Screenshots von Projekten auf realer Hardware.
- Matter auf einem C3, C6 oder P4 braucht ein Plugin: [Matter als Plugin installieren](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/docs/MATTER_PLUGIN_DE.md). Sprachausgabe: [PICOTTS-Plugin](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/docs/PICOTTS_PLUGIN_DE.md).
- Jede Aenderung, Version fuer Version, steht im [Changelog](https://github.com/gemu2015/Sonoff-Tasmota/blob/universal/tasmota/tinyc/CHANGELOG.md).
