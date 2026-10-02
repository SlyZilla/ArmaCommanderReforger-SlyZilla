# Publishes dist\ACActual-Arland.zip as a GitHub release, so the launchers of
# everyone who has the package pick it up on their next start.
# Windows PowerShell treats native stderr output as an error under 'Stop'; gh writes
# normal status text to stderr, so keep 'Continue' and check exit codes instead.
$ErrorActionPreference = 'Continue'
$root = Split-Path $PSScriptRoot -Parent
$repo = (Get-Content (Join-Path $root 'tools\package\update-source.txt') -Raw).Trim()
$zip = Join-Path $root 'dist\ACActual-Arland.zip'
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
  & $gh repo create $repo --public --add-readme --description "Arma Commander Actual (AC: Actual) for Arma Reforger - downloads are under Releases"
  if ($LASTEXITCODE -ne 0) { Fail "Could not create repository $repo." }
  Start-Sleep -Seconds 3
}

# Keep the repository's front page and license in step with the project
# (this also gives an empty repository the commit a release needs).
function Sync-RepoFile($localPath, $repoPath) {
  $bytes = [IO.File]::ReadAllBytes($localPath)
  # Git blob hash of the local file, to skip unchanged uploads.
  $header = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
  $sha1 = [Security.Cryptography.SHA1]::Create()
  $local = -join ($sha1.ComputeHash([byte[]]($header + $bytes)) | ForEach-Object { $_.ToString('x2') })
  $remote = (& $gh api "repos/$repo/contents/$repoPath" --jq .sha 2>$null)
  if ($LASTEXITCODE -ne 0) { $remote = '' }
  $remote = "$remote".Trim()
  if ($remote -eq $local) { return }
  Write-Host "Updating $repoPath on GitHub ..."
  $b64 = [Convert]::ToBase64String($bytes)
  if ($remote) { & $gh api -X PUT "repos/$repo/contents/$repoPath" -f message="Update $repoPath" -f content="$b64" -f sha="$remote" | Out-Null }
  else { & $gh api -X PUT "repos/$repo/contents/$repoPath" -f message="Add $repoPath" -f content="$b64" | Out-Null }
  if ($LASTEXITCODE -ne 0) { Fail "Could not update $repoPath on $repo." }
}
Sync-RepoFile (Join-Path $root 'tools\package\REPO-README.md') 'README.md'
Sync-RepoFile (Join-Path $root 'LICENSE.md') 'LICENSE.md'
Sync-RepoFile (Join-Path $root 'MISSION-MAKING.md') 'MISSION-MAKING.md'
Sync-RepoFile (Join-Path $root 'tools\package\github\ISSUE_TEMPLATE\bug_report.md') '.github/ISSUE_TEMPLATE/bug_report.md'
Sync-RepoFile (Join-Path $root 'tools\package\github\ISSUE_TEMPLATE\config.yml') '.github/ISSUE_TEMPLATE/config.yml'
& $gh repo edit $repo --description "Arma Commander Actual (AC: Actual): unofficial, vibecoded fan port of Arma Commander to Arma Reforger. Downloads under Releases." 2>&1 | Out-Null
Start-Sleep -Seconds 1

# Remove drafts left behind by a failed upload.
$drafts = & $gh api "repos/$repo/releases" --jq '.[] | select(.draft) | .id' 2>$null
foreach ($id in @($drafts)) { if ("$id".Trim()) { & $gh api -X DELETE "repos/$repo/releases/$("$id".Trim())" 2>&1 | Out-Null } }

& $gh release view $version --repo $repo 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
  Write-Host "$version is already published. Nothing to do." -ForegroundColor Yellow
  exit 0
}

Write-Host "Publishing $version to $repo ..."
$notes = "Download ACActual-Arland.zip below, unzip it, and read 'READ ME FIRST.txt'.`nAlready have it? Just start Host-Arland or Join-Arland; they update themselves.`nFound a bug? [Report it here](https://github.com/$repo/issues/new/choose); the README lists what most needs multiplayer testing."
& $gh release create $version $zip --repo $repo --title $version --notes $notes --latest
if ($LASTEXITCODE -ne 0) { Fail "Release upload failed (see above)." }

Write-Host ""
Write-Host "Published $version." -ForegroundColor Green
Write-Host "Permanent download link for your buddies:"
Write-Host "  https://github.com/$repo/releases/latest/download/ACActual-Arland.zip"
