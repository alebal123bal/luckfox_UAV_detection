#!/usr/bin/env bash
# Step 7: Configure DNS resolver on the device.
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

echo "Setting DNS nameserver on device..."
device_ssh "echo 'nameserver 8.8.8.8' > /etc/resolv.conf"
echo "DNS configured."