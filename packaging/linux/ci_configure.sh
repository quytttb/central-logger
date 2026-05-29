#!/usr/bin/env bash
# Configure central_logger for CI — must use aqt Qt 6.11, not runner Qt 6.10.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${1:-${root}/build}"

cmake --version
command -v qmake6 >/dev/null || { echo "::error::qmake6 not on PATH after Qt install"; exit 1; }

qt_prefix="$(qmake6 -query QT_INSTALL_PREFIX)"
qt_version="$(qmake6 -query QT_VERSION)"
echo "qmake6=$(command -v qmake6)"
echo "QT_VERSION=${qt_version}"
echo "QT_PREFIX=${qt_prefix}"

case "${qt_version}" in
  6.11.*|6.12.*|6.13.*) ;;
  *)
    echo "::error::Need Qt 6.11+ from aqt; got ${qt_version} at ${qt_prefix}" >&2
    exit 1
    ;;
esac

for pkg in Qt6Graphs Qt6SerialBus Qt6TaskTree; do
  if [[ ! -f "${qt_prefix}/lib/cmake/${pkg}/${pkg}Config.cmake" ]]; then
    echo "::error::Missing ${pkg} under ${qt_prefix}" >&2
    exit 1
  fi
done

# Block distro Qt 6.10 cmake packages when CMAKE_PREFIX_PATH is ignored.
export CMAKE_IGNORE_PATH="/usr/lib/x86_64-linux-gnu/cmake;/lib/x86_64-linux-gnu/cmake"

cmake -S "${root}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
  -DCMAKE_PREFIX_PATH="${qt_prefix}" \
  -DQt6_DIR="${qt_prefix}/lib/cmake/Qt6"
