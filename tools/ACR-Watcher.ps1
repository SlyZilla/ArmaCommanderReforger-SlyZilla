# Arma Commander test launcher for Claude.
#
# Leave this window open. Claude drops a request file into .acr-requests\ naming
# one of the presets below; this script launches Reforger, waits for the run to
# finish, closes the game it started, and writes a result file with the [ACR]
# log lines. It can only run these presets. It never touches a game you started
# yourself: if Reforger is already running, the request is refused.
#
# Close this window (or press Ctrl+C) to stop it.

param([string]$GamePath)
$ErrorActionPreference = 'Stop'

$Presets = @{
    'compile'    = @{ Flags = '';                        TimeoutSec = 120; DoneWhen = '\[ACR\] Squad registered' }
    'auto'       = @{ Flags = '-acrAutoTest 1';          TimeoutSec = 180; DoneWhen = '\[ACR\] TEST COMPLETE' }
    'command'    = @{ Flags = '-acrCommandTest 1';       TimeoutSec = 200; DoneWhen = '\[ACR\] TEST COMPLETE' }
    'maphandoff' = @{ Flags = '-acrMapHandoffTest 1';    TimeoutSec = 180; DoneWhen = '\[ACR\] TEST COMPLETE' }
}

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$AddonsDir   = Split-Path $ProjectRoot -Parent
$RequestDir  = Join-Path $ProjectRoot '.acr-requests'
$ProfileDir  = Join-Path $ProjectRoot '.local-game'
$LogRoot     = Join-Path $ProfileDir 'logs'
New-Item -ItemType Directory -Force $RequestDir | Out-Null

function Find-Game {
    if ($GamePath -and (Test-Path -LiteralPath $GamePath)) { return $GamePath }
    $steamRoot = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    $libraries = @($steamRoot, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
    if ($steamRoot) {
        $vdf = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $libraries += $m.Groups[1].Value.Replace('\\', '\')
            }
        }
    }
    foreach ($lib in $libraries) {
        if (!$lib) { continue }
        $candidate = Join-Path $lib 'steamapps\common\Arma Reforger\ArmaReforgerSteam.exe'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'Arma Reforger was not found. Start this script with -GamePath pointing to ArmaReforgerSteam.exe.'
}

function Write-Result($id, $lines) {
    $path = Join-Path $RequestDir "$id.result.txt"
    $lines | Set-Content -LiteralPath $path -Encoding UTF8
}

function Invoke-Request($file) {
    $id = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    $preset = (Get-Content -LiteralPath $file.FullName -Raw).Trim().ToLower()
    Remove-Item -LiteralPath $file.FullName
    $time = Get-Date -Format 'HH:mm:ss'
    if (!$Presets.ContainsKey($preset)) {
        Write-Host "[$time] Refused unknown preset '$preset'" -ForegroundColor Yellow
        Write-Result $id @("status=refused", "reason=unknown preset '$preset'")
        return
    }
    if (Get-Process -Name 'ArmaReforgerSteam', 'ArmaReforger_BE' -ErrorAction SilentlyContinue) {
        Write-Host "[$time] Refused '$preset': Reforger is already running" -ForegroundColor Yellow
        Write-Result $id @("status=refused", "reason=Reforger is already running")
        return
    }

    $config = $Presets[$preset]
    $game = Find-Game
    $gameArgs = '-addonsDir "' + $AddonsDir + '" -addons AC230926A0010001 -profile "' + $ProfileDir + '" -noSplash -noBackend -world "Worlds/ACR_TestRange.ent" ' + $config.Flags
    Write-Host "[$time] Running '$preset'..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $game -ArgumentList $gameArgs -WorkingDirectory (Split-Path $game -Parent) -PassThru

    $outcome = 'timeout'
    $log = $null
    while ($true) {
        Start-Sleep -Seconds 3
        if (!$log -and (Test-Path -LiteralPath $LogRoot)) {
            $dir = Get-ChildItem -LiteralPath $LogRoot -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } |
                   Sort-Object CreationTime -Descending | Select-Object -First 1
            if ($dir) { $log = Join-Path $dir.FullName 'console.log' }
        }
        if ($proc.HasExited) { $outcome = 'game exited'; break }
        if ($log -and (Test-Path -LiteralPath $log)) {
            $text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
            $scriptLog = Join-Path (Split-Path $log -Parent) 'script.log'
            if (Test-Path -LiteralPath $scriptLog) { $text += Get-Content -LiteralPath $scriptLog -Raw -ErrorAction SilentlyContinue }
            if ($text -match 'SCRIPT\s+\(E\)') { Start-Sleep -Seconds 3; $outcome = 'script error'; break }
            if ($text -match $config.DoneWhen) { Start-Sleep -Seconds 2; $outcome = 'completed'; break }
        }
        if (((Get-Date) - $started).TotalSeconds -gt $config.TimeoutSec) { break }
    }
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue }

    $result = @("status=$outcome", "preset=$preset", "started=$($started.ToString('s'))", "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "log=$log", '---')
    if ($log -and (Test-Path -LiteralPath $log)) {
        $result += Get-Content -LiteralPath $log | Where-Object { $_ -match '\[ACR\]|SCRIPT\s+\(E\)|SCRIPT\s+\(W\).*ACR' }
        $scriptLog = Join-Path (Split-Path $log -Parent) 'script.log'
        if (Test-Path -LiteralPath $scriptLog) {
            $result += '--- script.log errors'
            $result += Get-Content -LiteralPath $scriptLog | Where-Object { $_ -match '\(E\)' } | Select-Object -First 40
        }
    } else {
        $result += 'No console.log was found for this run.'
    }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] '$preset' finished: $outcome" -ForegroundColor Green
}

Write-Host 'Arma Commander test launcher is running. Leave this window open; close it to stop.' -ForegroundColor Green
Write-Host "Watching $RequestDir"
while ($true) {
    try {
        Get-ChildItem -LiteralPath $RequestDir -Filter '*.request' | Sort-Object CreationTime | ForEach-Object { Invoke-Request $_ }
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    Start-Sleep -Seconds 2
}
