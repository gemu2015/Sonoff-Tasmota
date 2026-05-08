#!/usr/bin/env python3
"""
build_plugin.py — Tasmota BinPlugin build wrapper.

Picks one ``USE_X_MOD`` plugin gate, picks the right build env for a CPU
family (ESP8266 / ESP32 Tensilica / ESP32 RISC-V), rewrites
``tasmota/user_config_override.h`` to enable only that plugin, runs the
matching ``pio run -e <env>``, locates the generated plugin ``.bin``
under ``build_output/firmware/Plugins/`` and (by default) restores the
override file when done.

USAGE — CLI:

    python3 tasmota/Plugins/build_plugin.py --list
    python3 tasmota/Plugins/build_plugin.py --plugin USE_I2S_MOD --cpu esp32
    python3 tasmota/Plugins/build_plugin.py --plugin USE_DS18X20_MOD --cpu esp32_riscv --keep

USAGE — GUI (Tkinter, ships with Python on macOS / Linux / Windows):

    python3 tasmota/Plugins/build_plugin.py --gui

The GUI shows the discovered plugin list, three CPU radio buttons, a
"keep override" checkbox and a live pio output panel. On success it
prints the plugin .bin path so you know exactly what to upload.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# --------------------------------------------------------------------
# Locate the repository
# --------------------------------------------------------------------
HERE     = Path(__file__).resolve().parent          # tasmota/Plugins/
REPO     = HERE.parent.parent                        # <repo>/
OVERRIDE = REPO / 'tasmota' / 'user_config_override.h'

# --------------------------------------------------------------------
# CPU family → representative build env. Three entries — one per
# binplugin output directory under build_output/firmware/Plugins/.
# Tensilica covers original ESP32 + S2 + S3 (same Xtensa toolchain,
# same plugin .bin format). RISC-V covers C3 + C6 + future Cx parts.
# --------------------------------------------------------------------
CPU_ENVS = {
    'esp8266':     ('tasmota-4M',     'ESP8266'),
    'esp32':       ('tasmota32-4M',   'ESP32 Tensilica (orig / S2 / S3)'),
    'esp32_riscv': ('tasmota32c3-4M', 'ESP32 RISC-V (C3 / C6 / …)'),
}

# --------------------------------------------------------------------
# Discover available plugins by scanning *.cpp in this folder for the
# `#ifdef USE_..._MOD` (or `#if defined(USE_..._MOD)`) gate at the top
# of the file. Each plugin file is gated by exactly one such symbol —
# we pick the first one we find within the first ~200 lines.
# --------------------------------------------------------------------
_GATE_RE = re.compile(
    r'^\s*#\s*if(?:def)?\s+(?:defined\s*\(\s*)?(USE_[A-Z0-9_]+_MOD)\b'
)

def discover_plugins():
    """Return [(USE_X_MOD, filename), …] sorted by USE_X_MOD."""
    out = []
    for cpp in sorted(HERE.glob('*.cpp')):
        try:
            with cpp.open() as f:
                for n, line in enumerate(f):
                    if n > 200:
                        break
                    m = _GATE_RE.match(line)
                    if m:
                        out.append((m.group(1), cpp.name))
                        break
        except OSError:
            continue
    return out

# --------------------------------------------------------------------
# Rewrite user_config_override.h. The plugin-build workflow uses two
# layered toggles:
#
#   1. Top-level `#define device_X` — only one device_* selector is
#      active at a time. Plugin compiles use `device_lcd` (it carries
#      a `#undef USE_HOMEKIT` plus the right SCRIPT/TINYC/SML setup
#      for plugin work). Without this, the firmware build flags from
#      e.g. `device_devkit` would pull in HomeKit and other features
#      the plugin slots can't accommodate.
#
#   2. Inside the `#ifdef device_lcd … #endif` block, individual
#      USE_..._MOD lines pick which plugin .cpp gets compiled into
#      the resulting .bin. Only one of these may be active at a time.
#
# This function does both: comments every active `#define device_*`
# and uncomments `device_lcd`; then walks the device_lcd block and
# applies the same single-target rule to USE_..._MOD lines. Symbols
# outside the device_lcd block (and non-USE_*_MOD `#define`s anywhere)
# are left untouched.
#
# Returns the ORIGINAL text so the caller can restore it afterwards.
# --------------------------------------------------------------------
_DEVICE_ACTIVE_RE    = re.compile(r'^(\s*)#define\s+(device_[A-Za-z0-9_]+)\b(.*)$')
_DEVICE_COMMENTED_RE = re.compile(r'^(\s*)//\s*#define\s+(device_[A-Za-z0-9_]+)\b(.*)$')
_USE_ACTIVE_RE       = re.compile(r'^(\s*)#define\s+(USE_[A-Z0-9_]+_MOD)\b(.*)$')
_USE_COMMENTED_RE    = re.compile(r'^(\s*)//\s*#define\s+(USE_[A-Z0-9_]+_MOD)\b(.*)$')
# Recognise opening / closing preprocessor directives for nesting depth.
_PP_IF_RE            = re.compile(r'^\s*#\s*(if(?:def|ndef)?|elif)\b')
_PP_ENDIF_RE         = re.compile(r'^\s*#\s*endif\b')
_DEVICE_LCD_OPEN_RE  = re.compile(r'^\s*#\s*ifdef\s+device_lcd\b')

DEVICE_TARGET = 'device_lcd'

def rewrite_override(target_use):
    """Edit OVERRIDE in-place; return the prior text for restore."""
    if not OVERRIDE.exists():
        raise FileNotFoundError(OVERRIDE)
    original = OVERRIDE.read_text()

    out_lines = []
    device_target_seen = False
    use_target_seen = False
    in_device_lcd = False    # are we inside the device_lcd block right now?
    depth = 0                # nesting depth from the device_lcd open

    for line in original.splitlines(keepends=True):
        # ── First, track the device_lcd block boundary ──────────────
        if not in_device_lcd:
            if _DEVICE_LCD_OPEN_RE.match(line):
                in_device_lcd = True
                depth = 1
                # Fall through to top-level processing for the line itself
        else:
            # Inside device_lcd — track nested #ifdef / #endif so we
            # know when the block closes. We also process USE_X_MOD
            # toggles *inside* this block.
            if _PP_IF_RE.match(line):
                depth += 1
            elif _PP_ENDIF_RE.match(line):
                depth -= 1
                if depth == 0:
                    out_lines.append(line)        # the closing #endif itself
                    in_device_lcd = False
                    continue

        # ── Top-level: switch device_* selectors ───────────────────
        if not in_device_lcd or depth == 1:  # depth 1 = the device_lcd body's #ifdef line
            m = _DEVICE_ACTIVE_RE.match(line)
            if m:
                ind, name, tail = m.groups()
                if name == DEVICE_TARGET:
                    out_lines.append(line)
                    device_target_seen = True
                else:
                    out_lines.append(f'{ind}//#define {name}{tail}\n')
                continue
            m = _DEVICE_COMMENTED_RE.match(line)
            if m:
                ind, name, tail = m.groups()
                if name == DEVICE_TARGET:
                    out_lines.append(f'{ind}#define {name}{tail}\n')
                    device_target_seen = True
                    continue
                # else: leave commented as-is

        # ── Inside device_lcd block: switch USE_X_MOD lines ────────
        if in_device_lcd:
            m = _USE_ACTIVE_RE.match(line)
            if m:
                ind, name, tail = m.groups()
                if name == target_use:
                    out_lines.append(line)
                    use_target_seen = True
                else:
                    out_lines.append(f'{ind}//#define {name}{tail}\n')
                continue
            m = _USE_COMMENTED_RE.match(line)
            if m:
                ind, name, tail = m.groups()
                if name == target_use:
                    out_lines.append(f'{ind}#define {name}{tail}\n')
                    use_target_seen = True
                    continue

        out_lines.append(line)

    # Sanity: bail loudly if the rewrite couldn't find what it needed.
    if not device_target_seen:
        raise RuntimeError(
            f'Could not find a #define {DEVICE_TARGET} (active or commented) '
            f'at top level of {OVERRIDE.name}. Add one near the other '
            f'#define device_* selectors (around line 120).'
        )
    if not use_target_seen:
        raise RuntimeError(
            f'Could not find a #define {target_use} (active or commented) '
            f'inside the #ifdef {DEVICE_TARGET} block. Add it inside that '
            f'block first, or pick a different plugin from --list.'
        )

    OVERRIDE.write_text(''.join(out_lines))
    return original

def restore_override(original):
    OVERRIDE.write_text(original)

# --------------------------------------------------------------------
# pio invocation. Streams stdout/stderr line-by-line via the on_line
# callback so the GUI can mirror it into a scroll-window. CLI uses
# print() as the callback for plain console streaming.
# --------------------------------------------------------------------
def _pio_path():
    cands = [
        shutil.which('pio'),
        os.path.expanduser('~/.platformio/penv/bin/pio'),
        '/usr/local/bin/pio',
    ]
    for c in cands:
        if c and os.path.exists(c):
            return c
    raise FileNotFoundError('pio executable not found — is PlatformIO installed?')

def run_build(env_name, on_line=print):
    pio = _pio_path()
    cmd = [pio, 'run', '-e', env_name]
    on_line(f'>> {" ".join(cmd)}')
    proc = subprocess.Popen(
        cmd,
        cwd=str(REPO),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert proc.stdout is not None
    for line in proc.stdout:
        on_line(line.rstrip('\n'))
    return proc.wait()

# --------------------------------------------------------------------
# Locate the most recently modified plugin .bin produced by
# grepmodule-firmware.py. The script writes it to
# `build_output/firmware/<MODULE_NAME>.bin` (e.g. I2SAUDIO_32.bin) —
# NOT into the curated `Plugins/<arch>/` subdirectory, which holds
# older saved versions for distribution. To distinguish a plugin .bin
# from the env's own firmware .bin (e.g. tasmota32-4M.bin), we exclude
# files whose stem matches a known build-env name — env names contain
# a hyphen (`-`), plugin module names don't.
# --------------------------------------------------------------------
def find_output_bin(newer_than=None):
    """Most recently modified plugin .bin in build_output/firmware/.
    If `newer_than` is supplied (mtime as float seconds), only bins
    with mtime strictly greater are considered — useful in multi-CPU
    runs to identify the bin produced by THIS build rather than a
    leftover from a prior one. Returns None if nothing matches."""
    base = REPO / 'build_output' / 'firmware'
    if not base.exists():
        return None
    candidates = []
    for p in base.glob('*.bin'):
        stem = p.stem
        # Skip firmware .bin / .factory.bin (env names have hyphens; plugin
        # module names are uppercase with optional _32 / _32r suffix).
        if '-' in stem or '.factory' in p.name:
            continue
        if p.name.endswith('.bin.gz'):
            continue
        if newer_than is not None and p.stat().st_mtime <= newer_than:
            continue
        candidates.append(p)
    if not candidates:
        return None
    return max(candidates, key=lambda p: p.stat().st_mtime)


def _max_bin_mtime():
    """Highest mtime among existing plugin-shaped .bin files. Captured
    before each per-CPU build so find_output_bin(newer_than=…) can
    filter to only the freshly-produced bin."""
    base = REPO / 'build_output' / 'firmware'
    if not base.exists():
        return 0.0
    times = [
        p.stat().st_mtime
        for p in base.glob('*.bin')
        if '-' not in p.stem and '.factory' not in p.name and not p.name.endswith('.bin.gz')
    ]
    return max(times) if times else 0.0

# --------------------------------------------------------------------
# CLI flow. `cpus` is a list — each entry maps to one env and produces
# one plugin .bin. The override file is rewritten once before the loop
# (USE_X_MOD selection is CPU-agnostic) and restored once afterward.
# Each CPU is built sequentially; failures are logged but don't abort
# the remaining targets, so picking all three and getting partial
# results is normal during cross-compile work.
# --------------------------------------------------------------------
def cli(plugin, cpus, keep, on_line=print):
    plugins = dict(discover_plugins())
    if plugin not in plugins:
        on_line(f'!! Unknown plugin: {plugin}')
        on_line('Available:')
        for name, fname in discover_plugins():
            on_line(f'   {name:<28s} ({fname})')
        return 2

    if isinstance(cpus, str):
        cpus = [cpus]
    if not cpus:
        on_line('!! No CPU targets selected.')
        return 2

    unknown = [c for c in cpus if c not in CPU_ENVS]
    if unknown:
        on_line(f'!! Unknown CPU(s): {", ".join(unknown)}. Available: {", ".join(CPU_ENVS)}')
        return 2

    on_line(f'== Plugin: {plugin} ({plugins[plugin]}) ==')
    on_line(f'== Targets: {", ".join(cpus)} ==')

    original = rewrite_override(plugin)
    results = []   # list of (cpu, rc, output_path_or_None)
    overall_rc = 0

    try:
        for cpu in cpus:
            env, label = CPU_ENVS[cpu]
            on_line('')
            on_line(f'── [{cpu}] {label} via env "{env}" ──')
            mtime_before = _max_bin_mtime()
            rc = run_build(env, on_line=on_line)
            if rc != 0:
                on_line(f'!! [{cpu}] Build failed (rc={rc}) — continuing with remaining targets')
                results.append((cpu, rc, None))
                overall_rc = rc
                continue
            out = find_output_bin(newer_than=mtime_before)
            if out:
                try:
                    rel = out.relative_to(REPO)
                except ValueError:
                    rel = out
                on_line(f'   [{cpu}] Output: {rel} ({out.stat().st_size:,} bytes)')
                results.append((cpu, 0, out))
            else:
                on_line(f'!! [{cpu}] Build succeeded but no fresh plugin .bin found')
                results.append((cpu, 0, None))

        # Summary line so the user knows what landed without scrolling.
        on_line('')
        on_line('== Summary ==')
        for cpu, rc, out in results:
            tag = 'OK' if rc == 0 and out else ('OK (no bin?)' if rc == 0 else f'FAIL rc={rc}')
            extra = f'  {out.name}' if out else ''
            on_line(f'   {cpu:<14s} {tag}{extra}')
        return overall_rc
    finally:
        if keep:
            on_line('== Left user_config_override.h with chosen plugin enabled (--keep) ==')
        else:
            restore_override(original)
            on_line('== Restored user_config_override.h ==')

# --------------------------------------------------------------------
# GUI flow — tkinter. Same logic, two dropdowns, a checkbox, a button,
# a live log window. Built-in to Python on macOS / Linux / Windows.
# --------------------------------------------------------------------
def gui():
    import threading
    import tkinter as tk
    from tkinter import ttk, scrolledtext

    plugins = discover_plugins()
    if not plugins:
        raise RuntimeError(
            f'No plugin .cpp files found under {HERE} (looking for #ifdef USE_..._MOD).'
        )

    root = tk.Tk()
    root.title('Tasmota BinPlugin builder')
    root.geometry('780x560')

    main = ttk.Frame(root, padding=10)
    main.pack(fill=tk.BOTH, expand=True)

    # ── Plugin dropdown ────────────────────────────────────────────
    ttk.Label(main, text='Plugin:').grid(row=0, column=0, sticky='w')
    plug_var = tk.StringVar(value=plugins[0][0])
    plug_choices = [f'{name}   ({fname})' for name, fname in plugins]
    plug_box = ttk.Combobox(
        main, textvariable=plug_var, values=plug_choices, width=58, state='readonly'
    )
    plug_box.current(0)
    plug_box.grid(row=0, column=1, sticky='ew', pady=4)

    # ── CPU checkboxes (multi-select) ─────────────────────────────
    # Each ticked CPU is built sequentially using the same override
    # rewrite. Default to esp32 only — users typically pick more when
    # cross-compiling for distribution; single-target is the common case.
    ttk.Label(main, text='CPUs:').grid(row=1, column=0, sticky='nw')
    cpu_frame = ttk.Frame(main)
    cpu_frame.grid(row=1, column=1, sticky='w')
    cpu_vars = {}
    for key, (env, label) in CPU_ENVS.items():
        var = tk.IntVar(value=1 if key == 'esp32' else 0)
        cpu_vars[key] = var
        ttk.Checkbutton(
            cpu_frame, text=f'{label}    ({env})', variable=var
        ).pack(anchor='w')

    # ── Keep override checkbox ────────────────────────────────────
    keep_var = tk.IntVar(value=0)
    ttk.Checkbutton(
        main,
        text='Keep override file (don\'t restore after build)',
        variable=keep_var,
    ).grid(row=2, column=1, sticky='w', pady=(8, 4))

    # ── Build button + status ─────────────────────────────────────
    status_var = tk.StringVar(value='Ready')
    status = ttk.Label(main, textvariable=status_var, foreground='gray30')
    status.grid(row=3, column=0, columnspan=2, sticky='w')

    build_btn = ttk.Button(main, text='Build')
    build_btn.grid(row=4, column=0, columnspan=2, sticky='ew', pady=(8, 8))

    # ── Live log window ───────────────────────────────────────────
    log = scrolledtext.ScrolledText(
        main, height=22, font=('Menlo', 11), wrap='none'
    )
    log.grid(row=5, column=0, columnspan=2, sticky='nsew')

    main.columnconfigure(1, weight=1)
    main.rowconfigure(5, weight=1)

    def append(line):
        log.insert(tk.END, line + '\n')
        log.see(tk.END)
        root.update_idletasks()

    def do_build():
        plug_label = plug_var.get()
        plugin = plug_label.split()[0]
        cpus = [k for k, v in cpu_vars.items() if v.get()]
        if not cpus:
            status_var.set('Pick at least one CPU.')
            return
        keep = bool(keep_var.get())
        log.delete('1.0', tk.END)
        status_var.set(f'Building {plugin} for {len(cpus)} target(s)…')
        build_btn.state(['disabled'])

        def runner():
            try:
                rc = cli(plugin, cpus, keep, on_line=append)
                if rc == 0:
                    status_var.set(f'Build succeeded ({len(cpus)} target(s)).')
                else:
                    status_var.set(f'Build finished with errors (rc={rc}). See summary above.')
            except Exception as exc:
                append(f'!! {exc}')
                status_var.set(f'Build error: {exc}')
            finally:
                build_btn.state(['!disabled'])

        threading.Thread(target=runner, daemon=True).start()

    build_btn.configure(command=do_build)
    root.mainloop()

# --------------------------------------------------------------------
# Argument parsing + dispatch
# --------------------------------------------------------------------
def main():
    p = argparse.ArgumentParser(
        description='Build a Tasmota BinPlugin for a chosen CPU family.'
    )
    p.add_argument('--plugin', help='e.g. USE_I2S_MOD (use --list to see all)')
    p.add_argument(
        '--cpu',
        action='append',
        default=None,
        help=(
            'CPU family — one of: ' + ', '.join(CPU_ENVS) + '. Repeat the '
            'flag (--cpu esp32 --cpu esp32_riscv) or pass a comma-separated '
            'list (--cpu esp32,esp32_riscv) to build for multiple targets '
            'in one invocation. Defaults to esp32 if omitted.'
        ),
    )
    p.add_argument(
        '--keep',
        action='store_true',
        help='Do not restore user_config_override.h after the build',
    )
    p.add_argument(
        '--list',
        action='store_true',
        help='List discovered plugin gates and exit',
    )
    p.add_argument(
        '--gui',
        action='store_true',
        help='Launch the Tkinter GUI',
    )
    args = p.parse_args()

    if args.list:
        for name, fname in discover_plugins():
            print(f'{name:<28s} {fname}')
        return

    if args.gui:
        gui()
        return

    if not args.plugin:
        print('Pass --plugin USE_X_MOD or --gui or --list. Discovered plugins:')
        for name, fname in discover_plugins():
            print(f'   {name:<28s} ({fname})')
        print()
        print('CPU choices:', ', '.join(CPU_ENVS))
        sys.exit(2)

    # Normalise --cpu: argparse `action='append'` collects each occurrence,
    # but we also accept a single comma-separated value per flag for
    # convenience (`--cpu esp32,esp32_riscv`).
    raw_cpus = args.cpu or ['esp32']
    cpus = []
    for entry in raw_cpus:
        cpus.extend(c.strip() for c in entry.split(',') if c.strip())
    sys.exit(cli(args.plugin, cpus, args.keep))

if __name__ == '__main__':
    main()
