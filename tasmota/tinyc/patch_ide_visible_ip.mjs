#!/usr/bin/env node
// patch_ide_visible_ip.mjs — keep the device-IP input VISIBLE in the IDE
// even when the IDE was loaded directly from the device's IP (e.g.
// http://192.168.188.39/tinyc_ide.html). The original code hid the
// input as "redundant" once it was auto-filled from window.location
// .hostname, but in practice users want a visible confirmation of
// which device the IDE is actually talking to (otherwise diagnostics
// like "Load failed" on the SML tab have no context — they don't know
// if the wrong IP was being used).
//
// Change: replace the `display = 'none'` with a `readonly`+tooltip
// approach. The IP stays editable-looking but can't be accidentally
// overwritten, and a hover-tooltip explains it was auto-filled.
//
// Idempotent. Safe to re-run after each IDE rebuild.

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
// Keep the device-IP input visible when the IDE is served from the
// device itself. Replace `display = 'none'` with readonly + tooltip
// so the user can SEE which device the IDE thinks it's talking to,
// but can't accidentally edit it (they'd have to remove `readonly`
// via DevTools if they really wanted to point elsewhere — vanishingly
// rare on a device-served IDE).
// ─────────────────────────────────────────────────────────────────────
patch('Keep deviceIp input visible (readonly) when served-from-device',
`            if (isServedFromDevice) {
                // IDE loaded from ESP — use the device's own IP, hide the IP input (redundant)
                document.getElementById('deviceIp').value = hostIp;
                document.getElementById('deviceIp').style.display = 'none';
                try { localStorage.setItem('tinyc_device_ip', hostIp); } catch(e) {}
                document.getElementById('btnCloseIde').style.display = '';
            }`,
`            if (isServedFromDevice) {
                // IDE loaded from ESP — use the device's own IP. Keep the
                // input visible as a read-only confirmation (so failures
                // like SML "Load failed" have visible context), but mark
                // it readonly so accidental clicks don't blank the field.
                const ipEl = document.getElementById('deviceIp');
                ipEl.value = hostIp;
                ipEl.setAttribute('readonly', '');
                ipEl.title = 'Auto-erkannt aus IDE-URL (IDE liegt auf dem Gerät). Read-only.';
                ipEl.style.background = '#1a3a6a';   // subtle hint that it's locked
                try { localStorage.setItem('tinyc_device_ip', hostIp); } catch(e) {}
                document.getElementById('btnCloseIde').style.display = '';
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
