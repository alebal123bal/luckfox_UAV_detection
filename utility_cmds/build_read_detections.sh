#!/bin/bash
# build_read_detections.sh
# Cross-compile the read_detections utility for the Luckfox Pico.
# Output is placed in utility_cmds/bin/ and then scp'd to the device.
#
# Usage:
#   ./utility_cmds/build_read_detections.sh [uclibc|glibc]
#
# Environment:
#   GLIBC_COMPILER    – required for glibc  (same as the main build)

set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
OUT_DIR="$ROOT_DIR/install/uclibc/luckfox_pico_rtsp_yolov5_UAV_demo/utilities"
SRC="$ROOT_DIR/utility_srcs/read_detections.c"
BINARY="read_detections"

LIBC_TYPE="${1:-}"

# ── Interactive selection if no argument given ────────────────────────────────
if [[ -z "$LIBC_TYPE" ]]; then
    options=("uclibc" "glibc")
    PS3="Select libc type [1-2]: "
    select opt in "${options[@]}"; do
        if [[ -n "$opt" ]]; then
            LIBC_TYPE="$opt"
            break
        fi
        echo "Invalid selection."
    done
fi

# ── Pick cross-compiler ───────────────────────────────────────────────────────
if [[ "$LIBC_TYPE" == "uclibc" ]]; then
    SDK_PATH="${LUCKFOX_SDK_PATH:-}"
    CC="${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc"
elif [[ "$LIBC_TYPE" == "glibc" ]]; then
    GLIBC_COMPILER="${GLIBC_COMPILER:-}"
    if [[ -z "$GLIBC_COMPILER" ]]; then
        echo "ERROR: GLIBC_COMPILER is not set."
        echo "       export GLIBC_COMPILER=/opt/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-"
        exit 1
    fi
    CC="${GLIBC_COMPILER}gcc"
else
    echo "Unknown libc type: $LIBC_TYPE"
    exit 1
fi

if [[ ! -x "$CC" ]]; then
    echo "ERROR: Compiler not found or not executable: $CC"
    exit 1
fi

mkdir -p "$OUT_DIR"

echo "Building $BINARY for $LIBC_TYPE ..."
"$CC" \
    -O2 -Wall -pthread \
    -o "$OUT_DIR/$BINARY" \
    "$SRC"

echo ""
echo "Built: $OUT_DIR/$BINARY"
echo ""
echo "Deploy and run on the device:"
echo "  scp $OUT_DIR/$BINARY root@<device_ip>:/userdata/"
echo "  ssh root@<device_ip> /userdata/$BINARY --stats"
echo "  ssh root@<device_ip> /userdata/$BINARY --tail 50"
echo "  ssh root@<device_ip> /userdata/$BINARY -c /tmp/export.csv"
