#!/bin/bash
# release.sh — build the 4 standard TinyC firmware binaries, bundle the IDE,
# and update the GitHub "testing" pre-release on gemu2015/Sonoff-Tasmota.
#
# Designed so a release is one command:
#
#   ./release.sh                          # build all + bundle IDE + update release notes + upload
#   ./release.sh --skip-build             # reuse existing bins in $TASMOTA_ROOT/build_output/firmware
#   ./release.sh --skip-upload            # build + bundle only, leave GitHub alone
#   ./release.sh --skip-doccheck          # release even though check_docs.mjs found something
#   ./release.sh --dry-run                # show what would happen, do nothing
#   ./release.sh --notes /path/to/x.md    # use this file as the new "Changes in vX.Y.Z" section
#                                         # instead of the CHANGELOG.md entry
#
# Release notes come from tasmota/tinyc/CHANGELOG.md (gemu2015/Sonoff-Tasmota#122):
# the entry "## X.Y.Z — date" for the version in TC_RELEASE becomes "Changes in
# vX.Y.Z", followed by the four previous entries and a link to the full file. No
# entry → the release stops. The body is rebuilt from the CHANGELOG every time,
# so the GitHub text can no longer drift from it.
#
# Requirements: pio, gh (logged in), node, python3, gzip.

set -euo pipefail

# ─────────── Configuration ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# After the May-2026 in-tree relocation, the canonical layout is:
#   tasmota/tinyc/scripts/release.sh   ← $SCRIPT_DIR
#   tasmota/tinyc/                     ← $TINYC_DIR (TinyC_Reference*.md, tinyc_ide.html.gz, examples/, …)
#   tasmota/tinyc/idesrc/              ← $IDE_SRC_DIR (bundle.py, src/, index.html)
#   <repo-root>                        ← $TASMOTA_ROOT
TINYC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IDE_SRC_DIR="$TINYC_DIR/idesrc"
TASMOTA_ROOT="${TASMOTA_ROOT:-$(cd "$TINYC_DIR/../.." && pwd)}"
TC_RELEASE_HEADER="$TASMOTA_ROOT/tasmota/include/xdrv_124_tinyc_vm.h"
RELEASE_REPO="${RELEASE_REPO:-gemu2015/Sonoff-Tasmota}"
RELEASE_TAG="${RELEASE_TAG:-testing}"
CHANGELOG="$TINYC_DIR/CHANGELOG.md"
CHANGELOG_URL="https://github.com/$RELEASE_REPO/blob/universal/tasmota/tinyc/CHANGELOG.md"
PREV_ENTRIES=4          # older CHANGELOG entries shown below the current one

# Standard firmware targets (env name in platformio_override.ini). The ESP32
# targets now ship with Matter (USE_MATTER_C) — at ~57 kB it fits where HomeKit
# (~156 kB) did not, so testers can pair over Apple Home / Google / Alexa.
# ESP8266 ships again as of 1.6.42: the output-program HWDT — tc_syscall scratch
# buffers overflowing the loop-task stack — is fixed (buffers moved to heap, 53cd18108),
# so a lean ESP8266 build (env tinyc8266-4M) runs basic TinyC programs again. It is
# RAM+flash bound (no Matter / LVGL / camera) and ships .bin + .bin.gz only — no
# .factory.bin (ESP8266 has no separate safeboot/factory image; the .bin is the full one).
STD_TARGETS=(
  tinyc32-4M-plain # ESP32 4MB classic WROOM — plain (no Matter / no camera / no HomeKit)
  # tinyc32-4M-cam # ESP32 4MB WROVER + PSRAM — Matter + camera. EXCLUDED: the
  #                # Matter+camera build overflows internal DRAM (dram0_0_seg by
  #                # ~2 KB as of 1.6.58); not shippable on the 4M WROVER until
  #                # something big moves out of DRAM. Re-enable when it links.
  tinyc32s3        # ESP32-S3 16MB flash (Matter + camera + LVGL)
  tinyc32c3        # ESP32-C3           (Matter)
  tinyc32c6        # ESP32-C6           (Matter)
  tinyc32-p4-full  # ESP32-P4 16MB+PSRAM — FULL: DSI display + LVGL, Matter, MIPI camera, audio
  tinyc8266-4M     # ESP8266 4MB — lean (no Matter / LVGL / camera); HWDT fix, runs basic programs
)

# ─────────── Argument parsing ────────────────────────────────────────────────
SKIP_BUILD=false
SKIP_UPLOAD=false
SKIP_DOCCHECK=false
DRY_RUN=false
NOTES_FILE=""
VERSION_OVERRIDE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)   SKIP_BUILD=true; shift ;;
    --skip-upload)  SKIP_UPLOAD=true; shift ;;
    --skip-doccheck) SKIP_DOCCHECK=true; shift ;;
    --dry-run)      DRY_RUN=true; shift ;;
    --notes)        NOTES_FILE="$2"; shift 2 ;;
    --version)      VERSION_OVERRIDE="$2"; shift 2 ;;
    -h|--help)
      sed -n '2,/^set -e/p' "$0" | sed 's/^# \?//; s/^#//'
      exit 0 ;;
    *)
      echo "Unknown arg: $1" >&2
      echo "Use --help for usage." >&2
      exit 2 ;;
  esac
done

# ─────────── Helpers ─────────────────────────────────────────────────────────
log()  { printf '\033[1;36m[release]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[release]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[release]\033[0m %s\n' "$*" >&2; exit 1; }
run()  {
  if $DRY_RUN; then
    printf '\033[2m[dry-run]\033[0m %s\n' "$*"
  else
    eval "$@"
  fi
}

# Extract firmware version from xdrv_124_tinyc_vm.h `#define TC_RELEASE "x.y.z"`
# (single source of truth for the firmware build the testers flash). The legacy
# IDE-side TINYC_RELEASE in idesrc/src/opcodes.js follows a separate counter and
# is not what users see in the device banner — bundle.py auto-injects TC_RELEASE
# into the IDE at bundle time, so both stay in sync.
extract_version() {
  if [[ -n "$VERSION_OVERRIDE" ]]; then
    echo "$VERSION_OVERRIDE"
    return
  fi
  [[ -f "$TC_RELEASE_HEADER" ]] || die "TC_RELEASE header not found: $TC_RELEASE_HEADER"
  grep -E '^[[:space:]]*#define[[:space:]]+TC_RELEASE\b' "$TC_RELEASE_HEADER" \
    | head -1 \
    | sed -E 's|^[^"]*"([^"]+)".*|\1|'
}

# CHANGELOG.md: version numbers of all single-version entries, newest first
changelog_versions() {
  awk '/^## [0-9]+\.[0-9]+\.[0-9]+ — / { print $2 }' "$CHANGELOG"
}

# CHANGELOG.md entry of version $1 as a release-notes section:
# "### Changes in vX.Y.Z (date):" + its bullets. Prints nothing if absent.
changelog_section() {
  awk -v v="$1" '
    /^## / || /^---/ { if (inside) exit }
    /^## / && $2 == v && $3 == "—" { inside = 1; printf "### Changes in v%s (%s):\n", v, $4; next }
    # drop the blank lines around the entry, keep the ones inside it
    inside && /^[[:space:]]*$/ { if (text) blank++; next }
    inside { while (blank > 0) { print ""; blank-- } print; text = 1 }
  ' "$CHANGELOG"
}

VERSION="$(extract_version)"
[[ -n "$VERSION" ]] || die "Could not extract TINYC_RELEASE from src/opcodes.js"
TODAY="$(date +%F)"

log "Release version: v$VERSION  ($TODAY)"
log "Target repo:     $RELEASE_REPO  (tag: $RELEASE_TAG)"
log "Tasmota root:    $TASMOTA_ROOT"

# ─────────── Sanity checks ───────────────────────────────────────────────────
[[ -d "$TASMOTA_ROOT" ]] || die "TASMOTA_ROOT does not exist: $TASMOTA_ROOT"
[[ -d "$TINYC_DIR" ]]    || die "TINYC_DIR does not exist: $TINYC_DIR"

if ! $SKIP_BUILD; then
  command -v pio >/dev/null || die "pio (PlatformIO) not in PATH"
fi
if ! $SKIP_UPLOAD; then
  command -v gh >/dev/null || die "gh (GitHub CLI) not in PATH"
  if ! $DRY_RUN; then
    gh auth status >/dev/null 2>&1 || die "gh is not authenticated — run 'gh auth login'"
  fi
fi

FW_DIR="$TASMOTA_ROOT/build_output/firmware"
STAGE_DIR="/tmp/tinyc_release_v${VERSION}"

# ─────────── Check the reference tables ──────────────────────────────────────
# The VM-limits table drifted from the #defines for several releases, and the
# "Differences from Standard C" table called two shipped features unsupported —
# nobody reads a 6000-line reference front to back, so nothing caught it. This
# does, in well under a second. Deliberately BEFORE the firmware build: a doc
# problem should cost seconds, not a full pio run.
# Fatal by design — shipping a reference that contradicts the firmware is worse
# than a late release. Override with --skip-doccheck when a finding is known and
# the release cannot wait.
if $DRY_RUN; then
  printf '\033[2m[dry-run]\033[0m node %s\n' "$TINYC_DIR/scripts/check_docs.mjs"
elif $SKIP_DOCCHECK; then
  warn "Skipping reference-table check (--skip-doccheck)"
else
  command -v node >/dev/null || die "node not in PATH (needed to check the reference tables)"
  log "Checking reference tables against the chapters, the #defines and each other…"
  node "$TINYC_DIR/scripts/check_docs.mjs" \
    || die "check_docs.mjs found inconsistencies (above) — fix them, or pass --skip-doccheck"
  # The README version line went stale for 22 releases (1.6.46 while shipping
  # 1.6.68, #122). Both READMEs must name the version being released.
  for readme in README.md README_DE.md; do
    grep -q "v$VERSION\*\*" "$TINYC_DIR/$readme" \
      || die "$readme does not name v$VERSION in its 'Current firmware' line — update it, or pass --skip-doccheck"
  done
fi

# ─────────── Recompile TinyC examples + regenerate the download index ────────
# Keep bytecode/*.tcb fresh (current compiler/ABI) and bytecode/index.txt — the
# repo download list the device fetches — complete, so a release never ships
# stale or missing example binaries. Cheap; runs every time, honors --dry-run.
if $DRY_RUN; then
  printf '\033[2m[dry-run]\033[0m node %s\n' "$TINYC_DIR/scripts/compile_examples.mjs"
else
  command -v node >/dev/null || die "node not in PATH (needed to compile TinyC examples)"
  log "Recompiling TinyC examples + regenerating bytecode/index.txt…"
  node "$TINYC_DIR/scripts/compile_examples.mjs" || die "compile_examples.mjs failed"
  if command -v git >/dev/null && \
     [[ -n "$(git -C "$TASMOTA_ROOT" status --porcelain tasmota/tinyc/bytecode 2>/dev/null)" ]]; then
    log "  NOTE: bytecode/ changed — commit the refreshed .tcb + index.txt so devices fetch them."
  fi
fi

# ─────────── Build firmware ──────────────────────────────────────────────────
if $SKIP_BUILD; then
  log "Skipping build (--skip-build) — reusing $FW_DIR"
else
  log "Building ${#STD_TARGETS[@]} firmware targets sequentially…"
  cd "$TASMOTA_ROOT"
  for env in "${STD_TARGETS[@]}"; do
    log "  → $env"
    if $DRY_RUN; then
      printf '\033[2m[dry-run]\033[0m pio run -e %s\n' "$env"
    else
      # PlatformIO writes a final summary line; tail keeps the log readable.
      if ! pio run -e "$env" 2>&1 | tail -8; then
        die "Build failed for env $env — see output above"
      fi
    fi
  done
fi

# ─────────── Bundle IDE ──────────────────────────────────────────────────────
log "Bundling IDE (tinyc_ide.html.gz)…"
# bundle.py chdirs to its own directory (idesrc/) and writes both
# tinyc_ide.html and tinyc_ide.html.gz there, then copies the .gz up to
# tasmota/tinyc/ so it ends up next to the docs and README. We invoke it
# directly to avoid the bundle.sh wrapper assuming a flat layout.
run "python3 '$IDE_SRC_DIR/bundle.py' --gzip | tail -8"

IDE_GZ="$TINYC_DIR/tinyc_ide.html.gz"
[[ -f "$IDE_GZ" ]] || $DRY_RUN || die "IDE bundle missing: $IDE_GZ"

# ─────────── Stage assets ────────────────────────────────────────────────────
log "Staging assets in $STAGE_DIR"
run "rm -rf '$STAGE_DIR'"
run "mkdir -p '$STAGE_DIR'"

for env in "${STD_TARGETS[@]}"; do
  src="$FW_DIR/${env}.bin"
  [[ -f "$src" ]] || $DRY_RUN || die "Missing $src"
  if [[ "$env" == tinyc8266* ]]; then
    # ESP8266: ship .bin (OTA / esptool) + .bin.gz (compressed OTA). No .factory.bin —
    # ESP8266 has no separate safeboot/factory image; the .bin is the full one.
    run "cp '$src' '$STAGE_DIR/'"
    [[ -f "${src}.gz" ]] && run "cp '${src}.gz' '$STAGE_DIR/'"
  else
    # ESP32 family: ship .bin (OTA) + .factory.bin (esptool / web installer)
    [[ -f "${src%.bin}.factory.bin" ]] || $DRY_RUN || die "Missing ${src%.bin}.factory.bin"
    run "cp '$src' '${src%.bin}.factory.bin' '$STAGE_DIR/'"
  fi
done

# IDE + docs
run "cp '$IDE_GZ' '$STAGE_DIR/'"
run "cp '$TINYC_DIR/TinyC_Reference.md'    '$STAGE_DIR/'"
run "cp '$TINYC_DIR/TinyC_Reference_DE.md' '$STAGE_DIR/'"

# ─────────── Build release notes ─────────────────────────────────────────────
# Local only (files under $STAGE_DIR), so this also runs with --dry-run.
mkdir -p "$STAGE_DIR"
NEW_SECTION="$STAGE_DIR/_new_section.md"
COMBINED_NOTES="$STAGE_DIR/_release_body.md"

if [[ -n "$NOTES_FILE" ]]; then
  [[ -f "$NOTES_FILE" ]] || die "Notes file not found: $NOTES_FILE"
  cp "$NOTES_FILE" "$NEW_SECTION"
  log "Release notes: $NOTES_FILE (--notes)"
else
  [[ -f "$CHANGELOG" ]] || die "CHANGELOG not found: $CHANGELOG"
  changelog_section "$VERSION" > "$NEW_SECTION"
  [[ -s "$NEW_SECTION" ]] \
    || die "CHANGELOG.md has no entry '## $VERSION — <date>' — add it (tinyc/CLAUDE.md §12), or pass --notes"
  log "Release notes: CHANGELOG.md entry for $VERSION"
fi

# Header (fixed text), the current entry, the previous ones, then the link.
cat > "$COMBINED_NOTES" <<HEADER
## TinyC Test Firmware v$VERSION — $TODAY

**For testers only** — may contain experimental features.

### Included targets:
| File | Description |
|------|-------------|
| \`tinyc32-4M-plain.bin\` / \`.factory.bin\` | ESP32 4MB classic WROOM — plain (no Matter / no camera) |
| \`tinyc32s3.bin\` / \`.factory.bin\` | ESP32-S3 — **Matter** + camera + **LVGL** GUI |
| \`tinyc32c3.bin\` / \`.factory.bin\` | ESP32-C3 — **Matter** |
| \`tinyc32c6.bin\` / \`.factory.bin\` | ESP32-C6 — **Matter** |
| \`tinyc32-p4-full.bin\` / \`.factory.bin\` | ESP32-P4 16MB+PSRAM — **FULL**: DSI display + **LVGL**, **Matter**, MIPI camera, audio |
| \`tinyc8266-4M.bin\` / \`.bin.gz\` | **ESP8266** 4MB — lean (no Matter / LVGL / camera); runs basic TinyC programs (HWDT fix). Flash the \`.bin\` (OTA/esptool); no \`.factory.bin\` |
| \`tinyc_ide.html.gz\` | Browser IDE (upload to filesystem) |
| \`TinyC_Reference.md\` / \`_DE.md\` | Documentation EN/DE |

The ESP32 builds include **Matter** (Apple Home / Google / Alexa): a script
defines the device (see \`matter_*.tc\` examples); open pairing from the \`/mt\`
web page (Bind) and add the QR in your controller app.

### How to flash:
- OTA: Firmware Upgrade → Upload \`.bin\` file
- Factory install: Use \`.factory.bin\` with esptool or web installer
- Upload \`tinyc_ide.html.gz\` via Tasmota file manager (Consoles → Manage File System),
  or run \`TinyCIde\` in the console — a firmware flash does **not** replace the IDE

HEADER
cat "$NEW_SECTION" >> "$COMBINED_NOTES"
echo "" >> "$COMBINED_NOTES"

n=0
for v in $(changelog_versions | awk -v cur="$VERSION" 'seen { print } $0 == cur { seen = 1 }'); do
  [[ $n -lt $PREV_ENTRIES ]] || break
  changelog_section "$v" >> "$COMBINED_NOTES"
  echo "" >> "$COMBINED_NOTES"
  n=$((n + 1))
done
echo "**All versions and the syscall-ABI table:** [CHANGELOG.md]($CHANGELOG_URL)" >> "$COMBINED_NOTES"

log "Release body: $COMBINED_NOTES ($(wc -c < "$COMBINED_NOTES" | tr -d ' ') bytes, $n older entries)"

# ─────────── Upload to GitHub ────────────────────────────────────────────────
if $SKIP_UPLOAD; then
  log "Skipping upload (--skip-upload). Staged artifacts in $STAGE_DIR"
  exit 0
fi

log "Updating GitHub release '$RELEASE_TAG' on $RELEASE_REPO …"

# `gh release upload --clobber` overwrites existing assets in place.
# Dedupe by basename — `*.bin.gz` and `*.gz` would otherwise both match the
# ESP8266 .gz and GitHub 404s on the second upload.
# (Plain string list — macOS ships bash 3.2, no associative arrays.)
SEEN_LIST=" "
UPLOAD_ASSETS=()
for f in "$STAGE_DIR"/*.bin "$STAGE_DIR"/*.bin.gz "$STAGE_DIR"/*.gz "$STAGE_DIR"/*.md; do
  [[ -f "$f" ]] || continue
  base="$(basename "$f")"
  case "$base" in
    _*) continue ;;            # internal helper files (_stub.md, _release_body.md, …)
  esac
  case "$SEEN_LIST" in
    *" $base "*) continue ;;
  esac
  SEEN_LIST="$SEEN_LIST$base "
  UPLOAD_ASSETS+=("$f")
done

# An empty array trips `set -u` in macOS bash 3.2 on the loop below — which is
# exactly what a --dry-run looks like (nothing is staged). Stop cleanly instead.
if [[ ${#UPLOAD_ASSETS[@]} -eq 0 ]]; then
  $DRY_RUN && { log "Dry run: nothing staged — stopping before the upload."; exit 0; }
  die "No assets staged in $STAGE_DIR"
fi

log "Uploading ${#UPLOAD_ASSETS[@]} asset(s):"
for f in "${UPLOAD_ASSETS[@]}"; do printf '         %s\n' "$(basename "$f")"; done

run "gh release upload '$RELEASE_TAG' -R '$RELEASE_REPO' --clobber ${UPLOAD_ASSETS[@]@Q}"

log "Updating release title + body …"
run "gh release edit '$RELEASE_TAG' -R '$RELEASE_REPO' \
       --title 'TinyC Test Build v$VERSION' \
       --notes-file '$COMBINED_NOTES'"

log "Done. https://github.com/$RELEASE_REPO/releases/tag/$RELEASE_TAG"
