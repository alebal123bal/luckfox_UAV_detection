#!/usr/bin/env bash
# Step 3: Upload picoclaw binary (skips if already present — it is ~22 MB).
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/_common.sh"
fi

PICOCLAW_SRC="${ROOT_DIR}/tools/picoclaw-linux-armv7"
PICOCLAW_DST="/root/picoclaw"

if [ -f "${PICOCLAW_SRC}" ]; then
    device_ssh "test -f ${PICOCLAW_DST}" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "picoclaw not found on device — uploading..."
        device_scp "${PICOCLAW_SRC}" "root@${DEVICE_IP}:${PICOCLAW_DST}"
        device_ssh "chmod +x ${PICOCLAW_DST}"
        echo "picoclaw uploaded."
    else
        echo "picoclaw already present on device — skipping upload."
    fi
else
    echo "Warning: ${PICOCLAW_SRC} not found locally — skipping picoclaw upload."
fi
