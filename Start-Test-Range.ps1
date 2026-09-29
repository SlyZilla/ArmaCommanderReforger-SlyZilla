param([switch]$HostMenu, [string]$GamePath)
$ErrorActionPreference = 'Stop'
try {
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
 $profilePath = Join-Path $PSScriptRoot '.local-game'
 $arguments = '-addonsDir "' + $addonsPath + '" -addons AC230926A0010001 -profile "' + $profilePath + '" -noSplash'
 if (!$HostMenu) { $arguments += ' -noBackend -world "Worlds/ACR_TestRange.ent"' }
 # User-invoked interactive game launcher.
 Start-Process -FilePath $GamePath -ArgumentList $arguments -WorkingDirectory (Split-Path $GamePath -Parent)
} catch {
 Write-Host $_.Exception.Message -ForegroundColor Red
 Read-Host 'Press Enter to close'
 exit 1
}

