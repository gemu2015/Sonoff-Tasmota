# Erste Schritte

## 1. Tasmota mit TinyC bauen

Folgendes in die Datei `user_config_override.h` eintragen:

```c
#define USE_TINYC         // TinyC-VM aktivieren (XDRV_124)
#define USE_TINYC_IDE     // Eingebaute Browser-IDE (benoetigt USE_UFILESYS)
```

`USE_TINYC_IDE` fuegt den Endpunkt `/tinyc_ide.html` hinzu. Erfordert einen Build mit
Dateisystem (`USE_UFILESYS`).

Alternativ laesst sich eine vorkompilierte Firmware von der [Versionen](releases.md)-Seite
direkt flashen.

## 2. IDE hochladen

1. `tinyc_ide.html.gz` aus dem [Testing-Release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) laden.
2. In Tasmota **Konsolen → Dateisystem verwalten** oeffnen (oder per POST an `http://<geraet>/ufsu` senden).
3. `tinyc_ide.html.gz` in das Wurzelverzeichnis des Dateisystems hochladen.
4. Im Browser `http://<geraet-ip>/tinyc_ide.html` aufrufen.

Der TinyC-Treiber erweitert die Tasmota-Weboberflaeche um eine eigene Konsolenseite —
eine Zeile pro VM-Slot, Programm-Upload, Bytecode-Repository sowie ein Shortcut zum
Oeffnen der IDE:

![TinyC-Geraetekonsole](images/tinyc_console.png){ loading=lazy }

### Elemente der Geraetekonsole

**TinyC VM Slots** — bis zu sechs unabhaengige VM-Instanzen (0–5). Jede Zeile zeigt
die aktuell geladene `.tcb`-Datei samt Groesse, gefolgt von vier Aktionsknoepfen:

| Knopf | Wirkung |
|-------|---------|
| :material-play: gruen | Programm im Slot starten / fortsetzen |
| :material-stop: dunkel | Ausfuehrung stoppen und Heap-Speicher freigeben |
| :material-refresh: blau | Dieselbe `.tcb` erneut aus Flash laden und neu starten |
| **A** blau | Autoexec umschalten — dieser Slot startet bei jedem Boot |

**Load Program** — eine bereits auf dem Geraet liegende `.tcb` auswaehlen und in
den gewuenschten Slot laden. **Delete All .tcb** entfernt jeden kompilierten
Bytecode aus dem Flash (die Quelldateien `.tc` auf dem PC bleiben unberuehrt).

**Repository** — Online-Bytecode-Bibliothek. Das Auswahlmenue listet vorkompilierte
Beispiele; **Download & Load** holt die Datei auf das Geraet und laedt sie
zugleich in den gewaehlten Slot.

**Upload Program** — eine lokal kompilierte `.tcb` direkt in einen Slot senden.
Nuetzlich beim Entwickeln ausserhalb der IDE.

**TinyC IDE** — oeffnet `/tinyc_ide.html` in einem neuen Tab (direkt aus dem
Geraete-Dateisystem; keine Cloud, kein externer Host).

**Display Mirror** — Live-Browseransicht des angeschlossenen Displays bei Geraeten
mit TFT / OLED / E-Paper.

**Werkzeuge / Tools** — zurueck zum Standard-Menue **Werkzeuge** von Tasmota.

## 3. Das erste Programm

```c
void main() {
    addLog("Hallo aus TinyC!");
}

void EverySecond() {
    float t = temperature();
    char buf[64];
    sprintf(buf, "temp=%.1f C", t);
    addLog(buf);
}
```

- **Ctrl+Enter** kompiliert.
- **Ctrl+Shift+Enter** laedt hoch und startet.
- **Stop** bricht die Ausfuehrung ab.

Konsolenausgaben erscheinen im Tasmota-Tab **Konsole**.

![TinyC-IDE](images/Tinyc_ide.png){ loading=lazy }

### Elemente der Browser-IDE

**Obere Werkzeugleiste** (von links nach rechts):

| Knopf | Zweck |
|-------|-------|
| **New** | Leerer Editor + neuer Dateiname |
| **Open** | `.tc`-Quelldatei vom PC laden |
| **Save** | Aktuelle Quelle auf PC speichern |
| **Load Example…** | Eines der 51 mitgelieferten Programme waehlen (Sensoren, Displays, Charts, Matter, BLE, Netzwerk) |
| **Repo Examples…** | Das groessere Online-Beispiel-Repository (~150 Programme) durchsuchen und direkt laden |
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
