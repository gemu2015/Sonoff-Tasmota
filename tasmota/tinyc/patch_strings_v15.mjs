#!/usr/bin/env node
// patch_strings_v15.mjs — TinyC 1.5.0 string-ops additions
//
// 7 new char[] operations (literal-needle variants — needle is always a
// string literal at the call site; the IDE compiles to a const-pool index):
//
//   int n = strReplace(buf, "old", "new");      // in-place, returns count
//   if  (strStartsWith(buf, "ON")) {...}        // 1/0
//   if  (strEndsWith(file, ".tcb")) {...}       // 1/0
//   if  (strContains(html, "<error>")) {...}    // 1/0
//   strToUpper(buf);                             // in-place ASCII A-Z
//   strToLower(buf);                             // in-place ASCII a-z
//   int new_len = strTrim(buf);                  // in-place strip ws, return new len
//
// Idempotent. VM impact: 7 new SYS_* IDs (302–308) handled in firmware
// (xdrv_124_tinyc_vm.h); IDE side only adds Syscall enum entries +
// BUILTINS table.

import fs from 'node:fs';
import zlib from 'node:zlib';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = path.dirname(__filename);
const inPath  = process.argv[2] || path.join(__dirname, 'tinyc_ide.html.gz');
const outPath = process.argv[3] || inPath;

let html = zlib.gunzipSync(fs.readFileSync(inPath)).toString('utf-8');
const orig = html;

let stepNum = 0;
function patch(name, find, replace) {
    stepNum++;
    const tag = `${String(stepNum).padStart(2,'0')} ${name}`;
    if (html.includes(replace) && !html.includes(find)) {
        console.log(`[noop ] ${tag}`); return;
    }
    if (!html.includes(find)) {
        throw new Error(`patch "${name}" — anchor not found`);
    }
    html = html.replace(find, replace);
    console.log(`[ok   ] ${tag}`);
}

// ─────────────────────────────────────────────────────────────────────
// 1. Syscall enum: add the 7 new IDs after KILL_TASK / TASK_RUNNING
// ─────────────────────────────────────────────────────────────────────
patch('Syscall enum: STR_* additions',
`    KILL_TASK:        300, // (name_const) -> int 0=signaled, -1=not running
    TASK_RUNNING:     301, // (name_const) -> int 1/0`,
`    KILL_TASK:        300, // (name_const) -> int 0=signaled, -1=not running
    TASK_RUNNING:     301, // (name_const) -> int 1/0

    // String ops 1.5.0
    STR_REPLACE_CONST:   302, // (arr, old_const, new_const) -> int (count)
    STR_STARTS_CONST:    303, // (arr, prefix_const) -> int 1/0
    STR_ENDS_CONST:      304, // (arr, suffix_const) -> int 1/0
    STR_CONTAINS_CONST:  305, // (arr, substr_const) -> int 1/0
    STR_TO_UPPER:        306, // (arr) -> void
    STR_TO_LOWER:        307, // (arr) -> void
    STR_TRIM:            308, // (arr) -> int (new length)`);

// ─────────────────────────────────────────────────────────────────────
// 2. BUILTINS table: register the 7 new functions
// ─────────────────────────────────────────────────────────────────────
patch('BUILTINS: register strReplace/Starts/Ends/Contains/ToUpper/ToLower/Trim',
`    'strFind':          { syscall: Syscall.STR_FIND,        args: 2, returns: true,  strArgs: [0, 1] },`,
`    'strFind':          { syscall: Syscall.STR_FIND,        args: 2, returns: true,  strArgs: [0, 1] },

    // ── 1.5.0 string ops (literal-needle / in-place) ──
    'strReplace':       { syscall: Syscall.STR_REPLACE_CONST,  args: 3, returns: true,  strArgs: [0], constArgs: [1, 2] },
    'strStartsWith':    { syscall: Syscall.STR_STARTS_CONST,   args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strEndsWith':      { syscall: Syscall.STR_ENDS_CONST,     args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strContains':      { syscall: Syscall.STR_CONTAINS_CONST, args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strToUpper':       { syscall: Syscall.STR_TO_UPPER,       args: 1, returns: false, strArgs: [0] },
    'strToLower':       { syscall: Syscall.STR_TO_LOWER,       args: 1, returns: false, strArgs: [0] },
    'strTrim':          { syscall: Syscall.STR_TRIM,           args: 1, returns: true,  strArgs: [0] },`);

// ─────────────────────────────────────────────────────────────────────
// Write back
// ─────────────────────────────────────────────────────────────────────
if (html === orig) {
    console.log('No changes — already fully patched.');
} else {
    fs.writeFileSync(outPath, zlib.gzipSync(html, { level: 9 }));
    const sz = (fs.statSync(outPath).size / 1024).toFixed(1);
    console.log(`\nWrote ${outPath} (${sz} KB gzipped)`);
}
