#!/usr/bin/env bash
# Configure, build, and install the SuperCollider plugins into the user
# Extensions folder.
#
# Usage:
#   ./build.sh [SC_PATH]
#   SC_PATH=/path/to/supercollider ./build.sh
#
# SC_PATH must point at a SuperCollider source tree whose plugin ABI
# (sc_api_version) matches the scsynth you run: v3 for SC 3.12-3.14.x, v6 for
# develop/3.15. The default is a worktree of the 3.14.1 tag:
#   git -C /path/to/supercollider worktree add --detach ../supercollider-3.14.1 Version-3.14.1

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SC_PATH="${1:-${SC_PATH:-${HERE}/../../supercollider-3.14.1}}"
BUILD_DIR="${HERE}/build"

echo "==> SuperCollider source: ${SC_PATH}"
echo "==> Build dir:            ${BUILD_DIR}"

cmake -S "${HERE}" -B "${BUILD_DIR}" \
    -DSC_PATH="${SC_PATH}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSUPERNOVA=OFF

cmake --build "${BUILD_DIR}" --config Release -j
cmake --install "${BUILD_DIR}"

echo
echo "==> Built artifacts:"
find "${BUILD_DIR}" -name '*.scx' -o -name '*.so' | grep -v CMakeFiles || true
echo "Installed. In SuperCollider: recompile the class library (Cmd/Ctrl-Shift-L), then reboot the server."
