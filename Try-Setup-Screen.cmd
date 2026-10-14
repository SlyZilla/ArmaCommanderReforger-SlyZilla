@echo off
rem Try the in-game match setup screen (aca-f21): Arland with the screen forced on, vanilla factions.
rem (The game's own Scenarios list only shows Workshop items, so this is the way in until we publish.)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Test-Range.ps1" -NoSetup -World "Worlds/ACR_Arland.ent" -Extra "-acrSetup 1"
