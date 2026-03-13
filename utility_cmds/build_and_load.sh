#!/usr/bin/env bash

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)

# Load picoclaw credentials — use .secret.sh if present, fall back to template
_CREDS_SECRET="${SCRIPT_DIR}/picoclaw/credentials.secret.sh"
_CREDS_TEMPLATE="${SCRIPT_DIR}/picoclaw/credentials.sh"
if [ -f "${_CREDS_SECRET}" ]; then
    source "${_CREDS_SECRET}"
elif [ -f "${_CREDS_TEMPLATE}" ]; then
    source "${_CREDS_TEMPLATE}"
    echo "Warning: using placeholder credentials from credentials.sh — create credentials.secret.sh with real values."
else
    echo "ERROR: no credentials file found."
    exit 1
fi

# Host IP reachable from the device (USB-gadget network gateway)
OLLAMA_HOST="${OLLAMA_HOST:-172.32.0.100}"

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

# Upload CA certificate bundle only if not already present on the device.
# Reads the host system trust store and uploads it as /etc/ssl/cacert.pem.
# Also writes SSL_CERT_FILE to /etc/profile.d so it persists across reboots.
_CA_CANDIDATES=(
    /etc/ssl/certs/ca-certificates.crt   # Debian / Ubuntu
    /etc/pki/tls/certs/ca-bundle.crt     # RHEL / CentOS / Fedora
    /etc/ssl/ca-bundle.pem               # openSUSE
    /etc/ssl/cert.pem                    # Alpine / macOS
)
_CA_BUNDLE=""
for _f in "${_CA_CANDIDATES[@]}"; do
    if [ -f "${_f}" ]; then _CA_BUNDLE="${_f}"; break; fi
done

if [ -z "${_CA_BUNDLE}" ]; then
    echo "Warning: could not locate host CA bundle — skipping cacert upload."
else
    sshpass -p 'luckfox' ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        root@172.32.0.93 \
        "test -f /etc/ssl/cacert.pem" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "cacert.pem not found on device — uploading..."
        sshpass -p 'luckfox' scp \
            -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null \
            "${_CA_BUNDLE}" "root@172.32.0.93:/etc/ssl/cacert.pem"
        echo "cacert.pem uploaded."
    else
        echo "cacert.pem already present on device — skipping."
    fi

    # Persist env vars so every shell session (and picoclaw) picks them up
    sshpass -p 'luckfox' ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        root@172.32.0.93 \
        "mkdir -p /etc/profile.d
         for line in \
             'export SSL_CERT_FILE=/etc/ssl/cacert.pem' \
             'export ALL_PROXY=socks5://127.0.0.1:1080' \
             'export HTTPS_PROXY=socks5://127.0.0.1:1080'; do
             grep -qxF \"\${line}\" /etc/profile.d/picoclaw.sh 2>/dev/null || echo \"\${line}\" >> /etc/profile.d/picoclaw.sh
         done"
fi

# Upload picoclaw config.json only if not already present on the device.
# Credentials are substituted here on the host side before upload.
_CONFIG_TMP="$(mktemp /tmp/picoclaw_config_XXXXXX.json)"
trap 'rm -f "${_CONFIG_TMP}"' EXIT
cat > "${_CONFIG_TMP}" << CONFIG_EOF
{
  "agents": {
    "defaults": {
      "model": "qwen2.5-coder:3b",
      "restrict_to_workspace": false
    }
  },
  "model_list": [
    {
      "model_name": "deepseek-chat",
      "model": "deepseek/deepseek-chat",
      "api_base": "https://api.deepseek.com/v1",
      "api_key": "${PICOCLAW_DEEPSEEK_KEY}"
    },
    {
      "model_name": "qwen2.5-coder:3b",
      "model": "qwen2.5-coder:3b",
      "api_base": "http://${OLLAMA_HOST}:11434/v1",
      "api_key": "ollama"
    }
  ],
  "channels": {
    "telegram": {
      "enabled": true,
      "token": "${PICOCLAW_TELEGRAM_TOKEN}",
      "allow_from": ["${PICOCLAW_TELEGRAM_ALLOW_FROM}"]
    }
  }
}
CONFIG_EOF
sshpass -p 'luckfox' ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    root@172.32.0.93 \
    "test -f /oem/.picoclaw/config.json" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "config.json not found on device — uploading..."
    sshpass -p 'luckfox' ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        root@172.32.0.93 \
        "mkdir -p /oem/.picoclaw"
    sshpass -p 'luckfox' scp \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        "${_CONFIG_TMP}" "root@172.32.0.93:/oem/.picoclaw/config.json"
    echo "config.json uploaded."
else
    echo "config.json already present on device — skipping."
fi