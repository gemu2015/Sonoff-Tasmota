# Echte Byte-Arrays in TinyC (`byte[]`)

Stand: Entwurf + Umsetzung ab 2026-08-21. Auftrag gemu: „wir machen es gleich
richtig" — ein echter gepackter Typ, nicht Hilfsfunktionen auf int32-Arrays.

## Das Problem, gemessen

Ein Array-Element ist heute **überall** ein `int32_t`:

```c
typedef struct {
  uint16_t offset;   // start offset in heap_data[]
  uint16_t size;     // number of int32 slots      ← auch bei char[]
  bool     alive;
} TcHeapHandle;
```

`char buf[160]` kostet 640 Byte; als LOKALE Variable frisst es 160 der 256
Slots eines Rahmens (`TC_MAX_LOCALS`). Über unsere 190 Beispielskripte
zusammen: 132 864 char-Elemente = **531 kB statt 133 kB**.

| Skript | char-Elemente | heute | gepackt |
|---|---|---|---|
| `hyundai_soc.tc` | 9 742 | 38,9 kB | 9,7 kB |
| `growatt_shine.tc` | 8 273 | 33,1 kB | 8,3 kB |
| `webradio.tc` | 5 876 | 23,5 kB | 5,9 kB |

## Entscheidungen

**1. Neuer Typ `byte`, `char[]` bleibt wie es ist.**
`char[]` zu packen wäre der größere Gewinn, ändert aber das Verhalten von 190
vorhandenen Skripten, jeden String-Syscall und die `.pvs`-Dateien auf allen
Geräten draußen. Der neue Typ ist zusätzlich; wer ihn nimmt, bekommt den
Faktor 4. Umstellen können wir später Skript für Skript.

**2. Gepackt in dieselben Ablagen, nicht in eine neue.**
Ein `byte[n]` belegt `(n+3)/4` int32-Slots in Globals, Locals oder Heap —
dieselbe Verwaltung, dieselbe Freigabe, dieselbe Persist-Mechanik. Element `i`
ist Byte `i` der Region (`((uint8_t*)basis)[i]`). Beide Seiten (ESP32 und der
JS-VM im Browser) sind little-endian, die Zuordnung ist also überall dieselbe
und `.pvs`-Dateien bleiben zwischen ihnen austauschbar.

**3. Eigene Opcodes statt Typprüfung zur Laufzeit.**
Der Compiler weiß den Typ an der Zugriffsstelle, also emittiert er dort direkt
den Byte-Zugriff. Acht neue Opcodes in der freien Lücke **0xA8–0xAF**, genau
neben den bestehenden Array-Opcodes:

```
OP_LOAD_LOCAL_BYTE  0xA8   OP_STORE_LOCAL_BYTE  0xA9
OP_LOAD_GLOBAL_BYTE 0xAA   OP_STORE_GLOBAL_BYTE 0xAB
OP_LOAD_HEAP_BYTE   0xAC   OP_STORE_HEAP_BYTE   0xAD
OP_LOAD_REF_BYTE    0xAE   OP_STORE_REF_BYTE    0xAF
```

Damit kostet ein Byte-Zugriff genauso viel wie ein int32-Zugriff — kein
Zweig, keine Verzweigung im heißen Pfad.

**4. Ein Kennbit in der Referenz, je Tag an anderer Stelle.**
Syscalls bekommen Arrays als gepackte 32-Bit-Referenz. Die Kodierung ist voll
belegt, ein Bit ist aber in jeder der drei Formen frei:

| Form | Aufbau heute | belegt | Kennbit für `byte[]` |
|---|---|---|---|
| Heap  | `0xC0000000 \| (offset<<16) \| handle` | Bits 16–29, 0–7 (Bit 15 = Konstante) | **Bit 8** |
| Global | `0x80000000 \| idx` | Bits 0–15 | **Bit 16** |
| Local | `(fp<<16) \| idx` | Bits 16–23, 0–7 | **Bit 24** |

Gekapselt in `tc_ref_is_bytes()` / `tc_ref_mark_bytes()`, damit die Stelle
einmal existiert und nicht 156-mal.

**5. Ein Helfer für alle byte-orientierten Syscalls.**
Sie wandeln heute jedes Element von Hand um:

```c
for (int i = 0; i < hex_len; i++) src[i] = (char)(hex_arr[i] & 0xFF);
```

Das wird zu einem Aufruf, der beide Arten annimmt: bei `byte[]` zeigt er
direkt in den Speicher, bei int32-Arrays kopiert er wie bisher.

```c
static uint8_t *tc_ref_bytes(TcVM *vm, int32_t ref, int32_t want,
                             uint8_t *tmp, int32_t tmpcap, int32_t *out_len);
```

## Reihenfolge der Umsetzung

1. ✅ **VM (C++)**: Opcodes, Kennbit-Helfer, `elem` im Handle, `tc_ref_bytes`
2. ✅ **VM (JS)**: dieselben Opcodes — der Browser muss dasselbe rechnen
3. ✅ **Compiler**: `byte` als Typ, Deklaration (global/lokal/heap), Indexrechnung,
   Argumentübergabe, `sizeof`, `_Q()`-Deskriptor
4. ✅ **Syscalls** (1.6.56, 2026-08-24): die byte-orientierten nehmen `byte[]` an
   (siehe Liste unten). Am C3 .172 geprüft.
5. ✅ **Persist** (2026-08-24): `persist byte[]` sichert die richtige Slot-Zahl.
   Der Fehler saß im Compiler, nicht im `.pvs`-Format: `persistGlobals` trug die
   ELEMENT-Zahl als `slotCount`, nicht `(n+3)/4` Slots — die Firmware hätte 4×
   zu viel gelesen (über die Region hinaus). Fix in `codegen.js` (`slotsFor`).
   Das `.pvs` speichert die rohen Slots LE; die gepackten Bytes bleiben so
   erhalten, KEIN Firmware- und KEIN Formatwechsel nötig. Elementgröße muss NICHT
   vermerkt werden — nur ein Typwechsel `char[]`↔`byte[]` unter demselben Namen
   ist nicht sauber migrierbar (rohe Slots bedeuten dann etwas anderes; der
   Layout-Hash wirft die alte Datei ohnehin auf `.bak`). Gerätetest
   `persist_byte_test.tc` am **S3 .39** bestätigt: nach Slot-Neustart kommen alle
   64 Bytes aus dem `.pvs` zurück (`pbuf[10]=30 pbuf[63]=189 alle_ok=1`).
6. ✅ **Geprüft** (2026-08-24): `examples/byte_array_suite.tc` — 35 Selbsttests
   (lokal/heap/global byte[], Packungsgrenzen, Ref-Arg, %s-Lesen, Krypto/Hex/
   Datei-Syscalls, `byte`-Alias), am S3 .39 **35/35**. ⭐ Die Suite fand dabei
   einen echten Bug: globale `byte[]`-SCHREIBzugriffe liefen mit int32-Stride
   (streuten in Nachbar-Globals), weil der Assignment-Pfad `STORE_GLOBAL_ARR`
   hardcodete statt `emitArrStore` — nur Index 0 passte zufällig, lokal/heap
   waren korrekt. Compilerseitig gefixt (commit 22349772c, bundle neu).
   ⚠️ GRENZE: String-SCHREIB-Syscalls (strcpy/strcat/sprintf) sind char[]-only.
   Frühere Einzeltests: byte[]-Syscalls (sha256/hex2bin/bin2hex/aesEcb) Syscalls zuerst am C3 .172,
   danach — nachdem dessen serielle Schnittstelle ausfiel — alles am S3 .39
   (Env `tasmota32s3-devkit`, 1.6.57). ⚠️ Upload auf .39 nur über den
   Dateimanager `/ufsu` (der `/tc_upload`-Endpoint hängt dort, auch nach
   Frisch-Flash — eigener offener Punkt, nichts mit byte[] zu tun).

## Syscalls, die `byte[]` annehmen (Stand 1.6.56)

Ein neuer Helfer `tc_ref_put_bytes()` ist die Gegenrichtung zu `tc_ref_bytes()`
(Ergebnis IN das Array schreiben — Prüfsumme, Klartext, empfangene Bytes). Bei
`byte[]` schreibt/liest er direkt in die gepackte Region, bei `char[]`/int32
Byte je Slot wie bisher. Umgestellt:

* **Krypto**: `aesEcb`, `aesCbc` (Schlüssel/IV/Daten), `hmacSha256`, `sha256`,
  `md5` (Daten + Ausgabepuffer)
* **Hex/Base64**: `hex2bin`, `bin2hex`, `base64Enc` (Ein- und Ausgabe)
* **Datei**: `fileReadBin`/`fileWriteBin` — bei `byte[]` ROHE Bytes (1 Byte je
  Element), NICHT die 4-Byte-LE-int32-Elemente; `count` ist dann eine Byte-Zahl
* **Netz**: `tcpReadArray`/`tcpWriteArray`, `udp(1,…)`-Empfang und
  `udp(7,…)`-Rohbytes senden
* **Seriell**: `serialWriteBytes`
* **I²C**: `i2cReadBuf`, `i2cReadRs`, `i2cWriteBuf`
* **CAN**: `twaiSend`/`twaiRecv` (nur der Datenpuffer; die Meta-Arrays mit
  ID/DLC bleiben int32)
* **Textpfade**: `tc_ref_to_cstr`/`tc_stream_ref`/`tc_cstr_to_ref` sind
  byte-bewusst — ein `byte[]` mit Text funktioniert als Ersatz für `char[]` in
  ALLEN String-Syscalls (`webSend`, `mqttPublish`, `httpPost`, `serialWrite`, …)

NICHT umgestellt, weil Wert- statt Byte-Arrays: `udp(…)`-Float-Globalvars
(`udpSend`/`udpRecv`), `spiTransfer` (8/16/24-Bit-Wörter), die Meta-/Statistik-
Arrays von TWAI. Ein `byte[]` ergibt dort keinen Sinn.

## Fertig — vollständig (Stand 1.6.58, 2026-08-24)

Alle ursprünglich offenen Punkte sind erledigt und am S3 .39 verifiziert
(`byte_array_suite.tc`, **62/62**):

* **Syscalls** nehmen `byte[]` (crypto/hex/base64/file/net/serial/i2c/twai) — 1.6.57
* **persist byte[]** — 1.6.57
* **String-Syscalls byte-bewusst** (strcpy/strcat/sprintf/strToken/strSub/
  strReplace/strToUpper/strToLower/strTrim schreiben gepackt; strlen/strcmp/
  strFind/strStartsWith/strEndsWith/strContains lesen gepackt) — 1.6.58. Eine
  `byte[]` ist damit ein voller Ersatz für `char[]` als Textpuffer.
* **`uint8_t`** ist ein Alias für `byte` (gepackt) — 1.6.58
* **`byte`/`uint8_t` in structs** packen (Feld-Slots, Byte-Offset, byte-Opcodes;
  Whole-Copy/sizeof/persist folgen automatisch) — 1.6.58

## Was NICHT dazugehört

* `char[]` bleibt int32 (siehe Entscheidung 1)
* keine Zeiger, keine `byte`-Skalare außerhalb von Arrays (ein einzelnes Byte
  bringt nichts — die Rechnung läuft ohnehin in int32)
* Vorzeichen: `byte` ist **ohne** (0..255), wie `uint8_t`


## Nachtrag 2026-08-26 — drei Fallen, die erst die int16-Runde zeigte

**1. Ein `char[]`-PARAMETER liest ein `byte[]`-Argument falsch.** Gemessen:
Syscalls auf dem Parameter (`strlen`, `strcpy`, `sprintf`) arbeiten korrekt —
die Packung reist in der Referenz mit. Ein direkter `dst[i]` NICHT: der Opcode
kommt aus dem deklarierten Typ des PARAMETERS, läuft also mit int32-Schrittweite.
Lesen trifft jedes vierte Byte, Schreiben überschreibt drei Nachbarn.

In `examples/common/ct002_common.tc` steckten dadurch **zwei echte Fehler**:
`ctTok(char dst[], int n)` und `ctUrlMail(char dst[], char src[])` indizieren
beide ihre Parameter, und jeder Aufrufer übergibt einen `byte[]`-Puffer —
Überbleibsel der ersten byte[]-Runde, bei der die Puffer umgestellt wurden, die
Helfer aber nicht. Der Compiler **warnt jetzt** bei dieser Paarung
(`_packungPruefen`), und beide Funktionen sind auf `byte[]` gezogen.

**2. Zweidimensionale gepackte Arrays waren kaputt.** `byte m[6][16]`: die
Zeilenreferenz `m[i]` rechnete `i * cols` und gab das an `ADDR_HEAP_OFF` — dessen
Versatz aber in int32-SLOTS zählt. Zeile 5 landete auf Versatz 80 in einem
24-Slot-Array. Jetzt `i * cols / (4/Elementbreite)`, plus das Kennfeld auf der
Zeilenreferenz; passt die Zeilenlänge nicht auf eine Slotgrenze, gibt es einen
klaren Übersetzungsfehler statt einer stillen Fehladresse.

**3. Der IDE-Simulator kannte `STRCMP_CONST` (275) nicht** — deshalb ließ sich
`examples/byte_array_suite.tc` dort nie ausführen, es starb am ersten `strcmp`
gegen ein Literal. Nachgetragen. (Die Krypto-Syscalls fehlen weiterhin, die
Suite bleibt also ein Gerätetest.)

**4. `webText()` schrieb noch nie dorthin zurück, wo der Puffer liegt.** Beim
Umstellen von `lcd_i2c.tc` gesucht, ob die Packung den Web-Rückweg übersteht —
und dabei einen älteren, schwereren Fehler gefunden, der mit `byte[]` gar nichts
zu tun hat.

Der Ablauf: `webText(puffer, 20, "Line 1")` rendert ein Eingabefeld mit
`onchange='siva(value,<id>)'`; die `id` kommt aus `tc_widget_id(gref)` und ist
`gref & 0x0FFF`. Der Rückweg in `TinyC_WebSetVar()` schrieb den getippten Text
dann Zeichen für Zeichen nach `globals[id]`.

Das kann für ein Array nicht stimmen. Arrays über `HEAP_THRESHOLD` (16 Elemente)
liegen im **Heap**, nicht in den Globals — die unteren 12 Bit sind dann ein
Heap-HANDLE. Handle 0 hieß `globals[0]`. Jeder `webText` auf einem Puffer > 16
hat also den eingegebenen Text verworfen und stattdessen die ersten Skalare des
Skripts überschrieben: `devname[32]` in `webui_demo.tc`, `webcall_demo.tc` und
`multipage_demo.tc`, die beiden Textzeilen in `lcd_i2c.tc`. Nur `ip_in[16]` in
`matter_bridge_ui.tc` — genau auf der Schwelle, also inline — hat je funktioniert.

Unsichtbar war es, weil **nur der Rückweg** betroffen ist: das Anzeigen läuft
über `tc_ref_to_cstr()`, das Heap-Referenzen sauber auflöst. Das Feld zeigte
immer den richtigen Text und behielt nur den neuen nicht.

Die Reparatur macht die Packung gleich mit: `webText` schickt seine **ganze
Referenz** an das JavaScript (`siva(value,<id>,<ref>)` → `sv=<id>_S_<ref>_<text>`),
und der Rückweg ruft `tc_cstr_to_ref()` — den Schreiber des VM selbst, der Heap
und Globals kennt **und** die Elementbreite. Damit ist die Frage, mit der die
Suche anfing, nebenbei beantwortet. Die Referenz kommt aus einer URL, wird also
wie der Index vorher geprüft: nur Tag 2 (global) und Tag 3 ohne Const-Pool-Bit
(Heap) werden angenommen, `tc_ref_maxlen()` begrenzt den Rest. Der alte
`s_`-Zweig bleibt stehen, damit eine noch offene Browserseite keinen Unsinn
schreibt.
