# Deploy script for SOFA Spatial Audio (Replaces HrtfDspCpu.dll with proxy)
# Requires Run as Administrator

$ErrorActionPreference = "Stop"

# Verify Administrator Privileges
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warning "Administrator rights required to install system audio proxy."
    Write-Host "Please right-click PowerShell and select 'Run as Administrator', then re-run deploy.ps1." -ForegroundColor Yellow
    exit 1
}

$system32 = "$env:WINDIR\System32"
$targetDll = Join-Path $system32 "HrtfDspCpu.dll"
$backupDll = Join-Path $system32 "HrtfDspCpu_orig.dll"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceDll = Join-Path $projectRoot "bin\HrtfDspCpu.dll"

if (-not (Test-Path $sourceDll)) {
    Write-Error "Built proxy DLL not found at: $sourceDll. Please run build.ps1 first."
    exit 1
}

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "   Deploying SOFA Spatial Audio System Proxy              " -ForegroundColor Cyan
Write-Host "=========================================================`n" -ForegroundColor Cyan

# 1. Stop Audio Service
Write-Host "[1/4] Stopping Windows Audio Service (audiosrv)..." -ForegroundColor Yellow
Stop-Service -Name "audiosrv" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# 2. Backup original DLL
if (-not (Test-Path $backupDll)) {
    Write-Host "[2/4] Backing up original Microsoft HrtfDspCpu.dll..." -ForegroundColor Green
    & takeown.exe /f $targetDll /a | Out-Null
    & icacls.exe $targetDll /grant "Administrators:F" /q | Out-Null
    Copy-Item -Path $targetDll -Destination $backupDll -Force
    Write-Host "      Backup created at: $backupDll" -ForegroundColor Green
} else {
    Write-Host "[2/4] Backup already exists at: $backupDll" -ForegroundColor Gray
}

# 3. Copy our proxy DLL into System32
Write-Host "[3/4] Installing proxy HrtfDspCpu.dll into System32..." -ForegroundColor Green
& takeown.exe /f $targetDll /a | Out-Null
& icacls.exe $targetDll /grant "Administrators:F" /q | Out-Null
Copy-Item -Path $sourceDll -Destination $targetDll -Force

# Grant read/execute permissions so audiodg.exe (LOCAL SERVICE) can load the DLL
& icacls.exe $targetDll /grant "ALL APPLICATION PACKAGES:RX" /q | Out-Null
& icacls.exe $targetDll /grant "Users:RX" /q | Out-Null
& icacls.exe $targetDll /grant "NT AUTHORITY\LOCAL SERVICE:RX" /q | Out-Null

# Copy config folder and SOFA datasets to common ProgramData
$configDir = "C:\ProgramData\SofaSpatialAudio"
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}
Copy-Item -Path (Join-Path $projectRoot "config\*") -Destination $configDir -Recurse -Force
& icacls.exe $configDir /grant "ALL APPLICATION PACKAGES:(OI)(CI)M" /q | Out-Null
& icacls.exe $configDir /grant "Users:(OI)(CI)M" /q | Out-Null
& icacls.exe $configDir /grant "NT AUTHORITY\LOCAL SERVICE:(OI)(CI)M" /q | Out-Null
Write-Host "      Config & SOFA files installed to: $configDir" -ForegroundColor Green

# 4. Restart Audio Service
Write-Host "[4/4] Restarting Windows Audio Service..." -ForegroundColor Green
Start-Service -Name "audiosrv"
Start-Sleep -Seconds 1

Write-Host "`nSUCCESS! SOFA Spatial Audio proxy is now active system-wide." -ForegroundColor Green
Write-Host "Make sure 'Windows Sonic for Headphones' is selected in your Windows Sound Settings." -ForegroundColor Yellow
