# Finds a Steam game or tool on this PC, wherever it is installed:
#   1. a location remembered from last time (game-paths.txt next to this file),
#   2. every Steam library (Steam's registry keys + libraryfolders.vdf),
#   3. Windows' list of installed programs,
#   4. common drive letters,
# and if all that fails, asks the player to point at the .exe once (remembered).
function Find-SteamApp([string]$folder, [string]$exe) {
  $memory = Join-Path $PSScriptRoot 'game-paths.txt'
  $remembered = @{}
  if (Test-Path -LiteralPath $memory) {
    foreach ($line in Get-Content -LiteralPath $memory) {
      $parts = $line.Split('|')
      if ($parts.Count -eq 2) { $remembered[$parts[0]] = $parts[1] }
    }
  }
  if ($remembered.ContainsKey($exe) -and (Test-Path -LiteralPath $remembered[$exe])) { return $remembered[$exe] }

  $roots = @()
  foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
    $item = Get-ItemProperty $key -ErrorAction SilentlyContinue
    if ($item -and $item.SteamPath) { $roots += $item.SteamPath }
    if ($item -and $item.InstallPath) { $roots += $item.InstallPath }
  }
  $libraries = @()
  foreach ($root in $roots) {
    $libraries += $root
    $libraryFile = Join-Path $root 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryFile) {
      foreach ($m in [regex]::Matches((Get-Content -LiteralPath $libraryFile -Raw), '"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\', '\') }
    }
  }
  foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
    $libraries += (Join-Path $drive.Root 'SteamLibrary')
    $libraries += (Join-Path $drive.Root 'Steam')
    $libraries += (Join-Path $drive.Root 'Program Files (x86)\Steam')
    $libraries += (Join-Path $drive.Root 'Program Files\Steam')
    $libraries += (Join-Path $drive.Root 'Games\Steam')
    $libraries += (Join-Path $drive.Root 'Games\SteamLibrary')
  }
  foreach ($library in ($libraries | Select-Object -Unique)) {
    if (!$library) { continue }
    $candidate = Join-Path $library "steamapps\common\$folder\$exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }
  }
  # Windows' installed-programs list (Steam registers every game there).
  foreach ($uninstall in 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*') {
    foreach ($entry in (Get-ItemProperty $uninstall -ErrorAction SilentlyContinue)) {
      if ($entry.DisplayName -ne $folder -or !$entry.InstallLocation) { continue }
      $candidate = Join-Path $entry.InstallLocation $exe
      if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
  }

  # Still nothing: ask once, and remember the answer.
  Write-Host ''
  Write-Host "Could not find $folder automatically." -ForegroundColor Yellow
  Write-Host "A window will open: please select $exe (usually in ...\steamapps\common\$folder\)."
  try {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = "Where is $exe ($folder)?"
    $dialog.Filter = "$exe|$exe|Programs (*.exe)|*.exe"
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK -and (Test-Path -LiteralPath $dialog.FileName)) {
      $chosen = $dialog.FileName
      Add-Content -LiteralPath $memory -Value ($exe + '|' + $chosen)
      return $chosen
    }
  } catch {
    $typed = Read-Host "Paste the full path to $exe"
    if ($typed -and (Test-Path -LiteralPath $typed.Trim('"'))) {
      Add-Content -LiteralPath $memory -Value ($exe + '|' + $typed.Trim('"'))
      return $typed.Trim('"')
    }
  }
  return $null
}

# Where downloaded mods (faction mods, ACE, the expansion packs' and map add-ons' Workshop
# dependencies) live: $env:ACR_MODS_DIR if set, the game's own Workshop folder
# (Documents\My Games\ArmaReforger\addons) and E:\ArmaReforgerMods\addons (the maintainer's).
# Every one that holds mods is appended: a Workshop folder with only 'saves' in it used to end
# the search, so a PC with its mods on E: launched without them.
function Add-ModAddons([string]$addonsDir) {
  $candidates = @()
  if ($env:ACR_MODS_DIR) { $candidates += $env:ACR_MODS_DIR }
  $docs = [Environment]::GetFolderPath('MyDocuments')
  if ($docs) { $candidates += (Join-Path $docs 'My Games\ArmaReforger\addons') }
  $candidates += 'E:\ArmaReforgerMods\addons'
  $found = $false
  foreach ($dir in $candidates) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    $mod = Get-ChildItem -LiteralPath $dir -Directory -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -match '_[0-9A-Fa-f]{16}$' -or (Test-Path -LiteralPath (Join-Path $_.FullName 'addon.gproj')) } | Select-Object -First 1
    if (!$mod) { continue }
    $found = $true
    if (($addonsDir.Split(',') | ForEach-Object { $_.TrimEnd('\') }) -contains $dir.TrimEnd('\')) { continue }
    $addonsDir += ',' + $dir
  }
  if (!$found) { Write-Host 'Mods folder not found (set ACR_MODS_DIR to it); starting without it.' -ForegroundColor Yellow }
  return $addonsDir
}

# A Workshop/mod copy of an optional mod: its folder, or '' when it isn't installed.
function Find-WorkshopMod([string]$modId) {
  $candidates = @()
  $docs = [Environment]::GetFolderPath('MyDocuments')
  if ($docs) { $candidates += (Join-Path $docs 'My Games\ArmaReforger\addons') }
  $candidates += 'E:\ArmaReforgerMods\addons'
  foreach ($dir in $candidates) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    $hit = Get-ChildItem -LiteralPath $dir -Directory -Filter "*_$modId" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName }
  }
  return ''
}

# All-in-One Command Menu (separate mod, 629405AEFC950C12): when its Workshop copy is
# downloaded on this PC, play/host/join load it too (the menus' AiO row, yes by default). Only its own folder goes
# FIRST in -addonsDir, so its Workshop copy wins without exposing Workshop ACA/BBB copies
# ahead of the launcher's local addons. With duplicate mod IDs the first folder wins.
# Returns @(addonsDir, addonList), unchanged when it isn't installed.
function Add-AIOCommandMenu([string]$addonsDir, [string]$addonList) {
  $aioId = '629405AEFC950C12'
  $aioDir = Find-WorkshopMod $aioId
  if (!$aioDir) { return @($addonsDir, $addonList) }
  $dirs = @($aioDir) + @($addonsDir.Split(',') | Where-Object { $_ -and $_.TrimEnd('\') -ne $aioDir.TrimEnd('\') })
  if (($addonList.Split(',')) -notcontains $aioId) { $addonList += ',' + $aioId }
  Write-Host 'All-in-One Command Menu found (Workshop copy): loading it too.' -ForegroundColor DarkCyan
  return @(($dirs -join ','), $addonList)
}

# Civilians: Shadow Company Civil Population plus ACA's compat add-on (ACACivilPopulation,
# AC230926CC000001): a copy beside ACA (development, ZIP) or its Workshop copy. Offered only
# when both are installed.
$script:SCCPId = 'BCE0D49568A9E6BF'
$script:SCCPCompatId = 'AC230926CC000001'
function Find-CivilPopulationCompat {
  $project = Join-Path (Split-Path $PSScriptRoot -Parent) 'ACACivilPopulation\addon.gproj'
  if ((Test-Path -LiteralPath $project) -and (Get-Content -LiteralPath $project -Raw) -match ('GUID\s+"' + $script:SCCPCompatId + '"')) { return (Split-Path $project -Parent) }
  return (Find-WorkshopMod $script:SCCPCompatId)
}

function Test-CivilPopulation {
  if (!(Find-CivilPopulationCompat)) { return $false }
  return (Find-WorkshopMod $script:SCCPId) -ne ''
}

function Add-CivilPopulation([string]$addonsDir, [string]$addonList) {
  if (!(Test-CivilPopulation)) { return @($addonsDir, $addonList) }
  $dirs = @($addonsDir.Split(',') | Where-Object { $_ })
  foreach ($modDir in @((Find-WorkshopMod $script:SCCPId), (Find-CivilPopulationCompat))) {
    $parent = Split-Path $modDir -Parent
    if (($dirs | ForEach-Object { $_.TrimEnd('\') }) -notcontains $parent.TrimEnd('\')) { $dirs += $parent }
  }
  foreach ($id in @($script:SCCPId, $script:SCCPCompatId)) { if (($addonList.Split(',')) -notcontains $id) { $addonList += ',' + $id } }
  Write-Host 'Civil Population and the ACA compat add-on: loading civilians.' -ForegroundColor DarkCyan
  return @(($dirs -join ','), $addonList)
}

# Personal mods: extra Workshop mods the player always wants loaded (visual and sound mods). The list is
# local and git-excluded: personal-mods.txt beside this file, one 16-digit mod ID per line, '#' starts
# a comment. Without the file nothing is loaded and the menus show no row. Each mod loads only when
# its Workshop copy is on this PC. They go last in -addonsDir, so local ACA/BBB copies still win.
function Get-PersonalModIds {
  $file = Join-Path $PSScriptRoot 'personal-mods.txt'
  $ids = @()
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $id = ($line -replace '#.*$', '').Trim()
      if ($id -match '^[0-9A-Fa-f]{16}$') { $ids += $id.ToUpperInvariant() }
    }
  }
  return $ids
}

function Test-PersonalMods {
  foreach ($id in @(Get-PersonalModIds)) { if (Find-WorkshopMod $id) { return $true } }
  return $false
}

function Add-PersonalMods([string]$addonsDir, [string]$addonList) {
  $dirs = @($addonsDir.Split(',') | Where-Object { $_ })
  $count = 0
  foreach ($id in @(Get-PersonalModIds)) {
    $modDir = Find-WorkshopMod $id
    if (!$modDir) { continue }
    $parent = Split-Path $modDir -Parent
    if (($dirs | ForEach-Object { $_.TrimEnd('\') }) -notcontains $parent.TrimEnd('\')) { $dirs += $parent }
    if (($addonList.Split(',')) -notcontains $id) { $addonList += ',' + $id }
    $count++
  }
  if ($count -gt 0) { Write-Host ('Personal mods: loading ' + $count + ' of ' + @(Get-PersonalModIds).Count + '.') -ForegroundColor DarkCyan }
  return @(($dirs -join ','), $addonList)
}

# Project Outbreak (zombie mode, issue #107): Outbreak itself, its Creature Melee AI dependency and
# Special Zombies & Creatures, all Workshop mods. The optional contagion mod (Project Outbreak
# Infection) is separate and off unless asked for. Resolved like every optional mod, via Find-WorkshopMod.
$script:OutbreakIds = @('4FD4A1BD550793F4', 'F1D4B9B5245B36ED', '297640FBAC0C46C3') # Creature Melee AI, Project Outbreak, Special Zombies & Creatures
$script:OutbreakContagionId = '7B763100BAFA1F9A'
# ACA's zombie add-on (ACAOutbreak, AC230926CC000004): depends on the three mods above and adds the
# zombie scenarios. Loaded alongside them when found (a copy beside ACA or its Workshop copy); ACA's
# zombie mode itself needs only the mods.
$script:OutbreakCompatId = 'AC230926CC000004'
function Find-OutbreakCompat {
  $project = Join-Path (Split-Path $PSScriptRoot -Parent) 'ACAOutbreak\addon.gproj'
  if ((Test-Path -LiteralPath $project) -and (Get-Content -LiteralPath $project -Raw) -match ('GUID\s+"' + $script:OutbreakCompatId + '"')) { return (Split-Path $project -Parent) }
  return (Find-WorkshopMod $script:OutbreakCompatId)
}

function Test-ProjectOutbreak {
  foreach ($id in $script:OutbreakIds) { if (!(Find-WorkshopMod $id)) { return $false } }
  return $true
}

function Add-ProjectOutbreak([string]$addonsDir, [string]$addonList, [bool]$contagion = $false) {
  if (!(Test-ProjectOutbreak)) {
    Write-Host 'Project Outbreak (or Creature Melee AI / Special Zombies & Creatures) is not downloaded: starting without zombies.' -ForegroundColor Yellow
    return @($addonsDir, $addonList)
  }
  $ids = @($script:OutbreakIds)
  if ($contagion) {
    if (Find-WorkshopMod $script:OutbreakContagionId) { $ids += $script:OutbreakContagionId }
    else { Write-Host 'The contagion mod is not downloaded: starting without it.' -ForegroundColor Yellow }
  }
  $dirs = @($addonsDir.Split(',') | Where-Object { $_ })
  foreach ($id in $ids) {
    $parent = Split-Path (Find-WorkshopMod $id) -Parent
    if (($dirs | ForEach-Object { $_.TrimEnd('\') }) -notcontains $parent.TrimEnd('\')) { $dirs += $parent }
    if (($addonList.Split(',')) -notcontains $id) { $addonList += ',' + $id }
  }
  $what = 'Project Outbreak: loading ' + $ids.Count + ' zombie mods'
  $compat = Find-OutbreakCompat
  if ($compat) {
    $parent = Split-Path $compat -Parent
    if (($dirs | ForEach-Object { $_.TrimEnd('\') }) -notcontains $parent.TrimEnd('\')) { $dirs += $parent }
    if (($addonList.Split(',')) -notcontains $script:OutbreakCompatId) { $addonList += ',' + $script:OutbreakCompatId }
    $what += ' and the ACA zombie add-on'
  }
  Write-Host ($what + '.') -ForegroundColor DarkCyan
  return @(($dirs -join ','), $addonList)
}

# On-screen banner for a development copy that opens the game on the maintainer's screen
# (cross-mod rule; ACR-Watcher.ps1 does the same for automated tests). Packaged copies have no
# .git folder or tools\ folder, so players never see it.
function Start-PlayBanner([string]$title, [string[]]$lines, [int]$gamePid) {
  try {
    $script = Join-Path $PSScriptRoot 'tools\Show-TestBanner.ps1'
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git')) -or !(Test-Path -LiteralPath $script) -or $gamePid -le 0) { return }
    $started = Get-Date
    $spec = Join-Path $env:TEMP ('aca-play-' + $gamePid + '.banner.json')
    @{ title = $title; lines = @($lines); timeoutSec = 14400; started = $started.ToString('o') } | ConvertTo-Json | Set-Content -LiteralPath $spec -Encoding UTF8
    $bannerArgs = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $script + '" -SpecFile "' + $spec + '" -WatchPid ' + $gamePid
    Start-Process -FilePath 'powershell.exe' -ArgumentList $bannerArgs -WindowStyle Hidden | Out-Null
  } catch { Write-Host ('Banner not shown: ' + $_.Exception.Message) -ForegroundColor Yellow }
}

# The optional mods picked in the Play/Host/Join menu (AiO, Civilians, Personal, Zombies = yes/no).
function Add-OptionalMods([string]$addonsDir, [string]$addonList, $choices) {
  $r = @($addonsDir, $addonList)
  if ($choices.AiO -ne 'no') { $r = Add-AIOCommandMenu $r[0] $r[1] }
  if ($choices.Civilians -eq 'yes') { $r = Add-CivilPopulation $r[0] $r[1] }
  if ($choices.Personal -ne 'no') { $r = Add-PersonalMods $r[0] $r[1] }
  if ($choices.Zombies -and $choices.Zombies -ne 'no') { $r = Add-ProjectOutbreak $r[0] $r[1] ($choices.Infection -eq 'yes') }
  return $r
}

# BBB is a required standalone addon. Development and ZIP copies keep it beside ACA.
function Assert-BBBDependency([string]$addonsDir) {
  foreach ($dir in $addonsDir.Split(',')) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    foreach ($addon in Get-ChildItem -LiteralPath $dir -Directory) {
      $project = Join-Path $addon.FullName 'addon.gproj'
      if ((Test-Path -LiteralPath $project) -and (Get-Content -LiteralPath $project -Raw) -match 'GUID\s+"BBB0261015000001"') { return }
    }
  }
  throw 'Better Battle Buddies is required. Install the complete ACActual-WithBBB.zip with both addon folders, or use the in-game Workshop installation.'
}
