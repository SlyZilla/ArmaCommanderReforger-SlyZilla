param(
    [string]$Workbench = 'F:\SteamLibrary\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe',
    [string]$GameAddons = 'E:\SteamLibrary\steamapps\common\Arma Reforger\addons'
)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $Workbench)) { throw 'Reforger Workbench was not found at the configured path.' }
if (!(Test-Path -LiteralPath (Join-Path $GameAddons 'data\ArmaReforger.gproj'))) { throw 'Reforger game data was not found.' }
$projectPath = Join-Path $PSScriptRoot 'addon.gproj'
$profilePath = Join-Path $PSScriptRoot '.local-workbench'
$arguments = '-gproj "' + $projectPath + '" -addonsDir "' + $GameAddons + '" -profile "' + $profilePath + '"'
# This launcher is for the user to open the interactive editor.
Start-Process -FilePath $Workbench -ArgumentList $arguments
