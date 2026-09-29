@echo off
rem Automated regression run: leave the game alone for ~100 seconds, then close it.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Test-Range.ps1" -Extra "-acrCommandTest 1"
