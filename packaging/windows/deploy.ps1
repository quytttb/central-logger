# Deploy / release: bump VERSION -> commit -> tag -> push -> GitHub Actions build-release.
#   .\packaging\windows\deploy.ps1
#   .\packaging\windows\deploy.ps1 release patch
param(
    [Parameter(Position = 0)]
    [ValidateSet("release", "bump", "commit", "tag", "push-tag", "status", "help", "cheatsheet", "")]
    [string]$Command = "",

    [Parameter(Position = 1)]
    [ValidateSet("major", "minor", "patch", "")]
    [string]$Bump = "",

    [string]$Remote = $(if ($env:DEPLOY_REMOTE) { $env:DEPLOY_REMOTE } else { "origin" })
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $Root
$BumpScript = Join-Path $Root "packaging\linux\bump_version.py"
$ReleaseFile = "CMakeLists.txt"

function Get-Python {
    $py = Get-Command python -ErrorAction SilentlyContinue
    if (-not $py) { $py = Get-Command python3 -ErrorAction SilentlyContinue }
    if (-not $py) { throw "python not found on PATH" }
    return $py
}

function Get-ProjectVersion {
    $py = Get-Python
    $v = & $py.Source $BumpScript show 2>$null
    if ($LASTEXITCODE -ne 0) { return "?" }
    return $v.Trim()
}

function Get-TagName { return "v$(Get-ProjectVersion)" }

function Get-GitBranch {
    try { return (git rev-parse --abbrev-ref HEAD).Trim() }
    catch { return "?" }
}

function Test-Confirm {
    param([string]$Prompt)
    return (Read-Host "$Prompt [y/N]") -match '^[Yy]$'
}

function Show-UnexpectedDirtyWarning {
    $status = git status --porcelain 2>$null
    if (-not $status) { return }
    $unexpected = @()
    foreach ($line in $status) {
        $path = ($line -replace '^..\s+', '').Trim()
        if ($path -match ' -> ') { $path = ($path -split ' -> ', 2)[1].Trim() }
        if ($path -eq $ReleaseFile) { continue }
        $unexpected += $line
    }
    if ($unexpected.Count -eq 0) { return }
    Write-Host "Warning: uncommitted changes remain (besides $ReleaseFile):" -ForegroundColor Yellow
    $unexpected | ForEach-Object { Write-Host "    $_" }
}

function Add-ReleaseFiles { git add $ReleaseFile }

function Test-TagExists {
    param([string]$Tag)
    git rev-parse $Tag 2>$null | Out-Null
    return $LASTEXITCODE -eq 0
}

function Test-TagOnRemote {
    param([string]$Tag)
    $out = git ls-remote --tags $Remote "refs/tags/$Tag" 2>$null
    return ($LASTEXITCODE -eq 0) -and ($out -match '\S')
}

function Show-GitHubUrls {
    try { $url = (git remote get-url $Remote 2>$null).Trim() } catch { return }
    if ($url -match 'github\.com[:/]([^/]+)/([^/.]+)') {
        $owner = $Matches[1]
        $repo = $Matches[2] -replace '\.git$', ''
        Write-Host "  Actions:  https://github.com/$owner/$repo/actions/workflows/build-release.yml"
        Write-Host "  Releases: https://github.com/$owner/$repo/releases"
    }
}

function Invoke-BumpVersion {
    param([ValidateSet("major", "minor", "patch")][string]$Level)
    $py = Get-Python
    Write-Host "== Bump version ($Level) =="
    & $py.Source $BumpScript bump $Level
    if ($LASTEXITCODE -ne 0) { throw "bump failed" }
}

function Read-BumpChoice {
    $ver = Get-ProjectVersion
    while ($true) {
        Write-Host ""
        Write-Host "Select bump level (current version: $ver)"
        Write-Host "  1) PATCH  - bug fix (e.g. 0.1 -> 0.1.1)"
        Write-Host "  2) MINOR  - new feature (e.g. 0.1 -> 0.2.0)"
        Write-Host "  3) MAJOR  - breaking change (e.g. 0.1 -> 1.0.0)"
        Write-Host "  0) Back to menu"
        Write-Host ""
        $raw = (Read-Host "Enter 1/2/3 or patch/minor/major [0-3]").Trim().ToLowerInvariant()
        switch ($raw) {
            "1" { return "patch" }
            "2" { return "minor" }
            "3" { return "major" }
            "patch" { return "patch" }
            "minor" { return "minor" }
            "major" { return "major" }
            "0" { return $null }
            default {
                Write-Host "Invalid: '$raw'. Choose 1-3 or patch/minor/major (do not type a version number)." -ForegroundColor Yellow
            }
        }
    }
}

function Invoke-DoBump {
    param([string]$Level)
    if (-not $Level) {
        $Level = Read-BumpChoice
        if (-not $Level) { return }
    }
    Invoke-BumpVersion -Level $Level
}

function Invoke-DoCommit {
    $tag = Get-TagName
    $msg = "chore(release): release $tag"
    Show-UnexpectedDirtyWarning
    git diff --quiet -- $ReleaseFile 2>$null
    $clean = $LASTEXITCODE -eq 0
    git diff --cached --quiet -- $ReleaseFile 2>$null
    $cleanCached = $LASTEXITCODE -eq 0
    if ($clean -and $cleanCached) {
        Write-Host "$ReleaseFile has no changes - skipping commit."
        return
    }
    Add-ReleaseFiles
    if (Test-Confirm "Commit $ReleaseFile with message: $msg") {
        git commit -m $msg
        Write-Host "Committed."
    } else {
        Write-Host "Staged $ReleaseFile; not committed."
    }
}

function Invoke-DoTag {
    $tag = Get-TagName
    if (Test-TagExists $tag) { throw "Tag $tag already exists." }
    Show-UnexpectedDirtyWarning
    if (Test-Confirm "Create annotated tag $tag") {
        git tag -a $tag -m "Release $tag"
        Write-Host "Created tag $tag"
    }
}

function Invoke-DoPushTag {
    $tag = Get-TagName
    if (-not (Test-TagExists $tag)) {
        throw "Local tag $tag does not exist. Run option 4 or: .\packaging\windows\deploy.ps1 tag"
    }
    Write-Host "Will push: git push $Remote $tag"
    Show-GitHubUrls
    if (Test-Confirm "Push tag $tag to $Remote? (triggers Build Release)") {
        git push $Remote $tag
        Write-Host "Pushed $tag."
        Show-GitHubUrls
    }
}

function Invoke-DoRelease {
    param([string]$Level)
    if (-not $Level) {
        $Level = Read-BumpChoice
        if (-not $Level) { return }
    }
    Invoke-BumpVersion -Level $Level

    git diff --quiet -- $ReleaseFile 2>$null
    $noUnstaged = $LASTEXITCODE -eq 0
    git diff --cached --quiet -- $ReleaseFile 2>$null
    $noStaged = $LASTEXITCODE -eq 0
    if ($noUnstaged -and $noStaged) {
        throw "$ReleaseFile did not change after bump - aborting release."
    }

    $ver = Get-ProjectVersion
    $tag = "v$ver"
    Write-Host ""
    Write-Host "Release: version $ver -> tag $tag -> push $Remote"
    Show-UnexpectedDirtyWarning
    if (-not (Test-Confirm "Continue (commit $ReleaseFile -> tag -> push)?")) {
        Write-Host "Cancelled."
        return
    }
    Add-ReleaseFiles
    git commit -m "chore(release): release $tag"
    if ($LASTEXITCODE -ne 0) { throw "Commit failed." }
    if (Test-TagExists $tag) { throw "Tag $tag already exists." }
    git tag -a $tag -m "Release $tag"
    Write-Host "Pushing $tag..."
    git push $Remote HEAD
    git push $Remote $tag
    Write-Host "Done. GitHub Actions will build .deb + CentralLoggerSetup.exe and create the Release."
    Show-GitHubUrls
}

function Write-StatusLine {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    $branch = Get-GitBranch
    $tagLocal = if (Test-TagExists $tag) { "local yes" } else { "local no" }
    $tagRemote = if (Test-TagOnRemote $tag) { "$Remote yes" } else { "$Remote no" }
    $dirty = ""
    if (git status --porcelain 2>$null) { $dirty = " | working tree: dirty" }
    Write-Host "  $ver -> $tag | branch $branch | tag $tagLocal, $tagRemote$dirty"
}

function Show-Help {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    Write-Host @"

================================================================
  Help - Deploy / Release (Central Logger)
================================================================

  Current version: $ver  ->  git tag: $tag
  Default remote: $Remote  (override: `$env:DEPLOY_REMOTE = 'upstream')

-- Interactive menu (.\packaging\windows\deploy.ps1) ----------

  1) Full release
     Bump SemVer -> commit CMakeLists.txt -> tag v{version}
     -> push branch + tag to $Remote.
     Triggers the GitHub Actions "Build Release" workflow.

  2) Bump version only - edit CMakeLists.txt, no commit/tag/push.

  3) Commit CMakeLists.txt - stage + commit version (with confirmation).

  4) Create annotated git tag v{version} locally.

  5) Push tag to $Remote - CI builds the release (needs a local tag).

  6) Help - print this guide (does not change git).

  0) Exit menu.

  Status line on the menu: version, tag, branch, tag local/remote, dirty.

-- CLI commands (no menu) -------------------------------------

  .\packaging\windows\deploy.ps1
  .\packaging\windows\deploy.ps1 release [patch|minor|major]
  .\packaging\windows\deploy.ps1 bump {patch|minor|major}
  .\packaging\windows\deploy.ps1 commit | tag | push-tag | status | help

  (cheatsheet = alias of help)

-- Manual bump ------------------------------------------------

  python packaging\linux\bump_version.py show
  python packaging\linux\bump_version.py bump patch

-- Manual git (equivalent to menu 1) --------------------------

  git add CMakeLists.txt
  git commit -m "chore(release): release $tag"
  git tag -a $tag -m "Release $tag"
  git push $Remote HEAD
  git push $Remote $tag

-- Rebuild an existing tag ------------------------------------

  GitHub -> Actions -> "Build Release" -> Run workflow -> enter $tag

-- Build installer locally ------------------------------------

  .\packaging\windows\build_installer.ps1

  Details: packaging\README.md

"@
}

function Show-DeployMenu {
    while ($true) {
        Write-Host ""
        Write-Host "========================================"
        Write-Host "  Central Logger - Deploy / Release"
        Write-Host "========================================"
        Write-StatusLine
        Write-Host ""
        Write-Host "  1) Full release - bump -> commit -> tag -> push $Remote"
        Write-Host "  2) Bump version only (CMakeLists.txt)"
        Write-Host "  3) Commit CMakeLists.txt"
        Write-Host "  4) Create annotated git tag v{version}"
        Write-Host "  5) Push tag to $Remote (triggers Build Release)"
        Write-Host "  6) Help - menu, CLI and release workflow guide"
        Write-Host "  0) Exit"
        Write-Host ""
        $choice = (Read-Host "Select [0-6]").Trim()
        try {
            switch ($choice) {
                "1" { Invoke-DoRelease }
                "2" { Invoke-DoBump }
                "3" { Invoke-DoCommit }
                "4" { Invoke-DoTag }
                "5" { Invoke-DoPushTag }
                "6" { Show-Help }
                "0" { Write-Host "Bye."; return }
                default {
                    Write-Host "Invalid: '$choice'. Enter a number 0-6." -ForegroundColor Yellow
                }
            }
        } catch {
            Write-Host $_.Exception.Message -ForegroundColor Red
        }
    }
}

if (-not (Test-Path (Join-Path $Root $ReleaseFile))) {
    throw "Run this script from the repo root (missing $ReleaseFile)."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git is not on PATH."
}

if ($Command) {
    switch ($Command) {
        "release" { if ($Bump) { Invoke-DoRelease -Level $Bump } else { Invoke-DoRelease } }
        "bump" {
            if (-not $Bump) {
                Write-Host "Usage: .\packaging\windows\deploy.ps1 bump {major|minor|patch}"
                exit 1
            }
            Invoke-DoBump -Level $Bump
        }
        "commit" { Invoke-DoCommit }
        "tag" { Invoke-DoTag }
        "push-tag" { Invoke-DoPushTag }
        "status" { Write-StatusLine }
        "help" { Show-Help }
        "cheatsheet" { Show-Help }
    }
    exit 0
}

Show-DeployMenu
