@echo off
rem Host the Arland scenario from this development folder (always the latest build)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ACR-Multiplayer.ps1" -Mode Host
