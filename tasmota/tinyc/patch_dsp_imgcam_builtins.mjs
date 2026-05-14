#!/usr/bin/env node
// patch_dsp_imgcam_builtins.mjs — re-register the missing image↔camera bridge
//
// Three SYS_DSP_* syscalls have existed in the firmware (xdrv_124_tinyc_vm.h)
// for some time but were dropped from the IDE BUILTINS table at some point
// (likely during a refactor before this script was tracked). Result:
// camera-side TinyC examples that decode JPEG → annotate → re-encode hit
// "Undefined function" at compile time, even though the firmware can run them.
//
// Mapping (firmware IDs from xdrv_124_tinyc_vm.h):
//   dspLoadImageFromCam  277  (cam_slot)                              -> int  (img_slot, -1 = err)
//   dspImgTextBurn       278  (slot, x, y, color, fieldw, align, buf) -> void (mutates image in place)
//   dspImageToCam        279  (img_slot, cam_slot, quality)           -> int  (bytes written, -1 = err)
//
// Usage:
//   img = dspLoadImageFromCam(cam_slot);          // decode JPEG → RGB565 buffer
//   dspImgTextBurn(img, 10, 10, YELLOW, 0, 0, ts);// burn pixels in (no TFT push)
//   jlen = dspImageToCam(img, out_cam, 12);       // re-encode JPEG, quality 12
//   // then camControl(11, out_cam, fh) to save
//
// Idempotent.

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

if (src.includes("'dspLoadImageFromCam':")) {
  console.log(`OK: ${target} already patched — dspLoadImageFromCam present.`);
  process.exit(0);
}

// ── Patch 1: Syscall enum entries. Anchor on existing DSP_IMG_TEXT.
// (The Syscall enum keeps numeric order, and DSP_IMG_TEXT = 268 is the
//  closest predecessor to our 277/278/279 trio that's already registered.)
const enumAnchor = `    DSP_IMG_TEXT:    268, // (slot,x,y,col,fldw,align,buf)`;
if (!src.includes(enumAnchor)) {
  // Different formatting — fall back to a more lenient anchor
  const fallback = `    DSP_IMG_TEXT:`;
  if (!src.includes(fallback)) {
    console.error('ERROR: DSP_IMG_TEXT enum anchor not found — IDE source changed.');
    process.exit(2);
  }
}
// Find the enum line, regardless of its exact comment formatting
const enumRe = /(\n\s*DSP_IMG_TEXT:\s+268,[^\n]*)/;
const enumMatch = src.match(enumRe);
if (!enumMatch) {
  console.error('ERROR: Could not locate DSP_IMG_TEXT enum entry');
  process.exit(3);
}
const enumLine = enumMatch[1];
const enumInsert = enumLine +
  `\n    DSP_LOAD_IMG_CAM: 277, // (cam_slot) -> img_slot  (-1=err)` +
  `\n    DSP_IMG_TEXT_BURN:278, // (slot,x,y,col,fldw,align,buf) -> void (mutates image)` +
  `\n    DSP_IMG_TO_CAM:   279, // (img_slot,cam_slot,quality) -> bytes  (-1=err)`;
let patched = src.replace(enumRe, enumInsert);

// ── Patch 2: BUILTINS entries. Anchor on existing dspImgText line.
const biAnchor = `    'dspImgText':       { syscall: Syscall.DSP_IMG_TEXT,   args: 7, returns: false, strArgs: [6] },`;
if (!patched.includes(biAnchor)) {
  console.error('ERROR: dspImgText BUILTIN anchor not found — IDE source changed.');
  process.exit(4);
}
const biInsert = biAnchor +
  `\n    'dspLoadImageFromCam':{ syscall: Syscall.DSP_LOAD_IMG_CAM,   args: 1, returns: true },` +
  `\n    'dspImgTextBurn':   { syscall: Syscall.DSP_IMG_TEXT_BURN,    args: 7, returns: false, strArgs: [6] },` +
  `\n    'dspImageToCam':    { syscall: Syscall.DSP_IMG_TO_CAM,       args: 3, returns: true  },`;
patched = patched.replace(biAnchor, biInsert);

if (patched === src) {
  console.error('ERROR: no changes were made.');
  process.exit(5);
}

const bakPath = target + '.bak';
if (!existsSync(bakPath)) copyFileSync(target, bakPath);

const out = gzipSync(Buffer.from(patched, 'utf8'), { level: 9 });
writeFileSync(target, out);

console.log(`OK: patched ${target}`);
console.log(`    backup: ${bakPath}`);
console.log(`    size:   ${gz.length} → ${out.length} bytes`);
console.log(`    added:  3 BUILTINS (dspLoadImageFromCam / dspImgTextBurn / dspImageToCam)`);
console.log(`            + Syscall enum entries 277..279`);
