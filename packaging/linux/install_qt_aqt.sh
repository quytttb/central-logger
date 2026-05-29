#!/usr/bin/env bash
# Install Qt 6.11.1 + addons via aqtinstall (same modules as CI workflows).
set -euo pipefail

version="${QT_VERSION:-6.11.1}"
modules="${QT_AQT_MODULES:-qtserialbus qtserialport qtgraphs qttasktree qtquick3d qtshadertools}"
out_dir="${QT_INSTALL_DIR:-${RUNNER_WORKSPACE:-$HOME}/Qt}"

python3 -m pip install --upgrade pip
python3 -m pip install "aqtinstall==3.3.*"

python3 -m aqt install-qt linux desktop "${version}" linux_gcc_64 \
  -m ${modules} \
  -O "${out_dir}"

qt_root="${out_dir}/${version}/gcc_64"
for pkg in Qt6Graphs Qt6SerialBus Qt6TaskTree Qt6Quick3D Qt6ShaderTools; do
  if [[ ! -f "${qt_root}/lib/cmake/${pkg}/${pkg}Config.cmake" ]]; then
    echo "::error::Missing ${pkg} under ${qt_root} (modules: ${modules})" >&2
    exit 1
  fi
done
qt_bin="${qt_root}/bin"
if [[ -n "${GITHUB_PATH:-}" ]]; then
  echo "${qt_bin}" >> "${GITHUB_PATH}"
fi
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "QT_ROOT_DIR=${qt_root}" >> "${GITHUB_ENV}"
fi
if [[ -n "${GITHUB_WORKSPACE:-}" ]]; then
  echo "${qt_root}" > "${GITHUB_WORKSPACE}/.ci_qt_root"
fi
