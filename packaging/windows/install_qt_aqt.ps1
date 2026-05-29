# Install Qt 6.11.1 + MinGW via aqtinstall (local dev; CI uses install-qt-action).
$ErrorActionPreference = "Stop"

$version = if ($env:QT_VERSION) { $env:QT_VERSION } else { "6.11.1" }
if ($env:QT_INSTALL_DIR) {
    $outDir = $env:QT_INSTALL_DIR
} elseif ($env:GITHUB_WORKSPACE) {
    $outDir = Join-Path (Split-Path $env:GITHUB_WORKSPACE -Parent) "Qt"
} else {
    $outDir = Join-Path $env:USERPROFILE "Qt"
}
$mods = if ($env:QT_AQT_MODULES) { $env:QT_AQT_MODULES -split '\s+' } else {
    @("qtserialbus", "qtserialport", "qtgraphs", "qttasktree", "qtquick3d", "qtshadertools")
}
$aqtSource = if ($env:AQT_SOURCE) { $env:AQT_SOURCE } else { "aqtinstall>=3.3.0" }

python -m pip install --upgrade pip
python -m pip install $aqtSource

$toolArgs = @("install-tool", "windows", "desktop", "tools_mingw1310", "-O", $outDir)
if ($env:AQT_BASE) { $toolArgs += @("--base", $env:AQT_BASE) }
python -m aqt @toolArgs
if ($LASTEXITCODE -ne 0) { throw "aqt install-tool mingw failed" }

$aqtArgs = @("install-qt", "windows", "desktop", $version, "win64_mingw", "-m") + $mods + @("-O", $outDir)
if ($env:AQT_BASE) { $aqtArgs += @("--base", $env:AQT_BASE) }
python -m aqt @aqtArgs
if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed" }

$baseVer = Join-Path $outDir $version
$qtRoot = Join-Path $baseVer "mingw_64"
if (-not (Test-Path (Join-Path $qtRoot "lib\cmake\Qt6\Qt6Config.cmake"))) {
    $cfg = Get-ChildItem -Path $baseVer -Recurse -Filter "Qt6Config.cmake" -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -eq "Qt6" } |
        Select-Object -First 1
    if ($cfg) {
        $qtRoot = $cfg.Directory.Parent.Parent.Parent.FullName
    } else {
        throw "Qt 6.11 install not found under $baseVer"
    }
}

$qtBin = Join-Path $qtRoot "bin"
if ($env:GITHUB_PATH) { Add-Content -Path $env:GITHUB_PATH -Value $qtBin }
if ($env:GITHUB_ENV) { Add-Content -Path $env:GITHUB_ENV -Value "QT_ROOT_DIR=$qtRoot" }
if ($env:GITHUB_WORKSPACE) {
    Set-Content -Path (Join-Path $env:GITHUB_WORKSPACE ".ci_qt_root") -Value $qtRoot -NoNewline
}
Write-Host "Installed Qt $(& (Join-Path $qtBin 'qmake.exe') -query QT_VERSION) at $qtRoot"
