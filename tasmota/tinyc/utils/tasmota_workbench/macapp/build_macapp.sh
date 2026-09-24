#!/bin/bash
# Builds the standalone "Tasmota Workbench.app" for macOS 11+ -- universal
# (Intel + Apple silicon), no Python or anything else needed on the target Mac.
#
#   ./build_macapp.sh            -> build/dist/Tasmota Workbench.app + .zip
#
# What goes in:
#   Contents/MacOS/Tasmota Workbench   native launcher (launcher.swift), universal
#   Contents/Resources/python/         CPython from python-build-standalone:
#                                      the x86_64 and aarch64 builds merged with
#                                      lipo, every Mach-O file, one by one
#   Contents/Resources/python/.../site-packages
#                                      pyserial + esptool and its dependencies;
#                                      binary modules merged the same way
#   Contents/Resources/tasmota_workbench_server.py   (from the parent folder)
#
# Needs on the BUILD machine: an Intel Mac with Xcode (swiftc, lipo, codesign,
# iconutil) and network access the first time (downloads are cached in
# build/cache). Run it with the system Python out of the way -- it uses only the
# downloaded interpreter.
#
# The result is signed ad hoc. On another Mac, the first start needs
# right-click -> Open (or System Settings -> Privacy & Security -> Open Anyway),
# because it is not notarized.

set -euo pipefail
cd "$(dirname "$0")"
HERE="$(pwd)"

PY_VER=3.12.14
PBS_TAG=20260901
ESPTOOL_VER=5.4.0
CRYPTO_VER=48.0.1          # last cryptography with an Intel (universal2) macOS wheel
PYSERIAL_VER=3.5
APP_VERSION="$(date +%Y.%m.%d)"
MIN_MACOS=11.0

B="$HERE/build"
CACHE="$B/cache"
WORK="$B/work"
DIST="$B/dist"
APP="$DIST/Tasmota Workbench.app"
mkdir -p "$CACHE"

say() { printf '\n== %s\n' "$*"; }

fetch() {                                   # fetch URL FILE  (cached)
    local url="$1" out="$CACHE/$2"
    if [ ! -s "$out" ]; then
        say "download $2"
        curl -fL --retry 3 -o "$out.part" "$url"
        mv "$out.part" "$out"
    fi
}

is_macho() { file -b "$1" | grep -q 'Mach-O'; }

# ── 1. Python runtimes ──────────────────────────────────────────────────────
PBS="https://github.com/astral-sh/python-build-standalone/releases/download/$PBS_TAG"
X86_TGZ="cpython-$PY_VER+$PBS_TAG-x86_64-apple-darwin-install_only.tar.gz"
ARM_TGZ="cpython-$PY_VER+$PBS_TAG-aarch64-apple-darwin-install_only.tar.gz"
fetch "$PBS/${X86_TGZ//+/%2B}" "$X86_TGZ"
fetch "$PBS/${ARM_TGZ//+/%2B}" "$ARM_TGZ"

rm -rf "$WORK"; mkdir -p "$WORK/x86" "$WORK/arm"
say "unpack runtimes"
tar -xzf "$CACHE/$X86_TGZ" -C "$WORK/x86"
tar -xzf "$CACHE/$ARM_TGZ" -C "$WORK/arm"
PYX="$WORK/x86/python/bin/python3"
"$PYX" -c 'import sys, platform; print("x86 runtime", sys.version.split()[0], platform.machine())'

# ── 2. packages into the x86 tree (it runs here), arm64 wheels alongside ──
say "install pyserial + esptool"
"$PYX" -m pip install --disable-pip-version-check --no-warn-script-location -q \
    --no-compile \
    "pyserial==$PYSERIAL_VER" "esptool==$ESPTOOL_VER" "cryptography==$CRYPTO_VER"
SITE_X="$("$PYX" -c 'import sysconfig; print(sysconfig.get_paths()["purelib"])')"
REL_SITE="${SITE_X#$WORK/x86/python/}"

say "arm64 wheels for the binary packages"
"$PYX" -m pip freeze --disable-pip-version-check | grep -v '^pip==' > "$WORK/freeze.txt"
mkdir -p "$CACHE/wheels-arm64" "$WORK/armsite"
while read -r req; do
    [ -z "$req" ] && continue
    # a pure-python or sdist-only package simply has no arm64 wheel: skip it
    "$PYX" -m pip download --disable-pip-version-check -q --no-deps \
        --only-binary=:all: --platform macosx_11_0_arm64 --platform macosx_11_0_universal2 \
        --python-version 3.12 --implementation cp -d "$CACHE/wheels-arm64" "$req" 2>/dev/null || true
done < "$WORK/freeze.txt"
for w in "$CACHE"/wheels-arm64/*.whl; do
    case "$w" in *none-any.whl) continue ;; esac
    ( cd "$WORK/armsite" && unzip -oq "$w" )
done

# ── 3. strip what the Workbench never uses ──────────────────────────────────
say "strip runtime"
for T in "$WORK/x86/python" "$WORK/arm/python"; do
    L="$T/lib/python3.12"
    rm -rf "$L/test" "$L/idlelib" "$L/tkinter" "$L/turtledemo" "$L/lib2to3" \
           "$L/ensurepip" "$L/pydoc_data" "$L/config-3.12-darwin" \
           "$T/include" "$T/share" "$T"/lib/tcl* "$T"/lib/tk* "$T"/lib/itcl* \
           "$T"/lib/libtcl* "$T"/lib/libtk* "$T"/bin/idle3* "$T"/bin/pydoc3* \
           "$T"/bin/2to3* "$T"/bin/python3*-config
    rm -f "$L"/lib-dynload/_tkinter*.so
    # the interpreter is statically linked (otool -L shows no libpython) and no
    # extension module links it either: 36 MB per architecture for nothing
    rm -rf "$T"/lib/libpython3*.dylib "$T"/lib/pkgconfig
    find "$T" -name '__pycache__' -type d -prune -exec rm -rf {} +
done
# pip is only needed to BUILD; the app never installs anything at run time.
# The console scripts pip wrote into bin/ carry this build folder in their
# shebang -- the server runs everything as "python -m ...", so only python stays.
rm -rf "$WORK/x86/python/$REL_SITE"/pip "$WORK/x86/python/$REL_SITE"/pip-*.dist-info
find "$WORK/x86/python/bin" -mindepth 1 ! -name 'python*' -exec rm -f {} +

# ── 4. merge: every Mach-O file becomes universal ───────────────────────────
say "merge x86_64 + arm64 with lipo"
UNI="$WORK/uni/python"
mkdir -p "$WORK/uni"
cp -R "$WORK/x86/python" "$UNI"
missing=0
while IFS= read -r -d '' f; do
    is_macho "$f" || continue
    rel="${f#$UNI/}"
    if lipo -info "$f" 2>/dev/null | grep -q 'arm64'; then
        continue                             # already universal (e.g. cryptography)
    fi
    if [[ "$rel" == "$REL_SITE"/* ]]; then
        a="$WORK/armsite/${rel#$REL_SITE/}"
    else
        a="$WORK/arm/python/$rel"
    fi
    if [ -f "$a" ] && is_macho "$a"; then
        lipo -create "$f" "$a" -output "$f.uni" && mv "$f.uni" "$f"
    else
        echo "  !! no arm64 counterpart: $rel"; missing=$((missing + 1))
    fi
done < <(find "$UNI" -type f -print0)
[ "$missing" -eq 0 ] || { echo "$missing file(s) stay Intel-only -- aborting"; exit 1; }

# byte-compile once: .pyc are architecture independent and make start-up fast.
# ⚠️ unchecked-hash: a normal .pyc is tied to the source file's mtime, and every
# copy (cp, zip, Finder) changes that -- Python then recompiled on each start
# and, if allowed, WROTE the new .pyc into the signed bundle (seen in the first
# build: "a sealed resource is missing or invalid"). Unchecked .pyc are used as
# they are. The launcher also sets PYTHONDONTWRITEBYTECODE.
PYTHONDONTWRITEBYTECODE=1 "$UNI/bin/python3" -m compileall -q -j 0 \
    --invalidation-mode unchecked-hash "$UNI/lib/python3.12" >/dev/null || true

# ── 5. launcher + icon ──────────────────────────────────────────────────────
say "launcher + icon"
for a in x86_64 arm64; do
    swiftc -O -target "$a-apple-macos$MIN_MACOS" -o "$B/launcher-$a" launcher.swift 2>/dev/null
done
lipo -create "$B/launcher-x86_64" "$B/launcher-arm64" -output "$B/launcher"
swiftc -O -o "$B/make_icon" make_icon.swift 2>/dev/null
"$B/make_icon" "$B/icon_1024.png"
ICONSET="$B/AppIcon.iconset"; rm -rf "$ICONSET"; mkdir "$ICONSET"
for s in 16 32 128 256 512; do
    sips -z $s $s "$B/icon_1024.png" --out "$ICONSET/icon_${s}x${s}.png" >/dev/null
    sips -z $((s*2)) $((s*2)) "$B/icon_1024.png" --out "$ICONSET/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$B/AppIcon.icns"

# ── 6. assemble + sign ──────────────────────────────────────────────────────
say "assemble $APP"
rm -rf "$DIST"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$B/launcher" "$APP/Contents/MacOS/Tasmota Workbench"
sed "s/@VERSION@/$APP_VERSION/g" Info.plist > "$APP/Contents/Info.plist"
cp "$B/AppIcon.icns" "$APP/Contents/Resources/"
ditto "$UNI" "$APP/Contents/Resources/python"
cp ../tasmota_workbench_server.py "$APP/Contents/Resources/"

say "sign (ad hoc)"
# every Mach-O inside first (lipo left them unsigned), then the bundle
while IFS= read -r -d '' f; do
    is_macho "$f" && codesign --force -s - "$f" 2>/dev/null
done < <(find "$APP/Contents/Resources/python" -type f -print0)
codesign --force -s - "$APP"
codesign --verify --deep --strict "$APP" && echo "  signature ok"

# ── 7. checks ───────────────────────────────────────────────────────────────
say "check"
PYA="$APP/Contents/Resources/python/bin/python3"
lipo -info "$APP/Contents/MacOS/Tasmota Workbench"
lipo -info "$PYA"
thin=0
while IFS= read -r -d '' f; do
    if is_macho "$f" && ! lipo -info "$f" 2>/dev/null | grep -q 'x86_64 arm64\|arm64 x86_64'; then
        echo "  !! not universal: ${f#$APP/}"; thin=$((thin + 1))
    fi
done < <(find "$APP" -type f -print0)
echo "  Mach-O files not universal: $thin"
export PYTHONDONTWRITEBYTECODE=1      # the checks must not touch the signed bundle
TASMOTA_WORKBENCH_BUNDLED=1 PYTHONNOUSERSITE=1 "$PYA" -c '
import serial, esptool, cryptography, yaml, bitarray, sys, platform
print("  imports ok:", sys.version.split()[0], platform.machine(), "esptool", esptool.__version__)'
echo "  $("$PYA" -m esptool version 2>&1 | sed -n 1p)"
du -sh "$APP"
codesign --verify --deep --strict "$APP" && echo "  signature still ok after the checks"

say "zip"
( cd "$DIST" && ditto -c -k --keepParent "Tasmota Workbench.app" "Tasmota_Workbench_macOS_universal.zip" )
ls -l "$DIST/Tasmota_Workbench_macOS_universal.zip"
