#!/usr/bin/env bash
# Opens a reverse SOCKS tunnel so the device can reach the internet through the host.
# Usage: bash utility_cmds/tunnel.sh
# The device can then use socks5://127.0.0.1:1080 as a proxy.

sshpass -p 'luckfox' ssh \
    -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null \
    -R 1080 \
    root@172.32.0.93 \
    -N
