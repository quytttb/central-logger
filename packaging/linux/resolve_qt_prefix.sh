#!/usr/bin/env bash
# Print CMAKE_PREFIX_PATH for aqt-installed Qt 6.11 (install-qt-action / aqt folder layout).
set -euo pipefail

qt_ok() {
  local root="$1"
  [[ -f "${root}/lib/cmake/Qt6/Qt6Config.cmake" ]] \
    && [[ -f "${root}/lib/cmake/Qt6Graphs/Qt6GraphsConfig.cmake" ]] \
    && [[ -f "${root}/lib/cmake/Qt6SerialBus/Qt6SerialBusConfig.cmake" ]] \
    && [[ -f "${root}/lib/cmake/Qt6TaskTree/Qt6TaskTreeConfig.cmake" ]]
}

candidates=()
# aqt installs under gcc_64 even when arch is linux_gcc_64 — prefer that path first
if [[ -n "${RUNNER_WORKSPACE:-}" ]]; then
  candidates+=(
    "${RUNNER_WORKSPACE}/Qt/6.11.1/gcc_64"
    "${RUNNER_WORKSPACE}/Qt/6.11.1/linux_gcc_64"
  )
fi
if [[ -n "${QT_ROOT_DIR:-}" ]]; then
  candidates+=("${QT_ROOT_DIR}")
fi
if [[ -n "${HOME:-}" ]]; then
  candidates+=(
    "${HOME}/Qt/6.11.1/gcc_64"
    "${HOME}/Qt/6.11.1/linux_gcc_64"
  )
fi

for root in "${candidates[@]}"; do
  if qt_ok "${root}"; then
    echo "${root}"
    exit 0
  fi
done

echo "::error::Qt 6.11 install incomplete (need Graphs, SerialBus, TaskTree). QT_ROOT_DIR=${QT_ROOT_DIR:-<unset>}" >&2
for root in "${candidates[@]}"; do
  if [[ -d "${root}" ]]; then
    echo "::warning::inspecting ${root}" >&2
    ls "${root}/lib/cmake" 2>/dev/null | head -20 >&2 || true
  fi
done
exit 1
