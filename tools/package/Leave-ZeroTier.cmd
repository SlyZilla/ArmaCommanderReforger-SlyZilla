@echo off
rem Disconnect this PC from the AC: Actual ZeroTier network (asks first)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ArmaCommanderReforger\ACR-Multiplayer.ps1" -Mode Leave
