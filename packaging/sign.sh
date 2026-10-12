#!/usr/bin/env bash
# Sign a PE file (exe/dll/installer) in place with osslsigncode.
# Usage: sign.sh <file> [more files...]
#
# Config via environment:
#   SIGN_PFX       path to pfx        (default: ~/.codesign/codesign.pfx)
#   SIGN_PASSWORD  pfx password      (required)
#   SIGN_NAME      program name shown in the signature
#   SIGN_URL       project URL shown in the signature
#   SIGN_DISABLE   set to 1 to make this a no-op (unsigned dev builds)
#
# This script is the ONLY place that knows how signing happens. When you
# later move to Azure Artifact Signing / a real OV cert, you change this
# file and nothing else in the build.
set -euo pipefail

[[ "${SIGN_DISABLE:-0}" == "1" ]] && { echo "sign.sh: signing disabled"; exit 0; }

SIGN_PFX="${SIGN_PFX:-$HOME/.codesign/lammps-gui.pfx}"
SIGN_NAME="${SIGN_NAME:-LAMMPS-GUI}"
SIGN_URL="${SIGN_URL:-https://lammps-gui.lammps.org}"
TSA="http://timestamp.digicert.com"   # free RFC-3161 timestamp server

if [[ -z "${SIGN_PASSWORD:-}" ]]; then
    echo "sign.sh: SIGN_PASSWORD is not set" >&2
    exit 1
fi

command -v osslsigncode >/dev/null || {
    echo "sign.sh: osslsigncode not found (apt install osslsigncode)" >&2
    exit 1
}

for f in "$@"; do
    tmp="$(mktemp --tmpdir="$(dirname "$f")" "$(basename "$f").XXXX.signed")"
    rm -f ${tmp}

    # Timestamping works fine with self-signed certs; it means the
    # signature stays valid after the cert expires.
    osslsigncode sign \
        -pkcs12 "$SIGN_PFX" -pass "$SIGN_PASSWORD" \
        -n "$SIGN_NAME" -i "$SIGN_URL" \
        -h sha256 -ts "$TSA" \
        -in "$f" -out "$tmp"

    mv -f "$tmp" "$f"
    osslsigncode verify -in "$f" >/dev/null 2>&1 || true
    echo "signed: $f"
done
