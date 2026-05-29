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

cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib

cmake --build "$BUILD" -j"$(nproc)"

# Let CPack run install()+deploy into package staging (avoids manual DESTDIR + qt.conf issues).
( cd "$BUILD" && cpack -G DEB )

shopt -s nullglob
for deb in "$BUILD"/*.deb; do
  cp -f "$deb" "$DIST/"
  echo "Built $DIST/$(basename "$deb")"
done
