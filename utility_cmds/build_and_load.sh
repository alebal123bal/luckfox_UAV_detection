#!/usr/bin/env bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)

bash "${ROOT_DIR}/build.sh"

sshpass -p 'luckfox' scp -r \
"${ROOT_DIR}/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo" \
"root@172.32.0.93:/root/"