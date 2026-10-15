# Preserve obsolete copies outside addon discovery. Never removes development repositories.
[CmdletBinding(SupportsShouldProcess=$true)]
param()
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$addonsRoot = [IO.Path]::GetFullPath((Split-Path $sourceRoot -Parent))
$workbenchRoot = [IO.Path]::GetFullPath((Split-Path $addonsRoot -Parent))
if (Get-Process '*Workbench*' -ErrorAction SilentlyContinue) { throw 'Close Workbench before archiving its obsolete projects.' }
$archiveRoot = Join-Path $workbenchRoot ('_ModReleaseArchive/' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$targets = @(
 @{ Parent=$addonsRoot; Name='_ACR-release'; Archive='old-addons-ACA' },
 @{ Parent=$addonsRoot; Name='_ACR-local-test'; Archive='old-addons-local-test' },
 @{ Parent=$addonsRoot; Name='_AIO-release'; Archive='old-addons-AIO' },
 @{ Parent=$workbenchRoot; Name='_ACR-release'; Archive='old-workbench-ACA' }
)
foreach ($entry in $targets) {
 $target = [IO.Path]::GetFullPath((Join-Path $entry.Parent $entry.Name))
 if ((Split-Path $target -Parent) -ne $entry.Parent -or (Split-Path $target -Leaf) -ne $entry.Name -or $target -eq $sourceRoot) { throw "Unsafe archive source: $target" }
 if (!(Test-Path -LiteralPath $target)) { continue }
 # Check every existing ancestor/descendant before a recursive directory move.
 for ($ancestor = $target; $ancestor; $ancestor = Split-Path $ancestor -Parent) {
  if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked archive source refused: $ancestor" }
 }
 foreach ($child in Get-ChildItem -LiteralPath $target -Recurse -Force) {
  if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked archive content refused: $($child.FullName)" }
 }
 $destination = [IO.Path]::GetFullPath((Join-Path $archiveRoot $entry.Archive))
 if (!$destination.StartsWith($workbenchRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or (Split-Path $destination -Parent) -ne $archiveRoot) { throw "Unsafe archive destination: $destination" }
 if (Test-Path -LiteralPath $destination) { throw "Archive destination already exists: $destination" }
 for ($ancestor = $archiveRoot; $ancestor; $ancestor = Split-Path $ancestor -Parent) {
  if ((Test-Path -LiteralPath $ancestor) -and ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked archive destination refused: $ancestor" }
 }
 if ($PSCmdlet.ShouldProcess($target, "Preserve in $destination")) {
  New-Item -ItemType Directory -Path $archiveRoot -Force | Out-Null
  Move-Item -LiteralPath $target -Destination $destination
  Write-Output "Archived: $target -> $destination"
 }
}
