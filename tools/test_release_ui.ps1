# Test publication wiring with mocked clipboard/processes only. Never starts Workbench or uploads.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Mod-Release-Console.ps1') -NoMenu
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('release-ui-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
try {
 $script:root = Join-Path $fixtureRoot 'Workbench/addons/ArmaCommanderReforger'
 $script:consoleState = Join-Path $fixtureRoot 'state'
 New-Item -ItemType Directory -Path $script:root,$script:consoleState -Force | Out-Null
 $script:WorkbenchPath = Join-Path $fixtureRoot 'Tools with spaces/Workbench.exe'
 New-Item -ItemType Directory -Path (Split-Path $script:WorkbenchPath -Parent) -Force | Out-Null
 Set-Content -LiteralPath $script:WorkbenchPath -Value ''
 $script:projectPath = Join-Path $fixtureRoot 'candidate with spaces/BetterBattleBuddies/addon.gproj'
 $script:notePath = Join-Path $fixtureRoot 'candidate with spaces/WORKSHOP-CHANGELOG.txt'
 New-Item -ItemType Directory -Path (Split-Path $script:projectPath -Parent) -Force | Out-Null
 $unicodeNote = "0.1.2`n`n- Reviewed fixture notes " + [char]0x2014 + ' verified.'
 [IO.File]::WriteAllText($script:notePath, $unicodeNote, [Text.UTF8Encoding]::new($false))
 $script:launches = @()
 $script:answer = ''
 $script:copied = ''
 $script:publishInfoCalls = 0
 $script:changeAfterReview = $false
 $script:processingResult = 'Addon processing successful'
 $script:recorded = @()
 function Get-Process { [CmdletBinding()] param([string]$Name) }
 function Find-BaseGameAddons { return (Join-Path $fixtureRoot 'Game/addons') }
 function Set-Clipboard { param([string]$Value) $script:copied = $Value }
 function Read-Host { param([string]$Prompt) return $script:answer }
 function Invoke-Guard {
  param([string[]]$Arguments)
  if ($Arguments[0] -eq 'check') { return $script:projectPath }
  if ($Arguments[0] -eq 'record-publication') { $script:recorded += ,$Arguments; return '{}' }
  $version = '0.1.2'
  if ($Arguments[0] -eq 'publish-info') {
   $script:publishInfoCalls++
   if ($script:changeAfterReview -and $script:publishInfoCalls -gt 1) { $version = '0.1.3' }
  }
  return (@{ project=$script:projectPath; changelogFile=$script:notePath; version=$version; installedVersion='0.1.1' } | ConvertTo-Json)
 }
 function Start-Process {
  [CmdletBinding()] param([string]$FilePath,[string]$ArgumentList,[string]$WorkingDirectory,[string]$WindowStyle,[switch]$Wait,[switch]$PassThru)
  $script:launches += @{ args=$ArgumentList; cwd=$WorkingDirectory; wait=$Wait }
  if ($Wait) {
   $logDir = Join-Path $fixtureRoot 'Workbench/logs/fixture'
   New-Item -ItemType Directory -Path $logDir -Force | Out-Null
   Set-Content -LiteralPath (Join-Path $logDir 'console.log') -Value ($script:projectPath.Replace('\','/') + "`nProject uploaded successfully`n" + $script:processingResult)
   return [pscustomobject]@{ ExitCode=0 }
  }
 }
 Open-CheckedProject -Mod BBB
 if ($script:launches.Count -ne 1 -or $script:launches[0].args -match '-publishAddon') { throw 'Manual Open accidentally entered publication mode.' }
 if ($script:copied -notmatch 'Reviewed fixture notes') { throw 'Manual Open did not copy candidate notes.' }
 if (!$script:copied.Contains([string][char]0x2014)) { throw 'Unicode changelog text was corrupted on the clipboard path.' }
 if ($script:launches[0].cwd -ne (Split-Path $script:WorkbenchPath -Parent)) { throw 'Wrong Workbench launch directory.' }
 Open-CheckedProject -Mod BBB -PublishUpdate
 if ($script:launches.Count -ne 1) { throw 'Cancelled review started a publisher.' }
 if ($script:recorded.Count -ne 0) { throw 'Open/cancellation recorded a publication.' }
 $script:answer = 'PUBLISH BBB 0.1.2'
 Open-CheckedProject -Mod BBB -PublishUpdate
 $publishArgs = $script:launches[1].args
 if ($publishArgs -notmatch '-publishAddonVersion 0.1.2' -or !$publishArgs.Contains('-publishAddonChangeNoteFile "' + $script:notePath + '"')) { throw 'CLI version/notes file arguments are wrong.' }
 if (!(Test-Path -LiteralPath (Join-Path $script:consoleState 'BBB-publication.json'))) { throw 'Confirmed mock upload did not record its log.' }
 if ($script:recorded.Count -ne 1 -or $script:recorded[0][0] -ne 'record-publication' -or $script:recorded[0][3] -ne '0.1.2') { throw 'Confirmed upload did not record its exact version in source history.' }
 $script:publishInfoCalls = 0
 $script:changeAfterReview = $true
 $blocked = $false
 try { Open-CheckedProject -Mod BBB -PublishUpdate } catch { $blocked = $_.Exception.Message -match 'changed during review' }
 if (!$blocked -or $script:launches.Count -ne 2) { throw 'Changed release was not blocked before publication.' }
 $script:changeAfterReview = $false
 $script:processingResult = 'Addon processing failed'
 $failed = $false
 try { Open-CheckedProject -Mod BBB -PublishUpdate } catch { $failed = $_.Exception.Message -match 'processing result could not be confirmed' }
 if (!$failed) { throw 'Backend processing failure was incorrectly reported as publication success.' }
 if ($script:recorded.Count -ne 1) { throw 'Failed upload recorded a publication.' }
 Write-Output 'Release UI PASS: clipboard/manual mode, working directory, cancellation, CLI note file/version, upload/processing log, changed-release refusal, processing failure. No real processes/uploads.'
} finally {
 try { Stop-Transcript | Out-Null } catch { }
 $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
 if ((Split-Path $resolvedFixture -Parent) -ne ([IO.Path]::GetTempPath()).TrimEnd('\')) { throw 'Unsafe fixture cleanup path.' }
 Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
