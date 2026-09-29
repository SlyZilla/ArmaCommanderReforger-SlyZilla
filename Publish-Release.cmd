@echo off
rem Upload the current package (dist\ArmaCommander-Arland.zip) as a GitHub release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Publish-Release.ps1"
pause
