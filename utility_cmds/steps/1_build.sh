#!/usr/bin/env bash
# Step 1: CMake build + cross-compile read_detections for uclibc.
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

bash "${ROOT_DIR}/build.sh"

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
