# Gepackte 16-Bit-Arrays in TinyC (`int16[]` / `uint16[]`)

Stand: Umsetzung 2026-08-26. Auftrag gemu: „bau die 16 bit vars und arrays",
nachdem die Aufwandsschätzung stand. Die Fortsetzung von
[`BYTE_ARRAYS.md`](./BYTE_ARRAYS.md) — dieselbe Mechanik, eine Stufe feiner.

## Warum, wenn es `byte[]` schon gibt

`byte[]` spart das Vierfache, kostet aber Auflösung. Genau deshalb gibt es
überhaupt `WebChartQ(scale, offset)`: ein Byte trägt eine Temperatur nur in
0,5-K-Schritten über −40..87,5 °C, eine Feuchte in 0,4 %. Wer das nicht will,
landet bei `float[]` — und drei Ringe zu 1441 Werten sind 16,9 KB, ein Viertel
des 64-KB-Heaps eines C3.

`int16` liegt dazwischen und braucht **keinen Umrechnungstrick**:

| Typ | 1441 Werte | drei Ringe im 64-KB-Heap | Auflösung |
|---|---|---|---|
| `float[]` / `int[]` | 5,6 KB | 26,4 % | voll |
| `int16[]` / `uint16[]` | 2,8 KB | 13,2 % | voll, für alles bis ±32767 |
| `byte[]` | 1,4 KB | 6,6 % | 8 bit, braucht Skala/Versatz |

Ein roher ADC-Wert (12 bit), eine Temperatur in Hundertstel Kelvin, eine
Leistung in Watt, ein Modbus-Halteregister — alles passt unverändert hinein.

## Sprache

```c
int16  temp[1441];      // −32768 .. 32767
uint16 roh[512];        // 0 .. 65535
short  s[10];           // Aliasname für int16
ushort u[10];           // Aliasname für uint16
// ebenso: int16_t, uint16_t
```

Alles, was ein `int[]` kann, kann auch ein `int16[]`: lokal, global, im Heap
(> 16 Elemente), als Strukturfeld, als Funktionsparameter, `persist`.

```c
struct Messung {
    int    zeit;
    int16  werte[6];    // 3 Slots statt 6
    byte   marke[4];    // 1 Slot
};

int summe(int16 a[], int n) { ... }   // Referenz trägt die Elementbreite mit
persist int16 ring[1441];             // .pvs speichert die rohen Slots
```

⚠️ **Ein SKALAR ist kein 16-Bit-Wert.** `int16 x;` belegt einen vollen Slot und
rechnet in 32 bit — genau wie ein skalares `char` in TinyC seit jeher. Die
Packung ist eine Eigenschaft des ARRAYS. Wer die Abschneidung eines Skalars
braucht, schreibt sie hin: `x = wert & 0xFFFF;`. `sizeof(int16)` ist deshalb 1
(Slots), nicht 2.

## Wie es gebaut ist

**1. Zwölf Opcodes, 0xB6–0xC1.** Wie beim `byte[]` entscheidet der Compiler an
der Zugriffsstelle, es gibt also keinen Zweig im heißen Pfad.

```
LOAD_LOCAL_I16  0xB6   STORE_LOCAL_I16  0xB7
LOAD_GLOBAL_I16 0xB8   STORE_GLOBAL_I16 0xB9
LOAD_HEAP_I16   0xBA   STORE_HEAP_I16   0xBB
LOAD_REF_I16    0xBC   STORE_REF_I16    0xBD
LOAD_LOCAL_U16  0xBE   LOAD_GLOBAL_U16  0xBF
LOAD_HEAP_U16   0xC0   LOAD_REF_U16     0xC1
```

⭐ **Das Vorzeichen steckt im LADEN, nicht in der Ablage.** Ein Speichern
schreibt für `int16` und `uint16` dieselben sechzehn Bit, also gibt es nur vier
zusätzliche Opcodes für `uint16` statt acht.

**2. Das Kennbit der Referenz wurde zum Zwei-Bit-Feld.** Frei waren die
Nachbarbits: Heap 8–9, Global 16–17, Lokal 24–25.

| Code | Bedeutung |
|---|---|
| `00` | int32 — was jede Referenz vor `byte[]` bedeutete |
| `01` | gepacktes `byte[]` — **bitgleich dem alten Kennbit** |
| `10` | gepacktes `int16[]` |
| `11` | gepacktes `uint16[]` |

⭐ Weil `01` unverändert das alte Byte-Bit ist, bedeutet **jedes vorhandene
`.tcb` weiter genau dasselbe**. Der ABI-Sprung hängt allein an den neuen
Opcodes.

⚠️ **Warum `uint16` einen eigenen Code bekommt**, obwohl das Vorzeichen sonst
im Opcode steckt: eine Referenz überlebt die Zugriffsstelle. Ein Syscall, der
das Array liest (`WebChart`), hat nur die Referenz — mit einem gemeinsamen Code
für beide Breiten hätte er einen `uint16`-Messwert von 60000 als −5536
gezeichnet.

**3. ABI 27, gestempelt in `emit()`.** Der Haken sitzt an der einen Stelle, an
der jeder Weg vorbeikommt: wer einen Opcode aus 0xB6–0xC1 ausgibt, verlangt
ABI 27. Ältere Firmware weist das `.tcb` beim Laden ab, statt mitten in der
Schleife an `BAD_OPCODE` zu sterben.

## Syscalls

- **`WebChart`** liest `int16[]` und `uint16[]` direkt. `WebChartQ` bleibt
  benutzbar (etwa `WebChartQ(0.01, 0)` für Hundertstel), ist aber nicht mehr
  nötig, um das Diagramm überhaupt lesbar zu machen.
- **`sortArray`** sortiert in der Elementbreite des Arrays. ⚠️ Das war für
  `byte[]` bisher **stillschweigend falsch** — ohne Kennfeld sortierte es
  int32-Slots, also je vier Byte-Werte am Stück; im IDE-Simulator sortierte es
  sogar ÜBERHAUPT NICHT. Mit ABI 27 stimmt es für alle drei Breiten.
- Die **Textfamilie** (`sprintf`, `strcat`, `httpGet`, `tasmCmd`, `smlGetStr`,
  `fileRead/Write`) ist bewusst NICHT 16-bit-bewusst: 16 bit ist ein Zahlentyp.
  Wer Bytes will, nimmt `byte[]`.

## Geprüft

**Am Gerät belegt** — ESP32-C3 (.172, RISC-V), Firmware 1.6.62 / ABI 27,
2026-08-26:

- `examples/int16_array_suite.tc` — **139/139 PASSED**, im Simulator und am
  Gerät gleichermaßen, 80 947 Instruktionen, sauber gehalten. Deckt ab:
  lokal/global/heap, beide Vorzeichen, die Grenzen samt Umlauf, Nachbarschaft
  im Slot und zum Folge-Global, Referenzparameter lesend und schreibend,
  Strukturfelder gemischt mit `byte[]`, ein Feld von Strukturen, `sortArray`,
  `persist`.
- ⭐ **Rückwärtskompatibilität am Gerät**: die `byte_array_suite.tcb`, die seit
  dem 24.08. auf dem Gerät liegt, ist **bitgleich** mit dem, was der neue
  Compiler erzeugt, und lief auf der ABI-27-Firmware unverändert **62/62
  PASSED**. Vorhandenes Bytecode bedeutet also weiter genau dasselbe.
- ⭐ **`WebChart` über alle drei Breiten am Gerät**: int16 liefert
  `-32768, -1000, -1, 0, 1, 1000, 32767, -12345` (Vorzeichenerweiterung an
  beiden Enden), uint16 liefert **60000 als 60000** — nicht als −5536. Genau
  der Fall, für den `uint16` den eigenen Kind-Code `11` bekommen hat.
- **Rückschritts-Probe: alle 233 Beispiele übersetzen byte-identisch** zu
  vorher (`scripts/compile_examples.mjs`, `changed 0`). Der Umbau der
  Struktur-Pfade von einem Boolean auf einen Elementtyp ändert also an
  vorhandenem Code nichts.

## ⚠️ Was NICHT geändert werden darf

`tc_ref_maxlen()` antwortet weiter nach dem alten Vertrag: „byte[] → Bytes,
sonst Slots". Der Versuch, es auf Elemente umzustellen, wurde
zurückgenommen — **121 Aufrufstellen** lesen es so und schreiben danach `cap`
int32-Slots. Eine dritte Einheit hätte jede davon zweimal über das Ende eines
`int16[]` schreiben lassen: genau die Fehlerklasse, für die es ABI 26 gibt, mal
121. Ein int16-Ref liest dort seine SLOT-Zahl (immer zu klein, also sicher);
die zwei Stellen, die wirklich Elemente wollen — `WebChart` und `sortArray` —
fragen `tc_ref_elem_count()`.

## Nebenbei gefunden und behoben

- **Der Disassembler kannte die `byte[]`-Opcodes nicht.** 0xA8–0xAF fielen in
  den `default`-Zweig, also lief die Auflistung nach jedem Byte-Zugriff aus dem
  Takt und deutete das Operandenbyte als nächsten Opcode. Jetzt sind beide
  Blöcke eingetragen.
- **Die Schranke eines `*_REF_BYTE`-Zugriffs** kam aus der Elementbreite der
  ÜBERGEBENEN Referenz statt aus der Größe der Ablage. Im richtigen Fall
  bitgleich; bei einem falsch typisierten Argument hätte sie zu weit gereicht.
  Jetzt physikalisch aus `tc_ref_maxlen_slots`.
- **`sizeof(byte)`** warf „unknown type" — jetzt 1, wie `char`.
