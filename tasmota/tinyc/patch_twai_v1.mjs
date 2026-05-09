#!/usr/bin/env node
// patch_twai_v1.mjs — TinyC IDE: register the TWAI / CAN-bus syscalls.
//
// Adds the seven SYS_TWAI_* syscalls (380..386) to the IDE so scripts
// can drive a CAN-bus on ESP32 / ESP32-C3 / ESP32-S3:
//
//   twaiBegin(rxpin, txpin, bitrate_kbps, mode)        -> int 1=ok 0=err
//   twaiEnd()                                          -> void
//   twaiAvailable()                                    -> int frames_queued
//   twaiRecv(id_ref, ext_ref, dlc_ref, data_buf, max)  -> int bytes_read | -1
//   twaiSend(id, ext, dlc, data_buf)                   -> int 1=ok 0=err
//   twaiStatus(rx_q_ref, tx_q_ref, err_ref)            -> int 1=ok 0=err
//   twaiFilter(id_mask, id_value, ext)                 -> int 1=ok 0=err (hint
//                                                              for next Begin)
//
// Motivating use case: an SLCAN-protocol bridge running on ESP32-C3 +
// CAN transceiver, terminating a USB-CDC connection from the desktop
// SML emulator. See `examples/slcan_bridge.tc`.
//
// What this patch adds to the IDE HTML:
//   1. Syscall enum entries  TWAI_BEGIN..TWAI_FILTER (380..386)
//   2. BUILTINS entries      'twaiBegin', 'twaiEnd', 'twaiAvailable',
//                            'twaiRecv', 'twaiSend', 'twaiStatus',
//                            'twaiFilter'
//   3. Simulator stubs that print a `[TWAI]` log line and return a
//      sentinel, so standalone-IDE runs don't fail on TWAI calls.
//
// Idempotent. Run after each IDE rebuild that doesn't already include
// the entries below.

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
// 1. Syscall enum: add TWAI_* block after BLIB_CALL.
// ─────────────────────────────────────────────────────────────────────
patch('Syscall enum: TWAI_* = 380..386',
`    BLIB_CALL:               370, // (name_const, buf_ref, len) -> int`,
`    BLIB_CALL:               370, // (name_const, buf_ref, len) -> int

    // TWAI / CAN-bus (380..386). ESP32-only on the device; standalone
    // IDE runs return sentinel values via the simulator stubs below.
    TWAI_BEGIN:              380, // (rxpin, txpin, bitrate_kbps, mode) -> int 1=ok 0=err
    TWAI_END:                381, // () -> void
    TWAI_AVAILABLE:          382, // () -> int frames_queued
    TWAI_RECV:               383, // (id_ref, ext_ref, dlc_ref, data_buf, max) -> int bytes | -1
    TWAI_SEND:               384, // (id, ext, dlc, data_buf) -> int 1=ok 0=err
    TWAI_STATUS:             385, // (rx_q_ref, tx_q_ref, err_ref) -> int 1=ok 0=err
    TWAI_FILTER:             386, // (id_mask, id_value, ext) -> int 1=ok 0=err`);

// ─────────────────────────────────────────────────────────────────────
// 2. BUILTINS table: register the seven twai* names.
//    `strArgs` marks args that are TinyC char[] / int[] runtime arrays
//    rather than plain ints — the compiler emits an array-ref push for
//    those. ID/dlc/max/bitrate args are plain ints, no special handling.
// ─────────────────────────────────────────────────────────────────────
patch('BUILTINS: register twai* (7 fns)',
`    'bcall':                { syscall: Syscall.BLIB_CALL,             args: 3, returns: true,
                              constArgs: [0], strArgs: [1] },`,
`    'bcall':                { syscall: Syscall.BLIB_CALL,             args: 3, returns: true,
                              constArgs: [0], strArgs: [1] },

    // TWAI / CAN-bus syscalls.
    'twaiBegin':            { syscall: Syscall.TWAI_BEGIN,             args: 4, returns: true },
    'twaiEnd':              { syscall: Syscall.TWAI_END,               args: 0, returns: false },
    'twaiAvailable':        { syscall: Syscall.TWAI_AVAILABLE,         args: 0, returns: true },
    'twaiRecv':             { syscall: Syscall.TWAI_RECV,              args: 3, returns: true,
                              strArgs: [0, 1] },          // meta_arr, data_buf
    'twaiSend':             { syscall: Syscall.TWAI_SEND,              args: 4, returns: true,
                              strArgs: [3] },             // data_buf
    'twaiStatus':           { syscall: Syscall.TWAI_STATUS,            args: 1, returns: true,
                              strArgs: [0] },             // stats_arr
    'twaiFilter':           { syscall: Syscall.TWAI_FILTER,            args: 3, returns: true },`);

// ─────────────────────────────────────────────────────────────────────
// 3. Simulator stubs. Standalone IDE runs (without a device) need to
//    POP the right number of args, print a friendly note, and PUSH a
//    sentinel value so subsequent VM ops don't underflow the stack.
// ─────────────────────────────────────────────────────────────────────
patch('Simulator: twai* stubs',
`            case Syscall.BLIB_CALL: { // bcall("name", buf, len) -> int
                const len      = this.pop();
                const buf_ref  = this.pop();
                const name_ci  = this.pop();
                const name     = (this.consts && this.consts[name_ci]) || '?';
                this.onOutput(\`[BLIB] bcall("\${name}", buf, len=\${len}) — simulator stub, returning -1 (no registry on host)\\n\`);
                this.push(-1);
                break;
            }`,
`            case Syscall.BLIB_CALL: { // bcall("name", buf, len) -> int
                const len      = this.pop();
                const buf_ref  = this.pop();
                const name_ci  = this.pop();
                const name     = (this.consts && this.consts[name_ci]) || '?';
                this.onOutput(\`[BLIB] bcall("\${name}", buf, len=\${len}) — simulator stub, returning -1 (no registry on host)\\n\`);
                this.push(-1);
                break;
            }
            case Syscall.TWAI_BEGIN: { // twaiBegin(rx, tx, kbps, mode) -> int
                const mode    = this.pop();
                const kbps    = this.pop();
                const tx_pin  = this.pop();
                const rx_pin  = this.pop();
                this.onOutput(\`[TWAI] twaiBegin(rx=\${rx_pin}, tx=\${tx_pin}, \${kbps} kbps, mode=\${mode}) — simulator stub, returning 0 (no CAN on host)\\n\`);
                this.push(0);
                break;
            }
            case Syscall.TWAI_END: { // twaiEnd()
                this.onOutput(\`[TWAI] twaiEnd() — simulator stub\\n\`);
                break;
            }
            case Syscall.TWAI_AVAILABLE: { // twaiAvailable() -> int
                this.push(0);
                break;
            }
            case Syscall.TWAI_RECV: { // twaiRecv(meta_arr, data_buf, max) -> int
                this.pop(); this.pop(); this.pop();
                this.push(0); // no frame
                break;
            }
            case Syscall.TWAI_SEND: { // twaiSend(id, ext, dlc, buf) -> int
                const buf_ref = this.pop();
                const dlc     = this.pop();
                const ext     = this.pop();
                const id      = this.pop();
                this.onOutput(\`[TWAI] twaiSend(id=0x\${(id>>>0).toString(16)}, ext=\${ext}, dlc=\${dlc}) — simulator stub, returning 1\\n\`);
                this.push(1);
                break;
            }
            case Syscall.TWAI_STATUS: { // twaiStatus(stats_arr) -> int
                this.pop();
                this.push(0); // not installed
                break;
            }
            case Syscall.TWAI_FILTER: { // twaiFilter(mask, value, ext) -> int
                this.pop(); this.pop(); this.pop();
                this.push(1); // accepted
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
