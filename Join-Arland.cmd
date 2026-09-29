@echo off
rem Join a host from this development folder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ACR-Multiplayer.ps1" -Mode Join
