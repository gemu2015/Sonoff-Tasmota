# TinyC Sprachreferenz

**TinyC** ist eine Untermenge von C, die zu Bytecode fuer eine stackbasierte virtuelle Maschine kompiliert wird.
Es laeuft sowohl im Browser (JavaScript-VM) als auch auf ESP32/ESP8266 (als Tasmota-Treiber XDRV_124).

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
15. [VM-Grenzen](#vm-grenzen)
16. [Geraetedateiverwaltung (IDE)](#geraetedateiverwaltung-ide)
17. [Tastenkuerzel (IDE)](#tastenkuerzel-ide)
18. [Beispiele](#beispiele)

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
| `EverySecond()` | FUNC_EVERY_SECOND | Jede Sekunde | Periodische Aufgaben, Zaehler, langsames Polling |
| `JsonCall()` | FUNC_JSON_APPEND | Telemetriezyklus (~300s) | JSON zu MQTT-Telemetrie hinzufuegen |
| `WebPage()` | FUNC_WEB_ADD_MAIN_BUTTON | Seitenladen (einmalig) | Diagramme, benutzerdefiniertes HTML, Skripte |
| `WebCall()` | FUNC_WEB_SENSOR | Webseitenaktualisierung (~1s) | Sensorzeilen zur Tasmota-Weboberflaeche hinzufuegen |
| `WebUI()` | AJAX /tc_ui Aktualisierung | Alle 2s + bei Widget-Aenderung | Interaktives Widget-Dashboard (Schaltflaechen, Regler usw.) |
| `UdpCall()` | UDP-Paket empfangen | Bei jeder Multicast-Variable | Eingehende UDP-Variablen verarbeiten |
| `WebOn()` | Benutzerdefinierter HTTP-Endpunkt | Bei Anfrage an `webOn()`-URL | REST-APIs, JSON-Endpunkte, Webhooks |
| `TaskLoop()` | FreeRTOS-Task (ESP32) | Kontinuierliche Schleife im eigenen Task | Hintergrundverarbeitung, unabhaengig vom Haupt-Thread |

### Ausfuehrungsmodell

1. **`main()`** laeuft zuerst in einem FreeRTOS-Task (ESP32) — `delay()` funktioniert als echte blockierende Verzoegerung
2. Nach dem Anhalten von main bleiben **Globale und Heap erhalten** — sie werden NICHT freigegeben
3. Tasmota ruft periodisch Ihre Callbacks auf, die Globale lesen/aendern koennen
4. Callbacks laufen synchron mit einer Instruktionsbegrenzung — kein `delay()` erlaubt
5. Wenn `TaskLoop()` definiert ist, laeuft es im selben FreeRTOS-Task nach dem Anhalten von main() — `delay()` funktioniert, laeuft unabhaengig von Tasmota's Haupt-Thread

### Tasmota-Ausgabefunktionen

Verwenden Sie diese Funktionen in Callbacks, um Daten an Tasmota zu senden:

| Funktion | Beschreibung | Verwenden in |
|----------|-------------|--------------|
| `responseAppend(buf)` | Char-Array an JSON-Telemetrie anfuegen (-> `ResponseAppend_P`) | `JsonCall()` |
| `responseAppend("literal")` | Zeichenketten-Literal an JSON-Telemetrie anfuegen | `JsonCall()` |
| `webSend(buf)` | Char-Array an Webseite senden (-> `WSContentSend`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webSend("literal")` | Zeichenketten-Literal an Webseite senden | `WebPage()` / `WebCall()` / `WebOn()` |
| `webFlush()` | Web-Inhaltspuffer zum Client leeren (-> `WSContentFlush`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webFile("filename")` | Dateiinhalt vom Dateisystem an Webseite senden | `WebPage()` / `WebCall()` / `WebUI()` / `WebOn()` |

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
    sprintfInt(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

void WebCall() {
    // Fuegt eine Zeile zur Tasmota-Webseite hinzu
    char buf[64];
    sprintfInt(buf, "{s}TinyC Counter{m}%d{e}", counter);
    webSend(buf);
}

int main() {
    counter = 0;
    return 0;
}
```

**Ergebnis:** Nach dem Hochladen und Ausfuehren zeigt die Tasmota-Webseite eine "TinyC Counter"-Zeile, die jede Sekunde hochzaehlt, und die MQTT-Telemetrie enthaelt `,"TinyC":{"Count":N}`.

### TaskLoop-Beispiel (ESP32)

```c
int counter = 0;

void TaskLoop() {
    counter++;
    char buf[64];
    sprintfInt(buf, "TaskLoop count=%d", counter);
    addLog(buf);       // erscheint im Tasmota-Konsolenlog
    delay(1000);       // echte 1-Sekunden-Verzoegerung, blockiert Tasmota nicht
}

void JsonCall() {
    char buf[64];
    sprintfInt(buf, ",\"TinyC\":{\"Count\":%d}", counter);
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
- Verwenden Sie `WebPage()` fuer einmaligen Seiteninhalt (Diagramme, Skripte) — wird einmal beim Laden der Seite aufgerufen
- Verwenden Sie `WebCall()` fuer Sensor-aehnliche Zeilen, die periodisch aktualisiert werden
- Verwenden Sie `UdpCall()` zur Verarbeitung eingehender UDP-Multicast-Variablen
- `TaskLoop()` laeuft in einem eigenen FreeRTOS-Task (nur ESP32) — kann `delay()` frei verwenden, VM-Zugriff ist Mutex-serialisiert mit Haupt-Thread-Callbacks

---

## Tasmota-Systemvariablen

TinyC stellt virtuelle `tasm_*`-Variablen bereit, die den Tasmota-Systemzustand direkt lesen/schreiben. Sie werden wie normale Variablen verwendet — keine Funktionsaufrufe noetig. Der Compiler uebersetzt sie automatisch in Syscalls.

### Verfuegbare Variablen

| Variable | Typ | L/S | Beschreibung |
|----------|-----|-----|--------------|
| `tasm_wifi` | int | lesen | WiFi-Status (1 = verbunden, 0 = getrennt) |
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
    sprintfFloat(buf, "{s}Temp{m}%.1f C{e}", tasm_temp);
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
```

### Zugriff
```c
int x = data[0];       // lesen
data[3] = 42;          // schreiben
data[i + 1] = data[i]; // berechneter Index
```

### Gueltigkeitsbereich
- **Globale Arrays** — im globalen Datenspeicher gespeichert (bis zu 255 Elemente)
- **Lokale Arrays** — im lokalen Rahmen der Funktion gespeichert (bis zu 255 Elemente)
- **Heap-Arrays** — Arrays mit mehr als 255 Elementen werden automatisch auf dem dynamischen Heap gespeichert

### Grosse Arrays (Heap)

Arrays mit mehr als 255 Elementen werden vom Compiler **automatisch** in den Heap-Speicher weitergeleitet. Keine spezielle Syntax noetig — der Compiler erkennt die Groesse und allokiert transparent auf dem Heap:

```c
float data[2000];      // automatisch -> Heap (2000 > 255)
int small[10];         // bleibt in Globalen (10 <= 255)

int main() {
    data[1999] = 3.14;  // Heap-Zugriff — gleiche Syntax wie regulaere Arrays
    small[0] = 42;      // globaler Zugriff
    return 0;
}
```

Heap-Arrays unterstuetzen alle gleichen Operationen wie regulaere Arrays: Elementzugriff, Zeichenkettenoperationen auf `char[]`, Uebergabe an Funktionen usw.

**Heap-Grenzen:**

| Plattform | Max. Heap-Slots | Max. Handles |
|-----------|-----------------|--------------|
| ESP8266   | 2.048 (8 KB)    | 8            |
| ESP32     | 8.192 (32 KB)   | 16           |
| Browser   | 16.384 (64 KB)  | 32           |

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

Einen einzelnen Wert in ein char-Array formatieren:

```c
char line[64];
sprintfInt(line, "x = %d", 42);            // "x = 42"
sprintfFloat(line, "pi = %.2f", 3.14);     // "pi = 3.14"
sprintfStr(line, "name: %s", name);        // "name: World"
```

### Mehrteilige Zeichenketten erstellen (sprintfAppend)

Da TinyC keine variadischen Funktionen hat, verwenden Sie `sprintfAppend`-Varianten, um
mehrere Werte in einen Puffer zu verketten. Sie fuegen am aktuellen Ende der Zeichenkette an:

```c
char report[128];
sprintfInt(report, "Sensor %d", 1);              // "Sensor 1"
sprintfAppendStr(report, " name=%s", name);       // "Sensor 1 name=World"
sprintfAppendInt(report, " val=%d", 42);          // "Sensor 1 name=World val=42"
sprintfAppendFloat(report, " temp=%.1f", 3.14);   // "Sensor 1 name=World val=42 temp=3.1"
printString(report);
```

| Funktion | Beschreibung |
|----------|-------------|
| `sprintfInt(char dst[], "fmt", int val)` | Int in dst formatieren (ueberschreibt) |
| `sprintfFloat(char dst[], "fmt", float val)` | Float in dst formatieren (ueberschreibt) |
| `sprintfStr(char dst[], "fmt", char src[])` | Zeichenkette in dst formatieren (ueberschreibt) |
| `sprintfAppendInt(char dst[], "fmt", int val)` | Int formatieren und an dst anfuegen |
| `sprintfAppendFloat(char dst[], "fmt", float val)` | Float formatieren und an dst anfuegen |
| `sprintfAppendStr(char dst[], "fmt", char src[])` | Zeichenkette formatieren und an dst anfuegen |

**Format-Spezifikatoren:** `%d` (int), `%f` `%.2f` `%e` `%g` (float), `%s` (Zeichenkette). Jeder Aufruf verarbeitet genau einen `%`-Spezifikator.

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
| `strSub(char dst[], char src[], int pos, int len)` | `len` Zeichen ab `pos` (0-basiert, negativ=vom Ende) in dst kopieren. Gibt die tatsaechliche Laenge zurueck. |
| `strFind(char haystack[], char needle[])` | Erstes Vorkommen von needle in haystack finden. Gibt die Position (0-basiert) oder -1 zurueck, wenn nicht gefunden. |

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

**Einschraenkungen:**
- Kein `#include`

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

## Eingebaute Funktionen

### Ausgabe

| Funktion                | Beschreibung                       |
|-------------------------|-------------------------------------|
| `print(int value)`      | Ganzzahl + Zeilenumbruch ausgeben   |
| `printStr("literal")`   | Zeichenketten-Literal ausgeben      |
| `printString(char arr[])` | Null-terminiertes Char-Array ausgeben |

### GPIO

| Funktion                             | Beschreibung                          |
|--------------------------------------|---------------------------------------|
| `pinMode(int pin, int mode)`         | Pin-Modus setzen (0=INPUT, 1=OUTPUT)  |
| `digitalWrite(int pin, int value)`   | HIGH(1) oder LOW(0) schreiben         |
| `int digitalRead(int pin)`           | Pin-Zustand lesen                     |
| `int analogRead(int pin)`            | Analogwert lesen (0–4095)             |
| `analogWrite(int pin, int value)`    | PWM-Wert schreiben                    |
| `gpioInit(int pin, int mode)`        | Pin von Tasmota freigeben + pinMode   |

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

| Funktion                          | Beschreibung                            |
|-----------------------------------|-----------------------------------------|
| `serialBegin(int baud)`           | Seriell mit Baudrate initialisieren     |
| `serialPrint("literal")`          | Zeichenkette auf seriell ausgeben       |
| `serialPrintInt(int value)`       | Ganzzahl auf seriell ausgeben           |
| `serialPrintFloat(float value)`   | Gleitkommazahl auf seriell ausgeben     |
| `serialPrintln("literal")`        | Zeichenkette + Zeilenumbruch auf seriell |
| `int serialRead()`                | Byte lesen (-1 wenn keines verfuegbar)  |
| `int serialAvailable()`           | Verfuegbare Bytes zum Lesen             |

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

Einen einzelnen Wert in ein char-Array formatieren. Jede Funktion verarbeitet einen `%`-Spezifikator.

| Funktion | Beschreibung |
|----------|-------------|
| `int sprintfInt(char dst[], "fmt", int val)` | Int in dst formatieren (ueberschreibt) |
| `int sprintfFloat(char dst[], "fmt", float val)` | Float in dst formatieren (ueberschreibt) |
| `int sprintfStr(char dst[], "fmt", char src[])` | Zeichenkette in dst formatieren (ueberschreibt) |
| `int sprintfAppendInt(char dst[], "fmt", int val)` | Int formatieren, an Ende von dst anfuegen |
| `int sprintfAppendFloat(char dst[], "fmt", float val)` | Float formatieren, an Ende von dst anfuegen |
| `int sprintfAppendStr(char dst[], "fmt", char src[])` | Zeichenkette formatieren, an Ende von dst anfuegen |

**Format-Spezifikatoren:** `%d` (int), `%f` `%.Nf` `%e` `%g` (float), `%s` (Zeichenkette).
Alle Funktionen geben die Gesamtlaenge der Zeichenkette zurueck.

```c
// Mehrteilige Zeichenkette durch Verkettung von Append-Aufrufen erstellen:
char buf[128];
sprintfInt(buf, "ID=%d", 1);
sprintfAppendStr(buf, " name=%s", name);
sprintfAppendFloat(buf, " val=%.1f", 3.14);
// buf = "ID=1 name=World val=3.1"
```

### Datei-E/A

Dateien auf dem ESP32-Dateisystem (LittleFS) lesen und schreiben. In der Browser-IDE werden Dateien in einem virtuellen Dateisystem simuliert.

| Funktion                                   | Beschreibung                                          |
|--------------------------------------------|-------------------------------------------------------|
| `int fileOpen("path", mode)`               | Datei oeffnen, gibt Handle (0–3) oder -1 bei Fehler zurueck |
| `int fileClose(handle)`                    | Datei-Handle schliessen, gibt 0 oder -1 zurueck      |
| `int fileRead(handle, char buf[], max)`    | Bis zu max Bytes in buf lesen, gibt Anzahl zurueck    |
| `int fileWrite(handle, char buf[], len)`   | len Bytes aus buf schreiben, gibt Anzahl zurueck      |
| `int fileExists("path")`                   | Pruefen ob Datei existiert: 1=ja, 0=nein             |
| `int fileDelete("path")`                   | Datei loeschen, gibt 0=ok, -1=Fehler zurueck         |
| `int fileSize("path")`                     | Dateigroesse in Bytes, -1 bei Fehler                  |

**Dateimodi:** `0` = Lesen, `1` = Schreiben (Erstellen/Abschneiden), `2` = Anfuegen

**Hinweise:**
- Dateipfade muessen Zeichenketten-Literale sein (z.B. `"/data.txt"`)
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
```

### Tasmota-Befehl

Einen beliebigen Tasmota-Konsolenbefehl ausfuehren und die JSON-Antwort erfassen.

| Funktion                                     | Beschreibung                                          |
|----------------------------------------------|-------------------------------------------------------|
| `int tasmCmd("command", char response[])`    | Befehl ausfuehren, Antwort speichern, Laenge zurueckgeben |

**Hinweise:**
- Der Befehl muss ein Zeichenketten-Literal sein (z.B. `"Status 0"`, `"Power ON"`)
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

| Funktion | Beschreibung |
|----------|-------------|
| `int httpGet(char url[], char response[])` | HTTP GET, gibt Antwortlaenge oder negativen Fehler zurueck |
| `int httpPost(char url[], char data[], char response[])` | HTTP POST, gibt Antwortlaenge oder negativen Fehler zurueck |
| `void httpHeader(char name[], char value[])` | Benutzerdefinierten Header fuer die naechste Anfrage setzen |

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

**Beispiel — Tasmota-Befehl an ein anderes Geraet:**
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

### mDNS-Dienstankuendigung

Das Geraet als mDNS-Dienst im lokalen Netzwerk registrieren, um Geraeteemulation zu ermoeglichen (Everhome ecotracker, Shelly oder benutzerdefinierte Dienste).

| Funktion | Beschreibung |
|----------|-------------|
| `int mdns("name", "mac", "type")` | mDNS-Responder starten und Dienst ankuendigen. Gibt 0 bei Erfolg zurueck |

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
    mdns("ecotracker-", "-", "everhome");
    return 0;
}
```

Dies entspricht Scripter's `mdns("ecotracker-", "-", "everhome")`.

### WebUI-Widgets

Interaktive Dashboards mit Widget-Funktionen erstellen. Widgets koennen an zwei Stellen erscheinen:

1. **Dedizierte `/tc_ui`-Seite** — verwenden Sie den `WebUI()`-Callback
2. **Tasmota-Hauptseite** (Sensorbereich) — verwenden Sie den `WebCall()`-Callback

Beide Callbacks verwenden die gleichen Widget-Funktionen.

| Funktion | Beschreibung |
|----------|-------------|
| `wButton(var, "label")` | Umschalt-Schaltflaeche (0/1) — zeigt EIN/AUS, Klick schaltet um |
| `wSlider(var, min, max, "label")` | Bereichsregler — ziehen zum Einstellen des Werts |
| `wCheckbox(var, "label")` | Kontrollkaestchen (0/1) — Aktivieren/Deaktivieren schaltet um |
| `wText(chararray, maxlen, "label")` | Texteingabe — Zeichenkettenvariable bearbeiten |
| `wNumber(var, min, max, "label")` | Zahleneingabe mit Min/Max-Grenzen |
| `wPulldown(var, "opt0\|opt1\|opt2")` | Dropdown-Auswahl — Pipe-getrennte Optionen, 0-basierter Index |
| `wRadio(var, "opt0\|opt1\|opt2")` | Optionsschaltflaechengruppe — Pipe-getrennte Optionen, 0-basierter Index |
| `wTime(var, "label")` | Zeitauswahl (HH:MM) — gespeichert als HHMM-Ganzzahl (z.B. 1430 = 14:30) |
| `wLabel(page, "label")` | Seite 0–5 mit einer Schaltflaechenbeschriftung auf der Hauptseite registrieren |
| `int wPage()` | Gibt die aktuelle Seitennummer zurueck, die gerendert wird (in `WebUI()` zur Verzweigung verwenden) |

Das erste Argument der Widget-Funktionen ist immer eine **globale Variable**, die das Widget liest und in die es schreibt. Der Compiler uebergibt automatisch die Adresse der Variable an den Syscall.

**Beispiel — Widgets auf der Hauptseite:**
```c
int relay;
int brightness;

void WebCall() {
    wButton(relay, "Power");
    wSlider(brightness, 0, 100, "Brightness");
}
```

**Beispiel — Mehrere Seiten mit benutzerdefinierten Schaltflaechen:**

Bis zu 6 Seiten koennen mit `wLabel()` registriert werden. Jede erstellt eine Schaltflaeche auf der Tasmota-Hauptseite. Verwenden Sie `wPage()` innerhalb von `WebUI()`, um verschiedene Widgets pro Seite zu rendern.

```c
int power;
int brightness;
int mode;
int alarm_time;
char devname[32];

void WebUI() {
    int page = wPage();
    if (page == 0) {
        wButton(power, "Power");
        wSlider(brightness, 0, 100, "Brightness");
        wPulldown(mode, "Off|Auto|Manual");
    }
    if (page == 1) {
        wTime(alarm_time, "Wake-up Time");
        wText(devname, 32, "Device Name");
    }
}

int main() {
    wLabel(0, "Controls");   // erste Schaltflaeche auf der Hauptseite
    wLabel(1, "Settings");   // zweite Schaltflaeche auf der Hauptseite
    return 0;
}
```

Wenn kein `wLabel()` aufgerufen wird, aber `WebUI()` existiert, erscheint eine einzelne "TinyC UI"-Schaltflaeche.

**Funktionsweise:**
1. `WebCall()` rendert Widgets im Sensorbereich der Tasmota-Hauptseite
2. `WebUI()` rendert Widgets auf dedizierten Seiten unter `http://<device>/tc_ui?p=N`
3. `wLabel(N, "text")` registriert Seite N (0–5) mit einer Schaltflaeche auf der Hauptseite
4. `wPage()` gibt die aktuelle Seitennummer zurueck, damit `WebUI()` verschiedene Widgets anzeigen kann
5. Wenn Sie einen Regler bewegen / eine Schaltflaeche klicken, sendet JavaScript den neuen Wert per AJAX
6. Der Server schreibt den Wert direkt in die TinyC-globale Variable
7. Die Seite aktualisiert sich automatisch, um den aktualisierten Zustand anzuzeigen
8. Text- und Zahleneingaben pausieren die automatische Aktualisierung waehrend der Bearbeitung (wird bei Fokusverlust fortgesetzt)

**HTML aus Dateien einbinden:**

Verwenden Sie `webFile("filename")`, um den Inhalt einer Datei vom Geraetedateisystem direkt an die Webseite zu senden. Dies ist nuetzlich fuer grosses HTML, CSS oder JavaScript, das zu gross waere, um in Bytecode-Konstanten kompiliert zu werden.

```c
void WebPage() {
    webFile("chart.html");  // Diagrammbibliothek von /chart.html einbinden
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
        sprintfFloat(buf, "{\"handler\":1,\"id\":\"%s\",\"value\":42}", id);
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
| `void udpSend("name", float_val)` | Float-Variable per binaeren Multicast senden |
| `float udpRecv("name")` | Letzten empfangenen Wert fuer benannte Variable abrufen (0 wenn keiner) |
| `int udpReady("name")` | Gibt 1 zurueck wenn neuer Wert seit letzter Pruefung empfangen |
| `void udpSendArray("name", float_arr, count)` | Float-Array per binaeren Multicast senden |
| `int udpRecvArray("name", float_arr, maxcount)` | Float-Array empfangen, gibt tatsaechliche Anzahl zurueck |

**Protokoll:**
- Einzelner Float: sende `=>name:[4 Bytes IEEE-754 Float]`
- Float-Array: sende `=>name:[2-Byte LE Anzahl][N x 4-Byte Float]`
- Empfang: sowohl ASCII (`=>name=value`) als auch binaer (einzeln oder Array)
- Multicast-Gruppe: `239.255.255.250`, Port `1999`
- Maximal 8 ueberwachte Variablennamen, je 16 Zeichen
- Maximal 64 Floats pro Array

**Callback:** Definieren Sie `void UdpCall()`, um bei jeder empfangenen Variable benachrichtigt zu werden.
Der UDP-Socket wird beim ersten `udpSend()`- oder `udpRecv()`-Aufruf automatisch initialisiert.

**Beispiel (Skalar):**
```c
float temperature = 0.0;

void EverySecond() {
    temperature = 20.0 + sin(counter) * 5.0;
    udpSend("temperature", temperature);
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

**Hinweise:**
- `bus` = 0 oder 1 — waehlt welcher I2C-Bus verwendet wird
- Adresse ist 7-Bit (0x00–0x7F), z.B. `0x48` fuer TMP102
- Register ist 8-Bit (0x00–0xFF)
- Pufferfunktionen verwenden `char[]`-Arrays — jedes Element enthaelt ein Byte (0–255)
- Maximale Pufferlaenge ist 255 Bytes
- Gibt 0 zurueck wenn I2C nicht einkompiliert ist oder die Operation fehlschlaegt

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
        sprintfFloat(out, "TMP102: %.2f °C\n", temp);
        printString(out);
    }
}
```

### Smart Meter (SML)

Zaehlerstaende auslesen und Zaehler ueber Tasmota's SML-Treiber steuern (erfordert `USE_SML` oder `USE_SML_M`).

SML kann **ohne Scripter** laufen — nur `USE_UFILESYS` wird fuer dateibasierte Zaehlerbeschreibungen benoetigt.
Der SML-Deskriptor-Tab der IDE verwaltet die Zaehlerdefinitionsdatei (`/sml_meter.def`) auf dem Geraet.

#### Zaehlerstaende lesen

| Funktion | Beschreibung |
|----------|-------------|
| `float smlGet(int index)` | Zaehlerwert abrufen. Index 0 gibt Anzahl zurueck, 1..N gibt Werte zurueck |
| `int smlGetStr(int index, char buf[])` | Zaehler-ID-Zeichenkette in Puffer abrufen, gibt Laenge zurueck |

**Hinweise:**
- Index ist 1-basiert: `smlGet(1)` gibt den ersten Zaehlerwert zurueck
- `smlGet(0)` gibt die Gesamtzahl der Zaehlervariablen zurueck
- Gibt 0 zurueck wenn SML nicht einkompiliert ist oder der Index ausserhalb des Bereichs liegt
- Die Werte sind dieselben wie Scripter's `sml[x]`-Syntax

**Beispiel:**
```c
void WebCall() {
    char buf[64];
    int n = smlGet(0);  // Gesamtzaehler
    int i = 1;
    while (i <= n) {
        float val = smlGet(i);
        sprintfFloat(buf, "{s}Meter %d{m}%.2f{e}", val);
        webSend(buf);
        i++;
    }
}
```

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
    sprintfFloat(out, "Thermocouple: %.2f °C\n", temp);
    printString(out);
    return 0;
}
```

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
    sprintfInt(buf, "Count: %d", counter);
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

### Audio

| Funktion | Beschreibung |
|---|---|
| `audioVol(int vol)` | Audiolautstaerke setzen (0-100) |
| `audioPlay("file.mp3")` | MP3-Datei vom Dateisystem abspielen |
| `audioSay("hello")` | Text-zu-Sprache-Ausgabe |

Erfordert einen auf dem Geraet konfigurierten I2S-Audiotreiber.

```c
audioVol(50);              // Lautstaerke auf 50% setzen
audioPlay("/alarm.mp3");   // MP3-Datei abspielen
audioSay("sensor alert");  // Text sprechen
```

### Debug

| Funktion      | Beschreibung                    |
|---------------|---------------------------------|
| `dumpVM()`    | VM-Zustand auf Konsole ausgeben |

---

## VM-Grenzen

| Ressource         | ESP8266  | ESP32    | Browser  | Anmerkungen                        |
|--------------------|----------|----------|----------|------------------------------------|
| Stack-Tiefe        | 64       | 256      | 256      | Operandenstack-Eintraege           |
| Aufrufrahmen       | 8        | 32       | 32       | Maximale Rekursions-/Aufruftiefe   |
| Lokale pro Rahmen  | 256      | 256      | 256      | Einschliesslich Arrays (1 Slot pro Element) |
| Globale Variablen  | 64       | 256      | 256      | Einschliesslich globaler Arrays (<=255 Elem.) |
| Codegroesse        | 4 KB     | 16 KB    | 64 KB    | Bytecode (16-Bit-Adressierung)     |
| Heap-Speicher      | 8 KB     | 32 KB    | 64 KB    | Fuer Arrays >255 Elemente + malloc |
| Heap-Handles       | 8        | 16       | 32       | Max. gleichzeitige Heap-Allokationen |
| Konstantenpool     | 32       | 64       | 65536    | Zeichenketten- & Float-Konstanten  |
| Instruktionslimit  | 1M       | 1M       | 1M       | Sicherheitslimit pro Ausfuehrung   |
| GPIO-Pins          | 40       | 40       | 40       | Pins 0–39 (im Browser simuliert)   |
| Datei-Handles      | 4        | 4        | 8        | Gleichzeitig geoeffnete Dateien    |

---

## Geraetedateiverwaltung (IDE)

### IDE-Installation

Die IDE-Datei (`tinyc_ide.html.gz`) muss auf das **Flash-Dateisystem** (`ffsp`) hochgeladen werden, nicht auf die SD-Karte. Auf Geraeten mit SD-Karte mountet Tasmota die SD-Karte als Benutzer-Dateisystem (`ufsp`) — aber der `/ide`-Endpunkt liest speziell vom Flash-Dateisystem. Verwenden Sie die Tasmota-Seite **Dateisystem verwalten**, um `tinyc_ide.html.gz` in den Flash-Speicher hochzuladen.

> **Hinweis:** TinyC-Skripte und Datendateien (`.tc`, `.tcb` usw.) werden auf dem Benutzer-Dateisystem (`ufsp`) gespeichert, das die SD-Karte ist, wenn eine vorhanden ist. Nur die IDE-HTML-Datei selbst muss auf dem Flash sein.

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
| `/tc_api?cmd=writefile&path=/name` | POST | Schreibt POST-Body in Datei, gibt `{"ok":true,"size":N}` zurueck |
| `/tc_api?cmd=deletefile&path=/name` | GET | Loescht eine Datei vom Dateisystem |

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
#define OUTPUT 1

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
    sprintfInt(line, "count = %d", 42);
    printString(line);      // count = 42

    // Mehrere Werte mit sprintfAppend
    char report[128];
    sprintfInt(report, "Sensor %d", 1);
    sprintfAppendStr(report, " name=%s", name);
    sprintfAppendFloat(report, " temp=%.1f", 23.5);
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
    int page = wPage();
    if (page == 0) {
        wButton(power, "Power");
        wSlider(brightness, 0, 100, "Brightness");
    }
    if (page == 1) {
        wPulldown(mode, "Off|Auto|Manual");
    }
}

int main() {
    wLabel(0, "Controls");
    wLabel(1, "Settings");
    brightness = 50;
    return 0;
}
```

---

## Unterschiede zu Standard-C

| Merkmal                  | Standard-C     | TinyC                        |
|--------------------------|----------------|------------------------------|
| Zeiger                   | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Structs / Unions         | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Enums                    | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Dynamischer Speicher     | malloc/free    | Automatischer Heap fuer Arrays >255 (kein explizites malloc) |
| Mehrdimensionale Arrays  | `int a[3][4]`  | **Nicht unterstuetzt**       |
| Zeichenkettentyp         | `char*`        | Nur `char arr[N]`            |
| Praeprozessor            | Volles CPP     | `#define`, `#ifdef`, `#if`, `#else`, `#endif` (kein `#include`, keine Makros) |
| Header-Dateien           | `#include`     | **Nicht unterstuetzt**       |
| Typedef                  | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| sizeof                   | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Ternaerer Operator       | `a ? b : c`   | **Nicht unterstuetzt**       |
| do-while                 | `do {} while`  | **Nicht unterstuetzt**       |
| goto                     | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Funktionszeiger          | Volle Unterstuetzung | **Nicht unterstuetzt**  |
| Variadische Funktionen   | `printf(...)`  | **Nicht unterstuetzt**       |
| Standardbibliothek       | stdio, stdlib  | Nur eingebaute Funktionen    |

---

*Generiert aus TinyC-Quellcode — lexer.js, parser.js, codegen.js, opcodes.js, vm.js*
