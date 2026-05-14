# TinyC IDE source

Canonical sources for the browser-based TinyC IDE that's served from the
device at `http://<tasmota>/ide`. The IDE bundles into a single
`tinyc_ide.html.gz` (~163-167 KB compressed, ~740-825 KB unzipped) that
sits at `../tinyc_ide.html.gz` (one level up — alongside the rest of the
TinyC tree at `tasmota/tinyc/`).

## Layout

```
idesrc/
├── README.md            ← this file
├── bundle.py            ← inline modules + examples → tinyc_ide.html(.gz)
├── index.html           ← HTML/CSS/inline JS template (UI shell + glue)
└── src/                 ← compiler + VM as ES modules
    ├── preprocessor.js  ← #include / #define / #ifdef resolution
    ├── opcodes.js       ← Op + Syscall enums; CALLBACK_NAMES; BUILTINS
    ├── lexer.js
    ├── parser.js
    ├── codegen.js       ← AST → bytecode; this is where new syscalls
    │                       get registered as IDE-side BUILTINS entries
    ├── vm.js            ← in-browser simulator (for `Run in Browser`)
    └── compiler.js      ← top-level entry point glueing the above
```

## Build

```bash
cd tasmota/tinyc/idesrc
python3 bundle.py
```

Outputs `tinyc_ide.html` (debuggable) and `tinyc_ide.html.gz` (what the
device serves), and **auto-copies the .gz to `../tinyc_ide.html.gz`** so
the next `git add` picks up the new IDE for device deploys.

The bundler strips `import`/`export` statements from each ES module
file and inlines them into a single `<script>` block inside the HTML
template. Examples are read from `../examples/*.tc` and inlined as
template literals; the per-example dropdown list is hard-coded at the
top of `bundle.py` (the `EXAMPLES` array).

---

## ⚠️ IMPORTANT — drift status (2026-05-14)

The `src/*.js` files here are **stale** relative to the currently
deployed `../tinyc_ide.html.gz`. The deployed file was kept up to date
via a series of idempotent `patch_*.mjs` scripts that rewrote the
gzipped HTML directly. Re-running `bundle.py` right now would clobber
all those patches and roll the IDE back to its April-23 state.

**Drift to backport** (run `diff <(gunzip -c ../tinyc_ide.html.gz)
<(python3 bundle.py && cat tinyc_ide.html)` for the canonical list):

- Compiler / codegen additions:
  - `udp(10, mcast_ip)` IGMP-Leave (commit `68a5c5419`)
  - `shareDump()` syscall + Syscall enum entry (commit `ebb277e64`)
  - 11 `ui*` BUILTINS for the retained-mode widget API
    (`uiScreen`, `uiTheme`, `uiClearScreen`, `uiLabel`, `uiLabelSet`,
    `uiCheckbox`, `uiProgress`, `uiProgressSet`, `uiGauge`, `uiIcon`,
    `uiButton` — commit `bf0b570d1`)
  - 3 image↔cam bridge BUILTINS (`dspLoadImageFromCam`,
    `dspImgTextBurn`, `dspImageToCam` — commit `3ecd0a06b`)
  - `webRawMode` / `webRawWrite` / `webKeepAlive` BUILTINS
    (commit `a306863b8`)
  - Various IDE UX patches: visible-IP, smlRestart no-cors,
    H1 release label (1.3.11 → 1.6.5)

- HTML template (index.html) additions:
  - Multi-file upload (`multiple` attr on `#fileInput`)
  - Save .tcb button + `saveTcb()` handler
  - Target Slot dropdown (`#targetSlot`)
  - `#include "file.tc"` preprocessor directive support
    (resolved via `tcResolveIncludes()` before `preprocess()`)

Total drift: ~1935 diff lines between Apr-23 `src/` bundle output and
live `../tinyc_ide.html.gz` (today).

**Pending work:** incrementally backport the drift into `src/*.js` and
`index.html`, then re-bundle and byte-compare against the live IDE.
Until that's done, treat `../tinyc_ide.html.gz` as the source of truth
and continue patching it via `../patch_*.mjs` scripts. **Do not
re-run `bundle.py` until backports complete** — it will overwrite
the live IDE with the stale Apr-23 state.

## History

This folder used to live at `/Volumes/vp_dev/TinyC/` (an external dev
volume separate from the Tasmota repo). It moved into the Tasmota tree
on 2026-05-14 so that:

- The canonical IDE sources are version-controlled alongside the
  rest of `tasmota/tinyc/`.
- Future syscall additions can edit `src/*.js` directly and re-bundle,
  instead of accumulating in `patch_*.mjs` scripts that target the
  gzipped HTML.
- New contributors can see the full IDE source without needing access
  to the external dev volume.
