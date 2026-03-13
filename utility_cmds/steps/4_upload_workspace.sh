#!/usr/bin/env bash
# Step 4: Upload picoclaw workspace markdown files (skips files already present).
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

WORKSPACE_SRC="${ROOT_DIR}/tools/picoclaw/workspace"
WORKSPACE_DST="/oem/.picoclaw/workspace"

device_ssh "mkdir -p ${WORKSPACE_DST}"

for md in AGENT.md IDENTITY.md SOUL.md USER.md; do
    device_ssh "test -f ${WORKSPACE_DST}/${md}" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "${md} not found on device — uploading..."
        device_scp "${WORKSPACE_SRC}/${md}" "root@${DEVICE_IP}:${WORKSPACE_DST}/${md}"
        echo "${md} uploaded."
    else
        echo "${md} already present on device — skipping."
    fi
done
