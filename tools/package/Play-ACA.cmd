@echo off
rem AC: Actual (singleplayer; pick the map in the setup menu)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ArmaCommanderReforger\Start-Test-Range.ps1" -World "Worlds/ACR_Arland.ent"
