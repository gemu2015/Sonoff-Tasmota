#!/usr/bin/env node
// patch_ide_smlrestart_nocors.mjs — fix "Error: Load failed" on the SML
// Restart button in Safari when the IDE is loaded from cross-origin
// (e.g. the :82 background-download server redirects from /ide → :82/ide,
// then the IDE's `fetch("http://${ip}:80/cm?cmnd=sensor53+r", { mode:
// 'cors' })` is cross-origin to :80 — and /cm in Tasmota core has no
// Access-Control-Allow-Origin header, so Safari throws TypeError: Load
// failed in the fetch promise).
//
// Workaround: smlRestart is fire-and-forget — the IDE never reads the
// JSON body, it only needs the GET to reach Tasmota and trigger the
// reload. Switch the fetch to `mode: 'no-cors'` so Safari accepts the
// opaque response without crying about missing CORS headers.
//
// Side effect: we can't tell whether the command actually succeeded
// (response is opaque). Best we can say is "request sent" — if the
// fetch promise resolves at all, Tasmota received and executed the
// command (HTTP status itself is hidden, but a network reachability
// error still throws to the catch branch).
//
// This is purely an IDE-side fix; no Tasmota core change needed. The
// alternative would be adding `Access-Control-Allow-Origin: *` to /cm
// in xdrv_01_9_webserver.ino which Hans explicitly wanted to avoid.
//
// Idempotent. Re-run safely after each IDE rebuild that re-emits the
// original smlRestart shape.

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
// smlRestart() — switch fetch mode to 'no-cors' and adjust the success
// reporting (opaque responses have resp.ok = false even on a successful
// HTTP 200, so the `if (resp.ok)` check is meaningless — treat
// fetch-didn't-throw as success).
// ─────────────────────────────────────────────────────────────────────
patch('smlRestart: switch to mode no-cors + accept opaque response',
`        async function smlRestart() {
            const ip = getDeviceIp();
            if (!ip) { smlSetStatus('No device IP', 'error'); return; }
            smlSetStatus('Restarting SML...');
            try {
                const resp = await fetch(\`http://\${ip}/cm?cmnd=sensor53+r\`, { mode: 'cors' });
                if (resp.ok) {
                    smlSetStatus('SML restarted', 'success');
                } else {
                    smlSetStatus(\`Restart failed: HTTP \${resp.status}\`, 'error');
                }
            } catch (e) {
                smlSetStatus(\`Error: \${e.message}\`, 'error');
            }
        }`,
`        async function smlRestart() {
            const ip = getDeviceIp();
            if (!ip) { smlSetStatus('No device IP', 'error'); return; }
            smlSetStatus('Restarting SML...');
            // mode: 'no-cors' so Safari accepts the opaque response when the
            // IDE was loaded cross-origin (e.g. served from :82 vs. /cm on
            // :80). The fire-and-forget GET still reaches Tasmota and runs
            // the command; we just can't read the JSON ack. If the request
            // makes it through the network stack, treat as success — only a
            // genuine network error (unreachable / DNS / etc.) throws to the
            // catch branch.
            try {
                await fetch(\`http://\${ip}/cm?cmnd=sensor53+r\`, { mode: 'no-cors' });
                smlSetStatus('SML restart sent', 'success');
            } catch (e) {
                smlSetStatus(\`Error: \${e.message}\`, 'error');
            }
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
