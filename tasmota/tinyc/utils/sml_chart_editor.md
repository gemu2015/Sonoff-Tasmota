# SML Chart Data Editor

A single self-contained HTML page to **view and edit the raw chart data** that the
TinyC [`sml_chart`](../examples/sml_chart.tc) example stores on the device — no install, works
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

### PV variant (`sml_chart_pv.tc`)

The PV port of ottelo's `2_SML_Chart_PV.tas` adds two more arrays after the four
above (production-side daily/monthly), so `/sml_chart_pv.bin` is **2008 floats /
8032 bytes**:

| Array       | Count | Meaning |
|-------------|-------|---------|
| `sml_s4h`   | 481   | 4 h chart |
| `sml_s24h`  | 1441  | 24 h chart |
| `sml_dcon`  | 31    | daily consumption |
| `sml_mcon`  | 12    | monthly consumption |
| `sml_dprod` | 31    | daily production |
| `sml_mprod` | 12    | monthly production |

Use the **Preset** dropdown at the top of the editor to switch — it reconfigures
the file path, the reload-script path, and the schema in one click. If you load
a file whose size matches a preset other than the one currently selected, the
warning message points you to the right preset.

It also reads/writes the human-readable **`#define CHART_CSV`** format
(`/sml_chart.csv`) — one array per line, TAB-separated (what `fileWriteArray()`
writes).

## How to use

### Recommended: host the editor on the device (firmware 1.6.21+)

1. On the device, open the **file manager** at `http://<device>/ufsd` and **upload
   `sml_chart_editor.html`** (or `.html.gz` to save space) to the filesystem.
2. Open **`http://<device>/cedit`** in a browser — the editor renders directly
   from the device (a dedicated `/cedit` route serves it as `text/html` inline;
   `/ufsd?download=…` would just download the HTML file, not render it).
3. In the **"On-device file"** card at the top:
   - Click **Load from device** → reads `/sml_chart.bin` over `/ufsd?download=`.
   - Edit each array as a chart + values box (min/max/avg/non-zero, *Set all 0*,
     *Wrap*). Keep each array's value count unchanged.
   - Click **Save to device + restart slot** → POSTs the rebuilt `.bin` to
     `/ufsu` and runs `Backlog TinyCStop 0; TinyCRun /sml_chart.tcb` so the
     script reloads the edited arrays. No reboot needed.

### Offline fallback (any firmware)

1. Download `/sml_chart.bin` from the device's file manager (`/ufsd`).
2. Open `sml_chart_editor.html` locally in any browser and drop the file in.
3. Edit, then click **Download .bin**.
4. Re-upload the edited file to the device via the file manager and restart the
   script (or reboot).

## Notes

- Values are shown rounded to 3 decimals — fine for W/kWh chart data, but a
  re-save re-quantizes to ~0.001.
- Editing is fully client-side; nothing is uploaded anywhere by the page.
