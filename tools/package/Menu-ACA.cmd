@echo off
rem AC: Actual: the game's own main menu with the mod loaded (Scenarios lists AC: Actual - Arland / Everon,
rem which open with the in-game match setup screen). No scenario is started for you.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ArmaCommanderReforger\Start-Test-Range.ps1" -HostMenu
