# Install Qt 6.11.1 + MinGW addons via aqtinstall (CI).
$ErrorActionPreference = "Stop"

$version = if ($env:QT_VERSION) { $env:QT_VERSION } else { "6.11.1" }
$modules = if ($env:QT_AQT_MODULES) { $env:QT_AQT_MODULES } else {
    "qtserialbus qtserialport qtgraphs qttasktree qtquick3d qtshadertools"
}
$outDir = if ($env:QT_INSTALL_DIR) { $env:QT_INSTALL_DIR } else { Join-Path $env:GITHUB_WORKSPACE "Qt" }

python -m pip install --upgrade pip
python -m pip install "aqtinstall==3.3.*"

python -m aqt install-tool windows desktop tools_mingw1310 -O $outDir

$moduleList = $modules.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
$aqtArgs = @(
    "install-qt", "windows", "desktop", $version, "win64_mingw1310_64",
    "-m"
) + $moduleList + @("-O", $outDir)
python -m aqt @aqtArgs

$qtRoot = Join-Path $outDir "$version\win64_mingw1310_64"
foreach ($pkg in @("Qt6Graphs", "Qt6SerialBus", "Qt6TaskTree", "Qt6Quick3D", "Qt6ShaderTools")) {
    $cfg = Join-Path $qtRoot "lib\cmake\$pkg\${pkg}Config.cmake"
    if (-not (Test-Path $cfg)) {
        Write-Error "Missing $pkg at $cfg"
    }
}

$qtBin = Join-Path $qtRoot "bin"
Add-Content -Path $env:GITHUB_PATH -Value $qtBin
Add-Content -Path $env:GITHUB_ENV -Value "QT_ROOT_DIR=$qtRoot"
Set-Content -Path (Join-Path $env:GITHUB_WORKSPACE ".ci_qt_root") -Value $qtRoot -NoNewline
