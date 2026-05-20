#!/bin/bash
# Compile a .tc file and push the .tcb to a Tasmota device
# Usage: push_tcb.sh <file.tc> [device_ip]
set -e

DEVICE_IP="${2:-192.168.188.128}"
INPUT="${1:?Usage: push_tcb.sh <file.tc> [device_ip]}"
BASENAME=$(basename "$INPUT" .tc)
TCB="/tmp/${BASENAME}.tcb"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Compile
echo "Compiling $INPUT ..."
node "$SCRIPT_DIR/compile_cli.js" "$INPUT" "$TCB"

# Upload
SIZE=$(stat -f%z "$TCB" 2>/dev/null || stat -c%s "$TCB" 2>/dev/null)
echo "Uploading ${BASENAME}.tcb ($SIZE bytes) to $DEVICE_IP ..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "http://${DEVICE_IP}/ufsu?fsz=${SIZE}" \
    -F "ufsu=@${TCB};filename=${BASENAME}.tcb")
if [ "$HTTP_CODE" != "200" ]; then
    echo "Upload failed (HTTP $HTTP_CODE)"
    exit 1
fi

# Stop any running program
echo "Stopping current program ..."
curl -s "http://${DEVICE_IP}/tc_api?cmd=stop" -o /dev/null

# Load from filesystem (web page handler)
echo "Loading /${BASENAME}.tcb ..."
curl -s "http://${DEVICE_IP}/tc?cmd=load&file=/${BASENAME}.tcb" -o /dev/null

# Run
echo "Running /${BASENAME}.tcb ..."
RESULT=$(curl -s "http://${DEVICE_IP}/tc_api?cmd=run")
echo "$RESULT"
echo "Done."
