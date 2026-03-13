#!/usr/bin/env bash
# Step 2: chmod binaries, scp app to device, sync device clock.
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

_RD_OUT="${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/utilities"

chmod a+x "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/luckfox_pico_rtsp_yolov5_UAV"
chmod a+x "${_RD_OUT}/read_detections"

device_scp -r \
    "${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo" \
    "root@${DEVICE_IP}:/root/"

# Sync system clock on the device to the host's current UTC time
device_ssh "date -s '$(date -u "+%Y-%m-%d %H:%M:%S")'"
