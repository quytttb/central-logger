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
    Write-Host "Warning: con thay doi chua commit (ngoai $ReleaseFile):" -ForegroundColor Yellow
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
        Write-Host "Chon muc bump (version hien tai: $ver)"
        Write-Host "  1) PATCH  — sua loi (vd. 0.1 -> 0.1.1)"
        Write-Host "  2) MINOR  — tinh nang moi (vd. 0.1 -> 0.2.0)"
        Write-Host "  3) MAJOR  — breaking (vd. 0.1 -> 1.0.0)"
        Write-Host "  0) Quay lai menu"
        Write-Host ""
        $raw = (Read-Host "Nhap 1/2/3 hoac patch/minor/major [0-3]").Trim().ToLowerInvariant()
        switch ($raw) {
            "1" { return "patch" }
            "2" { return "minor" }
            "3" { return "major" }
            "patch" { return "patch" }
            "minor" { return "minor" }
            "major" { return "major" }
            "0" { return $null }
            default {
                Write-Host "Khong hop le: '$raw'. Chon 1-3 hoac patch/minor/major (khong nhap so version)." -ForegroundColor Yellow
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
        Write-Host "$ReleaseFile khong co thay doi — bo qua commit."
        return
    }
    Add-ReleaseFiles
    if (Test-Confirm "Commit $ReleaseFile voi message: $msg") {
        git commit -m $msg
        Write-Host "Da commit."
    } else {
        Write-Host "Da stage $ReleaseFile; chua commit."
    }
}

function Invoke-DoTag {
    $tag = Get-TagName
    if (Test-TagExists $tag) { throw "Tag $tag da ton tai." }
    Show-UnexpectedDirtyWarning
    if (Test-Confirm "Tao annotated tag $tag") {
        git tag -a $tag -m "Release $tag"
        Write-Host "Da tao tag $tag"
    }
}

function Invoke-DoPushTag {
    $tag = Get-TagName
    if (-not (Test-TagExists $tag)) {
        throw "Tag local $tag chua co. Chay option 4 hoac: .\packaging\windows\deploy.ps1 tag"
    }
    Write-Host "Se push: git push $Remote $tag"
    Show-GitHubUrls
    if (Test-Confirm "Push tag $tag len $Remote? (kich hoat Build Release)") {
        git push $Remote $tag
        Write-Host "Da push $tag."
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
        throw "$ReleaseFile chua doi sau bump — huy phat hanh."
    }

    $ver = Get-ProjectVersion
    $tag = "v$ver"
    Write-Host ""
    Write-Host "Phat hanh: version $ver -> tag $tag -> push $Remote"
    Show-UnexpectedDirtyWarning
    if (-not (Test-Confirm "Tiep tuc (commit $ReleaseFile -> tag -> push)?")) {
        Write-Host "Da huy."
        return
    }
    Add-ReleaseFiles
    git commit -m "chore(release): release $tag"
    if ($LASTEXITCODE -ne 0) { throw "Commit that bai." }
    if (Test-TagExists $tag) { throw "Tag $tag da ton tai." }
    git tag -a $tag -m "Release $tag"
    Write-Host "Push $tag..."
    git push $Remote HEAD
    git push $Remote $tag
    Write-Host "Hoan tat. GitHub Actions se build .deb + CentralLoggerSetup.exe va tao Release."
    Show-GitHubUrls
}

function Write-StatusLine {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    $branch = Get-GitBranch
    $tagLocal = if (Test-TagExists $tag) { "local co" } else { "local chua co" }
    $tagRemote = if (Test-TagOnRemote $tag) { "$Remote co" } else { "$Remote chua co" }
    $dirty = ""
    if (git status --porcelain 2>$null) { $dirty = " | working tree: dirty" }
    Write-Host "  $ver -> $tag | branch $branch | tag $tagLocal, $tagRemote$dirty"
}

function Show-Help {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    Write-Host @"

================================================================
  Help — Deploy / Release (Central Logger)
================================================================

  Version hien tai: $ver  ->  tag git: $tag
  Remote mac dinh: $Remote  (doi: `$env:DEPLOY_REMOTE = 'upstream')

-- Menu tuong tac (.\packaging\windows\deploy.ps1) -----------

  1) Phat hanh day du
     Tang SemVer -> commit CMakeLists.txt -> tag v{version}
     -> push nhanh + tag len $Remote.
     Kich hoat workflow GitHub Actions "Build Release".

  2) Chi bump version — sua CMakeLists.txt, chua commit/tag/push.

  3) Commit CMakeLists.txt — stage + commit version (co xac nhan).

  4) Tao git tag annotated v{version} tren may (local).

  5) Push tag len $Remote — CI build ban release (can tag local).

  6) Help — in huong dan nay (khong thay doi git).

  0) Thoat menu.

  Dong trang thai tren menu: version, tag, branch, tag local/remote, dirty.

-- Lenh CLI (khong menu) ---------------------------------------

  .\packaging\windows\deploy.ps1
  .\packaging\windows\deploy.ps1 release [patch|minor|major]
  .\packaging\windows\deploy.ps1 bump {patch|minor|major}
  .\packaging\windows\deploy.ps1 commit | tag | push-tag | status | help

  (cheatsheet = alias cua help)

-- Bump thu cong -----------------------------------------------

  python packaging\linux\bump_version.py show
  python packaging\linux\bump_version.py bump patch

-- Git thu cong (tuong duong menu 1) ---------------------------

  git add CMakeLists.txt
  git commit -m "chore(release): release $tag"
  git tag -a $tag -m "Release $tag"
  git push $Remote HEAD
  git push $Remote $tag

-- Build lai tag da co -----------------------------------------

  GitHub -> Actions -> "Build Release" -> Run workflow -> nhap $tag

-- Build installer local ---------------------------------------

  .\packaging\windows\build_installer.ps1

  Chi tiet: packaging\README.md

"@
}

function Show-DeployMenu {
    while ($true) {
        Write-Host ""
        Write-Host "========================================"
        Write-Host "  Central Logger — Deploy / Release"
        Write-Host "========================================"
        Write-StatusLine
        Write-Host ""
        Write-Host "  1) Phat hanh day du — bump -> commit -> tag -> push $Remote"
        Write-Host "  2) Chi bump version (CMakeLists.txt)"
        Write-Host "  3) Commit CMakeLists.txt"
        Write-Host "  4) Tao git tag annotated v{version}"
        Write-Host "  5) Push tag len $Remote (kich hoat Build Release)"
        Write-Host "  6) Help — huong dan menu, CLI va quy trinh release"
        Write-Host "  0) Thoat"
        Write-Host ""
        $choice = (Read-Host "Chon [0-6]").Trim()
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
                    Write-Host "Khong hop le: '$choice'. Nhap so 0-6." -ForegroundColor Yellow
                }
            }
        } catch {
            Write-Host $_.Exception.Message -ForegroundColor Red
        }
    }
}

if (-not (Test-Path (Join-Path $Root $ReleaseFile))) {
    throw "Chay script tu root repo (thieu $ReleaseFile)."
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git khong co tren PATH."
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
