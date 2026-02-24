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