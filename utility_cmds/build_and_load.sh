#!/usr/bin/env bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)

bash "${ROOT_DIR}/build.sh"

# Cross-compile read_detections for uclibc
_RD_OUT="${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/utilities"
_RD_SRC="${ROOT_DIR}/utility_srcs/read_detections.c"
_RD_CC="${LUCKFOX_SDK_PATH:-}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc"
if [[ ! -x "${_RD_CC}" ]]; then
    echo "ERROR: uclibc cross-compiler not found: ${_RD_CC}"
    exit 1
fi
mkdir -p "${_RD_OUT}"
echo "Building read_detections (uclibc)..."
"${_RD_CC}" -O2 -Wall -pthread -o "${_RD_OUT}/read_detections" "${_RD_SRC}"

# Make all built binaries executable
chmod a+x "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/luckfox_pico_rtsp_yolov5_UAV"
chmod a+x "${_RD_OUT}/read_detections"

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

# Upload picoclaw workspace markdown files only if not already present on the device
WORKSPACE_SRC="${SCRIPT_DIR}/picoclaw/workspace"
WORKSPACE_DST="/oem/.picoclaw/workspace"
sshpass -p 'luckfox' ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    root@172.32.0.93 \
    "mkdir -p ${WORKSPACE_DST}"
for md in AGENT.md IDENTITY.md SOUL.md USER.md; do
    sshpass -p 'luckfox' ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        root@172.32.0.93 \
        "test -f ${WORKSPACE_DST}/${md}" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "${md} not found on device — uploading..."
        sshpass -p 'luckfox' scp \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            "${WORKSPACE_SRC}/${md}" "root@172.32.0.93:${WORKSPACE_DST}/${md}"
        echo "${md} uploaded."
    else
        echo "${md} already present on device — skipping."
    fi
done