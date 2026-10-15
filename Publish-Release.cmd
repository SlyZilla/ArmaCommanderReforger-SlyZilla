@echo off
rem Legacy GitHub ZIP publication. Workshop updates use Mod-Release-Console.cmd.
echo This publishes the fallback ZIP to GitHub. For Workshop use Mod-Release-Console.cmd.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Publish-Release.ps1"
pause
