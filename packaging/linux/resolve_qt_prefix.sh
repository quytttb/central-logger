#!/usr/bin/env bash
# Print CMAKE_PREFIX_PATH for aqt-installed Qt 6.11 (install-qt-action / aqt folder layout).
set -euo pipefail

candidates=()
if [[ -n "${QT_ROOT_DIR:-}" ]]; then
  candidates+=("${QT_ROOT_DIR}")
fi
if [[ -n "${RUNNER_WORKSPACE:-}" ]]; then
  candidates+=(
    "${RUNNER_WORKSPACE}/Qt/6.11.1/linux_gcc_64"
    "${RUNNER_WORKSPACE}/Qt/6.11.1/gcc_64"
  )
fi
if [[ -n "${HOME:-}" ]]; then
  candidates+=(
    "${HOME}/Qt/6.11.1/linux_gcc_64"
    "${HOME}/Qt/6.11.1/gcc_64"
  )
fi

for root in "${candidates[@]}"; do
  if [[ -f "${root}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "${root}"
    exit 0
  fi
done

echo "::error::Qt 6.11 install not found. QT_ROOT_DIR=${QT_ROOT_DIR:-<unset>}" >&2
for root in "${candidates[@]}"; do
  [[ -d "${root}" ]] && ls -la "${root}" >&2 || true
done
exit 1
