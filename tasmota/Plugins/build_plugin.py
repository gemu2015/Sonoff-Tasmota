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
# Rewrite user_config_override.h: ensure exactly one USE_..._MOD is
# active. Comment out every other one we find, uncomment (or insert)
# the target. Other symbols (e.g. BRESSER_6_IN_1, DS18x20_USE_ID_ALIAS)
# are left untouched — those are sub-options of plugins, not plugin
# gates themselves.
#
# Returns the ORIGINAL text so the caller can restore it afterwards.
# --------------------------------------------------------------------
_ACTIVE_RE     = re.compile(r'^(\s*)#define\s+(USE_[A-Z0-9_]+_MOD)\b(.*)$')
_COMMENTED_RE  = re.compile(r'^(\s*)//\s*#define\s+(USE_[A-Z0-9_]+_MOD)\b(.*)$')

def rewrite_override(target_use):
    """Edit OVERRIDE in-place; return the prior text for restore."""
    if not OVERRIDE.exists():
        raise FileNotFoundError(OVERRIDE)
    original = OVERRIDE.read_text()

    out_lines = []
    target_seen = False
    for line in original.splitlines(keepends=True):
        m = _ACTIVE_RE.match(line)
        if m:
            ind, name, tail = m.groups()
            if name == target_use:
                out_lines.append(line)
                target_seen = True
            else:
                out_lines.append(f'{ind}//#define {name}{tail}\n')
            continue
        m = _COMMENTED_RE.match(line)
        if m:
            ind, name, tail = m.groups()
            if name == target_use:
                out_lines.append(f'{ind}#define {name}{tail}\n')
                target_seen = True
                continue
            # else: leave the comment as-is
        out_lines.append(line)

    if not target_seen:
        # Plugin gate didn't appear in the override — append it.
        if not out_lines or not out_lines[-1].endswith('\n'):
            out_lines.append('\n')
        out_lines.append(f'#define {target_use}\n')

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
# Locate the most recent plugin .bin produced by grepmodule-firmware.py
# --------------------------------------------------------------------
def find_output_bin():
    base = REPO / 'build_output' / 'firmware' / 'Plugins'
    if not base.exists():
        return None
    bins = list(base.rglob('*.bin'))
    if not bins:
        return None
    return max(bins, key=lambda p: p.stat().st_mtime)

# --------------------------------------------------------------------
# CLI flow
# --------------------------------------------------------------------
def cli(plugin, cpu, keep, on_line=print):
    plugins = dict(discover_plugins())
    if plugin not in plugins:
        on_line(f'!! Unknown plugin: {plugin}')
        on_line('Available:')
        for name, fname in discover_plugins():
            on_line(f'   {name:<28s} ({fname})')
        return 2
    if cpu not in CPU_ENVS:
        on_line(f'!! Unknown CPU: {cpu}. Available: {", ".join(CPU_ENVS)}')
        return 2
    env, label = CPU_ENVS[cpu]
    on_line(f'== Building {plugin} ({plugins[plugin]}) for {label} via env "{env}" ==')

    original = rewrite_override(plugin)
    try:
        rc = run_build(env, on_line=on_line)
        if rc != 0:
            on_line(f'!! Build failed (rc={rc})')
            return rc
        out = find_output_bin()
        if out:
            try:
                rel = out.relative_to(REPO)
            except ValueError:
                rel = out
            on_line(f'== Output: {rel} ({out.stat().st_size:,} bytes) ==')
        else:
            on_line('!! Build succeeded but no plugin .bin under build_output/firmware/Plugins/')
        return 0
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

    # ── CPU radio ─────────────────────────────────────────────────
    ttk.Label(main, text='CPU:').grid(row=1, column=0, sticky='w')
    cpu_var = tk.StringVar(value='esp32')
    cpu_frame = ttk.Frame(main)
    cpu_frame.grid(row=1, column=1, sticky='w')
    for key, (env, label) in CPU_ENVS.items():
        ttk.Radiobutton(
            cpu_frame, text=f'{label}    ({env})', variable=cpu_var, value=key
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
        cpu = cpu_var.get()
        keep = bool(keep_var.get())
        log.delete('1.0', tk.END)
        status_var.set(f'Building {plugin}…')
        build_btn.state(['disabled'])

        def runner():
            try:
                rc = cli(plugin, cpu, keep, on_line=append)
                if rc == 0:
                    status_var.set('Build succeeded.')
                else:
                    status_var.set(f'Build failed (rc={rc}).')
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
        choices=list(CPU_ENVS),
        default='esp32',
        help='CPU family (default: esp32)',
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

    sys.exit(cli(args.plugin, args.cpu, args.keep))

if __name__ == '__main__':
    main()
