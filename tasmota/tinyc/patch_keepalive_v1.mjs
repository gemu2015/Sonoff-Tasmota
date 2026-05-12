#!/usr/bin/env node
// patch_keepalive_v1.mjs — TinyC IDE: register the WebOn raw / keep-alive
// syscalls (SYS_WEB_RAW_MODE / SYS_WEB_RAW_WRITE / SYS_WEB_KEEP_ALIVE,
// numbers 390..392).
//
//   webRawMode()           -> void   disable Tasmota auto-headers for this request
//   webRawWrite(buf)       -> void   write raw bytes to Webserver->client()
//   webKeepAlive()         -> void   keep TCP socket alive for the next request
//
// Motivating use case: emulating EcoTracker-style devices whose firmware
// expects an exact 3-header HTTP response shape AND a kept-alive TCP
// connection — Jackery Homepower 2000 Ultra and friends. See
// sdeigm/uni-meter#265 and ottelo/tasmota-sml-script#24 for the
// investigation history, and `examples/ecotracker.tc` (post-rewrite)
// for the canonical usage pattern.
//
// What this patch adds to the IDE HTML:
//   1. Syscall enum entries  WEB_RAW_MODE / WEB_RAW_WRITE / WEB_KEEP_ALIVE
//   2. BUILTINS entries      'webRawMode' / 'webRawWrite' / 'webKeepAlive'
//   3. Simulator stubs that emit a `[WEB]` log line and return — so
//      standalone-IDE runs don't crash when the script under test calls
//      these on a host without a real HTTP server.
//
// Idempotent. Re-run safely after each IDE rebuild that drops the
// entries below.

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
// 1. Syscall enum: add the three new entries after TWAI_FILTER (386).
// ─────────────────────────────────────────────────────────────────────
patch('Syscall enum: WEB_RAW_MODE / WEB_RAW_WRITE / WEB_KEEP_ALIVE = 390..392',
`    TWAI_FILTER:             386, // (id_mask, id_value, ext) -> int 1=ok 0=err`,
`    TWAI_FILTER:             386, // (id_mask, id_value, ext) -> int 1=ok 0=err

    // WebOn raw / keep-alive controls (390..392). Required for emulating
    // EcoTracker-style devices whose firmware needs an exact 3-header
    // HTTP response (no auto Server/Date/Connection) plus a kept-alive
    // TCP socket — Jackery Homepower 2000 Ultra and similar storages.
    WEB_RAW_MODE:            390, // ()           -> void  disable Tasmota auto-headers for this request
    WEB_RAW_WRITE:           391, // (str_ref)    -> void  write raw bytes to Webserver->client()
    WEB_KEEP_ALIVE:          392, // ()           -> void  keep TCP socket alive after response`);

// ─────────────────────────────────────────────────────────────────────
// 2. BUILTINS table: register the three webRaw* / webKeepAlive names.
//    strArgs marker on webRawWrite tells the compiler arg 0 is a
//    runtime char[] / int[] array ref (push as array-ref, not int).
// ─────────────────────────────────────────────────────────────────────
patch('BUILTINS: register webRawMode / webRawWrite / webKeepAlive',
`    'twaiFilter':           { syscall: Syscall.TWAI_FILTER,            args: 3, returns: true },`,
`    'twaiFilter':           { syscall: Syscall.TWAI_FILTER,            args: 3, returns: true },

    // WebOn raw response + keep-alive controls.
    'webRawMode':           { syscall: Syscall.WEB_RAW_MODE,           args: 0, returns: false },
    'webRawWrite':          { syscall: Syscall.WEB_RAW_WRITE,          args: 1, returns: false,
                              strArgs: [0] },             // str_ref
    'webKeepAlive':         { syscall: Syscall.WEB_KEEP_ALIVE,         args: 0, returns: false },`);

// ─────────────────────────────────────────────────────────────────────
// 3. Simulator stubs. Standalone IDE runs without a real Tasmota host
//    need to POP the right number of args and emit a friendly log line.
//    No PUSH needed (all three syscalls are void-returning).
// ─────────────────────────────────────────────────────────────────────
patch('Simulator: webRaw* / webKeepAlive stubs',
`            case Syscall.TWAI_FILTER: { // twaiFilter(mask, value, ext) -> int
                this.pop(); this.pop(); this.pop();
                this.push(1); // accepted
                break;
            }`,
`            case Syscall.TWAI_FILTER: { // twaiFilter(mask, value, ext) -> int
                this.pop(); this.pop(); this.pop();
                this.push(1); // accepted
                break;
            }
            case Syscall.WEB_RAW_MODE: { // webRawMode()
                this.onOutput(\`[WEB] webRawMode() — simulator stub\\n\`);
                break;
            }
            case Syscall.WEB_RAW_WRITE: { // webRawWrite(buf)
                const buf_ref = this.pop();
                this.onOutput(\`[WEB] webRawWrite(buf=ref \${buf_ref}) — simulator stub (would write to client socket)\\n\`);
                break;
            }
            case Syscall.WEB_KEEP_ALIVE: { // webKeepAlive()
                this.onOutput(\`[WEB] webKeepAlive() — simulator stub (would arm Webserver->setKeepAlive(true))\\n\`);
                break;
            }`);

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
