# Packages the Central Logger application with the Qt Installer Framework (QTIFW).
# Supported OS: Windows (PowerShell)

$ErrorActionPreference = "Stop"

# ==================== DEFAULT PATHS ====================
# CI: set CL_PROJECT_ROOT, CL_QT_DIR, CL_BUILD_DIR, CL_MINGW_DIR, CL_IFW_DIR
$PackagingWindows = $PSScriptRoot
$PROJECT_ROOT = if ($env:CL_PROJECT_ROOT) {
    $env:CL_PROJECT_ROOT
} else {
    (Resolve-Path (Join-Path $PackagingWindows "..\..")).Path
}
$QT_DIR = if ($env:CL_QT_DIR) { $env:CL_QT_DIR } elseif ($env:QT_ROOT_DIR) { $env:QT_ROOT_DIR } else { "C:\Qt\6.11.1\mingw_64" }
$MINGW_DIR = if ($env:CL_MINGW_DIR) { $env:CL_MINGW_DIR } else { "C:\Qt\Tools\mingw1310_64" }

$IFW_DIR = $env:CL_IFW_DIR
if (-not $IFW_DIR -or -not (Test-Path "$IFW_DIR\bin\binarycreator.exe")) {
    $toolsRoot = if ($env:IQTA_TOOLS) { $env:IQTA_TOOLS } elseif ($env:CL_MINGW_DIR) { Split-Path $env:CL_MINGW_DIR -Parent } else { $null }
    if ($toolsRoot) {
        $hit = Get-ChildItem -Path $toolsRoot -Filter binarycreator.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) { $IFW_DIR = $hit.Directory.Parent.FullName }
    }
}
if (-not $IFW_DIR) { $IFW_DIR = "C:\Qt\Tools\QtInstallerFramework\4.11" }
$BUILD_RELEASE_DIR = if ($env:CL_BUILD_DIR) { $env:CL_BUILD_DIR } else { "$PROJECT_ROOT\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release" }
$INSTALLER_BUILD_DIR = Join-Path $PackagingWindows "installer_build"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "BUILDING CENTRAL LOGGER INSTALLER" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# ==================== ENVIRONMENT CHECKS ====================
Write-Host "[1/6] Checking environment..." -ForegroundColor Yellow
if (-not (Test-Path "$BUILD_RELEASE_DIR\bin\central_logger.exe")) {
    Write-Error "Release binary central_logger.exe not found at: $BUILD_RELEASE_DIR\bin\central_logger.exe. Please build the Release target in Qt Creator first."
}
if (-not (Test-Path "$QT_DIR\bin\windeployqt.exe")) {
    Write-Error "windeployqt.exe not found at: $QT_DIR\bin\windeployqt.exe"
}
if (-not (Test-Path "$IFW_DIR\bin\binarycreator.exe")) {
    Write-Error "Qt Installer Framework binarycreator.exe not found at: $IFW_DIR\bin\binarycreator.exe"
}
Write-Host " Environment OK." -ForegroundColor Green

# Add Qt and MinGW to PATH so windeployqt can locate the compiler runtime.
$env:PATH = "$QT_DIR\bin;$MINGW_DIR\bin;" + $env:PATH

# ==================== PREPARE TEMP DIRECTORIES ====================
Write-Host "[2/6] Cleaning and creating temp directories..." -ForegroundColor Yellow
$TEMP_DEPLOY_DIR = "$PROJECT_ROOT\build\deploy"
if (Test-Path $TEMP_DEPLOY_DIR) {
    Remove-Item -Path $TEMP_DEPLOY_DIR -Recurse -Force
}
New-Item -ItemType Directory -Path $TEMP_DEPLOY_DIR -Force | Out-Null

$DATA_DIR = "$INSTALLER_BUILD_DIR\packages\com.central_logger.app\data"
if (Test-Path $DATA_DIR) {
    Remove-Item -Path $DATA_DIR -Recurse -Force
}
New-Item -ItemType Directory -Path $DATA_DIR -Force | Out-Null
Write-Host " Directories ready." -ForegroundColor Green

# ==================== COPY EXE AND DEPLOY DEPENDENCIES ====================
Write-Host "[3/6] Copying the main executable..." -ForegroundColor Yellow
Copy-Item -Path "$BUILD_RELEASE_DIR\bin\central_logger.exe" -Destination "$TEMP_DEPLOY_DIR\"
Write-Host " Copied central_logger.exe to the temp directory." -ForegroundColor Green

Write-Host "[4/6] Running windeployqt to collect DLL dependencies..." -ForegroundColor Yellow
& "$QT_DIR\bin\windeployqt.exe" --qmldir "$PROJECT_ROOT\src" --compiler-runtime "$TEMP_DEPLOY_DIR\central_logger.exe"
Write-Host " windeployqt finished successfully." -ForegroundColor Green

# ==================== MOVE COLLECTED DATA INTO THE PACKAGE ====================
Write-Host "[5/6] Copying collected data into the QTIFW package directory..." -ForegroundColor Yellow
Copy-Item -Path "$TEMP_DEPLOY_DIR\*" -Destination "$DATA_DIR\" -Recurse -Force
Write-Host " Copy finished." -ForegroundColor Green

# ==================== BUILD THE SETUP INSTALLER ====================
Write-Host "[6/6] Building the installer with binarycreator..." -ForegroundColor Yellow
$OUTPUT_INSTALLER = "$INSTALLER_BUILD_DIR\CentralLoggerSetup.exe"
if (Test-Path $OUTPUT_INSTALLER) {
    Remove-Item -Path $OUTPUT_INSTALLER -Force
}

& "$IFW_DIR\bin\binarycreator.exe" --offline-only -c "$INSTALLER_BUILD_DIR\config\config.xml" -p "$INSTALLER_BUILD_DIR\packages" "$OUTPUT_INSTALLER"

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $OUTPUT_INSTALLER)) {
    Write-Error "binarycreator failed (exit code $LASTEXITCODE). Check config.xml / control script above."
}

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "Installer created at:" -ForegroundColor Green
Write-Host "$OUTPUT_INSTALLER" -ForegroundColor Yellow -NoNewline
Write-Host " (~$([Math]::Round((Get-Item $OUTPUT_INSTALLER).Length / 1MB, 2)) MB)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Green
