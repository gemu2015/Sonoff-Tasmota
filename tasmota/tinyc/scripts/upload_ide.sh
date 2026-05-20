#!/bin/bash
# Upload tinyc_ide.html.gz to Tasmota device filesystem
# Usage: ./upload_ide.sh [device_ip]
#   device_ip defaults to 192.168.188.128

DEVICE_IP="${1:-192.168.188.128}"
FILE="$(dirname "$0")/tinyc_ide.html.gz"

if [ ! -f "$FILE" ]; then
    echo "ERROR: $FILE not found"
    echo "Run bundle.py first"
    exit 1
fi

SIZE=$(stat -f%z "$FILE" 2>/dev/null || stat -c%s "$FILE" 2>/dev/null)
echo "Uploading tinyc_ide.html.gz ($SIZE bytes) to $DEVICE_IP ..."

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "http://${DEVICE_IP}/ufsu?fsz=${SIZE}" \
    -F "ufsu=@${FILE};filename=tinyc_ide.html.gz")

if [ "$HTTP_CODE" = "200" ] || [ "$HTTP_CODE" = "303" ]; then
    echo "OK — uploaded to http://${DEVICE_IP}"
else
    echo "FAILED — HTTP $HTTP_CODE"
    exit 1
fi
