# TinyC Sprachreferenz

**TinyC** ist eine Untermenge von C, die zu Bytecode fuer eine stackbasierte virtuelle Maschine kompiliert wird.
Es laeuft sowohl im Browser (JavaScript-VM) als auch auf ESP32/ESP8266 (als Tasmota-Treiber XDRV_124).

> **Hinweis:** Diese deutsche Referenz folgt der englischen
> [`TinyC_Reference.md`](TinyC_Reference.md), kann bei den **neuesten** Funktionen
> aber etwas hinterherhinken. Die englische Referenz ist die vollstaendige,
> massgebliche Quelle — dort sind u. a. auch Krypto (`sha256`, `hmacSha256`,
> `aesCbc`/`aesEcb`, `bin2hex`/`hex2bin`), der PSRAM-Bildspeicher (`imgCreate`,
> `imgBlit`, …), erweiterte String-Funktionen (`strContains`, `strReplace`,
> `strTrim`, `strToLower`/`strToUpper`, …) und Binaer-Datei-I/O
> (`fileReadBin`/`fileWriteBin`) dokumentiert.

---

## Inhaltsverzeichnis

1. [Datentypen](#datentypen)
2. [Literale](#literale)
3. [Variablen & Gueltigkeitsbereich](#variablen--gueltigkeitsbereich)
4. [Operatoren](#operatoren)
5. [Kontrollfluss](#kontrollfluss)
6. [Funktionen](#funktionen)
7. [Callback-Funktionen](#callback-funktionen)
8. [Tasmota-Systemvariablen](#tasmota-systemvariablen)
9. [Arrays](#arrays)
10. [Zeichenketten](#zeichenketten)
11. [Praeprozessor](#praeprozessor)
12. [Kommentare](#kommentare)
13. [Typumwandlung](#typumwandlung)
14. [Eingebaute Funktionen](#eingebaute-funktionen)
15. [Multi-VM Slots (ESP32)](#multi-vm-slots-esp32)
16. [VM-Grenzen](#vm-grenzen)
17. [Geraetedateiverwaltung (IDE)](#geraetedateiverwaltung-ide)
18. [Tastenkuerzel (IDE)](#tastenkuerzel-ide)
19. [Beispiele](#beispiele)

---

## Datentypen

| Typ     | Groesse | Beschreibung                              |
|---------|---------|-------------------------------------------|
| `int`   | 32-Bit  | Vorzeichenbehaftete Ganzzahl              |
| `float` | 32-Bit  | IEEE 754 Gleitkommazahl                   |
| `char`  | 8-Bit   | Vorzeichenloses Zeichen (maskiert auf 0xFF) |
| `bool`  | 32-Bit  | Boolescher Wert (0 = false, ungleich 0 = true) |
| `void`  | —       | Kein Wert (Rueckgabetyp fuer Funktionen)  |

### Typ-Aliase

| Alias          | Entspricht |
|----------------|------------|
| `int32_t`      | `int`      |
| `uint32_t`     | `int`      |
| `unsigned int` | `int`      |
| `uint8_t`      | `char`     |

---

## Literale

### Ganzzahl-Literale
```c
42          // dezimal
0xFF        // hexadezimal (Praefix 0x oder 0X)
0b1010      // binaer (Praefix 0b oder 0B)
```

### Gleitkomma-Literale
```c
3.14        // Dezimalpunkt
2.5f        // mit Float-Suffix
0.001       // fuehrende Null
```

### Zeichen-Literale
```c
'A'         // einzelnes Zeichen
'\n'        // Escape-Sequenz
'\0'        // Null-Terminator
```

**Unterstuetzte Escape-Sequenzen:** `\n` `\t` `\r` `\\` `\'` `\"` `\0`

### Zeichenketten-Literale
```c
"Hello"             // einfache Zeichenkette
"Line 1\nLine 2"    // mit Escape-Sequenzen
```
Zeichenketten-Literale werden fuer die Initialisierung von `char`-Arrays und als Argumente fuer Zeichenketten-Funktionen verwendet.

### Boolesche Literale
```c
true        // ergibt 1
false       // ergibt 0
```

---

## Variablen & Gueltigkeitsbereich

### Globale Variablen
Ausserhalb jeder Funktion deklariert. Von allen Funktionen aus zugaenglich.
```c
int counter = 0;
float pi = 3.14;
char buffer[64];
```

### Persistente Variablen
Globale Variablen mit dem Schluesselwort `persist` werden automatisch im Flash gespeichert und beim Programmstart wiederhergestellt. Dies entspricht den `p:` Variablen im Tasmota Scripter.

```c
persist float totalEnergy = 0.0;   // wird ueber Neustarts gespeichert
persist int bootCount;              // Skalar — 4 Bytes in Datei
persist char deviceName[32];        // Array — 32 Slots in Datei
```

- Nur globale Variablen koennen `persist` sein (keine lokalen Variablen oder Funktionsparameter)
- Persist-Variablen werden **automatisch geladen** aus einer `.pvs`-Datei (abgeleitet vom `.tcb`-Dateinamen, z.B. `/weather.pvs` fuer `/weather.tcb`) beim Programmstart
- Persist-Variablen werden **automatisch gespeichert** beim Stoppen des Programms (`TinyCStop`)
- `saveVars()` aufrufen zum manuellen Speichern (z.B. nach Mitternachts-Zaehleraktualisierung)
- Maximal 32 Persist-Eintraege pro Programm
- Binaerformat — kompakt und schnell (rohe int32 Werte, Floats als bit-cast int32)

```c
persist float dval = 0.0;
persist float mval = 0.0;

void EverySecond() {
    if (tasm_hour == 0 && last_hr != 0) {
        dval = smlGet(2);  // Tageszaehler aktualisieren
        saveVars();         // sofort speichern
    }
}
```

### Watch-Variablen (Aenderungserkennung)
Globale Variablen mit dem Schluesselwort `watch` verfolgen automatisch Aenderungen. Bei jedem Schreibzugriff wird der alte Wert als Schattenwert gespeichert — unverzichtbar fuer IOT-Monitoring.

```c
watch float power;
watch int relay;
```

- Nur skalare Globals koennen `watch` sein (int, float — keine Arrays oder lokale Variablen)
- Jeder Schreibzugriff speichert automatisch den vorherigen Wert und setzt ein Written-Flag
- Benoetigt 2 zusaetzliche Global-Slots pro Watch-Variable (Schatten + Written-Flag)

**Intrinsische Funktionen:**

| Funktion | Rueckgabe | Beschreibung |
|----------|-----------|-------------|
| `changed(var)` | `int` | 1 wenn aktueller Wert vom Schattenwert abweicht |
| `delta(var)` | `int/float` | aktuell - Schatten (vorzeichenbehaftete Differenz) |
| `written(var)` | `int` | 1 wenn Variable seit letztem `snapshot()` zugewiesen wurde |
| `snapshot(var)` | `void` | Schatten = aktuell, Written-Flag loeschen |

```c
watch float power;

void EverySecond() {
    power = sensorGet("ENERGY#Power");
    if (changed(power)) {
        float diff = delta(power);
        // auf Leistungsaenderung reagieren
        snapshot(power);  // Aenderung bestaetigen
    }
}
```

### Geteilte Variablen (UDP) — das Schluesselwort `global`

Eine skalare globale Variable, die mit dem Schluesselwort `global` deklariert wird, wird automatisch mit anderen Tasmota-Geraeten ueber UDP-Multicast geteilt — das direkte Aequivalent zu Scripter-`g:`-Variablen. **Eine Zuweisung sendet den neuen Wert automatisch per Broadcast**; die Firmware **aktualisiert die Variable automatisch**, wenn ein passender benannter Wert von einem anderen Geraet eintrifft. Es sind keine expliziten `udpSend`/`udpRecv`-Aufrufe noetig (diese bleiben fuer Arrays/Strings/manuelle Steuerung verfuegbar — siehe UDP-Multicast).

```c
global int   mh_pwr;     // schreiben -> Broadcast im Netzwerk als "mh_pwr"
global float btemp;      // automatisch aktualisiert, wenn ein anderes Geraet "btemp" sendet

void EverySecond() {
    mh_pwr = 1;                                         // sendet mh_pwr=1
    matterSetFloat(ep, CLUSTER_TEMP, 0, btemp, 100);   // nutzt den zuletzt empfangenen btemp
}
```

- Nur skalare Globals (`int`/`float`) koennen `global` sein; der **Variablenname ist der geteilte Schluessel** (entspricht dem Scripter-`g:<name>`).
- Multicast-Gruppe `239.255.255.250:1999`; der Socket initialisiert sich bei der ersten Verwendung automatisch (siehe UDP-Multicast fuer das Wire-Protokoll + `UdpCall()`).
- Mit den anderen Speicher-Schluesselwoertern kombinierbar: `global watch int x;` um auch eingehende Aenderungen zu erkennen (`written(x)`/`changed(x)` feuern bei UDP-Updates), oder `global persist float y;` um zusaetzlich einen Neustart zu ueberstehen.

### Lokale Variablen
Innerhalb von Funktionen oder Bloecken deklariert. Blockbasierter Gueltigkeitsbereich (neuer Bereich pro `{ }`).
```c
void myFunc() {
    int x = 10;        // lokal in myFunc
    if (x > 5) {
        int y = 20;    // lokal in diesem Block
    }
    // y ist hier nicht zugaenglich
}
```

### Funktionsparameter
Skalare werden als Wert uebergeben, Arrays als Referenz.
```c
void process(int value, int data[]) {
    // value ist eine Kopie, data ist eine Referenz
}
```

---

## Operatoren

### Arithmetisch
| Op  | Beschreibung   | Typen              |
|-----|----------------|--------------------|
| `+` | Addition       | int, float, char[] |
| `-` | Subtraktion    | int, float         |
| `*` | Multiplikation | int, float         |
| `/` | Division       | int, float         |
| `%` | Modulo         | nur int            |
| `-` | Unaere Negation | int, float        |

**Hinweis:** Fuer `char[]`-Variablen fuehrt `+` eine Zeichenkettenverkettung durch (siehe [Zeichenketten](#zeichenketten)).

### Vergleich
| Op   | Beschreibung         |
|------|----------------------|
| `==` | Gleich               |
| `!=` | Ungleich             |
| `<`  | Kleiner als          |
| `>`  | Groesser als         |
| `<=` | Kleiner oder gleich  |
| `>=` | Groesser oder gleich |

### Logisch
| Op     | Beschreibung                       |
|--------|------------------------------------|
| `&&`   | Logisches UND (Kurzschluss)        |
| `\|\|` | Logisches ODER (Kurzschluss)       |
| `!`    | Logisches NICHT                    |

### Bitweise
| Op  | Beschreibung    |
|-----|-----------------|
| `&` | UND             |
| `\|`| ODER            |
| `^` | XOR             |
| `~` | NICHT           |
| `<<`| Linksverschiebung  |
| `>>`| Rechtsverschiebung |

### Zuweisung
| Op  | Beschreibung                                           |
|-----|--------------------------------------------------------|
| `=` | Zuweisen (fuer `char[]`: Zeichenketten-Kopie)          |
| `+=`| Addieren und zuweisen (fuer `char[]`: Zeichenkette anfuegen) |
| `-=`| Subtrahieren und zuweisen                              |
| `*=`| Multiplizieren und zuweisen                            |
| `/=`| Dividieren und zuweisen                                |

### Inkrement / Dekrement
```c
++x     // Prae-Inkrement
--x     // Prae-Dekrement
x++     // Post-Inkrement
x--     // Post-Dekrement
```

### Operatorvorrang (hoechste bis niedrigste Prioritaet)

1. Postfix: `x++` `x--` `a[i]` `f()` `(type)`
2. Unaer: `++x` `--x` `-x` `!x` `~x`
3. Multiplikativ: `*` `/` `%`
4. Additiv: `+` `-`
5. Verschiebung: `<<` `>>`
6. Relational: `<` `>` `<=` `>=`
7. Gleichheit: `==` `!=`
8. Bitweises UND: `&`
9. Bitweises XOR: `^`
10. Bitweises ODER: `|`
11. Logisches UND: `&&`
12. Logisches ODER: `||`
13. Zuweisung: `=` `+=` `-=` `*=` `/=`

---

## Kontrollfluss

### if / else
```c
if (condition) {
    // ...
}

if (condition) {
    // ...
} else {
    // ...
}

if (a > 0) {
    // ...
} else if (a == 0) {
    // ...
} else {
    // ...
}
```

### while-Schleife
```c
while (condition) {
    // ...
    if (done) break;
    if (skip) continue;
}
```

### for-Schleife
```c
for (int i = 0; i < 10; i++) {
    // ...
}

// alle Teile optional:
for (;;) {
    // Endlosschleife
    break;
}
```

### switch / case
```c
switch (value) {
    case 1:
        // ... Durchfall!
    case 2:
        // ...
        break;
    default:
        // ...
        break;
}
```
**Hinweis:** Faelle fallen durch, sofern nicht `break` verwendet wird (wie in Standard-C).

### break / continue
- `break;` — verlasse die innerste Schleife oder switch-Anweisung
- `continue;` — springe zur naechsten Iteration der innersten Schleife

---

## Funktionen

### Deklaration
```c
int add(int a, int b) {
    return a + b;
}

void doSomething() {
    // kein Rueckgabewert noetig
}
```

### Einstiegspunkt
Jedes Programm muss eine `main()`-Funktion haben:
```c
int main() {
    // Programm startet hier
    return 0;
}
```

### Rekursion
Vollstaendig unterstuetzt:
```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

### Array-Parameter
Arrays werden als Referenz uebergeben:
```c
void fill(int arr[], int size, int value) {
    for (int i = 0; i < size; i++) {
        arr[i] = value;
    }
}
```

---

## Callback-Funktionen

TinyC unterstuetzt **Callback-Funktionen**, die Tasmota automatisch bei bestimmten Ereignissen aufruft.
Definieren Sie einfach Funktionen mit diesen bekannten Namen — keine Registrierung erforderlich.

### Verfuegbare Callbacks

| Funktion | Tasmota-Hook | Wann aufgerufen | Anwendungsfall |
|----------|-------------|-----------------|----------------|
| `EveryLoop()` | FUNC_LOOP | Jede Hauptschleifen-Iteration (~1–5 ms) | Ultraschnelles Polling, Bit-Banging, zeitkritische E/A |
| `Every50ms()` | FUNC_EVERY_50_MSECOND | Alle 50 ms (20x/Sek.) | Schnelles Polling, Funkempfang, Sensorabtastung |
| `Every100ms()` | FUNC_EVERY_100_MSECOND | Alle 100 ms (10x/Sek.) | Mittleres Polling, Display-Aktualisierungen, Entprellen |
| `EverySecond()` | FUNC_EVERY_SECOND | Jede Sekunde | Periodische Aufgaben, Zaehler, langsames Polling |
| `JsonCall()` | FUNC_JSON_APPEND | Telemetriezyklus (~300s) | JSON zu MQTT-Telemetrie hinzufuegen |
| `WebPage()` | FUNC_WEB_ADD_MAIN_BUTTON | Seitenladen (einmalig) | Diagramme, benutzerdefiniertes HTML, Skripte |
| `WebCall()` | FUNC_WEB_SENSOR | Webseitenaktualisierung (~1s) | Sensorzeilen zur Tasmota-Weboberflaeche hinzufuegen |
| `WebUI()` | AJAX /tc_ui Aktualisierung | Alle 2s + bei Widget-Aenderung | Interaktives Widget-Dashboard (Schaltflaechen, Regler usw.) |
| `UdpCall()` | UDP-Paket empfangen | Bei jeder Multicast-Variable | Eingehende UDP-Variablen verarbeiten |
| `WebOn()` | Benutzerdefinierter HTTP-Endpunkt | Bei Anfrage an `webOn()`-URL | REST-APIs, JSON-Endpunkte, Webhooks |
| `TaskLoop()` | FreeRTOS-Task (ESP32) | Kontinuierliche Schleife im eigenen Task | Hintergrundverarbeitung, unabhaengig vom Haupt-Thread |
| `CleanUp()` | FUNC_SAVE_BEFORE_RESTART | Vor Geraete-Neustart | Dateien schliessen, Daten sichern, Ressourcen freigeben |
| `TouchButton(btn, val)` | Touch-Ereignis | Bei GFX-Button/Slider-Beruehrung | Touch-Button-Druecke und Slider-Aenderungen behandeln |
| `HomeKitWrite(dev, var, val)` | HomeKit-Schreibzugriff | Wenn Apple Home einen Wert aendert | Licht, Schalter, Steckdose von Apple Home steuern |
| `Command(char cmd[])` | Benutzerdefinierter Konsolenbefehl | Wenn Benutzer registriertes Praefix in Konsole eingibt | Benutzerdefinierte Tasmota-Befehle verarbeiten (z.B. MP3Play, MP3Stop) |
| `Event(char cmd[])` | Tasmota-Event-Regel-Trigger | Bei `Event`-Befehl aus Regeln oder Konsole | Auf Tasmota-Regel-Events reagieren |
| `OnExit()` | Skript-Stopp | Wenn VM gestoppt oder Skript ersetzt wird | Serielle Ports schliessen, Ressourcen freigeben |
| `OnMqttConnect()` | FUNC_MQTT_INIT | MQTT-Broker verbunden | Topics abonnieren, Status publizieren |
| `OnMqttDisconnect()` | mqtt_disconnected Flag | MQTT-Broker getrennt | Offline-Status setzen, Publizierung stoppen |
| `OnMqttData(char topic[], char payload[])` | FUNC_MQTT_DATA | Nachricht auf abonniertem Topic eingetroffen | Fernbefehle verarbeiten, Sensordaten empfangen |
| `OnInit()` | Erstes FUNC_NETWORK_UP | Einmal nach erster WiFi-Verbindung | Einmalige Init: Dienste starten, MQTT abonnieren |
| `OnWifiConnect()` | FUNC_NETWORK_UP | WiFi/Netzwerk verbunden (jedes Mal) | Reconnect-Behandlung |
| `OnWifiDisconnect()` | FUNC_NETWORK_DOWN | WiFi/Netzwerk getrennt | Netzwerkabhaengige Aufgaben pausieren |
| `OnTimeSet()` | FUNC_TIME_SYNCED | NTP-Zeit synchronisiert | Zeitbasierte Aktionen planen |

### Ausfuehrungsmodell

1. **`main()`** laeuft zuerst in einem FreeRTOS-Task (ESP32) — `delay()` funktioniert als echte blockierende Verzoegerung
2. Nach dem Anhalten von main bleiben **Globale und Heap erhalten** — sie werden NICHT freigegeben
3. Tasmota ruft periodisch Ihre Callbacks auf, die Globale lesen/aendern koennen
4. Callbacks laufen synchron mit einer Instruktionsbegrenzung — kein `delay()` erlaubt
5. Wenn `TaskLoop()` definiert ist, laeuft es im selben FreeRTOS-Task nach dem Anhalten von main() — `delay()` funktioniert, laeuft unabhaengig von Tasmota's Haupt-Thread

### Nebenlaeufigkeitsmodell — eine VM pro Slot

Jeder Slot hat genau **eine VM** — einen Operandenstack, einen Aufrufstack, einen
Programmzaehler — und sie ist **nicht wiedereintrittsfaehig**. Zwei Threads koennen
ihren Bytecode ausfuehren wollen:

- **Der eigene Task des Slots** (ESP32): wo `main()` laeuft und danach `TaskLoop()`
  sowie ein etwaiger `spawnTask()`-Worker. `delay()` und blockierende Netzwerk-I/O
  sind hier erlaubt.
- **Tasmotas Hauptschleife (`loopTask`)**: wo jeder *Callback* ausgeloest wird —
  `EverySecond`, `Every100ms`, `WebCall`, `WebUI`, `Command`, `Event`, Timer,
  MQTT-Callbacks und Out-of-band-Schreibzugriffe auf Watch-Variablen.

Nur einer von beiden darf zu einem Zeitpunkt VM-Bytecode ausfuehren. Was tatsaechlich
passiert, haengt davon ab, was der eigene Task des Slots gerade tut:

| Task-Zustand des Slots | Callbacks auf `loopTask` | Anmerkungen |
|---|---|---|
| `main()` angehalten, kein Worker | **laufen normal** | Der Standardfall. Die VM ist zwischen Callbacks im Leerlauf — voellig sicher, kein Nachdenken noetig. |
| `TaskLoop()`, **kurze** Iterationen | **laufen, verschraenkt** | Der VM-Mutex serialisiert sie: Callbacks schieben sich zwischen die Iterationen. Ideal fuer schnelle Modbus-/Seriell-Abfragen (Sub-Millisekunde). |
| `TaskLoop()`, **lange/blockierende** Iteration | **werden waehrenddessen verworfen** | Eine lange Iteration haelt den VM-Mutex; Callbacks, die dabei anfallen, werden uebersprungen. Iterationen kurz und nicht-blockierend halten. |
| `spawnTask()`-Worker aktiv | **laufen, drop-on-busy** | Auf dem ESP32 laeuft der Worker in seiner **eigenen** VM (Dual-Context, Standard), daher verteilt die primaere VM weiter `EverySecond` / `WebCall` / `WebUI` / `Command` — ein Worker **und** lokales UI auf einem Slot. Ein Callback, der anfaellt waehrend der Worker den VM-Mutex haelt, wird uebersprungen (best-effort), ohne loopTask zu blockieren. Nur **ein** `spawnTask` pro Slot. |

> **Dual-Context-Worker-VM — Standard auf ESP32 (`USE_TINYC_WORKER_VM`).** Ein
> `spawnTask()`-Worker erhaelt seine **eigene** VM (privater Operandenstack / Aufruframes /
> Heap), die sich nur das `globals[]` des Slots teilt — so koennen ein Hintergrund-Worker
> **und** ein lokales Web-UI / LCD / `EverySecond` auf **demselben** Slot koexistieren. Daten
> zwischen den Kontexten laufen ueber skalare `global`-Variablen / den Share-Store — **nicht**
> ueber Heap-Objekte (jede VM hat ihren eigenen Heap, ein Heap-Handle ist ueber die Grenze
> hinweg bedeutungslos). Schreibzugriffe auf globals[] sind per Lock geschuetzt; Callbacks
> nutzen drop-on-busy am VM-Mutex, ohne loopTask zu blockieren.

> **Deaktivieren — `USE_TINYC_NO_WORKER_VM`.** Mit diesem Flag gebaut, kehrt das Verhalten
> zum Legacy-Pfad zurueck: ein Worker leiht sich die einzige gemeinsame VM des Slots und der
> Slot wird **kopflos** — `EverySecond` / `WebCall` / `WebUI` / `Command` feuern fuer diesen
> Slot nicht mehr, solange der Worker lebt. Nur einsetzen, um den kleinen VM-Overhead pro
> Worker auf einem Slot zu sparen, der keine lokalen Callbacks braucht. (Der ESP8266 hat einen
> einzigen Slot und kein `spawnTask`, dort greift dies also nicht.)

> ⚠️ **Ein ausgelassener Web-Callback erscheint als NICHTS — keine Zeile, kein Fehler, kein
> Logeintrag.** „Werden waehrenddessen verworfen“ in der Tabelle oben ist etwas anderes als
> ein verpasster `EverySecond`-Tick: ein ausgelassener `WebCall()` bedeutet, dass die
> Sensorzeilen dieses Slots auf der ausgelieferten Seite **fehlen**, und ein ausgelassener
> `WebPage()`, dass sein **gesamtes** Zeichenprogramm fehlt — Canvas, Diagramm, Knöpfe. Der
> Browser bekommt eine gültige, vollständige, nur kürzere Seite. Von außen liest sich das als
> Flackern oder als Diagramm, das „manchmal nicht da ist“, und die naheliegenden Verdächtigen
> sind der Browser, das WLAN oder das eigene HTML.
>
> **In einer Minute festgestellt.** Dieselbe Seite zehn- bis fünfzehnmal holen und zählen, wie
> oft der eigene Block erscheint. Dann dasselbe gegen einen `webOn`-Endpunkt desselben
> Skripts. `webOn` wartet bis zu `TC_WEBON_HALTED_WAIT_MS` (1500 ms) auf die VM und wird
> deshalb fast immer bedient. Liefert `webOn` 25/25, während `WebCall`/`WebPage` bei 11/12 und
> 4/8 stehen, ist die Antwort VM-Konkurrenz mit dem eigenen `TaskLoop` — und sonst nichts.
> (Das sind Rolfs gemessene Zahlen an `max30102.tc` vom 2026-08-07 bei `delay(10)`. Die
> Schleife auf `delay(30)` anzuheben brachte den Canvas von 4/8 auf 11/12 — was die Ursache
> beweist, aber zwei Drittel der Abtastrate kostet. Also eine Diagnose, keine Lösung.)
>
> **Seit dem 2026-08-07 wartet der Seitenaufbau** auf das nächste nutzbare Fenster des Slots,
> statt ihn auf der Stelle auszulassen — gedeckelt auf 400 ms für die ganze Seite
> (`TC_WEB_PASS_BUDGET_MS`), damit ein einzelner klemmender Slot nicht den Rest aufhält.
> Auslassungen, die trotzdem passieren, werden **gezählt**: das nackte Konsolenkommando
> `TinyC` meldet `"WebSkip":N` je Slot, `TinyCInfo` zeigt es in den Webzeilen. Ein `WebSkip`,
> der stetig steigt, ist die Firmware, die einem sagt: dieser Slot ist zu beschäftigt, um sich
> selbst zu zeichnen.
>
> **Die bauliche Lösung ist, schweres Zeichnen aus `WebPage()` in einen `webOn`-Endpunkt zu
> verlegen** und auf der Hauptseite nur einen kleinen Lader zu lassen. Der bekommt die längere
> Wartezeit, wird unabhängig abgerufen statt die Seite aufzuhalten, und die Nutzlast wird nur
> geholt, wenn wirklich jemand hinsieht.
>
> ⚠️ **Wo das `delay()` steht, entscheidet, ob Callbacks überhaupt drankommen.** Die
> Reentranz-Sperre lässt einen Callback aus, sobald die VM tiefer als einen Aufruframe geparkt
> ist. Ein `TaskLoop()`, das `delay()` **direkt im eigenen Rumpf** aufruft, parkt in Tiefe 1
> und kommt durch; dasselbe `delay()` in eine Hilfsfunktion verlegt parkt in Tiefe 2, und ab da
> bleibt nur noch die Lücke von einem Tick zwischen den Iterationen. Gleiches Skript, gleiche
> Rate, eine Umstrukturierung dazwischen — und nichts im Quelltext deutet darauf hin.

**Das Muster nach der Arbeitslast waehlen:**

1. **Gelegentliche blockierende I/O + lokales UI → Single-Task, kein Worker.** Den
   blockierenden Aufruf (z. B. ein Wechselrichter-/Powerwall-`httpGet`) direkt in
   `EverySecond` auf `loopTask` ausfuehren; das LCD in `EverySecond` zeichnen und die
   Web-Zeilen in `WebCall`. Ein ~1–2 s Stillstand pro Abfrage ist kooperativ und wird
   von Tasmota toleriert — so hat die Scripter-Engine ueber Monate Displays +
   blockierendes TLS betrieben. Einen Worker nur dann einsetzen, wenn die
   Hintergrundarbeit lang oder kontinuierlich genug ist, dass ein Stillstand pro
   Sekunde inakzeptabel ist.
2. **Kurzes, haeufiges Hintergrund-Polling + lokales UI → `TaskLoop()`.**
   Sub-Millisekunden-Iterationen (Modbus, Seriell) verschraenken sich ueber den
   VM-Mutex sauber mit Callbacks. Jede Iteration kurz und nicht-blockierend halten,
   damit Callbacks nicht ausgehungert werden.
3. **Lange / kontinuierliche blockierende Arbeit + lokales UI → `spawnTask()`
   (Dual-Context, Standard).** Auf dem ESP32 laeuft der Worker in seiner eigenen VM, daher
   feuern `EverySecond` / `WebCall` weiter auf der primaeren VM — LCD / Web-Zeilen lokal
   zeichnen, waehrend der Worker blockiert. Die Grenze mit skalaren `global`-Variablen / dem
   Share-Store ueberbruecken, nie mit Heap-Objekten. (Bei einem `USE_TINYC_NO_WORKER_VM`-Build
   wird der Slot stattdessen kopflos — dann Ergebnisse als `global` (UDP)-Variablen
   veroeffentlichen und aus einem zweiten Slot oder Geraet rendern.)

> **Niemals** Display- oder Web-Widget-Syscalls (`dspText`, `webButton`, …) aus einem
> `spawnTask`-Worker aufrufen — sie veraendern geteilten Tasmota-Zustand, der
> `loopTask` gehoert, und loesen einen Hard-Reset des Geraets aus. Einen Worker auf
> Berechnung, Netzwerk und Datei-I/O beschraenken.

### Tasmota-Ausgabefunktionen

Verwenden Sie diese Funktionen in Callbacks, um Daten an Tasmota zu senden:

| Funktion | Beschreibung | Verwenden in |
|----------|-------------|--------------|
| `responseAppend(buf)` | Char-Array an JSON-Telemetrie anfuegen (-> `ResponseAppend_P`) | `JsonCall()` |
| `responseAppend("literal")` | Zeichenketten-Literal an JSON-Telemetrie anfuegen | `JsonCall()` |
| `webSend(buf)` | Char-Array an Webseite senden (-> `WSContentSend`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webSend("literal")` | Zeichenketten-Literal an Webseite senden | `WebPage()` / `WebCall()` / `WebOn()` |
| `webFlush()` | Web-Inhaltspuffer zum Client leeren (-> `WSContentFlush`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webSendFile("filename")` | Dateiinhalt vom Dateisystem an Webseite senden | `WebPage()` / `WebCall()` / `WebUI()` / `WebOn()` |
| `addCommand("prefix")` | Benutzerdefiniertes Konsolen-Befehlspraefix registrieren (z.B. `"MP3"` -> MP3Play, MP3Stop) | `main()` |
| `responseCmnd(buf)` | Char-Array als Konsolenbefehls-Antwort senden | `Command()` |
| `responseCmnd("literal")` | Zeichenketten-Literal als Konsolenbefehls-Antwort senden | `Command()` |

### Webseitenformat

Verwenden Sie Tasmota's `{s}` `{m}` `{e}` Makros in `webSend()`, um Tabellenzeilen zu erstellen:
- `{s}` — Zeile beginnen (Beschriftungsspalte)
- `{m}` — Mitte (Wertspalte)
- `{e}` — Zeile beenden

Beispiel: `"{s}Temperature{m}25.3 °C{e}"` wird als beschriftete Zeile auf der Webseite dargestellt.

### JSON-Telemetrieformat

Verwenden Sie `responseAppend()`, um JSON-Fragmente hinzuzufuegen. Beginnen Sie mit einem Komma:
- `",\"Sensor\":{\"Temp\":25}"` wird an das Telemetrie-JSON angefuegt

### Beispiel

```c
int counter = 0;

void EverySecond() {
    counter++;
}

void JsonCall() {
    // Fuegt an Tasmota MQTT-Telemetrie-JSON an
    char buf[64];
    sprintf(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

void WebCall() {
    // Fuegt eine Zeile zur Tasmota-Webseite hinzu
    char buf[64];
    sprintf(buf, "{s}TinyC Counter{m}%d{e}", counter);
    webSend(buf);
}

int main() {
    counter = 0;
    return 0;
}
```

**Ergebnis:** Nach dem Hochladen und Ausfuehren zeigt die Tasmota-Webseite eine "TinyC Counter"-Zeile, die jede Sekunde hochzaehlt, und die MQTT-Telemetrie enthaelt `,"TinyC":{"Count":N}`.

### Benutzerdefinierte Konsolenbefehle

Skripte koennen benutzerdefinierte Tasmota-Konsolenbefehle mit `addCommand("prefix")` registrieren. Wenn ein Benutzer z.B. `MP3Play Sound.mp3` in der Konsole eingibt, erkennt Tasmota das Praefix `"MP3"`, extrahiert den Unterbefehl `"PLAY SOUND.MP3"` und ruft `Command("PLAY SOUND.MP3")` im Skript auf.

**Hinweis:** Tasmota konvertiert das Befehls-Topic in Grossbuchstaben, daher kommen Unterbefehle als `"PLAY"`, `"STOP"` usw. an. Daten nach einem Leerzeichen (Dateinamen, Zahlen) behalten ihre urspruengliche Gross-/Kleinschreibung.

```c
int volume = 15;

void Command(char cmd[]) {
    char buf[64];
    if (strFind(cmd, "PLAY") == 0) {
        // Play verarbeiten
        responseCmnd("Playing");
    } else if (strFind(cmd, "STOP") == 0) {
        responseCmnd("Stopped");
    } else if (strFind(cmd, "VOL") == 0) {
        char arg[16];
        strSub(arg, cmd, 4, 0);  // alles nach "VOL " extrahieren
        volume = atoi(arg);
        sprintf(buf, "Volume: %d", volume);
        responseCmnd(buf);
    } else {
        responseCmnd("Unknown: Play|Stop|Vol");
    }
}

int main() {
    addCommand("MP3");   // Praefix "MP3" registrieren
    return 0;
}
```

**Ergebnis:** Die Eingabe von `MP3Play` in der Tasmota-Konsole ruft `Command("PLAY")` auf, `MP3Vol 20` ruft `Command("VOL 20")` auf.

### TaskLoop-Beispiel (ESP32)

```c
int counter = 0;

void TaskLoop() {
    counter++;
    char buf[64];
    sprintf(buf, "TaskLoop count=%d", counter);
    addLog(buf);       // erscheint im Tasmota-Konsolenlog
    delay(1000);       // echte 1-Sekunden-Verzoegerung, blockiert Tasmota nicht
}

void JsonCall() {
    char buf[64];
    sprintf(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

int main() {
    addLog("TaskLoop demo starting");
    return 0;
}
```

**Ergebnis:** `TaskLoop()` laeuft unabhaengig in einem FreeRTOS-Task und erhoeht den Zaehler jede Sekunde. `JsonCall()` meldet den Zaehler in der MQTT-Telemetrie. Beide laufen gleichzeitig — der Mutex stellt sicheren VM-Zugriff sicher.

### Wichtige Hinweise

- Callbacks muessen **schnell** sein — maximal 200.000 Instruktionen (ESP32) / 20.000 (ESP8266) pro Aufruf
- Kein `delay()` in Callbacks (begrenzt auf 100ms falls aufgerufen) — ausser `TaskLoop()`, das echte Verzoegerungen unterstuetzt
- `main()` muss zurueckkehren (nicht endlos schleifen), damit Callbacks aktiviert werden
- Nur die acht oben genannten bekannten Namen werden erkannt
- Der Compiler erkennt diese Funktionsnamen automatisch und bettet sie in die Binaerdatei ein
- `EveryLoop()` laeuft bei jeder Hauptschleifen-Iteration (~1–5 ms) — halten Sie es **sehr kurz**, um Tasmota nicht zu blockieren
- `Every50ms()` ist ideal fuer schnelles, nicht-blockierendes E/A-Polling (SPI-Funk, GPIO usw.)
- `Every100ms()` eignet sich fuer Display-Aktualisierungen, Tasten-Entprellen und mittlere Sensorleseraten
- Verwenden Sie `WebPage()` fuer einmaligen Seiteninhalt (Diagramme, Skripte) — wird einmal beim Laden der Seite aufgerufen
- Verwenden Sie `WebCall()` fuer Sensor-aehnliche Zeilen, die periodisch aktualisiert werden
- Verwenden Sie `UdpCall()` zur Verarbeitung eingehender UDP-Multicast-Variablen
- `TaskLoop()` laeuft in einem eigenen FreeRTOS-Task (nur ESP32) — kann `delay()` frei verwenden, VM-Zugriff ist Mutex-serialisiert mit Haupt-Thread-Callbacks

### Dynamische Task-Erzeugung (ESP32)

Ueber den festen `TaskLoop()` hinaus kann **ein zusaetzlicher Hintergrund-Task pro Slot** dynamisch per Namen gestartet werden (der Task-Pool ist global — bis zu 4 gleichzeitig ueber alle Slots). Jeder gespawnte Task teilt sich den VM-Zustand des aufrufenden Slots — Globals, Heap, Konstanten — und laeuft parallel zu `main()`, Callbacks und `TaskLoop()`. Ideal als Ersatz fuer einmalige Timer, verzoegerte Jobs oder lang laufende Hintergrundarbeiter. **Hinweis:** nur ein spawnTask pro Slot — siehe „Ein Worker pro Slot" weiter unten.

| Funktion | Beschreibung |
|----------|--------------|
| `spawnTask(char name[])` | Startet einen FreeRTOS-Task, der `name()` aufruft. Gibt Pool-Slot 0..3 zurueck oder -1 bei Fehler. Stack-Default **5 KB** |
| `spawnTask(char name[], int stack_kb)` | Gleich, aber mit anpassbarer Stack-Groesse (auf 3..16 KB begrenzt) |
| `killTask(char name[])` | Kooperativer Stopp: setzt ein Flag, das der Task beim naechsten Instruktions- oder `delay()`-Boundary erkennt. Gibt 0 bei Signal, -1 falls nicht laufend |
| `taskRunning(char name[])` | Gibt 1 zurueck, wenn ein Task mit diesem Namen im aktuellen Slot laeuft, sonst 0 |

**Name muss ein String-Literal sein** — der Compiler erzwingt dies und traegt das Ziel zur Compile-Zeit in die Bytecode-Funktionstabelle ein. Dynamische Namen (Variablen/Ausdruecke) werden nicht unterstuetzt. Ein Literal, das keine im Programm definierte Funktion benennt, ist ein Compile-Fehler.

**Stack-Groesse-Leitfaden** (Default 5 KB reicht fuer die meisten Worker):
- **3 KB** — absolutes Minimum, nur sicher wenn der Worker kein `addLog` / `sprintf*` / VM-Syscalls ausser Arithmetik und `delay()` nutzt
- **5 KB** (Default) — triviale Worker mit `addLog` + `delay`-Schleifen
- **6–8 KB** — `httpGet` ueber HTTP (ohne TLS), kleines JSON-Parsing
- **10–16 KB** — `httpGet` ueber HTTPS/TLS, grosses JSON-Parsing, komplexe Worker-Pipelines

**Semantik:**

- **Geteilte VM**: Gespawnte Tasks sehen und veraendern die gleichen Globals/Heap wie `main()`. Ideal fuer Worker-Jobs, die globalen Zustand aktualisieren.
- **Ein Worker pro Slot**: Pro Slot darf nur **ein** spawnTask laufen. Ein zweites `spawnTask(...)` auf einem Slot mit bereits aktivem Worker — **auch mit anderem Namen** — gibt -1 zurueck und loggt einen Fehler. Die einzelne VM/der Mutex des Slots kann keine zwei parallelen Worker treiben: der zweite liefe, aber der **erste friert nach einer Schleife stillschweigend ein** (kein Crash, kein Log). Den Rest in `TaskLoop()` erledigen oder in den laufenden Worker integrieren; zum Wechseln erst `killTask(...)` + `taskRunning(...)` pollen.
- **Kooperatives Toeten**: `killTask` ist nicht-blockierend. Der Task terminiert am naechsten Instruktions-Boundary oder nachdem das laufende `delay()` aufwacht. Verwenden Sie `while (taskRunning("foo")) delay(10);` zum Warten.
- **Mutex-Disziplin**: SpawnTasks ehren denselben Mutex wie `TaskLoop()`. `delay()` in einem SpawnTask gibt den Mutex frei, sodass andere Tasks und Callbacks laufen koennen.
- **Auto-Cleanup**: Beim Stoppen des Scripts (TinyCStop) werden alle gespawnten Tasks signalisiert und erhalten 2 s zum Beenden.
- **Keine Argumente**: Die gespawnte Funktion nimmt keine Parameter; ihr Rueckgabewert wird ignoriert.

**Beispiel — einmaliger verzoegerter Job:**

```c
void Blinker() {
    for (int i = 0; i < 5; i++) {
        gpioWrite(2, 1); delay(200);
        gpioWrite(2, 0); delay(200);
    }
}

void Command(char s[]) {
    if (strcmp(s, "BLINK") == 0) {
        if (taskRunning("Blinker")) {
            addLog("Blinker bereits aktiv");
        } else {
            spawnTask("Blinker");
        }
    }
}

int main() { return 0; }
```

`TinyCCmd BLINK` in der Konsole spawnt den Blinker ohne die Konsole zu blockieren. Ein zweites `TinyCCmd BLINK` waehrend des Blinkens wird abgelehnt.

**Beispiel — paralleler Hintergrund-Downloader:**

```c
char url[] = "http://example.com/data.json";
int download_done = 0;
char body[2048];

void Downloader() {
    int rc = httpGet(url, body, sizeof(body));
    download_done = (rc > 0) ? 1 : -1;
}

void EverySecond() {
    if (download_done == 1) {
        addLog("Download ok");
        download_done = 0;
    } else if (download_done == -1) {
        addLog("Download fehlgeschlagen");
        download_done = 0;
    }
}

int main() {
    spawnTask("Downloader", 6);  // 6 KB Stack fuer HTTPS
    return 0;
}
```

**Beispiel — toetbarer Worker:**

```c
int worker_ticks = 0;

void Worker() {
    while (1) {
        worker_ticks++;
        delay(500);
    }
}

void Command(char s[]) {
    if (strcmp(s, "START") == 0 && !taskRunning("Worker")) spawnTask("Worker");
    if (strcmp(s, "STOP")  == 0) killTask("Worker");
}

int main() { return 0; }
```

**Grenzen:**

- Max. 4 gleichzeitige gespawnte Tasks **pro Geraet** (gemeinsamer Pool ueber alle VM-Slots)
- Funktionsname max. 23 Zeichen
- Stack 2..12 KB, Default 3 KB — fuer HTTPS / JSON / grosse Puffer auf 6+ erhoehen
- ESP8266: Alle vier Aufrufe geben -1 zurueck (nicht unterstuetzt)

---

## Tasmota-Systemvariablen

TinyC stellt virtuelle `tasm_*`-Variablen bereit, die den Tasmota-Systemzustand direkt lesen/schreiben. Sie werden wie normale Variablen verwendet — keine Funktionsaufrufe noetig. Der Compiler uebersetzt sie automatisch in Syscalls.

### Verfuegbare Variablen

| Variable | Typ | L/S | Beschreibung |
|----------|-----|-----|--------------|
| `tasm_wifi` | int | lesen | **Netzwerk aktiv** (1 = aktiv, 0 = aus). Trotz Name NICHT nur WLAN — es ist `!network_down`, also 1 sobald **WLAN *oder* Ethernet** eine Verbindung hat. Sicher zum Gaten von Boot-Netzwerkaufrufen auf reinen LAN-Geräten. |
| `tasm_net` | int | lesen | Alias von `tasm_wifi` (klarerer Name) — 1 = Netzwerk (WLAN oder Ethernet) aktiv |
| `tasm_eth` | int | lesen | 1 = Ethernet-Verbindung aktiv (hat IP); liest 0 auf reinen WLAN-/ESP8266-Builds |
| `tasm_mqttcon` | int | lesen | MQTT-Verbindungsstatus (1 = verbunden) |
| `tasm_teleperiod` | int | lesen/schreiben | Telemetrieperiode in Sekunden (10–3600, begrenzt) |
| `tasm_uptime` | int | lesen | Geraete-Betriebszeit in Sekunden |
| `tasm_heap` | int | lesen | Freier Heap-Speicher in Bytes |
| `tasm_power` | int | lesen/schreiben | Relais-Schaltzustand (Bitmaske, Schreiben schaltet Relais) |
| `tasm_dimmer` | int | lesen/schreiben | Dimmer-Pegel 0–100 (Schreiben sendet Dimmer-Befehl) |
| `tasm_temp` | float | lesen | Temperatur vom Tasmota-Sensor (globales `TempRead()`) |
| `tasm_hum` | float | lesen | Luftfeuchtigkeit vom Tasmota-Sensor (globales `HumRead()`) |
| `tasm_hour` | int | lesen | Aktuelle Stunde (0–23, von RTC) |
| `tasm_minute` | int | lesen | Aktuelle Minute (0–59, von RTC) |
| `tasm_second` | int | lesen | Aktuelle Sekunde (0–59, von RTC) |
| `tasm_year` | int | lesen | Aktuelles Jahr (z.B. 2026, von RTC) |
| `tasm_month` | int | lesen | Aktueller Monat (1–12, von RTC) |
| `tasm_day` | int | lesen | Tag des Monats (1–31, von RTC) |
| `tasm_wday` | int | lesen | Wochentag (1=So, 2=Mo, ... 7=Sa) |
| `tasm_cw` | int | lesen | ISO-Kalenderwoche (1–53) |
| `tasm_sunrise` | int | lesen | Sonnenaufgang, Minuten seit Mitternacht (erfordert USE_SUNRISE) |
| `tasm_sunset` | int | lesen | Sonnenuntergang, Minuten seit Mitternacht (erfordert USE_SUNRISE) |
| `tasm_time` | int | lesen | Aktuelle Uhrzeit, Minuten seit Mitternacht |
| `tasm_pheap` | int | lesen | Freier PSRAM-Speicher in Bytes (nur ESP32) |
| `tasm_smlj` | int | lesen/schreiben | SML-Optionsbits (erfordert USE_SML_M): 1 = auf TelePeriod veröffentlichen, 2 = obis_line_mode, 4 = eigene Web-Karte des SML-Treibers unterdrücken, 8 = jede Deskriptorzeile veröffentlichen, auch nie empfangene Register |
| `tasm_npwr` | int | lesen | Anzahl der Power-Geraete (Relais) |
| `tasm_rule` | int | lesen/schreiben | Rule1 aktiviert (Bit 0 von `Settings->rule_enabled`). Lesen liefert 0 oder 1. Schreiben: jeder Wert ungleich 0 aktiviert, 0 deaktiviert. Entspricht den Konsolen-Befehlen `Rule1 1` / `Rule1 0`. Hinweis: einige Tasmota-Subsysteme (z. B. SML-Deskriptoren) pruefen dieses Flag bei der Initialisierung und ueberspringen still wenn Rule1 deaktiviert ist — vor dem Start ggf. `tasm_rule = 1` setzen. |
| `tasm_lat` | float | lesen/schreiben | Geraete-Breitengrad in Dezimalgrad (z. B. 48.137). Backing: `Settings->latitude` (intern als int x 1 000 000 gespeichert). Wird von `tasm_sunrise` / `tasm_sunset` verwendet. |
| `tasm_lon` | float | lesen/schreiben | Geraete-Laengengrad in Dezimalgrad (z. B. 11.575). Backing: `Settings->longitude`. |
| `tasm_maxblock` | int | lesen | Groesster zusammenhaengender freier Heap-Block in Bytes (nur ESP32) — zeigt Heap-Fragmentierung: freier Heap kann hoch sein, waehrend `maxblock` niedrig ist |
| `tasm_frag` | int | lesen | Heap-Fragmentierung 0..100 % (nur ESP32) — abgeleitet aus `1 - maxblock/free_heap` |

### Indizierte Tasmota-Zustandsfunktionen

| Funktion | Beschreibung |
|----------|-------------|
| `int tasmPower(int index)` | Power-Zustand des Relais `index` (0-basiert). Gibt 0 oder 1 zurueck |
| `int tasmSwitch(int index)` | Schalter-Zustand (0-basiert, Switch1 = Index 0). Gibt -1 bei ungueltigem Index zurueck |
| `int tasmCounter(int index)` | Impulszaehler-Wert (0-basiert, Counter1 = Index 0). Erfordert USE_COUNTER |

### Tasmota String-Info

`int tasmInfo(int sel, char buf[])` — fuellt `buf` mit einem Tasmota-Info-String. Gibt Stringlaenge zurueck.

| sel | Inhalt |
|-----|--------|
| 0 | MQTT-Topic |
| 1 | MAC-Adresse |
| 2 | Lokale IP-Adresse |
| 3 | Friendly Name |
| 4 | Device Name |
| 5 | MQTT Group-Topic |
| 6 | Reset-Grund (String) |

**Beispiel:**
```c
char topic[64];
tasmInfo(0, topic);    // MQTT-Topic holen
char ip[20];
tasmInfo(2, ip);       // lokale IP holen
```

### Verwendung

```c
// Systemzustand lesen
if (tasm_wifi) {
    printStr("WiFi connected\n");
}

// Sensorwerte lesen (float)
float t = tasm_temp;
float h = tasm_hum;

// Echtzeituhr lesen
int h = tasm_hour;       // 0–23
int m = tasm_minute;     // 0–59
int s = tasm_second;     // 0–59
int y = tasm_year;       // z.B. 2026
int mo = tasm_month;     // 1–12
int d = tasm_day;        // 1–31
int wd = tasm_wday;      // 1=So..7=Sa
int cw = tasm_cw;        // ISO-Kalenderwoche 1–53

// Sonnenauf-/untergangsautomatisierung
int now = tasm_time;     // Minuten seit Mitternacht
if (now > tasm_sunset || now < tasm_sunrise) {
    tasm_power = 1;      // Nacht — Licht einschalten
}

// Systemzustand schreiben
tasm_teleperiod = 60;    // Telemetrie auf 60 Sekunden setzen
tasm_power = 1;          // Relais EIN schalten
tasm_dimmer = 50;        // Dimmer auf 50% setzen
```

### Hinweise

- **Keine Deklaration noetig** — `tasm_*`-Namen werden vom Compiler automatisch erkannt
- **Kein globaler Slot belegt** — sie verbrauchen keinen globalen Variablenspeicher
- **Nur-Lesen-Erzwingung** — Schreiben auf Nur-Lesen-Variablen (z.B. `tasm_wifi = 1`) erzeugt einen Kompilierfehler
- **Float-Typinferenz** — `tasm_temp` und `tasm_hum` sind in Ausdruecken korrekt als `float` typisiert
- **Schreib-Seiteneffekte** — `tasm_power` fuehrt den `Power`-Befehl aus, `tasm_dimmer` fuehrt den `Dimmer`-Befehl aus, `tasm_teleperiod` aktualisiert Tasmota's Einstellungen direkt
- In der Browser-IDE geben alle Variablen simulierte Werte zurueck

### Beispiel — Automatische Leistungssteuerung

```c
void EverySecond() {
    // Relais ausschalten wenn Temperatur zu hoch
    if (tasm_temp > 30.0) {
        tasm_power = 0;
    }

    // Per Web melden
    char buf[64];
    sprintf(buf, "{s}Temp{m}%.1f C{e}", tasm_temp);
    webSend(buf);
}

int main() {
    tasm_teleperiod = 30;  // schnelle Telemetrie zum Testen
    return 0;
}
```

---

## Arrays

### Deklaration & Initialisierung
```c
int data[10];                       // nicht initialisiert
int primes[5] = {2, 3, 5, 7, 11};  // mit Initialisierer
float values[3] = {1.5, 2.5};      // teilweise initialisiert
char name[32] = "TinyC";           // Zeichenketten-Initialisierung (null-terminiert)
char greeting[] = "Hello World";    // Groesse aus String abgeleitet (12)
int flags[] = {1, 0, 1, 1};        // Groesse aus Initialisierer abgeleitet (4)
```

Wenn die Groesse weggelassen wird (`[]`), leitet der Compiler sie automatisch ab:
- **String-Initialisierer:** Groesse = Stringlaenge + 1 (fuer Null-Terminator)
- **Array-Initialisierer:** Groesse = Anzahl der Elemente

### Zugriff
```c
int x = data[0];       // lesen
data[3] = 42;          // schreiben
data[i + 1] = data[i]; // berechneter Index
```

### Gueltigkeitsbereich
- **Kleine Arrays (≤16 Elemente)** — inline im globalen Datenspeicher oder lokalen Rahmen (schneller Direktzugriff)
- **Grosse Arrays (>16 Elemente)** — automatisch auf dem VM-Heap allokiert

### Array-Speicher

Arrays mit bis zu 16 Elementen werden inline im globalen oder lokalen Rahmen gespeichert fuer schnellen Direktzugriff. Arrays mit mehr als 16 Elementen werden vom Compiler automatisch auf dem VM-Heap allokiert — keine spezielle Syntax noetig:

```c
int rgb[3];            // inline (3 ≤ 16) — schneller Direktzugriff
char buf[128];         // Heap (128 > 16) — automatische Allokation
float data[2000];      // Heap (2000 > 16)

int main() {
    rgb[0] = 255;       // direkter Rahmenzugriff
    buf[0] = 'H';       // Heap-Zugriff — gleiche Syntax
    data[1999] = 3.14;  // Heap-Zugriff
    return 0;
}
```

Sowohl Inline- als auch Heap-Arrays unterstuetzen alle gleichen Operationen: Elementzugriff, Zeichenkettenoperationen auf `char[]`, Uebergabe an Funktionen usw.

**Heap-Grenzen:**

| Plattform | Max. Heap-Slots | Max. Handles |
|-----------|-----------------|--------------|
| ESP8266   | 2.048 (8 KB)    | 8            |
| ESP32     | 8.192 (32 KB)   | 16           |
| Browser   | 16.384 (64 KB)  | 32           |

### 2D-Arrays *(seit 1.3.38)*

Zweidimensionale Arrays fuer `char`, `int` und `float` arbeiten wie in
Standard-C, mit zeilenweiser flacher Speicherung im Heap:

```c
char names[7][16];           //   7 Zeilen × 16 Spalten   (char  → Heap)
int  ltab[5][4];             //   5 Zeilen ×  4 Spalten   (int   → Heap)
float coef[3][2];            //   3 Zeilen ×  2 Spalten   (float → Heap)

int main() {
    // Element-Zugriff
    ltab[2][3] = 42;
    int v = ltab[r][c];
    float k = coef[i][j];

    // String-Operationen auf Char-Zeilen
    strcpy(names[0], "Sonntag");
    strcat(names[1], " ergaenzt");
    int eq = strcmp(names[0], names[1]);

    // Zeile an eine Funktion uebergeben, die ein 1D-Array erwartet
    show_row_int(ltab[3], 4);
    show_row_str(names[i]);

    // sprintf %s mit einer 2D-Char-Zeile (konstanter oder variabler Index)
    char buf[64];
    sprintf(buf, "name=%s laenge=%d", names[i], strlen(names[i]));
    return 0;
}

void show_row_int(int row[], int n) { /* row ist die i-te Zeile des Aufrufers */ }
void show_row_str(char s[])         { addLog(s); }
```

**Speicher & Grenzen:**

- Gesamte flache Groesse = `Zeilen × Spalten`. Unterliegt denselben Heap-
  Obergrenzen wie 1D-Arrays in der obigen Tabelle. `char buf[8][32]` =
  256 Elemente (Heap).
- **Zeilenreferenzen erfordern Heap-Speicherung.** Automatische
  Heap-Promotion erfolgt bei >16 Gesamtelementen (die uebliche 1D-
  Schwelle), also qualifiziert sich praktisch jede 2D-Groesse. Wer ein
  winziges 2D wie `char buf[2][3]` (= 6 Elemente, bleibt inline)
  schreibt und `func(buf[0])` versucht, bekommt vom Compiler eine
  klare Fehlermeldung — Array vergroessern oder Zeile manuell zusammen-
  setzen.
- **`buf` (ohne Index)** an eine Funktion uebergeben, die ein Array
  dieses Typs erwartet, wird als gesamter flacher Datenblock behandelt
  (Laenge `Zeilen × Spalten`).
- **`buf[i]` (ein Index)** an eine Funktion uebergeben, die ein 1D-
  Array erwartet, ist die i-te Zeile (Laenge `Spalten`).
- **`buf[i][j]`** ist ein einzelnes Element.

**Einschraenkungen:**

- 3D und hoehere Dimensionen werden nicht unterstuetzt. Fuer die
  wenigen Faelle, die das brauchen, manuelle Stride-Arithmetik mit
  einem 2D-Array verwenden.
- Gemischte Typen-Promotion in 2D-Elementausdruecken folgt denselben
  Regeln wie 1D — int↔float-Konvertierung erfolgt automatisch.
- Initialisierungs-Literale fuer 2D werden noch nicht akzeptiert
  (`int m[2][3] = {{1,2,3},{4,5,6}};` wirft derzeit einen Fehler).
  In `main()` per Schleife oder Element-fuer-Element initialisieren.

**Intern:** die Laufzeit ist unveraendert — der Compiler flacht
`buf[i][j]` zu `buf[i*cols + j]` an den vorhandenen 1D-Heap-Array-
Opcodes ab und emittiert in Zeilenuebergabe-Kontexten eine offset-
behaftete Referenz (`ADDR_HEAP_OFF`) fuer `buf[i]`. 2D ist also rein
eine ergonomische Schicht ueber dem 1D-Heap; keine neuen VM-Features.

---

## Zeichenketten

Zeichenketten in TinyC sind `char`-Arrays mit Null-Terminierung.

### Deklaration
```c
char greeting[32] = "Hello";
char buffer[64];    // nicht initialisierter Puffer
```

### Zeichenkettenzuweisung & Verkettung mit `+`

Die Operatoren `=` und `+=` funktionieren mit `char[]`-Variablen fuer intuitive Zeichenkettenverarbeitung:

```c
char buf[64];
char name[16] = "World";

// Zeichenketten-Literal oder char-Array zuweisen
buf = "Hello";          // entspricht strcpy(buf, "Hello")
buf = name;             // entspricht strcpy(buf, name)

// Mit += anfuegen
buf += " ";             // entspricht strcat(buf, " ")
buf += name;            // entspricht strcat(buf, name)

// Mit + verketten
buf = buf + "!";        // entspricht strcat(buf, "!")
buf = buf + name;       // entspricht strcat(buf, name)
```

**Hinweis:** Der `+`-Operator funktioniert nur, wenn die linke Seite von `=` dieselbe Variable wie die linke Seite von `+` ist (d.h. `buf = buf + ...`). Variablenueberschreitende Verkettung wie `a = b + c` wird nicht unterstuetzt — verwenden Sie dafuer `strcpy` + `strcat`.

**String-Ternary:** Ein Ternary mit String-Zweigen (Literale und/oder `char[]`) ist direkt als String-Argument verwendbar — die Bedingung waehlt zur Laufzeit den Zweig (kein Kopieren):

```c
addLog("Relais %s", on ? "EIN" : "AUS");
sprintf(buf, "modus=%s", autom ? "AUTO" : name);   // Literal- oder char[]-Zweig
strcpy(dst, ok ? "ja" : "nein");
```

Frueher ergab das den Fehler *"String functions require array variable, not expression"* und erforderte eine Hilfsvariable (`char w[4]; if (on) strcpy(w,"EIN"); …`) — dieser Umweg entfaellt jetzt.

### Eingebaute Zeichenketten-Funktionen
```c
int len = strlen(greeting);             // Laenge (ohne \0)
strcpy(buffer, greeting);               // Array in Array kopieren
strcpy(buffer, "World");                // Literal in Array kopieren
strcat(buffer, greeting);               // Array anfuegen
strcat(buffer, "!");                    // Literal anfuegen
int cmp = strcmp(greeting, buffer);     // Vergleich: -1, 0 oder 1
printString(greeting);                  // Zeichenkette ausgeben
```

### Formatierte Zeichenkettenausgabe (sprintf)

`sprintf` unterstuetzt mehrere Werte in einem einzigen Aufruf. Der Compiler erkennt den Typ jedes Wertes automatisch:

```c
char buf[128];
int id = 1;
float temp = 23.5;
char name[] = "sensor";

// Mehrere Werte in einem Aufruf:
sprintf(buf, "id=%d temp=%.1f name=%s", id, temp, name);
// buf = "id=1 temp=23.5 name=sensor"

// Einzelner Wert wie bisher:
sprintf(buf, "x = %d", 42);              // "x = 42"
sprintf(buf, "pi = %.2f", 3.14);         // "pi = 3.14"
```

### Mehrteilige Zeichenketten erstellen (sprintfAppend)

Verwenden Sie `sprintfAppend`, um Werte an eine bestehende Zeichenkette anzuhaengen:

```c
char report[128];
sprintf(report, "Sensor %d", 1);               // "Sensor 1"
sprintfAppend(report, " val=%.1f", 3.14);      // "Sensor 1 val=3.1"
printString(report);
```

| Funktion | Beschreibung |
|----------|-------------|
| `sprintf(char dst[], "fmt", val, ...)` | Wert(e) in dst formatieren (ueberschreibt). Typ wird automatisch erkannt. |
| `sprintfAppend(char dst[], "fmt", val, ...)` | Wert(e) formatieren und an dst anfuegen. Typ wird automatisch erkannt. |

> **Alte Varianten:** `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr` funktionieren weiterhin.

### Variadisches addLog

`addLog` akzeptiert dieselbe `printf`-Format-Notation wie `sprintf` direkt —
fuer einmalige Log-Zeilen wird kein Scratch-Puffer mehr gebraucht:

```c
addLog("boot ok");                                  // String-Literal (am guenstigsten)
addLog("counter=%d", counter);                      // einzelner int
addLog("id=%d temp=%.1f name=%s", id, temp, name);  // mehrere Werte
```

Intern routet der Compiler den variadischen Aufruf durch dieselbe
`sprintf`-Maschinerie, formatiert in einen Stack-Puffer und ruft dann den
`AddLog`-Syscall auf `LOG_LEVEL_INFO` auf. Diese Form ist dem
`sprintf(buf, ...); addLog(buf);`-Pattern vorzuziehen wann immer die formatierte
Zeichenkette nur einmal benoetigt wird — kuerzer und ohne explizite
Puffer-Deklaration.

`addLogLevel(level, "fmt", val, ...)` ist die Variante mit waehlbarem Level
(`1=ERROR / 2=INFO / 3=DEBUG / 4=DEBUG_MORE`).

**Format-Spezifikatoren:** `%d` (int), `%f` `%.2f` `%e` `%g` (float), `%s` (Zeichenkette).

### Zeichenkettenmanipulation

```c
char src[64] = "hello,world,test";
char dst[32];

// N-tes Token (1-basiert) nach Trennzeichen extrahieren
int len = strToken(dst, src, ',', 2);  // dst = "world", len = 5

// Teilzeichenkette (0-basierte Position, Laenge)
strSub(dst, src, 6, 5);               // dst = "world"
strSub(dst, src, -4, 4);              // dst = "test" (negativ = vom Ende)

// Teilzeichenketten-Position finden (-1 wenn nicht gefunden)
int pos = strFind(src, "world");       // pos = 6
int no = strFind(src, "xyz");          // no = -1
```

| Funktion | Beschreibung |
|----------|-------------|
| `strToken(char dst[], char src[], int delim, int n)` | N-tes Token (1-basiert) durch Trennzeichen `delim` in dst kopieren. Gibt die Token-Laenge zurueck. |
| `strSub(char dst[], char src[], int pos, int len)` | `len` Zeichen ab `pos` (0-basiert, negativ=vom Ende) in dst kopieren. `len=0` kopiert bis zum Ende der Zeichenkette. Gibt die tatsaechliche Laenge zurueck. |
| `strFind(char haystack[], char needle[])` | Erstes Vorkommen von needle in haystack finden. Gibt die Position (0-basiert) oder -1 zurueck, wenn nicht gefunden. |
| `int strToInt(char str[])` | String in Integer umwandeln (wie `atoi`) |
| `float strToFloat(char str[])` | String in Float umwandeln (wie `atof`) |

### Array-Sortierung

| Funktion | Beschreibung |
|----------|-------------|
| `sortArray(int arr[], int count, int flags)` | Array sortieren. `flags`: 0=int aufsteigend, 1=float aufsteigend, 2=int absteigend, 3=float absteigend |
| `arrayFill(int arr[], int value, int count)` | Erste `count` Elemente mit `value` fuellen |
| `arrayCopy(int dst[], int src[], int count)` | `count` Elemente von `src` nach `dst` kopieren |
| `int smlCopy(int arr[], int count)` | SML-Decoderwerte in Float-Array kopieren. Gibt Anzahl zurueck (erfordert USE_SML_M) |

### Zeichenzugriff
```c
char ch = greeting[0];     // lesen: 'H'
greeting[0] = 'h';         // schreiben: jetzt "hello"
```

### Escape-Sequenzen in Zeichenketten
| Escape | Zeichen          |
|--------|------------------|
| `\n`   | Zeilenumbruch    |
| `\t`   | Tabulator        |
| `\r`   | Wagenruecklauf   |
| `\\`   | Backslash        |
| `\"`   | Anfuehrungszeichen |
| `\'`   | Einfaches Anfuehrungszeichen |
| `\0`   | Null-Terminator  |

---

## Praeprozessor

### #define — Kompilierzeit-Konstanten

Einfache Kompilierzeit-Konstanten (keine Makro-Expansion):
```c
#define LED_PIN 5
#define MAX_SIZE 100
#define PI 3.14
#define DOUBLE_PI (PI * 2)
```

**Eigenschaften:**
- Der Wert muss ein konstanter Ausdruck sein
- Unterstuetzt Arithmetik mit anderen `#define`-Werten: `+`, `-`, `*`, `/`
- Verwendet fuer Array-Groessen, Funktionsargumente usw.
- Gueltigkeitsbereich: gesamtes Programm
- Wertlose Definitionen fuer Bedingungen erlaubt: `#define ESP32`

### `#include "filename.tc"`

Fuegt eine andere `.tc`-Datei zur Compile-Zeit ein (Text-Paste vor der
Praeprozessor-Verarbeitung). Wird genutzt, um Helfer zwischen Skripten zu
teilen:

```c
// in sml_chart_pv.tc
#include "sml_chart_common.tc"      // gemeinsame Chart-Infrastruktur einziehen
#include "sml_chart_pv_common.tc"   // PV-spezifische Helfer einziehen
```

**Wie es funktioniert:**
- Die IDE laedt jede `#include`-Datei aus dem Projekt (bzw. aus dem Geraete-
  Dateisystem beim Aufruf via `/cedit`), splittet sie textuell an der
  Direktiven-Position ein und faehrt dann mit `#define` / `#ifdef` / Lexer /
  Codegen fort.
- Verschachtelte `#include`-Ketten werden rekursiv aufgeloest. Bereits
  inkludierte Dateien werden gemerkt, damit `#include`-Zyklen den Compile
  nicht aufhaengen.
- Der resultierende `.tcb`-Bytecode enthaelt alles inline — keine
  Laufzeit-Aufloesung. Umbenennen oder Loeschen einer Header-Datei nach
  dem Compile beeinflusst ein Geraet, das die fertige `.tcb` laeuft, nicht.

**Pfade:**
- `#include "foo.tc"` und `#include "/foo.tc"` funktionieren beide — das
  fuehrende `/` ist toleriert. Aufloesung ist projekt-relativ (IDE) bzw.
  geraete-FS-relativ (`/cedit`).

### Funktionsaehnliche Makros

Parametrisierte Makros fuehren Textersetzung vor der Kompilierung durch:

```c
#define LOG(A) addLog(A)
#define CLAMP(V, MX) min(max(V, 0), MX)
#define SQUARE(X) (X * X)
```

**Verwendung:**
```c
LOG("sensor init");          // -> addLog("sensor init")
int v = CLAMP(reading, 100); // -> int v = min(max(reading, 0), 100)
int s = SQUARE(5);           // -> int s = (5 * 5)
```

**Eigenschaften:**
- Parameter werden durch Ganzwort-Abgleich ersetzt (ersetzt keine Teilbezeichner)
- Verschachtelte Klammern in Argumenten werden korrekt behandelt: `LOG(foo(1,2))` funktioniert
- Zeichenketten-Literal-Argumente bleiben erhalten: `LOG("hello, world")` — das Komma innerhalb der Anfuehrungszeichen wird nicht als Argumenttrenner behandelt
- Verschachtelte Makro-Expansion: Makros im expandierten Rumpf werden expandiert (bis zu 10 Iterationen)
- Mehrere Parameter unterstuetzt: `#define ADD(A, B) (A + B)`

**Makros mit leerem Rumpf — Debug-Entfernung:**
```c
#define DBG(M)              // leerer Rumpf — kein Ersetzungstext

DBG("checkpoint 1");        // -> vollstaendig entfernt (einschliesslich Semikolon)
int x = 42;                 // diese Zeile bleibt unberuehrt
```

Makros mit leerem Rumpf entfernen den gesamten Aufruf einschliesslich des abschliessenden Semikolons. Dies ist nuetzlich zum Entfernen von Debug-Aufrufen in Produktionsversionen:

```c
#ifdef DEBUG
  #define DBG(M) addLog(M)
#else
  #define DBG(M)
#endif

DBG("init done");  // loggt im Debug, entfernt im Release
```

### Bedingte Kompilierung

```c
#define ESP32
#define USE_SENSOR

#ifdef ESP32
  int pin = 8;       // eingeschlossen — ESP32 ist definiert
#else
  int pin = 2;       // ausgeschlossen
#endif

#ifndef USE_DISPLAY
  // eingeschlossen — USE_DISPLAY ist nicht definiert
#endif
```

| Direktive               | Beschreibung                                          |
|-------------------------|-------------------------------------------------------|
| `#define NAME`          | Einen Namen definieren (ohne Wert, fuer Bedingungen)  |
| `#define NAME value`    | Einen Namen mit einem konstanten Wert definieren      |
| `#define NAME(A) body`  | Funktionsaehnliches Makro mit Textersetzung           |
| `#undef NAME`          | Einen zuvor definierten Namen aufheben                |
| `#ifdef NAME`          | Block einschliessen, wenn NAME definiert ist          |
| `#ifndef NAME`         | Block einschliessen, wenn NAME NICHT definiert ist    |
| `#if EXPR`             | Block einschliessen, wenn Ausdruck ungleich Null      |
| `#else`                | Alternativer Block                                    |
| `#endif`               | Bedingten Block beenden                               |

**`#if`-Ausdruecke** unterstuetzen:
- Ganzzahl-Literale: `#if 1`, `#if 0`
- Definierte Namen (1 wenn definiert, 0 wenn nicht): `#if ESP32`
- `defined(NAME)`-Operator: `#if defined(ESP32)`
- Logische Operatoren: `&&`, `||`, `!`
- Vergleich: `==`, `!=`, `>`, `<`, `>=`, `<=`
- Klammern zur Gruppierung

```c
#if defined(ESP32) && !defined(USE_LEGACY)
  // ESP32-spezifischer moderner Code
#endif
```

**Hinweise:**
- Bedingungen koennen verschachtelt werden
- Uebersprungener Code wird nicht kompiliert (muss keine gueltige Syntax sein)
- Zeilennummern in Fehlermeldungen bleiben erhalten

---

## Kommentare

```c
// Einzeiliger Kommentar

/* Mehrzeiliger
   Kommentar */
```

---

## Typumwandlung

### Explizite Umwandlungen
```c
float f = 3.14;
int i = (int)f;         // schneidet auf 3 ab

int x = 42;
float y = (float)x;     // konvertiert zu 42.0

int ch = 321;
char c = (char)ch;      // maskiert auf 0xFF -> 65 ('A')

int b = (bool)42;       // ungleich Null -> 1
```

### Implizite Konvertierungen
Wenn `int` und `float` in einem Ausdruck gemischt werden, wird der `int`-Operand automatisch zu `float` heraufgestuft:
```c
int a = 5;
float b = 2.5;
float c = a + b;    // a wird zu float heraufgestuft, Ergebnis = 7.5
```

---

## sizeof-Operator

`sizeof` ist ein **Uebersetzungszeit**-Operator, der zu einer Ganzzahl-Konstante aufgeloest wird. Keine Laufzeitkosten — der Compiler faltet den Wert direkt in den Bytecode.

### Formen

```c
sizeof(typ)       // int, float, char, bool, struct Tag oder Typedef-Name
sizeof(name)      // deklarierte Variable, Array oder Struct
sizeof name       // wie oben, ohne Klammern
```

### Groessen (Bytes, nach C-Konvention)

| Objekt             | sizeof |
|--------------------|--------|
| `int`, `float`     | 4      |
| `char`, `bool`     | 1      |
| `char buf[40]`     | 40     |
| `int arr[10]`      | 40     |
| `float ff[5]`      | 20     |
| `struct Foo`       | Summe der Member-Bytes |
| `struct Foo v[N]`  | N x sizeof(struct Foo) |

**Hinweis:** In TinyCs VM belegt jeder Skalar intern einen 32-Bit-Slot, aber `sizeof` liefert immer Bytes wie in Standard-C. `sizeof(char)` ist **1**, nicht 4.

### Beispiele

```c
char buf[80];
int  arr[10];

int a = sizeof(int);               // 4
int b = sizeof(buf);               // 80
int n = sizeof(arr) / sizeof(int); // 10 (Elementzahl-Idiom)

struct Frame { int id; char name[8]; float v; };
int s = sizeof(struct Frame);      // 16 (4 + 8 + 4)
```

### In konstanten Ausdruecken

`sizeof` kann in Array-Groessen verwendet werden, da es zu einer Konstante faltet:

```c
char header[8];
char packet[sizeof(header) + 32];   // packet[40]
```

### Nicht unterstuetzt

```c
sizeof(arr[0])     // FEHLER — beliebige Ausdruecke nicht erlaubt
sizeof(x + y)      // FEHLER
```

Alternative: fuer die Elementgroesse eines Arrays `sizeof(typ)` direkt nutzen, z.B. `sizeof(arr) / sizeof(int)`.

---

## Eingebaute Funktionen

### Ausgabe

| Funktion                | Beschreibung                       |
|-------------------------|-------------------------------------|
| `print(int value)`      | Ganzzahl + Zeilenumbruch ausgeben   |
| `print("literal")`     | String-Literal ausgeben (automatisch erkannt) |
| `print(char buf[])`    | Char-Array als String ausgeben (automatisch erkannt) |
| `printStr("literal")`   | Zeichenketten-Literal ausgeben (explizit) |
| `printString(char arr[])` | Null-terminiertes Char-Array ausgeben (explizit) |

> **Hinweis:** `print()` erkennt den Argumenttyp automatisch. Bei einem String-Literal wird der String ausgegeben. Bei einem `char[]`-Array wird der Inhalt als String ausgegeben. Bei einem `int` wird der numerische Wert ausgegeben. Die expliziten Funktionen `printStr`/`printString` sind weiterhin verfügbar, aber selten nötig.

### GPIO

| Funktion                             | Beschreibung                          |
|--------------------------------------|---------------------------------------|
| `pinMode(int pin, int mode)`         | Pin-Modus setzen (1=INPUT, 3=OUTPUT, 5=INPUT_PULLUP, 9=INPUT_PULLDOWN) |
| `digitalWrite(int pin, int value)`   | HIGH(1) oder LOW(0) schreiben         |
| `int digitalRead(int pin)`           | Pin-Zustand lesen                     |
| `int analogRead(int pin)`            | Analogwert lesen (0–4095)             |
| `analogWrite(int pin, int value)`    | PWM-Wert schreiben                    |
| `gpioInit(int pin, int mode)`        | Pin von Tasmota freigeben + pinMode   |
| `int pinFree(int pin)`               | Weiche Pruefung: liefert 1, wenn der Pin frei nutzbar ist (nicht von der laufenden Tasmota-Konfiguration belegt/gesperrt), sonst 0. Haelt das Programm **nicht** an — so kann ein Skript `pinMode`/`owSetPin` usw. an einem konfigurierbaren Pin absichern, statt bei veralteter Konfiguration abzustuerzen. |

### Schneller GPIO-Multiplexer (`fastMux`)

Eine IRAM-Hardware-Timer-ISR, die einen Scan-Puffer aus Pin-Mustern direkt in die
GPIO-Set/Clear-Register (Pins 0–31) schreibt — jitterfreies LED-Matrix- / 7-Segment-
/ Charlieplex-Multiplexing, weit ruhiger als das Schalten der Pins aus der VM-Schleife.
Portiert vom Scripter `ESP32_FAST_MUX`. **Per `USE_TINYC_FAST_MUX` aktiviert, standardmaessig
aus, nur Dual-Core-Xtensa** (klassischer ESP32 oder ESP32-S3; RISC-V C-Serie und ESP8266
ausgeschlossen → der Aufruf liefert `-1`).

| Aufruf | Beschreibung |
|--------|--------------|
| `int fastMux(0, period_us, buf, len)` | **Start**: die `len` in `buf[]` aufgelisteten GPIOs als Ausgaenge konfigurieren und die Scan-ISR alle `period_us` Mikrosekunden laufen lassen (1-MHz-Timer-Basis). Liefert 0 bei Erfolg, -1 wenn nicht unterstuetzt / nicht gebaut. |
| `int fastMux(1, 0, buf, 0)`           | Timer + ISR **stoppen**. |
| `int fastMux(2, 0, buf, len)`         | Scan-Sequenz **laden** (`buf[]`, `len` Schritte), die die ISR durchlaeuft, um die konfigurierten Pins zu setzen/loeschen. |
| `int fastMux(3, 0, buf, 0)`           | Aktuelle Scan-Position **lesen**. |

Den Scan-Puffer-Aufbau zeigen `examples/fast_mux.tc` und `examples/clock_7seg.tc`
(eine 7-Segment-Uhr).

### DMX-Ausgabe

DMX-512-Universum ueber einen GPIO ausgeben (nutzt das RMT-Peripheral). Kanaele
sind 1-basiert, Werte `0..255`.

| Funktion | Beschreibung |
|----------|-------------|
| `int dmxInit(int gpio)` | DMX-Ausgabe an `gpio` initialisieren. Liefert 1 bei Erfolg, 0 bei Fehler. |
| `dmxWrite(int channel, int value)` | DMX-`channel` (1..512) auf `value` (0..255) setzen. Gepuffert; wird beim laufenden DMX-Refresh gesendet. |

### Zeitsteuerung

| Funktion                         | Beschreibung                        |
|----------------------------------|-------------------------------------|
| `delay(int ms)`                  | Millisekunden warten                |
| `delayMicroseconds(int us)`      | Mikrosekunden warten                |
| `int millis()`                   | Millisekunden seit Programmstart    |
| `int micros()`                   | Mikrosekunden seit Programmstart    |

### Software-Timer

4 unabhaengige Countdown-Timer (IDs 0-3) basierend auf `millis()`. Timer laufen unabhaengig von Callbacks — setzen Sie einen Timer in `main()` oder einem beliebigen Callback, pruefen Sie ihn in `EveryLoop()`.

| Funktion                              | Beschreibung                                              |
|---------------------------------------|-----------------------------------------------------------|
| `timerStart(int id, int ms)`          | Timer `id` (0-3) mit `ms` Millisekunden Timeout starten  |
| `int timerDone(int id)`               | Gibt 1 zurueck wenn Timer abgelaufen (oder nie gestartet), 0 wenn laufend |
| `timerStop(int id)`                   | Timer abbrechen                                           |
| `int timerRemaining(int id)`          | Verbleibende Millisekunden (0 wenn abgelaufen/gestoppt)   |

**Beispiel — Wiederholender Timer mit Timeout:**
```c
int counter;

void main() {
    counter = 0;
    timerStart(0, 5000);    // Timer 0: alle 5 Sekunden
    timerStart(1, 60000);   // Timer 1: nach 1 Minute stoppen
}

void EveryLoop() {
    if (timerDone(0)) {
        counter++;
        print(counter);
        timerStart(0, 5000);  // fuer naechstes Intervall neu starten
    }
    if (timerDone(1)) {
        timerStop(0);         // wiederholenden Timer stoppen
    }
}
```

### Seriell

Es koennen bis zu 3 serielle Ports gleichzeitig geoeffnet sein. `serialBegin()` gibt einen **Handle** (0–2) zurueck, der an alle anderen seriellen Funktionen uebergeben werden muss. Bei Fehler wird -1 zurueckgegeben.

| Funktion                                          | Beschreibung                                              |
|---------------------------------------------------|-----------------------------------------------------------|
| `int serialBegin(int rx, int tx, int baud, int config, int bufsize)` | Seriellen Port oeffnen, gibt Handle (0–2) oder -1 bei Fehler |
| `serialPrint(int h, "literal")`                   | Zeichenkette auf Port `h` ausgeben                        |
| `serialPrintInt(int h, int value)`                | Ganzzahl auf Port `h` ausgeben                            |
| `serialPrintFloat(int h, float value)`            | Gleitkommazahl auf Port `h` ausgeben                      |
| `serialPrintln(int h, "literal")`                 | Zeichenkette + Zeilenumbruch auf Port `h`                 |
| `int serialRead(int h)`                           | Byte von Port `h` lesen (-1 wenn keines verfuegbar)       |
| `int serialAvailable(int h)`                      | Verfuegbare Bytes auf Port `h`                            |
| `serialClose(int h)`                              | Port `h` schliessen                                       |
| `serialWriteByte(int h, int b)`                   | Einzelnes Byte an Port `h` senden                         |
| `serialWrite(int h, char str[])`                  | Char-Array an Port `h` senden (binaer-sicher)             |
| `serialWriteBytes(int h, char buf[], int len)`    | `len` Bytes aus Buffer an Port `h` senden                 |

**`serialBegin` Parameter:**
- `rx` — GPIO-Pin fuer Empfang (-1 zum Deaktivieren, z.B. nur-TX Geraete)
- `tx` — GPIO-Pin fuer Senden (-1 zum Deaktivieren, z.B. nur-RX Geraete)
- `baud` — Baudrate (z.B. 9600, 115200)
- `config` — Serielles Frame-Format (siehe Tabelle), Standard 3 = 8N1
- `bufsize` — Empfangspuffer-Groesse in Bytes (64–2048)

**Serielle Konfigurations-Werte:**

| Wert | Format | Wert | Format | Wert | Format |
|------|--------|------|--------|------|--------|
| 0    | 5N1    | 8    | 5E1    | 16   | 5O1    |
| 1    | 6N1    | 9    | 6E1    | 17   | 6O1    |
| 2    | 7N1    | 10   | 7E1    | 18   | 7O1    |
| **3**| **8N1**| 11   | 8E1    | 19   | 8O1    |
| 4    | 5N2    | 12   | 5E2    | 20   | 5O2    |
| 5    | 6N2    | 13   | 6E2    | 21   | 6O2    |
| 6    | 7N2    | 14   | 7E2    | 22   | 7O2    |
| 7    | 8N2    | 15   | 8E2    | 23   | 8O2    |

**Beispiel — ein Port:**
```c
// LD2410 Radar: RX=Pin 16, TX=Pin 17, 256000 Baud, 8N1, 256 Byte Puffer
int ser = serialBegin(16, 17, 256000, 3, 256);
if (ser < 0) { addLog("Seriell Fehler"); }

// Nur-TX fuer MP3-Modul: kein RX, TX=Pin 4, 9600 Baud
int mp3 = serialBegin(-1, 4, 9600, 3, 64);
serialWriteByte(mp3, 0x7E);
```

**Beispiel — zwei Ports gleichzeitig:**
```c
int radar = serialBegin(16, 17, 256000, 3, 256);  // Handle 0
int gps   = serialBegin(18, 19,   9600, 3, 256);  // Handle 1

void EverySecond() {
  while (serialAvailable(gps) > 0) {
    int b = serialRead(gps);
    // GPS-Byte verarbeiten...
  }
}
```

### 1-Wire

| Funktion                          | Beschreibung                                               |
|-----------------------------------|------------------------------------------------------------|
| `owSetPin(int pin)`               | GPIO-Pin fuer nativen 1-Wire-Bus setzen                    |
| `int owReset()`                   | Reset-Puls senden, 1 bei Praesenz-Erkennung                |
| `owWrite(int byte)`               | Ein Byte auf den Bus schreiben                             |
| `int owRead()`                    | Ein Byte vom Bus lesen                                     |
| `owWriteBit(int bit)`             | Ein einzelnes Bit schreiben (0 oder 1)                     |
| `int owReadBit()`                 | Ein einzelnes Bit lesen                                    |
| `owSearchReset()`                 | ROM-Suchstatus zuruecksetzen                               |
| `int owSearch(char rom[])`        | Naechstes Geraet finden, 8-Byte-ROM in `rom[]` speichern, 1 bei Erfolg |

> Die nativen 1-Wire-Funktionen verwenden hardware-getimtes Bit-Banging in C — keine externe Bibliothek noetig. Ein 4,7 kΩ Pull-up-Widerstand auf der Datenleitung ist erforderlich. Fuer lange Busse oder stoeranfaellige Umgebungen kann eine DS2480B Seriell-zu-1-Wire-Bruecke verwendet werden (siehe `examples/onewire.tc`).

### Mathematik

| Funktion                                            | Beschreibung                     |
|-----------------------------------------------------|----------------------------------|
| `int abs(int value)`                                | Absolutwert                      |
| `int min(int a, int b)`                             | Minimum zweier Werte             |
| `int max(int a, int b)`                             | Maximum zweier Werte             |
| `int map(int val, int fLo, int fHi, int tLo, int tHi)` | Wert von einem Bereich auf einen anderen abbilden |
| `int random(int min, int max)`                      | Zufaellige Ganzzahl im Bereich   |
| `float sqrt(float x)`                               | Quadratwurzel                    |
| `float sin(float x)`                                | Sinus (Bogenmass)                |
| `float cos(float x)`                                | Kosinus (Bogenmass)              |
| `float exp(float x)`                                | Exponentialfunktion (e^x)        |
| `float log(float x)`                                | Natürlicher Logarithmus (ln x)  |
| `float pow(float basis, float exp)`                  | Potenz (basis^exp)               |
| `float acos(float x)`                               | Arkuskosinus (Bogenmass)         |
| `float intBitsToFloat(int bits)`                     | Int als IEEE 754 Float interpretieren |
| `int floor(float x)`                                | Ganzzahlanteil (Richtung −∞)     |
| `int ceil(float x)`                                 | Ganzzahlanteil + 1 (Richtung +∞) |
| `int round(float x)`                                | Auf naechste Ganzzahl runden     |

### Zeichenketten

| Funktion                             | Beschreibung                         |
|--------------------------------------|--------------------------------------|
| `int strlen(char arr[])`             | Zeichenkettenlaenge (ohne Null)      |
| `strcpy(char dst[], char src[])`     | Zeichenkette kopieren                |
| `strcpy(char dst[], "literal")`      | Literal in Array kopieren            |
| `strcat(char dst[], char src[])`     | Zeichenkette verketten               |
| `strcat(char dst[], "literal")`      | Literal verketten                    |
| `int strcmp(char a[], char b[])`     | Vergleich: gibt -1, 0 oder 1 zurueck |
| `printString(char arr[])`            | Zeichenkette ausgeben                |

**Zeichenketten-Operatoren:** `char[]`-Variablen unterstuetzen auch `=`, `+=` und `+` fuer Zeichenkettenzuweisung und -verkettung — siehe Abschnitt [Zeichenketten](#zeichenketten).

### sprintf — Formatierte Zeichenketten

Einen oder mehrere Werte in einem einzigen Aufruf in ein char-Array formatieren. Der Compiler erkennt den Typ jedes Wertes automatisch und expandiert mehrere Argumente zur Compilezeit.

| Funktion | Beschreibung |
|----------|-------------|
| `int sprintf(char dst[], "fmt", val, ...)` | Wert(e) in dst formatieren (ueberschreibt). Typ automatisch erkannt. |
| `int sprintfAppend(char dst[], "fmt", val, ...)` | Wert(e) formatieren, an Ende von dst anfuegen. Typ automatisch erkannt. |

> **Alte Varianten:** `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr` funktionieren weiterhin.

**Format-Spezifikatoren:** `%d` `%i` `%x` (int), `%f` `%.Nf` `%e` `%g` (float), `%s` (Zeichenkette).
Alle Funktionen geben die Gesamtlaenge der Zeichenkette zurueck.

```c
char buf[128];
char name[] = "sensor";
int id = 1;
float temp = 23.5;

// Mehrere Werte in einem Aufruf:
sprintf(buf, "id=%d temp=%.1f name=%s", id, temp, name);
// buf = "id=1 temp=23.5 name=sensor"

// sprintfAppend haengt an bestehenden Inhalt an:
sprintf(buf, "ID=%d", id);
sprintfAppend(buf, " val=%.1f", temp);
// buf = "ID=1 val=23.5"
```

### Datei-E/A

Dateien auf dem ESP32-Dateisystem (LittleFS) lesen und schreiben. In der Browser-IDE werden Dateien in einem virtuellen Dateisystem simuliert.

| Funktion                                   | Beschreibung                                          |
|--------------------------------------------|-------------------------------------------------------|
| `int fileOpen("path", mode)`               | Datei oeffnen, gibt Handle (0–3) oder -1 bei Fehler zurueck |
| `int fileClose(handle)`                    | Datei-Handle schliessen, gibt 0 oder -1 zurueck      |
| `int fileRead(handle, char buf[], max)`    | Bis zu max Bytes in buf lesen, gibt Anzahl zurueck    |
| `int fileWrite(handle, char buf[], len)`   | len Bytes aus buf schreiben, gibt Anzahl zurueck      |
| `int fileRename(von, nach)`                | Datei umbenennen/verschieben. `0` = ok, `-1` = Fehler. Schlaegt fehl (und aendert nichts), wenn die Quelle fehlt, das **Ziel schon existiert** (wird nie stillschweigend ueberschrieben — ein Tippfehler im Zielnamen darf keine Daten vernichten) oder Quelle und Ziel auf verschiedenen Dateisystemen liegen (`/ffs/` vs. `/sdfs/` — `rename` kann nicht dazwischen kopieren). Beide Pfade duerfen Zeichenketten oder `char[]`-Puffer sein. Praktisch fuer „schon erledigt"-Vermerke: eine Eingabedatei nach `<name>.done` umbenennen, dann ist ihr blosses Vorhandensein die Markierung — ohne zusaetzliche Zustandsdatei. |
| `int fileExists("path")`                   | Pruefen ob Datei existiert: 1=ja, 0=nein             |
| `int fileDelete("path")`                   | Datei loeschen, gibt 0=ok, -1=Fehler zurueck         |
| `int fileSize("path")`                     | Dateigroesse in Bytes, -1 bei Fehler                  |
| `int fileSeek(handle, offset, whence)`     | Zur Position springen. Gibt 1=ok, 0=Fehler zurueck   |
| `int fileTell(handle)`                     | Aktuelle Position in Datei, -1 bei Fehler             |
| `int fsInfo(int sel)`                      | Dateisystem-Info: sel=0 → Gesamtgroesse KB, sel=1 → frei KB |
| `int fileOpenDir("path")`                  | Verzeichnis zum Auflisten oeffnen, gibt Handle oder -1 zurueck |
| `int fileReadDir(handle, char name[])`     | Naechsten Dateinamen in name lesen. Gibt 1=Eintrag, 0=Ende zurueck |

**Dateimodi:** `0` = Lesen, `1` = Schreiben (Erstellen/Abschneiden), `2` = Anfuegen

**Seek-Modus (whence):** `0` = SEEK_SET (vom Anfang), `1` = SEEK_CUR (von aktueller Position), `2` = SEEK_END (vom Ende)

**Hinweise:**
- Dateipfade können Zeichenketten-Literale oder char[]-Variablen sein (z.B. `"/data.txt"`)
- **Dateisystem-Auswahl** (Scripter-kompatibel): Standard ist SD-Karte (`ufsp`). Praefix `/ffs/` fuer Flash, `/sdfs/` fuer SD-Karte explizit: `fileOpen("/ffs/config.txt", 0)` oeffnet von Flash, `fileOpen("/data.txt", 0)` oeffnet von SD-Karte
- Maximal 4 gleichzeitig geoeffnete Dateien (ESP32), 8 im Browser
- Puffer-Argumente (`buf`) muessen `char`-Arrays sein, keine Zeichenketten-Literale
- `fileRead` gibt die tatsaechlich gelesene Byteanzahl zurueck (kann weniger als `max` sein)
- Schliessen Sie Dateien immer, wenn Sie fertig sind, um Handles freizugeben

```c
// Beispiel: Schreiben und zuruecklesen
char data[32];
char buf[32];
strcpy(data, "Hello!\n");

int f = fileOpen("/test.txt", 1);   // Schreibmodus
fileWrite(f, data, strlen(data));
fileClose(f);

f = fileOpen("/test.txt", 0);       // Lesemodus
int n = fileRead(f, buf, 31);
buf[n] = 0;
fileClose(f);
printString(buf);                    // gibt "Hello!" aus

fileDelete("/test.txt");             // aufraeumen

// Beispiel: Dateien in einem Verzeichnis auflisten
char fname[64];
int dir = fileOpenDir("/images");
if (dir >= 0) {
    while (fileReadDir(dir, fname)) {
        printString(fname);
        print("\n");
    }
    fileClose(dir);
}
```

**Hinweise zur Verzeichnisauflistung:**
- `fileOpenDir` belegt einen Datei-Handle-Slot (gleicher Pool wie `fileOpen`), mit `fileClose` schliessen
- `fileReadDir` gibt nur Dateinamen zurueck (kein Pfad-Praefix), ueberspringt Unterverzeichnisse
- Pfad-Argument kann ein String-Literal oder eine char-Array-Variable sein

### Erweiterte Dateioperationen

Dateisystemverwaltung, strukturierte Array-Ein-/Ausgabe und Logdatei-Rotation.

| Funktion | Beschreibung |
|----------|--------------|
| `int fileFormat()` | LittleFS-Dateisystem formatieren (loescht alle Daten). Gibt 0=ok zurueck |
| `int fileMkdir("pfad")` | Verzeichnis erstellen. Gibt 1=ok, 0=Fehler zurueck |
| `int fileRmdir("pfad")` | Verzeichnis entfernen. Gibt 1=ok, 0=Fehler zurueck |
| `int fileReadArray(float arr[], handle [, count])` | Tab/Komma-getrennte **Float**-Werte in Array lesen (gestreamt, beliebige Groesse). `count` begrenzt die Anzahl (Standard: Array-Kapazitaet, stoppt bei EOF). Gibt gelesene Elemente zurueck |
| `fileWriteArray(float arr[], handle, count)` | `count` **Float**-Werte als Tab-getrennten Text (Standard 2 Nachkommastellen) + Zeilenumbruch schreiben. `count` explizit (wie `fileWriteBin`), damit kleine globale Arrays die richtige Laenge schreiben |
| `fileWriteArray(float arr[], handle, count, append)` | append=1 haelt die Zeile offen (Tab am Ende), um mehrere Arrays in einer Zeile abzulegen |
| `fileWriteArray(float arr[], handle, count, append, decimals)` | `decimals` = max. Nachkommastellen pro Wert, nachgestellte Nullen entfernt (0–7). Kleiner = kleinere Datei |
| `int fileLog("datei", char str[], limit)` | String + Zeilenumbruch an Datei anhaengen. Erste Zeile entfernen wenn Datei `limit` Bytes ueberschreitet. Gibt Dateigroesse zurueck |
| `int fileDownload("datei", char url[])` | URL-Inhalt in Datei herunterladen. Gibt HTTP-Statuscode zurueck (200=ok). Kompatibel mit Scripters `frw()` |
| `int fileGetStr(char dst[], handle, "delim", index, endChar)` | Datei von Anfang nach N-tem Vorkommen des Trennzeichens durchsuchen, String bis endChar extrahieren. Gibt Stringlaenge zurueck. Kompatibel mit Scripters `fcs()` |

**fileReadArray / fileWriteArray Format:** Werte werden als menschenlesbarer **Float**-Text durch TAB-Zeichen getrennt gespeichert, ein Array pro Zeile — kompatibel mit Scripters `fra()`/`fwa()`. Die Datei ist eine editierbare `.csv`/`.tab`. `count` wird explizit angegeben (wie `fileWriteBin`/`fileReadBin`), da globale TinyC-Arrays (≤64 Elemente) ihre deklarierte Groesse nicht mitfuehren — die echte Elementanzahl uebergeben, damit kleine Arrays die richtige Laenge lesen/schreiben. Das optionale Argument `decimals` (Standard 2) begrenzt die Nachkommastellen pro Wert (nachgestellte Nullen werden entfernt, wie Scripters Zahlenpraezision) und haelt die Datei kompakt — wichtig bei grossen Arrays, da kleine/gebrochene Werte sonst lange Zeichenketten erzeugen koennen (z. B. `0.0001234567`). Das Lesen erfolgt wertweise (gestreamt), daher funktionieren auch grosse Arrays (z. B. ein 1441-Slot-Chart-Puffer) ohne Zeilenlaengen-Limit. Es gibt keine Integer-Variante (fuer kompakte Binaerspeicherung `fileWriteBin`/`fileReadBin` verwenden).

```c
// Beispiel: Float-Array-Daten speichern und laden (menschenlesbar)
float values[5];
values[0] = 1.5; values[1] = 22.7; values[2] = 300.0;
values[3] = 4.25; values[4] = 500.5;

int f = fileOpen("/data.tab", 1);     // Schreibmodus
fileWriteArray(values, f, 5);         // schreibt "1.5\t22.7\t300\t4.25\t500.5\n"
fileClose(f);

float loaded[5];
f = fileOpen("/data.tab", 0);        // Lesemodus
int n = fileReadArray(loaded, f, 5); // n = 5
fileClose(f);
```

```c
// Beispiel: Rotierende Logdatei (max 4096 Bytes)
char msg[64];
strcpy(msg, "Sensorwert: 23.5C");
fileLog("/log.txt", msg, 4096);
// Haengt Zeile an, entfernt aelteste Zeile wenn Datei > 4096 Bytes
```

```c
// Beispiel: Datei aus dem Web herunterladen
char url[128];
strcpy(url, "http://192.168.1.100/data.csv");
int status = fileDownload("/data.csv", url);
// status = 200 bei Erfolg, negativ bei Fehler
```

```c
// Beispiel: 2. komma-getrenntes Feld aus CSV-Datei extrahieren
// Dateiinhalt: "name,temperature,humidity\nSensor1,23.5,65\n"
int f = fileOpen("/data.csv", 0);       // zum Lesen oeffnen
char value[32];
int len = fileGetStr(value, f, ",", 2, '\n');
// value = "23.5", len = 4 (Inhalt zwischen 2. Komma und Zeilenumbruch)
fileClose(f);
```

### Datei-Datenextraktion (IoT-Zeitreihen)

Einen Zeitbereich aus Tab-getrennten CSV-Datendateien in Float-Arrays extrahieren fuer die Analyse. Entwickelt fuer IoT-Datensammler die Sensorwerte in regelmaessigen Intervallen protokollieren.

**Dateiformat:** Erste Spalte ist ein Zeitstempel (ISO oder deutsches Format), gefolgt von Tab-getrennten Float-Werten. Erste Zeile kann eine Kopfzeile sein (wird automatisch uebersprungen).

| Funktion | Beschreibung |
|----------|--------------|
| `int fileExtract(handle, char from[], char to[], col_offs, accum, int arr1[], ...)` | Zeilen extrahieren wo `from <= Zeitstempel <= to`. Sucht immer vom Dateianfang. Gibt Zeilenanzahl zurueck |
| `int fileExtractFast(handle, char from[], char to[], col_offs, accum, int arr1[], ...)` | Wie oben, merkt sich Dateiposition fuer effiziente sequenzielle Zeitbereichsabfragen |

**Parameter:**
- `handle` — offener Datei-Handle (von `fileOpen`)
- `from`, `to` — Zeitbereich als char[] (ISO `2024-01-15T12:00:00` oder Deutsch `15.1.24 12:00`)
- `col_offs` — so viele Datenspalten ueberspringen bevor Arrays gefuellt werden (0 = ab erster Datenspalte)
- `accum` — 0: Werte speichern, 1: zu bestehenden Array-Werten addieren (zum Kombinieren mehrerer Extraktionen)
- `arr1, arr2, ...` — variable Anzahl Int-Arrays, eines pro zu extrahierender Spalte (max. 16). Werte werden als IEEE 754 Float-Bitmuster gespeichert — Float-Variablen oder Casts zum Lesen verwenden

```c
// Beispiel: Temperatur und Luftfeuchtigkeit fuer einen Tag extrahieren
int temp[96], hum[96];  // 96 = 24h * 4 (15-Min-Intervalle)
char from[24], to[24];
strcpy(from, "15.12.21 00:00");
strcpy(to, "16.12.21 00:00");

int f = fileOpen("/daily.csv", 0);
// col_offs=4 ueberspringt WB,WR1,WR2,WR3 → startet bei ATMP_a (5. Datenspalte)
int rows = fileExtract(f, from, to, 4, 0, temp, hum);
fileClose(f);
// rows = Anzahl 15-Min-Abtastungen, temp[] und hum[] mit Floats gefuellt
```

```c
// Beispiel: Sequenzielle Tagesabfragen mit fileExtractFast
int energy[96];
char from[24], to[24];
int f = fileOpen("/yearly.csv", 0);

strcpy(from, "1.1.24 00:00");
strcpy(to, "2.1.24 00:00");
int r1 = fileExtractFast(f, from, to, 0, 0, energy);
// Naechster Tag — fileExtractFast ueberspringt bereits gescannte Daten
strcpy(from, "2.1.24 00:00");
strcpy(to, "3.1.24 00:00");
int r2 = fileExtractFast(f, from, to, 0, 0, energy);
fileClose(f);
```

### Zeit- / Zeitstempel-Funktionen

Zeitstempel-Konvertierung und -Arithmetik. Unterstuetzt ISO-Webformat (`2024-01-15T12:30:45`) und deutsches Gebietsformat (`15.1.24 12:30`). Kompatibel mit Scripters `tstamp`, `cts`, `tso`, `tsn`, `s2t`.

| Funktion | Beschreibung |
|----------|--------------|
| `int timeStamp(char buf[])` | Aktuellen Tasmota-Zeitstempel in buf schreiben. Gibt 0 zurueck |
| `int timeConvert(char buf[], flg)` | Zeitstempel-Format in-place konvertieren. 0=Deutsch→Web, 1=Web→Deutsch. Gibt 0 zurueck |
| `int timeOffset(char buf[], days)` | `days` Tage zum Zeitstempel in buf addieren (in-place). Gibt 0 zurueck |
| `int timeOffset(char buf[], days, zeroFlag)` | Mit `zeroFlag`=1: zusaetzlich Uhrzeit auf Null setzen (HH:MM:SS→00:00:00) |
| `int timeToSecs(char buf[])` | Zeitstempel-String in Epochensekunden umwandeln. Gibt Sekunden zurueck |
| `int utcSecs()` | Aktuelle **UTC**-Unix-Epoche (echtes UTC, anders als `timeToSecs(timeStamp())` = lokal-als-UTC). Fuer Request-Signierung / API-Stamps |
| `int secsToTime(char buf[], secs)` | Epochensekunden in ISO-Zeitstempel-String in buf umwandeln. Gibt 0 zurueck |

**Format-Erkennung:** `timeConvert` und `timeOffset` erkennen das Eingabeformat automatisch (ISO wenn `T` enthalten, sonst Deutsch) und konvertieren entsprechend.

```c
// Beispiel: Aktuelle Zeit holen und Formate konvertieren
char ts[24];
timeStamp(ts);               // ts = "2024-06-15T14:30:00"

char de[24];
strcpy(de, ts);
timeConvert(de, 1);          // de = "15.6.24 14:30"

timeConvert(de, 0);          // de = "2024-06-15T14:30:00" (zurueck zu Web)
```

```c
// Beispiel: Datumsarithmetik
char ts[24];
timeStamp(ts);               // "2024-06-15T14:30:00"
timeOffset(ts, 7);           // "2024-06-22T14:30:00" (+ 7 Tage)
timeOffset(ts, -3, 1);       // "2024-06-19T00:00:00" (- 3 Tage, Zeit nullen)
```

```c
// Beispiel: In Sekunden umwandeln und zurueck
char ts[24];
timeStamp(ts);
int secs = timeToSecs(ts);   // Epochensekunden

secs = secs + 3600;          // 1 Stunde addieren
secsToTime(ts, secs);        // zurueck zu Zeitstempel-String
```

### Tasmota-Befehl

Einen beliebigen Tasmota-Konsolenbefehl ausfuehren und die JSON-Antwort erfassen.

| Funktion                                     | Beschreibung                                          |
|----------------------------------------------|-------------------------------------------------------|
| `int tasmCmd("command", char response[])`    | Befehl ausfuehren, Antwort speichern, Laenge zurueckgeben |
| `int tasmCmd(char cmd[], char response[])`   | Befehl ausfuehren (char-Array), Antwort speichern |

**Hinweise:**
- Befehl kann ein String-Literal oder ein char[]-Array sein
- Der Antwortpuffer sollte ein `char`-Array sein (empfohlene Groesse: 256)
- Gibt die Laenge der Antwortzeichenkette zurueck, oder -1 bei Fehler
- In der Browser-IDE wird eine simulierte Scheinantwort zurueckgegeben
- Auf dem ESP32 werden echte Tasmota-Befehle ausgefuehrt und die JSON-Antwort erfasst

```c
char resp[256];
int len = tasmCmd("Status 0", resp);
if (len > 0) {
    printString(resp);   // gibt JSON-Antwort aus
}
```

### Sensor-JSON-Parsing

Einen beliebigen Tasmota-Sensorwert ueber seinen JSON-Pfad auslesen. Pfadsegmente werden durch `#` getrennt (gleiche Konvention wie Tasmota Scripter).

| Funktion | Beschreibung |
|----------|-------------|
| `float sensorGet("Sensor#Key")` | Sensorwert lesen, gibt float zurueck |

Die Funktion loest intern eine Sensor-Statusabfrage aus und navigiert durch den JSON-Baum. Unterstuetzt bis zu 3 Verschachtelungsebenen.

```c
// BME280-Sensor lesen
float temp = sensorGet("BME280#Temperature");
float hum = sensorGet("BME280#Humidity");
float press = sensorGet("BME280#Pressure");

// SHT3X an Adresse 0x44 lesen
float t = sensorGet("SHT3X_0x44#Temperature");

// Energiezaehler lesen (wenn USE_ENERGY_SENSOR definiert)
float power = sensorGet("ENERGY#Power");
float voltage = sensorGet("ENERGY#Voltage");
float today = sensorGet("ENERGY#Today");

// Verschachtelt: Zigbee-Geraet
float zt = sensorGet("ZbReceived#0x2342#Temperature");
```

**Hinweise:**
- Der Pfad muss ein Zeichenketten-Literal sein (wird zur Kompilierzeit aufgeloest)
- Gibt 0.0 zurueck, wenn der Sensor oder Schluessel nicht gefunden wird
- Gibt einen Float zurueck — weisen Sie ihn einer `float`-Variable zu
- In der Browser-IDE werden Temperature=22.5, Humidity=55.0, Pressure=1013.25 simuliert

### Lokalisierte Zeichenketten

Lokalisierte Anzeigetexte von Tasmota zur Laufzeit abrufen. Die Texte entsprechen der Spracheinstellung der Firmware (z.B. `en_GB.h`, `de_DE.h`). Fuer Web-UI-Beschriftungen verwenden; JSON-Schluessel bleiben auf Englisch.

| Funktion | Beschreibung |
|----------|-------------|
| `int LGetString(int index, char dst[])` | Lokalisierten Text nach `dst` kopieren, gibt Laenge zurueck (0 bei ungueltigem Index) |

**Index-Tabelle:**

| Index | Tasmota-Define | Englisch | Deutsch |
|-------|---------------|----------|---------|
| 0 | D_TEMPERATURE | Temperature | Temperatur |
| 1 | D_HUMIDITY | Humidity | Feuchtigkeit |
| 2 | D_PRESSURE | Pressure | Luftdruck |
| 3 | D_DEWPOINT | Dew point | Taupunkt |
| 4 | D_CO2 | Carbon dioxide | Kohlendioxid |
| 5 | D_ECO2 | eCO2 | eCO2 |
| 6 | D_TVOC | TVOC | TVOC |
| 7 | D_VOLTAGE | Voltage | Spannung |
| 8 | D_CURRENT | Current | Strom |
| 9 | D_POWERUSAGE | Power | Leistung |
| 10 | D_POWER_FACTOR | Power Factor | Leistungsfaktor |
| 11 | D_ENERGY_TODAY | Energy Today | Energie Heute |
| 12 | D_ENERGY_YESTERDAY | Energy Yesterday | Energie Gestern |
| 13 | D_ENERGY_TOTAL | Energy Total | Energie Gesamt |
| 14 | D_FREQUENCY | Frequency | Frequenz |
| 15 | D_ILLUMINANCE | Illuminance | Beleuchtungsstaerke |
| 16 | D_DISTANCE | Distance | Entfernung |
| 17 | D_MOISTURE | Moisture | Feuchtigkeit |
| 18 | D_LIGHT | Light | Licht |
| 19 | D_SPEED | Speed | Geschwindigkeit |
| 20 | D_ABSOLUTE_HUMIDITY | Abs Humidity | Abs Feuchtigkeit |

**Beispiel:**
```c
char lbl[32];
char buf[80];

void web_row(int idx, float val, char unit[]) {
    LGetString(idx, lbl);
    strcpy(buf, "{s}");
    strcat(buf, lbl);
    strcat(buf, "{m}");
    webSend(buf);
    sprintf(buf, "%.1f ", val);
    strcat(buf, unit);
    strcat(buf, "{e}");
    webSend(buf);
}

void WebCall() {
    web_row(0, temperature, "&deg;C");  // "Temperatur" (bei de_DE)
    web_row(1, humidity, "%");           // "Feuchtigkeit"
    web_row(2, pressure, "hPa");         // "Luftdruck"
}
```

### Tasmota-Ausgabe (Callbacks)

Daten direkt an Tasmota's Telemetrie- und Websysteme aus Callback-Funktionen senden.

| Funktion | Beschreibung |
|----------|-------------|
| `void responseAppend(char buf[])` | Zeichenkette an MQTT-JSON-Telemetrie anfuegen (`ResponseAppend_P`) |
| `void responseAppend("literal")` | Zeichenketten-Literal an JSON anfuegen (kein Puffer noetig) |
| `void webSend(char buf[])` | Zeichenkette an Webseiten-HTML senden (`WSContentSend`) |
| `void webSend("literal")` | Zeichenketten-Literal an Webseite senden (kein Puffer noetig) |
| `void webFlush()` | Web-Inhaltspuffer zum Client leeren (`WSContentFlush`) |
| `void addLog(char buf[])` | Nachricht ins Tasmota-Log schreiben (`AddLog` auf INFO-Ebene) |
| `void addLog("literal")` | Zeichenketten-Literal ins Tasmota-Log schreiben |
| `webSendJsonArray(float arr[], int count)` | Float-Array als JSON-Integer-Array ausgeben |

**Hinweise:**
- `addLog`, `webSend` und `responseAppend` akzeptieren sowohl ein char-Array als auch ein Zeichenketten-Literal
- Zeichenketten-Literal-Varianten sind effizienter — keine Kopie durch einen Puffer, direkt aus dem Konstantenpool gesendet
- Verwenden Sie `responseAppend()` innerhalb von `JsonCall()` — fuegt an das MQTT-Telemetrie-JSON an
- Verwenden Sie `webSend()` innerhalb von `WebPage()` fuer einmaligen Seiteninhalt (Diagramme, Skripte, benutzerdefiniertes HTML)
- Verwenden Sie `webSend()` innerhalb von `WebCall()` fuer Sensor-aehnliche Zeilen, die periodisch aktualisiert werden
- Verwenden Sie das Format `{s}Beschriftung{m}Wert{e}` in `webSend()` fuer Sensor-aehnliche Tabellenzeilen
- Rufen Sie `webFlush()` periodisch auf, wenn Sie grosse HTML-Seiten erstellen, um den Chunked-Transfer-Puffer zu leeren (500 Bytes)
- Beginnen Sie JSON mit Komma: `",\"Key\":value"` um korrekt an die Telemetrie anzufuegen
- In der Browser-IDE werden beide zur Ausgabekonsole geleitet; `webFlush()` ist eine Leeroperation
- Callback-Instruktionslimit: 200.000 (ESP32), 20.000 (ESP8266)
- Siehe [Callback-Funktionen](#callback-funktionen) fuer vollstaendige Beispiele

### HTTP-Anfragen

HTTP GET/POST-Anfragen an externe APIs stellen. URLs koennen Zeichenketten-Literale oder dynamisch in char-Arrays erstellt sein. Anfragen sind blockierend mit einem 5-Sekunden-Timeout.

> **⚠ Wiederholte/blockierende Netzwerkaufrufe aus einem `spawnTask`-Worker ausfuehren — nicht aus `EverySecond`/`Every50ms`/Befehls-Callbacks.**
> `httpGet`/`httpPost`/`sendMail` blockieren fuer die gesamte Anfrage (Verbindung + Uebertragung — bis zu mehreren Sekunden bei einem langsamen oder **toten** Host). Auf dem **Main-Loop-Task** (der `EverySecond`, `Every50ms` und Befehls-Callbacks ausfuehrt) haelt die Firmware den VM-Mutex des Slots die ganze Zeit, was alle anderen Slots und die Tasmota-Weboberflaeche blockiert, bis der Aufruf zurueckkehrt. Er wird dort **bewusst** gehalten: gibt man den Mutex auf dem Main-Loop frei und nimmt ihn neu, kann ein konkurrierender Task den Main-Loop dauerhaft verkeilen — Web/LwIP tot, nur ein Spannungsreset hilft (die `4f947fb8d`-Regression). Fuer periodisches Polling die Anfrage daher in einem **Worker-Task** ausfuehren, wo der Mutex um die blockierende E/A herum **freigegeben** wird → kein Stall *und* kein Wedge. Der Worker schreibt Ergebnisse in Globals (oder ein `shareSet*`), die deine Callbacks lesen.
>
> ```c
> char url[96]; char resp[256]; float temp = 0;
>
> void poller() {                         // laeuft in seinem EIGENEN FreeRTOS-Task
>     while (taskRunning("poller")) {
>         strcpy(url, "http://192.168.1.100/cm?cmnd=Status%208");
>         if (httpGet(url, resp) > 0) {
>             temp = jsonNum(resp, "StatusSNS#DS18B20#Temperature");
>         }
>         delay(5000);                     // alle 5 s pollen, ABSEITS des Main-Loops
>     }
> }
> int main() { spawnTask("poller", 8); return 0; }   // 8 KB Stack fuer reines HTTP, 10–16 fuer TLS
> ```
> Ein einmaliger Abruf in `main()` (das bereits in seinem eigenen Task laeuft) ist in Ordnung — es ist der *wiederholte* Main-Loop-Callback-Fall, der blockiert.

| Funktion | Beschreibung |
|----------|-------------|
| `int httpGet(char url[], char response[])` | HTTP GET, gibt Antwortlaenge oder negativen Fehler zurueck |
| `int httpPost(char url[], char data[], char response[])` | HTTP POST, gibt Antwortlaenge oder negativen Fehler zurueck |
| `void httpHeader(char name[], char value[])` | Benutzerdefinierten Header fuer die naechste Anfrage setzen |
| `int jsonStr(char json[], "pfad", char out[])` | JSON parsen, String-Wert am `#`-Pfad nach `out` schreiben; gibt Laenge zurueck (`-1` wenn nicht vorhanden) |
| `float jsonNum(char json[], "pfad")` | JSON parsen, numerischen Wert am `#`-Pfad (in eine **float**-Variable zuweisen — siehe Hinweis) |
| `int webParse(char source[], "delim", int index, char result[])` | Nicht-JSON Antworttext parsen (siehe unten) |

**Rueckgabewerte:** `> 0` = Laenge des Antwortkoerpers, `0` = leere Antwort, negativ = HTTP-Fehlercode (z.B. -404).

**Beispiel — Daikin-Klimaanlage Sensorabfrage:**
```c
char url[64];
char response[256];
char token[32];
int len;
int pos;

void main() {
    strcpy(url, "http://192.168.188.43/aircon/get_sensor_info");
    len = httpGet(url, response);
    // response = "ret=OK,htemp=19.0,hhum=-,otemp=7.0,err=0,cmpfreq=0"

    if (len > 0) {
        // Innentemperatur extrahieren (htemp)
        pos = strFind(response, token);  // "htemp=" finden
        strToken(token, response, ',', 3);  // 3. Token = "htemp=19.0"
        printString(token);
    }
}
```

**Beispiel — Tasmota-Befehl an ein anderes Geraet** (hier in `EverySecond` zur Kuerze; fuer *periodisches* Polling das Worker-Muster oben verwenden, damit die Anfrage abseits des Main-Loops laeuft):
```c
char url[128];
char response[512];
int len;

void EverySecond() {
    strcpy(url, "http://192.168.1.100/cm?cmnd=Status%200");
    len = httpGet(url, response);
    if (len > 0) {
        print(len);
        // Antwort mit strFind/strToken parsen...
    }
}
```

**Beispiel — POST mit benutzerdefiniertem Header:**
```c
char url[128];
char data[128];
char hname[32];
char hval[64];
char response[512];

void main() {
    strcpy(url, "http://192.168.1.100/api/data");
    strcpy(data, "{\"value\":42}");
    strcpy(hname, "Content-Type");
    strcpy(hval, "application/json");
    httpHeader(hname, hval);  // Header vor Anfrage setzen
    int len = httpPost(url, data, response);
}
```

**webParse() — Nicht-JSON Web-Antworten parsen**

Entspricht Scripters `gwr()`. Extrahiert Daten aus Klartext-HTTP-Antworten (Key=Value, CSV, zeilenbasierte Formate).

**Zwei Modi:**
- **index > 0** — `source` an `delim` aufteilen, das N-te Segment zurueckgeben (1-basiert). Gibt Laenge zurueck.
- **index < 0** — Muster `delim=wert` finden, Wert extrahieren (stoppt bei `,`, `:` oder NUL). Gibt Laenge zurueck.
- **index == 0** — Keine Aktion, gibt 0 zurueck.

**Beispiel — Daikin-Klimaanlage mit webParse:**
```c
char url[64];
char response[256];
char value[32];

void main() {
    strcpy(url, "http://192.168.188.43/aircon/get_sensor_info");
    int len = httpGet(url, response);
    // response = "ret=OK,htemp=19.0,hhum=-,otemp=7.0,err=0,cmpfreq=0"

    if (len > 0) {
        // Name=Wert Modus: Wert nach "htemp=" extrahieren
        webParse(response, "htemp", -1, value);  // value = "19.0"
        float temp = atof(value);
        print(temp);  // 19.0

        // Split Modus: 4. komma-getrenntes Feld holen
        webParse(response, ",", 4, value);  // value = "otemp=7.0"
        printString(value);
    }
}
```

**jsonNum() / jsonStr() — JSON-Web-Antworten parsen**

Fuer JSON (statt key=value-Text) den eingebauten Parser nutzen — das ist Tasmotas `JsonParser`, du musst also nicht selbst mit `strFind`/`strToken` parsen. Der Pfad ist `#`-getrennt fuer verschachtelte Objekte, dieselbe Syntax wie `sensorGet()` (z.B. `"StatusSNS#ENERGY#Power"`); fuer einen flachen Schluessel einfach den Namen (`"soc"`). Das Quell-JSON wird bis ~640 Bytes geparst.

- `jsonStr(json, "pfad", out)` — schreibt den Wert als String nach `out`, gibt die Laenge zurueck (`-1` wenn der Schluessel fehlt). Zahlen kommen als Text (`"35"` → `atoi`), Booleans als `"true"`/`"false"`.
- `jsonNum(json, "pfad")` — gibt den Wert als **float** zurueck.

> ⚠️ `jsonNum` gibt die rohen float-**Bits** zurueck, wenn du es einer `int`-Variable zuweist. In eine `float`-Variable lesen, oder `jsonStr` + `atoi`/`atof` verwenden, wenn du eine Ganzzahl willst.

**Beispiel — EV-Ladezustand von einem lokalen JSON-Endpunkt lesen:**
```c
char url[64];
char resp[384];
char tmp[24];

void main() {
    strcpy(url, "http://192.168.188.158:8129/soc");
    int n = httpGet(url, resp);
    // resp = {"ok":true,"soc":73,"charging":false,"range_km":320,"power_kw":11.0}
    if (n > 0) {
        jsonStr(resp, "soc", tmp);       int soc   = atoi(tmp);   // 73
        jsonStr(resp, "range_km", tmp);  int range = atoi(tmp);   // 320
        jsonStr(resp, "charging", tmp);  int chg   = (strFind(tmp, "true") >= 0);
        float pkw = jsonNum(resp, "power_kw");   // 11.0 — OK in einer float-Variable
        print(soc);
    }
}
```

### TCP-Server

Einen TCP-Stream-Server starten, um eingehende Verbindungen anzunehmen. Es wird nur ein Client gleichzeitig bedient.

| Funktion | Beschreibung |
|----------|--------------|
| `int tcpServer(int port)` | TCP-Server auf `port` starten. Gibt 0=ok, -1=Fehler, -2=kein Netzwerk zurueck |
| `tcpClose()` | TCP-Server schliessen und Client trennen |
| `int tcpAvailable()` | Wartenden Client annehmen und verfuegbare Bytes zurueckgeben |
| `int tcpRead(char buf[])` | String vom TCP-Client in `buf` lesen. Gibt gelesene Bytes zurueck |
| `tcpWrite(char str[])` | String an TCP-Client senden |
| `int tcpReadArray(int arr[])` | Verfuegbare Bytes in Int-Array lesen (ein Byte pro Element). Gibt Anzahl zurueck |
| `tcpWriteArray(int arr[], int num)` | `num` Array-Elemente als uint8-Bytes an TCP-Client senden |
| `tcpWriteArray(int arr[], int num, int type)` | Mit Typ senden: 0=uint8, 1=uint16 BE, 2=sint16 BE, 3=float BE |

**Beispiel — Einfacher TCP-Echo-Server:**
```c
char buf[128];

void main() {
    tcpServer(8888);   // auf Port 8888 horchen
}

void Every50ms() {
    int n = tcpAvailable();  // Client annehmen + pruefen
    if (n > 0) {
        tcpRead(buf);        // eingehenden String lesen
        tcpWrite(buf);       // zuruecksenden
    }
}
```

**Beispiel — Binaere Datenuebertragung:**
```c
int data[100];

void main() {
    tcpServer(9000);
}

void EverySecond() {
    int n = tcpAvailable();
    if (n > 0) {
        // Rohdaten in Array lesen
        int count = tcpReadArray(data);
        print(count);
        // als uint16 Big-Endian zuruecksenden
        tcpWriteArray(data, count, 1);
    }
}
```

### TCP-Client

Ausgehende TCP-Verbindungen zu entfernten Hosts oeffnen. Bis zu **4 parallele Client-Slots** werden unterstuetzt; ein Selektor waehlt den aktiven Slot, alle Lese/Schreib-Aufrufe arbeiten auf diesem Slot. Slot 0 faellt zusaetzlich auf den vom `tcpServer()` angenommenen Client zurueck, sodass dieselben `tcpRead`/`tcpWrite`/`tcpAvailable`-Aufrufe fuer beide Rollen funktionieren.

| Funktion | Beschreibung |
|----------|-------------|
| `int tcpConnect("host", port)` | TCP-Verbindung vom aktiven Slot zu `host:port` oeffnen. Gibt 0=verbunden, -1=Fehler, -2=kein Netzwerk zurueck |
| `int tcpConnect(char host[], port)` | Dasselbe mit char-Array als Host (IP oder DNS-Name) statt Literal |
| `int tcpConnected()` | Gibt 1 zurueck, wenn der aktive Slot eine offene Verbindung hat, sonst 0 |
| `tcpDisconnect()` | Client-Verbindung des aktiven Slots schliessen |
| `tcpSelect(int slot)` | Aktiven Client-Slot waehlen (0–3). Alle nachfolgenden Client-Aufrufe zielen auf diesen Slot |

**Hinweise:**
- `tcpRead(buf)`, `tcpWrite(buf)`, `tcpAvailable()`, `tcpReadArray()`, `tcpWriteArray()` arbeiten alle auf dem **aktiven** Slot. Mit `tcpSelect(n)` umschalten.
- `tcpWrite()` benoetigt weiterhin ein `char[]` — String-Literale werden nicht akzeptiert (`char msg[] = "hello\n"; tcpWrite(msg);`).
- Slot 0 ist speziell: Wenn kein ausgehender Client auf Slot 0 offen ist, faellt er transparent auf den Server-Client von `tcpServer()` zurueck. Bestehende Nur-Server-Skripte laufen unveraendert weiter.
- Verbindungen sind im Wesentlichen nicht-blockierend, haben aber einen kurzen Socket-Timeout — ein fehlgeschlagenes `tcpConnect()` kehrt schnell mit -1 zurueck.

**Beispiel — Periodischer TCP-Client mit Heartbeat:**
```c
char rxbuf[128];
char msg[]  = "ping\n";

void EverySecond() {
    tcpSelect(0);                          // aktiver Slot = 0
    if (!tcpConnected()) {
        int r = tcpConnect("192.168.1.50", 1234);
        if (r != 0) { return; }            // naechsten Tick erneut versuchen
    }
    tcpWrite(msg);
    delay(150);                            // dem Server Zeit zum Antworten geben
    if (tcpAvailable() > 0) {
        int n = tcpRead(rxbuf);
        print(n);                          // z.B. 24 Bytes echo zurueck
    }
}

void OnExit() {
    tcpDisconnect();                       // beim Skript-Stopp sauber aufraeumen
}
```

**Beispiel — Zwei unabhaengige TCP-Clients parallel:**
```c
char buf[128];
char hello[] = "hello\n";

void main() {
    tcpSelect(0);
    tcpConnect("10.0.0.10", 9000);         // Slot 0 → Metrik-Server

    tcpSelect(1);
    tcpConnect("10.0.0.11", 9001);         // Slot 1 → Befehls-Server
}

void EverySecond() {
    // Heartbeat auf Slot 0 senden
    tcpSelect(0);
    if (tcpConnected()) { tcpWrite(hello); }

    // Antworten auf Slot 1 abfragen
    tcpSelect(1);
    if (tcpConnected() && tcpAvailable() > 0) {
        tcpRead(buf);
        // Befehl in buf verarbeiten...
    }
}
```

#### TCP-Client Tuning *(seit 1.5.1)*

Vier Per-Slot-Helfer fuer produktive TCP-Verbindungen. Alle arbeiten
auf dem **aktuell ausgewaehlten** Slot — also vorher `tcpSelect(N)`
aufrufen. Loesen das wiederkehrende Idle-Disconnect-Problem
(SMA / Solar-Edge / Powerwall) und die Modbus-TCP Request-Response-
Standardroutine.

| Funktion | Beschreibung |
|----------|-------------|
| `int tcpKeepalive(int idle_sec, int intvl_sec, int count)` | SO_KEEPALIVE auf dem aktiven Slot aktivieren und TCP_KEEPIDLE / KEEPINTVL / KEEPCNT via setsockopt setzen. Returns 1=ok, 0=err. Typisch SMA Tripower: `tcpKeepalive(30, 10, 3)` — nach 30 s idle bis zu 3 Probes im 10 s-Abstand bevor TCP als tot deklariert wird. Loest das "Peer schliesst Verbindung nach 60 s Idle"-Pattern. |
| `tcpNoDelay(int on)` | Nagle-Algorithmus auf dem aktiven Slot umschalten. `tcpConnect()` setzt bereits `setNoDelay(true)` — dieser Aufruf reaktiviert Nagle z.B. fuer Bulk-Transfers. |
| `int tcpDisconnectReason()` | Letzter Disconnect-Grund: 0=NEVER, 1=CONNECTED (offen), 2=PEER_CLOSED (FIN), 3=TIMEOUT, 4=NETWORK, 5=USER_CLOSED. Erlaubt Watchdog-Logik die RST/FIN von Netzwerk-Fehlern unterscheidet. |
| `int tcpTransact(char req[], int req_len, char resp[], int resp_max, int timeout_ms)` | Atomisches write-and-await-reply auf dem aktiven Slot — fasst `tcpWriteArray + poll-tcpAvailable + tcpReadArray` in einen Syscall. Returns Bytes empfangen (alle sofort verfuegbaren bis `resp_max`); -1 Timeout; -2 nicht verbunden oder Peer hat waehrend des Wartens FIN gesendet (`tcpDisconnectReason()` zeigt PEER_CLOSED); -3 ungueltige Argumente. Haelt `vm_mutex` fuer die gesamte Wartezeit — gedacht fuer `spawnTask`-Worker. Geeignet fuer Protokolle deren Antwort in einem TCP-Segment passt (Modbus-TCP, <=256 B). |

**Beispiel — Modbus-TCP-Polling mit einem Roundtrip pro Sekunde:**
```c
char req[12]  = {0,1, 0,0, 0,6,  1, 3, 0,0x10, 0,4};  // FC03 4 Register lesen
char resp[260];

void main() {
    tcpSelect(0);
    tcpConnect("192.168.1.50", 502);
    tcpKeepalive(30, 10, 3);                  // SMA-Style Keep-Alive
}

void EverySecond() {
    tcpSelect(0);
    int n = tcpTransact(req, 12, resp, 260, 200);   // <=200 ms
    if (n > 0) {
        // resp[0..n-1] = MBAP Header + FC03 Antwort
    } else if (n == -2) {
        int reason = tcpDisconnectReason();
        if (reason == 2 || reason == 4) tcpConnect("192.168.1.50", 502);
    }
}
```

Vorgefertigte Helfer `mbFC03/04/06/16` in `examples/modbus_lib.tc`.

### Raw-TLS-Client (HTTPS)

**Nur ESP32.** Ein roher TLS-Socket (leichtgewichtiges BearSSL, eine Verbindung zur Zeit) — du sprichst HTTPS selbst: Anfrage schreiben, Statuszeile / Header / Body lesen. Dafuer gedacht, wenn `httpGet` zu hoch-level ist: OAuth-Redirect+Cookie-Abläufe, eigenes Signieren von Anfragen (z.B. ein API-"Stamp"), oder jedes Protokoll, bei dem du die rohe Antwort brauchst. Alles bleibt im `.tcb` — eine Cloud-API die sich aendert ist ein `tc_upload`, kein Firmware-Reflash.

> ⚠️ Handshake und Lesevorgänge **blockieren** (ein TLS-Handshake dauert 1–3 s). Diese aus einer **TaskLoop** aufrufen, nie aus `EverySecond`/`Command` (dort hielten sie die Slot-Mutex wie ein langsames `httpGet`).

| Funktion | Beschreibung |
|----------|--------------|
| `int tlsConnect(host, port)` | DNS + TLS-Connect (SNI aus `host`); `0`=ok, `-1`=Fehler. `host` = Literal oder `char[]` |
| `int tlsWrite(char req[])` | Anfrage-String schreiben; gibt geschriebene Bytes zurueck, `-1` wenn nicht verbunden |
| `int tlsReadLine(char buf[])` | Eine Zeile lesen (bis `\n`, entfernt `\r`) nach `buf`; Laenge, `-1` wenn keine. Fuer Statuszeile + Header |
| `int tlsRead(char buf[], int maxbytes)` | Bis zu `maxbytes` rohe Bytes nach `buf` lesen; Anzahl. Fuer den Body |
| `int tlsAvailable()` | Verfuegbare Bytes |
| `int tlsConnected()` | `1` wenn verbunden |
| `void tlsStop()` | Schliessen + TLS-Client freigeben |
| `int base64Enc(char in[], int inlen, char out[])` | `inlen` Bytes aus `in[]` Base64-kodieren nach `out[]` (binaersicher — fuer Signieren / Basic-Auth); kodierte Laenge |

Zertifikatspruefung ist aus (`setInsecure`, wie beim Scripter-httpsget). Empfangspuffer 8 KB (`TC_TLS_RX_BUF` erhoehen, falls ein Server groessere TLS-Records nutzt).

**Beispiel — GET https://example.com/ (auf Hardware bewiesen):**
```c
char sline[200];
char body[300];

void doFetch() {
    if (tlsConnect("example.com", 443) != 0) { addLog("TLS connect fail"); return; }
    char req[160];
    sprintf(req, "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    tlsWrite(req);
    tlsReadLine(sline);                       // "HTTP/1.1 200 OK"
    int ok = (strFind(sline, "200") >= 0);
    int n = 1;
    while (n > 0) { n = tlsReadLine(body); }  // Header bis zur Leerzeile ueberspringen
    int len = tlsRead(body, 250);             // erster Teil des Body
    tlsStop();
    addLog("TLS: ok200=%d len=%d", ok, len);
}

void TaskLoop() {                             // TLS-I/O ausserhalb der Main-Loop
    doFetch();
    while (1) { delay(60000); }
}
```

### MQTT Abonnieren / Publizieren

MQTT-Topics abonnieren und auf eingehende Nachrichten reagieren oder beliebige
Payloads publizieren. MQTT gehoert zum Kern von Tasmota — es gibt nichts
einzuschalten.

> ⚠️ Bis 1.6.51 standen diese Aufrufe hinter `#ifdef USE_MQTT`, und diesen
> Schalter gibt es seit Tasmota 3.1.13 (Januar 2017) nicht mehr, seit MQTT zum
> Kern wurde. Das Gate hat also nicht "abgeschaltetes MQTT abgefangen", sondern
> die MQTT-Syscalls aus JEDEM Bau entfernt — `mqttPublish()` lieferte immer -1
> (Hans, 2026-08-18).

| Funktion | Beschreibung |
|----------|-------------|
| `int mqttSubscribe("topic")` | `topic` abonnieren. Gibt Abo-Slot (0–9) bei Erfolg zurueck, -1 bei Fehler (kein freier Slot, Broker offline) |
| `int mqttSubscribe(char topic[])` | Dasselbe mit char-Array als Topic (zur Laufzeit gebaut) |
| `int mqttUnsubscribe("topic")` | Zuvor abonniertes Topic abbestellen. Gibt 0=ok, -1=nicht gefunden zurueck |
| `mqttPublish("topic", "payload")` | `payload` auf `topic` publizieren |
| `mqttPublish(char topic[], char payload[])` | Dasselbe mit zur Laufzeit gebauten Zeichenketten — etwa einem Topic, das den Geraetenamen traegt |
| `mqttPublish(topic, payload, stufe)` | …und mit Log-Stufe: **0 = still**, 2 = Info (wie die anderen Formen), 3 = nur bei `weblog 3` |

Rückgabe: `0` gesendet, `-1` Argumente falsch, **`-2` MQTT ist am Gerät
abgeschaltet** (`SetOption3`, Vorgabe aus `MQTT_USE`). Dann verwirft Tasmota die
Nachricht und beantwortet überhaupt keinen MQTT-Befehl mehr — `MqttHost` meldet
`Unknown`.

**Hinweise:**
- Bis zu **10 Abonnements** pro VM, Topic maximal **128 Zeichen**.
- Wildcard `'#'` wird ausschliesslich als **Praefix-Match am Ende** unterstuetzt (`"sensors/#"` matcht `sensors/temp`, `sensors/humi/1` usw.). MQTTs `+`-Single-Level-Wildcard wird nicht unterstuetzt.
- Passende Topics loesen den `OnMqttData(char topic[], char payload[])` Callback aus. Beide Strings werden fuer die Dauer des Callbacks in den VM-Heap kopiert.
- Abonnements bleiben ueber `TinyCRun`-Neuladen desselben Slots erhalten. `mqttUnsubscribe()` in `OnExit()` aufrufen, wenn bei Neustart eine saubere Basis gewuenscht ist.
- Abonnements werden bei Reconnect automatisch neu an den Broker gesendet (ueber `FUNC_MQTT_INIT`).
- **Alle paar Sekunden publizieren?** Dann die Log-Stufe nehmen. Ein Regler, der
  seinen Sollwert alle 5 s schickt, schreibt bei der Vorgabe `weblog 2` alle 5 s
  zwei Konsolenzeilen — und wer den Log herunterdreht, verliert alles andere mit.
  `mqttPublish(topic, wert, 0)` publiziert und sagt nichts. Topic maximal **128**
  Zeichen, Nutzlast **512** — laenger wird abgeschnitten, nicht abgewiesen.
- Die Drei-Argument-Form und jede Form mit einer Laufzeit-Zeichenkette brauchen
  **Firmware-ABI 24**. Zwei Literale erzeugen weiter den alten Syscall und laufen
  auf jeder Firmware.
- ⚠️ Der Umweg ueber den **Befehl** `Publish` (`tasmCmd`) ist etwas anderes:
  `CmndPublish` uebernimmt das Topic woertlich (nur `#`→Leerzeichen), `%topic%`
  wird also **nicht** ersetzt — ein Abonnent abonniert brav die Zeichenkette
  `stat/%topic%/…` (Hans, 2026-08-18). Mit `mqttPublish()` braucht es den Umweg
  jetzt nicht mehr.

**Beispiel — Fernsteuerung via MQTT:**
```c
char reply[64];

void main() {
    mqttSubscribe("cmnd/room1/#");         // Wildcard-Praefix
    mqttSubscribe("home/heartbeat");       // exakter Match
}

void OnMqttData(char topic[], char payload[]) {
    if (strcmp(topic, "home/heartbeat") == 0) {
        mqttPublish("stat/room1/alive", "ok");
        return;
    }
    // cmnd/room1/light → GPIO umschalten etc.
    sprintf(reply, "got %s = %s", topic, payload);
    addLogLevel(2, reply);
}

void OnExit() {
    mqttUnsubscribe("cmnd/room1/#");
    mqttUnsubscribe("home/heartbeat");
}
```

### mDNS-Dienstankuendigung

Das Geraet als mDNS-Dienst im lokalen Netzwerk registrieren, um Geraeteemulation zu ermoeglichen (Everhome ecotracker, Shelly oder benutzerdefinierte Dienste).

| Funktion | Beschreibung |
|----------|-------------|
| `int mdnsRegister("name", "mac", "type")` | mDNS-Responder starten und Dienst ankuendigen. Gibt 0 bei Erfolg zurueck |

**Parameter (alle Zeichenketten-Literale):**
- **name** — Hostname-Praefix. Verwenden Sie `"-"` fuer Tasmota's Standard-Hostname, oder ein benutzerdefiniertes Praefix (MAC wird automatisch angefuegt)
- **mac** — MAC-Adresse. Verwenden Sie `"-"` fuer die eigene MAC des Geraets (Kleinbuchstaben, ohne Doppelpunkte), oder geben Sie eine benutzerdefinierte Zeichenkette an
- **type** — Diensttyp: `"everhome"` (ecotracker), `"shelly"` oder ein beliebiger benutzerdefinierter Dienstname

**Eingebaute Emulationstypen:**
- `"everhome"` — registriert `_everhome._tcp` mit IP-, Serial-, Productid-TXT-Eintraegen
- `"shelly"` — registriert `_http._tcp` und `_shelly._tcp` mit Firmware-Metadaten-TXT-Eintraegen
- Jede andere Zeichenkette — registriert `_<type>._tcp` mit IP- und Serial-TXT-Eintraegen

**Beispiel — Everhome-Ecotracker-Emulation:**
```c
int main() {
    mdnsRegister("ecotracker-", "-", "everhome");
    return 0;
}
```

Dies entspricht Scripter's `mdnsRegister("ecotracker-", "-", "everhome")`.

### WebUI-Widgets

Interaktive Dashboards mit Widget-Funktionen erstellen. Widgets koennen an zwei Stellen erscheinen:

1. **Dedizierte `/tc_ui`-Seite** — verwenden Sie den `WebUI()`-Callback
2. **Tasmota-Hauptseite** (Sensorbereich) — verwenden Sie den `WebCall()`-Callback

Beide Callbacks verwenden die gleichen Widget-Funktionen.

| Funktion | Beschreibung |
|----------|-------------|
| `webButton(var, "label")` | Momentane Aktions-Schaltflaeche — setzt `var` bei Klick auf 1 (Skript liest, handelt, setzt auf 0 zurueck). Kein EIN/AUS-Zusatz. Optionales `"Idle\|Active"`-Label zeigt den `Active`-Text ~2,5 s als Klick-Bestaetigung, dann zurueck (generisches ✓ ohne `\|`) |
| `webToggle(var, "label")` | Rastende Ein/Aus-Schaltflaeche (0/1) — **gruen wenn `var`≠0, grau wenn 0**, Klick schaltet um. Optionales `"Ein\|Aus"`-Label zeigt verschiedenen Text/Emoji je Zustand (z.B. `"💡 An\|🌙 Aus"`); ohne `\|` → gleicher Text, nur Farbe |
| `webSlider(var, min, max, "label")` | Bereichsregler — ziehen zum Einstellen des Werts |
| `webCheckbox(var, "label")` | Kontrollkaestchen (0/1) — Aktivieren/Deaktivieren schaltet um |
| `webText(chararray, maxlen, "label")` | Texteingabe — Zeichenkettenvariable bearbeiten |
| `webNumber(var, min, max, "label")` | Zahleneingabe mit Min/Max-Grenzen |
| `webPulldown(var, "label", "opt0\|opt1\|opt2")` | Dropdown-Auswahl mit Beschriftung — Pipe-getrennte Optionen, 0-basierter Index. `"@getfreepins"` als Optionen zeigt verfuegbare GPIO-Pins |
| `webRadio(var, "opt0\|opt1\|opt2")` | Optionsschaltflaechengruppe — Pipe-getrennte Optionen, 0-basierter Index |
| `webTime(var, "label")` | Zeitauswahl (HH:MM) — gespeichert als HHMM-Ganzzahl (z.B. 1430 = 14:30) |
| `webPageLabel(page, "label")` | Seite 0–5 mit einer Schaltflaechenbeschriftung auf der Hauptseite registrieren |
| `int webPage()` | Gibt die aktuelle Seitennummer zurueck, die gerendert wird (in `WebUI()` zur Verzweigung verwenden) |
| `webConsoleButton("/url", "label")` | Schaltflaeche im Tasmota-Utilities-Menue registrieren (max 4). Navigiert zu URL bei Klick |
| `webCard(on)` | Pro-Slot-Schalter fuer den **Karten-Rahmen** auf der Hauptseite (siehe unten). Standard ein; `webCard(0)` in `main()` rendert die Hauptseiten-Zeilen dieses Skripts ohne Rahmen |

Das erste Argument der Widget-Funktionen ist immer eine **globale Variable**, die das Widget liest und in die es schreibt. Der Compiler uebergibt automatisch die Adresse der Variable an den Syscall.

**Beispiel — Widgets auf der Hauptseite:**
```c
int relay;
int brightness;

void WebCall() {
    webToggle(relay, "Power");
    webSlider(brightness, 0, 100, "Brightness");
}
```

**Karten pro Skript (Hauptseite).** Wenn mehrere Skripte gleichzeitig Widgets auf der Hauptseite rendern, wird die `WebCall()`-Ausgabe jedes Skripts automatisch in eine eigene Karte gerahmt — ein umrandeter Kasten mit einer `slot N`-Beschriftung und einem Abstand zwischen den Karten — sodass die Apps optisch getrennt sind statt ineinanderzulaufen. Das ist **standardmaessig ein**; kein Code noetig. Um ein Skript auszunehmen und seine Zeilen ohne Rahmen darzustellen (die Optik vor den Karten), rufen Sie `webCard(0)` einmal in `main()` auf:

```c
void main() {
    webCard(0);   // Hauptseiten-Zeilen dieses Skripts ohne Karten-Rahmen
}
```

Die Einstellung gilt pro Slot, die anderen Skripte behalten ihre Karten. (Benoetigt Firmware mit Syscall-ABI ≥ 16.)

**Beispiel — Mehrere Seiten mit benutzerdefinierten Schaltflaechen:**

Bis zu 6 Seiten koennen mit `webPageLabel()` registriert werden. Jede erstellt eine Schaltflaeche auf der Tasmota-Hauptseite. Verwenden Sie `webPage()` innerhalb von `WebUI()`, um verschiedene Widgets pro Seite zu rendern.

```c
int power;
int brightness;
int mode;
int alarm_time;
char devname[32];

void WebUI() {
    int page = webPage();
    if (page == 0) {
        webToggle(power, "Power");
        webSlider(brightness, 0, 100, "Brightness");
        webPulldown(mode, "Mode", "Off|Auto|Manual");
    }
    if (page == 1) {
        webTime(alarm_time, "Wake-up Time");
        webText(devname, 32, "Device Name");
    }
}

int main() {
    webPageLabel(0, "Controls");   // erste Schaltflaeche auf der Hauptseite
    webPageLabel(1, "Settings");   // zweite Schaltflaeche auf der Hauptseite
    return 0;
}
```

Wenn kein `webPageLabel()` aufgerufen wird, aber `WebUI()` existiert, erscheint eine einzelne "TinyC UI"-Schaltflaeche.

**Funktionsweise:**
1. `WebCall()` rendert Widgets im Sensorbereich der Tasmota-Hauptseite
2. `WebUI()` rendert Widgets auf dedizierten Seiten unter `http://<device>/tc_ui?p=N`
3. `webPageLabel(N, "text")` registriert Seite N (0–5) mit einer Schaltflaeche auf der Hauptseite
4. `webPage()` gibt die aktuelle Seitennummer zurueck, damit `WebUI()` verschiedene Widgets anzeigen kann
5. Wenn Sie einen Regler bewegen / eine Schaltflaeche klicken, sendet JavaScript den neuen Wert per AJAX
6. Der Server schreibt den Wert direkt in die TinyC-globale Variable
7. Die Seite aktualisiert sich automatisch, um den aktualisierten Zustand anzuzeigen
8. Text- und Zahleneingaben pausieren die automatische Aktualisierung waehrend der Bearbeitung (wird bei Fokusverlust fortgesetzt)

### WebChart — Automatische Google Charts

`WebChart()` rendert Google Charts auf der Tasmota-Hauptseite mit einem einzigen Funktionsaufruf pro Datenserie. Die Google Charts-Bibliothek und das gesamte JavaScript werden automatisch generiert.

```c
void WebChart(int type, "title", "unit", int color, int pos, int count,
              float array[], int decimals, int interval, float ymin, float ymax)
```

| Parameter | Beschreibung |
|-----------|-------------|
| `type` | Diagrammtyp: `0` = Liniendiagramm, `1` = Saeulendiagramm |
| `"title"` | Diagrammtitel (String-Literal). Leer `""` = Serie zum vorherigen Diagramm hinzufuegen |
| `"unit"` | Y-Achsen-Einheit (String-Literal, z.B. `"°C"`, `"%"`, `"m/s"`) |
| `color` | Linien-/Balkenfarbe als Hex-RGB (z.B. `0xe74c3c` fuer Rot) |
| `pos` | Aktuelle Schreibposition im Ringpuffer |
| `count` | Anzahl gueltiger Datenpunkte (≤ Array-Groesse) |
| `array` | Float-Array mit den Daten (Ringpuffer) |
| `decimals` | Anzahl Dezimalstellen fuer Datenwerte (0–6) |
| `interval` | Minuten zwischen Datenpunkten (fuer X-Achsen-Zeitbeschriftung) |
| `ymin` | Y-Achsen-Minimum. Wenn `ymin >= ymax`, automatische Skalierung |
| `ymax` | Y-Achsen-Maximum. Wenn `ymin >= ymax`, automatische Skalierung |

**Chart-Konfiguration (optional, vor `WebChart()` aufrufen):**

| Funktion | Beschreibung |
|----------|-------------|
| `WebChartSize(int width, int height)` | Groesse des Chart-`<div>` in Pixeln setzen (z. B. `640 × 200`). `0` fuer einen der Werte = Standard verwenden (seit 1.6.54: volle Containerbreite × 300 px). |
| `WebChartTimeBase(int minutes)` | Zeitbasis der X-Achse relativ zu „jetzt“ verschieben. `0` = an „jetzt“ verankert (Standard); negativ = in die Vergangenheit (z. B. `-1440` = vor 24 h). Nuetzlich, um das aelteste Sample eines Ringpuffers an den linken Rand zu legen. |
| `WebChartJS("…js…")` | JavaScript-Schnipsel an das **zuletzt erzeugte** Diagramm haengen — also **nach** dem `WebChart()`, zu dem er gehoert. Er laeuft im Zeichen-Kontext mit `dt` (Google `DataTable`), `o` (Optionen) und `el` (DOM-Element), nachdem die Standardoptionen gebaut sind und bevor gezeichnet wird. Entweder `o`/`dt` veraendern und TinyC zeichnen lassen — oder selbst zeichnen und `o.done=1` setzen, dann entfaellt das Standard-Zeichnen (damit ist jeder Diagrammtyp moeglich). |

⚠️ `WebChartJS()` **weist zu, es haengt nicht an.** Zwei aufeinanderfolgende Aufrufe auf
dasselbe Diagramm überschreiben sich gegenseitig — der zweite gewinnt, der erste ist
wirkungslos, ohne Fehlermeldung. Wer Achsenformat *und* Nullpunkt setzen will, muss beides
in **einen** Aufruf packen.

⚠️ `ymin`/`ymax` sind gewöhnliche **Laufzeit-Floats**, keine Literale — eine Variable oder
ein Ausdruck ist erlaubt. Das ist mehr als eine Feinheit: es ist der Unterschied zwischen
„ich muss den Bereich beim Übersetzen kennen“ und „ich rechne ihn aus den Daten aus, die
ich gerade gesammelt habe“.

**Beispiel — 24h Wetterdaten:**
```c
#define NPTS 288       // 24h bei 5-Minuten-Intervallen
persist float h_temp[NPTS];
persist float h_hum[NPTS];
persist int h_pos = 0;
persist int h_count = 0;

void WebPage() {
    if (h_count < 1) return;
    WebChart(0, "Temperatur", "\u00b0C", 0xe74c3c, h_pos, h_count, h_temp, 1, 5, -20, 50);
    WebChart(0, "Luftfeuchte", "%",      0x3498db, h_pos, h_count, h_hum,  1, 5, 0, 100);
}
```

- **Fester Bereich** fuer Daten mit bekannten Grenzen (Luftfeuchte 0–100, UV-Index 0–12)
- **Auto-Skalierung** (`0, 0`) fuer Daten mit variablem Bereich (Helligkeit, Wind, Regen)
- Aufruf aus `WebPage()`-Callback — jeder Aufruf erzeugt eine Datenserie
- Mehrere Serien in einem Diagramm: erster Aufruf hat Titel, weitere verwenden `""` als Titel
- **Nullpunkt** verwenden, wenn der Leser **Balkenhöhen vergleichen** soll — siehe unten

#### Nullpunkt bei unbekanntem Maximum

Die Auto-Skalierung legt die Achse um die Daten. Vier Werte von 11,3 / 11,4 / 10,5 / 9,2 kWh
bekommen so eine Achse von 9 bis 12, und der letzte Balken schrumpft zum Stummel. Das
Diagramm ist nicht falsch, aber es sagt auf den ersten Blick „am Montag fast nichts“, wo
der Montag tatsächlich 80 % des Freitags geliefert hat. Immer wenn der Leser **Höhen**
vergleicht statt Werte an der Achse abzulesen, muss die Achse bei null beginnen — und die
Obergrenze kennt man selten im Voraus.

Zwei Wege, beide ohne Firmware-Änderung:

**Das Minimum festnageln, das Maximum automatisch lassen** (eine Zeile, keine Rechnerei):
```c
WebChart(1, "Solar-Ertrag Prognose", "kWh", 0xf39c12, 0, vdays, f_yield, 1, 1440, 0.0, 0.0);
WebChartJS("o.vAxis.viewWindow={min:0}");     // NACH dem WebChart, zu dem es gehört
```
`WebChartJS()` läuft, nachdem die Standardoptionen gebaut sind und bevor gezeichnet wird —
es kann deshalb ein **halbes** Sichtfenster setzen. Das Paar `ymin`/`ymax` kann das nicht
ausdrücken: es gilt alles oder nichts (`ymin >= ymax` bedeutet automatisch). Bei einem
Diagramm mit **zwei** Y-Achsen stattdessen `o.vAxes[0].viewWindow={min:0}`.

**Oder die Grenze aus den Daten rechnen** — sinnvoll, wenn zusätzlich Luft nach oben oder
ein gerundeter Höchstwert gewünscht ist:
```c
float mx = 0.0;
int i = 0;
while (i < vdays) {
    if (f_yield[i] > mx) { mx = f_yield[i]; }
    i = i + 1;
}
if (mx <= 0.0) { mx = 1.0; }    // ⚠️ lauter Nullen ergäben sonst wieder ymin >= ymax = auto
WebChart(1, "Solar-Ertrag Prognose", "kWh", 0xf39c12, 0, vdays, f_yield, 1, 1440, 0.0, mx * 1.15);
```
Die Absicherung ist kein Formalismus: ein Prognose-Array, das noch leer ist, oder eine
Messung bei Nacht macht `mx` zu null, damit gilt `ymin >= ymax`, und das Diagramm fällt
still auf genau die Auto-Skalierung zurück, die man vermeiden wollte — ausgerechnet an dem
Tag, an dem die Daten am seltsamsten aussehen.

#### Diagrammgröße über mehrere Skripte hinweg

`WebChartSize(width, height)` setzt die Größe des Diagramm-`<div>`; `0` für einen der Werte
nimmt den Standard. Seit **1.6.54** ist der Standard `width:100%` × 300 px — also die
Kartenbreite und nicht mehr feste 960 px. Die festen 960 px machten auf dem Telefon jedes
Diagramm breiter als das Sichtfeld und die ganze Seite waagerecht schiebbar.

⚠️ **Es ist eine Einstellung je Seitenaufbau, nicht je Skript.** Sie wird einmal zu Beginn
jedes Hauptseiten-Aufbaus auf „nicht gesetzt“ zurückgestellt und wandert dann in
Slot-Reihenfolge weiter. Ein Skript, das sie nie aufruft, bekommt also **nicht** den
Standard — es erbt, was der *vorherige* Slot gesetzt hat, und dasselbe Skript erscheint
verschieden breit, je nachdem in welchem Slot es läuft und welche Nachbarn geladen sind.
Das ist die übliche Ursache, wenn Diagramme zweier Skripte auf einer Seite unterschiedlich
breit und gegeneinander versetzt stehen.

Abhilfe: jedem diagrammzeichnenden Skript denselben ausdrücklichen Aufruf als erste Zeile
seines `WebPage()` geben:
```c
void WebPage() {
    WebChartSize(0, 260);   // Breite 0 = volle Containerbreite, unabhängig vom Slot
    ...
}
```

**HTML aus Dateien einbinden:**

Verwenden Sie `webSendFile("filename")`, um den Inhalt einer Datei vom Geraetedateisystem direkt an die Webseite zu senden. Dies ist nuetzlich fuer grosses HTML, CSS oder JavaScript, das zu gross waere, um in Bytecode-Konstanten kompiliert zu werden.

```c
void WebPage() {
    webSendFile("chart.html");  // Diagrammbibliothek von /chart.html einbinden
}
```

Die Datei wird in 256-Byte-Stuecken gelesen und per `WSContentSend` gesendet. Der Dateiname kann mit oder ohne fuehrendes `/` angegeben werden.

### Benutzerdefinierte Web-Handler

Benutzerdefinierte HTTP-Endpunkte auf dem Tasmota-Webserver registrieren. Wenn eine Anfrage eintrifft, wird der `WebOn()`-Callback aufgerufen, wobei die Handler-Nummer ueber `webHandler()` zugaenglich ist.

| Funktion | Beschreibung |
|----------|-------------|
| `webOn(int num, "url")` | Handler 1–4 fuer den angegebenen URL-Pfad registrieren |
| `int webHandler()` | Gibt die Handler-Nummer (1–4) innerhalb des `WebOn()`-Callbacks zurueck |
| `int webArg("name", buf)` | HTTP-Anfrageparameter in char-Puffer lesen, gibt Laenge zurueck (0 wenn fehlend) |

Verwenden Sie `webSend(buf)`, um den Antwortkoerper auszugeben. Der Standard-Inhaltstyp der Antwort ist `text/plain`.

**Beispiel — JSON-API-Endpunkt:**
```c
char buf[128];

void WebOn() {
    int h = webHandler();
    if (h == 1) {
        // GET /v1/json?id=xxx
        char id[32];
        int len = webArg("id", id);
        sprintf(buf, "{\"handler\":1,\"id\":\"%s\",\"value\":42}", id);
        webSend(buf);
    }
}

int main() {
    webOn(1, "/v1/json");
    return 0;
}
```

**Beispiel — Mehrere Endpunkte:**
```c
void WebOn() {
    int h = webHandler();
    char buf[64];
    if (h == 1) {
        sprintf(buf, "{\"temp\":%.1f}", smlGet(1));
        webSend(buf);
    }
    if (h == 2) {
        webSend("OK");
    }
}

int main() {
    webOn(1, "/api/sensor");
    webOn(2, "/api/ping");
    return 0;
}
```

**Hinweise:**
- Bis zu 4 Handler koennen registriert werden (1–4)
- URLs muessen mit `/` beginnen (z.B. `/v1/json`, `/api/data`)
- `webOn()` wird in `main()` aufgerufen — Handler werden beim Programmstart registriert
- Der `WebOn()`-Callback laeuft nachdem `main()` zurueckgekehrt ist (wie andere Callbacks)
- `webArg()` liest sowohl GET-Abfrageparameter als auch POST-Formularfelder
- Aequivalent zu Scripter's `won(N, "/url")` + `>onN`-Abschnitt
- CORS ist aktiviert, sodass Endpunkte von externen Anwendungen zugaenglich sind

### UDP-Multicast (Scripter-kompatibel)

Float-Variablen zwischen Tasmota-Geraeten ueber UDP-Multicast auf 239.255.255.250:1999 teilen.
Kompatibel mit Tasmota Scripter's globalem Variablenprotokoll.

| Funktion | Beschreibung |
|----------|-------------|
| `float udpRecv("name")` | Letzten empfangenen Wert fuer benannte Variable abrufen (0 wenn keiner) |
| `int udpReady("name")` | Gibt 1 zurueck wenn neuer Wert seit letzter Pruefung empfangen |
| `void udpSendArray("name", float_arr, count)` | Float-Array per binaeren Multicast senden |
| `int udpRecvArray("name", float_arr, maxcount)` | Float-Array empfangen, gibt tatsaechliche Anzahl zurueck |
| `udpSendStr("name", char str[])` | String über UDP-Multicast senden |

**Protokoll:**
- Einzelner Float: sende `=>name:[4 Bytes IEEE-754 Float]`
- Float-Array: sende `=>name:[2-Byte LE Anzahl][N x 4-Byte Float]`
- Empfang: sowohl ASCII (`=>name=value`) als auch binaer (einzeln oder Array)
- Multicast-Gruppe: `239.255.255.250`, Port `1999`
- Maximal 8 ueberwachte Variablennamen, je 16 Zeichen
- Maximal 64 Floats pro Array

**Callback:** Definieren Sie `void UdpCall()`, um bei jeder empfangenen Variable benachrichtigt zu werden.
Der UDP-Socket wird beim ersten Schreibzugriff auf eine globale Variable, `udpRecv()`- oder `udpReady()`-Aufruf automatisch initialisiert.
Skalare `global` Float-Variablen werden bei Zuweisung automatisch per UDP gesendet (kein expliziter Aufruf noetig).

**Socket-Watchdog:** Der Multicast-Socket hat einen eingebauten Inaktivitaets-Watchdog (Standard: 60 Sekunden). Wenn innerhalb der Timeout-Periode kein Paket empfangen wird, wird der Socket automatisch geschlossen und neu geoeffnet. Dies behebt das bekannte ESP32-Problem, bei dem der UDP-Empfangspfad nach variabler Zeit stillschweigend aufhoert zu funktionieren. Mit `udp(8, 0, sekunden)` kann der Timeout geaendert werden (0 = deaktiviert).

**Beispiel (Skalar — automatischer Broadcast):**
```c
global float temperature = 0.0;  // als 'global' deklariert → sendet automatisch bei Zuweisung

void EverySecond() {
    temperature = 20.0 + sin(counter) * 5.0;
    // Kein udpSend() noetig — Zuweisung an 'global' Variable sendet automatisch
}

void UdpCall() {
    float remote = udpRecv("temperature");
    // Fernwert verarbeiten...
}
```

**Beispiel (Array):**
```c
float sensors[8];

void EverySecond() {
    // 8 Sensorwerte als Array senden
    udpSendArray("sensors", sensors, 8);
}

void UdpCall() {
    float remote[8];
    int n = udpRecvArray("sensors", remote, 8);
    // n = Anzahl der tatsaechlich empfangenen Floats
}
```

### Allgemeine UDP-Funktion

Scripter-kompatible `udp()`-Funktion fuer beliebige UDP-Kommunikation. Verwendet einen separaten Socket von der Multicast-Variablenfreigabe oben.

| Funktion | Beschreibung |
|----------|-------------|
| `int udp(0, int port)` | UDP-Port oeffnen. Gibt 1 bei Erfolg zurueck |
| `int udp(1, char buf[])` | Empfangenen String in buf lesen. Gibt Byteanzahl zurueck (0 = nichts) |
| `void udp(2, char str[])` | Antwort an Absender-IP und -Port senden |
| `void udp(3, char url[], char str[])` | String an url mit dem Port von `udp(0)` senden |
| `int udp(4, char buf[])` | Remote-Absender-IP als String. Gibt Laenge zurueck |
| `int udp(5)` | Remote-Absender-Port zurueckgeben |
| `int udp(6, char url[], int port, char str[])` | String an beliebige url:port senden |
| `int udp(7, char url[], int port, int arr[], int count)` | Array als Rohbytes an url:port senden |
| `int udp(8, int welcher, int sekunden)` | Socket-Inaktivitaets-Timeout setzen (welcher: 0=Multicast, 1=Allgemeiner Port; 0=deaktiviert) |
| `int udp(9, char mcast_ip[], int port)` | Beliebiger UDP-Multicast-Gruppe beitreten und an Port binden. Gibt 1 bei Erfolg zurueck |

**Hinweise:**
- Das erste Argument (Modus) muss ein ganzzahliges Literal (0-9) sein
- Modi 6 und 7 erstellen einen temporaeren Socket (kein vorheriges `udp(0)` noetig)
- Modus 1 ist nicht-blockierend: gibt sofort 0 zurueck wenn kein Paket verfuegbar
- Modus 7 sendet das untere Byte jedes Array-Elements
- Modus 8 konfiguriert den Socket-Watchdog: wenn innerhalb von `sekunden` kein Paket empfangen wird, wird der Socket automatisch zurueckgesetzt. Standard ist 60 Sekunden. 0 zum Deaktivieren.
- Modus 9 tritt einer benutzerdefinierten Multicast-Gruppe bei (z.B. SMA Speedwire `239.12.255.254:9522`). Empfang ueber `udp(1, buf)`. Ersetzt eine bestehende Unicast-Bindung auf demselben Socket; `udp(0, port)` erneut aufrufen, um zurueck zu Unicast zu wechseln.

### I2C-Bus

Direkter I2C-Bus-Zugriff fuer Sensortreiber (erfordert `USE_I2C`). Alle Funktionen nehmen `bus` als letzten Parameter (0 oder 1).

| Funktion | Beschreibung |
|----------|-------------|
| `int i2cExists(int addr, int bus)` | Pruefen ob Geraet an Adresse antwortet. Gibt 1 zurueck wenn gefunden |
| `int i2cRead8(int addr, int reg, int bus)` | Einzelnes Byte aus Register lesen. Gibt Bytewert (0–255) zurueck |
| `int i2cWrite8(int addr, int reg, int val, int bus)` | Einzelnes Byte in Register schreiben. Gibt 1=ok, 0=Fehler zurueck |
| `int i2cRead(int addr, int reg, char buf[], int len, int bus)` | `len` Bytes in char-Array lesen. Gibt 1=ok zurueck |
| `int i2cWrite(int addr, int reg, char buf[], int len, int bus)` | `len` Bytes aus char-Array schreiben. Gibt 1=ok zurueck |
| `int i2cRead0(int addr, char buf[], int len, int bus)` | `len` Bytes ohne Register lesen. Gibt 1=ok zurueck |
| `int i2cWrite0(int addr, int reg, int bus)` | Nur Register-Byte schreiben (keine Daten). Gibt 1=ok zurueck |
| `int i2cSetDevice(int addr, int bus)` | Pruefen ob Adresse **nicht belegt** und ansprechbar. Gibt 1=verfuegbar zurueck |
| `i2cSetActiveFound(int addr, "type", int bus)` | Adresse als belegt registrieren. Loggt Erkennung |
| `int i2cReadRS(int addr, int reg, char buf[], int len, int bus)` | I2C-Lesen mit Repeated-Start (SMBus) |
| `I2cResetActive(int addr, int bus)` | Beanspruchte I2C-Adresse freigeben |

**Hinweise:**
- `bus` = 0 oder 1 — waehlt welcher I2C-Bus verwendet wird
- Adresse ist 7-Bit (0x00–0x7F), z.B. `0x48` fuer TMP102
- Register ist 8-Bit (0x00–0xFF)
- Pufferfunktionen verwenden `char[]`-Arrays — jedes Element enthaelt ein Byte (0–255)
- Maximale Pufferlaenge ist 255 Bytes
- Gibt 0 zurueck wenn I2C nicht einkompiliert ist oder die Operation fehlschlaegt
- `i2cSetDevice` + `i2cSetActiveFound` verwenden, um I2C-Adressen korrekt zu beanspruchen und Konflikte mit Tasmota-Treibern zu vermeiden

**Beispiel — TMP102-Temperatursensor auf Bus 0 lesen:**
```c
#define TMP102_ADDR  0x48
#define TMP102_TEMP  0x00
#define I2C_BUS      0

void EverySecond() {
    if (!i2cExists(TMP102_ADDR, I2C_BUS)) return;

    char buf[2];
    if (i2cRead(TMP102_ADDR, TMP102_TEMP, buf, 2, I2C_BUS)) {
        // TMP102: 12-Bit Temperatur in oberen Bits von 2 Bytes
        int raw = (buf[0] << 4) | (buf[1] >> 4);
        if (raw > 2047) raw = raw - 4096;  // Vorzeichenerweiterung
        float temp = (float)raw * 0.0625;

        char out[64];
        sprintf(out, "TMP102: %.2f °C\n", temp);
        printString(out);
    }
}
```

### Smart Meter (SML)

Zaehlerstaende auslesen und Zaehler ueber Tasmota's SML-Treiber steuern (erfordert `USE_SML` oder `USE_SML_M`).

SML kann **ohne Scripter** laufen — nur `USE_UFILESYS` wird fuer dateibasierte Zaehlerbeschreibungen benoetigt.
Der SML-Deskriptor-Tab der IDE verwaltet die Zaehlerdefinitionsdatei (`/sml_meter.def`) auf dem Geraet.

> **⚠ Stolperfalle: Rule1 teilt sich mit Scripter.** Der SML-Treiber gated
> auf `bitRead(rule_enabled, 0)` und laeuft nur wenn **Rule1** aktiv ist
> (`tasm_rule = 1` aus TinyC, oder `Rule1 1` in der Konsole). Dasselbe Bit
> aktiviert auch jeden noch vorhandenen Scripter-`>S`-Abschnitt. Liegt ein
> altes `*.tas`-Skript noch im Flash (z.B. ein altes ottelo
> `1_SML_Chart.tas` / `2_SML_Chart_PV.tas`), wird es sofort wenn SML
> aktiviert wird seine eigenen Chart-HTML- und
> `setOnLoadCallback`-Registrierungen parallel zu deiner TinyC-`WebPage()`
> emittieren — Chart-Targets kollidieren, JS-Callbacks ueberschreiben sich
> gegenseitig, die Hauptseite zeigt halb-gezeichnete Charts.
>
> **Fix beim Portieren von Scripter:** Das Scripter-Quelltext via IDE-
> *Tools → Edit Script* loeschen (Textfeld leeren, speichern). Der
> SML-Deskriptor in `/sml_meter.def` ist unabhaengig und bleibt erhalten.

#### Zaehlerstaende lesen

| Funktion | Beschreibung |
|----------|-------------|
| `float smlGet(int index)` | Zaehlerwert abrufen. Index 0 gibt Anzahl zurueck, 1..N gibt Werte zurueck |
| `int smlGetStr(int index, char buf[])` | Positiver Index: Zaehler-ID/OBIS-Zeichenkette. Negativer Index: numerischer Wert mit voller Praezision als Zeichenkette (4 Nachkommastellen) — entspricht Scripter's `smls[-x]` |

**Hinweise:**
- Index ist 1-basiert: `smlGet(1)` gibt den ersten Zaehlerwert zurueck
- `smlGet(0)` gibt die Gesamtzahl der Zaehlervariablen zurueck
- Gibt 0 zurueck wenn SML nicht einkompiliert ist oder der Index ausserhalb des Bereichs liegt
- `smlGet()`-Werte entsprechen Scripter's `sml[x]`-Syntax (einfache Float-Praezision)
- `smlGetStr(-i, buf)` formatiert den zugrundeliegenden `double`-SML-Wert mit 4 Nachkommastellen — nuetzlich bei kumulativen Energiezaehlern, die `float`'s ~7-stellige Praezision ueberschreiten

**Beispiel:**
```c
void WebCall() {
    char buf[64];
    int n = smlGet(0);  // Gesamtzaehler
    int i = 1;
    while (i <= n) {
        float val = smlGet(i);
        sprintf(buf, "{s}Meter %d{m}%.2f{e}", val);
        webSend(buf);
        i++;
    }
}
```

#### Zaehler-Setup

Einen Zaehler-Deskriptor laden und die seriellen Pins zur Laufzeit binden
(statt ueber das GPIO-Template), sodass ein einziges Firmware-Image jeden
Zaehler bedient — nur die Datei `/sml_meter.def` wird getauscht.

| Funktion | Beschreibung |
|----------|-------------|
| `int smlScripterLoad(char path[])` | SML-Zaehler-Deskriptor aus einer Datei laden (z. B. `"/sml_meter.def"`). Liefert 1 bei Erfolg. |
| `int smlApplyPins(char path[], int rxPin, int txPin, int flags)` | Deskriptor laden **und** den Zaehler an `rxPin`/`txPin` starten. `flags`-Bit 4 (`16`) waehlt den invertierten/IR-Lesekopf-Eingang. Liefert 1 bei Erfolg. Einmal aus `main()` aufrufen. |

#### Erweiterte Zaehlersteuerung

Diese Funktionen erfordern, dass `USE_SML_SCRIPT_CMD` in der Firmware aktiviert ist.

| Funktion | Beschreibung |
|----------|-------------|
| `int smlWrite(int meter, char buf[])` | Hex-Sequenz an Zaehler senden (z.B. Aufweck- oder Anfragebefehle) |
| `int smlWrite(int meter, "hex")` | Dasselbe, mit Zeichenketten-Literal (kein temporaerer Puffer noetig) |
| `int smlRead(int meter, char buf[])` | Rohen Zaehlerpuffer in char-Array lesen, gibt gelesene Bytes zurueck |
| `int smlSetBaud(int meter, int baud)` | Baudrate des seriellen Ports eines Zaehlers aendern |
| `int smlSetWStr(int meter, char buf[])` | Asynchrone Schreibzeichenkette fuer naechsten geplanten Sendevorgang setzen |
| `int smlSetWStr(int meter, "hex")` | Dasselbe, mit Zeichenketten-Literal |
| `int smlSetOptions(int options)` | Globale SML-Optionen-Bitmaske setzen |
| `int smlGetV(int sel)` | Daten-Gueltigkeitsflags abrufen/zuruecksetzen (0=abrufen, 1=zuruecksetzen) |

**Hinweise:**
- `meter` ist der 1-basierte Zaehlerindex aus dem SML-Deskriptor
- `smlWrite` und `smlSetWStr` akzeptieren entweder ein `char[]`-Array oder ein Zeichenketten-Literal — der Compiler erkennt automatisch welche Variante verwendet wird
- `smlWrite` sendet eine hex-kodierte Bytesequenz (z.B. `"AA0100"`) an den seriellen Port des Zaehlers
- `smlRead` kopiert den rohen Empfangspuffer in ein char-Array fuer benutzerdefiniertes Parsen
- `smlSetBaud` aendert dynamisch die Baudrate des Zaehlers (nuetzlich fuer Zaehler, die Geschwindigkeitsverhandlung erfordern)
- `smlSetWStr` setzt eine Hex-Zeichenkette, die beim naechsten geplanten Zaehlerabfragezyklus gesendet wird
- Diese Funktionen ersetzen Scripter's `>F`/`>S`-Abschnitt-Zaehlersteuerungsbefehle

**Beispiel — OBIS-Zaehler-Aufwecksequenz:**
```c
void EverySecond() {
    // Zeichenketten-Literal — kein temporaerer Puffer noetig
    smlWrite(1, "2F3F210D0A");  // "/?!\r\n" in Hex
}
```

**Beispiel — Dynamische Baudratenverhandlung:**
```c
void EverySecond() {
    // Zaehlerantwort lesen
    char buf[64];
    int n = smlRead(1, buf);
    if (n > 0 && buf[0] == 0x06) {
        // ACK empfangen, auf hohe Geschwindigkeit umschalten
        smlSetBaud(1, 9600);
    }
}
```

#### SML-Deskriptor-Editor (IDE)

Die IDE enthaelt einen **SML-Deskriptor**-Tab im linken Bereich zur Verwaltung von Zaehlerdefinitionen:

- **Zaehler-Datenbank**: Ein Dropdown laedt `.tas`-Zaehlerdefinitionen aus der [Community-Datenbank](https://github.com/ottelo9/tasmota-sml-script)
- **Benutzerdefinierte Zaehler-URL**: Die Datenbank-URL wird aus `/sml_meter_url.txt` auf dem Geraetedateisystem gelesen. Um ein anderes Zaehler-Repository zu verwenden, bearbeiten Sie diese Datei mit einer URL, die auf ein Verzeichnis mit einer `smartmeter.json`-Indexdatei zeigt. Die Standard-URL verweist auf das Community-GitHub-Repository.
- **RX/TX-Pin-Auswahl**: Dropdowns werden aus den freien GPIOs des Geraets befuellt (ueber `freegpio`-API)
- **Pin-Platzhalter**: `%0rxpin%` und `%0txpin%` in Deskriptoren werden beim Speichern durch die ausgewaehlten Pins ersetzt
- **Auf Geraet speichern**: Extrahiert nur den `>M`-Abschnitt und speichert ihn als `/sml_meter.def`
- **Von Geraet laden**: Liest die aktuelle `/sml_meter.def` vom Geraet

#### Callback-Zusammenfuehrung

Viele `.tas`-Zaehlerdateien erfordern periodischen Code (Scripter's `>S`- und `>F`-Abschnitte) fuer Zaehlerkommunikation, Aufwecksequenzen oder Baudratenverhandlung. In TinyC schreiben Sie diese direkt als Callback-Funktionen im SML-Editor:

```
void EverySecond() {
    smlWrite(1, "2F3F210D0A");
}

>M 1
+1,3,s,16,9600,SML,1
1,1-0:1.8.0*255(@1,Energy In,kWh,E_in,3
#
```

**Funktionsweise:**
1. Schreiben Sie TinyC-Callback-Funktionen (`EverySecond()`, `Every100ms()` usw.) an beliebiger Stelle im SML-Editor — vor oder nach dem `>M`-Abschnitt
2. Beim **Speichern** geht nur der `>M`-Abschnitt als `/sml_meter.def` auf das Geraet
3. Beim **Kompilieren** fuehrt die IDE automatisch SML-Callbacks in das Hauptprogramm zusammen:
   - Wenn der Haupteditor bereits denselben Callback hat — wird der SML-Code an den bestehenden Funktionsrumpf angefuegt
   - Wenn der Haupteditor ihn nicht hat — wird eine neue Callback-Funktion erstellt
4. Der zusammengefuehrte Quellcode wird als ein Programm kompiliert — SML-Code und Hauptcode teilen sich dieselben Globalen und Funktionen

### SPI-Bus

Direkter SPI-Bus-Zugriff fuer Sensoren und Displays. Unterstuetzt sowohl Hardware-SPI (unter Verwendung der von Tasmota konfigurierten Pins) als auch Software-Bitbang auf beliebigen GPIO-Pins.

| Funktion | Beschreibung |
|----------|-------------|
| `int spiInit(int sclk, int mosi, int miso, int speed_mhz)` | SPI-Bus initialisieren. Gibt 1=ok zurueck |
| `spiSetCS(int index, int pin)` | Chip-Select-Pin fuer Slot-Index (1–4) setzen |
| `int spiTransfer(int cs, char buf[], int len, int mode)` | Bytes uebertragen. Gibt uebertragene Bytes zurueck |

**`spiInit`-Pin-Modi:**
- `sclk = -1` — Tasmota's primaeren Hardware-SPI-Bus verwenden (GPIO in Tasmota konfiguriert)
- `sclk = -2` — HSPI sekundaeren Hardware-SPI-Bus verwenden (nur ESP32)
- `sclk >= 0` — Bitbang-Modus mit GPIO-Pins (`sclk`, `mosi`, `miso`)
- Setzen Sie `mosi` oder `miso` auf -1, wenn nicht benoetigt (z.B. Nur-Lesen- oder Nur-Schreiben-Geraet)
- `speed_mhz` setzt die Taktfrequenz fuer Hardware-SPI (wird fuer Bitbang ignoriert)

**`spiTransfer`-Modi:**
| Modus | Beschreibung |
|-------|-------------|
| 1 | 8-Bit pro Element — jedes `buf[]`-Element = 1 uebertragenes Byte |
| 2 | 16-Bit pro Element — jedes `buf[]`-Element = 2 Bytes (MSB zuerst) |
| 3 | 24-Bit pro Element — jedes `buf[]`-Element = 3 Bytes (MSB zuerst) |
| 4 | 8-Bit mit CS-Umschaltung pro Byte — CS geht fuer jedes Byte Low/High |

**Hinweise:**
- Der `cs`-Parameter ist ein 1-basierter CS-Slot-Index (entsprechend `spiSetCS`). Verwenden Sie 0 fuer keine automatische CS-Verwaltung
- Die Uebertragung ist Vollduplex: `buf[]` wird geschrieben (MOSI) und gelesene Werte (MISO) ersetzen jedes Element
- Die maximale praktische Uebertragungslaenge ist durch Ihre char-Array-Groesse begrenzt
- SPI-Ressourcen werden automatisch bereinigt, wenn die VM stoppt
- Hardware-SPI erfordert in Tasmota konfigurierte SPI-Pins (Template- oder Modul-Einstellungen)

**Beispiel — MAX31855-Thermoelement lesen (SPI, 32-Bit-Lesung):**
```c
#define CS_PIN  5

int main() {
    spiInit(-1, -1, -1, 4);   // HW-SPI bei 4 MHz
    spiSetCS(1, CS_PIN);       // CS-Slot 1 = Pin 5

    char buf[4];
    buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0;
    spiTransfer(1, buf, 4, 1); // 4 Bytes lesen

    // MAX31855: Bits 31..18 = 14-Bit Thermoelement-Temperatur
    int raw = ((buf[0] << 8) | buf[1]) >> 2;
    if (raw & 0x2000) raw = raw - 16384;  // Vorzeichenerweiterung
    float temp = (float)raw * 0.25;

    char out[64];
    sprintf(out, "Thermocouple: %.2f °C\n", temp);
    printString(out);
    return 0;
}
```

### TWAI / CAN-Bus (ESP32)

Der ESP32 hat einen eingebauten TWAI-Controller (Two-Wire Automotive
Interface, elektrisch identisch zu CAN 2.0). Die meisten ESP32-S3 /
ESP32-C3 / ESP32-C6 Boards koennen ihn ueber beliebige zwei GPIOs
via GPIO-Matrix exponieren. Ein 3.3 V CAN-Transceiver (SN65HVD230,
TCAN332, TJA1051 mit Level-Shifter, ...) ist zwingend zwischen MCU
und dem Differential-Bus-Paar (CAN_H / CAN_L) erforderlich.

| Funktion | Beschreibung |
|----------|-------------|
| `int twaiBegin(int rx_pin, int tx_pin, int kbits, int mode)` | TWAI-Treiber installieren + starten. `kbits` aus {10, 25, 50, 100, 125, 250, 500, 800, 1000}. `mode`: 0=NORMAL (ACK auf Bus), 1=NO_ACK (Self-Test mit Loopback-Jumper fuer Software-Validierung ohne Transceiver). Returns 0=ok, -1=err |
| `twaiEnd()` | Treiber stoppen, Pins freigeben. Vor jedem erneuten `twaiBegin()` notwendig (Treiber ist Single-Instance) |
| `int twaiAvailable()` | Anzahl RX-Frames in der Treiber-Queue. 0 wenn leer |
| `int twaiRecv(int meta[], char data[], int max_dlc)` | Einen RX-Frame abholen. `meta[0]=ID, meta[1]=ext_flag, meta[2]=dlc`. `data[0..dlc-1]` mit Payload gefuellt. Returns Anzahl Payload-Bytes (0 = Queue leer, <0 = Treiber-Fehler) |
| `int twaiSend(int id, char data[], int dlc, int ext_flag)` | Einen Frame senden. Returns 0=ok, -1=err. `ext_flag=0` Standard-11-Bit-ID, `=1` Extended-29-Bit |
| `int twaiStatus(int counters[])` | Snapshot des Treiber-Zustands. Fuellt `counters[]` mit `[state, tx_err, rx_err, tx_failed, rx_missed, arb_lost, bus_err]`. Returns Status: 0=gestoppt, 1=laeuft, 2=bus-off, 3=recovering |
| `int twaiFilter(int id_acc, int id_mask, int ext_flag)` | Acceptance-Filter setzen. `id_acc` matcht `RX_ID & ~id_mask`; `id_mask=0` akzeptiert nur die exakte ID, `id_mask=0x1FFFFFFF` akzeptiert alles. Muss VOR `twaiBegin()` aufgerufen werden. Returns 0=ok |

**Hinweise:**
- Mode 1 (NO_ACK) dient nur dem Software-Bring-up — der Controller treibt TX, erwartet aber keinen ACK. Mit TX→RX-Jumper am selben MCU laesst sich der Protokoll-Stack ohne Transceiver validieren.
- Nach Bus-Off (Status 2) `twaiEnd()` + `twaiBegin()` aufrufen zum Reset.
- **ESP32-C3 GPIO 9** ist ein BOOT-Strap-Pin mit Pull-up. Funktioniert als TWAI-TX, aber **NICHT** als TWAI-RX (Strap haelt die Leitung dominant). RX auf einen Non-Strap-Pin legen (10, 18, 19, ...).

**Beispiel — Sniffer der jeden Frame loggt:**
```c
int rx_meta[4];
char rx_data[8];
int rx_total = 0;

void EveryLoop() {
    while (twaiAvailable() > 0) {
        int n = twaiRecv(rx_meta, rx_data, 8);
        if (n <= 0) break;
        rx_total = rx_total + 1;
        char ext = rx_meta[1] ? 'E' : 'S';
        addLog("CAN RX #%d %cID=0x%X DLC=%d  %02X %02X %02X %02X %02X %02X %02X %02X",
            rx_total, ext, rx_meta[0], rx_meta[2],
            rx_data[0], rx_data[1], rx_data[2], rx_data[3],
            rx_data[4], rx_data[5], rx_data[6], rx_data[7]);
    }
}

int main() {
    twaiBegin(38, 39, 250, 0);   // rx=38, tx=39, 250 kbit/s, NORMAL
    addLog("CAN-Sniffer bereit");
    return 0;
}
```

Vollstaendige SLCAN-ueber-TCP-Bridge: `examples/slcan_bridge_tcp.tc`.

### Display-Zeichnung

Erfordert einen Tasmota-Build mit aktiviertem `USE_DISPLAY` und einem konfigurierten Display-Treiber. Alle Zeichenfunktionen arbeiten direkt auf dem Tasmota-Display-Renderer — viel effizienter als das Erstellen von DisplayText-Befehlszeichenketten.

#### Einrichtung & Steuerung

| Funktion | Beschreibung |
|----------|-------------|
| `dspClear()` | Display loeschen, Position auf (0,0) zuruecksetzen |
| `dspPos(x, y)` | Aktuelle Zeichenposition setzen (Pixel) |
| `dspFont(f)` | Schriftart setzen (0-7), setzt Textgroesse auf 1 fuer Nicht-GFX-Schriften zurueck |
| `dspSize(s)` | Textgroessen-Multiplikator setzen |
| `dspColor(fg, bg)` | Vordergrund- und Hintergrundfarbe setzen (16-Bit RGB565) |
| `dspPad(n)` | Text-Auffuellung fuer `dspDraw()` setzen: positiv = linksbuendig aufgefuellt auf n Zeichen, negativ = rechtsbuendig aufgefuellt auf n Zeichen, 0 = aus |
| `dspDim(val)` | Display-Helligkeit setzen (0-15) |
| `dspOnOff(on)` | Display ein- (1) oder ausschalten (0) |
| `dspUpdate()` | Display-Aktualisierung erzwingen (erforderlich fuer E-Paper-Displays) |
| `dspWidth()` | Gibt Display-Breite in Pixeln zurueck |
| `dspHeight()` | Gibt Display-Hoehe in Pixeln zurueck |

#### Zeichenprimitiven

Alle Primitiven verwenden die aktuelle Position, die durch `dspPos()` gesetzt wurde, und die aktuelle Vordergrundfarbe, die durch `dspColor()` gesetzt wurde.

| Funktion | Beschreibung |
|----------|-------------|
| `dspDraw(buf)` | Textzeichenkette an aktueller Position zeichnen |
| `dspPixel(x, y)` | Einzelnen Pixel an (x,y) zeichnen |
| `dspLine(x1, y1)` | Linie von aktueller Position zu (x1,y1) zeichnen, aktualisiert Position |
| `dspHLine(w)` | Horizontale Linie von aktueller Position, Breite w, aktualisiert Position |
| `dspVLine(h)` | Vertikale Linie von aktueller Position, Hoehe h, aktualisiert Position |
| `dspRect(w, h)` | Rechteckumriss an aktueller Position zeichnen |
| `dspFillRect(w, h)` | Gefuelltes Rechteck an aktueller Position zeichnen |
| `dspCircle(r)` | Kreisumriss an aktueller Position mit Radius r zeichnen |
| `dspFillCircle(r)` | Gefuellten Kreis an aktueller Position zeichnen |
| `dspRoundRect(w, h, r)` | Abgerundetes Rechteck an aktueller Position mit Eckenradius r |
| `dspFillRoundRect(w, h, r)` | Gefuelltes abgerundetes Rechteck |
| `dspTriangle(x1, y1, x2, y2)` | Dreieck von aktueller Position zu (x1,y1) und (x2,y2) |
| `dspFillTriangle(x1, y1, x2, y2)` | Gefuelltes Dreieck |

#### Bild & Rohbefehle

| Funktion | Beschreibung |
|----------|-------------|
| `dspPicture("file.jpg", scale)` | Bilddatei vom Dateisystem an aktueller Position zeichnen (scale: 0=Original) |
| `int dspLoadImage("file.jpg")` | JPG in PSRAM als RGB565-Pixelspeicher laden, gibt Slot 0-3 zurueck (-1 bei Fehler). Bleibt im Speicher bis VM stoppt. Nur ESP32+JPEG_PICTS |
| `dspPushImageRect(slot, sx, sy, dx, dy, w, h)` | Teilrechteck aus geladenem Bild auf Bildschirm zeichnen. Liest aus Bild bei (sx,sy), schreibt auf Bildschirm bei (dx,dy), Groesse w×h. Fuer Hintergrund-Wiederherstellung (z.B. Uhrzeiger ueber Zifferblatt) |
| `int dspImageWidth(slot)` | Breite des geladenen Bildes im Slot abfragen (0 bei ungueltigem Slot) |
| `int dspImageHeight(slot)` | Hoehe des geladenen Bildes im Slot abfragen (0 bei ungueltigem Slot) |
| `int dspTextWidth(len)` | Pixelbreite fuer `len` Zeichen im aktuellen Font und Textgroesse. Fuer transparenten Text auf Bildhintergrund: Text messen, zeichnen, spaeter Hintergrund mit `dspPushImageRect` wiederherstellen |
| `int dspTextHeight()` | Pixelhoehe fuer aktuellen Font und Textgroesse |
| `dspImgText(slot, x, y, color, fieldWidth, align, text)` | Text auf ein Bild-Teilrechteck im RAM zusammensetzen und das Ergebnis in einer einzigen SPI-Transaktion uebertragen (flimmerfrei). Der Bildpuffer liefert die Hintergrundpixel; nur Vordergrund-Fontpixel werden ueberschrieben. `slot`: Bild-Slot von `dspLoadImage()`. `x, y`: Pixelposition auf dem Bild (und Bildschirm). `color`: RGB565-Textfarbe. `fieldWidth`: Gesamtfeldbreite in Zeichen — wenn groesser als Textlaenge, zeigt der Rest den Bildhintergrund; 0 fuer automatisch (passt genau zum Text). `align`: 0=links, 1=rechts, 2=zentriert (Ausrichtung innerhalb des Feldes). `text`: der darzustellende String. Funktioniert mit EPD-Fonts 1-4 (gesetzt via `dspText("[f1]")`..`dspText("[f4]")`) bei jeder Textgroesse. Beispiel: `dspText("[f2s1]"); dspImgText(img, 10, 10, 0, 28, 0, buf);` |
| `int dspLoadImageFromCam(cam_slot)` | JPEG aus einem PSRAM **Cam-Slot** (1-4, gefuellt ueber `camControl(10, ...)`) in einen freien RGB565 **Bild-Slot** (0-3) dekodieren. Gibt den neuen Bild-Slot zurueck, -1 bei Fehler. Der Quell-Cam-Slot bleibt unveraendert. Nur ESP32+JPEG_PICTS+Kamera |
| `void dspFreeImage(img_slot)` | Einen RGB565-**Bild-Slot** (0-3) freigeben, der zuvor mit `dspLoadImage` / `dspLoadImageFromCam` geladen wurde. In einer Per-Frame-Kameraschleife unverzichtbar, damit die 4 Bild-Slots nicht ausgehen: dekodieren → anzeigen/`lvglCanvasSetImgSlot` → den Slot des Vorframes freigeben. |
| `dspImgTextBurn(slot, x, y, color, fieldWidth, align, text)` | Glyphen-Pixel direkt **in** den Bildpuffer schreiben — im Gegensatz zu `dspImgText` wird das Display NICHT angefasst. Fuer kopflose Cam-Boards (kein TFT angeschlossen), um Zeitstempel/Label in ein Frame zu brennen, bevor es neu kodiert wird. Faellt auf Font12 Groesse 1 zurueck, wenn kein Renderer aktiv ist; bei vorhandenem Display werden aktueller Font und Groesse respektiert (`dspText("[f2s1]")` etc.). Parameter identisch zu `dspImgText` |
| `int dspImageToCam(img_slot, cam_slot, quality)` | RGB565-Bild-Slot zurueck in einen Cam-Slot als JPEG kodieren (ueber esp32-camera `fmt2jpg`). `quality` 1..63 (esp_camera-Konvention, niedriger=besser; 12 ≈ JPEG Q=85). Gibt kodierte Byte-Groesse zurueck, -1 bei Fehler. Das Ergebnis ist sofort einsatzbereit fuer `camControl(11, cam_slot, fh)` (Datei speichern), Mail-Anhang oder jeden anderen Cam-Slot-Konsument |
| `dspText(buf)` | Rohen DisplayText-Befehl ausfuehren (z.B. `"[z][x50][y20]Hello"`) |

#### Vordefinierte Farbkonstanten (RGB565)

Die folgenden Farbkonstanten sind **vordefiniert** — kein `#define` noetig:

| Konstante | Wert | Konstante | Wert |
|-----------|------|-----------|------|
| `BLACK` | 0 | `WHITE` | 65535 |
| `RED` | 63488 | `GREEN` | 2016 |
| `BLUE` | 31 | `YELLOW` | 65504 |
| `CYAN` | 2047 | `MAGENTA` | 63519 |
| `ORANGE` | 64800 | `PURPLE` | 30735 |
| `GREY` | 33808 | `DARKGREY` | 21130 |
| `LIGHTGREY` | 50712 | `DARKGREEN` | 992 |
| `NAVY` | 16 | `MAROON` | 32768 |
| `OLIVE` | 33792 | | |

Benutzerdefinierte `#define`-Ueberschreibungen haben Vorrang vor vordefinierten Farben.

#### Beispiel

```c
int counter;
char buf[32];

void EverySecond() {
    counter++;

    dspClear();
    dspColor(WHITE, BLACK);    // Weiss auf Schwarz

    // Titel
    dspFont(2);
    dspSize(2);
    dspPos(10, 10);
    dspDraw("TinyC Display");

    // Zaehler
    dspFont(1);
    dspSize(1);
    sprintf(buf, "Count: %d", counter);
    dspPos(10, 60);
    dspDraw(buf);

    // Roten Rahmen um den Zaehler zeichnen
    dspColor(RED, BLACK);
    dspPos(5, 55);
    dspRect(150, 25);

    // Blauen gefuellten Kreis zeichnen
    dspColor(BLUE, BLACK);
    dspPos(200, 80);
    dspFillCircle(20);

    dspUpdate();  // noetig fuer E-Paper
}

int main() {
    counter = 0;
    dspClear();
    return 0;
}
```

### Touch-Buttons & Slider

GFX-Touch-Buttons und Slider auf dem Display erstellen. Diese Komfortfunktionen formatieren intern `[b...]` und `[bs...]` DisplayText-Befehle.

#### Button-Erstellung

| Funktion | Beschreibung |
|----------|-------------|
| `dspButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Power-Button erstellen (steuert Relais `num`) |
| `dspTButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Virtuellen Toggle-Button erstellen (MQTT TBT) |
| `dspPButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Virtuellen Push-Button erstellen (MQTT PBT) |
| `dspSlider(num, x, y, w, h, nelem, bg, fc, bc)` | Slider erstellen |

Parameter: `num` = Button-Index (0-15), `x,y` = Position, `w,h` = Groesse, `oc` = Umrissfarbe, `fc` = Fuellfarbe, `tc` = Textfarbe, `ts` = Textgroesse, `nelem` = Slider-Segmente, `bg` = Hintergrundfarbe, `bc` = Balkenfarbe.

#### Zustand setzen & lesen

| Funktion | Beschreibung |
|----------|-------------|
| `dspButtonState(num, val)` | Button-Zustand setzen (0/1) oder Slider-Wert (0-100) |
| `int touchButton(num)` | Button-Zustand lesen: 0/1 fuer Buttons, -1 wenn undefiniert |
| `dspButtonDel(num)` | Button/Slider `num` loeschen, oder alle wenn `num` = -1 |

#### Touch-Callback

Der `TouchButton`-Callback wird bei Touch-Ereignissen mit Button-Index und Wert aufgerufen:

```c
void TouchButton(int btn, int val) {
    if (btn == 0) {
        // Toggle-Button gedrueckt, val = 0 oder 1
        char buf[16];
        sprintf(buf, "%d", val);
        tasmCmd("Power1", buf);
    }
    if (btn == 1) {
        // Slider bewegt, val = 0-100
        char buf[16];
        sprintf(buf, "%d", val);
        tasmCmd("Dimmer", buf);
    }
}

int main() {
    dspTButton(0, 10, 10, 100, 50, WHITE, BLUE, WHITE, 2, "Light");
    dspSlider(1, 10, 80, 200, 40, 10, DARKGREY, WHITE, CYAN);
    return 0;
}
```

### TinyUI — Retained-Mode-Widget-Schicht

Eine duenne Retained-Mode-UI-Schicht auf den primitiven `dsp*`-Aufrufen. Bietet:

* **Bildschirme** — bis zu 256 logische Screens; Umschalten loescht die Zeichenflaeche, entfernt interaktive Widgets und zeichnet passive Widgets des neuen Screens neu.
* **Theme** — eine einzige globale Farb-/Padding-Palette, die auf alle Widgets angewendet wird.
* **Passive Widgets** (`uiLabel`, `uiProgress`, `uiGauge`) — in einem separaten Pool (`tc_ui_widgets[TC_UI_MAX_WIDGETS]`, 16 Eintraege per Default). Sie ueberdauern `uiScreen()`-Wechsel und werden automatisch neu gezeichnet.
* **Interaktive Widgets** (`uiCheckbox`, `uiIcon`) — auf dem bestehenden VButton-Pool (`MAX_TOUCH_BUTTONS` Eintraege). Ereignisse laufen ueber den normalen `TouchButton(num, state)`-Callback. **Eigener Indexraum, getrennt von passiven Widgets.**

TinyUI ist wirklich klein: ~400 Zeilen C, kein zusaetzlicher RAM im unbenutzten Zustand, keine zusaetzlichen Abhaengigkeiten, und verwendet den bestehenden Display-Renderer. Im Vergleich zu LVGL (~150–500 KB Flash, 10–30 KB RAM).

#### API

| Funktion | Beschreibung |
|---|---|
| `uiScreen(int id)` | Zu Screen `id` (0..255) wechseln. Fuellt Canvas mit `theme.bg`, loescht alle VButtons, zeichnet passive Widgets des neuen Screens neu. Danach eigene `build_screenN()` aufrufen, um interaktive Widgets wiederherzustellen. |
| `uiTheme(bg, accent, text, border)` | Globale Palette setzen (RGB565). Wird von danach erstellten Widgets verwendet. |
| `uiClearScreen()` | Canvas mit `theme.bg` fuellen. |
| `uiLabel(num, x, y, w, h, "text", align)` | Passives Textlabel im Widget-Pool-Slot `num` (0..15). `align`: `-1`=rechts, `0`=zentriert, `1`=links. |
| `uiLabelSet(num, "text")` oder `uiLabelSet(num, buf)` | Label-Text aktualisieren und neu zeichnen. Akzeptiert Stringliteral oder `char[]`-Puffer. |
| `uiProgress(num, x, y, w, h, value, max)` | Horizontaler Fortschrittsbalken. Bereich `0..max`. |
| `uiProgressSet(num, value)` | Balkenwert aktualisieren + neu zeichnen. |
| `uiGauge(num, x, y, r, value, vmin, vmax)` | 240°-Skala zentriert bei `x,y`, Radius `r`. Erneuter Aufruf mit gleichem `num` bewegt die Nadel. |
| `uiCheckbox(num, x, y, w, h, "label")` | Interaktive rastende Toggle-Checkbox ueber VButton-Slot `num`, Trefferflaeche vom Aufrufer bestimmt (`w` × `h`, Minimum 8×8). Ein `TouchButton(num, state)` pro Tap, `state` = neuer rastender Wert (0/1). |
| `uiButton(num, x, y, w, h, "label")` | Tastender Momentantaster im VButton-Slot `num` (gleiche Trefferflaechen-Regel wie `uiCheckbox`). Feuert `TouchButton(num, 1)` beim Druecken und `TouchButton(num, 0)` beim Loslassen — fuer Ausloeseaktionen (Puls, Klingel, Weiter). |
| `uiIcon(num, x, y, img_slot)` | *(reserviert)* bildbasiertes Icon — Verdrahtung zum Image-Slot-System folgt. |

Passive Widgets (Label/Progress/Gauge) verwenden einen Indexraum (0..15). Checkboxes / Pushbuttons / Icons teilen sich den VButton-Indexraum (0..MAX_TOUCH_BUTTONS-1). Die beiden Indexraeume kollidieren **nicht** miteinander.

#### Beispiel

```c
int current = 1;
float power = 0;

void build_screen1() {
    uiLabel(0,   0,  0, 320, 30, "Dashboard",   0);
    uiLabel(1,  10, 50, 150, 20, "Power:  0 W", 1);
    uiProgress(3, 10, 80, 300, 18, 0, 1000);
}

void main() {
    uiTheme(0x0000, 0x07FF, 0xFFFF, 0x39E7);  // bg, accent, text, border
    uiScreen(1);
    build_screen1();
}

void EverySecond() {
    power = power + 50; if (power > 1000) power = 0;
    char buf[32];
    sprintfFloat(buf, "Power: %.0f W", power);
    uiLabelSet(1, buf);
    uiProgressSet(3, power);
}
```

Siehe `examples/tinyui_demo.tc` fuer ein 3-Screen-Demo mit Live-Werten, Arc-Gauge und interaktiven Checkboxes.

#### Compile-time-Limits

| Konstante | Default | Zweck |
|---|---|---|
| `TC_UI_MAX_WIDGETS` | 16 | Passiver Widget-Pool (`tc_ui_widgets[]`) |
| `MAX_TOUCH_BUTTONS` | 16 | Interaktiver VButton-Pool (geteilt mit `dspButton/dspTButton/…`) |

### Audio

| Funktion | Beschreibung |
|---|---|
| `audioVol(int vol)` | Audiolautstaerke setzen (0-100) |
| `audioPlay("file.mp3")` | MP3-Datei vom Dateisystem abspielen |
| `audioSay("hello")` | Text-zu-Sprache-Ausgabe |
| `audioMicGain(int gain)` | Mikrofon-Eingangsverstaerkung (1-100) fuer den Codec-ADC (ES7210/ES8311) des Audio-Plugins setzen — das Eingangs-Pendant zu `audioVol`. Laeuft ueber das Audio-BinPlugin, also der Weg, die Mic-Verstaerkung auf Boards zu setzen, wo das Plugin den Codec besitzt (z. B. P4). |

Erfordert einen auf dem Geraet konfigurierten I2S-Audiotreiber.

**Plugin-Mikrofon (Boards mit geteiltem Codec, z. B. P4).** Wo das Audio-Plugin den Codec besitzt
(der Vollduplex-ES8311 + ES7210 des P4), kann der native `i2sMic*`-Pfad weiter unten keinen zweiten
I2S-Kanal oeffnen. Stattdessen die Verstaerkung mit `audioMicGain(gain)` setzen und die Live-Lautheit
ueber `pluginQuery(buf, 42, 0, 10)` lesen — Audio-Plugin **Modul 42, Selektor 10 = Mic-Pegel** (siehe
`examples/mic_plugin_level.tc`). Hinweis: gleichzeitiges Abspielen **und** Mic wird auf diesem
Vollduplex-Codec noch nicht unterstuetzt — es ist ein Pegel-/Listen-Pfad.

```c
audioVol(50);              // Lautstaerke auf 50% setzen
audioPlay("/alarm.mp3");   // MP3-Datei abspielen
audioSay("sensor alert");  // Text sprechen
```

#### Rohe I2S-Ausgabe

Zugriff auf niedrigerer Ebene auf einen I2S-DAC/-Verstaerker, um eigene
PCM-Samples zu streamen (z. B. eine WAV-Datei stueckweise abzuspielen).

| Funktion | Beschreibung |
|----------|-------------|
| `int i2sBegin(int mclk, int bclk, int lrclk, int dout, int sampleRate)` | I2S-TX-Pins und Abtastrate (Hz) konfigurieren. `mclk=-1` für einen reinen I2S-Verstärker (MAX98357A/PCM5102); ein Codec-DAC (ES8311/WM8960) braucht den MCLK-Pin (256·fs) **und** seine per I2C aus dem Skript gesetzten Register. Gibt 0 bei Erfolg, -1 bei Fehler zurück. |
| `int i2sWrite(int[] pcm, int frames)` | `frames` 16-Bit-Stereo-PCM-Samples aus `pcm` an den I2S-Bus schreiben (blockiert bis eingereiht). Liefert geschriebene Frames. |
| `i2sStop()` | I2S-Treiber und Pins freigeben. |
| `int fileReadPCM16(int handle, int[] pcm, int frames, int wavChannels)` | Liest bis zu `frames` 16-Bit-Samples aus einer offenen WAV-Datei in `pcm` (Stereo→Mono-Mischung wenn `wavChannels == 2`). Gibt gelesene Frames zurück (0 bei EOF). Passt zu `i2sWrite()`. |
| `int i2sMicBegin(int mclk, int bclk, int lrclk, int din, int sampleRate)` | Öffnet einen I2S-RX-(Mikrofon-)Kanal als Master. `mclk=-1` für ein reines MEMS-/PDM-Mikro; ein Codec-ADC (ES7210) braucht MCLK (256·fs) + per I2C gesetzte Register. Gibt 0 bei Erfolg, -1 bei Fehler zurück. |
| `int i2sMicRead(int[] buf, int max)` | Liest bis zu `max` 16-Bit-Mono-Samples vom Mikrofon in `buf`. Gibt die gelesene Anzahl zurück. |
| `int i2sMicLevel()` | RMS-Lautstärke 0..32767 über einen ~256-Sample-Block — ein günstiger Pegelmesser, kein Puffer nötig. |
| `void i2sMicStop()` | Stoppt den Mikrofon-RX-Kanal und gibt ihn frei. |
| `int i2sDuplexBegin(int mclk, int bclk, int ws, int dout, int din, int sampleRate)` | Öffnet EIN Vollduplex-I2S-Kanalpaar (TX+RX, gemeinsamer Takt). Nötig für einen kombinierten Codec (z. B. WM8960), dessen ADC vom I2S-TX getaktet wird — ein separater `i2sMicBegin`-RX-Kanal bekommt keinen Takt und bleibt stumm. Danach spielt `i2sWrite()` auf dem TX ab und `i2sMicLevel()`/`i2sMicRead()` lesen das Mikrofon **gleichzeitig**. Stop mit `i2sStop()` + `i2sMicStop()`. Gibt 0 bei Erfolg, -1 bei Fehler zurück. |

### Persistente Variablen

| Funktion | Beschreibung |
|---|---|
| `saveVars()` | Alle `persist` Globals in die `.pvs`-Datei des Programms speichern |

Persist-Variablen werden automatisch beim Programmstart geladen und beim `TinyCStop` gespeichert. Verwenden Sie `saveVars()` um an kritischen Stellen zu speichern (z.B. nach Mitternachts-Zaehleraktualisierung).

### Watch-Variablen (Aenderungserkennung)

| Funktion | Beschreibung |
|---|---|
| `changed(var)` | Gibt 1 zurueck wenn Watch-Variable vom Schattenwert abweicht |
| `delta(var)` | Gibt aktuell - Schatten zurueck (int oder float je nach Variablentyp) |
| `written(var)` | Gibt 1 zurueck wenn Variable seit letztem `snapshot()` zugewiesen wurde |
| `snapshot(var)` | Schattenwert aktualisieren und Written-Flag loeschen |

Watch-Variablen sind Compiler-Intrinsics — sie erzeugen Inline-Vergleichscode ohne Laufzeit-Overhead (kein Syscall).

### Deep Sleep (ESP32)

| Funktion | Beschreibung |
|---|---|
| `deepSleep(int sekunden)` | Tiefschlaf mit Timer-Aufwachen nach `sekunden` |
| `deepSleepGpio(int sekunden, int pin, int level)` | Tiefschlaf mit Timer + GPIO-Aufwachen (0=low, 1=high) |
| `int wakeupCause()` | Gibt ESP32-Aufwachgrund zurueck (0=Reset, 2=EXT0, 3=EXT1, 4=Timer, ...) |

Persistente Variablen und Einstellungen werden vor dem Tiefschlaf automatisch gespeichert.

```c
// Alle 5 Minuten aufwachen zum Sensor-Ablesen
int cause = wakeupCause();
if (cause == 4) {
    // Timer-Aufwachen — Sensor lesen, Daten senden
}
deepSleep(300);  // 300 Sekunden schlafen

// Schlafen bis GPIO12 HIGH wird (oder max 1 Stunde)
deepSleepGpio(3600, 12, 1);
```

### Hardware-Register

Direkt auf Peripherie-Register des ESP32 zugreifen (fuer Low-Level-Treiber und Debugging).

| Funktion | Beschreibung |
|----------|-------------|
| `int peekReg(int addr)` | 32-Bit-Wert aus Peripherie-Register lesen |
| `pokeReg(int addr, int val)` | 32-Bit-Wert in Peripherie-Register schreiben |

### Email (ESP32 — benoetigt USE_SENDMAIL)

| Funktion | Beschreibung |
|---|---|
| `mailBody(body)` | E-Mail-Text setzen (HTML). `body` ist ein `char[]`-Array |
| `mailAttach("/pfad")` | Dateianhang vom Dateisystem hinzufuegen (String-Literal, bis zu 8) |
| `int mailSend(params)` | E-Mail senden. `params` ist `char[]` mit `[server:port:user:passwd:von:an:betreff]`. Gibt 0=ok zurueck |

Fuer einfache E-Mails ohne Anhaenge den Text nach `]` in params setzen:
```c
char cmd[200];
strcpy(cmd, "[smtp.gmail.com:465:user:pass:von@x.com:an@y.com:Alarm] Sensor ausgeloest!");
int result = mailSend(cmd);
```

Fuer E-Mails mit Dateianhaengen `mailBody()` und `mailAttach()` vor `mailSend()` verwenden:
```c
// Text aufbauen
char body[200];
sprintf(body, "<h1>Tagesbericht</h1><p>Temperatur: %d C</p>", "%.1f");

// Text und Anhaenge registrieren
mailBody(body);
mailAttach("/daten.csv");
mailAttach("/log.txt");

// Senden — params nur [server:port:user:passwd:von:an:betreff]
char params[200];
strcpy(params, "[*:*:*:*:*:an@example.com:Tagesbericht]");
int result = mailSend(params);
// result: 0=ok, 1=Parse-Fehler, 4=Speicherfehler
```

`*` fuer Server/Port/User/Passwort/Von-Felder verwenden um `#define`-Standardwerte aus `user_config_override.h` zu nutzen.

### Tesla Powerwall (ESP32 — benoetigt TESLA_POWERWALL)

Zugriff auf die lokale Tesla Powerwall API ueber HTTPS. Verwendet die SSL-Implementierung der E-Mail-Bibliothek (Standard-Arduino-SSL funktioniert nicht mit Powerwall).

**Benoetigt:** `#define TESLA_POWERWALL` in `user_config_override.h` und die ESP-Mail-Client-Bibliothek.

| Funktion | Beschreibung |
|----------|-------------|
| `int pwlRequest(url)` | Konfigurationsbefehl oder API-Anfrage. Gibt 0=ok, -1=Fehler zurueck |
| `pwlBind(&var, pfad)` | Globale Float-Variable fuer Auto-Fill registrieren. Pfad mit `#`-Trenner (max 24 Bindings) |
| `float pwlGet(pfad)` | Float-Wert aus letzter Antwort. Unterstuetzt `[N]`-Suffix fuer N-tes Vorkommen |
| `int pwlStr(pfad, buf)` | String aus letzter Antwort in `char[]`-Puffer extrahieren. Gibt Laenge zurueck |

**Empfohlener Ansatz — `pwlBind` (einmal parsen, alle fuellen):**

Globale Variablen mit JSON-Pfaden in `Setup()` registrieren. Wenn `pwlRequest()` eine Antwort erhaelt, wird JSON **einmal** geparst und alle passenden gebundenen Variablen direkt gefuellt. Keine String-Ersetzungen, kein wiederholtes Parsen.

```c
float sip, sop, bip, hip, pwl, rper;

void Setup() {
    pwlRequest("@D192.168.188.60,email@example.com,meinpasswort");
    pwlRequest("@C0x000004714B006CCD,0x000004714B007969");

    // Bindings registrieren — originale JSON-Schluesselnamen verwenden
    pwlBind(&sip, "site#instant_power");
    pwlBind(&sop, "solar#instant_power");
    pwlBind(&bip, "battery#instant_power");
    pwlBind(&hip, "load#instant_power");
    pwlBind(&pwl, "percentage");
    pwlBind(&rper, "backup_reserve_percent");
}

void Loop() {
    // Alle passenden Bindings werden automatisch gefuellt:
    pwlRequest("/api/meters/aggregates");
    // sip, sop, bip, hip sind jetzt gesetzt

    pwlRequest("/api/system_status/soe");
    // pwl ist jetzt gesetzt

    pwlRequest("/api/operation");
    // rper ist jetzt gesetzt
}
```

**Konfigurations-Praefixe:**
| Praefix | Beschreibung |
|---------|-------------|
| `@Dip,email,passwort` | IP und Zugangsdaten konfigurieren |
| `@Ccts1,cts2` | CTS-Seriennummern konfigurieren (werden in Antworten maskiert) |
| `@N` | Auth-Cookie loeschen (erneute Authentifizierung erzwingen) |

**Haeufige API-Endpunkte:**
| Endpunkt | Daten |
|----------|-------|
| `/api/meters/aggregates` | Netz-, Batterie-, Haus-, Solarleistung (W) |
| `/api/system_status/soe` | Ladestand / Batterieprozent |
| `/api/system_status` | Systemstatus-Informationen |
| `/api/operation` | Betriebsmodus, Reserveprozent |
| `/api/meters/readings` | Detaillierte Zaehlerablesung pro CTS |

**N-tes Vorkommen:** `pwlGet("key[N]")` extrahiert das N-te Vorkommen eines wiederholten Schluessels aus der JSON-Antwort. Nuetzlich fuer `/api/meters/readings` mit mehreren CTS-Objekten mit gleichen Schluesselnamen:

```c
// Netzphasen — CTS2 Grid-Phasen sind Vorkommen 6,7,8 von "p_W"
phs1 = pwlGet("p_W[6]");
phs2 = pwlGet("p_W[7]");
phs3 = pwlGet("p_W[8]");
```

**Ad-hoc-Zugriff:** `pwlGet()` und `pwlStr()` stehen fuer einmalige Wertextraktion zur Verfuegung, aber `pwlBind()` wird fuer wiederholtes Polling bevorzugt, da es erneutes Parsen vermeidet.

### Adressierbare LED-Streifen (WS2812 — benoetigt USE_WS2812)

WS2812 / NeoPixel adressierbare LED-Streifen direkt aus TinyC steuern.

**Benoetigt:** `#define USE_WS2812` in `user_config_override.h`.

| Funktion | Beschreibung |
|----------|-------------|
| `setPixels(array, len, offset)` | Setzt `len` Pixel aus `array`, ab Strip-Position `offset & 0x7FF`. Aktualisiert den Strip sofort. |
| `int rgbLed(gpio, color)` | Steuert eine **einzelne** WS2812/NeoPixel an `gpio` mit gepacktem `0xRRGGBB`-`color` (`0` schaltet sie aus). Liefert 1 bei Erfolg, 0 bei Fehler. Der RMT-Treiber wird beim ersten Aufruf fuer diesen Pin angelegt. Praktisch fuer eine On-Board-Status-LED (z. B. GPIO8 auf einem ESP32-C6-Devboard) und vom Matter-Farblicht-Beispiel zum Darstellen von Hue/Saturation/Level genutzt. |

**Farbformat:** Jedes Array-Element (und `rgbLed`s `color`) ist `0xRRGGBB` (24-Bit RGB als Integer gepackt).

**RGBW-Modus:** Bit 12 von offset setzen (`offset | 0x1000`) fuer RGBW-Modus. Im RGBW-Modus kodieren zwei aufeinanderfolgende Array-Elemente ein Pixel (High-Word = `0x00RG`, Low-Word = `0xBW00`).

**Beispiel — Regenbogen-Effekt:**
```c
int leds[60];

void setup() {
    for (int i = 0; i < 60; i++) {
        int hue = (i * 256) / 60;
        leds[i] = hueToRGB(hue);
    }
    setPixels(leds, 60, 0);
}

int hueToRGB(int h) {
    int r, g, b;
    int region = h / 43;
    int remainder = (h - region * 43) * 6;
    switch (region) {
        case 0:  r = 255; g = remainder; b = 0; break;
        case 1:  r = 255 - remainder; g = 255; b = 0; break;
        case 2:  r = 0; g = 255; b = remainder; break;
        case 3:  r = 0; g = 255 - remainder; b = 255; break;
        case 4:  r = remainder; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - remainder; break;
    }
    return (r << 16) | (g << 8) | b;
}
```

---

### ESP Kamera (ESP32)

Kamera-Unterstuetzung fuer ESP32-Boards mit OV2640/OV3660/OV5640-Sensoren. Zwei Modi verfuegbar:

- **Tasmota Webcam-Treiber** (sel 0-7): Verwendet den Standard `USE_WEBCAM` Treiber. `USE_WEBCAM` in `user_config_override.h` definieren.
- **TinyC integrierte Kamera** (sel 8-18): Direkter esp_camera-Treiber mit boardspezifischen Pins, MJPEG-Streaming auf Port 81 und PSRAM-Slot-Verwaltung. `USE_TINYC_CAMERA` definieren (via `-DTINYC_CAMERA` Build-Flag). Keine `USE_WEBCAM` Abhaengigkeit.

Beide Modi unterstuetzen `mailAttachPic()` fuer E-Mail-Bildanhaenge (bis zu 4 Bilder pro E-Mail).

#### Kamera-Init mit eigenen Pins (TinyC integrierter Modus)

```c
// Pin-Reihenfolge: pwdn, reset, xclk, sda, scl, d7..d0, vsync, href, pclk
int campins[] = {-1, -1, 15, 4, 5, 16, 17, 18, 12, 10, 8, 9, 11, 6, 7, 13};
int ok = cameraInit(campins, PIXFORMAT_JPEG, FRAMESIZE_VGA, 12, 0, 0, -1);
```

| Funktion | Beschreibung |
|----------|-------------|
| `cameraInit(pins[], format, framesize, quality, fb_count, grab_mode, xclk_freq)` | Kamera mit Pin-Array initialisieren. Gibt 0=ok, sonst Fehler zurueck. `fb_count`=0 auto, `grab_mode`=0 auto, `xclk_freq`=-1 Standard 20MHz. |

#### Kamerasteuerung (camControl)

Alle Kamera-Operationen nutzen `camControl(sel, p1, p2)`:

**Tasmota Webcam-Treiber (sel 0-7, benoetigt USE_WEBCAM):**

| sel | Funktion | Beschreibung |
|-----|----------|-------------|
| 0 | `camControl(0, resolution, 0)` | Init ueber Tasmota-Treiber (WcSetup) |
| 1 | `camControl(1, bufnum, 0)` | Bild in Tasmota Pic-Buffer aufnehmen (1-4) |
| 2 | `camControl(2, option, wert)` | Optionen setzen (WcSetOptions) |
| 3 | `camControl(3, 0, 0)` | Breite abfragen |
| 4 | `camControl(4, 0, 0)` | Hoehe abfragen |
| 5 | `camControl(5, on_off, 0)` | Tasmota Stream-Server starten/stoppen |
| 6 | `camControl(6, param, 0)` | Bewegungserkennung (-1=Bewegung lesen, -2=Helligkeit lesen, ms=Intervall) |

**TinyC integrierte Kamera (sel 7-18, benoetigt USE_WEBCAM oder USE_TINYC_CAMERA):**

| sel | Funktion | Beschreibung |
|-----|----------|-------------|
| 7 | `camControl(7, bufnum, dateiHandle)` | Bild-Buffer in Datei speichern, gibt Bytes zurueck |
| 8 | `camControl(8, 0, 0)` | Sensor-PID abfragen (z.B. 0x2642 = OV2640, 0x3660 = OV3660) |
| 9 | `camControl(9, param, wert)` | Sensor-Parameter setzen (siehe Tabelle) |
| 10 | `camControl(10, slot, 0)` | Bild in PSRAM-Slot aufnehmen (1-4), gibt JPEG-Groesse zurueck |
| 11 | `camControl(11, slot, dateiHandle)` | PSRAM-Slot in Datei speichern, gibt Bytes zurueck |
| 12 | `camControl(12, slot, 0)` | PSRAM-Slot freigeben (0 = alle Slots) |
| 13 | `camControl(13, 0, 0)` | Kamera deinitialisieren + alle Slots + Stream stoppen |
| 14 | `camControl(14, slot, 0)` | Slot-Groesse in Bytes abfragen (0 wenn leer) |
| 15 | `camControl(15, on_off, 0)` | MJPEG Stream-Server auf Port 81 starten/stoppen |
| 16 | `camControl(16, intervall_ms, schwelle)` | Bewegungserkennung aktivieren (0=deaktivieren) |
| 17 | `camControl(17, sel, 0)` | Bewegungswert: 0=Trigger, 1=Helligkeit, 2=ausgeloest, 3=Intervall |
| 18 | `camControl(18, 0, 0)` | Bewegungs-Referenzbuffer freigeben |
| 19 | `camControl(19, addr, mask)` | Rohes Sensorregister lesen |
| 20 | `camControl(20, addr, val)` | Rohes Sensorregister schreiben |

Aufnahme (sel 10) kopiert das JPEG vom Kamera-Framebuffer in einen PSRAM-Slot und gibt den Framebuffer sofort zurueck, was schnelle aufeinanderfolgende Aufnahmen ermoeglicht. Bis zu 4 Slots koennen gleichzeitig Bilder halten.

**Wichtig:** Kamera-Aufnahme (`camControl(10, ...)`) muss in `TaskLoop()` (VM-Task-Thread) laufen. Aufruf aus `EverySecond()` (Haupt-Thread) friert das Geraet ein.

**Stream-Server (sel 15):** Startet einen MJPEG-Server auf Port 81 mit `/stream`, `/cam.mjpeg` und `/cam.jpg` Endpunkten. Wird automatisch verzoegert, wenn WiFi noch nicht bereit ist (sicher fuer Autoexec). Der Stream wird auf der Tasmota-Hauptseite eingebettet.

#### Sensor-Parameter (sel=9)

| param | Einstellung | Bereich |
|-------|------------|---------|
| 0 | vflip | 0/1 |
| 1 | Helligkeit | -2..2 |
| 2 | Saettigung | -2..2 |
| 3 | hmirror | 0/1 |
| 4 | Kontrast | -2..2 |
| 5 | Bildgroesse | FRAMESIZE_* |
| 6 | Qualitaet | 10..63 |
| 7 | Schaerfe | -2..2 |

#### E-Mail Bildanhaenge

In PSRAM-Slots aufgenommene Bilder koennen per `mailAttachPic()` an E-Mails angehaengt werden. Bis zu 4 Bilder pro E-Mail:

```c
// 2 Bilder in Slot 1 und 2 aufnehmen
camControl(10, 1, 0);
camControl(10, 2, 0);

// E-Mail mit beiden Bildern senden
mailBody("Bewegungsalarm");
mailAttachPic(1);
mailAttachPic(2);
mailSend("[*:*:*:*:*:user@example.com:Alarm]");
```

#### Aufnahme und Speichern Beispiel

```c
// Bild in PSRAM-Slot 1 aufnehmen
int size = camControl(10, 1, 0);

// Slot 1 in Datei speichern
int fh = fileOpen(path, 1);    // zum Schreiben oeffnen
int written = camControl(11, 1, fh);
fileClose(fh);

// MJPEG Stream auf Port 81 starten
camControl(15, 1, 0);
```

#### Zeitstempel- / Text-Overlay auf JPEG-Frames (Cam ↔ Bild Bruecke)

Drei Hilfs-Syscalls verbinden **Cam-Slots** (JPEG im PSRAM, geschrieben von `camControl(10)`) und **Bild-Slots** (RGB565 im PSRAM, vom Display-Subsystem genutzt). Gemeinsam bilden sie eine Pipeline **Aufnahme → Dekodierung → Text-Overlay → Neu-Enkodierung → Speichern/Streamen** auf jedem Cam-Board, mit oder ohne physisch angeschlossenem Display.

| Funktion | Beschreibung |
|----------|-------------|
| `int dspLoadImageFromCam(cam_slot)` | JPEG aus Cam-Slot in einen freien RGB565-Bild-Slot dekodieren. Gibt Bild-Slot zurueck, -1 bei Fehler. Quell-Cam-Slot bleibt unveraendert |
| `dspImgTextBurn(slot, x, y, color, fieldw, align, text)` | Glyphen-Pixel direkt in den Bildpuffer schreiben (kein Display-Push). Parameter wie `dspImgText`. Funktioniert kopflos |
| `int dspImageToCam(img_slot, cam_slot, quality)` | Bild-Slot zurueck in einen Cam-Slot als JPEG kodieren. Gibt Bytes zurueck, -1 bei Fehler. `quality` 1..63 (12 ≈ Q=85) |

**Build-Voraussetzungen:** `USE_WEBCAM` oder `USE_TINYC_CAMERA`, zusaetzlich `USE_DISPLAY` (fuer die Font-Tabellen — das TFT muss nicht verdrahtet sein), auf ESP32 mit `JPEG_PICTS` / PSRAM.

**Beispiel — Zeitstempel in VGA-Aufnahme brennen, auf Dateisystem speichern:**
```c
#define CAM_IN   1     // Cam-Slot mit der Rohaufnahme
#define CAM_OUT  2     // Cam-Slot der das gestempelte JPEG haelt
int  counter;
char path[40];
char line[48];

void main() {
    camControl(0, 8);  // Kamera init, FRAMESIZE_VGA (8 = VGA)
}

void TaskLoop() {
    if (camControl(10, CAM_IN, 0) <= 0) { return; }            // JPEG aufnehmen

    int img = dspLoadImageFromCam(CAM_IN);                      // → RGB565
    if (img < 0) { return; }

    sprintf(line, "%04d-%02d-%02d %02d:%02d:%02d",
            tasm_year, tasm_month, tasm_day,
            tasm_hour, tasm_minute, tasm_second);
    dspImgTextBurn(img, 10, 10, YELLOW, 0, 0, line);            // Overlay

    int jlen = dspImageToCam(img, CAM_OUT, 12);                 // Neu-Enkodierung
    if (jlen <= 0) { return; }

    counter = counter + 1;
    sprintf(path, "/snap_%04d.jpg", counter);
    int fh = fileOpen(path, 1);
    if (fh >= 0) {
        camControl(11, CAM_OUT, fh);                            // auf FS speichern
        fileClose(fh);
    }

    delay(60000);   // eine Aufnahme pro Minute
}
```

**Hinweise:**
- Aufnahme + Dekodierung + Neu-Enkodierung muessen in `TaskLoop()` (VM-Task-Thread) laufen. Aufruf aus `EverySecond()` friert das Geraet ein (gleiche Regel wie bei reinem `camControl(10)`).
- Font-Auswahl folgt dem Display-Stack: `dspText("[f2s1]")` vor `dspImgTextBurn` aufrufen, um einen groesseren Font zu waehlen. Auf kopflosen Boards ohne geladenen Display-Treiber faellt es auf Font12 Groesse 1 zurueck.
- Das Ergebnis in `CAM_OUT` verhaelt sich genau wie eine frische Aufnahme — es kann via `camControl(11)` in eine Datei geschrieben, via `mailAttachPic()` angehaengt oder ueber den Stream-Server geroutet werden.

Siehe `snap_with_timestamp.tc` fuer die vollstaendige Pipeline oben.

#### Komplettes Kamera-Skript

Siehe `webcam_tinyc.tc` fuer ein vollstaendiges Sicherheitskamera-Beispiel mit MJPEG-Streaming, Bewegungserkennung, PIR-Alarm, E-Mail-Benachrichtigung, Zeitraffer und automatischem Aufraeumen. Siehe `webcam.tc` fuer die Variante mit dem Tasmota Webcam-Treiber.

---

### HomeKit (ESP32 — benoetigt USE_HOMEKIT)

Apple HomeKit-Integration — Geraete direkt aus TinyC als HomeKit-Zubehoer bereitstellen. Sensoren, Lichter, Schalter und Steckdosen werden ueber Apple Home steuerbar. Alle HomeKit-gebundenen Variablen verwenden **native Float-Werte** — keine x10-Skalierung noetig.

**Benoetigt:** `#define USE_HOMEKIT` in `user_config_override.h`.

#### Vordefinierte HomeKit-Konstanten

| Konstante | Wert | HAP-Kategorie | Variablen |
|-----------|------|---------------|-----------|
| `HK_TEMPERATURE` | 1 | Sensor (Temperatur) | 1: Temperatur in °C |
| `HK_HUMIDITY` | 2 | Sensor (Feuchte) | 1: Feuchte in % |
| `HK_LIGHT_SENSOR` | 3 | Sensor (Helligkeit) | 1: Lux-Wert |
| `HK_BATTERY` | 4 | Sensor (Batterie) | 3: Ladezustand, Schwach-Flag, Ladestatus |
| `HK_CONTACT` | 5 | Sensor (Kontakt) | 1: Offen/Geschlossen |
| `HK_SWITCH` | 6 | Schalter | 1: Ein/Aus |
| `HK_OUTLET` | 7 | Steckdose | 1: Ein/Aus |
| `HK_LIGHT` | 8 | Licht (Farbe) | 4: Power, Hue, Saturation, Brightness |

#### HomeKit-Funktionen

| Funktion | Beschreibung |
|----------|-------------|
| `hkSetCode(code)` | Kopplungscode festlegen (Format: `"XXX-XX-XXX"`) |
| `hkAdd(name, typ)` | Geraet hinzufuegen — Name und Typ (z.B. `HK_TEMPERATURE`) |
| `hkVar(variable)` | Float-Variable an das aktuelle Geraet binden |
| `int hkReady(variable)` | Gibt 1 zurueck wenn HomeKit diese Variable geaendert hat (loescht Flag automatisch) |
| `int hkStart()` | Deskriptor fertigstellen und HomeKit starten. Gibt 0=ok zurueck |
| `int hkInit(char descriptor[])` | HomeKit mit Raw-Deskriptor starten |
| `hkReset()` | Alle Kopplungsdaten loeschen (Werksreset). Nach Neustart erneut koppeln |
| `hkStop()` | HomeKit-Server beenden |

#### hkReady() — Aenderungsabfrage

`hkReady(var)` funktioniert wie `udpReady()` — gibt 1 zurueck wenn Apple Home diese Variable seit dem letzten Aufruf geaendert hat, und loescht das Flag automatisch. Die Firmware schreibt den Wert direkt in die globale Variable, daher ist keine manuelle Zuweisung noetig. Da `global` Variablen automatisch per UDP senden, ist kein expliziter Aufruf mehr noetig:

```c
void EverySecond() {
    // global Variablen senden automatisch bei Zuweisung — kein expliziter udpSend noetig
}
```

#### HomeKitWrite-Callback (Optional)

Wird aufgerufen wenn Apple Home einen Wert aendert. Der Wert ist bereits in der globalen Variable gespeichert bevor dieser Callback laeuft — nur fuer lokale Seiteneffekte wie Relais-Weiterleitung verwenden:

```c
void HomeKitWrite(int dev, int var, float val) {
    // dev = Geraeteindex (Reihenfolge der hkAdd-Aufrufe, ab 0)
    // var = Variablenindex (Reihenfolge der hkVar-Aufrufe pro Geraet, ab 0)
    // val = neuer Float-Wert von Apple Home (bereits in der Variablen gespeichert)
    // Nur fuer Seiteneffekte wie tasm_power = 1 noetig
}
```

#### Builder-Pattern (hkAdd + hkVar)

Geraete werden schrittweise definiert. `hkAdd()` beginnt ein Geraet, `hkVar()` bindet Float-Variablen daran. Mehrere `hkVar()`-Aufrufe fuer Geraete mit mehreren Eigenschaften (z.B. Licht mit Farbe):

```c
// Farbiges Licht — 4 Variablen: Power, Hue, Saturation, Brightness
float pwr, hue, sat, bri;

hkSetCode("111-22-333");
hkAdd("Lampe", HK_LIGHT);
hkVar(pwr); hkVar(hue); hkVar(sat); hkVar(bri);

// Einfacher Sensor — 1 Variable
float temp;
hkAdd("Temperatur", HK_TEMPERATURE);
hkVar(temp);

hkStart();
```

#### Vollstaendiges Beispiel — Buero mit Licht + Sensoren

```c
// HomeKit-gebundene Variablen (native Float-Werte)
float mh_pwr, mh_hue, mh_sat, mh_bri;  // Farblicht
float elamp;     // Ecklicht ein/aus
float btemp;     // Temperatur (z.B. 22.5)
float bhumi;     // Feuchte (z.B. 55.0)
int last_pwr;

// Nur fuer Relais-Weiterleitung noetig — Wert ist bereits in der Variable
void HomeKitWrite(int dev, int var, float val) {
    if (dev == 0 && var == 0) {
        int pwr;
        pwr = 0;
        if (val > 0.0) { pwr = 1; }
        if (pwr != last_pwr) { tasm_power = pwr; last_pwr = pwr; }
    }
}

void EverySecond() {
    // Sensorwerte via UDP empfangen
    if (udpReady("btemp")) { btemp = udpRecv("btemp"); }
    if (udpReady("bhumi")) { bhumi = udpRecv("bhumi"); }

    // global Variablen senden automatisch bei Zuweisung — kein expliziter udpSend noetig
}

int main() {
    mh_pwr = 0.0; mh_hue = 0.0; mh_sat = 0.0; mh_bri = 50.0;
    elamp = 0.0; btemp = 22.0; bhumi = 50.0;
    last_pwr = -1;

    hkSetCode("111-11-111");
    hkAdd("Licht", HK_LIGHT);
    hkVar(mh_pwr); hkVar(mh_hue); hkVar(mh_sat); hkVar(mh_bri);
    hkAdd("Ecklicht", HK_OUTLET);          hkVar(elamp);
    hkAdd("Temperatur", HK_TEMPERATURE);    hkVar(btemp);
    hkAdd("Feuchte", HK_HUMIDITY);          hkVar(bhumi);
    hkStart();
    return 0;
}
```

#### Kopplung

1. Firmware mit `USE_HOMEKIT` kompilieren und flashen
2. TinyC-Programm mit `hkSetCode()` / `hkAdd()` / `hkStart()` kompilieren und hochladen
3. QR-Code unter `http://<Geraet>/hk` mit iPhone scannen
4. Bei Konfigurationsaenderungen `hkReset()` einmalig ausfuehren, dann erneut koppeln

### Matter (ESP32 — benoetigt USE_MATTER_C)

Matter ist die Alternative zu HomeKit und nutzt **denselben TinyC-Integrations-
Slot** (`TINYC_MATTER` ersetzt `TINYC_HOMEKIT` zur Build-Zeit — beide schliessen
sich gegenseitig aus). Die reine C-Engine `matter_c` in der Firmware erledigt
die schwierigen Teile — Kopplung (SPAKE2+/PASE), das Interaction Model
(Read/Subscribe) und die Subscription-/Report-Engine. Ein `.tc`-Skript
*deklariert* nur das Matter-Geraet und *veroeffentlicht* Attributwerte — kein
Firmware-Neubau, um das Geraet zu aendern.

#### Vordefinierte Matter-Konstanten

| Gruppe | Konstanten |
|---|---|
| Geraetetypen | `MATTER_PLUG` `MATTER_ONOFF_LIGHT` `MATTER_DIMM_LIGHT` `MATTER_TEMP_SENSOR` `MATTER_HUM_SENSOR` |
| Cluster-IDs | `CLUSTER_ONOFF` `CLUSTER_LEVEL` `CLUSTER_TEMP` `CLUSTER_HUM` `CLUSTER_POWER` `CLUSTER_ENERGY` |
| Attributtypen | `MTR_BOOL` `MTR_U8` `MTR_U16` `MTR_U32` `MTR_U64` `MTR_ENUM8` |

#### Matter-Funktionen

| Funktion | Beschreibung |
|---|---|
| `int matterAdd(geraetetyp)` | Endpunkt eines Geraetetyps hinzufuegen; liefert die Endpunkt-ID (<0 bei Fehler). Die Pflicht-Cluster des Typs werden automatisch angehaengt (z.B. `MATTER_PLUG` → OnOff → Relais 1) |
| `matterCluster(ep, clusterId)` | Cluster zu einem Endpunkt hinzufuegen |
| `matterAttr(ep, cl, attr, typ)` | Attribut deklarieren (`typ` = `MTR_U32` usw.) |
| `matterSet(ep, cl, attr, wert)` | Attributwert veroeffentlichen; Abonnenten werden im naechsten Loop benachrichtigt |
| `int matterGet(ep, cl, attr)` | Zwischengespeicherten Attributwert lesen (0 falls nicht vorhanden) |
| `matterName(ep, "label")` | Endpunkt benennen, damit ein Controller ihn mit diesem Titel anzeigt (siehe *Endpunkte benennen* unten) |
| `int matterStart()` | Bewerben + Kopplung annehmen. Liefert 0=ok |
| `matterReset()` | Datenmodell auf den Root-Knoten zuruecksetzen (vor eigener Deklaration aufrufen) |

OnOff (Cluster `CLUSTER_ONOFF`) auf einem Plug-/Light-Endpunkt steuert
automatisch Relais 1 — die Firmware wendet On/Off/Toggle auf den realen GPIO an.

#### Endpunkte benennen (`matterName`)

Reines Matter kennt keinen Namen pro Endpunkt, daher erscheint ein Knoten mit
mehreren Endpunkten in Apple Home als *"Temperatursensor 1 … N"*.
`matterName(ep, "label")` macht den Knoten zu einer Matter-**Bridge**, sodass
jeder Endpunkt mit eigenem Titel erscheint: der erste Aufruf legt einen
**Aggregator**-Endpunkt an, der benannte Endpunkt wird zum **Bridged Node** und
traegt einen Bridged-Device-Basic-Information-`NodeLabel`.

```c
e = matterAdd(MATTER_TEMP_SENSOR);
matterName(e, "Buero Temp");          // erscheint als "Buero Temp" statt "Temperatursensor 1"
```

- **Nach** `matterAdd` fuer diesen Endpunkt aufrufen; pro Endpunkt optional
  (unbenannte Endpunkte bleiben einfach); idempotent (zum Umbenennen erneut).
- Labels sind ASCII — ein String-Literal speichert ein Byte pro Zeichen, Umlaute
  wuerden Latin-1 statt UTF-8 erzeugen; ASCII verwenden (`"Buero"`) und bei
  Bedarf im Controller in `Büro` umbenennen.
- Eine Bridge aendert die Knotenidentitaet → ein bereits gekoppelter Knoten muss
  im Controller **entfernt und neu hinzugefuegt** werden, um die Namen zu uebernehmen.

#### Farblichter (Extended Color Light)

Fuer Farblichter gibt es noch keine benannten Konstanten — die rohen Matter-IDs
verwenden: Geraetetyp **`0x010D`** (Extended Color Light) und Cluster **`0x0300`**
(Color Control). Die Firmware verarbeitet `LevelControl` (`CLUSTER_LEVEL`,
Helligkeit) und `ColorControl` (`0x0300`, Hue/Saturation) vom Controller und
schreibt sie ins Datenmodell; dein `MatterInvoke()` liest die Werte dann mit
`matterGet()` zurueck und faerbt eine LED mit `rgbLed()`. ColorControl-Attribute:
`0` = CurrentHue, `1` = CurrentSaturation (beide 0..254).

Siehe **`examples/matter_rgb.tc`** fuer ein vollstaendiges Dual-Endpunkt-Geraet
(On/Off-Steckdose + HSV-Farblicht auf einer On-Board-WS2812) und
**`examples/rgb_selftest.tc`** fuer einen Controller-freien Test der Farb-Pipeline.

#### MatterInvoke-Callback (Optional)

Definiere `MatterInvoke(ep, cluster, cmd)`, um Controller-Befehle selbst zu
behandeln (das Matter-Gegenstueck zu `HomeKitWrite`). Ist die Funktion
vorhanden, **gehoert der Befehl deinem Skript** — das eingebaute OnOff→Relais-
Standardverhalten tritt zurueck, also kein doppeltes Umschalten. Weglassen, um
das automatische Relaisverhalten zu behalten.

```c
void MatterInvoke(int ep, int cluster, int cmd) {
    if (cluster == CLUSTER_ONOFF) {
        if (cmd == 2) { tasm_power = 1 - tasmPower(0); }  // Umschalten
        else          { tasm_power = cmd; }               // 0=Aus, 1=Ein
    }
}
```

#### Beispiel — Steckdose + Leistungssensor

```c
int ep;
int watts, tick;

void EverySecond() {
    // Echter Zaehler: watts = (int)sensorGet("ENERGY#Power");  // oder smlGet("Power")
    tick = tick + 1; watts = (tick * 13) % 250;            // Demo-Saegezahn
    matterSet(ep, CLUSTER_POWER, 0, watts);                 // ActivePower
}

int main() {
    matterReset();                          // sauberer Start (nur Root-Knoten)
    ep = matterAdd(MATTER_PLUG);            // Endpunkt + OnOff-Cluster -> Relais 1
    matterCluster(ep, CLUSTER_POWER);      // Electrical Power Measurement
    matterAttr(ep, CLUSTER_POWER, 0, MTR_U32);
    matterStart();                          // bewerben + Kopplung annehmen
    return 0;
}
```

#### Kopplung

1. Firmware mit `USE_MATTER_C` (`-DTINYC_MATTER`) kompilieren und flashen
2. TinyC-Programm mit `matterAdd()` / `matterStart()` kompilieren und hochladen
3. Mit einem beliebigen On-Network-Matter-Controller koppeln (chip-tool,
   Apple Home, …); die Kopplungsinfo wird unter `http://<Geraet>/mt` angezeigt

> Status: an allen drei grossen Ecosystemen am Geraet verifiziert. Die
> Datenmodell-Skripting-API (`matter*`) und der `MatterInvoke`-Callback sind
> aktiv; der CSA-Referenz-Controller **chip-tool**, **Apple Home**, **Google
> Home** und **Amazon Alexa** koppeln und steuern den Knoten ueber IPv6 (PASE →
> Attestation → CSR → AddNOC → CASE), und die Fabric bleibt ueber Neustarts
> erhalten. Die operative Discovery wird spezifikationskonform unter
> `_matter._tcp` beworben; Multi-Fabric / mehrere gleichzeitige operative
> Sessions werden unterstuetzt.
>
> Seit **v1.6.28 koppelt und steuert die volle gemischte Bridge** (Aktoren +
> Sensoren, `matter_home_bridge.tc`) **auch unter Alexa**, auf einem Knoten —
> der fruehere Rat, fuer Alexa in einen Lampen-Knoten + einen Sensor-Knoten zu
> teilen, ist **hinfaellig** (das war ein False-Negative durch veraltete
> Firmware; ein frisch verifizierter Flash koppelt die volle Bridge). Die
> Bind/Unbind-Buttons und der On-Device-QR liegen unter `http://<Geraet>/mt`.

#### Vordefinierte Datei-Konstanten

Fuer `fileOpen()` stehen folgende Kurzformen zur Verfuegung:

| Konstante | Wert | Beschreibung |
|-----------|------|-------------|
| `r` | 0 | Lesen |
| `w` | 1 | Schreiben |
| `a` | 2 | Anhaengen |

```c
int f = fileOpen("/daten.csv", r);   // statt fileOpen("/daten.csv", 0)
f = fileOpen("/log.txt", a);          // statt fileOpen("/log.txt", 2)
```

### Plugin-Abfrage

| Funktion | Beschreibung |
|----------|-------------|
| `int pluginQuery(char dst[], int index, int p1, int p2)` | Binäres Plugin abfragen. Gibt Ergebnis zurueck und schreibt optionale String-Antwort in `dst` |

### Cross-VM Share-Tabelle (ESP32)

Treiber-globaler benannter Schluessel/Wert-Speicher mit Mutex-Schutz, ueber den zwei oder mehr TinyC-Slots Skalare und kurze Strings austauschen koennen. Sinnvoll, wenn ein Programm einen einzelnen Slot sprengt (`TC_MAX_PROGRAM = 128 KB`) und auf zwei Slots aufgeteilt wird, oder wenn mehrere kooperierende Programme Zustand austauschen sollen — ohne Umweg ueber MQTT oder Dateisystem.

Kapazitaet (per `user_config_override.h` aenderbar): **`TC_SHARE_MAX = 32`** Eintraege · **`TC_SHARE_KEY_LEN = 16`** Zeichen Schluessel · **`TC_SHARE_STR_LEN = 64`** Zeichen Wert. Worst-Case-Speicher ≈ 2,6 KB DRAM. Mutex wird beim ersten Zugriff angelegt.

| Funktion | Beschreibung |
|---|---|
| `void shareSetInt(char key[], int v)`     | Integer-Wert fuer `key` setzen (Eintrag wird angelegt, Typ ueberschrieben) |
| `void shareSetFloat(char key[], float v)` | Float-Wert fuer `key` setzen |
| `void shareSetStr(char key[], char v[])`  | String-Wert fuer `key` setzen (auf `TC_SHARE_STR_LEN` gekuerzt) |
| `int shareGetInt(char key[])`             | Integer lesen; **0** wenn Schluessel fehlt oder falscher Typ |
| `float shareGetFloat(char key[])`         | Float lesen; **0.0** wenn fehlend |
| `int shareGetStr(char key[], char dst[])` | String nach `dst` lesen; gibt kopierte Zeichen zurueck, **0** + leerer `dst` wenn fehlend |
| `int shareHas(char key[])`                | **1** wenn Schluessel existiert, sonst **0** |
| `int shareDelete(char key[])`             | Eintrag loeschen; **1** wenn vorhanden, **0** sonst |

**Schluessel-Beschraenkung:** jedes `key`-Argument muss ein **String-Literal** sein (zu einem Konstantenpool-Index zur Compilezeit aufgeloest). Variable Schluessel werden nicht unterstuetzt. Schluessel sind Gross-/Kleinschreibung-relevant.

**Semantik fehlender Schluessel:** Lesezugriffe loesen niemals einen Fehler aus. `shareHas()` unterscheidet "Schluessel fehlt" von "Schluessel existiert mit Wert 0". Erneutes `shareSet*` mit anderem Typ ueberschreibt den Eintrag stillschweigend.

**Beispiel — Slot 0 Schreiber + Slot 1 Leser:**
```c
// Slot 0 (Schreiber)
int counter = 0;
void EverySecond() {
    counter = counter + 1;
    shareSetInt("counter", counter);
    shareSetFloat("kwh", counter * 0.1);
    char nm[32];
    sprintf(nm, "tick=%d", counter);
    shareSetStr("name", nm);
}
int main() { return 0; }
```
```c
// Slot 1 (Leser)
void Command(char cmd[]) {
    if (strcmp(cmd, "ALL") == 0) {
        int   c = shareGetInt("counter");
        float f = shareGetFloat("kwh");
        char  n[32];
        shareGetStr("name", n);
        char r[160];
        sprintf(r, "counter=%d kwh=%.1f name=%s", c, f, n);
        responseCmnd(r);
    } else {
        responseCmnd("RDR: ALL");
    }
}
int main() { addCommand("RDR"); return 0; }
```

### Bluetooth LE (ESP32)

BLE-Advertisements scannen, als GATT-**Client** agieren (verbinden / lesen / schreiben / Notifications
abonnieren) oder einen GATT-**Server** betreiben (als Peripheral werben, sodass sich ein Handy mit dem
Gerät verbindet). Baut auf Tasmotas Common-BLE-Treiber (`xdrv_79`) auf und teilt sich daher ein
Funkmodul mit den MI32-/iBeacon-Scannern. **Benötigt eine Firmware mit `USE_TINYC_BLE`** (zieht
`USE_BLE_ESP32` ≈ **+292 KB Flash / +9 KB RAM** nach). Ohne dieses Flag ist jeder BLE-Builtin ein
No-op und liefert einen Sentinel-Wert (`0` / `-1`). Nur ESP32-Familie (kein ESP8266). Der erste
`bleScan()` / `bleServer()` aktiviert BLE zur Laufzeit — kein `SetOption115` nötig.

**Threading:** Advertisement- und GATT-Completion-Callbacks laufen auf dem NimBLE-/Haupt-Task, nie auf
der VM. Sie schreiben in kleine Puffer; dein Skript leert sie in `TaskLoop()` — die API ist also
**nicht-blockierend** (pollen, nicht warten).

**Scannen / beobachten** — einen Scan starten, dann die gepufferten Adverts einzeln abholen:

| Funktion | Beschreibung |
|---|---|
| `int bleScan(int ms)` | Adverts in einen Ring aufnehmen. `ms` > 0 stoppt automatisch nach so vielen ms; `ms = 0` läuft bis `bleScanStop()`. Leert die Queue. Liefert 1 |
| `int bleScanStop()` | Aufnahme stoppen. Liefert 1 |
| `int bleNext()` | Das nächste Advert aus der Queue in den „aktuellen" Slot holen. Liefert 1, wenn eines vorhanden war, 0 bei leerer Queue. Danach die Getter unten für das aktuelle Advert aufrufen |
| `int bleMac(char buf[])` | Die 6 MAC-Bytes des aktuellen Adverts in `buf[0..5]` schreiben (Anzeige-Reihenfolge, MSB zuerst). Liefert 6 |
| `int bleAddrType()` | Adresstyp des aktuellen Adverts: 0 = public, 1/2/3 = random. Zum Verbinden nötig |
| `int bleRssi()` | RSSI des aktuellen Adverts in dBm (negativ) |
| `int bleName(char buf[])` | Den lokalen Namen des Adverts in `buf` kopieren (NUL-terminiert). Liefert die Länge (0 falls keiner) |
| `int bleMfg(char buf[])` | Die herstellerspezifischen Daten-Bytes in `buf` kopieren. Liefert die Länge. Die ersten beiden Bytes sind die Company-ID, Little-Endian (z. B. `buf[0]=0xD0, buf[1]=0x06` → 0x06D0) |

**GATT-Client** — ein Ziel setzen, dann eine Lese- oder Schreib-Transaktion starten und auf Abschluss
pollen. Jede Transaktion ist ein Verbinden → (optional Schreiben) → (optional Abonnieren-und-eine-
Notification-abwarten) → Trennen:

| Funktion | Beschreibung |
|---|---|
| `int bleTarget(char mac[], int addrtype, int svc16)` | GATT-Ziel setzen: 6 MAC-Bytes (Anzeige-Reihenfolge, wie von `bleMac()`), Adresstyp (von `bleAddrType()`) und die 16-Bit-Service-UUID (z. B. `0x180D`). Liefert 1 |
| `int bleReadStart(int notify16)` | Mit dem Ziel verbinden, die Notify-Charakteristik `notify16` des Service abonnieren und auf eine Notification warten. Liefert 1 = gestartet, < 0 = busy/Fehler. `bleDone()` pollen |
| `int bleWriteStart(int chr16, char buf[], int len)` | Mit dem Ziel verbinden und `buf[0..len-1]` in die Charakteristik `chr16` schreiben. Liefert 1 = gestartet, < 0 = busy/Fehler. `bleDone()` pollen |
| `int bleDone()` | Die laufende Transaktion pollen: 0 = läuft noch, > 0 = fertig (der Wert ist die Ergebnislänge in Bytes), < 0 = fehlgeschlagen (negativer xdrv_79-Statuscode, z. B. -5 = Service nicht gefunden, -8 = Notify-Timeout, -11 = Verbindung fehlgeschlagen) |
| `int bleResult(char buf[])` | Nach `bleDone() > 0` die empfangenen Notification-/Lese-Bytes in `buf` kopieren. Liefert die Länge |

Es ist immer nur eine GATT-Transaktion gleichzeitig in Bearbeitung (ein einziger Half-Duplex-Slot).
Geräte mit **random**-Adresse wechseln diese zwischen Sitzungen — jedes Mal neu per Hersteller-ID /
Name suchen und mit der aktuell beworbenen Adresse verbinden; die MAC niemals fest verdrahten.

**Diagnose:** Der Konsolenbefehl `BLEDebug 1` lässt den Common-BLE-Treiber die tatsächlichen Services
+ Charakteristiken eines Geräts ausgeben, wenn ein angeforderter Service nicht gefunden wird —
praktisch bei der Inbetriebnahme eines neuen Geräts mit unbekannten UUIDs.

Siehe `examples/ble_scan.tc` für einen vollständigen Scanner mit Geräte-Filter-Vorlage.

```c
// Eine Notification von einem BLE-Peripheral lesen (Service 0x180D, Notify-Char 0x2A37)
char nm[40]; int mac[8]; char frame[40]; int st;

int main() { st = 0; bleScan(0); return 0; }       // Scan starten

void TaskLoop() {
  if (st == 0) {                                    // per Name finden, dann verbinden
    if (bleNext()) {
      bleName(nm);
      if (strFind(nm, "MyDevice") >= 0) {
        bleMac(mac); int t = bleAddrType();
        bleScanStop();
        bleTarget(mac, t, 0x180D);
        if (bleReadStart(0x2a37) == 1) { st = 1; }
      }
    }
  } else if (st == 1) {                             // auf die Notification warten
    int d = bleDone();
    if (d > 0) {
      int n = bleResult(frame);
      char m[64]; sprintf(m, "got %d bytes, b0=%02x", n, frame[0]); addLog(m);
      st = 2;
    } else if (d < 0) { st = 0; bleScan(0); }       // fehlgeschlagen — neu scannen
  }
  delay(250);
}
```

**BLE-„SPP“ — eine bleibende Verbindung** für Geräte, die *strömen* (BlueRadios/BRSP-Module,
Nordic-UART-artige Peripherie, Seriell-über-BLE-Adapter). Der GATT-Client oben verbindet,
führt **eine** Operation aus und trennt wieder — richtig für eine Waage, die aufwacht,
meldet und schläft; falsch für einen fortlaufenden Datenstrom, bei dem die Verbindung
abrisse, bevor der zweite Messwert ankommt. Diese Aufrufe halten die Verbindung offen, bis
man sie schließt. UUIDs sind hier **Zeichenketten** (16 Bit `"180a"` oder volle 128 Bit),
anders als die int16-`svc16`/`chr16` der Einmal-Familie, die eine 128-Bit-UUID gar nicht
adressieren kann — und genau die benutzt ein proprietärer serieller Dienst fast immer.

| Funktion | Beschreibung |
|---|---|
| `int bleSppTarget(char mac[], int addrtype, "svc-uuid")` | Bleibendes Ziel setzen: 6 MAC-Bytes, Adresstyp, Service-UUID als **Zeichenketten-Literal**. Liefert 1 |
| `int bleSppConnect()` | Verbinden und den Dienst auflösen. **Blockiert** bis zu einigen Sekunden — nur aus `TaskLoop()`. 1 = verbunden, 0 = nicht |
| `int bleSppState()` | 1 solange verbunden *und* benutzbar, sonst 0. Wird auch 0, wenn die Gegenstelle die Verbindung fallen lässt |
| `int bleSppSub("notify-uuid")` | Die Charakteristik abonnieren, auf der das Gerät benachrichtigt. Liefert 1 |
| `int bleSppAvailable()` | Wartende Bytes im Empfangsring |
| `int bleSppRead(char buf[], int max)` | Bis zu `max` Bytes nach `buf` abholen. Liefert die Anzahl. Blockiert nie |
| `int bleSppWrite("chr-uuid", char buf[], int len)` | In eine Charakteristik schreiben, **ohne die Verbindung zu trennen**. Liefert 1 |
| `int bleSppClose()` | Trennen |
| `int bleGattDump(char mac[], int addrtype, char out[])` | Einmalig: verbinden, alle Dienste und Charakteristiken mit ihren Eigenschaften (`R`/`W`/`w`/`N`/`I`) als Text auflisten, trennen. Liefert die Textlänge |

**Bei jedem unbekannten Gerät zuerst `bleGattDump()` laufen lassen.** Eine proprietäre UUID
steht in keinem Datenblatt — man muss das Gerät fragen. `examples/ble_gatt_explore.tc`
verpackt das in drei Konsolenkommandos (scannen → MAC wählen → auslesen).

Scharfe Kanten, jede davon hat echte Fehlersuchzeit gekostet:

* **`bleSppConnect()` liefert 0, solange der BLE-Stapel nicht oben ist.** Beim ersten Aufruf
  nach dem Start ist das normal und kein Fehler — einfach erneut versuchen. Keine
  blockierende Sechs-Versuche-Schleife in ein Programm bauen, das nebenbei einen Sensor
  bedient: jeder Fehlversuch blockiert sekundenlang.
* **Den Rückgabewert von `bleSppWrite()` prüfen — und die LÄNGE mitloggen.** Ein `#define`,
  dessen Wert eine Zeichenkette ist, übersteht die Übergabe durch einen `char[]`-**Parameter**
  nicht; ein direkt hingeschriebenes Literal schon. Ein 29 Byte langer Befehl wurde auf diesem
  Weg still zu einem 1-Byte-Befehl, und das Schreiben meldete trotzdem Erfolg, weil es
  *irgendetwas* geschrieben hatte. Lange Befehle zur Laufzeit mit `sprintf` bauen.
* Schreibvorgänge, die länger als die ausgehandelte MTU sind, werden automatisch zerlegt
  (eine serielle Brücke kümmert sich nicht um Schreibgrenzen) — ein ganzer Befehl darf also
  in einem Aufruf übergeben werden.
* **`CleanUp()` MUSS `bleSppClose()` aufrufen.** `TinyCStop` beendet nur das *Skript* — die
  Verbindung bleibt in der Firmware offen und ihre Benachrichtigungen laufen weiter. Ein
  neues `.tcb` hochzuladen, während ein paar hundert Pakete je Sekunde auf einen
  abgeschalteten Flash-Cache treffen, löst den Interrupt-Watchdog aus.
* **Der Durchsatz ist begrenzt.** An einem BRSP-Modul über 40-Sekunden-Fenster gemessen: 223
  Sätze/s kamen an, wo 256/s angefordert waren (12,8 % zu wenig, mit Synchronfehlern); 185/s
  bei 200 (7,6 %, ohne Synchronfehler). Wenn die Gegenstelle eine Abtastrate wählen lässt,
  lieber eine verlangen, die die Verbindung trägt, statt Daten zu verlieren.
* Die Rate der Gegenstelle nicht annehmen — **messen** und die Anzeige dem gemessenen Wert
  folgen lassen.

```c
// Serielle Peripherie: einmal verbinden, dann fortlaufend lesen
#define SVC   "da2b84f1-6279-48de-bdc0-afbea0226079"
#define TX    "18cda784-4bd3-4370-85bb-bfed91ec86af"   // hier benachrichtigt das Gerät
#define RX    "bf03260c-7205-4c25-af43-93b1c299d159"   // hier schreiben wir
int  mac[6];
char rx[256];
int  an;

int main() {
    mac[0] = 0xEC; mac[1] = 0xFE; mac[2] = 0x7E;
    mac[3] = 0x10; mac[4] = 0xE1; mac[5] = 0xEF;
    bleSppTarget(mac, 0, SVC);
    return 0;
}

void TaskLoop() {
    if (!an) {
        if (!bleSppConnect()) { delay(3000); return; }   // Stapel noch nicht oben, oder Gegenstelle weg
        if (!bleSppSub(TX))   { delay(3000); return; }
        char hallo[] = "VS\r";
        bleSppWrite(RX, hallo, strlen(hallo));
        an = 1;
        return;
    }
    if (!bleSppState()) { an = 0; return; }              // Gegenstelle hat getrennt
    int n = bleSppAvailable();
    if (n > 0) {
        if (n > 255) { n = 255; }
        int got = bleSppRead(rx, n);
        // ... hier den Bytestrom auswerten ...
    }
    delay(10);
}

void CleanUp() { bleSppClose(); }   // nicht optional — siehe oben
```

Ein durchgearbeitetes Beispiel (Rahmen, Neusynchronisation, Skalierung, Live-Kurve) ist
`examples/variograf_ekg.tc`; `examples/max30102.tc` zeigt dieselbe Quelle kombiniert mit
einem zweiten Messkanal.

⚠️ Verbinden braucht eine deutlich bessere Verbindung als Scannen: bei −88 dBm wurde jeder
Versuch abgewiesen, bei −63 dBm klappte es sofort.

**GATT-Server (Peripheral)** — einen Service bewerben, sodass sich ein Handy (oder ein beliebiger
BLE-Central) mit dem Gerät verbindet und Daten austauscht — das übliche Handy-App-↔-IoT-Muster.
Einmalig konfigurieren (`bleServer` → `bleService` → `bleChar` × N → `bleServerStart`), dann zur
Laufzeit pollen/pushen. UUIDs sind Strings: 16-Bit (`"180a"`) oder vollständige 128-Bit
(`"6e400001-…"`). Charakteristik-Eigenschaften werden mit `|` kombiniert:

| Konstante | Bedeutung |
|---|---|
| `BLE_READ` | Central darf den Wert lesen |
| `BLE_WRITE` | Central darf den Wert schreiben |
| `BLE_NOTIFY` | Gerät darf Notifications an einen abonnierten Central pushen |

| Funktion | Beschreibung |
|---|---|
| `int bleServer(char name[])` | Server-Konfiguration beginnen; `name` ist der beworbene Gerätename. Aktiviert BLE zur Laufzeit. Liefert 1 |
| `int bleService(char uuid[])` | Die Service-UUID setzen (16- oder 128-Bit-String). Liefert 1 |
| `int bleChar(char uuid[], int props)` | Eine Charakteristik hinzufügen; `props` = `BLE_READ`/`BLE_WRITE`/`BLE_NOTIFY` (mit `|` kombiniert). Liefert ein **Handle** (≥ 0) für die Aufrufe unten, oder -1 |
| `int bleServerStart()` | Den Service aufbauen und das Werben starten. Liefert 1 |
| `int bleConnected()` | 1, wenn ein Central (Handy) verbunden ist, sonst 0 |
| `int bleCharWritten(int h)` | Anzahl der Bytes, die der Central seit dem letzten Lesen in Charakteristik `h` geschrieben hat (0 = nichts Neues) |
| `int bleCharRead(int h, char buf[])` | Diese geschriebenen Bytes in `buf` kopieren; löscht das Pending-Flag. Liefert die Länge |
| `int bleCharSet(int h, char buf[], int len)` | Den Wert setzen, den ein Central von `h` liest (ohne Notification). Liefert 1 |
| `int bleNotify(int h, char buf[], int len)` | Den Wert setzen **und** eine Notification an den abonnierten Central pushen. Liefert 1 |
| `int bleServerStop()` | Das Werben stoppen. Liefert 1 |

Den Server in `main()` aufbauen, dann in `TaskLoop()` mit `bleCharWritten()` / `bleCharRead()` auf
Handy→Gerät-Daten pollen und mit `bleNotify()` / `bleCharSet()` pushen. Das GATT-Layout wird **einmal
pro Boot** aufgebaut — um die Services/Charakteristiken zu ändern, das Gerät neu starten (ein erneuter
Skript-Lauf hängt sich an den bestehenden Server an). Funktioniert auf jedem ESP32 mit BLE: Auf dem
ESP32-P4 ist das Funkmodul der On-Board-C6 über esp-hosted, auf anderen ESP32 der native Controller —
die API ist identisch. Mit einer Handy-App wie **nRF Connect** testen.

Siehe `examples/ble_server.tc` für einen Server im Nordic-UART-Stil (Handy schreibt auf RX → schaltet
Power1; Gerät benachrichtigt einen hochzählenden Zähler auf TX).

```c
// GATT-Server: Handy schreibt 00/01 auf RX -> Power1; Gerät benachrichtigt einen Zähler auf TX.
#define RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
int hrx; int htx; int n; char buf[64];

int main() {
  bleServer("TasmotaBLE");
  bleService("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  hrx = bleChar(RX, BLE_WRITE);
  htx = bleChar(TX, BLE_READ | BLE_NOTIFY);
  bleServerStart();
  n = 0;
  return 0;
}

void TaskLoop() {
  if (bleCharWritten(hrx)) {                 // Handy -> Gerät
    bleCharRead(hrx, buf);
    tasmCmd(buf[0] ? "Power1 1" : "Power1 0", buf);
  }
  if (bleConnected()) {                      // Gerät -> Handy
    n = n + 1; buf[0] = n & 0xff; buf[1] = (n >> 8) & 0xff;
    bleNotify(htx, buf, 2);
  }
  delay(1000);
}
```

### Bluetooth Classic — SPP (nur urspruenglicher ESP32, benoetigt USE_TINYC_SPP)

Eine **serielle Verbindung zu jedem Bluetooth-Classic-Geraet**. Das Protokoll darueber
steht im Skript, nicht in der Firmware — dasselbe Grundwerkzeug bedient SMA-Wechselrichter,
OBD-Adapter, Waagen und Bondrucker, und es laesst sich ohne Flashen aendern.

| Funktion | Beschreibung |
|----------|--------------|
| `int sppInit()` | Stapel hochfahren (mehrfach aufrufbar). `1` = bereit. Kostet ~48 KB internen Heap. |
| `int sppConnect("aa:bb:..", kanal)` | Als Master verbinden. `kanal 0` = ueber die Dienstsuche ermitteln. Adresse auch als `char[]`. `0` = angestossen, `<0` = Fehler. Kehrt SOFORT zurueck — `sppState()` abfragen. |
| `int sppState()` | `0` aus, `1` bereit, `2` verbindet, `3` **offen**, `4` letzter Versuch FEHLGESCHLAGEN |
| `int sppAvailable()` | wartende Bytes |
| `int sppRead(char buf[], int n)` | nicht blockierend; liefert, was da ist (auch 0) |
| `int sppWrite(char buf[], int n)` | senden; gesendete Bytes oder `-1` |
| `int sppClose()` | trennen (Stapel bleibt oben) |
| `int sppDeinit()` | Stapel **abbauen**, die ~48 KB zurueckgeben |
| `int sppScan(char buf[], int n, int sekunden)` | Suche; schreibt je Fund `"AA:BB:CC:DD:EE:FF Name\n"`. Gibt die Anzahl zurueck. Wartet die Suchzeit ab. |

**Lesen blockiert nicht.** `sppRead()` liefert, was angekommen ist, und kehrt sofort
zurueck — das Skript wartet selbst und bleibt Herr des Ablaufs. Wuerde die Firmware warten,
haengte die VM an den Zeitueberschreitungen der Gegenstelle.

⚠️ **Aus `TaskLoop()` betreiben, NIE aus einem `spawnTask()`-Worker.** Ein Worker laeuft auf
einer eigenen VM, deren Feld-Tabelle nie gefuellt wird — dort hat *jedes* `char[]` die
Groesse 0 und der erste Zugriff endet in Laufzeitfehler 9.

⚠️ **`sppDeinit()` ist wichtig.** Der Stapel belegt ~48 KB, solange er laeuft, und gibt sie
von selbst nie zurueck; ein offener RFCOMM-Kanal kostet weitere ~19 KB. Auf einem 4-MB-Board
blieben damit 2,3 KB frei bei einem groessten Block von 1,5 KB — und *jeder* Funktionsaufruf
der VM braucht 1 KB am Stueck. Der Fehlschlag meldet sich als **"Stack overflow"**, weil die
OOM-Pfade des Laders diesen Code zurueckgeben. Nichts daran zeigt auf Bluetooth.

⚠️ **Vor `sppInit()` den freien Speicher pruefen.** Bluedroids eigenes `bta_sys_init()` macht
ein memset auf einen *ungeprueften* malloc; bei Speichermangel schreibt es auf NULL und das
ganze Geraet startet neu (`Exception 29 StoreProhibited`). Mit `tasm_heap` / `tasm_maxblock`
absichern und lieber einen Takt auslassen.

**Fehlersuche.** Die Firmware protokolliert jedes SPP- und GAP-Ereignis mit Status. Die
Schichten antworten in dieser Reihenfolge, und wo das Protokoll aufhoert, liegt der Fehler:

```
SPP: gap ACL_CONN_CMPL stat=0   Basisband steht -> Funk, Adresse und Reichweite stimmen
SPP: ev CL_INIT status=0        die RFCOMM-Anfrage ist raus
SPP: gap AUTH_CMPL stat=0       Kopplung erledigt (nur falls verlangt)
SPP: ev OPEN status=0           Kanal offen — erst jetzt meldet sppState() eine 3
```

Gar kein `ACL_CONN_CMPL` heisst: die Gegenstelle hat nicht geantwortet — zu weit weg, aus,
oder ihr einziger Verbindungsplatz ist belegt. GAP-Statuswerte ueber 0x100 sind HCI-Fehler
und werden zusaetzlich im Klartext ausgegeben (`stat=260` = HCI 0x04 = PAGE TIMEOUT).

**Voraussetzungen.** Bluetooth Classic gibt es nur auf dem *urspruenglichen* ESP32 — S3, C3,
C6 und P4 koennen ausschliesslich BLE. Tasmota liefert ein mit NimBLE vorkompiliertes
Framework ohne jeden Classic-Header; die Umgebung muss es mit Bluedroid neu bauen und drei
`-I`-Pfade von Hand ergaenzen. Einzelheiten im Kopf von
`tasmota/include/xdrv_124_tinyc_spp.h`.

Siehe `examples/spp_scan.tc` (findet die Suche ueberhaupt etwas?), `examples/spp_connect.tc`
(an welcher Schicht stirbt eine Verbindung?) und `examples/sma_sunnyboy.tc` (ein
vollstaendiges Protokoll darueber).

### LVGL-GUI (ESP32 — benötigt USE_TINYC_LVGL)

Baue eine **retained-mode, berührungsfähige GUI** auf dem Display des Geräts mit der LVGL-9-Engine
(Buttons, Slider, Diagramme, …). Aufgesetzt auf Tasmotas `xdrv_54_lvgl` über Universal Display —
zeichnet also auf dasselbe Panel wie `drawLine()`/TinyUI und nutzt denselben Touch-Treiber (GT911
usw.). **Benötigt eine Firmware mit `USE_TINYC_LVGL`** (zieht `USE_LVGL` mit, ≈ **+250 KB Flash** +
ein partieller Zeichenpuffer in RAM/PSRAM). Ohne diesen Build ist jeder `lvgl*`-Builtin ein No-op und
liefert `0`. Nur ESP32-Familie.

LVGL ist *nicht* reentrant: Die Firmware rendert es im Haupt-Loop, während deine `lvgl*`-Aufrufe auf
dem VM-Task laufen — ein Mutex serialisiert das automatisch, du rufst die Builtins einfach normal auf.

**Rotation.** `DisplayRotate 0|1|2|3` (0/90/180/270°) dreht das gesamte Panel — Legacy-`dsp*`-Zeichnen,
LVGL **und** Touch folgen alle — auf dem P4-MIPI-DSI-Panel; der Renderer transponiert in den
Portrait-Framebuffer, kein Code pro App nötig. Für eine **Querformat**-LVGL-UI `DisplayRotate 1`
**persistent setzen und neu starten**, damit das Panel gedreht hochkommt, *bevor* `lvglInit()` den
Screen in der getauschten Auflösung anlegt. (Ein Wechsel zur Laufzeit unter einem laufenden
LVGL-Screen kann das gerade gerenderte Frame stören.)

**Modell.** Objekte werden über einen Integer-**Handle** (1…) angesprochen. **Handle `0` ist der
aktive Screen** — als Parent (`lvglLabel(0)`) oder zum Stylen des Screens selbst
(`lvglSetBgColor(0, …)`). Handles werden automatisch ungültig entfernt, wenn LVGL ein Objekt löscht
(keine baumelnden Handles). Es gibt **keinen Callback nach TinyC**; stattdessen **pollst** du einen
Event-Ring in deiner Schleife (wie bei BLE):
`while (lvglEvent()) { if (lvglEventObj()==btn && lvglEventCode()==10) … }`.

Farben sind `0xRRGGBB`. Häufige LVGL-9-Konstanten als einfache Integer:
- **Align:** `1`=TOP_LEFT, `2`=TOP_MID, `5`=BOTTOM_MID, `9`=CENTER.
- **Event-Codes:** `0`=ALL, `1`=PRESSED, `4`=SHORT_CLICKED, `10`=CLICKED, `11`=RELEASED, `35`=VALUE_CHANGED.
- **Style-Props (`lvglSetStyleInt`):** `120`=RADIUS, `56`=BORDER_WIDTH, `112`=OPA.
- **Chart:** Typ `1`=LINE, `2`=BAR; Achse `0`=PRIMARY_Y.

**Lebenszyklus & Objekte**

| Funktion | Beschreibung |
|---|---|
| `int lvglInit()` | LVGL auf dem Panel starten (idempotent). 1 = aktiv. Einmal vor allen anderen `lvgl*` aufrufen. |
| `int lvglActive()` | 1 wenn LVGL läuft, sonst 0 |
| `int lvglObj(int parent)` | Basis-Container. `parent` = Handle oder 0 (Screen). Liefert Handle (0 = Tabelle voll) |
| `int lvglLabel(int parent)` | Label erzeugen |
| `int lvglButton(int parent)` | Button erzeugen (Beschriftung als Kind-Label hinzufügen) |
| `int lvglDelete(int h)` | Objekt (samt Kindern) löschen. 1=ok |
| `int lvglClean(int h)` | Nur die Kinder eines Objekts löschen |

**Allgemeine Eigenschaften**

| Funktion | Beschreibung |
|---|---|
| `void lvglSetPos(int h, int x, int y)` | Absolute Position im Parent |
| `void lvglSetSize(int h, int w, int ht)` | Größe in Pixeln |
| `void lvglAlign(int h, int align, int dx, int dy)` | Ausrichtung im Parent (z. B. `9`=CENTER) + Offset |
| `void lvglSetText(int h, str)` | Label-/Checkbox-Text setzen |
| `void lvglSetBgColor(int h, int rgb888)` | Hintergrundfarbe (deckend) |
| `void lvglSetTextColor(int h, int rgb888)` | Textfarbe |
| `void lvglSetStyleInt(int h, int prop, int val)` | Generischer Int-Style auf MAIN (z. B. `120`=RADIUS) |

**Events (Polling)**

| Funktion | Beschreibung |
|---|---|
| `void lvglEventEnable(int h, int filter)` | Events mit Code `filter` (0=ALL) von Objekt `h` in den Ring leiten |
| `int lvglEvent()` | Nächstes Event in „current" holen; 1=erhalten, 0=leer |
| `int lvglEventObj()` | Handle des aktuellen Events |
| `int lvglEventCode()` | Code des aktuellen Events (z. B. `10`=CLICKED, `35`=VALUE_CHANGED) |

**Wert-Widgets**

| Funktion | Beschreibung |
|---|---|
| `int lvglSlider(int parent)` / `lvglBar` / `lvglArc` | Slider / Bar / Arc erzeugen |
| `int lvglSwitch(int parent)` / `lvglCheckbox(int parent)` | Switch / Checkbox erzeugen |
| `void lvglSetValue(int h, int v, int anim)` | Wert setzen (Slider/Bar/Arc). `anim`=1 animiert (Arc ignoriert es) |
| `int lvglGetValue(int h)` | Aktueller Wert (Slider/Bar/Arc) |
| `void lvglSetRange(int h, int min, int max)` | Wertebereich |
| `void lvglSetChecked(int h, int on)` | Checked-Zustand setzen (Switch/Checkbox) |
| `int lvglIsChecked(int h)` | 1 wenn checked |

**Diagramm & Bild**

| Funktion | Beschreibung |
|---|---|
| `int lvglChart(int parent)` | Diagramm erzeugen |
| `void lvglChartType(int h, int type)` | `1`=LINE, `2`=BAR |
| `int lvglChartSeries(int chart, int rgb888)` | Farbige Serie hinzufügen; liefert Serien-Handle |
| `void lvglChartNext(int chart, int series, int v)` | Nächsten Wert einschieben (scrollend) |
| `void lvglChartRange(int chart, int axis, int min, int max)` | Y-Achsen-Bereich (`axis` 0=PRIMARY_Y) |
| `void lvglChartCount(int chart, int n)` | Anzahl der Punkte |
| `void lvglChartUpdateMode(int chart, int mode)` | `0` = schieben (LVGL-Vorgabe), `1` = umlaufend. **Entscheidet, ob eine schnelle Messkurve ueberhaupt darstellbar ist** — siehe Hinweis unten. |

> **⭐ Schnelle Messkurven: der Modus zaehlt, nicht nur die Rate.**
> Beim **Schieben** wandern bei jedem neuen Wert *alle* Punkte, also wird die **ganze
> Diagrammflaeche** ungueltig und neu gezeichnet. **Umlaufend** ueberschreibt den aeltesten
> Wert an Ort und Stelle — ein wandernder Schreibstrich wie auf einem Krankenhausmonitor —
> und nur eine schmale Spalte wird ungueltig. Bei einer 760x300-Flaeche mit 250 Punkten sind
> das rund 228.000 Pixel je Wert gegen etwa 900, also Faktor ~250. Bei 250 Hz (EKG) ist
> Schieben aussichtslos und Umlaufen unproblematisch. LVGL steht auf Schieben, der Aufruf
> ist also noetig.
>
> Wer beim Schieben bleibt, entkoppelt stattdessen die Raten: mit voller Geschwindigkeit in
> einen Ringpuffer abtasten und je Bild 8-10 Punkte anhaengen, 25-30 Bilder/s. Mehr sieht
> das Auge ohnehin nicht.
| `int lvglImage(int parent)` | Bild erzeugen |
| `void lvglImageSrc(int h, str path)` | Bildquelle aus LVGL-FS-Pfad (z. B. `"A:/logo.bin"` oder `"A:/img.png"` — PNG-Dekodierung ist eingebaut) |
| `void lvglImageAngle(int h, int deci_deg)` | Bild drehen, 0,1°-Einheiten (3600 = 360°) — z. B. ein Uhrzeiger |
| `void lvglImagePivot(int h, int x, int y)` | Drehpunkt setzen, px ab Bild-Ecke oben links (Standard = Bildmitte) |
| `void lvglSetFont(int h, int size)` | Schriftgröße eines Labels setzen; auf die eingebauten Montserrat-Größen (10/14/20/28) gerundet. |
| `void lvglImageScale(int h, int sx, int sy)` | Bild pro Achse skalieren, 256 = 100% (z. B. ein pulsierendes Cover). |
| `int lvglCanvas(int parent)` | Ein Live-Bild-Objekt mit rohem RGB565-Puffer erzeugen. Trotz des Namens ist es ein `lv_image`, also wirken `lvglImageScale`/`lvglImageAngle`/`lvglImagePivot` darauf — für Kamera-Frames oder jeden dynamischen Pixelpuffer. |
| `void lvglCanvasSetImgSlot(int canvas, int img_slot)` | Ein `lvglCanvas` ohne Kopie auf einen PSRAM-RGB565-**Bild-Slot** (0-3, z. B. aus `dspLoadImageFromCam`) zeigen lassen und neu zeichnen. Der Slot muss bis zum nächsten Aufruf gültig bleiben; den Slot des Vorframes mit `dspFreeImage` freigeben. So wird ein Live-Kamerabild auf dem LVGL-Screen getrieben (siehe `examples/cam_view_lvgl.tc`). |
| `int lvglLine(int parent)` | Ein Linien-Objekt erstellen. |
| `void lvglLinePoints(int h, int x1, int y1, int x2, int y2)` | Die zwei Endpunkte einer Linie setzen. |
| `void lvglLineStyle(int h, int rgb, int width)` | Farbe (0xRRGGBB) und Breite (px) einer Linie setzen. |

Beispiele: `lvgl_demo.tc` (Button → Label), `lvgl_widgets.tc` (Slider/Bar/Switch), `lvgl_chart.tc`
(Live-Diagramm), `lvgl_smoke.tc` (Bring-up-Test).

### Debug

| Funktion      | Beschreibung                    |
|---------------|---------------------------------|
| `dumpVM()`    | VM-Zustand auf Konsole ausgeben |
| `int vmStackDepth()` | Liefert die aktuelle Tiefe des Operanden-Stacks. Diagnose fuer Stack-Lecks in Skripten/Callback-Ketten — an gleicher Stelle ueber mehrere Durchlaeufe aufrufen; der Wert sollte konstant bleiben. |

---

## Multi-VM Slots (ESP32)

Auf dem ESP32 koennen bis zu **6 unabhaengige TinyC-Programme** gleichzeitig in separaten VM-Slots laufen. Jeder Slot hat eigenen Bytecode, Globals, Stack, Heap und Ausgabepuffer. Speicher wird dynamisch allokiert — leere Slots kosten null Bytes, nicht-autoexec Slots verwenden Lazy Loading (nur ~33 Bytes bis zum ersten Start). Der ESP8266 unterstuetzt nur 1 Slot.

### Slot-Konfiguration

Slot-Zuweisungen und Autoexec-Flags werden in `/tinyc.cfg` auf dem Dateisystem gespeichert. Diese Datei wird automatisch erstellt und aktualisiert, wenn ein Programm geladen, hochgeladen oder das Autoexec-Flag umgeschaltet wird. Eine manuelle Bearbeitung ist nicht notwendig.

Beispiel `/tinyc.cfg`:
```
/weather.tcb,1
/display.tcb,1
/logger.tcb,0
,0
_info,0
```

Jede Zeile entspricht einem Slot (0–3): `Dateiname,Autoexec-Flag`. Die letzte Zeile `_info,<0|1>` steuert, ob Debug-Statuszeilen auf der Tasmota-Hauptseite angezeigt werden.

### Tasmota-Befehle

Alle Befehle verwenden standardmaessig Slot 0, wenn keine Slot-Nummer angegeben wird (abwaertskompatibel).

| Befehl                        | Beschreibung                                     |
|-------------------------------|--------------------------------------------------|
| `TinyC`                       | Status aller Slots anzeigen (JSON)               |
| `TinyCRun [slot] [/datei.tcb]`| Slot starten (optional Datei vorher laden)       |
| `TinyCStop [slot]`            | Slot stoppen                                     |
| `TinyCReset [slot]`           | Slot stoppen und zuruecksetzen                   |
| `TinyCExec <n>`               | Instruktionen pro Tick setzen (Standard 1000)    |
| `TinyCInfo 0\|1`              | VM-Debug-Zeilen auf Hauptseite ein-/ausblenden   |
| `TinyCIde [url]`              | Browser-IDE aus dem Repo (oder einer URL) aktualisieren; ersetzt `/tinyc_ide.html.gz`, kein Dateimanager (benoetigt `USE_UFILESYS`) |

> ⚠️ **Ein Firmware-Flash tauscht die IDE NICHT mit aus.** Die Browser-IDE ist eine *Datei im Geraete-Dateisystem* (`/tinyc_ide.html.gz`) und nicht Teil des Firmware-Abbilds. Nach dem Flashen einer Version mit neuen Syscalls kennt die alte IDE diese nicht, und der Compiler meldet `Undefined function: <name>` — obwohl die Firmware es koennte. Nach jedem Firmware-Update, das neue Built-ins bringt, einmal **`TinyCIde`** in der Konsole aufrufen und die Browser-Seite hart neu laden.
| `TinyC ?<abfrage>`            | Globale Variablen per Index abfragen (siehe unten)|

**Beispiele:**
```
TinyCRun                    → Slot 0 starten
TinyCRun /weather.tcb       → Datei in Slot 0 laden und starten
TinyCRun 2 /logger.tcb      → Datei in Slot 2 laden und starten
TinyCStop 1                 → Slot 1 stoppen
TinyCReset 3                → Slot 3 zuruecksetzen
TinyCInfo 1                 → Debug-Info auf Hauptseite anzeigen
```

### Web-Konsole (`/tc`)

Die TinyC-Konsolenseite unter `/tc` zeigt eine kompakte Uebersicht aller Slots:

- **Statusanzeige**: gruener Punkt = aktiv (laeuft oder Callback-bereit), orange = geladen aber nicht gestartet, grau = leer
- **Run / Stop Buttons**: kontextabhaengig — Run ausgegraut wenn aktiv, Stop ausgegraut wenn inaktiv
- **A-Button**: schaltet Auto-Ausfuehrung beim Booten um (gruen = aktiviert). Wird sofort in `/tinyc.cfg` gespeichert
- **Programm laden**: Dateiauswahl mit Slot-Dropdown um beliebige `.tcb`-Datei in beliebigen Slot zu laden
- **Repository**: wenn `/tinyc_repo.cfg` auf dem Dateisystem existiert, wird ein entferntes Programm-Repository angezeigt (siehe unten)
- **Programm hochladen**: Datei-Upload mit Slot-Dropdown um eine `.tcb`-Datei direkt hochzuladen

### Programm-Repository

TinyC unterstuetzt das Herunterladen vorkompilierter `.tcb`-Programme aus einem entfernten Repository direkt auf dem Geraet.

**Einrichtung:**
1. Erstelle eine Datei `/tinyc_repo.cfg` auf dem Geraete-Dateisystem mit der Basis-URL des Repositories (eine Zeile):
   ```
   https://raw.githubusercontent.com/gemu2015/Sonoff-Tasmota/universal/tasmota/tinyc/bytecode
   ```
2. Das Repository muss eine `index.txt`-Datei enthalten, die die verfuegbaren `.tcb`-Dateien auflistet (ein Dateiname pro Zeile):
   ```
   blink.tcb
   bme280.tcb
   lcd_i2c.tcb
   onewire.tcb
   ```
3. Die `.tcb`-Dateien muessen unter `<basis_url>/<dateiname>` erreichbar sein

**Verwendung:**
Wenn `/tinyc_repo.cfg` vorhanden ist, zeigt die TinyC-Konsolenseite ein zusaetzliches **Repository**-Feld mit:
- Einem Dropdown mit allen `.tcb`-Dateien aus der entfernten `index.txt`
- Einem Slot-Waehler
- Einem **Download & Load**-Button, der die ausgewaehlte Datei auf das Geraete-Dateisystem herunterlaed und in den gewaehlten Slot laed

Das Standard-Repository unter `https://raw.githubusercontent.com/gemu2015/Sonoff-Tasmota/universal/tasmota/tinyc/bytecode` enthaelt Beispielprogramme fuer Sensoren, Displays, Charts und mehr. Lade die mitgelieferte `tinyc_repo.cfg` auf dein Geraet hoch um es zu aktivieren.

### API-Endpunkte

Die JSON-API unter `/tc_api` unterstuetzt einen `slot`-Parameter:

```
GET /tc_api?cmd=run&slot=2     → Slot 2 starten
GET /tc_api?cmd=stop&slot=1    → Slot 1 stoppen
GET /tc_api?cmd=status         → Status aller Slots
POST /tc_upload?slot=3&api=1   → .tcb in Slot 3 hochladen (JSON-Antwort)
```

### Variablen-Abfrage — `_Q()` Makro (Google Charts)

Globale TinyC-Variablen koennen per HTTP als JSON abgefragt werden, um Live-Dashboards mit Google Charts oder anderen JavaScript-Charting-Bibliotheken zu erstellen.

Das `_Q()` Makro wird zur **Kompilierzeit** in String-Literalen expandiert. Der Compiler ersetzt Variablennamen durch ihre Indizes und Typen — das Binary enthaelt keine Variablennamen, nur kompakte Index-basierte Abfragen.

**Syntax:** `_Q(var1, var2, ...)`

Der Compiler ersetzt `_Q(...)` durch einen index-kodierten Abfrage-String:
- `<index>i` — int-Skalar
- `<index>f` — float-Skalar
- `<index>s<n>` — char[n] String
- `<index>I<n>` — int-Array mit n Elementen
- `<index>F<n>` — float-Array mit n Elementen

**Beispiel:** Bei den Globalen `float temperature; int counter;` wird der String:
```c
"TinyC+%3F_Q(temperature,counter)"
```
zur Kompilierzeit expandiert zu:
```
"TinyC+%3F0f;1i"
```

**Antwortformat:** JSON-Array in der Reihenfolge der Abfrage:
```json
{"TinyC":[23.5,42]}
```

**Verwendung im WebPage-Callback:**
```c
float temperature = 23.5;
int counter = 0;

void WebPage() {
    webSend("<script>fetch('/cm?cmnd=TinyC+%3F_Q(temperature,counter)')");
    webSend(".then(r=>r.json()).then(d=>{var v=d.TinyC;");
    webSend("// v[0]=temperature, v[1]=counter");
    webSend("});</script>");
}
```

Fuer einen bestimmten Slot wird die Slot-Nummer vorangestellt:
```
TinyC ?2 0f;1i      → Abfrage aus Slot 2
```

### Boot-Ablauf

Beim Booten liest TinyC `/tinyc.cfg` und:
1. Laedt jede konfigurierte `.tcb`-Datei in ihren Slot
2. Startet automatisch Slots mit gesetztem Autoexec-Flag (`1`)

Wenn keine `/tinyc.cfg` existiert (erster Start), werden keine Programme geladen.

### Ressourcenverbrauch

Jeder VM-Slot verbraucht ca. **3,2 KB RAM** (nur Struct, ohne Programm-Bytecode). Slots werden dynamisch allokiert — nur aktive Slots verbrauchen Speicher. Das Slot-Pointer-Array selbst benoetigt nur 24 Bytes. Nicht-autoexec Slots verwenden Lazy Loading: nur der Dateiname (~33 Bytes) wird gespeichert bis zum ersten Start.

| Ressource             | Kosten                       |
|-----------------------|------------------------------|
| Pointer-Array         | 16 Bytes (4 Zeiger)          |
| Pro-Slot Struct       | ~3,2 KB                      |
| Programm-Bytecode     | variabel (malloc)            |
| Heap (alle Arrays)    | max 32 KB, bei Bedarf allokiert |

### Callbacks mit mehreren Slots

Jeder Slot erhaelt seine eigenen Callbacks unabhaengig:

- `EverySecond()`, `Every100ms()`, `Every50ms()` — werden an alle aktiven Slots verteilt
- `WebCall()` — jeder Slot kann eigene Sensorzeilen zur Hauptseite hinzufuegen
- `JsonCall()` — jeder Slot fuegt eigene Telemetriedaten hinzu
- `TaskLoop()` — laeuft im eigenen FreeRTOS-Task des Slots (ESP32)
- `CleanUp()` — wird auf allen Slots vor Geraete-Neustart aufgerufen

Geteilte Ressourcen (UDP, SPI, Datei-Handles) sind global — nur ein Slot sollte diese gleichzeitig nutzen.

### Beispiel: Zwei Programme nebeneinander

Slot 0 — Temperaturueberwachung:
```c
int temp = 0;
void EverySecond() { temp = tasm_analog0; }
void WebCall() {
    char buf[64];
    sprintf(buf, "{s}Temperatur{m}%d{e}", temp);
    webSend(buf);
}
int main() { return 0; }
```

Slot 1 — Betriebszeitzaehler:
```c
int uptime = 0;
void EverySecond() { uptime++; }
void WebCall() {
    char buf[64];
    sprintf(buf, "{s}Betriebszeit{m}%d s{e}", uptime);
    webSend(buf);
}
int main() { return 0; }
```

Beide zeigen ihre Sensorzeilen gleichzeitig auf der Tasmota-Hauptseite an.

---

## VM-Grenzen

| Ressource         | ESP8266  | ESP32    | Browser  | Anmerkungen                        |
|--------------------|----------|----------|----------|------------------------------------|
| Stack-Tiefe        | 64       | 256      | 256      | Operandenstack-Eintraege           |
| Aufrufrahmen       | 8        | 32       | 32       | Maximale Rekursions-/Aufruftiefe   |
| Lokale pro Rahmen  | 256      | 256      | 256      | Skalare + kleine Arrays ≤16 inline  |
| Globale Variablen  | 64       | 256      | 256      | Skalare + kleine Arrays ≤16 inline  |
| Codegroesse        | 4 KB     | 128 KB   | 64 KB    | Bytecode; ESP32 faellt bei DRAM-Mangel auf PSRAM zurueck |
| Heap-Speicher      | 8 KB     | 32 KB    | 64 KB    | Fuer Arrays >16 Elemente (autom. Allokation) |
| Heap-Handles       | 8        | 32       | 32       | Max. gleichzeitige Heap-Allokationen |
| Konstantenpool     | 32       | 1024     | 65536    | Zeichenketten- & Float-Konstanten (DRAM, ESP32 faellt auf PSRAM zurueck) |
| Instruktionslimit  | 1M       | 1M       | 1M       | Sicherheitslimit pro Ausfuehrung   |
| GPIO-Pins          | 40       | 40       | 40       | Pins 0–39 (im Browser simuliert)   |
| Datei-Handles      | 4        | 4        | 8        | Gleichzeitig geoeffnete Dateien    |
| VM-Slots           | 1        | 6        | 1        | Gleichzeitige Programme            |
| Cross-VM-Share     | n/a      | 32 Keys  | n/a      | Treiber-globale Share-Tabelle (nur ESP32) |

**ESP32 PSRAM-Fallback (seit v1.3.19):** `TC_MAX_PROGRAM` von 64 KB auf 128 KB angehoben. Bytecode-Puffer (`s->program`) und Konstantendaten-Pool (`vm->const_data`) werden zuerst aus dem internen DRAM allokiert; bei OOM faellt die Allokation automatisch auf `heap_caps_malloc(MALLOC_CAP_SPIRAM)` zurueck. Kleine/normale Skripte bleiben im schnellen statischen RAM; nur sehr grosse Programme (100+ KB) landen im PSRAM. Ein `AddLog`-INFO-Eintrag wird ausgegeben, wenn der PSRAM-Pfad genutzt wird.

---

## Geraetedateiverwaltung (IDE)

### IDE-Installation

Die IDE-Datei (`tinyc_ide.html.gz`) kann entweder auf dem **Flash-Dateisystem** oder der **SD-Karte** liegen — je nachdem, welches als Benutzer-Dateisystem (`ufsp`) gemountet ist. Laden Sie `tinyc_ide.html.gz` ueber die Tasmota-Seite **Dateisystem verwalten** hoch.

> **Hinweis:** TinyC-Skripte und Datendateien (`.tc`, `.tcb` usw.) werden ebenfalls auf dem Benutzer-Dateisystem (`ufsp`) gespeichert.

### Dateioperationen

Die IDE-Werkzeugleiste enthaelt Steuerelemente zur Verwaltung von Dateien auf dem Tasmota-Geraetedateisystem:

- **Geraetedateien-Dropdown** — Listet alle Dateien auf dem Geraet auf. Waehlen Sie eine Datei, um sie in den Editor zu laden. Die Liste zeigt Dateiname und Groesse (z.B. `config.tc (1.2KB)`).
- **Datei-Speichern-Schaltflaeche** — Speichert den aktuellen Editorinhalt als Datei auf dem Geraet. Fragt nach einem Dateinamen (Standard ist der aktuelle Dateiname).
- **Automatische Aktualisierung** — Die Dateiliste wird automatisch aktualisiert, wenn die Geraete-IP eingegeben oder geaendert wird, und nach jedem Speichervorgang.

Alle Dateioperationen verwenden den `/tc_api`-Endpunkt mit CORS-Unterstuetzung, sodass die IDE von jedem Browser aus verwendet werden kann — sie muss nicht vom Geraet bereitgestellt werden.

### API-Endpunkte

| Endpunkt | Methode | Beschreibung |
|----------|---------|-------------|
| `/tc_api?cmd=listfiles` | GET | Gibt JSON-Liste der Dateien zurueck: `{"ok":true,"files":[{"name":"x","size":123},...]}` |
| `/tc_api?cmd=readfile&path=/name` | GET | Gibt Dateiinhalt als Klartext zurueck |
| `/tc_api?cmd=readfile&path=/name@von_bis` | GET | Gibt zeitgefilterte CSV-Daten zurueck (siehe unten) |
| `/tc_api?cmd=writefile&path=/name` | POST | Schreibt POST-Body in Datei, gibt `{"ok":true,"size":N}` zurueck |
| `/tc_api?cmd=deletefile&path=/name` | GET | Loescht eine Datei vom Dateisystem |

### Zeitbereichs-gefilterter Dateizugriff

Haengen Sie `@von_bis` an den Dateipfad an, um nur Zeilen innerhalb eines Zeitbereichs aus einer CSV-Datendatei zu extrahieren. Dies ist nuetzlich fuer die Bereitstellung von IoT-Zeitreihendaten an Chart-Bibliotheken.

**URL-Format:**
```
/tc_api?cmd=readfile&path=/data.csv@TT.MM.JJ-HH:MM_TT.MM.JJ-HH:MM
```

**Beispiel:**
```
/tc_api?cmd=readfile&path=/sml.csv@1.1.24-00:00_31.1.24-23:59
```

Sowohl das deutsche (`TT.MM.JJ HH:MM`) als auch das ISO-Format (`JJJJ-MM-TTTHH:MM:SS`) werden unterstuetzt. Der `_` (Unterstrich) trennt die Von- und Bis-Zeitstempel.

**Antwort:** Die Kopfzeile (erste Zeile) wird immer eingeschlossen, gefolgt von nur den Datenzeilen, deren Zeitstempel in der ersten Spalte innerhalb `[von..bis]` liegt. Zeilen nach dem Endzeitstempel werden effizient uebersprungen (fruehzeitiger Abbruch).

**Leistungsoptimierung:** Wenn eine Indexdatei existiert (gleicher Name mit `.ind`-Erweiterung, enthaltend `Zeitstempel\tByte-Offset`-Zeilen), werden Byte-Offsets verwendet, um direkt zur Startposition zu springen. Andernfalls wird eine geschaetzte Positionssuche basierend auf dem ersten und letzten Zeitstempel der Datei durchgefuehrt (aehnlich wie Scripters `opt_fext`).

### Port 82 Download-Server (ESP32)

Fuer grosse Datenbankdateien kann der zeitgefilterte Dateizugriff auf Port 80 die Haupt-Webserver-Schleife blockieren. TinyC enthaelt einen dedizierten **Port 82 Download-Server**, der Dateien in einem FreeRTOS-Hintergrund-Task bereitstellt und das Geraet waehrend grosser Uebertragungen reaktionsfaehig haelt.

**URL-Format:**
```
http://<ip>:82/ufs/<dateiname>
http://<ip>:82/ufs/<dateiname>@von_bis
```

**Beispiele:**
```
http://192.168.1.100:82/ufs/sml.csv
http://192.168.1.100:82/ufs/sml.csv@1.1.24-00:00_31.1.24-23:59
```

**Eigenschaften:**
- Laeuft in einem dedizierten FreeRTOS-Task (angeheftet an Core 1, Prioritaet 3)
- Blockiert nicht die Tasmota-Hauptschleife oder Weboberflaeche
- Unterstuetzt die gleiche `@von_bis` Zeitbereichsfilterung wie `/tc_api` readfile
- Verwendet Chunked-Transfer-Encoding fuer gefilterte Antworten
- Content-Disposition-Header fuer Browser-Download
- Ein Download gleichzeitig (gibt HTTP 503 zurueck wenn beschaeftigt)
- Automatische MIME-Typ-Erkennung (`.csv`/`.txt` = text/plain, `.html`, `.json`)
- Der Port kann durch Definition von `TC_DLPORT` vor der Kompilierung geaendert werden (Standard: 82)

### Typischer Arbeitsablauf

1. Geraete-IP in die Werkzeugleiste eingeben
2. Das **Geraetedateien**-Dropdown fuellt sich automatisch mit allen Dateien auf dem Geraet
3. Datei auswaehlen, um sie in den Editor zu laden — oder neuen Code schreiben
4. **Datei speichern** klicken, um den Quellcode auf dem Geraet zu speichern (z.B. als `myapp.tc`)
5. **Auf Geraet ausfuehren** klicken, um zu kompilieren, die `.tcb`-Binaerdatei hochzuladen und die Ausfuehrung zu starten

So koennen Sie TinyC-Quelldateien zusammen mit ihrem kompilierten Bytecode auf dem Geraet aufbewahren, was das Bearbeiten von Programmen direkt ohne lokale Dateispeicherung erleichtert.

## Tastenkuerzel (IDE)

| Tastenkuerzel      | Aktion                |
|--------------------|-----------------------|
| Ctrl + Enter       | Kompilieren           |
| Ctrl + Shift + Enter | Kompilieren & Ausfuehren |
| Ctrl + S           | Datei speichern       |
| Ctrl + O           | Datei oeffnen         |
| Ctrl + F           | Suchen                |
| Enter (in Suche)   | Naechstes finden      |
| Shift + Enter (in Suche) | Vorheriges finden |
| Escape             | Suchleiste schliessen |
| Tab (im Editor)    | 4 Leerzeichen einfuegen |

---

## Beispiele

Die IDE enthaelt 19 sofort einsatzbereite Beispiele im Dropdown "Beispiel laden..." — von einfachem Blinken bis zu Wetterstationsempfaengern und interaktiven WebUI-Dashboards.

### Hello World
```c
int main() {
    printStr("Hello, TinyC!\n");
    return 0;
}
```

### LED-Blinken
```c
#define LED 2
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09

int main() {
    gpioInit(LED, OUTPUT);
    while (true) {
        digitalWrite(LED, 1);
        delay(500);
        digitalWrite(LED, 0);
        delay(500);
    }
    return 0;
}
```

### Fibonacci
```c
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    for (int i = 0; i < 10; i++) {
        print(fib(i));
    }
    return 0;
}
```

### Zeichenkettenoperationen
```c
int main() {
    char greeting[32] = "Hello";
    char name[16] = "World";
    char buf[64];

    // Klassischer Funktionsstil
    strcpy(buf, greeting);
    strcat(buf, ", ");
    strcat(buf, name);
    strcat(buf, "!\n");
    printString(buf);       // Hello, World!

    // Dasselbe mit +-Operator
    buf = greeting;
    buf += ", ";
    buf += name;
    buf = buf + "!\n";
    printString(buf);       // Hello, World!

    // Formatierte Zeichenketten
    char line[64];
    sprintf(line, "count = %d", 42);
    printString(line);      // count = 42

    // Mehrere Werte mit sprintfAppend
    char report[128];
    sprintf(report, "Sensor %d", 1);
    sprintfAppend(report, " name=%s", name);
    sprintfAppend(report, " temp=%.1f", 23.5);
    printString(report);    // Sensor 1 name=World temp=23.5

    return 0;
}
```

### Bubble Sort
```c
void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int data[8] = {64, 34, 25, 12, 22, 11, 90, 1};
    bubbleSort(data, 8);
    for (int i = 0; i < 8; i++) {
        print(data[i]);
    }
    return 0;
}
```

### WebUI-Dashboard
```c
int power;
int brightness;
int mode;

void WebUI() {
    int page = webPage();
    if (page == 0) {
        webToggle(power, "Power");
        webSlider(brightness, 0, 100, "Brightness");
    }
    if (page == 1) {
        webPulldown(mode, "Mode", "Off|Auto|Manual");
    }
}

int main() {
    webPageLabel(0, "Controls");
    webPageLabel(1, "Settings");
    brightness = 50;
    return 0;
}
```

---

## Unterschiede zu Standard-C

| Merkmal                       | Standard-C         | TinyC                        |
|-------------------------------|--------------------|------------------------------|
| Zeiger                        | Volle Unterstuetzung | **Nicht unterstuetzt**     |
| Structs                       | Volle Unterstuetzung | Unterstuetzt: skalare Felder, Array-Felder (`char text[32]`), Member-Zugriff, Initialisierungslisten, zusammengesetzte Zuweisung. Keine verschachtelten Structs, keine Unions, keine Bit-Felder |
| Enums                         | Volle Unterstuetzung | Unterstuetzt: benannte/anonyme Enums, negative Werte, Auto-Inkrement, innerhalb von Funktionen |
| Dynamischer Speicher          | malloc/free        | Auto-Heap fuer Arrays >16 Elemente (kein explizites malloc) |
| Mehrdimensionale Arrays       | `int a[3][4]`      | **Nicht unterstuetzt**       |
| Zeichenkettentyp              | `char*`            | Nur `char arr[N]` — keine Zeigerarithmetik |
| Praeprozessor                 | Volles CPP         | `#define` (Konstanten + funktionsaehnliche Makros), `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif`/`#undef`, `#include "file.tc"` (Text-Paste zur Compile-Zeit, rekursiv, zyklus-sicher) |
| Header-Dateien                | `#include`         | `#include "file.tc"` unterstuetzt — Text-Paste vor der Praeprozessor-Verarbeitung; Aufloesung ist projekt-relativ (IDE) bzw. geraete-FS-relativ (`/cedit`) |
| typedef                       | Volle Unterstuetzung | Unterstuetzt: primitive Aliase, benannte Struct-Aliase, anonyme Struct-typedefs, verkettete Aliase, lokale typedefs |
| `const`                       | Typgeprueft        | Akzeptiert (Dokumentationshinweis, zur Laufzeit nicht erzwungen) |
| `static` lokale Variablen     | Volle Unterstuetzung | Unterstuetzt: nullinitialisiert, bleibt zwischen Aufrufen erhalten. Nicht-null-Initialisierer werden nicht ausgefuehrt |
| sizeof                        | Volle Unterstuetzung | Nur zur Uebersetzungszeit: `sizeof(typ)` und `sizeof(name)` unterstuetzt; `sizeof(ausdruck)` nicht unterstuetzt. Siehe [sizeof-Operator](#sizeof-operator) |
| Ternaerer Operator `?:`       | Volle Unterstuetzung | Unterstuetzt, auch verschachtelt; **String-Zweige** (`cond ? "a" : "b"`, Literale und/oder `char[]`) direkt als String-Argument nutzbar (sprintf `%s`, addLog, strcpy, webSend, …) |
| do-while                      | Volle Unterstuetzung | Unterstuetzt                 |
| Zusammengesetzte Zuweisungen  | Volle Unterstuetzung | Unterstuetzt: `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |
| Hex-Escape `\xNN`             | Volle Unterstuetzung | Unterstuetzt in String- und Char-Literalen |
| goto                          | Volle Unterstuetzung | **Nicht unterstuetzt**       |
| Funktionszeiger               | Volle Unterstuetzung | **Nicht unterstuetzt**       |
| Variadische Benutzerfunktionen | `va_list` etc.    | **Nicht unterstuetzt** (nur `sprintf`/`sprintfAppend` akzeptieren mehrere Argumente per Compiler-Expansion) |
| Standardbibliothek            | stdio, stdlib      | Nur eingebaute Funktionen (siehe [Eingebaute Funktionen](#eingebaute-funktionen)) |

---

*Generiert aus TinyC-Quellcode — lexer.js, parser.js, codegen.js, opcodes.js, vm.js*
