@echo off
rem Packages ARES-VI as a standalone Windows build. Extra arguments are passed to Package-Windows.ps1,
rem e.g.  Package-Windows.bat -EngineDir "C:\Program Files\Epic Games\UE_5.3" -Configuration Development
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Package-Windows.ps1" %*
pause
