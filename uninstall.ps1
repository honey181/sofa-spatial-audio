# Uninstall script for SOFA Spatial Audio (Restores original Microsoft HrtfDspCpu.dll)
# Requires Run as Administrator

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Warning "Administrator rights required to restore system audio DLL."
    Write-Host "Please right-click PowerShell and select 'Run as Administrator', then re-run uninstall.ps1." -ForegroundColor Yellow
    exit 1
}

$system32 = "$env:WINDIR\System32"
$targetDll = Join-Path $system32 "HrtfDspCpu.dll"
$backupDll = Join-Path $system32 "HrtfDspCpu_orig.dll"

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "   Restoring Original Microsoft HrtfDspCpu.dll           " -ForegroundColor Cyan
Write-Host "=========================================================`n" -ForegroundColor Cyan

if (-not (Test-Path $backupDll)) {
    Write-Error "Backup file not found at: $backupDll. Cannot restore automatically."
    exit 1
}

# 1. Stop Audio Service
Write-Host "[1/3] Stopping Windows Audio Service (audiosrv)..." -ForegroundColor Yellow
Stop-Service -Name "audiosrv" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# 2. Restore original DLL
Write-Host "[2/3] Restoring original Microsoft HrtfDspCpu.dll..." -ForegroundColor Green
Copy-Item -Path $backupDll -Destination $targetDll -Force
Remove-Item -Path $backupDll -Force
& icacls.exe $targetDll /grant "ALL APPLICATION PACKAGES:RX" /q | Out-Null
& icacls.exe $targetDll /grant "Users:RX" /q | Out-Null

# 3. Restart Audio Service
Write-Host "[3/3] Restarting Windows Audio Service..." -ForegroundColor Green
Start-Service -Name "audiosrv"
Start-Sleep -Seconds 1

Write-Host "`nSUCCESS! Original Microsoft Windows Sonic DLL restored." -ForegroundColor Green
