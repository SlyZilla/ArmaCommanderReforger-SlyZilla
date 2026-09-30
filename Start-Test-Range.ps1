param([switch]$HostMenu, [switch]$TestProfile, [string]$GamePath, [string]$Extra = '', [string]$World = 'Worlds/ACR_TestRange.ent')
$ErrorActionPreference = 'Stop'
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
  $steamRoot = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  $libraries = @($steamRoot, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
  if ($steamRoot) {
   $libraryFile = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
   if (Test-Path -LiteralPath $libraryFile) {
    $matches = [regex]::Matches((Get-Content -LiteralPath $libraryFile -Raw), '"path"\s+"([^"]+)"')
    foreach ($match in $matches) { $libraries += $match.Groups[1].Value.Replace('\\','\') }
   }
  }
  foreach ($library in $libraries) {
   if (!$library) { continue }
   $candidate = Join-Path $library 'steamapps\common\Arma Reforger\ArmaReforgerSteam.exe'
   if (Test-Path -LiteralPath $candidate) { $GamePath = $candidate; break }
  }
 }
 if (!$GamePath -or !(Test-Path -LiteralPath $GamePath)) { throw 'Arma Reforger was not found. Run this script with -GamePath pointing to ArmaReforgerSteam.exe.' }
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

