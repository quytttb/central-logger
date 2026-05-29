#!/usr/bin/env bash
# Configure central_logger for CI — must use aqt Qt 6.11, not runner Qt 6.10.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${1:-${root}/build}"

discover_qt_prefix() {
  local candidates=()
  if [[ -n "${QT_ROOT_DIR:-}" ]]; then
    candidates+=("${QT_ROOT_DIR}")
    candidates+=("$(dirname "${QT_ROOT_DIR}")/gcc_64")
    candidates+=("$(dirname "${QT_ROOT_DIR}")/linux_gcc_64")
  fi
  if [[ -f "${root}/.ci_qt_root" ]]; then
    candidates+=("$(tr -d '\r\n' < "${root}/.ci_qt_root")")
  fi
  if [[ -n "${GITHUB_WORKSPACE:-}" ]]; then
    local qt_base
    qt_base="$(cd "${GITHUB_WORKSPACE}/.." && pwd)/Qt/6.11.1"
    candidates+=("${qt_base}/gcc_64" "${qt_base}/linux_gcc_64")
  fi
  local c seen=""
  for c in "${candidates[@]}"; do
    [[ -z "${c}" ]] && continue
    [[ " ${seen} " == *" ${c} "* ]] && continue
    seen+=" ${c}"
    if [[ -f "${c}/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
      echo "${c}"
      return 0
    fi
  done
  return 1
}

if ! qt_prefix="$(discover_qt_prefix)"; then
  echo "::error::Qt 6.11 prefix not found (QT_ROOT_DIR=${QT_ROOT_DIR:-unset})" >&2
  ls -la "${GITHUB_WORKSPACE:-.}/../Qt/6.11.1" 2>/dev/null >&2 || true
  exit 1
fi

if [[ -n "${GITHUB_WORKSPACE:-}" ]]; then
  echo "${qt_prefix}" > "${GITHUB_WORKSPACE}/.ci_qt_root"
fi

qmake="${qt_prefix}/bin/qmake6"
[[ -x "${qmake}" ]] || qmake="${qt_prefix}/bin/qmake"
if [[ ! -x "${qmake}" ]]; then
  echo "::error::No qmake in ${qt_prefix}/bin" >&2
  exit 1
fi

qt_version="$("${qmake}" -query QT_VERSION)"
echo "::notice::Using Qt ${qt_version} at ${qt_prefix}"
case "${qt_version}" in
  6.11.*|6.12.*|6.13.*) ;;
  *)
    echo "::error::Need Qt 6.11+ from aqt; got ${qt_version} at ${qt_prefix}" >&2
    exit 1
    ;;
esac

export PATH="${qt_prefix}/bin:${PATH}"

cmake --version
log="${TMPDIR:-/tmp}/cmake-configure.err"
if ! cmake -S "${root}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Debug}" \
  -DCMAKE_PREFIX_PATH="${qt_prefix}" \
  -DQt6_DIR="${qt_prefix}/lib/cmake/Qt6" 2>&1 | tee "${log}"; then
  echo "::error::CMake configure failed. Last lines:" >&2
  tail -n 40 "${log}" >&2 || true
  exit 1
fi
