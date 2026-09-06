#!/usr/bin/env bash
# Build the Central Logger application.
# Usage: ./build.sh [Debug|Release] [-- extra cmake args]
set -euo pipefail

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="${BUILD_DIR:-build-${BUILD_TYPE,,}}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
QT_DIR="${QT_DIR:-$HOME/Qt/6.11.2/gcc_64}"

if [[ ! -f "$QT_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "Error: Qt not found at $QT_DIR" >&2
    echo "Set QT_DIR to your Qt 6.11 kit, for example:" >&2
    echo "  export QT_DIR=\$HOME/Qt/6.11.2/gcc_64" >&2
    exit 1
fi

# CMake refuses a cache copied from another checkout path. Keep the old build
# as a timestamped backup and configure a clean directory automatically.
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    CACHED_SOURCE=""
    while IFS='=' read -r key value; do
        if [[ "$key" == CMAKE_HOME_DIRECTORY:* ]]; then
            CACHED_SOURCE="$value"
            break
        fi
    done < "$BUILD_DIR/CMakeCache.txt"

    if [[ -n "$CACHED_SOURCE" && "$CACHED_SOURCE" != "$ROOT" ]]; then
        BACKUP_DIR="${BUILD_DIR}.stale-$(date +%Y%m%d%H%M%S)"
        echo "Warning: $BUILD_DIR belongs to $CACHED_SOURCE"
        echo "Moving stale build directory to $BACKUP_DIR"
        mv "$BUILD_DIR" "$BACKUP_DIR"
    fi
fi

cmake -S "$ROOT" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_PREFIX_PATH="$QT_DIR" \
      "${@:2}"

cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "Build complete: $BUILD_DIR/bin/central_logger"
