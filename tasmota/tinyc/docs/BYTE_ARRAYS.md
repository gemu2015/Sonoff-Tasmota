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
5. **Persist**: Elementgröße in der `.pvs` vermerken — OFFEN
6. **Prüfen**: JS-VM im Node-Lauf, alle Beispiele übersetzen, Firmware bauen,
   dann am Gerät

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

## Was NICHT dazugehört

* `char[]` bleibt int32 (siehe Entscheidung 1)
* keine Zeiger, keine `byte`-Skalare außerhalb von Arrays (ein einzelnes Byte
  bringt nichts — die Rechnung läuft ohnehin in int32)
* Vorzeichen: `byte` ist **ohne** (0..255), wie `uint8_t`
