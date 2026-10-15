# Human entry point. Backend operations never publish anything.
[CmdletBinding()]
param([string]$WorkbenchPath, [string]$InstalledAddons = 'E:\ArmaReforgerMods\addons')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$backend = Join-Path $PSScriptRoot 'release_console.py'
function Invoke-Guard([string[]]$Arguments) {
 $result = & python $backend @Arguments
 if ($LASTEXITCODE -ne 0) { throw 'Release blocked. The reason is printed above; ask your coding agent to resolve it.' }
 return $result
}
function Open-CheckedProject([string]$Mod) {
 if (Get-Process '*Workbench*' -ErrorAction SilentlyContinue) { throw 'Close Workbench first. This ensures the selected release opens as the active project.' }
 $project = Invoke-Guard @('check', $Mod, '--installed-root', $InstalledAddons)
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
 Write-Host "Opening checked $Mod release: $project" -ForegroundColor Green
 Write-Host 'Use Publish Project in this window. Review Workshop version/page settings before uploading.'
 Write-Host 'After upload, download the Workshop update in-game and use option 6 to verify it.'
 $addonDirs = Split-Path (Split-Path $project -Parent) -Parent
 if ($Mod -eq 'ACA') {
  $bbbProject = Invoke-Guard @('check', 'BBB')
  $addonDirs += ',' + (Split-Path (Split-Path $bbbProject -Parent) -Parent)
 }
 # An isolated search path also keeps Workbench from resolving a development BBB copy.
 Start-Process -FilePath $script:WorkbenchPath -ArgumentList ('-gproj "' + $project + '" -addonsDir "' + $addonDirs + '"')
}
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
   default { Write-Host 'Choose one of the listed numbers.' }
  }
 } catch { Write-Host $_.Exception.Message -ForegroundColor Red }
}
