param([switch]$HostMenu, [switch]$TestProfile, [switch]$NoSetup, [string]$GamePath, [string]$Extra = '', [string]$World = 'Worlds/ACR_TestRange.ent')
$ErrorActionPreference = 'Stop'
# Everything this window shows also goes to launcher-log.txt (send it if something fails).
try { Start-Transcript -Path (Join-Path $PSScriptRoot 'launcher-log.txt') -Force | Out-Null } catch {}
try {
 # Packaged copies keep themselves up to date (the development folder never updates).
 if (!$TestProfile) {
  . (Join-Path $PSScriptRoot 'ACR-Update.ps1')
  if (Update-FromGitHub) {
   $again = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-World', $World)
   if ($HostMenu) { $again += '-HostMenu' }
   if ($Extra) { $again += @('-Extra', $Extra) }
   if ($GamePath) { $again += @('-GamePath', $GamePath) }
   & powershell.exe @again
   exit $LASTEXITCODE
  }
 }
 if (!$GamePath) {
  . (Join-Path $PSScriptRoot 'ACR-Find.ps1')
  $GamePath = Find-SteamApp 'Arma Reforger' 'ArmaReforgerSteam.exe'
 }
 if (!$GamePath -or !(Test-Path -LiteralPath $GamePath)) { throw 'Arma Reforger was not found. Run this script with -GamePath pointing to ArmaReforgerSteam.exe.' }
 # Scenario worlds (not the test range, not automated tests): the host options menu, as when
 # hosting (Enter starts with the remembered settings). The game is its own server here.
 if (!$TestProfile -and !$HostMenu -and !$NoSetup -and $World -notlike '*TestRange*') {
  . (Join-Path $PSScriptRoot 'ACR-HostSetup.ps1')
  $setup = Show-HostSetup -Solo
  $World = $setup.World
  $Extra = ($Extra + $setup.Args).Trim()
 }
 $addonsPath = Split-Path $PSScriptRoot -Parent
 # Players get their normal profile (controls, graphics); automated tests use a separate one.
 $arguments = '-addonsDir "' + $addonsPath + '" -addons AC230926A0010001 -noSplash'
 if ($TestProfile) { $arguments += ' -profile "' + (Join-Path $PSScriptRoot '.local-game') + '"' }
 if (!$HostMenu) { $arguments += ' -noBackend -world "' + $World + '"' }
 if ($Extra) { $arguments += ' ' + $Extra }
 # User-invoked interactive game launcher.
 Start-Process -FilePath $GamePath -ArgumentList $arguments -WorkingDirectory (Split-Path $GamePath -Parent)
} catch {
 Write-Host $_.Exception.Message -ForegroundColor Red
 Read-Host 'Press Enter to close'
 exit 1
}

