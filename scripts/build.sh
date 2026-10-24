#!/usr/bin/env bash
# Build the firmware locally with Docker, the same way as GitHub Actions.
#
# Usage:
#   scripts/build.sh               Build every target in build.yaml.
#   scripts/build.sh tomkey_L      Build targets whose name contains tomkey_L.
#   scripts/build.sh --update      Also refresh branch-based west projects.
#
# The .uf2 files and build logs are written to firmware/.
#
# The west workspace (ZMK, Zephyr and the modules) lives in the Docker volume
# tomkey-zmk-ws, so only the first run downloads it and later builds are
# incremental. It is refreshed automatically when config/west.yml changes.
# Delete it with: docker volume rm tomkey-zmk-ws
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
image="zmkfirmware/zmk-build-arm:stable"
volume="tomkey-zmk-ws"

mkdir -p "$repo/firmware"

docker run --rm \
    -v "$volume:/ws" \
    -v "$repo:/src:ro" \
    -v "$repo/firmware:/out" \
    "$image" bash /src/scripts/build-in-container.sh "$@"
