@echo off
rem Try the in-game match setup screen (aca-f21): Kolguyev (generated layout, aca-f07) with the screen forced on.
rem (The game's own Scenarios list only shows Workshop items, so this is the way in until we publish.)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Test-Range.ps1" -NoSetup -World "Worlds/ACR_Kolguyev.ent" -Extra "-acrSetup 1"
