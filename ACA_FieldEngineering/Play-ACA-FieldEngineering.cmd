@echo off
rem ACA Field Engineering: double-click to play (Start-FieldEngineering.ps1 does the work).
rem It finds Arma Reforger through Steam and loads ACA, BBB and this addon.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-FieldEngineering.ps1" %*
