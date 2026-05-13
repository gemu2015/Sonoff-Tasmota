#!/usr/bin/env node
// patch_share_dump.mjs — TinyC IDE 1.6.2: add `shareDump` builtin + bump H1
//
// Firmware TC_RELEASE 1.6.2 adds SYS_SHARE_DUMP = 348:
//   int shareDump() — logs every live entry in the cross-VM share table
//                      via AddLog, returns the count of live entries.
//
// This patch updates the IDE in two places:
//   1. BUILTINS map — register `shareDump` so the compiler accepts the
//      name and emits the right syscall ID (348).
//   2. <h1> visible release label — bumps 1.6.0 → 1.6.2. The 1.6.1
//      release was missed (TC_RELEASE bumped but H1 hand-edit forgotten),
//      flagged by Andreas's Claude at 12:45 on 2026-05-13.
//
// Idempotent: detects already-patched state and exits clean.
//
// Usage: node patch_share_dump.mjs [path/to/tinyc_ide.html.gz]

import { readFileSync, writeFileSync, existsSync, copyFileSync } from 'node:fs';
import { gunzipSync, gzipSync } from 'node:zlib';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const target = process.argv[2] || join(__dirname, 'tinyc_ide.html.gz');

if (!existsSync(target)) {
  console.error(`ERROR: ${target} not found`);
  process.exit(1);
}

const gz = readFileSync(target);
const src = gunzipSync(gz).toString('utf8');

// ── Idempotency check
const alreadyPatched = src.includes("'shareDump':") && src.includes('TinyC IDE v1.6.2');
if (alreadyPatched) {
  console.log(`OK: ${target} already patched — shareDump + H1 v1.6.2 present.`);
  process.exit(0);
}

// ── Patch 1: add `shareDump` to BUILTINS map.
// Anchor on the existing shareDelete entry (last share-syscall in the table).
const sdelAnchor = `    'shareDelete':      { syscall: Syscall.SHARE_DELETE,    args: 1, returns: true,  constArgs: [0] },`;
const sdelWithDump = `    'shareDelete':      { syscall: Syscall.SHARE_DELETE,    args: 1, returns: true,  constArgs: [0] },
    'shareDump':        { syscall: Syscall.SHARE_DUMP,      args: 0, returns: true },`;

if (!src.includes(sdelAnchor)) {
  console.error('ERROR: shareDelete BUILTINS anchor not found — IDE source structure changed.');
  process.exit(2);
}
let patched = src.replace(sdelAnchor, sdelWithDump);

// ── Patch 2: register Syscall.SHARE_DUMP = 352 in the Syscall enum.
// Anchor on SHARE_DELETE: 347, matching the actual whitespace + comment.
// (348..351 are TCP_KEEPALIVE/NODELAY/DISCONNECT_REASON/TRANSACT; we take 352.)
const syscallAnchor = `    SHARE_DELETE:   347, // (key_const_idx)          -> int  1 if removed`;
const syscallWithDump = `    SHARE_DELETE:   347, // (key_const_idx)          -> int  1 if removed
    SHARE_DUMP:     352, // ()                       -> int  number of live entries`;
if (!patched.includes(syscallAnchor)) {
  console.error('ERROR: Syscall.SHARE_DELETE anchor not found in IDE source.');
  process.exit(3);
}
patched = patched.replace(syscallAnchor, syscallWithDump);

// ── Patch 3: bump the visible <h1> from 1.6.0 → 1.6.2.
// (Skipping 1.6.1 was the bug; we go straight to 1.6.2 to match TC_RELEASE.)
const h1Anchor = `<h1>TinyC IDE v1.6.0</h1>`;
const h1Bumped = `<h1>TinyC IDE v1.6.2</h1>`;
if (!patched.includes(h1Anchor)) {
  // Maybe the label was already updated to 1.6.1 by a manual edit — try that.
  const h1Anchor161 = `<h1>TinyC IDE v1.6.1</h1>`;
  if (patched.includes(h1Anchor161)) {
    patched = patched.replace(h1Anchor161, h1Bumped);
  } else {
    console.error('WARN: <h1> label anchor not found (expected v1.6.0 or v1.6.1).');
    console.error('      Skipping H1 patch; BUILTINS patch will still apply.');
  }
} else {
  patched = patched.replace(h1Anchor, h1Bumped);
}

if (patched === src) {
  console.error('ERROR: no changes were made.');
  process.exit(4);
}

// ── Backup + write
const bakPath = target + '.bak';
if (!existsSync(bakPath)) copyFileSync(target, bakPath);

const out = gzipSync(Buffer.from(patched, 'utf8'), { level: 9 });
writeFileSync(target, out);

console.log(`OK: patched ${target}`);
console.log(`    backup:           ${bakPath}`);
console.log(`    size:             ${gz.length} → ${out.length} bytes`);
console.log(`    new builtin:      shareDump() — returns int (live entry count)`);
console.log(`    H1 label bumped:  → v1.6.2`);
