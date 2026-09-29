@echo off
rem Same as Join-Arland.cmd but without -noBackend (try if joining fails)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\ArmaCommanderReforger\ACR-Multiplayer.ps1" -Mode Join -Online
