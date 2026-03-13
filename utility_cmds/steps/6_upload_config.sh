#!/usr/bin/env bash
# Step 6: Render and upload picoclaw config.json (skips if already present).
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

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

device_ssh "test -f /oem/.picoclaw/config.json" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "config.json not found on device — uploading..."
    device_ssh "mkdir -p /oem/.picoclaw"
    device_scp "${_CONFIG_TMP}" "root@${DEVICE_IP}:/oem/.picoclaw/config.json"
    echo "config.json uploaded."
else
    echo "config.json already present on device — skipping."
fi
