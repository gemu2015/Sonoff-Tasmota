#!/usr/bin/env node
// gen_examples_doc.mjs — writes tasmota/tinyc/docs/EXAMPLES.md: every example with a one-line explanation.
//
//   node tasmota/tinyc/scripts/gen_examples_doc.mjs           write the file
//   node tasmota/tinyc/scripts/gen_examples_doc.mjs --check   fail (exit 1) when the file is out of date (used by CI)
//
// Source of truth are the examples themselves: the title is the `// @name:` line, the explanation is taken from the
// comment lines right below it. An example that explains itself badly can be corrected WITHOUT touching the .tc by a
// line "name | text" in scripts/examples_descriptions.txt. The group of an example comes from the table GROUPS below;
// a new example that fits no group lands in "Other" and the script says so, so someone adds a rule.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const exDir = path.join(root, 'examples');
const outFile = path.join(root, 'docs', 'EXAMPLES.md');
const overrideFile = path.join(here, 'examples_descriptions.txt');
const check = process.argv.includes('--check');

// [group title, one-line intro, regex over the file name without .tc]; the first matching rule wins
const GROUPS = [
  ['Getting started and language features', 'Small programs that show one language or runtime feature at a time.',
    /^(blink|fibonacci|file_io|cfg_test|strings|sort|sizeof_demo|editor|benchmark|callbacks|spawn_tasks|c_extensions_demo|structs_demo|share_.*|slot\d_test|byte_.*|int16_.*|persist_.*|watch_demo|worker_arrays_test|crypto_test|rsa_test|tls_test|blib_crc_demo|gpio_sweep|gpio_toggle_test|fast_mux|uart_tx_test|udp)$/],
  ['Web pages and buttons', 'Custom web UI, REST endpoints, widgets.',
    /^(web_.*|webcall_demo|webui_.*|multipage_demo|clock_header|filemanager)$/],
  ['Sensors and measuring', 'I2C, 1-Wire, SPI and analog sensors.',
    /^(wave_rover|wave_rover_sensors|wave_sensors_test|bme280|bmp280|bmx280|sht31|sht31_publisher|sht31_th|scd30|sgp30|sps30|vl53l0x|mlx90614|tcs34725|veml6075|ltr308|ads1115|ccs811|max30102|max31855|ld2410|onewire|sensor_read|heating_ds2484|sensor_log87|wd_logger|weather)$/],
  ['Displays, charts and touch', 'LCD, e-paper, LVGL, TinyUI, charts, gauges, clocks.',
    /^(display_demo|sunton_display|guiton_display|epaper.*|epd_.*|m5_epd47|text_on_image|analog_clock|clock_7seg|lcd_chart|lcd_i2c|live_chart|chart|chart_types|gchart_demo|small_chart|lvgl_.*|tinyui_.*|touch_.*|voltmeter.*|jpg_.*|watch_renaissance|solar_dashboard|ledbar|rgb_.*)$/],
  ['Camera', 'Camera capture, streaming, snapshots.', /^(camera|cam_view.*|webcam.*|snap_with_timestamp)$/],
  ['Audio', 'I2S, USB audio stick, microphone, players, text to speech.',
    /^(audio_io|loudness.*|mic_plugin_level|wav_.*|dysv17f|music_.*|webradio|hourly_chime)$/],
  ['Bluetooth', 'BLE scan, GATT, serial over BLE and classic Bluetooth, BLE devices.',
    /^(ble_.*|spp_.*|esf37_.*|vesync_scale_sniff|variograf_ekg|sma_sunnyboy)$/],
  ['Matter and HomeKit', 'Device types for Home apps.', /^(matter_.*|homekit_.*)$/],
  ['Smart meters, solar and energy', 'SML meters, inverters, batteries, wall boxes, heat pumps, energy logging.',
    /^(sml_.*|deye|growatt.*|powerwall.*|power_meter|energy_.*|core2_energy|ecotracker.*|sma_speedwire|marstek_venus|wallbox_charge|pool_pump|tuya_pool_heater|house_monitor|steuve|meter_pin_unlock|ebus_clock_write|heatpump_.*|daikin_control|moritz_dash|eqmax_alarm|dyson_tp02|hyundai_.*|zeitschaltuhr)$/],
  ['Radio, buses and adapters', '2.4 GHz and 868 MHz radios, CAN, DMX, USB serial.',
    /^(a7105_.*|gm24g_lamp|lt8900_lamp|bresser_chart|can_sniffer|slcan_.*|dmx_dimmer_panel|usb_variolab_tcp)$/],
  ['Network, mail and storage', 'FTP, NAS, mail, fleet tools.', /^(ftp_.*|nas_log_upload|mail_test|device_scanner)$/],
];
const OTHER = ['Other', 'Examples that fit no group yet.'];

function readOverrides() {
  const m = new Map();
  if (!fs.existsSync(overrideFile)) return m;
  for (const l of fs.readFileSync(overrideFile, 'utf8').split('\n')) {
    const t = l.trim();
    if (!t || t.startsWith('#')) continue;
    const i = t.indexOf('|');
    if (i > 0) m.set(t.slice(0, i).trim(), t.slice(i + 1).trim());
  }
  return m;
}

// title and explanation of one example from its leading comment block
function describe(file) {
  const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/);
  let name = '';
  let i = 0;
  for (; i < Math.min(lines.length, 5); i++) {
    const m = lines[i].match(/^\/\/\s*@name:\s*(.+?)\s*$/);
    if (m) { name = m[1]; i++; break; }
  }
  if (!name) i = 0;
  const base = path.basename(file);
  const dec = /^[\s=\-*_#~.─-╿]*$/;                              // rulers made of = - * or box-drawing characters
  const parts = [];
  for (; i < lines.length; i++) {
    const l = lines[i];
    if (!l.startsWith('//')) { if (parts.length || l.trim()) break; else continue; }
    let t = l.replace(/^\/\/+\s?/, '').trim();
    if (dec.test(t)) { if (parts.length) break; else continue; }
    t = t.replace(/^[\w.\-]+\.tc\s*(?:[—–:\-]+\s*)?/i, '').trim();      // "name.tc — text" -> "text"
    t = t.replace(/^\(?\s*@\w+:.*$/, '').trim();                           // other @tags are not text
    if (!t) { if (parts.length) break; else continue; }
    parts.push(t);
    if (parts.join(' ').length > 240) break;
    if (/[.!?]$/.test(t) && parts.join(' ').length > 60) break;
  }
  let text = parts.join(' ').replace(/\s+/g, ' ').trim();
  if (text.length > 260) {
    const cut = text.slice(0, 260);
    const dot = cut.search(/[.!?]\s[^.!?]*$/);
    text = (dot > 80 ? cut.slice(0, dot + 1) : cut.replace(/\s+\S*$/, '') + ' …');
  }
  return { name: name || base.replace(/\.tc$/, ''), text };
}

const over = readOverrides();
const entry = (dir, f, rel) => {
  const key = f.replace(/\.tc$/, '');
  const d = describe(path.join(dir, f));
  return { key, file: f, rel, name: d.name, text: over.get(key) || d.text || '(no description in the file)' };
};

const tops = fs.readdirSync(exDir).filter(f => f.endsWith('.tc')).sort().map(f => entry(exDir, f, `../examples/${f}`));
const commonDir = path.join(exDir, 'common');
const commons = fs.existsSync(commonDir)
  ? fs.readdirSync(commonDir).filter(f => f.endsWith('.tc')).sort().map(f => entry(commonDir, f, `../examples/common/${f}`)) : [];

const groups = GROUPS.map(g => ({ title: g[0], intro: g[1], re: g[2], items: [] }));
const other = { title: OTHER[0], intro: OTHER[1], items: [] };
for (const e of tops) {
  const g = groups.find(x => x.re.test(e.key));
  (g || other).items.push(e);
}

const esc = s => s.replace(/\|/g, '\\|');
const line = e => `| [\`${e.file}\`](${e.rel}) | ${esc(e.name)} | ${esc(e.text)} |`;
const out = [];
out.push('# TinyC examples');
out.push('');
out.push(`Every program in [\`examples/\`](../examples/) with its title and a short explanation (${tops.length} programs, ${commons.length} include libraries).`);
out.push('');
out.push('*Generated by `tasmota/tinyc/scripts/gen_examples_doc.mjs` from the comments at the top of each example — do not edit this file.*');
out.push('*To improve an explanation, change the comment in the `.tc`, or put a line `name | text` into `scripts/examples_descriptions.txt`.*');
out.push('');
out.push('How to run one: the IDE\'s "Load Example…" list, the repo page `/tcrepo` on the device (compiles and starts the example straight from this repository), or');
out.push('`node tasmota/tinyc/tc_deploy.mjs examples/<name>.tc <device-ip>`. The `#include` libraries at the end have no `main()` and are pulled in by the programs that use them.');
out.push('');
out.push('## Contents');
out.push('');
for (const g of [...groups, ...(other.items.length ? [other] : [])]) {
  if (g.items.length) out.push(`- [${g.title}](#${g.title.toLowerCase().replace(/[^a-z0-9 ]/g, '').trim().replace(/\s+/g, '-')}) (${g.items.length})`);
}
if (commons.length) out.push(`- [Include libraries](#include-libraries) (${commons.length})`);
out.push('');
for (const g of [...groups, ...(other.items.length ? [other] : [])]) {
  if (!g.items.length) continue;
  out.push(`## ${g.title}`, '', g.intro, '', '| File | Title | What it does |', '|---|---|---|');
  for (const e of g.items) out.push(line(e));
  out.push('');
}
if (commons.length) {
  out.push('## Include libraries', '', 'Building blocks in `examples/common/`: no `main()`, used with `#include "name.tc"`.', '',
    '| File | Title | What it does |', '|---|---|---|');
  for (const e of commons) out.push(line(e));
  out.push('');
}
const text = out.join('\n');

if (other.items.length) {
  console.error(`note: ${other.items.length} example(s) fit no group, listed under "Other": ${other.items.map(e => e.key).join(', ')} — add them to GROUPS`);
}
const noText = [...tops, ...commons].filter(e => e.text.startsWith('(no description')).map(e => e.file);
if (noText.length) console.error(`note: ${noText.length} example(s) without a description: ${noText.join(', ')}`);

if (check) {
  const now = fs.existsSync(outFile) ? fs.readFileSync(outFile, 'utf8') : '';
  if (now !== text) {
    console.error('docs/EXAMPLES.md is out of date. Run: node tasmota/tinyc/scripts/gen_examples_doc.mjs');
    process.exit(1);
  }
  console.log(`docs/EXAMPLES.md is up to date (${tops.length} examples, ${commons.length} libraries)`);
} else {
  fs.writeFileSync(outFile, text);
  console.log(`wrote ${path.relative(process.cwd(), outFile)}: ${tops.length} examples, ${commons.length} libraries`);
}
