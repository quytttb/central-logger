# Install Qt 6.11.1 + MinGW addons via aqtinstall (CI).
$ErrorActionPreference = "Stop"

$version = if ($env:QT_VERSION) { $env:QT_VERSION } else { "6.11.1" }
$outDir = if ($env:QT_INSTALL_DIR) { $env:QT_INSTALL_DIR } else { Join-Path $env:GITHUB_WORKSPACE "Qt" }

python -m pip install --upgrade pip
python -m pip install "aqtinstall>=3.3.0"

python -m aqt install-tool windows desktop tools_mingw1310 -O $outDir

python -m aqt install-qt windows desktop $version win64_mingw1310_64 `
    -m qtserialbus qtserialport qtgraphs qttasktree qtquick3d qtshadertools `
    -O $outDir

$base = Join-Path $outDir $version
$qtRoot = $null
foreach ($name in @("win64_mingw1310_64", "mingw1310_64", "gcc_64")) {
    $candidate = Join-Path $base $name
    if (Test-Path (Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake")) {
        $qtRoot = $candidate
        break
    }
}
if (-not $qtRoot) {
    $cfg = Get-ChildItem -Path $base -Recurse -Filter "Qt6Config.cmake" -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -eq "Qt6" } |
        Select-Object -First 1
    if ($cfg) {
        $qtRoot = $cfg.Directory.Parent.Parent.Parent.FullName
    }
}
if (-not $qtRoot) {
    Write-Error "Qt 6.11 install not found under $base"
}

foreach ($pkg in @("Qt6Graphs", "Qt6SerialBus", "Qt6TaskTree", "Qt6Quick3D", "Qt6ShaderTools")) {
    $path = Join-Path $qtRoot "lib\cmake\$pkg\${pkg}Config.cmake"
    if (-not (Test-Path $path)) {
        Write-Error "Missing $pkg at $path"
    }
}

$qtBin = Join-Path $qtRoot "bin"
Add-Content -Path $env:GITHUB_PATH -Value $qtBin
Add-Content -Path $env:GITHUB_ENV -Value "QT_ROOT_DIR=$qtRoot"
Set-Content -Path (Join-Path $env:GITHUB_WORKSPACE ".ci_qt_root") -Value $qtRoot -NoNewline
Write-Host "Installed Qt at $qtRoot"
