#!/usr/bin/env bash
# Build Release + CPack .deb (Qt deploy via qt_generate_deploy_app_script).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# Qt deploy RPATH patching fails on very long build paths; keep BUILD short on CI/local.
if [[ -n "${BUILD_DIR:-}" ]]; then
  BUILD="$BUILD_DIR"
elif [[ -n "${RUNNER_TEMP:-}" ]]; then
  BUILD="${RUNNER_TEMP}/central-logger-deb"
else
  BUILD="/tmp/central-logger-deb-$$"
fi
DIST="${ROOT}/dist"
rm -rf "$BUILD"
mkdir -p "$DIST"

cmake_args=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX=/usr
  -DCMAKE_INSTALL_LIBDIR=lib
)
if [[ -f "${ROOT}/.ci_qt_root" ]]; then
  export QT_ROOT_DIR="$(tr -d '\r\n' < "${ROOT}/.ci_qt_root")"
fi
if [[ -f "${ROOT}/packaging/linux/ci_configure.sh" ]] \
    && [[ -n "${QT_ROOT_DIR:-}" ]] \
    && [[ -f "${QT_ROOT_DIR}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  CMAKE_BUILD_TYPE=Release bash "${ROOT}/packaging/linux/ci_configure.sh" "$BUILD"
else
  if [[ -n "${QT_ROOT_DIR:-}" ]]; then
    cmake_args+=(-DCMAKE_PREFIX_PATH="${QT_ROOT_DIR}" -DQt6_DIR="${QT_ROOT_DIR}/lib/cmake/Qt6")
  fi
  cmake -S "$ROOT" -B "$BUILD" "${cmake_args[@]}"
fi

cmake --build "$BUILD" -j"$(nproc)"

# Let CPack run install()+deploy into package staging (avoids manual DESTDIR + qt.conf issues).
( cd "$BUILD" && cpack -G DEB )

shopt -s nullglob
for deb in "$BUILD"/*.deb; do
  cp -f "$deb" "$DIST/"
  echo "Built $DIST/$(basename "$deb")"
done
