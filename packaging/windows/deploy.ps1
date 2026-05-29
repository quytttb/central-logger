# Deploy / release: bump VERSION -> commit -> tag -> push -> GitHub Actions build-release.
#   .\packaging\windows\deploy.ps1
#   .\packaging\windows\deploy.ps1 release patch
param(
    [Parameter(Position = 0)]
    [ValidateSet("release", "bump", "commit", "tag", "push-tag", "status", "cheatsheet", "")]
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
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Read-BumpChoice {
    $ver = Get-ProjectVersion
    Write-Host ""
    Write-Host "Chon muc bump — hien tai: $ver"
    Write-Host "  1) PATCH  — bug fixes (0.0.X)"
    Write-Host "  2) MINOR  — new features (0.X.0)"
    Write-Host "  3) MAJOR  — breaking change (X.0.0)"
    Write-Host "  0) Cancel"
    Write-Host ""
    switch (Read-Host "Select bump [0-3]") {
        "1" { return "patch" }
        "2" { return "minor" }
        "3" { return "major" }
        "0" { return $null }
        default { Write-Error "Invalid choice." }
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
    } else {
        Invoke-BumpVersion -Level $Level
    }
    $ver = Get-ProjectVersion
    $tag = "v$ver"
    Write-Host ""
    Write-Host "Phat hanh: version $ver -> tag $tag -> push $Remote"
    Show-UnexpectedDirtyWarning
    if (-not (Test-Confirm "Tiep tuc (commit $ReleaseFile -> tag -> push)?")) {
        Write-Host "Cancelled."
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

function Show-Status {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    $branch = Get-GitBranch
    Write-Host ""
    Write-Host "Version (CMake): $ver"
    Write-Host "Tag (expected):  $tag"
    Write-Host "Branch:          $branch"
    Write-Host "Remote:          $Remote"
    Write-Host ""
    if (Test-TagExists $tag) {
        Write-Host "Tag local $tag : co ($((git rev-parse --short $tag).Trim()))"
    } else {
        Write-Host "Tag local $tag : chua co"
    }
    if (git ls-remote --tags $Remote $tag 2>$null) {
        Write-Host "Tag tren $Remote : co"
    } else {
        Write-Host "Tag tren $Remote : chua co"
    }
    Write-Host ""
    git status -sb
    Write-Host ""
    Write-Host "Build installer local: .\packaging\windows\build_installer.ps1"
    Show-GitHubUrls
}

function Show-Cheatsheet {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    Write-Host @"

--- Cheat sheet (git release) ---

  Version: $ver  ->  tag $tag

  .\packaging\windows\deploy.ps1 release patch

  python packaging\linux\bump_version.py bump patch
  git add CMakeLists.txt; git commit -m "chore(release): release $tag"
  git tag -a $tag -m "Release $tag"
  git push $Remote HEAD
  git push $Remote $tag

  Build installer local:
  .\packaging\windows\build_installer.ps1

"@
}

function Show-DeployMenu {
    $ver = Get-ProjectVersion
    $tag = Get-TagName
    $branch = Get-GitBranch
    Write-Host ""
    Write-Host "========================================"
    Write-Host "  Central Logger — Deploy / Release"
    Write-Host "  Version: $ver  ->  tag $tag"
    Write-Host "  Branch: $branch    Remote: $Remote"
    Write-Host "========================================"
    Write-Host ""
    Write-Host "  1) Phat hanh day du — bump -> commit -> tag -> push $Remote"
    Write-Host "  2) Chi bump version (CMakeLists.txt)"
    Write-Host "  3) Commit CMakeLists.txt"
    Write-Host "  4) Tao git tag annotated v{version}"
    Write-Host "  5) Push tag len $Remote (kich hoat Build Release)"
    Write-Host "  6) Trang thai"
    Write-Host "  7) Cheat sheet"
    Write-Host "  0) Thoat"
    Write-Host ""
    switch (Read-Host "Select option [0-7]") {
        "1" { Invoke-DoRelease }
        "2" { Invoke-DoBump }
        "3" { Invoke-DoCommit }
        "4" { Invoke-DoTag }
        "5" { Invoke-DoPushTag }
        "6" { Show-Status }
        "7" { Show-Cheatsheet }
        "0" { Write-Host "Bye." }
        default { Write-Error "Invalid choice." }
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
        "status" { Show-Status }
        "cheatsheet" { Show-Cheatsheet }
    }
    exit 0
}

Show-DeployMenu
