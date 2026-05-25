# SML Chart Data Editor

A single self-contained HTML page to **view and edit the raw chart data** that the
TinyC [`sml_chart`](sml_chart.tc) example stores on the device — no install, works
offline, no server. Open [`sml_chart_editor.html`](sml_chart_editor.html) in any
browser.

## What it edits

`sml_chart` persists its four chart arrays to **`/sml_chart.bin`** as raw
**4-byte little-endian `float32`, concatenated, no header** (exactly what
`fileWriteBin()` writes). In file order:

| Array | Count | Meaning |
|-------|-------|---------|
| `sml_s4h`  | 481  | 4 h chart |
| `sml_s24h` | 1441 | 24 h chart |
| `sml_dcon` | 31   | daily consumption (per day of month) |
| `sml_mcon` | 12   | monthly consumption |

→ 1965 values = **7860 bytes**. The counts are editable in the page, so it still
works if a script declares different array sizes (the file is split in this order;
`.bin` size must equal *total × 4* bytes).

It also reads/writes the human-readable **`#define CHART_CSV`** format
(`/sml_chart.csv`) — one array per line, TAB-separated (what `fileWriteArray()`
writes).

## How to use

1. On the device, open the **file manager** (Consoles → Manage File system, or
   `http://<device>/ufsd`) and **download `/sml_chart.bin`** (or `.csv`).
2. Open `sml_chart_editor.html` and drop the file in.
3. Each array shows as a **chart + an editable values box** (whitespace- or
   newline-separated) with min/max/avg/non-zero stats. Edit values, or use
   *Set all 0* / *Wrap*. Keep each array's value count unchanged.
4. Click **Download .bin** (or **.csv**) and **re-upload** it to the device's file
   manager, overwriting the original. Restart the script (or reboot) to reload.

## Notes

- Values are shown rounded to 3 decimals — fine for W/kWh chart data, but a
  re-save re-quantizes to ~0.001.
- Editing is fully client-side; nothing is uploaded anywhere by the page.
