#!/bin/bash
#
# Upload test firmware binaries to GitHub Release (rolling "testing" tag)
# Replaces previous release — only the latest binaries are kept.
#
# Usage:
#   ./upload_test_binaries.sh                    # upload all standard targets
#   ./upload_test_binaries.sh tasmota32-DE       # upload specific target(s)
#   ./upload_test_binaries.sh --list             # show available binaries
#

REPO="gemu2015/Sonoff-Tasmota"
TAG="testing"
TITLE="TinyC Test Build"
FW_DIR="$(cd "$(dirname "$0")" && pwd)/build_output/firmware"

# Standard TinyC test targets (without .bin extension)
DEFAULT_TARGETS=(
  tinyc8266-4M
  tinyc32-4M
  tinyc32s3
  tinyc32c3
)

# --- helpers ---
die()  { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

# --- list mode ---
if [[ "$1" == "--list" ]]; then
  echo "Available firmware binaries in $FW_DIR:"
  echo ""
  ls -lh "$FW_DIR"/*.bin 2>/dev/null | awk '{printf "  %-40s %s\n", $NF, $5}'
  exit 0
fi

# --- check prerequisites ---
command -v gh >/dev/null 2>&1 || die "gh CLI not installed. Run: brew install gh"
gh auth status >/dev/null 2>&1 || die "Not logged in. Run: gh auth login"
[[ -d "$FW_DIR" ]] || die "Firmware directory not found: $FW_DIR"

# --- determine targets ---
if [[ $# -gt 0 ]]; then
  TARGETS=("$@")
else
  TARGETS=("${DEFAULT_TARGETS[@]}")
fi

# --- collect files to upload ---
FILES=()
for target in "${TARGETS[@]}"; do
  bin="$FW_DIR/${target}.bin"
  bingz="$FW_DIR/${target}.bin.gz"
  factory="$FW_DIR/${target}.factory.bin"
  if [[ "$target" == *8266* ]] && [[ -f "$bingz" ]]; then
    # ESP8266: use compressed .bin.gz (uncompressed may be too large for OTA)
    FILES+=("$bingz")
  elif [[ -f "$bin" ]]; then
    FILES+=("$bin")
    # include .factory.bin if it exists
    [[ -f "$factory" ]] && FILES+=("$factory")
  else
    echo "WARNING: $bin not found, skipping"
  fi
done

[[ ${#FILES[@]} -eq 0 ]] && die "No firmware files found to upload"

# --- include TinyC IDE ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IDE_FILE="/Volumes/vp_dev/TinyC/tinyc_ide.html.gz"
[[ -f "$IDE_FILE" ]] && FILES+=("$IDE_FILE") || echo "WARNING: TinyC IDE not found at $IDE_FILE"

# --- include documentation ---
TINYC_DIR="/Volumes/vp_dev/TinyC"
for doc in README.md TinyC_Reference.md TinyC_Reference_DE.md; do
  [[ -f "$TINYC_DIR/$doc" ]] && FILES+=("$TINYC_DIR/$doc") || echo "WARNING: $doc not found"
done

# --- show what will be uploaded ---
info "Files to upload:"
for f in "${FILES[@]}"; do
  size=$(ls -lh "$f" | awk '{print $5}')
  echo "  $(basename "$f")  ($size)"
done
echo ""

# --- build release notes ---
DATE=$(date "+%Y-%m-%d %H:%M")
NOTES="## TinyC Test Firmware — $DATE

**For testers only** — may contain experimental features.

### Included targets:
$(for f in "${FILES[@]}"; do echo "- \`$(basename "$f")\`"; done)

### How to flash:
- OTA: Firmware Upgrade → Upload \`.bin\` file
- Factory install: Use \`.factory.bin\` with esptool or web installer
- Upload \`tinyc_ide.html.gz\` via Tasmota file manager (Consoles → Manage File System)

### Changes since last release:
- \`addCommand()\` / \`responseCmnd()\` — custom console command callbacks
- \`Command(char cmd[])\` callback — scripts can handle Tasmota console commands
- \`OnExit()\` / \`Event()\` callbacks — script stop cleanup and rule event handling
- \`webPulldown(var, "label", "options")\` — now with label parameter
- \`@getfreepins\` — dynamic GPIO pin picker in webPulldown
- \`strFind()\` / \`responseCmnd()\` with string literals — auto-detected const variants
- \`strSub()\` fix — \`len=0\` now correctly copies to end of string
- DY-SV17F MP3 player driver example with serial TX and console commands
- I2C sensor examples: VL53L0X, MLX90614, TCS34725, VEML6075
- \`i2cReadRS()\` — repeated START I2C read for sensors that require it
- \`WebChart()\` — automatic Google Charts rendering with Y-axis min/max range control
- Computed goto VM dispatch — ~10% faster bytecode execution
- Multi-VM slots (up to 4 concurrent scripts)
- Pin safety: forbidden pins halt VM immediately
- Boot loop protection: autoexec disabled after 4 rapid reboots
- Deep sleep support: \`deepSleep()\`, \`deepSleepGpio()\`, \`wakeupCause()\`
- Email with file attachments: \`mailSend()\`, \`mailBody()\`, \`mailAttach()\`
- Portable bytecode — compile once in the browser, run on any ESP target
- Full documentation included (README + Reference EN/DE)
"

# --- delete old release if exists ---
info "Removing old '$TAG' release (if any)..."
gh release delete "$TAG" --repo "$REPO" --yes --cleanup-tag 2>/dev/null || true

# --- create new release ---
info "Creating release '$TAG' with ${#FILES[@]} files..."
gh release create "$TAG" \
  --repo "$REPO" \
  --title "$TITLE" \
  --notes "$NOTES" \
  --prerelease \
  "${FILES[@]}"

if [[ $? -eq 0 ]]; then
  URL="https://github.com/$REPO/releases/tag/$TAG"
  echo ""
  info "Done! Release URL:"
  echo "  $URL"
else
  die "Failed to create release"
fi
