#!/usr/bin/env node
// patch_tinyui_builtins.mjs — TinyC IDE: add the 11 missing ui* BUILTINS
//
// The firmware (xdrv_124_tinyc_vm.h) ships SYS_UI_* syscalls 310..327 for
// the retained-mode display-widget layer (screen + theme + label + progress
// + gauge + checkbox + pushbutton + icon). The handlers are implemented at
// VM level and dispatch to the existing Renderer plus the VButton touch
// pool, but the IDE's BUILTINS map never registered the ui* identifiers,
// so any script using them gets "Undefined function: uiLabel" / uiTheme
// etc. at compile time.
//
// This patch fixes the gap by adding the 11 BUILTINS entries + the
// matching Syscall enum entries. No firmware change needed — only the
// IDE's compile-side table.
//
// Mapping (matches xdrv_124_tinyc_vm.h SYS_UI_* values):
//   uiScreen        310  (id)                              -> void
//   uiTheme         311  (bg, accent, text, border)        -> void
//   uiClearScreen   312  ()                                -> void
//   uiLabel         320  (num,x,y,w,h,text,align)          -> void  [text: literal]
//   uiLabelSet      321  (num, text)                       -> void  [text: char[] or literal]
//   uiCheckbox      322  (num,x,y,w,h,label)               -> void  [label: literal]
//   uiProgress      323  (num,x,y,w,h,value,max)           -> void
//   uiProgressSet   324  (num, value)                      -> void
//   uiGauge         325  (num,x,y,r,value,vmin,vmax)       -> void
//   uiIcon          326  (num,x,y,img_slot)                -> void
//   uiButton        327  (num,x,y,w,h,label)               -> void  [label: literal]
//
// Idempotent. Usage: node patch_tinyui_builtins.mjs

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

if (src.includes("'uiScreen':")) {
  console.log(`OK: ${target} already patched — uiScreen present.`);
  process.exit(0);
}

// ── Patch 1: register UI_* Syscall enum entries.
// Anchor on SHARE_DUMP: 352 (the most recent share entry), which sits in
// the same number-space neighborhood. Insert the UI block right after it.
const syscallAnchor = `    SHARE_DUMP:     352, // ()                       -> int  number of live entries`;
const syscallWithUi = `    SHARE_DUMP:     352, // ()                       -> int  number of live entries
    UI_SCREEN:      310, // (id)                                       -> void
    UI_THEME:       311, // (bg, accent, text, border)                 -> void
    UI_CLEAR_SCREEN:312, // ()                                          -> void
    UI_LABEL:       320, // (num,x,y,w,h,text_const,align)             -> void
    UI_LABEL_SET:   321, // (num, text_ref_or_const)                   -> void
    UI_CHECKBOX:    322, // (num,x,y,w,h,label_const)                  -> void
    UI_PROGRESS:    323, // (num,x,y,w,h,value,max)                    -> void
    UI_PROGRESS_SET:324, // (num, value)                               -> void
    UI_GAUGE:       325, // (num,x,y,r,value,vmin,vmax)                -> void
    UI_ICON:        326, // (num,x,y,img_slot)                         -> void
    UI_BUTTON:      327, // (num,x,y,w,h,label_const)                  -> void`;

if (!src.includes(syscallAnchor)) {
  console.error('ERROR: SHARE_DUMP enum anchor not found — IDE source changed.');
  process.exit(2);
}
let patched = src.replace(syscallAnchor, syscallWithUi);

// ── Patch 2: register the 11 ui* BUILTINS.
// Anchor on shareDump BUILTIN (the latest entry I added in commit
// ebb277e64). The UI builtins go right after it.
const builtinsAnchor = `    'shareDump':        { syscall: Syscall.SHARE_DUMP,      args: 0, returns: true },`;
const builtinsWithUi = `    'shareDump':        { syscall: Syscall.SHARE_DUMP,      args: 0, returns: true },
    'uiScreen':         { syscall: Syscall.UI_SCREEN,       args: 1, returns: false },
    'uiTheme':          { syscall: Syscall.UI_THEME,        args: 4, returns: false },
    'uiClearScreen':    { syscall: Syscall.UI_CLEAR_SCREEN, args: 0, returns: false },
    'uiLabel':          { syscall: Syscall.UI_LABEL,        args: 7, returns: false, constArgs: [5] },
    'uiLabelSet':       { syscall: Syscall.UI_LABEL_SET,    args: 2, returns: false, strArgs: [1] },
    'uiCheckbox':       { syscall: Syscall.UI_CHECKBOX,     args: 6, returns: false, constArgs: [5] },
    'uiProgress':       { syscall: Syscall.UI_PROGRESS,     args: 7, returns: false },
    'uiProgressSet':    { syscall: Syscall.UI_PROGRESS_SET, args: 2, returns: false },
    'uiGauge':          { syscall: Syscall.UI_GAUGE,        args: 7, returns: false },
    'uiIcon':           { syscall: Syscall.UI_ICON,         args: 4, returns: false },
    'uiButton':         { syscall: Syscall.UI_BUTTON,       args: 6, returns: false, constArgs: [5] },`;

if (!patched.includes(builtinsAnchor)) {
  console.error('ERROR: shareDump BUILTINS anchor not found — IDE source changed.');
  process.exit(3);
}
patched = patched.replace(builtinsAnchor, builtinsWithUi);

// Backup + write
const bakPath = target + '.bak';
if (!existsSync(bakPath)) copyFileSync(target, bakPath);

const out = gzipSync(Buffer.from(patched, 'utf8'), { level: 9 });
writeFileSync(target, out);

console.log(`OK: patched ${target}`);
console.log(`    backup: ${bakPath}`);
console.log(`    size:   ${gz.length} → ${out.length} bytes`);
console.log(`    added:  11 ui* BUILTINS (uiScreen/uiTheme/uiClearScreen/uiLabel/uiLabelSet/`);
console.log(`            uiCheckbox/uiProgress/uiProgressSet/uiGauge/uiIcon/uiButton)`);
console.log(`            + Syscall enum entries UI_SCREEN..UI_BUTTON (310..327)`);
