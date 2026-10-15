# Human entry point. Backend operations never publish anything.
[CmdletBinding()]
param([string]$WorkbenchPath, [string]$InstalledAddons = 'E:\ArmaReforgerMods\addons', [switch]$NoMenu)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$backend = Join-Path $PSScriptRoot 'release_console.py'
$consoleState = Join-Path $root '.release-console'
New-Item -ItemType Directory -Path $consoleState -Force | Out-Null
try { Start-Transcript -Path (Join-Path $consoleState 'console.log') -Append -Force | Out-Null } catch { Write-Warning 'Console log could not be started.' }
function Find-BaseGameAddons {
 $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
 $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
 if ($steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf'))) {
  foreach ($match in [regex]::Matches((Get-Content -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf') -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
 }
 foreach ($library in $libraries) {
  if (!$library) { continue }
  $candidate = Join-Path $library 'steamapps/common/Arma Reforger/addons'
  $baseProject = Join-Path $candidate 'data/ArmaReforger.gproj'
  if ((Test-Path -LiteralPath $baseProject) -and (Get-Content -LiteralPath $baseProject -Raw) -match 'GUID\s+"58D0FB3206B6F859"') { return $candidate }
 }
 throw 'Arma Reforger base-game addon files were not found in the Steam libraries.'
}
function Invoke-Guard([string[]]$Arguments) {
 $result = & python $backend @Arguments
 if ($LASTEXITCODE -ne 0) { throw 'Release blocked. The reason is printed above; ask your coding agent to resolve it.' }
 return $result
}
function Open-CheckedProject {
 param([string]$Mod, [switch]$PublishUpdate)
 if (Get-Process '*Workbench*' -ErrorAction SilentlyContinue) { throw 'Close Workbench first. This ensures the selected release opens as the active project.' }
 $project = Invoke-Guard @('check', $Mod, '--installed-root', $InstalledAddons)
 $notes = (Invoke-Guard @('notes', $Mod)) | ConvertFrom-Json
 if ($PublishUpdate) { $notes = (Invoke-Guard @('publish-info', $Mod, '--installed-root', $InstalledAddons)) | ConvertFrom-Json }
 if (!$script:WorkbenchPath) {
  $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
  if ($steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf'))) {
   foreach ($match in [regex]::Matches((Get-Content -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf') -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
  }
  foreach ($library in $libraries) {
   if (!$library) { continue }
   foreach ($relative in @('Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe', 'Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteam.exe', 'Arma Reforger Tools/ArmaReforgerWorkbenchSteam.exe')) {
    $candidate = Join-Path $library ('steamapps/common/' + $relative)
    if (Test-Path -LiteralPath $candidate) { $script:WorkbenchPath = $candidate; break }
   }
   if ($script:WorkbenchPath) { break }
  }
 }
 if (!$script:WorkbenchPath -or !(Test-Path -LiteralPath $script:WorkbenchPath)) { throw 'Workbench executable not found. Ask the coding agent to configure -WorkbenchPath.' }
 $addonDirs = Split-Path (Split-Path $project -Parent) -Parent
 if ($Mod -eq 'ACA') {
  $bbbProject = Invoke-Guard @('check', 'BBB')
  $addonDirs += ',' + (Split-Path (Split-Path $bbbProject -Parent) -Parent)
 }
 # Tools supply core; the game supplies data. Neither may depend on the shortcut working folder.
 $addonDirs += ',' + (Find-BaseGameAddons)
 $launchArgs = '-gproj "' + $project + '" -addonsDir "' + $addonDirs + '"'
 if (!$PublishUpdate) {
  Set-Clipboard -Value (Get-Content -LiteralPath $notes.changelogFile -Raw -Encoding UTF8)
  Write-Host "Opening checked $Mod $($notes.version): $project" -ForegroundColor Green
  Write-Host 'Its changelog is copied. In Publish Project, click Change notes and press Ctrl+V.'
  Write-Host "Use version $($notes.version); review page settings before uploading."
  Write-Host 'After upload, download the Workshop update and use option 6 to verify it.'
  Start-Process -FilePath $script:WorkbenchPath -WorkingDirectory (Split-Path $script:WorkbenchPath -Parent) -ArgumentList $launchArgs
  return
 }
 Write-Host "WORKSHOP UPDATE: $Mod $($notes.installedVersion) -> $($notes.version)" -ForegroundColor Cyan
 Get-Content -LiteralPath $notes.changelogFile -Encoding UTF8 | Out-Host
 Write-Host 'This uploads an update using the existing Workshop item and your Workbench login.'
 $confirmation = 'PUBLISH ' + $Mod + ' ' + $notes.version
 if ((Read-Host "Type $confirmation to upload, or press Enter to cancel") -cne $confirmation) {
  Write-Host 'Cancelled; nothing uploaded.'
  return
 }
 # Recheck after review so edits/version changes cannot slip between review and upload.
 $fresh = (Invoke-Guard @('publish-info', $Mod, '--installed-root', $InstalledAddons)) | ConvertFrom-Json
 if ($fresh.project -ne $project -or $fresh.version -ne $notes.version -or $fresh.changelogFile -ne $notes.changelogFile) { throw 'The selected release changed during review. Start again.' }
 $launchArgs += ' -wbmodule=ResourceManager -publishAddon -publishAddonVersion ' + $notes.version + ' -publishAddonChangeNoteFile "' + $notes.changelogFile + '"'
 $started = Get-Date
 Write-Host 'Publishing checked update with its changelog file...'
 $process = Start-Process -FilePath $script:WorkbenchPath -WorkingDirectory (Split-Path $script:WorkbenchPath -Parent) -ArgumentList $launchArgs -WindowStyle Hidden -Wait -PassThru
 if ($process.ExitCode -ne 0) { throw "Workbench update exited with code $($process.ExitCode). Check its log before retrying." }
 $logsRoot = Join-Path (Split-Path (Split-Path $root -Parent) -Parent) 'logs'
 $log = Get-ChildItem -LiteralPath $logsRoot -Filter console.log -Recurse | Where-Object { $_.LastWriteTime -ge $started } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
 if (!$log) { throw 'Upload result could not be confirmed. Check Workbench before retrying; an upload may have occurred.' }
 $body = Get-Content -LiteralPath $log.FullName -Raw
 if (!$body.Replace('\','/').Contains($project.Replace('\','/')) -or $body -notmatch 'Project uploaded successfully' -or $body -notmatch 'Addon processing successful') { throw 'Upload/processing result could not be confirmed. Check Workbench before retrying; an upload may have occurred.' }
 @{ mod=$Mod; version=$notes.version; publication='CLI upload confirmed in Workbench log'; log=$log.FullName; installedVerification='pending' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $consoleState ($Mod + '-publication.json')) -Encoding UTF8
 Write-Host "Uploaded $Mod $($notes.version). Download its Workshop update, then use option 6." -ForegroundColor Green
}
if ($NoMenu) { return }
while ($true) {
 Write-Host ''
 Write-Host 'MOD RELEASE CONSOLE' -ForegroundColor Cyan
 Write-Host 'Development stays in addons. This console selects checked release copies for you.'
 & python $backend status
 Write-Host ''
 Write-Host '1  Prepare ACA + BBB update (fresh copies + isolated startup checks)'
 Write-Host '2  Prepare AiO update (fresh copy + isolated startup check)'
 Write-Host '3  Open checked BBB release for Workshop upload -- first for an ACA/BBB update'
 Write-Host '4  Open checked ACA release -- requires downloaded BBB to match'
 Write-Host '5  Open checked AiO release for Workshop upload'
 Write-Host '6  Verify a downloaded Workshop update'
 Write-Host '7  Publish a checked update with automatic version/changelog (review first)'
 Write-Host '0  Exit'
 $choice = Read-Host 'Choose'
 try {
  switch ($choice) {
   '0' { return }
   '1' { Invoke-Guard @('prepare', 'ACA') | Out-Host; Write-Host 'Prepared. Next: option 3, upload BBB, then download its Workshop update.' -ForegroundColor Green }
   '2' { Invoke-Guard @('prepare', 'AIO') | Out-Host; Write-Host 'Prepared. Next: option 5.' -ForegroundColor Green }
   '3' { Open-CheckedProject 'BBB' }
   '4' { Open-CheckedProject 'ACA' }
   '5' { Open-CheckedProject 'AIO' }
   '6' {
    $mod = (Read-Host 'Which downloaded mod: ACA, BBB, or AIO').ToUpperInvariant()
    if ($mod -notin @('ACA','BBB','AIO')) { throw 'Choose ACA, BBB, or AIO.' }
    Invoke-Guard @('installed', $mod, '--installed-root', $InstalledAddons) | Out-Host
   }
   '7' {
    $mod = (Read-Host 'Which mod to update: ACA, BBB, or AIO').ToUpperInvariant()
    if ($mod -notin @('ACA','BBB','AIO')) { throw 'Choose ACA, BBB, or AIO.' }
    Open-CheckedProject -Mod $mod -PublishUpdate
   }
   default { Write-Host 'Choose one of the listed numbers.' }
  }
 } catch {
  Write-Host $_.Exception.Message -ForegroundColor Red
  Write-Host ('Error saved in ' + (Join-Path $consoleState 'console.log'))
  Read-Host 'Press Enter to return to the menu' | Out-Null
 }
}
