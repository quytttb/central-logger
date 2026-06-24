# Script tu dong dong goi ung dung Central Logger bang Qt Installer Framework (QTIFW)
# He dieu hanh ho tro: Windows (PowerShell)

$ErrorActionPreference = "Stop"

# ==================== DINH NGHIA CAC DUONG DAN MAC DINH ====================
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
Write-Host "BAT DAU DONG GOI UNG DUNG CENTRAL LOGGER" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# ==================== KIEM TRA DUONG DAN ====================
Write-Host "[1/6] Kiem tra moi truong..." -ForegroundColor Yellow
if (-not (Test-Path "$BUILD_RELEASE_DIR\bin\central_logger.exe")) {
    Write-Error "Khong tim thay file build Release central_logger.exe tai: $BUILD_RELEASE_DIR\bin\central_logger.exe. Vui lau build Release trong Qt Creator truoc!"
}
if (-not (Test-Path "$QT_DIR\bin\windeployqt.exe")) {
    Write-Error "Khong tim thay windeployqt.exe tai: $QT_DIR\bin\windeployqt.exe"
}
if (-not (Test-Path "$IFW_DIR\bin\binarycreator.exe")) {
    Write-Error "Khong tim thay binarycreator.exe cua Qt Installer Framework tai: $IFW_DIR\bin\binarycreator.exe"
}
Write-Host " Moi truong hop le!" -ForegroundColor Green

# Cau hinh bien moi truong PATH de windeployqt tim thay compiler runtime
$env:PATH = "$QT_DIR\bin;$MINGW_DIR\bin;" + $env:PATH

# ==================== KHOI TAO THU MUC TAM ====================
Write-Host "[2/6] Don dep va khoi tao thu muc tam..." -ForegroundColor Yellow
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
Write-Host " Khoi tao thu muc hoan tat." -ForegroundColor Green

# ==================== SAO CHEP EXE VA DEPLOY DEPENDENCIES ====================
Write-Host "[3/6] Sao chep file thuc thi chinh..." -ForegroundColor Yellow
Copy-Item -Path "$BUILD_RELEASE_DIR\bin\central_logger.exe" -Destination "$TEMP_DEPLOY_DIR\"
Write-Host " Da sao chep central_logger.exe sang thu muc tam." -ForegroundColor Green

Write-Host "[4/6] Chay windeployqt de thu thap cac thu vien DLL..." -ForegroundColor Yellow
& "$QT_DIR\bin\windeployqt.exe" --qmldir "$PROJECT_ROOT\src" --compiler-runtime "$TEMP_DEPLOY_DIR\central_logger.exe"
Write-Host " Chay windeployqt thanh cong." -ForegroundColor Green

# ==================== CHUYEN DU LIEU DEN THU MUC DONG GOI ====================
Write-Host "[5/6] Sao chep du lieu da thu thap vao thu muc QTIFW..." -ForegroundColor Yellow
Copy-Item -Path "$TEMP_DEPLOY_DIR\*" -Destination "$DATA_DIR\" -Recurse -Force
Write-Host " Sao chep hoan tat." -ForegroundColor Green

# ==================== TAO BAN CAI DAT SETUP ====================
Write-Host "[6/6] Bien dich bo cai dat installer bang binarycreator..." -ForegroundColor Yellow
$OUTPUT_INSTALLER = "$INSTALLER_BUILD_DIR\CentralLoggerSetup.exe"
if (Test-Path $OUTPUT_INSTALLER) {
    Remove-Item -Path $OUTPUT_INSTALLER -Force
}

& "$IFW_DIR\bin\binarycreator.exe" --offline-only -c "$INSTALLER_BUILD_DIR\config\config.xml" -p "$INSTALLER_BUILD_DIR\packages" "$OUTPUT_INSTALLER"

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $OUTPUT_INSTALLER)) {
    Write-Error "binarycreator that bai (exit code $LASTEXITCODE). Kiem tra config.xml / control script o tren."
}

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "DONG GOI THANH CONG!" -ForegroundColor Green
Write-Host "File cai dat da duoc tao tai:" -ForegroundColor Green
Write-Host "$OUTPUT_INSTALLER" -ForegroundColor Yellow -NoNewline
Write-Host " (~$([Math]::Round((Get-Item $OUTPUT_INSTALLER).Length / 1MB, 2)) MB)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Green
