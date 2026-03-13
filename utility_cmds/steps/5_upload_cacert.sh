#!/usr/bin/env bash
# Step 5: Upload CA certificate bundle and persist proxy env vars on device.
# Can be run standalone or sourced from build_and_load.sh.

if [[ -z "${_COMMON_LOADED:-}" ]]; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/common/common.sh"
fi

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
    device_ssh "test -f /etc/ssl/cacert.pem" 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "cacert.pem not found on device — uploading..."
        device_scp "${_CA_BUNDLE}" "root@${DEVICE_IP}:/etc/ssl/cacert.pem"
        echo "cacert.pem uploaded."
    else
        echo "cacert.pem already present on device — skipping."
    fi

    # Persist env vars so every shell session (and picoclaw) picks them up
    device_ssh \
        "mkdir -p /etc/profile.d
         for line in \
             'export SSL_CERT_FILE=/etc/ssl/cacert.pem' \
             'export ALL_PROXY=socks5://127.0.0.1:1080' \
             'export HTTPS_PROXY=socks5://127.0.0.1:1080'; do
             grep -qxF \"\${line}\" /etc/profile.d/picoclaw.sh 2>/dev/null || echo \"\${line}\" >> /etc/profile.d/picoclaw.sh
         done"
fi
