#!/usr/bin/env bash
# Shared configuration and helper functions for all utility scripts.
# Source this file; do not execute it directly.

_COMMON_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SCRIPT_DIR=$(cd "${_COMMON_DIR}/.." && pwd)
ROOT_DIR=$(cd "${_COMMON_DIR}/../.." && pwd)

# Device connection
DEVICE_IP="${DEVICE_IP:-172.32.0.93}"
DEVICE_PASS="luckfox"
SSH_OPTS=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

# Host IP reachable from the device (USB-gadget network gateway)
OLLAMA_HOST="${OLLAMA_HOST:-172.32.0.100}"

# Load picoclaw credentials — use .secret.sh if present, fall back to template
_CREDS_SECRET="${SCRIPT_DIR}/credentials/credentials.secret.sh"
_CREDS_TEMPLATE="${SCRIPT_DIR}/credentials/credentials.sh"
if [ -f "${_CREDS_SECRET}" ]; then
    source "${_CREDS_SECRET}"
elif [ -f "${_CREDS_TEMPLATE}" ]; then
    source "${_CREDS_TEMPLATE}"
    echo "Warning: using placeholder credentials from credentials.sh — create credentials.secret.sh with real values."
else
    echo "ERROR: no credentials file found."
    exit 1
fi

device_ssh() {
    sshpass -p "${DEVICE_PASS}" ssh "${SSH_OPTS[@]}" "root@${DEVICE_IP}" "$@"
}

device_scp() {
    sshpass -p "${DEVICE_PASS}" scp "${SSH_OPTS[@]}" "$@"
}

_COMMON_LOADED=1
