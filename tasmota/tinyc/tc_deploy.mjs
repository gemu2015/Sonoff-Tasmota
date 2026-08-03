#!/usr/bin/env node
// =====================================================================
// tc_deploy.mjs — TinyC compile + upload + run from the command line.
//
// Reuses the compiler embedded in tinyc_ide.html.gz (no duplication),
// so it always tracks whatever ships with the firmware tree.
//
// Usage:
//   node tc_deploy.mjs <file.tc> [<device-ip> [<remote-name>]] [-DDEFINE …]
//
// Defaults:
//   device-ip    = 192.168.188.39
//   remote-name  = same basename as input, with .tcb extension
//
// Examples:
//   node tc_deploy.mjs examples/blink.tc
//   node tc_deploy.mjs examples/epd_compare_test.tc 192.168.188.39
//   node tc_deploy.mjs examples/camera.tc 192.168.188.190 -DBOARD_GOOUUU
//
// What it does:
//   1. Reads the .tc source (resolving #include files relatively)
//   2. Compiles to bytecode using the in-IDE compiler
//   3. POSTs the .tcb to http://<ip>/tc_upload?api=1&fsz=<size>&slot=<slot>
//   4. Sends `TinyCStop <slot>` then `TinyCRun <slot> /<remote-name>` via /cm
// =====================================================================

import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = path.dirname(__filename);

// --------------------------------------------------------------------
// CLI parsing
// --------------------------------------------------------------------
const args    = process.argv.slice(2);
const positional = [];
const defines   = [];
let   noUpload   = false;       // --no-upload: compile only, write .tcb, skip device push
// ⚠️ Ohne --slot geht ALLES in Slot 0. Auf einem Geraet mit mehreren belegten Slots
// (der Hausmonitor .135 faehrt vier) wirft das den dort laufenden Slot 0 hinaus, ohne
// zu fragen — und der Upload legt die Datei zusaetzlich in Slot 0 ab (?slot=N am
// Endpunkt, Vorgabe 0). Deshalb muss der Slot mitgegeben werden koennen.
let   slot       = 0;
for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a.startsWith('-D')) defines.push(a.slice(2));
    else if (a === '--no-upload' || a === '-n') noUpload = true;
    else if (a === '--slot' || a === '-s') { slot = parseInt(args[++i], 10); }
    else if (a.startsWith('--slot=')) { slot = parseInt(a.slice(7), 10); }
    else positional.push(a);
}
if (!(slot >= 0 && slot < 6)) {
    console.error(`Bad --slot ${slot}: must be 0..5`);
    process.exit(2);
}
if (positional.length < 1) {
    console.error('Usage: tc_deploy.mjs <file.tc> [<device-ip> [<remote-name>]] [-DDEFINE …] [--slot N] [--no-upload|-n]');
    console.error('       --slot N      target VM slot 0..5 (default 0) — REQUIRED on devices');
    console.error('                     that run several slots, or slot 0 is overwritten');
    console.error('       --no-upload   compile only, write .tcb to bytecode/, skip device push');
    process.exit(2);
}
const srcPath   = path.resolve(positional[0]);
const deviceIp  = positional[1] || '192.168.188.39';
const baseName  = path.basename(srcPath).replace(/\.tc$/i, '');
const remoteName = positional[2] || (baseName + '.tcb');

// --------------------------------------------------------------------
// Load compiler from tinyc_ide.html.gz
// --------------------------------------------------------------------
const idePath = path.join(__dirname, 'tinyc_ide.html.gz');
if (!fs.existsSync(idePath)) {
    console.error(`Cannot find ${idePath}`);
    process.exit(2);
}
const html = zlib.gunzipSync(fs.readFileSync(idePath)).toString('utf8');

// Extract the single <script> block, then keep only the lines from
// "// --- preprocessor.js ---" up to and including compileAndRun(). The
// rest of the script depends on the DOM and is not needed for compilation.
const scriptStart = html.indexOf('<script>');
const scriptEnd   = html.indexOf('</script>', scriptStart);
if (scriptStart < 0 || scriptEnd < 0) {
    console.error('Could not locate <script> block in tinyc_ide.html');
    process.exit(2);
}
const fullJs = html.slice(scriptStart + '<script>'.length, scriptEnd);

const preStart = fullJs.indexOf('// --- preprocessor.js ---');
const compEndMarker = 'function compileAndRun(';
const compEndIdx = fullJs.indexOf(compEndMarker, preStart);
// keep up through the closing brace of compileAndRun (next "}\n")
const compEndCloseIdx = fullJs.indexOf('\n}\n', compEndIdx);
if (preStart < 0 || compEndIdx < 0 || compEndCloseIdx < 0) {
    console.error('Could not slice compiler section from IDE script');
    process.exit(2);
}
const compilerJs = fullJs.slice(preStart, compEndCloseIdx + 3);

// --------------------------------------------------------------------
// Run compiler in a fresh sandbox
// --------------------------------------------------------------------
const sandbox = {
    console,
    Uint8Array, Uint16Array, Uint32Array, Int8Array, Int16Array, Int32Array,
    Float32Array, Float64Array, ArrayBuffer, DataView,
    Math, Number, String, Array, Object, JSON, Set, Map,
    TextEncoder, TextDecoder,
    Buffer,
};
sandbox.global = sandbox;
vm.createContext(sandbox);
vm.runInContext(compilerJs, sandbox, { filename: 'tinyc_compiler.js' });

const compileFn = sandbox.compile;
if (typeof compileFn !== 'function') {
    console.error('compile() not found after evaluating compiler script');
    process.exit(2);
}

// --------------------------------------------------------------------
// Read source (with #include resolution relative to the source file)
// --------------------------------------------------------------------
const source = fs.readFileSync(srcPath, 'utf8');
const srcDir = path.dirname(srcPath);
let resolved = source;
const resolveIncludes = sandbox.resolveIncludes;
if (typeof resolveIncludes === 'function') {
    const getFile = (name) => {
        const p = path.isAbsolute(name) ? name : path.join(srcDir, name);
        return fs.readFileSync(p, 'utf8');
    };
    try {
        resolved = resolveIncludes(source, getFile);
    } catch (e) {
        console.error('Include resolution failed:', e.message || e);
        process.exit(1);
    }
}

// --------------------------------------------------------------------
// Compile
// --------------------------------------------------------------------
const t0 = Date.now();
let compiled;
try {
    compiled = compileFn(resolved, { defines });
} catch (e) {
    if (e && e.phase) {
        console.error(`Compile error [${e.phase}] line ${e.line || '?'}: ${e.originalError?.message || e.message}`);
    } else {
        console.error('Compile error:', e?.message || e);
    }
    process.exit(1);
}
const elapsed = Date.now() - t0;
const binary  = Buffer.from(compiled.binary);
console.log(`Compiled ${path.basename(srcPath)} → ${binary.length} bytes (code ${compiled.codeSize}, header ${compiled.codeOffset}) in ${elapsed} ms`);
if (compiled.warnings?.length) {
    for (const w of compiled.warnings) console.warn('  warn:', w);
}

// Drop a copy into the tinyc/bytecode/ directory for inspection.
// Historical behaviour was to drop it next to the .tc source, which led
// to ~100 stale .tcb artefacts littering tasmota/tinyc/examples/ over
// time. The canonical home for inspectable bytecode is tinyc/bytecode/
// (which is also where the curated, in-tree .tcb files live for the
// 67-odd reference examples). Falls back to next-to-source if the
// bytecode/ dir is missing (e.g. tc_deploy was vendored to a different
// tree without it).
const bytecodeDir = path.join(__dirname, 'bytecode');
const localTcb = fs.existsSync(bytecodeDir)
    ? path.join(bytecodeDir, baseName + '.tcb')
    : path.join(srcDir, baseName + '.tcb');
fs.writeFileSync(localTcb, binary);
console.log(`  wrote ${localTcb}`);

// --------------------------------------------------------------------
// Upload to device
// --------------------------------------------------------------------
async function uploadAndRun() {
    // ⚠️ slot=N muss MIT: der Upload-Endpunkt legt die Datei sonst in Slot 0 ab
    // (xdrv_124_tinyc.ino, "Determine target slot from ?slot=N parameter (default 0)").
    const url = `http://${deviceIp}/tc_upload?api=1&fsz=${binary.length}&slot=${slot}`;
    console.log(`Uploading to ${url} as ${remoteName} …`);

    // multipart/form-data, hand-built to avoid extra deps
    const boundary = '----TinyCDeploy' + Date.now().toString(16);
    const head = Buffer.from(
        `--${boundary}\r\n` +
        `Content-Disposition: form-data; name="file"; filename="${remoteName}"\r\n` +
        `Content-Type: application/octet-stream\r\n\r\n`,
        'utf8'
    );
    const tail = Buffer.from(`\r\n--${boundary}--\r\n`, 'utf8');
    const body = Buffer.concat([head, binary, tail]);

    const resp = await fetch(url, {
        method : 'POST',
        body,
        headers: { 'Content-Type': `multipart/form-data; boundary=${boundary}` },
    });
    if (!resp.ok) {
        console.error(`Upload failed: HTTP ${resp.status} ${resp.statusText}`);
        process.exit(1);
    }
    const j = await resp.json().catch(() => ({}));
    console.log(`  device stored ${j.size ?? binary.length} bytes`,
                j.file ? `at ${j.file}` : '');

    // --------------------------------------------------------------------
    // Stop any running slot, then run the freshly uploaded file
    // --------------------------------------------------------------------
    async function cmnd(c) {
        const u = `http://${deviceIp}/cm?cmnd=${encodeURIComponent(c)}`;
        const r = await fetch(u);
        const t = await r.text();
        return t;
    }

    console.log(`  TinyCStop ${slot} …`);
    await cmnd(`TinyCStop ${slot}`).catch(() => {});
    // small grace
    await new Promise(r => setTimeout(r, 200));

    const runArg = '/' + remoteName.replace(/^\/+/, '');
    console.log(`  TinyCRun ${slot} ${runArg} …`);
    const out = await cmnd(`TinyCRun ${slot} ${runArg}`);
    console.log('  device:', out.trim());

    console.log('Done.');
}

if (noUpload) {
    // --no-upload: compile-only path used by tc_compile_all.mjs and any
    // user wanting to refresh bytecode/ without touching a device.
    console.log('--no-upload: skipping device push.');
} else {
    uploadAndRun().catch((e) => {
        console.error('Deploy error:', e?.message || e);
        process.exit(1);
    });
}
