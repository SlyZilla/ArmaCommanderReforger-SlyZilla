@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Mod-Release-Console.ps1"
if errorlevel 1 pause
