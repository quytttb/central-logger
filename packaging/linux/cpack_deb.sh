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
if [[ -x "${ROOT}/packaging/linux/ci_configure.sh" ]] && command -v qmake6 >/dev/null 2>&1; then
  CMAKE_BUILD_TYPE=Release bash "${ROOT}/packaging/linux/ci_configure.sh" "$BUILD"
else
  if [[ -f "${ROOT}/.ci_qt_root" ]]; then
    QT_ROOT_DIR="$(cat "${ROOT}/.ci_qt_root")"
  fi
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
