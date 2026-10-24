# ACA Field Engineering launcher (Play-ACA-FieldEngineering.cmd runs this).
# Finds Arma Reforger through Steam, then starts it with Arma Commander: Actual and Better Battle
# Buddies plus this addon, loaded unpacked from this folder. The map loads straight away and
# ACA's in-game setup screen follows.
#   -GamePath <ArmaReforgerSteam.exe>   skip the Steam search
#   -Map 1..5                           skip the map menu
#   -Extra "<GUID,GUID>"                more Workshop mods to load (comma-separated GUIDs)
#   -Options "<options>"                more launch options, e.g. "-acafeMode realistic"
param([string]$GamePath = '', [string]$Map = '', [string]$Extra = '', [string]$Options = '')
$ErrorActionPreference = 'Stop'

$AddonGuid = 'F4EF6A0A51B808A8'
$AcaGuid = 'AC230926A0010001'
$BbbGuid = 'BBB0261015000001'

# Every Steam library (registry, then each library's libraryfolders.vdf); the first one with
# Arma Reforger installed wins.
function Find-Reforger {
  $roots = @()
  foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
    try {
      $props = Get-ItemProperty -LiteralPath $key -ErrorAction Stop
      foreach ($name in @('SteamPath', 'InstallPath')) {
        if ($props.$name) { $roots += ([string]$props.$name).Replace('/', '\') }
      }
    } catch {}
  }
  $libraries = @()
  foreach ($root in ($roots | Select-Object -Unique)) {
    $libraries += $root
    $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
    if (!(Test-Path -LiteralPath $vdf)) { continue }
    $text = Get-Content -LiteralPath $vdf -Raw
    foreach ($m in [regex]::Matches($text, '"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\', '\') }
  }
  foreach ($library in ($libraries | Select-Object -Unique)) {
    $exe = Join-Path $library 'steamapps\common\Arma Reforger\ArmaReforgerSteam.exe'
    if (Test-Path -LiteralPath $exe) { return $exe }
  }
  return $null
}

# The folder of the addon with this GUID directly inside $dir (Workshop folders are Name_GUID).
function Find-Addon([string]$dir, [string]$guid) {
  if (!(Test-Path -LiteralPath $dir)) { return $null }
  foreach ($folder in Get-ChildItem -LiteralPath $dir -Directory -ErrorAction SilentlyContinue) {
    $gproj = Join-Path $folder.FullName 'addon.gproj'
    if (!(Test-Path -LiteralPath $gproj)) { continue }
    if ((Get-Content -LiteralPath $gproj -Raw) -match ('GUID\s+"' + $guid + '"')) { return $folder.FullName }
  }
  return $null
}

function Stop-WithMessage([string]$message) {
  Write-Host ''
  Write-Host $message -ForegroundColor Red
  Write-Host ''
  Read-Host 'Press Enter to close'
  exit 1
}

try {
  if (!$GamePath) { $GamePath = Find-Reforger }
  if (!$GamePath -or !(Test-Path -LiteralPath $GamePath)) {
    Stop-WithMessage ("Arma Reforger (ArmaReforgerSteam.exe) was not found in any Steam library.`n" +
      "Start it once from Steam, or run this from PowerShell with its path:`n" +
      "  .\Start-FieldEngineering.ps1 -GamePath 'D:\SteamLibrary\steamapps\common\Arma Reforger\ArmaReforgerSteam.exe'")
  }

  $here = $PSScriptRoot
  $dev = Split-Path $here -Parent
  $workshop = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\ArmaReforger\addons'

  # One copy of each addon only (the game refuses two with the same GUID). A development copy of
  # ACA next to this folder (its contributor layout, with BBB beside it) is used instead of the
  # Workshop one; otherwise ACA and BBB come from the Workshop downloads.
  $devAca = Find-Addon $dev $AcaGuid
  if ($devAca) {
    if (!(Find-Addon $dev $BbbGuid)) {
      Stop-WithMessage ("A development copy of ACA is next to this addon:`n  $devAca`n" +
        "but Better Battle Buddies isn't. Put BBB (the commit pinned in ACA's tools\bbb-source.json) next to it, " +
        "or move this addon folder somewhere else to use the Workshop copies.")
    }
    $addonsDir = $dev
    Write-Host "Using the development ACA and BBB in $dev"
  }
  else {
    if (!(Find-Addon $workshop $AcaGuid)) { Stop-WithMessage "Arma Commander: Actual is not downloaded. Subscribe to it in the game's Workshop (and start the game once so it downloads)." }
    if (!(Find-Addon $workshop $BbbGuid)) { Stop-WithMessage "Better Battle Buddies is not downloaded. Subscribe to it in the game's Workshop." }
    $addonsDir = $workshop
    if ((Resolve-Path -LiteralPath $dev).Path.TrimEnd('\') -ne (Resolve-Path -LiteralPath $workshop).Path.TrimEnd('\')) { $addonsDir = "$workshop,$dev" }
  }

  $addons = "$AcaGuid,$BbbGuid,$AddonGuid"
  if ($Extra) { $addons += ',' + $Extra.Trim([char[]]', ') }

  if (!$Map) {
    Write-Host ''
    Write-Host ' ACA Field Engineering' -ForegroundColor Cyan
    Write-Host ''
    Write-Host '   1  Kolguyev'
    Write-Host '   2  Arland'
    Write-Host '   3  Everon'
    Write-Host '   4  ACA test range (small, loads fast, no setup screen)'
    Write-Host '   5  Kolguyev, AI commander engineering test (the AI builds and mines often, shown on your map)'
    Write-Host ''
    $Map = (Read-Host '  Map [1-5, Enter = 1]').Trim()
  }
  $world = 'Worlds/ACR_Kolguyev.ent'
  $setup = '-acrSetup 1'
  switch ($Map) {
    '2' { $world = 'Worlds/ACR_Arland.ent' }
    '3' { $world = 'Worlds/ACR_Everon.ent' }
    '4' { $world = 'Worlds/ACR_TestRange.ent'; $setup = '' }
    '5' { $setup = '-acrSetup 1 -acafeAITest' }
  }

  $arguments = "-addonsDir `"$addonsDir`" -addons $addons -noSplash -noBackend -world `"$world`" $setup $Options"
  Write-Host ''
  Write-Host "Starting $world ..."
  Start-Process -FilePath $GamePath -ArgumentList $arguments.Trim() -WorkingDirectory (Split-Path $GamePath -Parent)
}
catch {
  Stop-WithMessage $_.Exception.Message
}
