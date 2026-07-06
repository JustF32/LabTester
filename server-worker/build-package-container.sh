#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if command -v podman >/dev/null 2>&1; then
    ENGINE="podman"
elif command -v docker >/dev/null 2>&1; then
    ENGINE="docker"
else
    echo "Neither podman nor docker was found." >&2
    exit 1
fi

"${ENGINE}" run --rm \
    -v "${PROJECT_DIR}:/work" \
    -w /work \
    ubuntu:22.04 \
    bash -lc '
        set -euo pipefail
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends \
            ca-certificates \
            build-essential \
            cmake \
            ninja-build \
            pkg-config \
            qt6-base-dev
        chmod +x server-worker/build-package.sh
        ./server-worker/build-package.sh
    '
