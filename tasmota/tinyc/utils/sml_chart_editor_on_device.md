# Editing SML chart data on the device (quick guide)

Run [`sml_chart_editor.html`](sml_chart_editor.html) **directly on the Tasmota
device** and edit `/sml_chart.bin` in place — no manual download/upload cycle.

## Requirements

- TinyC firmware **1.6.21+** (adds the `/cedit` route that serves the editor
  inline as `text/html`; without it `/ufsd?download=` would only *download* the
  HTML, not render it).
- The `sml_chart` script (or any chart script that persists
  `/sml_chart.bin` with the four arrays `sml_s4h[481] · sml_s24h[1441] ·
  sml_dcon[31] · sml_mcon[12]`).

## Install (one time)

1. Download the pre-built [`sml_chart_editor.html.gz`](sml_chart_editor.html.gz)
   (~7 KB) — or use the raw [`sml_chart_editor.html`](sml_chart_editor.html) if
   you don't mind the larger file. Either works; the route checks `.gz` first
   and falls back to `.html`.
2. Open the device's file manager at `http://<device>/ufsd`, click **Upload**,
   and upload the file.

## Use

1. Open **`http://<device>/cedit`** in any browser. The editor renders directly
   from the device.
2. In the **On-device file** card at the top:
   - **Load from device** → reads `/sml_chart.bin` and splits it into the four
     arrays (each with a chart + editable values box + min/max/avg stats).
   - Edit values (whitespace-separated). Keep each array's value count
     unchanged. *Set all 0* and *Wrap (one/line)* help bulk edits.
   - **Save to device + restart slot** → uploads the new bin and runs
     `Backlog TinyCStop 0; TinyCRun /sml_chart.tcb` so the script reloads the
     edited arrays. Clear the *Reload script* field to skip the restart.

## Under the hood

| Action | Endpoint |
|---|---|
| Open editor | `GET /cedit` → serves `/sml_chart_editor.html(.gz)` inline |
| Load | `GET /ufsd?download=/sml_chart.bin` |
| Save | `POST /ufsu?fsz=<bytes>` (multipart, field `ufsu`, filename → device path) |
| Reload | `GET /cm?cmnd=Backlog TinyCStop 0; TinyCRun /sml_chart.tcb` |

All requests are **same-origin** (the page is served by the device), so no CORS
and no IP entry needed.

Verified end-to-end on an ESP32-S3 devkit (TinyC 1.6.21): Load → edit (one
value) → Save → re-download is byte-exact.

## Troubleshooting

- **`/cedit` returns 404** → firmware is older than 1.6.21, or the editor file
  isn't on the FS. Re-flash and re-upload `sml_chart_editor.html.gz`.
- **Buttons disabled, status says "offline (file://)"** → you opened the local
  HTML file directly instead of `http://<device>/cedit`. Open the device URL.
- **Save succeeds but the script doesn't show the new data** → the reload step
  needs `/sml_chart.tcb` to exist. Adjust the *Reload script* field to your
  actual `.tcb` path, or reboot the device.
