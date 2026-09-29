@echo off
rem Same as Host-Arland.cmd but without -noBackend (try if joining fails)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\ArmaCommanderReforger\ACR-Multiplayer.ps1" -Mode Host -Online
