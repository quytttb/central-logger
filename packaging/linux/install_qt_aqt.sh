#!/usr/bin/env bash
# Install Qt 6.11.1 + addons via aqtinstall (same modules as CI workflows).
set -euo pipefail

version="${QT_VERSION:-6.11.1}"
modules="${QT_AQT_MODULES:-qtserialbus qtserialport qtgraphs qttasktree qtquick3d qtshadertools}"

# Match install-qt-action layout: $RUNNER_WORKSPACE/Qt (parent of checkout).
if [[ -n "${GITHUB_WORKSPACE:-}" ]]; then
  out_dir="$(cd "${GITHUB_WORKSPACE}/.." && pwd)/Qt"
elif [[ -n "${RUNNER_WORKSPACE:-}" ]]; then
  out_dir="${RUNNER_WORKSPACE}/Qt"
else
  out_dir="${QT_INSTALL_DIR:-${HOME}/Qt}"
fi
echo "::notice::aqt output dir ${out_dir}"

python3 -m pip install --upgrade pip
if [[ -n "${AQT_SOURCE:-}" ]]; then
  python3 -m pip install "${AQT_SOURCE}"
else
  python3 -m pip install "aqtinstall>=3.3.0"
fi

if [[ -n "${AQT_BASE:-}" ]]; then
  python3 -m aqt install-qt linux desktop "${version}" linux_gcc_64 \
    -m ${modules} -O "${out_dir}" --base "${AQT_BASE}"
else
  python3 -m aqt install-qt linux desktop "${version}" linux_gcc_64 \
    -m ${modules} -O "${out_dir}"
fi

# aqt arch linux_gcc_64 installs into .../gcc_64 (not linux_gcc_64).
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
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  {
    echo "qt_prefix=${qt_root}"
    echo "qt_version=$("${qt_root}/bin/qmake6" -query QT_VERSION 2>/dev/null || "${qt_root}/bin/qmake" -query QT_VERSION)"
  } >> "${GITHUB_OUTPUT}"
fi

echo "Installed Qt ${version} at ${qt_root}"
