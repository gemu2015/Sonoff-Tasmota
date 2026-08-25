#!/usr/bin/env node
// check_docs.mjs — hold the reference tables to account against the rest of the
// repo. Run from anywhere:  node tasmota/tinyc/scripts/check_docs.mjs
//
// Written after 1.6.60, where a documentation pass turned up four places in
// which a TABLE contradicted something the repo already stated elsewhere:
//
//   * the VM-limits table said the ESP32 heap was 32 KB / 32 handles while
//     TC_MAX_HEAP had said 16384 slots (64 KB) / 128 for several releases,
//     and globals 256 vs TC_MAX_GLOBALS 512, and the ESP8266 constant pool
//     32 vs TC_MAX_CONSTANTS 64;
//   * "Unterschiede zu Standard-C" listed function pointers and
//     multi-dimensional arrays as NOT SUPPORTED although both have their own
//     versioned chapter a few thousand lines further up in the same file;
//   * the same table said structs cannot nest while the struct chapter's own
//     notes say, in bold, that they can;
//   * the German table said the slot pointer array costs 16 bytes (4 pointers)
//     while the prose two lines above it said 24, and TC_MAX_VMS is 6.
//
// Each of those is mechanically checkable, and each survived years of reading
// because nobody reads a reference front to back. Hence three checks:
//
//   A  contradiction — a comparison-table row calls a feature unsupported
//                      although a versioned chapter for it exists in the file
//   B  constants     — a limits-table cell disagrees with the #define it names,
//                      OR states a number and names no constant at all. The
//                      second half matters as much as the first: the table was
//                      un-annotated for years, so there was nothing to check it
//                      against, and a checker that stayed quiet about that would
//                      have reported "clean" on the very version that drifted.
//   C  parity        — the German and English comparison tables disagree about
//                      what is supported (language-independent: only the
//                      not-supported flags are compared, never the wording)
//
// Plus a structural guard: each table must be identifiable exactly once. Both
// references hold several decoy tables that open with "Feature"/"Merkmal" or
// "Resource"/"Ressource", and an early version of this script matched the first
// of them and passed documents that were provably wrong.
//
// Verified against git history: run over the pre-1.6.60 references it reports
// 27 findings including both real contradictions and the DE/EN row-count
// divergence; over the corrected ones, none.
//
// Exit status 0 = nothing found, 1 = findings, 2 = the script could not run
// (missing file) — so it can gate a release without a read failure being
// mistaken for a clean bill of health.

import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join, relative } from 'path';

const here = dirname(fileURLToPath(import.meta.url));
const tinyc = join(here, '..');                    // tasmota/tinyc
const tasmota = join(tinyc, '..');                 // tasmota
const root = join(tasmota, '..');                  // repo root

const VM_HEADER = join(tasmota, 'include', 'xdrv_124_tinyc_vm.h');
const DOCS = [
  { lang: 'en', path: join(tinyc, 'TinyC_Reference.md') },
  { lang: 'de', path: join(tinyc, 'TinyC_Reference_DE.md') },
];

const rel = (p) => relative(root, p);
const argv = process.argv.slice(2);
const VERBOSE = argv.includes('--verbose') || argv.includes('-v');

const findings = [];
const notes = [];
const add = (check, file, line, msg) => findings.push({ check, file, line, msg });

/* ───────────────────────── Markdown parsing ─────────────────────────────── */

// Headings, with the *(since X.Y.Z)* / *(seit X.Y.Z)* marker pulled out. That
// marker is the useful signal: it is only ever written when a feature landed,
// so a heading carrying one is proof the feature EXISTS.
function headings(text) {
  const out = [];
  text.split('\n').forEach((l, i) => {
    const m = /^(#{2,4})\s+(.*?)\s*$/.exec(l);
    if (!m) return;
    const ver = /\*\((?:since|seit)\s+([0-9][0-9.]*)\)\*/i.exec(m[2]);
    out.push({
      line: i + 1,
      depth: m[1].length,
      title: m[2].replace(/\*\((?:since|seit)[^)]*\)\*/i, '').trim(),
      version: ver ? ver[1] : null,
    });
  });
  return out;
}

// Pipe tables. A table is a header row, a separator row of dashes, then body
// rows, all starting with '|'. Cells are split on unescaped pipes so a `\|`
// inside a cell (the compound-assignment row has one) does not open a column.
function tables(text) {
  const lines = text.split('\n');
  const out = [];
  for (let i = 0; i < lines.length - 1; i++) {
    if (!lines[i].trimStart().startsWith('|')) continue;
    if (!/^\s*\|[\s:|-]*\|\s*$/.test(lines[i + 1] || '')) continue;
    const header = cells(lines[i]);
    const rows = [];
    let j = i + 2;
    for (; j < lines.length && lines[j].trimStart().startsWith('|'); j++) {
      rows.push({ line: j + 1, cells: cells(lines[j]) });
    }
    out.push({ line: i + 1, header, rows });
    i = j - 1;
  }
  return out;
}

function cells(line) {
  const parts = [];
  let cur = '';
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '\\' && line[i + 1] === '|') { cur += '\\|'; i++; continue; }
    if (c === '|') { parts.push(cur); cur = ''; continue; }
    cur += c;
  }
  parts.push(cur);
  // A leading and trailing pipe produce empty outer cells — drop them.
  if (parts.length && parts[0].trim() === '') parts.shift();
  if (parts.length && parts[parts.length - 1].trim() === '') parts.pop();
  return parts.map((p) => p.trim());
}

const COMPARISON_HEAD = /^(feature|merkmal)$/i;
const LIMITS_HEAD = /^(resource|ressource)$/i;

// ⚠️ Identify a table by its FULL shape, never by its first header cell, and
// never with `.find()`. Both references contain several tables that open with
// "Feature" / "Merkmal": the two-column "Not in v1" lists in the
// function-pointer and struct chapters, and a "Feature | Use what instead"
// table — all of them before the real comparison table. A first-cell match
// therefore picked a two-column decoy, found no not-supported rows in it, and
// reported a clean bill of health on documents that were provably wrong. That
// is the exact failure this script exists to prevent, so it refuses to guess:
// zero matches or more than one is a finding, not a silent pick.
const isComparison = (t) =>
  t.header.length >= 3 && COMPARISON_HEAD.test(t.header[0]) && /^standard/i.test(t.header[1] || '');
const isLimits = (t) =>
  t.header.length >= 3 && LIMITS_HEAD.test(t.header[0]) && t.header.some((h) => /esp8266/i.test(h));

function pickTable(tbls, pred, label, file) {
  const hits = tbls.filter((t) => t.header.length && pred(t));
  if (hits.length === 1) return hits[0];
  add('structure', file, hits[0]?.line ?? 1,
    hits.length === 0
      ? `no ${label} table found — its checks did not run. Has its header changed?`
      : `${hits.length} tables look like the ${label} table (lines ${hits.map((h) => h.line).join(', ')}) ` +
        `— cannot tell which to check`);
  return null;
}

/* ───────────────────────── Text normalisation ───────────────────────────── */

// Fold German umlauts so "Größe"/"Groesse" and "Zähler"/"Zaehler" tokenise the
// same. The references are written with ASCII transliterations already, but the
// chapter titles are not consistently so.
const fold = (s) =>
  s.toLowerCase()
    .replace(/ä/g, 'ae').replace(/ö/g, 'oe').replace(/ü/g, 'ue').replace(/ß/g, 'ss')
    .replace(/[`*_]/g, ' ');

// Tokens of 4+ characters. Short ones ("2d", "of", "in") carry no signal and
// would match everything.
const tokens = (s) => new Set(fold(s).split(/[^a-z0-9]+/).filter((t) => t.length >= 4));

const NOT_SUPPORTED = /\*\*(not supported|nicht unterst(?:ue|ü)tzt)\*\*/i;
// A version claim anywhere in the row means it documents partial support.
const PARTIAL = /\b(since|seit)\s+\d/i;

/* ───────── A — a "not supported" row against the file's own chapters ────── */

function checkContradictions(doc, text, tbls, heads) {
  const table = pickTable(tbls, isComparison, 'comparison', doc.path);
  if (!table) return;

  // Only headings that carry a version marker count as proof a feature exists.
  // An ordinary heading is too weak: "goto" could plausibly head a section that
  // explains why it is absent.
  const versioned = heads.filter((h) => h.version).map((h) => ({ ...h, tok: tokens(h.title) }));

  for (const row of table.rows) {
    const claim = row.cells.slice(1).join(' ');
    if (!NOT_SUPPORTED.test(claim)) continue;
    // A row that names a version is describing PARTIAL support, and the
    // negative belongs to the part that is genuinely missing: "2D supported
    // since 1.3.38 … 3D+ **not supported**" is a correct row and must not be
    // read as "multi-dimensional arrays do not exist".
    if (PARTIAL.test(claim)) continue;

    const feature = row.cells[0];
    const ftok = tokens(feature);
    for (const h of versioned) {
      // Require one token set to CONTAIN the other, not merely to overlap.
      // Overlap alone equates any two rows sharing a common noun: it paired
      // "Multi-dimensional arrays" with "Packed byte arrays" on the word
      // "arrays", and "Pointers (data)" with "Function Pointers" on
      // "pointers" — two features that are unrelated on purpose. Containment
      // keeps "Multi-dimensional arrays" ⊃ "2D Arrays" and
      // "Funktionszeiger" = "Funktionszeiger", and drops both false pairs.
      if (!ftok.size || !h.tok.size) continue;
      const shared = [...ftok].filter((t) => h.tok.has(t));
      if (shared.length !== Math.min(ftok.size, h.tok.size)) continue;
      add('contradiction', doc.path, row.line,
        `row "${feature}" says NOT SUPPORTED, but "${h.title}" (since ${h.version}) ` +
        `exists at line ${h.line} — shared term: ${shared.join(', ')}`);
    }
  }
}

/* ───────── B — a limits-table cell against the constant it names ────────── */

// A miniature preprocessor: walk the header tracking which platform branch we
// are inside, so `#ifdef ESP8266 … #else … #endif` (and TC_MAX_VMS's reversed
// `#ifdef ESP32 … #else …`) both resolve correctly. `#ifndef GUARD` and plain
// `#if` do not select a platform — they push a neutral frame, so a define
// inside an override guard still lands in whatever platform encloses it.
function parseDefines(src) {
  const vals = {};            // NAME -> { esp8266, esp32 }
  const stack = [];           // 'esp8266' | 'esp32' | null
  const put = (name, value) => {
    const plat = [...stack].reverse().find((p) => p !== null) || null;
    const e = (vals[name] ||= { esp8266: null, esp32: null });
    // First definition wins, matching the #ifndef-guard semantics above.
    if (plat === null) { e.esp8266 ??= value; e.esp32 ??= value; }
    else e[plat] ??= value;
  };
  for (const raw of src.split('\n')) {
    const l = raw.trim();
    let m;
    if ((m = /^#ifdef\s+(\w+)/.exec(l))) {
      stack.push(m[1] === 'ESP8266' ? 'esp8266' : m[1] === 'ESP32' ? 'esp32' : null);
    } else if (/^#if(n?def)?\b/.test(l)) {
      stack.push(null);
    } else if (/^#else\b/.test(l)) {
      const top = stack.pop();
      stack.push(top === 'esp8266' ? 'esp32' : top === 'esp32' ? 'esp8266' : null);
    } else if (/^#endif\b/.test(l)) {
      stack.pop();
    } else if ((m = /^#define\s+(TC_[A-Z0-9_]+)\s+(\d+)\b/.exec(l))) {
      put(m[1], Number(m[2]));
    }
  }
  return vals;
}

// Read the leading quantity out of a cell. A trailing unit word is ignored
// ("32 keys" is thirty-two), "KB" is kept because it changes the arithmetic,
// and a range ("21–55", the per-chip GPIO count) returns null because it
// deliberately is not one value.
function quantity(cell) {
  const s = fold(cell).replace(/[.,](?=\d{3}\b)/g, '').trim();   // 20.000 / 1,000 → 20000
  const m = /^(\d+)\s*(kb|kib)?\b/.exec(s);
  if (!m) return null;
  if (/^\d+\s*[–—-]\s*\d/.test(s)) return null;                  // a range, not a value
  return { n: Number(m[1]), kb: !!m[2] };
}

// Does `q` state `value`? Two units are in play and both are correct depending
// on the constant: TC_MAX_PROGRAM counts bytes, so its 131072 is "128 KB",
// while TC_MAX_HEAP counts int32 slots, so its 16384 is "64 KB" too. Accepting
// either is deliberate — the check is for a WRONG number, not for a house style.
const states = (q, value) =>
  q.kb ? (q.n * 1024 === value || q.n * 1024 === value * 4) : q.n === value;

function checkConstants(doc, tbls, defines) {
  const table = pickTable(tbls, isLimits, 'VM-limits', doc.path);
  if (!table) return;
  const cols = table.header.map((h) => fold(h).trim());
  const iEsp8266 = cols.findIndex((c) => c.includes('esp8266'));
  const iEsp32 = cols.findIndex((c) => /esp32/.test(c));
  if (iEsp8266 < 0 || iEsp32 < 0) {
    notes.push(`${rel(doc.path)}: limits table has no ESP8266/ESP32 columns — check B skipped`);
    return;
  }

  for (const row of table.rows) {
    // Any ALL-CAPS backticked identifier is a candidate. Restricting this to
    // `TC_*` missed the GPIO row, whose governing constant is Tasmota's own
    // MAX_GPIO_PIN — and reported it as "names no constant", which reads like
    // the row is fine when in truth nothing was checked.
    const named = [...new Set((row.cells.join(' ').match(/`([A-Z][A-Z0-9_]{2,})`/g) || [])
      .map((s) => s.replace(/`/g, '')))];
    const known = named.filter((n) => defines[n]);
    if (named.length === 0) {
      // Not a note: an un-annotated row is precisely how the table drifted for
      // several releases. Nothing here can be verified, and a silent skip
      // would let the whole check report "clean" on a table it never read.
      const stated = [iEsp8266, iEsp32]
        .map((i) => row.cells[i]).filter((c) => c !== undefined && quantity(c));
      if (stated.length) {
        add('constants', doc.path, row.line,
          `${row.cells[0]}: states ${stated.map((c) => `"${c}"`).join(' / ')} but names no ` +
          `constant — nothing to verify it against. Add the governing \`TC_*\` name to the notes column`);
      }
      continue;
    }
    if (known.length === 0) {
      notes.push(`${rel(doc.path)}:${row.line} "${row.cells[0]}" names ${named.join(', ')}, ` +
        `not #defined in ${rel(VM_HEADER)} — not checked`);
      continue;
    }
    // A row may mention a second constant in passing (the cross-VM row names
    // TC_MAX_VMS while being about TC_SHARE_MAX). Accept the row if ANY named
    // constant accounts for the cell; only complain when none does.
    for (const [plat, idx] of [['esp8266', iEsp8266], ['esp32', iEsp32]]) {
      const cell = row.cells[idx];
      if (cell === undefined) continue;
      const q = quantity(cell);
      if (!q) continue;                       // "n/a", "unlimited", a range — nothing asserted
      const candidates = known.filter((n) => defines[n][plat] !== null);
      if (!candidates.length) continue;
      if (candidates.some((n) => states(q, defines[n][plat]))) continue;
      const shown = candidates
        .map((n) => `${n} = ${defines[n][plat]}`).join(', ');
      add('constants', doc.path, row.line,
        `${row.cells[0]} / ${plat.toUpperCase()}: table says "${cell}", but ${shown}`);
    }
  }
}

/* ───────── C — do the two languages agree about what exists? ───────────── */

function checkParity(docs) {
  const [a, b] = docs;
  const ta = pickTable(a.tables, isComparison, 'comparison', a.path);
  const tb = pickTable(b.tables, isComparison, 'comparison', b.path);
  if (!ta || !tb) return;   // pickTable already reported why

  if (ta.rows.length !== tb.rows.length) {
    add('parity', b.path, tb.line,
      `comparison tables differ in size: ${rel(a.path)} has ${ta.rows.length} rows, ` +
      `${rel(b.path)} has ${tb.rows.length} — per-row comparison skipped, align them first`);
    return;
  }

  // Compare only the not-supported FLAG, never the prose: the two files are
  // written independently and must be allowed to word a row differently. A
  // disagreement about supported-or-not, on the other hand, means one of them
  // is lying to its readers.
  for (let i = 0; i < ta.rows.length; i++) {
    const ra = ta.rows[i], rb = tb.rows[i];
    const na = NOT_SUPPORTED.test(ra.cells.slice(1).join(' '));
    const nb = NOT_SUPPORTED.test(rb.cells.slice(1).join(' '));
    if (na === nb) continue;
    const yes = na ? b : a, no = na ? a : b;
    const yesRow = na ? rb : ra, noRow = na ? ra : rb;
    add('parity', no.path, noRow.line,
      `"${noRow.cells[0]}" is marked NOT SUPPORTED in ${rel(no.path)}:${noRow.line}, ` +
      `but supported in ${rel(yes.path)}:${yesRow.line} ("${yesRow.cells[0]}") — one of them is wrong`);
  }
}

/* ───────────────────────────── Run ──────────────────────────────────────── */

let defines;
try {
  defines = parseDefines(readFileSync(VM_HEADER, 'utf-8'));
} catch (e) {
  console.error(`check_docs: cannot read ${rel(VM_HEADER)}: ${e.message}`);
  process.exit(2);
}

const loaded = [];
for (const doc of DOCS) {
  let text;
  try {
    text = readFileSync(doc.path, 'utf-8');
  } catch (e) {
    console.error(`check_docs: cannot read ${rel(doc.path)}: ${e.message}`);
    process.exit(2);
  }
  const tbls = tables(text);
  const heads = headings(text);
  checkContradictions(doc, text, tbls, heads);
  checkConstants(doc, tbls, defines);
  loaded.push({ ...doc, tables: tbls });
}
checkParity(loaded);

/* ───────────────────────────── Report ───────────────────────────────────── */

const LABEL = {
  structure: 'a table could not be identified — checks did not run',
  contradiction: 'table contradicts a chapter in the same file',
  constants: 'table disagrees with the firmware #define',
  parity: 'the two languages disagree',
};

if (findings.length) {
  const byCheck = new Map();
  for (const f of findings) (byCheck.get(f.check) || byCheck.set(f.check, []).get(f.check)).push(f);
  for (const [check, list] of byCheck) {
    console.log(`\n${check.toUpperCase()} — ${LABEL[check]} (${list.length})`);
    for (const f of list) console.log(`  ${rel(f.file)}:${f.line}\n    ${f.msg}`);
  }
}

if (notes.length && (VERBOSE || findings.length)) {
  console.log(`\nNOT CHECKED (${notes.length})`);
  for (const n of notes) console.log(`  ${n}`);
} else if (notes.length) {
  console.log(`(${notes.length} rows not checked — run with --verbose to list them)`);
}

console.log(
  findings.length
    ? `\n✗ ${findings.length} finding${findings.length === 1 ? '' : 's'}`
    : '\n✓ tables agree with the chapters, the #defines, and each other'
);
process.exit(findings.length ? 1 : 0);
