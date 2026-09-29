# Publishes dist\ArmaCommander-Arland.zip as a GitHub release, so the launchers of
# everyone who has the package pick it up on their next start.
# Windows PowerShell treats native stderr output as an error under 'Stop'; gh writes
# normal status text to stderr, so keep 'Continue' and check exit codes instead.
$ErrorActionPreference = 'Continue'
$root = Split-Path $PSScriptRoot -Parent
$repo = (Get-Content (Join-Path $root 'tools\package\update-source.txt') -Raw).Trim()
$zip = Join-Path $root 'dist\ArmaCommander-Arland.zip'
$versionFile = Join-Path $root 'dist\VERSION.txt'
try { Start-Transcript -Path (Join-Path $root 'dist\publish-log.txt') -Force | Out-Null } catch {}

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

& $gh auth status 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { Fail "Not logged in. Run: `"$gh`" auth login" }

# Create the repository the first time (public, with a README so tags have a commit).
& $gh repo view $repo 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
  Write-Host "Creating public repository $repo ..."
  & $gh repo create $repo --public --add-readme --description "Arma Commander for Arma Reforger - downloads are under Releases"
  if ($LASTEXITCODE -ne 0) { Fail "Could not create repository $repo." }
  Start-Sleep -Seconds 3
}

# A release needs at least one commit; give an empty repository a README.
& $gh api "repos/$repo/commits?per_page=1" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
  Write-Host "Repository is empty; adding a README ..."
  $readme = "# Arma Commander: Reforger`n`nA fan remake of the Arma 3 ""Arma Commander"" mode for Arma Reforger.`n`n**Download:** [ArmaCommander-Arland.zip](https://github.com/$repo/releases/latest/download/ArmaCommander-Arland.zip). Unzip it and open ""READ ME FIRST.txt"".`n`nThe Host and Join launchers update themselves from this page.`n`nBased on Arma Commander for Arma 3 by Martin Hajek (APL-SA).`n"
  $b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($readme))
  & $gh api -X PUT "repos/$repo/contents/README.md" -f message="Add README" -f content="$b64"
  if ($LASTEXITCODE -ne 0) { Fail "Could not add a README to $repo." }
  Start-Sleep -Seconds 2
}

# Remove drafts left behind by a failed upload.
$drafts = & $gh api "repos/$repo/releases" --jq '.[] | select(.draft) | .id' 2>$null
foreach ($id in @($drafts)) { if ("$id".Trim()) { & $gh api -X DELETE "repos/$repo/releases/$("$id".Trim())" 2>&1 | Out-Null } }

& $gh release view $version --repo $repo 2>&1 | Out-Null
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
