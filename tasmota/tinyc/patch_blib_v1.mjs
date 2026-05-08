#!/usr/bin/env node
// patch_blib_v1.mjs — TinyC IDE: register the bcall() syscall.
//
// `bcall("name", buf, len)` invokes a registered binary-library export
// via SYS_BLIB_CALL = 370. Phase-1 supports the (BUF, INT) -> INT
// signature shared by xblib_01_crc's mb_crc16 / crc32 / crc8_dallas;
// other shapes will be added later.
//
// What this patch adds to the IDE HTML:
//   1. Syscall enum entry  BLIB_CALL: 370
//   2. BUILTINS entry      'bcall': { syscall: …, args: 3, returns: true,
//                                     constArgs: [0], strArgs: [1] }
//      (arg 0 = string literal name, arg 1 = char[] buffer ref, arg 2 = int)
//   3. Simulator stub returning -1 so standalone-IDE runs don't fail
//      on bcall calls (the registry only exists on the device).
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
// 1. Syscall enum: add BLIB_CALL after TCP_TRANSACT.
// ─────────────────────────────────────────────────────────────────────
patch('Syscall enum: BLIB_CALL = 370',
`    TCP_TRANSACT:            351, // (req_ref, req_len, resp_ref, resp_max, timeout_ms) -> int`,
`    TCP_TRANSACT:            351, // (req_ref, req_len, resp_ref, resp_max, timeout_ms) -> int

    // Binary library call (xblib_*) — phase-1: (BUF, INT) -> INT only.
    BLIB_CALL:               370, // (name_const, buf_ref, len) -> int`);

// ─────────────────────────────────────────────────────────────────────
// 2. BUILTINS table: register bcall.
//    arg 0 = name literal           → constArgs[0]
//    arg 1 = char[] runtime buffer  → strArgs[1]
//    arg 2 = int len                → no special handling
// ─────────────────────────────────────────────────────────────────────
patch('BUILTINS: register bcall',
`    'tcpTransact':          { syscall: Syscall.TCP_TRANSACT,          args: 5, returns: true,
                              strArgs: [0, 2] },`,
`    'tcpTransact':          { syscall: Syscall.TCP_TRANSACT,          args: 5, returns: true,
                              strArgs: [0, 2] },

    // Binary library invocation. constArgs[0] requires the name to be a
    // string literal at the call site; it gets pushed as a const-pool
    // index that the firmware resolves to the registered fn pointer.
    'bcall':                { syscall: Syscall.BLIB_CALL,             args: 3, returns: true,
                              constArgs: [0], strArgs: [1] },`);

// ─────────────────────────────────────────────────────────────────────
// 3. Simulator stub: standalone IDE runs (without the device) should
//    return a stub value rather than crash. Sit between the closing
//    } of TCP_TRANSACT and the next case. Anchor on a unique adjacent
//    line — TCP_TRANSACT's `this.push(-2);  break;` ends with `}` then
//    a fresh `case Syscall.X` follows; we insert before the next case.
// ─────────────────────────────────────────────────────────────────────
patch('Simulator: bcall stub returning -1',
`            case Syscall.TCP_TRANSACT: { // tcpTransact(req,req_len,resp,resp_max,timeout) -> int
                const timeout_ms = this.pop();
                const resp_max   = this.pop();
                const resp_ref   = this.pop();
                const req_len    = this.pop();
                const req_ref    = this.pop();
                this.onOutput(\`[TCP] tcpTransact(req_len=\${req_len}, resp_max=\${resp_max}, timeout=\${timeout_ms}ms) — simulator stub, returning -2 (not connected)\\n\`);
                this.push(-2);
                break;
            }`,
`            case Syscall.TCP_TRANSACT: { // tcpTransact(req,req_len,resp,resp_max,timeout) -> int
                const timeout_ms = this.pop();
                const resp_max   = this.pop();
                const resp_ref   = this.pop();
                const req_len    = this.pop();
                const req_ref    = this.pop();
                this.onOutput(\`[TCP] tcpTransact(req_len=\${req_len}, resp_max=\${resp_max}, timeout=\${timeout_ms}ms) — simulator stub, returning -2 (not connected)\\n\`);
                this.push(-2);
                break;
            }
            case Syscall.BLIB_CALL: { // bcall("name", buf, len) -> int
                const len      = this.pop();
                const buf_ref  = this.pop();
                const name_ci  = this.pop();
                const name     = (this.consts && this.consts[name_ci]) || '?';
                this.onOutput(\`[BLIB] bcall("\${name}", buf, len=\${len}) — simulator stub, returning -1 (no registry on host)\\n\`);
                this.push(-1);
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
