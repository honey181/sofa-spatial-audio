@echo off
echo ========================================================
echo  Deploying SOFA Spatial Audio with Administrator Rights
echo ========================================================
powershell -Command "Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy Bypass -NoExit -File \"%~dp0deploy.ps1\"'"
