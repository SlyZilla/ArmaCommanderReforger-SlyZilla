# Refresh the Workshop release copy of AC: Actual while stripping tests and dev files.
# Run from Windows: pwsh -File tools/make_release.ps1
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$addonsRoot = Split-Path $sourceRoot -Parent
$releaseParent = [IO.Path]::GetFullPath((Join-Path $addonsRoot '_ACR-release'))
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $releaseParent 'ArmaCommanderReforger'))
$expectedRoot = [IO.Path]::GetFullPath((Join-Path $addonsRoot '_ACR-release\ArmaCommanderReforger'))
if ($releaseRoot -ne $expectedRoot -or $releaseRoot -eq $sourceRoot) {
    throw 'Release target is outside the expected _ACR-release/ArmaCommanderReforger directory.'
}
foreach ($path in @($releaseParent, $releaseRoot)) {
    if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing a release path containing a junction or symbolic link: $path"
    }
}

$releaseFiles = @('resourceDatabase.rdb', 'LICENSE.md', 'README.md', 'MISSION-MAKING.md')
foreach ($name in @('Scripts', 'Configs', 'Missions', 'Prefabs', 'Worlds', 'addon.gproj') + $releaseFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $sourceRoot $name))) { throw "Missing release source: $name" }
}

New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null

$keepDirs = @('Configs', 'Missions', 'Prefabs', 'Worlds', 'Scripts')
$skipFiles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$skipFiles.Add('Worlds/BBB_TestArland.ent') | Out-Null
$skipFiles.Add('Worlds/BBB_TestArland.ent.meta') | Out-Null
$skipFiles.Add('Missions/BBB_TestArland.conf') | Out-Null
$skipFiles.Add('Missions/BBB_TestArland.conf.meta') | Out-Null

function Is-TestScript($rel) {
    if ($rel.StartsWith('Scripts/Game/ACR_Tests/')) { return $true }
    if ($rel.StartsWith('Scripts/Game/BBB/') -and $rel.EndsWith('Test.c')) { return $true }
    return $false
}

foreach ($d in $keepDirs) {
    $targetDir = Join-Path $releaseRoot $d
    if (Test-Path -LiteralPath $targetDir) {
        Remove-Item -LiteralPath $targetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    $srcDir = Join-Path $sourceRoot $d
    Get-ChildItem -LiteralPath $srcDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
        if (!$skipFiles.Contains($rel) -and !(Is-TestScript $rel)) {
            $destFile = Join-Path $releaseRoot $rel
            $destParent = Split-Path $destFile -Parent
            if (!(Test-Path -LiteralPath $destParent)) {
                New-Item -ItemType Directory -Path $destParent -Force | Out-Null
            }
            Copy-Item -LiteralPath $_.FullName -Destination $destFile -Force
        }
    }
}

foreach ($name in $releaseFiles) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination (Join-Path $releaseRoot $name) -Force
}

# Update addon.gproj TITLE for Workshop release
$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'addon.gproj') -Raw
$project = $project -replace '(?m)^\s*TITLE\s+".*"', ' TITLE "Arma Commander: Actual"'
[IO.File]::WriteAllText((Join-Path $releaseRoot 'addon.gproj'), $project, [Text.UTF8Encoding]::new($false))

# Copy workshop description and thumbnails to _ACR-release
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'workshop') -File) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $releaseParent $file.Name) -Force
}

Write-Output "Workshop release copy refreshed: $releaseRoot"
Write-Output "Open Workbench, open _ACR-release\ArmaCommanderReforger, and publish under your Steam login."
