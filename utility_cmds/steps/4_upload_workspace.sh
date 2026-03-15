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

# Upload skills folder (each skill is a subdirectory containing SKILL.md)
SKILLS_SRC="${WORKSPACE_SRC}/skills"
SKILLS_DST="${WORKSPACE_DST}/skills"
device_ssh "mkdir -p ${SKILLS_DST}"
for skill_dir in "${SKILLS_SRC}"/*/; do
    skill_name="$(basename "${skill_dir}")"
    skill_file="${skill_dir}SKILL.md"
    if [ ! -f "${skill_file}" ]; then
        continue
    fi
    device_ssh "mkdir -p ${SKILLS_DST}/${skill_name}"
    device_ssh "test -f ${SKILLS_DST}/${skill_name}/SKILL.md" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "${skill_name}/SKILL.md not found on device — uploading..."
        device_scp "${skill_file}" "root@${DEVICE_IP}:${SKILLS_DST}/${skill_name}/SKILL.md"
        echo "${skill_name}/SKILL.md uploaded."
    else
        echo "${skill_name}/SKILL.md already present on device — skipping."
    fi
done
