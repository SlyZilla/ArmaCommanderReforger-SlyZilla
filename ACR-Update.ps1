# Self-update from GitHub releases (package copies only; the development folder,
# which has a .git folder, is never touched). The repository comes from
# update-source.txt ("owner/name"); the installed version from VERSION.txt.
# Returns $true when a new version was installed (the caller should restart).
#
# Recoverable (R02, 5 Oct review): the download is checked before anything is touched
# (the expected asset name, one package root, the addon's mod ID, the version, the
# required files); the versioned content is backed up, replaced, checked, and put back
# from the backup if anything fails. A run that was cut off half way is rolled back on
# the next launch. Local state (host/play settings, last host, zerotier.txt, logs,
# profiles) is never part of the package and is left alone.

$script:ACR_AssetName = 'ACActual-Arland.zip'
$script:ACR_PackageTop = 'ACActual-Arland'
$script:ACR_AddonFolder = 'ArmaCommanderReforger'
$script:ACR_ModId = 'AC230926A0010001'
# Versioned content, as tools/make_package.py builds it. Folders are replaced whole, so
# files removed upstream disappear (Prefabs included).
$script:ACR_AddonDirs = @('Configs', 'Missions', 'Prefabs', 'Scripts', 'Worlds')
$script:ACR_AddonFiles = @('LICENSE.md', 'addon.gproj', 'resourceDatabase.rdb', 'Start-Test-Range.ps1', 'ACR-Multiplayer.ps1', 'ACR-Update.ps1', 'ACR-Find.ps1', 'ACR-ZeroTier.ps1', 'ACR-HostSetup.ps1', 'Play-Test-Range.cmd', 'README.md', 'MISSION-MAKING.md', 'VERSION.txt', 'update-source.txt')
$script:ACR_RootItems = @('READ ME FIRST.txt', 'Play-ACA.cmd', 'Host-ACA.cmd', 'Join-ACA.cmd', 'Leave-ZeroTier.cmd', 'If joining fails')
$script:ACR_Required = @('addon.gproj', 'VERSION.txt', 'ACR-Update.ps1', 'ACR-Multiplayer.ps1', 'Start-Test-Range.ps1', 'Scripts', 'Worlds', 'Configs', 'Missions', 'Prefabs')

# The package's versioned items as (relative path, full path) under the package root
# (the folder that holds ArmaCommanderReforger and the Play/Host/Join files).
function Get-ACRVersionedItems([string]$packageRoot) {
  $items = @()
  foreach ($d in $script:ACR_AddonDirs) { $items += (Join-Path $script:ACR_AddonFolder $d) }
  foreach ($f in $script:ACR_AddonFiles) { $items += (Join-Path $script:ACR_AddonFolder $f) }
  foreach ($r in $script:ACR_RootItems) { $items += $r }
  return $items
}

function Copy-ACRItems([string]$fromRoot, [string]$toRoot, $items) {
  foreach ($rel in $items) {
    $src = Join-Path $fromRoot $rel
    if (!(Test-Path -LiteralPath $src)) { continue }
    $dst = Join-Path $toRoot $rel
    $parent = Split-Path $dst -Parent
    if (!(Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Force $parent | Out-Null }
    Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force -ErrorAction Stop
  }
}

function Remove-ACRItems([string]$root, $items) {
  foreach ($rel in $items) {
    $p = Join-Path $root $rel
    if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction Stop }
  }
}

# Checks an extracted download. Returns the package folder or throws with the reason.
function Test-ACRPackage([string]$extracted, [string]$tag) {
  $roots = @(Get-ChildItem -LiteralPath $extracted -Force | Where-Object { $_.Name -ne 'update.zip' })
  if ($roots.Count -ne 1 -or !$roots[0].PSIsContainer -or $roots[0].Name -ne $script:ACR_PackageTop) {
    throw ('the download is not an AC: Actual package (expected one folder ' + $script:ACR_PackageTop + ', found: ' + (($roots | ForEach-Object { $_.Name }) -join ', ') + ')')
  }
  $top = $roots[0].FullName
  $full = [IO.Path]::GetFullPath($top)
  foreach ($f in Get-ChildItem -LiteralPath $top -Recurse -Force) {
    if (!([IO.Path]::GetFullPath($f.FullName)).StartsWith($full, [StringComparison]::OrdinalIgnoreCase)) { throw ('unexpected path in the download: ' + $f.FullName) }
  }
  $addon = Join-Path $top $script:ACR_AddonFolder
  foreach ($r in $script:ACR_Required) {
    if (!(Test-Path -LiteralPath (Join-Path $addon $r))) { throw ('the download is missing ' + $script:ACR_AddonFolder + '\' + $r) }
  }
  $gproj = Get-Content -LiteralPath (Join-Path $addon 'addon.gproj') -Raw
  if ($gproj -notmatch $script:ACR_ModId) { throw 'the download is a different mod (mod ID does not match)' }
  $version = (Get-Content -LiteralPath (Join-Path $addon 'VERSION.txt') -Raw).Trim()
  if ($tag -and $version -ne $tag) { throw ('the download says version ' + $version + ', the release is ' + $tag) }
  return $top
}

# A run cut off between backup and the end (window closed, PC off): put the old version back.
function Restore-ACRInterrupted([string]$packageRoot) {
  $state = Join-Path $packageRoot '.acr-update-state'
  $backup = Join-Path $packageRoot '.acr-update-backup'
  if (!(Test-Path -LiteralPath $state)) {
    if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue }
    return $false
  }
  $items = Get-ACRVersionedItems $packageRoot
  if (Test-Path -LiteralPath (Join-Path $backup '.complete')) {
    Write-Host 'The last update did not finish: putting the previous version back.' -ForegroundColor Yellow
    Remove-ACRItems $packageRoot $items
    Copy-ACRItems $backup $packageRoot $items
  }
  Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $state -Force -ErrorAction SilentlyContinue
  return $true
}

# Installs an extracted, checked package over the installed one, or leaves the
# installed one as it was. Throws (after rolling back) when it can't.
function Install-ACRPackage([string]$top, [string]$packageRoot, [string]$tag) {
  $items = Get-ACRVersionedItems $packageRoot
  $state = Join-Path $packageRoot '.acr-update-state'
  $backup = Join-Path $packageRoot '.acr-update-backup'
  if (Test-Path -LiteralPath $backup) { Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction Stop }
  Set-Content -LiteralPath $state -Value ('installing ' + $tag) -Encoding ASCII
  New-Item -ItemType Directory -Force $backup | Out-Null
  Copy-ACRItems $packageRoot $backup $items
  Set-Content -LiteralPath (Join-Path $backup '.complete') -Value 'ok' -Encoding ASCII
  try {
    $done = 0
    # Item by item, so the launcher scripts are always there (old or new) and a run cut
    # off part way can be rolled back by the next launch: folders are replaced whole,
    # files overwritten, and items the new version no longer has removed last.
    foreach ($rel in $items) {
      $new = Join-Path $top $rel
      $old = Join-Path $packageRoot $rel
      if (!(Test-Path -LiteralPath $new)) { continue }
      if ((Get-Item -LiteralPath $new).PSIsContainer -and (Test-Path -LiteralPath $old)) { Remove-Item -LiteralPath $old -Recurse -Force -ErrorAction Stop }
      if ($env:ACR_UPDATE_TEST_FAIL -eq 'interrupt') { return $false } # tests: as if the window closed here
      Copy-ACRItems $top $packageRoot @($rel)
      $done++
      if ($env:ACR_UPDATE_TEST_FAIL -eq 'install' -and $done -eq 3) { throw 'test: copy failed part way' }
    }
    foreach ($rel in $items) { if (!(Test-Path -LiteralPath (Join-Path $top $rel))) { Remove-ACRItems $packageRoot @($rel) } }
    $addon = Join-Path $packageRoot $script:ACR_AddonFolder
    foreach ($r in $script:ACR_Required) { if (!(Test-Path -LiteralPath (Join-Path $addon $r))) { throw ('after copying, ' + $r + ' is missing') } }
    $now = (Get-Content -LiteralPath (Join-Path $addon 'VERSION.txt') -Raw).Trim()
    if ($tag -and $now -ne $tag) { throw ('after copying, the version is ' + $now) }
  } catch {
    $why = $_.Exception.Message
    try {
      Remove-ACRItems $packageRoot $items
      Copy-ACRItems $backup $packageRoot $items
      Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
      Remove-Item -LiteralPath $state -Force -ErrorAction SilentlyContinue
    } catch {
      throw ('update failed (' + $why + ') and the old version could not be put back yet; it will be restored next time you start a launcher')
    }
    throw ('update failed (' + $why + '); the previous version was kept')
  }
  Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $state -Force -ErrorAction SilentlyContinue
  return $true
}

function Get-ACRRelease([string]$repo) {
  return Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers @{ 'User-Agent' = 'ACR-Updater' } -TimeoutSec 15
}

function Save-ACRAsset([string]$url, [string]$path) {
  Invoke-WebRequest -Uri $url -OutFile $path -UseBasicParsing -TimeoutSec 120
}

function Update-FromGitHub {
  if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git')) { return $false }
  $packageRoot = Split-Path $PSScriptRoot -Parent
  try { Restore-ACRInterrupted $packageRoot | Out-Null } catch { Write-Host ('Could not clean up the last update: ' + $_.Exception.Message) -ForegroundColor Yellow }
  $sourceFile = Join-Path $PSScriptRoot 'update-source.txt'
  if (!(Test-Path -LiteralPath $sourceFile)) { return $false }
  $repo = (Get-Content -LiteralPath $sourceFile -Raw).Trim()
  if (!$repo) { return $false }
  $versionFile = Join-Path $PSScriptRoot 'VERSION.txt'
  $local = ''
  if (Test-Path -LiteralPath $versionFile) { $local = (Get-Content -LiteralPath $versionFile -Raw).Trim() }
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  try { $release = Get-ACRRelease $repo }
  catch { Write-Host 'Could not check for updates (offline?). Continuing with the installed version.' -ForegroundColor Yellow; return $false }
  if (!$release -or $release.tag_name -eq $local) { Write-Host ('Up to date (' + $local + ')'); return $false }
  $asset = $release.assets | Where-Object { $_.name -eq $script:ACR_AssetName } | Select-Object -First 1
  if (!$asset) { Write-Host ('Release ' + $release.tag_name + ' has no ' + $script:ACR_AssetName + '. Continuing with the installed version.') -ForegroundColor Yellow; return $false }
  Write-Host ('Updating ' + $local + ' -> ' + $release.tag_name + ' ...') -ForegroundColor Cyan
  $temp = Join-Path ([IO.Path]::GetTempPath()) ('acr-update-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force $temp | Out-Null
  try {
    $zip = Join-Path $temp 'update.zip'
    Save-ACRAsset $asset.browser_download_url $zip
    if ($asset.size -and (Get-Item -LiteralPath $zip).Length -ne [long]$asset.size) { throw 'the download is incomplete' }
    $extracted = Join-Path $temp 'x'
    Expand-Archive -LiteralPath $zip -DestinationPath $extracted -Force | Out-Null
    $top = Test-ACRPackage $extracted $release.tag_name
    Install-ACRPackage $top $packageRoot $release.tag_name | Out-Null
  } catch {
    Write-Host ('Update skipped: ' + $_.Exception.Message + '. Continuing with ' + $local + '.') -ForegroundColor Yellow
    return $false
  } finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
  }
  Write-Host ('Updated to ' + $release.tag_name + '.') -ForegroundColor Green
  return $true
}
