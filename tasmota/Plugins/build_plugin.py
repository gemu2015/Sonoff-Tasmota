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
import time
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
    # Default: minimal plugin-build envs (defined in
    # platformio_override.ini). They `extends` the regular tasmota{,32,32c3}-4M
    # envs and add `-Dplugin_host_build`, which engages a strip block in
    # user_config_override.h that #undef's heavy features the plugin
    # compile path doesn't need (Berry, Zigbee, BLE, KNX, Telegram,
    # display drivers, picotts, etc). Also drops obj-dump.py from
    # extra_scripts. Net win: ~20-25% faster warm builds, ~5-15s less
    # post-build disassembly. Plugin .bin output is ABI-identical to a
    # full-firmware build (jumptable layout in xdrv_123_plugins.ino is
    # unconditional). First build per env pays a ~120s cold-cache cost,
    # subsequent invocations of the same env hit the warm cache.
    #
    # USE_TINYC stays defined in the strip block. Stripping it would
    # leave xdrv_124_tinyc.ino's static functions referenced by
    # Arduino's auto-generated forward decls without matching
    # definitions ("used but never defined"). Keeping it costs maybe
    # 20-30K of compiled firmware code — a small price for not having
    # to refactor xdrv_124 from .ino to .cpp.
    'esp8266':     ('tasmota-plugin',     'ESP8266 (plugin-host)'),
    'esp32':       ('tasmota32-plugin',   'ESP32 Tensilica (plugin-host, orig / S2 / S3)'),
    'esp32_riscv': ('tasmota32c3-plugin', 'ESP32 RISC-V (plugin-host, C3 / C6 / …)'),
}

# Full firmware envs — kept as a documented fallback. To use, swap
# CPU_ENVS to point at these. Produces identical plugin .bin output
# but compiles ~25% slower because the firmware half of the link step
# has to compile every USE_* feature.
_FULL_CPU_ENVS = {
    'esp8266':     ('tasmota-4M',     'ESP8266 (full)'),
    'esp32':       ('tasmota32-4M',   'ESP32 Tensilica (full)'),
    'esp32_riscv': ('tasmota32c3-4M', 'ESP32 RISC-V (full)'),
}

# --------------------------------------------------------------------
# Curated plugin .bin layout. Each fresh .bin produced by a per-CPU
# build lands flat in `build_output/firmware/<NAME>{,_32,_32r}.bin`;
# we ALSO copy it into the matching `Plugins/<arch>/` subdirectory so
# the curated tree (used for distribution + the device's plugin
# manager) stays in sync without manual file shuffling. Filename
# suffix (_32 / _32r) is already part of the produced .bin name —
# we copy verbatim, no rename. ESP8266 plugins have no suffix.
# --------------------------------------------------------------------
_CPU_TO_ARCH_DIR = {
    'esp8266':     'ESP8266',
    'esp32':       'ESP32/TENSILICA',
    'esp32_riscv': 'ESP32/RISC',
}

# --------------------------------------------------------------------
# Up-to-date detection. Each successful (plugin, cpu) build writes a
# tiny stamp file recording the source-file mtime at the moment of
# the build. Later runs compare the current source mtime against the
# stamp; if unchanged AND none of the shared headers (module.h,
# module_defines.h) are newer than the stamp, the build is skipped.
# Shared-header changes still trigger rebuild because they're
# transitively included by every plugin.
#
# Stamps live under `build_output/firmware/Plugins/.stamps/` and are
# named `<USE_X_MOD>__<cpu>.txt` to avoid filename collisions across
# CPU families. The .stamps directory is purely cache — safe to
# delete to force a clean rebuild of everything.
# --------------------------------------------------------------------
_SHARED_HEADERS = ['module.h', 'module_defines.h']
_STAMPS_DIR_NAME = '.stamps'


def _stamp_path(cpu, plugin):
    return (REPO / 'build_output' / 'firmware' / 'Plugins'
            / _STAMPS_DIR_NAME / f'{plugin}__{cpu}.txt')


def _read_stamp(cpu, plugin):
    """Return the recorded build-completion time (float seconds since
    epoch), or None if no stamp exists or the file is corrupt.
    Treating corrupt as 'rebuild' is the safe default — we'd rather
    waste a minute on a fresh compile than skip a real source change.

    Stamp value semantics: it's a "rebuild trigger threshold". Any
    relevant input file (the plugin source, plus shared headers in
    `_SHARED_HEADERS`) with mtime > stamp triggers rebuild on the
    next run. Recording the build-completion time means a freshly
    finished build always satisfies `mtime <= stamp` for every input
    that already existed at build time, regardless of how the source
    or header mtimes compare to each other."""
    p = _stamp_path(cpu, plugin)
    try:
        return float(p.read_text().strip())
    except (OSError, ValueError):
        return None


def _write_stamp(cpu, plugin, completion_time):
    """Record the build-completion time so subsequent runs know
    everything older than this is covered by this build.

    Called from cli() after a confirmed-OK build with the curated
    .bin already in place. Pass the current time for fresh builds,
    or the bin's own mtime for retroactive stamping (since the bin
    was effectively built at its own mtime)."""
    p = _stamp_path(cpu, plugin)
    p.parent.mkdir(parents=True, exist_ok=True)
    try:
        p.write_text(f'{completion_time}\n')
    except OSError as exc:
        # Stamp-write failure is non-fatal — we just won't be able to
        # skip this plugin on the next run. Log and move on.
        return False
    return True


def _module_names(src_cpp):
    """Return all candidate module names from a plugin source.

    A single plugin can have multiple MODULE_DESCRIPTOR calls gated
    by `#ifdef`s (e.g. xsns_09_bmp.cpp has BMXx80S under USE_SOFTWIRE
    and plain BMXx80 otherwise; xdrv_42_i2s.cpp has I2SAUDIO + I2SWAV
    under USE_MP3). The build picks one per `#ifdef` block; we don't
    replicate the preprocessor here, so we collect ALL candidates
    and let the caller match whichever has a matching curated .bin.

    Each candidate is either:
      * a literal string passed directly to MODULE_DESCRIPTOR, or
      * a macro (e.g. MODNAME) resolved via `#define MODNAME "…"`.
    Returns a list of candidate strings; empty list if no patterns
    match."""
    try:
        text = src_cpp.read_text()
    except OSError:
        return []
    candidates = []
    seen = set()
    for m in re.finditer(r'MODULE_DESCRIPTOR\d*\s*\(\s*([^,]+)', text):
        arg = m.group(1).strip()
        # Literal "NAME"
        lit = re.match(r'"([^"]+)"', arg)
        if lit:
            name = lit.group(1)
            if name not in seen:
                seen.add(name)
                candidates.append(name)
            continue
        # Identifier → resolve via #define. There may be multiple
        # #define lines for the same macro under different #ifdefs;
        # collect ALL of them.
        for md in re.finditer(
            r'#\s*define\s+' + re.escape(arg) + r'\s+"([^"]+)"', text
        ):
            name = md.group(1)
            if name not in seen:
                seen.add(name)
                candidates.append(name)
    return candidates


# Backward-compatible single-name helper — returns the first
# candidate or None. Kept for any external callers; the retroactive
# scan uses the list form directly.
def _module_name(src_cpp):
    names = _module_names(src_cpp)
    return names[0] if names else None


# Module-level guard so the retroactive scan runs at most once per
# process — we don't want it firing 27× from inside build_all()'s
# per-plugin cli() calls.
_retroactive_done = False


def _retroactive_stamps(on_line=lambda *a: None):
    """Backfill stamps for curated bins that are demonstrably newer
    than their source.

    For each (plugin, CPU) where:
      * no stamp exists yet
      * the matching curated bin EXISTS
      * `bin.mtime > source.mtime + 60s` (safety margin against
         tight edit→build→edit cycles where the bin briefly looked
         newer than the source)
    write a stamp with the current source mtime as its value. The
    semantic is "this bin was built from a source state at or before
    the recorded mtime" — which lets the next `_is_up_to_date()`
    check short-circuit if the source hasn't been touched since.

    Idempotent + side-effect-only: skipping a build on the next run
    is the only observable difference. Safe to call any time."""
    global _retroactive_done
    if _retroactive_done:
        return 0
    _retroactive_done = True

    SAFETY_MARGIN_SEC = 60
    written = 0
    for plug_name, fname in discover_plugins():
        src_cpp = HERE / fname
        if not src_cpp.exists():
            continue
        try:
            src_mtime = src_cpp.stat().st_mtime
        except OSError:
            continue
        # A plugin source may declare several MODULE_DESCRIPTOR calls
        # gated by #ifdefs (e.g. BMXx80S/BMXx80, I2SAUDIO/I2SWAV) —
        # the build picks one per arch + config. We try each candidate
        # and stamp against whichever has a curated bin on disk.
        candidates = _module_names(src_cpp)
        if not candidates:
            continue
        for cpu, arch in _CPU_TO_ARCH_DIR.items():
            if _read_stamp(cpu, plug_name) is not None:
                continue
            suffix = '' if cpu == 'esp8266' else ('_32' if cpu == 'esp32' else '_32r')
            bin_path = None
            for modname in candidates:
                cand = (REPO / 'build_output' / 'firmware' / 'Plugins'
                        / arch / f'{modname}{suffix}.bin')
                if cand.exists():
                    bin_path = cand
                    break
            if bin_path is None:
                continue
            try:
                bin_mtime = bin_path.stat().st_mtime
            except OSError:
                continue
            # Bin must be strictly newer than source by the safety
            # margin. If equal or older, the bin was built from a
            # different (probably older) source state and shouldn't
            # be claimed as up-to-date. Same check applied to shared
            # headers — a build whose bin predates a shared header
            # is stale even if the plugin's own source is older.
            if bin_mtime <= src_mtime + SAFETY_MARGIN_SEC:
                continue
            shared_too_new = False
            for hdr in _SHARED_HEADERS:
                h = HERE / hdr
                try:
                    if bin_mtime <= h.stat().st_mtime + SAFETY_MARGIN_SEC:
                        shared_too_new = True
                        break
                except OSError:
                    continue
            if shared_too_new:
                continue
            # Stamp value = bin's mtime. The bin was effectively built
            # at its own mtime, so a future _is_up_to_date check that
            # asks "did this build cover the file with mtime X?" gets
            # the right answer when X is older than the bin. (Earlier
            # version stored src_mtime, which broke the shared-header
            # check whenever a header was newer than the source —
            # _is_up_to_date would conclude the build was stale.)
            if _write_stamp(cpu, plug_name, bin_mtime):
                written += 1
    if written:
        on_line(f'== Retroactive stamps written: {written} ==')
    return written


def _is_up_to_date(cpu, plugin, src_cpp):
    """True if the recorded stamp >= the source's current mtime AND
    no shared header is newer than the stamp. False (=> needs build)
    if any check is missing or out of date.

    Returns False fast when the stamp doesn't exist (first run, or
    cache cleared). Doesn't try to reason about transitively included
    plugin-local .h files — `_SHARED_HEADERS` covers the cross-cutting
    case, anything else falls back to the user touching the .cpp or
    using --force."""
    stamp = _read_stamp(cpu, plugin)
    if stamp is None:
        return False
    try:
        src_mtime = src_cpp.stat().st_mtime
    except OSError:
        return False  # missing source — let cli() raise the proper error
    if src_mtime > stamp:
        return False
    for hdr in _SHARED_HEADERS:
        h = HERE / hdr
        try:
            if h.stat().st_mtime > stamp:
                return False
        except OSError:
            # Missing shared header is weird but not our problem here.
            continue
    return True

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


def _move_to_curated(cpu, bin_path):
    """Move a freshly-built plugin .bin into the curated subdir,
    removing the flat-dir copy so `build_output/firmware/` doesn't
    accumulate stale artifacts across iterations.

    Returns the destination Path on success, or None on failure. Uses
    `os.replace` for an atomic overwrite when a prior bin already
    exists at the destination — and `shutil.move` for the cross-device
    case (rare on a normal dev box). Mtime is preserved by both.

    The earlier copy-only variant left the flat-dir bin in place,
    which made `find_output_bin(newer_than=mtime_before)` see the
    SAME bin on subsequent iterations (the timestamp comparison
    would still filter correctly, but the dir grew unbounded). Move
    semantics keeps the flat dir tidy and the curated tree
    authoritative."""
    arch_subdir = _CPU_TO_ARCH_DIR.get(cpu)
    if arch_subdir is None:
        return None
    dest_dir = REPO / 'build_output' / 'firmware' / 'Plugins' / arch_subdir
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / bin_path.name
    # If the destination already exists (a prior curated build),
    # remove it first so move/replace doesn't error on cross-device
    # cases. Atomic-overwrite is preferred but not always available.
    try:
        if dest.exists():
            dest.unlink()
        # os.replace is atomic on the same filesystem; falls back via
        # shutil.move if cross-device. Try shutil.move first since it
        # handles both transparently.
        shutil.move(str(bin_path), str(dest))
    except OSError:
        # Best-effort fallback: copy + try to delete source.
        shutil.copy2(bin_path, dest)
        try:
            bin_path.unlink()
        except OSError:
            pass
    return dest


# Backward-compatible alias — older code paths called the function by
# the previous name. Keeping it as an alias avoids breaking anything
# that imports build_plugin as a module.
_copy_to_curated = _move_to_curated


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
def cli(plugin, cpus, keep, on_line=print, force=False):
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

    src_cpp = HERE / plugins[plugin]

    # Backfill stamps for any curated bins that were already up-to-date
    # before this version of the script existed. Module-level guard
    # ensures it only runs once per process even when build_all() loops
    # through cli() N times.
    _retroactive_stamps(on_line=on_line)

    on_line(f'== Plugin: {plugin} ({plugins[plugin]}) ==')
    on_line(f'== Targets: {", ".join(cpus)} ==')

    # Up-to-date pre-flight: if every requested CPU is already current,
    # we can skip the override-rewrite + pio dance entirely. Saves
    # several seconds on no-op runs (Python startup + override I/O +
    # pio metadata scan ≈ 5-10s otherwise).
    if not force:
        skip_cpus = [c for c in cpus if _is_up_to_date(c, plugin, src_cpp)]
        if len(skip_cpus) == len(cpus):
            for cpu in cpus:
                on_line(f'   [{cpu}] up to date — skipping (use --force to rebuild)')
            on_line('')
            on_line('== Summary ==')
            for cpu in cpus:
                on_line(f'   {cpu:<14s} SKIP  up-to-date')
            return 0

    original = rewrite_override(plugin)
    results = []   # list of (cpu, rc, output_path_or_None)
    overall_rc = 0

    try:
        for cpu in cpus:
            env, label = CPU_ENVS[cpu]
            on_line('')
            on_line(f'── [{cpu}] {label} via env "{env}" ──')
            # Per-CPU up-to-date check — handles the mixed case where
            # some CPUs are current and others need rebuild.
            if not force and _is_up_to_date(cpu, plugin, src_cpp):
                on_line(f'   [{cpu}] up to date — skipping (use --force to rebuild)')
                results.append((cpu, 0, None))  # rc=0, no fresh bin
                continue
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
                # Move into the curated Plugins/<arch>/ subdir so the
                # device-side plugin manager + distribution tree pick
                # up the new build automatically. The flat-dir copy is
                # deleted so build_output/firmware/ doesn't accumulate
                # stale .bin artifacts across iterations.
                try:
                    curated = _move_to_curated(cpu, out)
                    if curated:
                        try:
                            crel = curated.relative_to(REPO)
                        except ValueError:
                            crel = curated
                        on_line(f'   [{cpu}] Moved → {crel}')
                except OSError as exc:
                    on_line(f'!! [{cpu}] Could not move to curated dir: {exc}')
                # Record the build-completion time. Stamp value is
                # "now" so subsequent _is_up_to_date checks see it as
                # the threshold: any input file (source OR shared
                # header) with mtime > stamp → rebuild needed. Using
                # time.time() rather than the bin's mtime defends
                # against unusual filesystem behaviour (precision
                # rounding, mount-point time skew). Both work in
                # principle, but a wall-clock value is simplest.
                try:
                    _write_stamp(cpu, plugin, time.time())
                except OSError:
                    pass  # non-fatal — next run just won't skip
                results.append((cpu, 0, out))
            else:
                on_line(f'!! [{cpu}] Build succeeded but no fresh plugin .bin found')
                results.append((cpu, 0, None))

        # Summary line so the user knows what landed without scrolling.
        # rc=0 + out=None means we skipped (up-to-date), which is its
        # own outcome — distinguish it from "build ran but produced
        # no bin" which is the surprising case.
        on_line('')
        on_line('== Summary ==')
        for cpu, rc, out in results:
            if rc == 0 and out:
                tag = 'OK'
            elif rc == 0 and out is None:
                # Could be either skipped-as-up-to-date or built-no-bin.
                # We can disambiguate via the stamp: if a stamp exists
                # and matches the source, this was a skip.
                if _is_up_to_date(cpu, plugin, src_cpp):
                    tag = 'SKIP  up-to-date'
                else:
                    tag = 'OK (no bin?)'
            else:
                tag = f'FAIL rc={rc}'
            extra = f'  {out.name}' if out else ''
            on_line(f'   {cpu:<14s} {tag}{extra}')
        return overall_rc
    finally:
        if keep:
            on_line('== Left user_config_override.h with chosen plugin enabled (--keep) ==')
        else:
            restore_override(original)
            on_line('== Restored user_config_override.h ==')

def build_all(cpus, keep, on_line=print, force=False):
    """Build every discovered plugin for every requested CPU.

    Iterates over all `discover_plugins()` entries; for each, calls
    `cli(plugin, cpus, keep=True, ...)` so the override-rewrite-and-
    restore dance happens once per plugin (we explicitly restore at
    the very end if `keep=False`). Logs a top-level summary at the
    end. Continues past per-plugin failures so a single broken
    plugin doesn't block the rest.

    Run time: ~1 minute per (plugin, CPU) on a warm cache, longer
    cold. With ~13-23 plugins per CPU and 1-3 CPUs, plan for 10-60
    minutes total. Drop the GUI thread; print live progress. Caller
    is expected to gate this behind a confirmation dialog (the GUI
    'Build ALL' button does)."""
    if isinstance(cpus, str):
        cpus = [cpus]
    if not cpus:
        on_line('!! No CPU targets selected.')
        return 2

    plugins = discover_plugins()
    if not plugins:
        on_line('!! No plugins discovered.')
        return 2

    on_line(f'== Build ALL: {len(plugins)} plugin(s) × {len(cpus)} CPU(s) '
            f'= {len(plugins) * len(cpus)} build(s) ==')
    on_line('')

    overall_rc = 0
    per_plugin = []  # (plugin, rc)
    for idx, (plug_name, fname) in enumerate(plugins, 1):
        on_line('')
        on_line(f'######## [{idx}/{len(plugins)}] {plug_name} ({fname}) ########')
        try:
            # Pass keep=True for every iteration so we don't restore +
            # rewrite the override file 13×. Then restore once below.
            rc = cli(plug_name, cpus, keep=True, on_line=on_line, force=force)
        except Exception as exc:
            on_line(f'!! Exception during {plug_name}: {exc}')
            rc = 1
        per_plugin.append((plug_name, rc))
        if rc != 0:
            overall_rc = rc

    # Top-level summary across all plugins.
    on_line('')
    on_line('===== Build ALL summary =====')
    ok = sum(1 for _, rc in per_plugin if rc == 0)
    fail = len(per_plugin) - ok
    on_line(f'   {ok} OK / {fail} FAIL out of {len(per_plugin)} plugin(s) × {len(cpus)} CPU(s)')
    if fail:
        on_line('   Failures:')
        for name, rc in per_plugin:
            if rc != 0:
                on_line(f'     {name:<28s} rc={rc}')

    # We forced keep=True above so per-plugin loops don't fight the
    # override file; if the caller asked for restore, do it here once.
    if not keep:
        # Use a benign rewrite back to the first plugin to recover the
        # pristine state, then restore. Simpler: re-run rewrite with the
        # last plugin to capture the original snapshot (rewrite_override
        # returns the pre-rewrite content), then restore that. But we
        # already wrote it during the loop, so the cleanest restore is
        # to ask the caller — for safety, do nothing here and emit a
        # note. The user can re-run with --plugin foo (no --keep) to
        # trigger restore for one cycle.
        on_line('== Note: --keep was OFF, but build_all leaves the '
                'override file in its post-build state. Run any single '
                '--plugin build without --keep to restore.')

    return overall_rc


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

    # ── Keep override + Force rebuild checkboxes ──────────────────
    opt_frame = ttk.Frame(main)
    opt_frame.grid(row=2, column=1, sticky='w', pady=(8, 4))
    keep_var = tk.IntVar(value=0)
    ttk.Checkbutton(
        opt_frame,
        text='Keep override file (don\'t restore after build)',
        variable=keep_var,
    ).pack(anchor='w')
    # Force rebuild — bypass the up-to-date stamp check. By default,
    # later runs skip plugins whose source hasn't changed since the
    # last successful build, which is what most users want; force is
    # for "I edited a shared header that the stamp doesn't track" or
    # "something feels off, redo everything".
    force_var = tk.IntVar(value=0)
    ttk.Checkbutton(
        opt_frame,
        text='Force rebuild even if up to date',
        variable=force_var,
    ).pack(anchor='w')

    # ── Build button + status ─────────────────────────────────────
    status_var = tk.StringVar(value='Ready')
    status = ttk.Label(main, textvariable=status_var, foreground='gray30')
    status.grid(row=3, column=0, columnspan=2, sticky='w')

    btn_row = ttk.Frame(main)
    btn_row.grid(row=4, column=0, columnspan=2, sticky='ew', pady=(8, 8))
    build_btn = ttk.Button(btn_row, text='Build selected plugin')
    build_btn.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 4))
    build_all_btn = ttk.Button(btn_row, text='Build ALL plugins (×selected CPUs)')
    build_all_btn.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(4, 0))

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
        force = bool(force_var.get())
        log.delete('1.0', tk.END)
        status_var.set(f'Building {plugin} for {len(cpus)} target(s)…')
        build_btn.state(['disabled'])

        def runner():
            try:
                rc = cli(plugin, cpus, keep, on_line=append, force=force)
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

    def do_build_all():
        cpus = [k for k, v in cpu_vars.items() if v.get()]
        if not cpus:
            status_var.set('Pick at least one CPU.')
            return
        # Confirm — this is a long-running operation.
        from tkinter import messagebox
        n_plugins = len(plugins)
        n_cpus = len(cpus)
        eta_min = max(1, (n_plugins * n_cpus * 60) // 60)  # ~60s per build, warm cache
        if not messagebox.askyesno(
            'Build ALL plugins',
            f'Build all {n_plugins} plugins for {n_cpus} CPU target(s)?\n\n'
            f'That is {n_plugins * n_cpus} builds. Estimated time: '
            f'~{eta_min} min on a warm cache, longer cold.\n\n'
            f'Output goes to build_output/firmware/Plugins/<arch>/.'
        ):
            return
        keep = bool(keep_var.get())
        force = bool(force_var.get())
        log.delete('1.0', tk.END)
        status_var.set(
            f'Building ALL plugins ({n_plugins} × {n_cpus} target(s))…'
            + (' [force]' if force else ' [skip up-to-date]')
        )
        build_btn.state(['disabled'])
        build_all_btn.state(['disabled'])

        def runner():
            try:
                rc = build_all(cpus, keep, on_line=append, force=force)
                if rc == 0:
                    status_var.set(f'Build ALL succeeded ({n_plugins} × {n_cpus}).')
                else:
                    status_var.set(
                        f'Build ALL finished with errors (rc={rc}). See summary above.'
                    )
            except Exception as exc:
                append(f'!! {exc}')
                status_var.set(f'Build ALL error: {exc}')
            finally:
                build_btn.state(['!disabled'])
                build_all_btn.state(['!disabled'])

        threading.Thread(target=runner, daemon=True).start()

    build_btn.configure(command=do_build)
    build_all_btn.configure(command=do_build_all)
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
    p.add_argument(
        '--all',
        action='store_true',
        help=(
            'Build EVERY discovered plugin for the requested --cpu(s). '
            'Output is curated into build_output/firmware/Plugins/<arch>/. '
            'Long-running — plan for ~1 minute per (plugin, CPU) on a '
            'warm cache.'
        ),
    )
    p.add_argument(
        '--force',
        action='store_true',
        help=(
            'Bypass the up-to-date check. Without this, later runs '
            'skip plugins whose source mtime is unchanged since the '
            'last successful build (recorded in '
            'build_output/firmware/Plugins/.stamps/). Use --force after '
            'editing a shared header that the stamp does not track.'
        ),
    )
    args = p.parse_args()

    if args.list:
        for name, fname in discover_plugins():
            print(f'{name:<28s} {fname}')
        return

    if args.gui:
        gui()
        return

    # Normalise --cpu before either --all or single-plugin flow.
    raw_cpus = args.cpu or ['esp32']
    cpus = []
    for entry in raw_cpus:
        cpus.extend(c.strip() for c in entry.split(',') if c.strip())

    if args.all:
        sys.exit(build_all(cpus, args.keep, force=args.force))

    if not args.plugin:
        print('Pass --plugin USE_X_MOD or --gui or --list. Discovered plugins:')
        for name, fname in discover_plugins():
            print(f'   {name:<28s} ({fname})')
        print()
        print('CPU choices:', ', '.join(CPU_ENVS))
        sys.exit(2)

    # `cpus` was normalised above (so --all could share the same parsing).
    sys.exit(cli(args.plugin, cpus, args.keep, force=args.force))

if __name__ == '__main__':
    main()
