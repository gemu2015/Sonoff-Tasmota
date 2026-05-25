#!/bin/bash
# release.sh — build the 4 standard TinyC firmware binaries, bundle the IDE,
# and update the GitHub "testing" pre-release on gemu2015/Sonoff-Tasmota.
#
# Designed so a release is one command:
#
#   ./release.sh                          # build all + bundle IDE + update release notes + upload
#   ./release.sh --skip-build             # reuse existing bins in $TASMOTA_ROOT/build_output/firmware
#   ./release.sh --skip-upload            # build + bundle only, leave GitHub alone
#   ./release.sh --dry-run                # show what would happen, do nothing
#   ./release.sh --notes /path/to/x.md    # use this file as the new "Changes in vX.Y.Z" section
#                                         # (default: opens $EDITOR with a stub)
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

# Standard firmware targets (env name in platformio_override.ini). The ESP32
# targets now ship with Matter (USE_MATTER_C) — at ~57 kB it fits where HomeKit
# (~156 kB) did not, so testers can pair over Apple Home / Google / Alexa.
# ESP8266 stays non-Matter (no resident crypto/IPv6 budget).
STD_TARGETS=(
  tinyc32-4M-plain # ESP32 4MB classic WROOM — plain (no Matter / no camera / no HomeKit)
  tinyc32-4M-cam   # ESP32 4MB WROVER + PSRAM — Matter + camera
  tinyc32s3        # ESP32-S3 16MB flash (Matter + camera)
  tinyc32c3        # ESP32-C3           (Matter)
  tinyc32c6        # ESP32-C6           (Matter)
  tinyc8266-4M     # ESP8266 4MB flash  (no Matter)
)

# ─────────── Argument parsing ────────────────────────────────────────────────
SKIP_BUILD=false
SKIP_UPLOAD=false
DRY_RUN=false
NOTES_FILE=""
VERSION_OVERRIDE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)   SKIP_BUILD=true; shift ;;
    --skip-upload)  SKIP_UPLOAD=true; shift ;;
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
  if [[ "$env" == tinyc8266* ]]; then
    # ESP8266: ship .bin and .bin.gz (no factory image)
    [[ -f "$src" ]] || $DRY_RUN || die "Missing $src"
    run "cp '$src' '$STAGE_DIR/'"
    run "gzip -kf -9 '$STAGE_DIR/${env}.bin'"   # produces .bin.gz alongside
  else
    # ESP32 family: ship .bin (OTA) + .factory.bin (esptool / web installer)
    [[ -f "$src" ]] || $DRY_RUN || die "Missing $src"
    [[ -f "${src%.bin}.factory.bin" ]] || $DRY_RUN || die "Missing ${src%.bin}.factory.bin"
    run "cp '$src' '${src%.bin}.factory.bin' '$STAGE_DIR/'"
  fi
done

# IDE + docs
run "cp '$IDE_GZ' '$STAGE_DIR/'"
run "cp '$TINYC_DIR/TinyC_Reference.md'    '$STAGE_DIR/'"
run "cp '$TINYC_DIR/TinyC_Reference_DE.md' '$STAGE_DIR/'"

# ─────────── Build release notes ─────────────────────────────────────────────
NEW_SECTION="$STAGE_DIR/_new_section.md"

if [[ -n "$NOTES_FILE" ]]; then
  [[ -f "$NOTES_FILE" ]] || die "Notes file not found: $NOTES_FILE"
  run "cp '$NOTES_FILE' '$NEW_SECTION'"
else
  # Open $EDITOR with a stub so the user can write the changelog inline.
  STUB="$STAGE_DIR/_stub.md"
  if ! $DRY_RUN; then
    cat > "$STUB" <<EOF
### Changes in v$VERSION ($TODAY):
- **<headline>** — <one paragraph: what changed, why, what the user sees>
EOF
    "${EDITOR:-vi}" "$STUB"
    cp "$STUB" "$NEW_SECTION"
  else
    printf '\033[2m[dry-run]\033[0m would open $EDITOR with stub for v%s notes\n' "$VERSION"
    echo "### Changes in v$VERSION ($TODAY): (stub)" > "$NEW_SECTION" 2>/dev/null || true
  fi
fi

# Prepend the new section to the existing release body to preserve history.
COMBINED_NOTES="$STAGE_DIR/_release_body.md"

if $DRY_RUN; then
  log "Would build release notes from existing body + $NEW_SECTION"
else
  EXISTING_BODY="$(gh release view "$RELEASE_TAG" -R "$RELEASE_REPO" --json body -q .body 2>/dev/null || true)"

  # Header (always replaced — version + date drive it).
  cat > "$COMBINED_NOTES" <<EOF
## TinyC Test Firmware v$VERSION — $TODAY

**For testers only** — may contain experimental features.

### Included targets:
| File | Description |
|------|-------------|
| \`tinyc32-4M-plain.bin\` / \`.factory.bin\` | ESP32 4MB classic WROOM — plain (no Matter / no camera) |
| \`tinyc32-4M-cam.bin\` / \`.factory.bin\` | ESP32 4MB WROVER + PSRAM — **Matter** + camera |
| \`tinyc32s3.bin\` / \`.factory.bin\` | ESP32-S3 — **Matter** + camera |
| \`tinyc32c3.bin\` / \`.factory.bin\` | ESP32-C3 — **Matter** |
| \`tinyc32c6.bin\` / \`.factory.bin\` | ESP32-C6 — **Matter** |
| \`tinyc8266-4M.bin\` / \`.bin.gz\` | ESP8266 4MB flash |
| \`tinyc_ide.html.gz\` | Browser IDE (upload to filesystem) |
| \`TinyC_Reference.md\` / \`_DE.md\` | Documentation EN/DE |

The ESP32 builds include **Matter** (Apple Home / Google / Alexa): a script
defines the device (see \`matter_*.tc\` examples); open pairing from the \`/mt\`
web page (Bind) and add the QR in your controller app.

### How to flash:
- OTA: Firmware Upgrade → Upload \`.bin\` file
- Factory install: Use \`.factory.bin\` with esptool or web installer
- Upload \`tinyc_ide.html.gz\` via Tasmota file manager (Consoles → Manage File System)

EOF
  cat "$NEW_SECTION" >> "$COMBINED_NOTES"
  echo "" >> "$COMBINED_NOTES"

  # Strip the old header (everything up to and including the first "### Changes"
  # line) from the previous body, then append the older changelog tail.
  # ALSO strip any pre-existing section for the current version so re-running
  # the script for the same v$VERSION doesn't produce duplicate entries.
  if [[ -n "$EXISTING_BODY" ]]; then
    OLD_TAIL="$(printf '%s' "$EXISTING_BODY" | awk -v ver="v$VERSION" '
      /^### Changes in v/ { found=1 }
      !found { next }
      {
        # End an active skip when we hit a different ### heading.
        if (skip && /^### / && $0 !~ ("^### Changes in " ver "[ (]")) skip=0
        # Start skipping when we see the duplicate version header.
        if ($0 ~ ("^### Changes in " ver "[ (]")) { skip=1; next }
        if (!skip) print
      }
    ')"
    if [[ -n "$OLD_TAIL" ]]; then
      echo "$OLD_TAIL" >> "$COMBINED_NOTES"
    fi
  fi
fi

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

log "Uploading ${#UPLOAD_ASSETS[@]} asset(s):"
for f in "${UPLOAD_ASSETS[@]}"; do printf '         %s\n' "$(basename "$f")"; done

run "gh release upload '$RELEASE_TAG' -R '$RELEASE_REPO' --clobber ${UPLOAD_ASSETS[@]@Q}"

log "Updating release title + body …"
run "gh release edit '$RELEASE_TAG' -R '$RELEASE_REPO' \
       --title 'TinyC Test Build v$VERSION' \
       --notes-file '$COMBINED_NOTES'"

log "Done. https://github.com/$RELEASE_REPO/releases/tag/$RELEASE_TAG"
