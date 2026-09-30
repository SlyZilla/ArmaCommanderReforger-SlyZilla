# Self-update from GitHub releases (package copies only; the development folder,
# which has a .git folder, is never touched). The repository comes from
# update-source.txt ("owner/name"); the installed version from VERSION.txt.
# Returns $true when a new version was installed (the caller should restart).
function Update-FromGitHub {
  if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git')) { return $false }
  $sourceFile = Join-Path $PSScriptRoot 'update-source.txt'
  if (!(Test-Path -LiteralPath $sourceFile)) { return $false }
  $repo = (Get-Content -LiteralPath $sourceFile -Raw).Trim()
  if (!$repo) { return $false }
  $versionFile = Join-Path $PSScriptRoot 'VERSION.txt'
  $local = ''
  if (Test-Path -LiteralPath $versionFile) { $local = (Get-Content -LiteralPath $versionFile -Raw).Trim() }
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  try { $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers @{ 'User-Agent' = 'ACR-Updater' } -TimeoutSec 15 }
  catch { Write-Host 'Could not check for updates (offline?). Continuing with the installed version.' -ForegroundColor Yellow; return $false }
  if (!$release -or $release.tag_name -eq $local) { Write-Host ('Up to date (' + $local + ')'); return $false }
  $asset = $release.assets | Where-Object { $_.name -like '*.zip' } | Select-Object -First 1
  if (!$asset) { return $false }
  Write-Host ('Updating ' + $local + ' -> ' + $release.tag_name + ' ...') -ForegroundColor Cyan
  $temp = Join-Path $env:TEMP ('acr-update-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force $temp | Out-Null
  $zip = Join-Path $temp 'update.zip'
  Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip -UseBasicParsing
  Expand-Archive -LiteralPath $zip -DestinationPath $temp -Force
  $source = Get-ChildItem -LiteralPath $temp -Directory | Select-Object -First 1
  $packageRoot = Split-Path $PSScriptRoot -Parent
  # Replace the mod's content folders so files removed upstream disappear too.
  foreach ($folder in 'Scripts', 'Worlds', 'Configs', 'Missions') {
    $old = Join-Path $PSScriptRoot $folder
    if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }
  }
  Copy-Item -Path (Join-Path $source.FullName '*') -Destination $packageRoot -Recurse -Force
  Remove-Item -LiteralPath $temp -Recurse -Force
  Write-Host ('Updated to ' + $release.tag_name + '.') -ForegroundColor Green
  return $true
}
