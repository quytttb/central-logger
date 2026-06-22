#!/usr/bin/env bash
# Deploy / release: bump VERSION → commit → tag → push → GitHub Actions build-release.
#   ./packaging/linux/deploy.sh
#   ./packaging/linux/deploy.sh release patch
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${ROOT}"

BUMP_SCRIPT="${ROOT}/packaging/linux/bump_version.py"
REMOTE="${DEPLOY_REMOTE:-origin}"
RELEASE_FILE="CMakeLists.txt"

_ensure_repo() {
  if [[ ! -f "${ROOT}/${RELEASE_FILE}" ]]; then
    echo "Chạy script từ root repo (thiếu ${RELEASE_FILE})." >&2
    exit 1
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "git không có trên PATH." >&2
    exit 1
  fi
}

_run_bump() {
  local level="$1"
  if ! python3 "${BUMP_SCRIPT}" bump "${level}"; then
    echo "Bump version thất bại." >&2
    return 1
  fi
}

_current_version() {
  python3 "${BUMP_SCRIPT}" show 2>/dev/null || echo "?"
}

_tag_name() {
  echo "v$(_current_version)"
}

_git_branch() {
  git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "?"
}

_confirm() {
  local prompt="$1"
  local answer
  read -rp "${prompt} [y/N]: " answer
  [[ "${answer}" =~ ^[Yy]$ ]]
}

_git_unexpected_dirty_warning() {
  local dirty line path unexpected=""
  dirty="$(git status --porcelain 2>/dev/null)" || return 0
  [[ -z "${dirty}" ]] && return 0
  while IFS= read -r line; do
    [[ -z "${line}" ]] && continue
    path="${line##* }"
    if [[ "${path}" == *" -> "* ]]; then
      path="${path##* -> }"
    fi
    case "${path}" in
      "${RELEASE_FILE}") continue ;;
      *) unexpected+="${line}"$'\n' ;;
    esac
  done <<< "${dirty}"
  [[ -z "${unexpected}" ]] && return 0
  echo "⚠ Còn thay đổi chưa commit (ngoài ${RELEASE_FILE}):"
  printf '%s' "${unexpected}" | sed 's/^/    /'
}

_stage_release_files() {
  git add "${RELEASE_FILE}"
}

_tag_exists() {
  git rev-parse "$1" >/dev/null 2>&1
}

_tag_on_remote() {
  local tag="$1"
  local out
  out="$(git ls-remote --tags "${REMOTE}" "refs/tags/${tag}" 2>/dev/null || true)"
  [[ -n "${out}" ]]
}

_github_urls() {
  local url
  url="$(git remote get-url "${REMOTE}" 2>/dev/null || true)"
  if [[ "${url}" =~ github\.com[:/]([^/]+)/([^/.]+) ]]; then
    local owner="${BASH_REMATCH[1]}"
    local repo="${BASH_REMATCH[2]%.git}"
    echo "  Actions:  https://github.com/${owner}/${repo}/actions/workflows/build-release.yml"
    echo "  Releases: https://github.com/${owner}/${repo}/releases"
  fi
}

_prompt_bump() {
  local ver choice level
  ver="$(_current_version)"
  while true; do
    echo ""
    echo "Chọn mức bump (version hiện tại: ${ver})"
    echo "  1) PATCH  — sửa lỗi (vd. 0.1 → 0.1.1)"
    echo "  2) MINOR  — tính năng mới (vd. 0.1 → 0.2.0)"
    echo "  3) MAJOR  — breaking (vd. 0.1 → 1.0.0)"
    echo "  0) Quay lại menu"
    echo ""
    read -rp "Nhập 1/2/3 hoặc patch/minor/major [0-3]: " choice
    choice="${choice,,}"
    case "${choice}" in
      1|patch) level=patch ;;
      2|minor) level=minor ;;
      3|major) level=major ;;
      0) return 1 ;;
      *)
        echo "Không hợp lệ: «${choice}». Chọn 1–3 hoặc patch/minor/major (không nhập số version)." >&2
        continue
        ;;
    esac
    if ! _run_bump "${level}"; then
      ver="$(_current_version)"
      continue
    fi
    return 0
  done
}

_do_bump() {
  if [[ $# -eq 0 ]]; then
    _prompt_bump
  else
    _run_bump "$1"
  fi
}

_do_commit() {
  local tag msg
  tag="$(_tag_name)"
  msg="chore(release): release ${tag}"
  _git_unexpected_dirty_warning
  if git diff --quiet -- "${RELEASE_FILE}" 2>/dev/null && \
     git diff --cached --quiet -- "${RELEASE_FILE}" 2>/dev/null; then
    echo "${RELEASE_FILE} không có thay đổi — bỏ qua commit."
    return 0
  fi
  _stage_release_files
  if _confirm "Commit ${RELEASE_FILE} với message: ${msg}?"; then
    git commit -m "${msg}"
    echo "Đã commit."
  else
    echo "Đã stage ${RELEASE_FILE}; chưa commit."
  fi
}

_do_tag() {
  local tag
  tag="$(_tag_name)"
  if _tag_exists "${tag}"; then
    echo "Tag ${tag} đã tồn tại." >&2
    return 1
  fi
  _git_unexpected_dirty_warning
  if _confirm "Tạo annotated tag ${tag}?"; then
    git tag -a "${tag}" -m "Release ${tag}"
    echo "Đã tạo tag ${tag}"
  fi
}

_do_push_tag() {
  local tag
  tag="$(_tag_name)"
  if ! _tag_exists "${tag}"; then
    echo "Tag local ${tag} chưa có. Chạy option 4 hoặc: $0 tag" >&2
    return 1
  fi
  echo "Sẽ push: git push ${REMOTE} ${tag}"
  _github_urls
  if _confirm "Push tag ${tag} lên ${REMOTE}? (kích hoạt workflow Build Release)"; then
    git push "${REMOTE}" "${tag}"
    echo "Đã push ${tag}. Xem tiến trình build trên GitHub Actions."
    _github_urls
  fi
}

_do_release() {
  local level="${1:-}"
  if [[ -z "${level}" ]]; then
    _prompt_bump || return 0
  elif ! _run_bump "${level}"; then
    return 1
  fi
  if ! git diff --quiet -- "${RELEASE_FILE}" 2>/dev/null || \
     ! git diff --cached --quiet -- "${RELEASE_FILE}" 2>/dev/null; then
    :
  else
    echo "${RELEASE_FILE} chưa đổi sau bump — hủy phát hành." >&2
    return 1
  fi
  local tag
  tag="$(_tag_name)"
  echo ""
  echo "Phát hành: version $(_current_version) → tag ${tag} → push ${REMOTE}"
  _git_unexpected_dirty_warning
  if ! _confirm "Tiếp tục (commit ${RELEASE_FILE} → tag → push)?"; then
    echo "Đã hủy."
    return 0
  fi
  _stage_release_files
  git commit -m "chore(release): release ${tag}" || {
    echo "Commit thất bại (có thể không có thay đổi?)." >&2
    return 1
  }
  if _tag_exists "${tag}"; then
    echo "Tag ${tag} đã tồn tại." >&2
    return 1
  fi
  git tag -a "${tag}" -m "Release ${tag}"
  echo "Push ${tag}..."
  git push "${REMOTE}" HEAD
  git push "${REMOTE}" "${tag}"
  echo "Hoàn tất. GitHub Actions sẽ build .deb + CentralLoggerSetup.exe và tạo Release."
  _github_urls
}

_status_line() {
  local ver tag branch tag_local tag_remote dirty
  ver="$(_current_version)"
  tag="$(_tag_name)"
  branch="$(_git_branch)"
  if _tag_exists "${tag}"; then
    tag_local="local có"
  else
    tag_local="local chưa có"
  fi
  if _tag_on_remote "${tag}"; then
    tag_remote="${REMOTE} có"
  else
    tag_remote="${REMOTE} chưa có"
  fi
  dirty=""
  if [[ -n "$(git status --porcelain 2>/dev/null)" ]]; then
    dirty=" | working tree: dirty"
  fi
  echo "  ${ver} → ${tag} | branch ${branch} | tag ${tag_local}, ${tag_remote}${dirty}"
}

_show_help() {
  local ver tag
  ver="$(_current_version)"
  tag="$(_tag_name)"
  cat <<EOF

══════════════════════════════════════════════════════════════
  Help — Deploy / Release (Central Logger)
══════════════════════════════════════════════════════════════

  Version hiện tại: ${ver}  →  tag git: ${tag}
  Remote mặc định: ${REMOTE}  (đổi: DEPLOY_REMOTE=upstream)

── Menu tương tác (./packaging/linux/deploy.sh) ──────────────

  1) Phát hành đầy đủ
     Tăng SemVer (patch/minor/major) → commit CMakeLists.txt
     → tạo tag v{version} → push nhánh + tag lên ${REMOTE}.
     Kích hoạt workflow GitHub Actions "Build Release"
     (.deb Linux + CentralLoggerSetup.exe + GitHub Release).

  2) Chỉ bump version
     Sửa số VERSION trong CMakeLists.txt; chưa commit/tag/push.
     Dùng khi muốn kiểm tra diff trước khi phát hành.

  3) Commit CMakeLists.txt
     Stage + commit file version với message chuẩn release.
     Hỏi xác nhận; bỏ qua nếu file không đổi.

  4) Tạo git tag
     Tag annotated v{version} trỏ commit hiện tại (local).
     Không push; tag trùng tên sẽ báo lỗi.

  5) Push tag
     Đẩy tag lên ${REMOTE} để CI build bản release.
     Cần tag local đã tồn tại (chạy 4 hoặc option 1 trước).

  6) Help — in hướng dẫn này (không thay đổi git).

  0) Thoát menu.

  Dòng trạng thái trên menu: version, tag, branch, tag local/remote,
  working tree dirty (nếu có thay đổi chưa commit).

── Lệnh CLI (không menu) ─────────────────────────────────────

  ./packaging/linux/deploy.sh
      Mở menu tương tác.

  ./packaging/linux/deploy.sh release [patch|minor|major]
      Giống menu 1; thiếu mức bump thì hỏi tương tác.

  ./packaging/linux/deploy.sh bump {patch|minor|major}
      Giống menu 2.

  ./packaging/linux/deploy.sh commit
      Giống menu 3.

  ./packaging/linux/deploy.sh tag
      Giống menu 4.

  ./packaging/linux/deploy.sh push-tag
      Giống menu 5.

  ./packaging/linux/deploy.sh status
      In một dòng trạng thái (giống header menu).

  ./packaging/linux/deploy.sh help
      In help (giống menu 6). Alias: cheatsheet

── Bump version thủ công ──────────────────────────────────────

  python3 packaging/linux/bump_version.py show
      In version đọc từ CMakeLists.txt.

  python3 packaging/linux/bump_version.py bump patch
      PATCH: 0.0.X  |  minor: 0.X.0  |  major: X.0.0

── Quy trình git thủ công (tương đương menu 1) ───────────────

  python3 packaging/linux/bump_version.py bump patch
  git add CMakeLists.txt
  git commit -m "chore(release): release ${tag}"
  git tag -a ${tag} -m "Release ${tag}"
  git push ${REMOTE} HEAD
  git push ${REMOTE} ${tag}

── Build lại khi tag đã có (không đổi git) ───────────────────

  GitHub → Actions → workflow "Build Release"
  → Run workflow → nhập tag (vd. ${tag})

── Build gói cài đặt trên máy (không qua git tag) ─────────────

  ./packaging/linux/cpack_deb.sh
      Tạo .deb local trong dist/ (cần Qt 6.11, CMAKE_PREFIX_PATH).

  Chi tiết: packaging/README.md

EOF
}

_show_menu() {
  local choice
  while true; do
    echo ""
    echo "========================================"
    echo "  Central Logger — Deploy / Release"
    echo "========================================"
    _status_line
    echo ""
    echo "  1) Phát hành đầy đủ — bump → commit → tag → push ${REMOTE}"
    echo "  2) Chỉ bump version (CMakeLists.txt)"
    echo "  3) Commit CMakeLists.txt"
    echo "  4) Tạo git tag annotated v{version}"
    echo "  5) Push tag lên ${REMOTE} (kích hoạt Build Release)"
    echo "  6) Help — hướng dẫn menu, CLI và quy trình release"
    echo "  0) Thoát"
    echo ""
    read -rp "Chọn [0-6]: " choice
    case "${choice}" in
      1) _do_release || true ;;
      2) _do_bump || true ;;
      3) _do_commit || true ;;
      4) _do_tag || true ;;
      5) _do_push_tag || true ;;
      6) _show_help ;;
      0) echo "Bye."; break ;;
      *)
        echo "Không hợp lệ: «${choice}». Nhập số 0–6." >&2
        continue
        ;;
    esac
  done
}

_ensure_repo

if [[ $# -gt 0 ]]; then
  CMD="${1}"
  ARG="${2:-}"
  case "${CMD}" in
    release)
      if [[ -z "${ARG}" ]]; then _do_release; else _do_release "${ARG}"; fi
      ;;
    bump)
      [[ -z "${ARG}" ]] && { echo "Usage: $0 bump {major|minor|patch}" >&2; exit 1; }
      _do_bump "${ARG}"
      ;;
    commit) _do_commit ;;
    tag) _do_tag ;;
    push-tag) _do_push_tag ;;
    status) _status_line ;;
    help|cheatsheet) _show_help ;;
    *)
      echo "Usage: $0  OR  $0 {release|bump|commit|tag|push-tag|status|help} [patch|minor|major]" >&2
      exit 1
      ;;
  esac
  exit 0
fi

_show_menu
