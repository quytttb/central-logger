#!/usr/bin/env bash
# Deploy / release: bump VERSION -> commit -> tag -> push -> GitHub Actions build-release.
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
    echo "Run this script from the repo root (missing ${RELEASE_FILE})." >&2
    exit 1
  fi
  if ! command -v git >/dev/null 2>&1; then
    echo "git is not on PATH." >&2
    exit 1
  fi
}

_run_bump() {
  local level="$1"
  if ! python3 "${BUMP_SCRIPT}" bump "${level}"; then
    echo "Bump version failed." >&2
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
  echo "Warning: uncommitted changes remain (besides ${RELEASE_FILE}):"
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
    echo "Select bump level (current version: ${ver})"
    echo "  1) PATCH  - bug fix (e.g. 0.1 -> 0.1.1)"
    echo "  2) MINOR  - new feature (e.g. 0.1 -> 0.2.0)"
    echo "  3) MAJOR  - breaking change (e.g. 0.1 -> 1.0.0)"
    echo "  0) Back to menu"
    echo ""
    read -rp "Enter 1/2/3 or patch/minor/major [0-3]: " choice
    choice="${choice,,}"
    case "${choice}" in
      1|patch) level=patch ;;
      2|minor) level=minor ;;
      3|major) level=major ;;
      0) return 1 ;;
      *)
        echo "Invalid: '${choice}'. Choose 1-3 or patch/minor/major (do not type a version number)." >&2
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
    echo "${RELEASE_FILE} has no changes - skipping commit."
    return 0
  fi
  _stage_release_files
  if _confirm "Commit ${RELEASE_FILE} with message: ${msg}?"; then
    git commit -m "${msg}"
    echo "Committed."
  else
    echo "Staged ${RELEASE_FILE}; not committed."
  fi
}

_do_tag() {
  local tag
  tag="$(_tag_name)"
  if _tag_exists "${tag}"; then
    echo "Tag ${tag} already exists." >&2
    return 1
  fi
  _git_unexpected_dirty_warning
  if _confirm "Create annotated tag ${tag}?"; then
    git tag -a "${tag}" -m "Release ${tag}"
    echo "Created tag ${tag}"
  fi
}

_do_push_tag() {
  local tag
  tag="$(_tag_name)"
  if ! _tag_exists "${tag}"; then
    echo "Local tag ${tag} does not exist. Run option 4 or: $0 tag" >&2
    return 1
  fi
  echo "Will push: git push ${REMOTE} ${tag}"
  _github_urls
  if _confirm "Push tag ${tag} to ${REMOTE}? (triggers the Build Release workflow)"; then
    git push "${REMOTE}" "${tag}"
    echo "Pushed ${tag}. Track the build on GitHub Actions."
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
    echo "${RELEASE_FILE} did not change after bump - aborting release." >&2
    return 1
  fi
  local tag
  tag="$(_tag_name)"
  echo ""
  echo "Release: version $(_current_version) -> tag ${tag} -> push ${REMOTE}"
  _git_unexpected_dirty_warning
  if ! _confirm "Continue (commit ${RELEASE_FILE} -> tag -> push)?"; then
    echo "Cancelled."
    return 0
  fi
  _stage_release_files
  git commit -m "chore(release): release ${tag}" || {
    echo "Commit failed (maybe no changes?)." >&2
    return 1
  }
  if _tag_exists "${tag}"; then
    echo "Tag ${tag} already exists." >&2
    return 1
  fi
  git tag -a "${tag}" -m "Release ${tag}"
  echo "Pushing ${tag}..."
  git push "${REMOTE}" HEAD
  git push "${REMOTE}" "${tag}"
  echo "Done. GitHub Actions will build .deb + CentralLoggerSetup.exe and create the Release."
  _github_urls
}

_status_line() {
  local ver tag branch tag_local tag_remote dirty
  ver="$(_current_version)"
  tag="$(_tag_name)"
  branch="$(_git_branch)"
  if _tag_exists "${tag}"; then
    tag_local="local yes"
  else
    tag_local="local no"
  fi
  if _tag_on_remote "${tag}"; then
    tag_remote="${REMOTE} yes"
  else
    tag_remote="${REMOTE} no"
  fi
  dirty=""
  if [[ -n "$(git status --porcelain 2>/dev/null)" ]]; then
    dirty=" | working tree: dirty"
  fi
  echo "  ${ver} -> ${tag} | branch ${branch} | tag ${tag_local}, ${tag_remote}${dirty}"
}

_show_help() {
  local ver tag
  ver="$(_current_version)"
  tag="$(_tag_name)"
  cat <<EOF

==============================================================
  Help - Deploy / Release (Central Logger)
==============================================================

  Current version: ${ver}  ->  git tag: ${tag}
  Default remote: ${REMOTE}  (override: DEPLOY_REMOTE=upstream)

-- Interactive menu (./packaging/linux/deploy.sh) ------------

  1) Full release
     Bump SemVer (patch/minor/major) -> commit CMakeLists.txt
     -> create tag v{version} -> push branch + tag to ${REMOTE}.
     Triggers the GitHub Actions "Build Release" workflow
     (.deb Linux + CentralLoggerSetup.exe + GitHub Release).

  2) Bump version only
     Edit the VERSION number in CMakeLists.txt; no commit/tag/push.
     Use this to inspect the diff before releasing.

  3) Commit CMakeLists.txt
     Stage + commit the version file with the standard release message.
     Asks for confirmation; skips if the file is unchanged.

  4) Create git tag
     Annotated tag v{version} pointing at the current commit (local).
     Does not push; a duplicate tag name errors out.

  5) Push tag
     Push the tag to ${REMOTE} so CI builds the release.
     Requires an existing local tag (run 4 or option 1 first).

  6) Help - print this guide (does not change git).

  0) Exit menu.

  Status line on the menu: version, tag, branch, tag local/remote,
  working tree dirty (if there are uncommitted changes).

-- CLI commands (no menu) ------------------------------------

  ./packaging/linux/deploy.sh
      Open the interactive menu.

  ./packaging/linux/deploy.sh release [patch|minor|major]
      Same as menu 1; prompts interactively if the bump level is omitted.

  ./packaging/linux/deploy.sh bump {patch|minor|major}
      Same as menu 2.

  ./packaging/linux/deploy.sh commit
      Same as menu 3.

  ./packaging/linux/deploy.sh tag
      Same as menu 4.

  ./packaging/linux/deploy.sh push-tag
      Same as menu 5.

  ./packaging/linux/deploy.sh status
      Print a single status line (same as the menu header).

  ./packaging/linux/deploy.sh help
      Print help (same as menu 6). Alias: cheatsheet

-- Manual bump ----------------------------------------------

  python3 packaging/linux/bump_version.py show
      Print the version read from CMakeLists.txt.

  python3 packaging/linux/bump_version.py bump patch
      PATCH: 0.0.X  |  minor: 0.X.0  |  major: X.0.0

-- Manual git workflow (equivalent to menu 1) ---------------

  python3 packaging/linux/bump_version.py bump patch
  git add CMakeLists.txt
  git commit -m "chore(release): release ${tag}"
  git tag -a ${tag} -m "Release ${tag}"
  git push ${REMOTE} HEAD
  git push ${REMOTE} ${tag}

-- Rebuild an existing tag (no git change) ------------------

  GitHub -> Actions -> "Build Release" workflow
  -> Run workflow -> enter the tag (e.g. ${tag})

-- Build the installer locally (without a git tag) ----------

  ./packaging/linux/cpack_deb.sh
      Create a local .deb in dist/ (needs Qt 6.11, CMAKE_PREFIX_PATH).

  Details: packaging/README.md

EOF
}

_show_menu() {
  local choice
  while true; do
    echo ""
    echo "========================================"
    echo "  Central Logger - Deploy / Release"
    echo "========================================"
    _status_line
    echo ""
    echo "  1) Full release - bump -> commit -> tag -> push ${REMOTE}"
    echo "  2) Bump version only (CMakeLists.txt)"
    echo "  3) Commit CMakeLists.txt"
    echo "  4) Create annotated git tag v{version}"
    echo "  5) Push tag to ${REMOTE} (triggers Build Release)"
    echo "  6) Help - menu, CLI and release workflow guide"
    echo "  0) Exit"
    echo ""
    read -rp "Select [0-6]: " choice
    case "${choice}" in
      1) _do_release || true ;;
      2) _do_bump || true ;;
      3) _do_commit || true ;;
      4) _do_tag || true ;;
      5) _do_push_tag || true ;;
      6) _show_help ;;
      0) echo "Bye."; break ;;
      *)
        echo "Invalid: '${choice}'. Enter a number 0-6." >&2
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
