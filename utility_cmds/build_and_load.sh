#!/usr/bin/env bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)

bash "${ROOT_DIR}/build.sh"
bash "${SCRIPT_DIR}/build_read_detections.sh" uclibc

# Make all built binaries executable
chmod a+x "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/luckfox_pico_rtsp_yolov5_UAV"
chmod a+x "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/utilities/read_detections"

sshpass -p 'luckfox' scp \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -r "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo" \
    "root@172.32.0.93:/root/"

# Sync system clock on the device to the host's current UTC time
sshpass -p 'luckfox' ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    root@172.32.0.93 \
    "date -s '$(date -u "+%Y-%m-%d %H:%M:%S")'"

# Upload picoclaw binary only if not already present on the device (it is ~22 MB)
PICOCLAW_SRC="${ROOT_DIR}/tools/picoclaw-linux-armv7"
PICOCLAW_DST="/root/picoclaw"
if [ -f "${PICOCLAW_SRC}" ]; then
    sshpass -p 'luckfox' ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        root@172.32.0.93 \
        "test -f ${PICOCLAW_DST}" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "picoclaw not found on device — uploading..."
        sshpass -p 'luckfox' scp \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            "${PICOCLAW_SRC}" "root@172.32.0.93:${PICOCLAW_DST}"
        sshpass -p 'luckfox' ssh \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            root@172.32.0.93 \
            "chmod +x ${PICOCLAW_DST}"
        echo "picoclaw uploaded."
    else
        echo "picoclaw already present on device — skipping upload."
    fi
else
    echo "Warning: ${PICOCLAW_SRC} not found locally — skipping picoclaw upload."
fi