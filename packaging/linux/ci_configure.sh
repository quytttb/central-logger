#!/usr/bin/env bash
# Configure central_logger for CI — must use aqt Qt 6.11, not runner Qt 6.10.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${1:-${root}/build}"

qt_prefix=""
if [[ -n "${QT_ROOT_DIR:-}" && -f "${QT_ROOT_DIR}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  qt_prefix="${QT_ROOT_DIR}"
elif [[ -f "${root}/.ci_qt_root" ]]; then
  qt_prefix="$(tr -d '\r\n' < "${root}/.ci_qt_root")"
fi

if [[ -z "${qt_prefix}" || ! -f "${qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo "::error::Qt 6.11 prefix not found (set QT_ROOT_DIR or run install_qt_aqt.sh)" >&2
  exit 1
fi

qmake="${qt_prefix}/bin/qmake6"
if [[ ! -x "${qmake}" ]]; then
  qmake="${qt_prefix}/bin/qmake"
fi
if [[ ! -x "${qmake}" ]]; then
  echo "::error::No qmake under ${qt_prefix}/bin" >&2
  exit 1
fi

qt_version="$("${qmake}" -query QT_VERSION)"
echo "qmake=${qmake}"
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

export PATH="${qt_prefix}/bin:${PATH}"
export CMAKE_IGNORE_PATH="/usr/lib/x86_64-linux-gnu/cmake;/lib/x86_64-linux-gnu/cmake"

cmake --version
cmake -S "${root}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
  -DCMAKE_PREFIX_PATH="${qt_prefix}" \
  -DQt6_DIR="${qt_prefix}/lib/cmake/Qt6"
