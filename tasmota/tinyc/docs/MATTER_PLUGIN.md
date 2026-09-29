# Installing Matter as a plugin

*Deutsch: [MATTER_PLUGIN_DE.md](MATTER_PLUGIN_DE.md)*

Since 1.6.70 only the **ESP32-S3** test build has Matter built in. The C3, C6
and P4 test builds leave the engine out, because it costs about 33 KB of RAM on
every device — including all the devices that never use Matter. If you want
Matter on one of them, you load it as a plugin (`MATTERF`). Scripts, pairing
and the `/mt` page are the same as with the built-in engine.

## Which firmware, which file

| Firmware (testing release) | Matter | Plugin file |
|---|---|---|
| `tinyc32s3` | built in — the plugin is optional; when loaded, it is used instead | `MATTERF_32.bin` |
| `tinyc32c3`, `tinyc32c6`, `tinyc32-p4-full` | **plugin only** | `MATTERF_32r.bin` |
| `tinyc32-4M-plain`, `tinyc8266-4M` | not available | — |

Both plugin files are attached to the
[testing release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing).
`_32` is for Xtensa (S3), `_32r` for RISC-V (C3, C6, P4). The wrong one is
rejected when you upload it.

Own builds: `-DTINYC_MATTER -DUSE_MATTER_C_PLUGIN_ONLY` gives the same
plugin-only firmware; `-DTINYC_MATTER` alone keeps the built-in engine.

## Before you start

- ⚠️ **Step 1 formats the file system.** Scripts (`.tc`/`.tcb`), the IDE,
  persist files (`.pvs`), and an existing Matter pairing (`/mtr_fab`) are
  gone afterwards. Download what you want to keep first: **Tools → Manage File
  System**. The Tasmota settings (Wi-Fi, MQTT, templates) are not affected.
- ⚠️ **Firmware 1.6.69 or newer.** Older firmware can leave the partition
  table invalid when a partition is removed (`chkpt r` / `chkpt d`) and the
  device then boot-loops; see the 1.6.69 entry in the CHANGELOG.
- **Memory:** the plugin takes about 71 KB of heap in one block. It is
  allocated at boot, while the heap is still in one piece. On a C3 without
  PSRAM the free heap drops from about 173 KB to 88 KB.
- **Flash:** the plugin partition is 64 KB; it is taken from the file system.

## Step 1 — create the plugin partition

In the console:

```
chkpt a
```

The device adds a 64 KB partition named `custom` behind the file system,
formats the file system and restarts on its own. `chkpt a2` makes 128 KB, if
you want to load other plugins as well; one 64 KB partition holds `MATTERF`
and nothing else.

`chkpt` without an argument lists the partitions; the new one shows as
`label: custom`. If the log says `custom plugin partition already there!`, the
partition exists already — go straight to step 2.

## Step 2 — upload the plugin

1. **Tools → Plugins directory.** The button only appears once the partition
   from step 1 exists.
2. Choose `MATTERF_32.bin` (S3) or `MATTERF_32r.bin` (C3/C6/P4) and press
   **Start**.
3. The table now lists `MATTERF` with its size (57–61 KB).
4. ⚠️ **Tick "Autostart plugins at boot"** at the top of the plugin page.
   Started at boot, the plugin gets its 71 KB while the heap is still in one
   piece. Without autostart Matter still works — the plugin is then started
   when a script starts Matter — but on a device without PSRAM the block may
   be fragmented by then. The tick is saved in `/plugins.auto` (firmware
   1.6.70 or newer); formatting the file system clears it, so set it after
   step 1. On older firmware the tick is lost at the next restart — there,
   set a free GPIO to **Option A** number **7** in **Configuration → Configure
   Module** instead.

## Step 3 — restart and check

Restart the device (**Main Menu → Restart**). The plugin has to be there at
boot, so that it gets its memory block before the heap fragments.

The log after the restart shows:

```
MTR: initializing the Matter plugin (module 1)
MTR: using the Matter plugin (MATTERF)
```

On the plugin page `MATTERF` now shows about 71 KB in the RAM column. From here
on everything works as with the built-in engine: load a Matter script (e.g.
`matter_home_bridge.tc` or one of the other `matter_*.tc` examples), open the
**`/mt`** page, press **Bind** and scan the QR code in Apple Home, Google Home
or Alexa.

Because step 1 formatted the file system: reload the IDE with `TinyCIde` and
upload your scripts again.

## Updating and removing

- **Update:** in the console `deiniz N` and `unlink N` (N = the slot number
  from the plugin page), or the 🔥 icon on the plugin page; then upload the new
  file (step 2) and restart. The pairing lives in the file system and is kept.
- **Remove:** as above, without uploading a new file. After the restart Matter
  is off on C3/C6/P4; the S3 falls back to its built-in engine.
- **Give back the partition:** `chkpt r`. This formats the file system again
  (firmware 1.6.69 or newer only, see above).

## When it does not work

| Symptom | Cause |
|---|---|
| No **Plugins directory** button | No plugin partition — step 1 is missing; or the firmware has no plugin support (`tinyc32-4M-plain`, ESP8266). Log: `Plugins: Partition not found`. |
| Log: `MTR: no Matter plugin (MATTERF) - this firmware has no built-in Matter` | Plugin not uploaded, or no restart after the upload. |
| Matter works after one restart and not after another, or the plugin shows no RAM | Autostart is not ticked (step 2.4): without it the plugin is only started when a script starts Matter, and on a device without PSRAM the 71 KB block may be gone by then. |
| Log: `MTR: using the built-in Matter` on the S3 | No plugin loaded — fine if intended, the S3 has the engine built in. |
| Upload refused | Wrong file for the CPU (`_32` vs `_32r`), or the partition is full (other plugins — use `chkpt a2`). |
| Matter was paired and is gone | Step 1 formats the file system and with it `/mtr_fab`. Remove the device in the controller app and pair again. |
