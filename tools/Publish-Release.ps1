# Publishes dist\ArmaCommander-Arland.zip as a GitHub release, so the launchers of
# everyone who has the package pick it up on their next start.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$repo = (Get-Content (Join-Path $root 'tools\package\update-source.txt') -Raw).Trim()
$zip = Join-Path $root 'dist\ArmaCommander-Arland.zip'
$versionFile = Join-Path $root 'dist\VERSION.txt'

function Fail($msg) { Write-Host ""; Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

# Find gh even if PATH hasn't picked it up yet.
$gh = (Get-Command gh -ErrorAction SilentlyContinue).Source
if (-not $gh) {
  foreach ($c in @("$env:ProgramFiles\GitHub CLI\gh.exe", "${env:ProgramFiles(x86)}\GitHub CLI\gh.exe", "$env:LOCALAPPDATA\Programs\GitHub CLI\gh.exe", "$env:LOCALAPPDATA\Microsoft\WinGet\Links\gh.exe")) {
    if ($c -and (Test-Path $c)) { $gh = $c; break }
  }
}
if (-not $gh) { Fail "GitHub CLI (gh.exe) not found. Install it with: winget install GitHub.cli" }
if (-not (Test-Path $zip)) { Fail "No package found at $zip. Ask Claude to rebuild it." }
if (-not (Test-Path $versionFile)) { Fail "No dist\VERSION.txt. Ask Claude to rebuild the package." }
$version = (Get-Content $versionFile -Raw).Trim()

& $gh auth status 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "Not logged in. Run: `"$gh`" auth login" }

# Create the repository the first time (public, with a README so tags have a commit).
& $gh repo view $repo 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
  Write-Host "Creating public repository $repo ..."
  & $gh repo create $repo --public --add-readme --description "Arma Commander for Arma Reforger - downloads are under Releases" | Out-Null
  if ($LASTEXITCODE -ne 0) { Fail "Could not create repository $repo." }
  Start-Sleep -Seconds 3
}

& $gh release view $version --repo $repo 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) {
  Write-Host "$version is already published. Nothing to do." -ForegroundColor Yellow
  exit 0
}

Write-Host "Publishing $version to $repo ..."
$notes = "Download ArmaCommander-Arland.zip below, unzip it, and read 'READ ME FIRST.txt'.`nAlready have it? Just start Host-Arland or Join-Arland; they update themselves."
& $gh release create $version $zip --repo $repo --title $version --notes $notes --latest
if ($LASTEXITCODE -ne 0) { Fail "Release upload failed (see above)." }

Write-Host ""
Write-Host "Published $version." -ForegroundColor Green
Write-Host "Permanent download link for your buddies:"
Write-Host "  https://github.com/$repo/releases/latest/download/ArmaCommander-Arland.zip"
