# Install Qt 6.11.1 + MinGW addons via aqtinstall (CI / local).
$ErrorActionPreference = "Stop"

$version = if ($env:QT_VERSION) { $env:QT_VERSION } else { "6.11.1" }
$outDir = if ($env:QT_INSTALL_DIR) { $env:QT_INSTALL_DIR } else { Join-Path $env:GITHUB_WORKSPACE "Qt" }
$base = if ($env:AQT_BASE) { $env:AQT_BASE } else { "https://download.qt.io" }
$mods = if ($env:QT_AQT_MODULES) { $env:QT_AQT_MODULES -split '\s+' } else {
    @("qtserialbus", "qtserialport", "qtgraphs", "qttasktree", "qtquick3d", "qtshadertools")
}

python -m pip install --upgrade pip
python -m pip install "aqtinstall>=3.3.0"

python -m aqt install-tool windows desktop tools_mingw1310 -O $outDir --base $base
if ($LASTEXITCODE -ne 0) { throw "aqt install-tool mingw failed" }

$aqtArgs = @(
    "install-qt", "windows", "desktop", $version, "win64_mingw1310_64",
    "-m"
) + $mods + @("-O", $outDir, "--base", $base)
python -m aqt @aqtArgs
if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed" }

$baseVer = Join-Path $outDir $version
$qtRoot = Join-Path $baseVer "win64_mingw1310_64"
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

$qmake = Join-Path $qtRoot "bin\qmake.exe"
$qtVer = & $qmake -query QT_VERSION
$qtBin = Join-Path $qtRoot "bin"
if ($env:GITHUB_PATH) { Add-Content -Path $env:GITHUB_PATH -Value $qtBin }
if ($env:GITHUB_ENV) { Add-Content -Path $env:GITHUB_ENV -Value "QT_ROOT_DIR=$qtRoot" }
if ($env:GITHUB_WORKSPACE) {
    Set-Content -Path (Join-Path $env:GITHUB_WORKSPACE ".ci_qt_root") -Value $qtRoot -NoNewline
}
Write-Host "Installed Qt $qtVer at $qtRoot"
