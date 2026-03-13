#!/usr/bin/env bash
# Orchestrator: build, deploy, and configure the device end-to-end.
# Each step can also be run standalone from utility_cmds/steps/.

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/common/common.sh"

source "${SCRIPT_DIR}/steps/1_build.sh"
source "${SCRIPT_DIR}/steps/2_upload_app.sh"
source "${SCRIPT_DIR}/steps/3_upload_picoclaw.sh"
source "${SCRIPT_DIR}/steps/4_upload_workspace.sh"
source "${SCRIPT_DIR}/steps/5_upload_cacert.sh"
source "${SCRIPT_DIR}/steps/6_upload_config.sh"