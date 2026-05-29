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

if command -v qmake6 >/dev/null 2>&1; then
  qroot="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  [[ -n "${qroot}" ]] && candidates+=("${qroot}")
fi

if [[ -n "${GITHUB_WORKSPACE:-}" ]]; then
  ws_parent="$(cd "${GITHUB_WORKSPACE}/.." && pwd)"
  candidates+=(
    "${ws_parent}/Qt/6.11.1/gcc_64"
    "${ws_parent}/Qt/6.11.1/linux_gcc_64"
  )
fi

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

seen=""
for root in "${candidates[@]}"; do
  [[ -z "${root}" ]] && continue
  [[ " ${seen} " == *" ${root} "* ]] && continue
  seen+=" ${root}"
  if qt_ok "${root}"; then
    echo "${root}"
    exit 0
  fi
done

echo "::error::Qt 6.11 install incomplete (need Graphs, SerialBus, TaskTree). QT_ROOT_DIR=${QT_ROOT_DIR:-<unset>}" >&2
if command -v qmake6 >/dev/null 2>&1; then
  echo "::error::qmake6=$(command -v qmake6) prefix=$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || echo '?')" >&2
fi
exit 1
