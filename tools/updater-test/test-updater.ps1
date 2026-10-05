# Fixture tests for ACR-Update.ps1 (R02). Never touches a real install.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$results = @()

function Hash-Tree([string]$root) {
  $lines = Get-ChildItem -LiteralPath $root -Recurse -File -Force | Sort-Object FullName | ForEach-Object {
    $_.FullName.Substring($root.Length) + ' ' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
  }
  return ($lines -join "`n")
}

function New-Run([string]$name) {
  $dir = Join-Path $here ('run/' + $name)
  if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
  New-Item -ItemType Directory -Force $dir | Out-Null
  Copy-Item (Join-Path $here 'installed/*') $dir -Recurse -Force
  return (Join-Path $dir 'ACActual-Arland')
}

function Run-Case([string]$name, [string]$zip, $assets = $null, [string]$tag = 'v-new', [string]$fail = '', [switch]$Git, [long]$size = 0) {
  $pkg = New-Run $name
  $addon = Join-Path $pkg 'ArmaCommanderReforger'
  if ($Git) { New-Item -ItemType Directory (Join-Path $addon '.git') | Out-Null }
  $before = Hash-Tree $pkg
  . (Join-Path $addon 'ACR-Update.ps1')
  $script:zipSource = ''
  if ($zip) { $script:zipSource = Join-Path $here $zip }
  if (!$assets) { $assets = @(@{ name = 'ACActual-Arland.zip'; browser_download_url = 'good'; size = $size }) }
  $script:rel = [pscustomobject]@{ tag_name = $tag; assets = $assets }
  function Get-ACRRelease([string]$repo) { return $script:rel }
  function Save-ACRAsset([string]$url, [string]$path) { Copy-Item $script:zipSource $path }
  $env:ACR_UPDATE_TEST_FAIL = $fail
  $out = $false
  try { $out = Update-FromGitHub } catch { $out = 'THREW ' + $_.Exception.Message }
  $env:ACR_UPDATE_TEST_FAIL = ''
  $after = Hash-Tree $pkg
  return [pscustomobject]@{ Name = $name; Returned = $out; Unchanged = ($before -eq $after); Pkg = $pkg; Addon = $addon }
}

function Check([string]$name, [bool]$ok, [string]$detail) {
  $v = 'FAIL'; if ($ok) { $v = 'PASS' }
  Write-Host ("UPDATER $v $name  $detail")
  $script:results += $ok
}

function Settings-Kept($addon) {
  return ((Get-Content (Join-Path $addon 'host-settings.txt') -Raw).Trim() -eq 'H') -and ((Get-Content (Join-Path $addon 'zerotier.txt') -Raw).Trim() -eq 'Use=yes') -and (Test-Path (Join-Path $addon '.local-profile/p.txt')) -and (Test-Path (Join-Path $addon 'last-host.txt')) -and (Test-Path (Join-Path $addon 'play-settings.txt'))
}
function Clean($pkg) { return !(Test-Path (Join-Path $pkg '.acr-update-state')) -and !(Test-Path (Join-Path $pkg '.acr-update-backup')) }

# 1. Valid package
$r = Run-Case 'valid' 'good.zip'
$ver = (Get-Content (Join-Path $r.Addon 'VERSION.txt') -Raw).Trim()
Check 'valid package installs' ($r.Returned -eq $true -and $ver -eq 'v-new' -and !(Test-Path (Join-Path $r.Addon 'Prefabs/ACR/Obsolete/Gone.et')) -and (Settings-Kept $r.Addon) -and (Clean $r.Pkg) -and (Test-Path (Join-Path $r.Addon 'Scripts/Game/ACR/ACR_Manager.c'))) "returned $($r.Returned) version $ver"

# 2-7, 11-12: bad downloads leave the install exactly as it was
foreach ($c in @(@('wrong root', 'wrongroot.zip'), @('two roots', 'tworoots.zip'), @('missing Prefabs', 'noprefabs.zip'), @('corrupt zip', 'corrupt.zip'), @('version mismatch', 'badversion.zip'), @('other mod id', 'othermod.zip'))) {
  $r = Run-Case ($c[0] -replace ' ', '-') $c[1]
  Check ('rejected: ' + $c[0]) ($r.Returned -eq $false -and $r.Unchanged -and (Clean $r.Pkg)) "returned $($r.Returned) unchanged $($r.Unchanged)"
}
# Size mismatch (truncated download)
$r = Run-Case 'truncated' 'good.zip' -size 12345
Check 'rejected: incomplete download' ($r.Returned -eq $false -and $r.Unchanged) "returned $($r.Returned) unchanged $($r.Unchanged)"
# Unrelated ZIP listed first: the named asset is used
$r = Run-Case 'unrelated-first' 'good.zip' -assets @(@{ name = 'source-code.zip'; browser_download_url = 'bad' }, @{ name = 'ACActual-Arland.zip'; browser_download_url = 'good' })
Check 'unrelated first zip is ignored' ($r.Returned -eq $true -and (Get-Content (Join-Path $r.Addon 'VERSION.txt') -Raw).Trim() -eq 'v-new') "returned $($r.Returned)"
# Release without our asset
$r = Run-Case 'no-asset' 'good.zip' -assets @(@{ name = 'other.zip'; browser_download_url = 'x' })
Check 'release without the package asset changes nothing' ($r.Returned -eq $false -and $r.Unchanged) "returned $($r.Returned)"
# 8. Copy failure during install: rolled back
$r = Run-Case 'copy-fails' 'good.zip' -fail 'install'
Check 'copy failure rolls back' ($r.Returned -eq $false -and $r.Unchanged -and (Clean $r.Pkg)) "returned $($r.Returned) unchanged $($r.Unchanged)"
# 9. Interrupted after removing the old content: next launch restores it
$r = Run-Case 'interrupted' 'good.zip' -fail 'interrupt'
$half = !(Test-Path (Join-Path $r.Addon 'Configs')) -and (Test-Path (Join-Path $r.Addon 'ACR-Multiplayer.ps1')) -and (Test-Path (Join-Path $r.Pkg '.acr-update-state'))
. (Join-Path $r.Addon "ACR-Update.ps1")
$restored = Restore-ACRInterrupted $r.Pkg
$after = Hash-Tree $r.Pkg
$pkgRef = New-Run 'reference'
$ref = Hash-Tree $pkgRef
Check 'interrupted update is restored on next launch' ($half -and $restored -and $after -eq $ref -and (Clean $r.Pkg)) "half-done $half restored $restored same as before $($after -eq $ref)"
# 10. Development copy (.git) never updates
$r = Run-Case 'devcopy' 'good.zip' -Git
Check 'development copy is never updated' ($r.Returned -eq $false -and $r.Unchanged) "returned $($r.Returned)"
# 11. Failed interrupted-update recovery stops before checking/installing a new release
$pkg = New-Run 'recovery-fails'
$addon = Join-Path $pkg 'ArmaCommanderReforger'
$state = Join-Path $pkg '.acr-update-state'
$backup = Join-Path $pkg '.acr-update-backup'
Set-Content -LiteralPath $state -Value 'installing v-old' -Encoding ASCII
New-Item -ItemType Directory -Force $backup | Out-Null
Set-Content -LiteralPath (Join-Path $backup '.complete') -Value 'ok' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $backup 'recovery-sentinel.txt') -Value 'keep' -Encoding ASCII
$backupBefore = Hash-Tree $backup
. (Join-Path $addon 'ACR-Update.ps1')
$script:releaseQueried = $false
$script:recoveryFailed = $false
$script:rel = [pscustomobject]@{ tag_name = 'v-new'; assets = @(@{ name = 'ACActual-Arland.zip'; browser_download_url = 'good'; size = 0 }) }
$script:zipSource = Join-Path $here 'good.zip'
function Get-ACRRelease([string]$repo) { $script:releaseQueried = $true; return $script:rel }
function Save-ACRAsset([string]$url, [string]$path) { Copy-Item $script:zipSource $path }
function Remove-ACRItems([string]$root, $items) { $script:recoveryFailed = $true; throw 'fixture: recovery cannot remove the partial install' }
$env:ACR_UPDATE_TEST_FAIL = ''
$stopped = Update-FromGitHub
$backupAfter = Hash-Tree $backup
$version = (Get-Content -LiteralPath (Join-Path $addon 'VERSION.txt') -Raw).Trim()
$kept = (Test-Path -LiteralPath $state) -and (Test-Path -LiteralPath $backup) -and ($backupAfter -eq $backupBefore)
Check 'failed recovery stops update and keeps backup' ($stopped -eq $false -and $script:recoveryFailed -and !$script:releaseQueried -and $kept -and $version -eq 'v-old') "returned $stopped recovery failed $($script:recoveryFailed) release queried $($script:releaseQueried) backup kept $kept version $version"

$failed = @($results | Where-Object { !$_ }).Count
Write-Host ("UPDATER SUMMARY passed=" + ($results.Count - $failed) + " failed=$failed")
if ($failed -gt 0) { exit 1 }
