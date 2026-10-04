# AC: Actual test launcher for Claude.
#
# Leave this window open. Claude drops a request file into .acr-requests\ naming
# one of the presets below; this script launches Reforger, waits for the run to
# finish, closes the game it started, and writes a result file with the [ACR]
# log lines. It can only run these presets. It never touches a game you started
# yourself: if Reforger is already running, the request is refused.
#
# Besides test presets it accepts one other request, 'publish': it runs
# tools\Publish-Release.ps1 exactly like double-clicking Publish-Release.cmd
# (uploads the already-built dist\ACActual-Arland.zip with your GitHub login).
# And 'push': runs 'git push origin main' for this repo (backup of committed
# work to the private GitHub repo, with your GitHub login). Nothing else.
#
# A request of the form 'srv:<preset>' runs the test on a hidden dedicated
# server instead (no game window, no focus stolen, you can keep using the PC
# or even play at the same time; it uses its own profile and port).
#
# Close this window (or press Ctrl+C) to stop it.

param([string]$GamePath)
$ErrorActionPreference = 'Stop'

$Presets = @{
    'compile'    = @{ Flags = '';                        TimeoutSec = 120; DoneWhen = '\[ACR\] Squad registered' }
    'auto'       = @{ Flags = '-acrAutoTest 1';          TimeoutSec = 180; DoneWhen = '\[ACR\] TEST COMPLETE' }
    'command'    = @{ Flags = '-acrCommandTest 1';       TimeoutSec = 200; DoneWhen = '\[ACR\] TEST COMPLETE' }
    'inspect'    = @{ Workbench = $true;                 TimeoutSec = 300 }
    'maphandoff' = @{ Flags = '-acrMapHandoffTest 1';    TimeoutSec = 180; DoneWhen = '\[ACR\] TEST COMPLETE' }
}

$ProjectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $ProjectRoot 'ACR-Find.ps1')
$TestServerProfile = Join-Path $ProjectRoot '.local-testserver'
$script:TestServerPid = 0
$AddonsDir   = Split-Path $ProjectRoot -Parent
$RequestDir  = Join-Path $ProjectRoot '.acr-requests'
$ProfileDir  = Join-Path $ProjectRoot '.local-game'
$LogRoot     = Join-Path $ProfileDir 'logs'
New-Item -ItemType Directory -Force $RequestDir | Out-Null
Set-Content -LiteralPath (Join-Path $RequestDir 'watcher-started.txt') -Value (Get-Date -Format o)

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

function Find-Workbench {
    $game = Find-Game
    $steamApps = Split-Path (Split-Path $game -Parent) -Parent
    $candidates = @((Join-Path $steamApps 'Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe'))
    foreach ($drive in 'C','D','E','F','G') {
        $candidates += "${drive}:\SteamLibrary\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe"
    }
    foreach ($c in $candidates) { if (Test-Path -LiteralPath $c) { return $c } }
    throw 'Arma Reforger Tools (Workbench) was not found.'
}

# Headless Workbench run that exports vanilla files listed in .local-inspect\profile\requests.txt.
function Invoke-Inspect($id) {
    $game = Find-Game
    $wb = Find-Workbench
    $inspectProfile = Join-Path $ProjectRoot '.local-inspect'
    New-Item -ItemType Directory -Force (Join-Path $inspectProfile 'profile') | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $inspectProfile 'profile') -Filter 'export_*.txt' -ErrorAction SilentlyContinue | Remove-Item
    $gproj = Join-Path $PSScriptRoot 'inspector\addon.gproj'
    $gameAddons = Join-Path (Split-Path $game -Parent) 'addons'
    $wbArgs = '-gproj "' + $gproj + '" -addonsDir "' + $gameAddons + '" -profile "' + $inspectProfile + '" -noThrow -wbmodule=ResourceManager -plugin=ACR_InspectPlugin'
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Running 'inspect' (Workbench, hidden)..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $wb -ArgumentList $wbArgs -WindowStyle Hidden -PassThru
    $outcome = 'completed'
    if (!$proc.WaitForExit(300000)) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue; $outcome = 'timeout' }
    $exports = @(Get-ChildItem -LiteralPath (Join-Path $inspectProfile 'profile') -Filter 'export_*.txt' -ErrorAction SilentlyContinue).Count
    Write-Result $id @("status=$outcome", 'preset=inspect', "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "exports=$exports")
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 'inspect' finished: $outcome, $exports files" -ForegroundColor Green
}

# Runs the release script unattended and reports its outcome and log.
function Invoke-Publish($id) {
    $script = Join-Path $PSScriptRoot 'Publish-Release.ps1'
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Publishing the current package to GitHub..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath 'powershell.exe' -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "' + $script + '"') -WindowStyle Hidden -PassThru
    $outcome = 'completed'
    if (!$proc.WaitForExit(300000)) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue; $outcome = 'timeout' }
    elseif ($proc.ExitCode -ne 0) { $outcome = 'failed' }
    $result = @("status=$outcome", 'preset=publish', "seconds=$([int]((Get-Date) - $started).TotalSeconds)", '---')
    $logFile = Join-Path $ProjectRoot 'dist\publish-log.txt'
    if (Test-Path -LiteralPath $logFile) { $result += Get-Content -LiteralPath $logFile | Select-Object -Last 25 }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Publish finished: $outcome" -ForegroundColor Green
}

# Pushes committed work on main to the private GitHub repo (fixed command, no arguments taken).
function Invoke-Push($id) {
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Pushing main to GitHub..." -ForegroundColor Cyan
    $started = Get-Date
    $out = Join-Path $env:TEMP 'acr-push-out.txt'
    $err = Join-Path $env:TEMP 'acr-push-err.txt'
    $proc = Start-Process -FilePath 'git' -ArgumentList 'push origin main' -WorkingDirectory $ProjectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    $null = $proc.Handle  # cache the handle, or ExitCode comes back empty and a good push reads as failed
    $outcome = 'completed'
    if (!$proc.WaitForExit(120000)) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue; $outcome = 'timeout' }
    elseif ($proc.ExitCode -ne 0) { $outcome = 'failed' }
    $result = @("status=$outcome", 'preset=push', "seconds=$([int]((Get-Date) - $started).TotalSeconds)", '---')
    foreach ($f in @($out, $err)) { if (Test-Path -LiteralPath $f) { $result += Get-Content -LiteralPath $f | Select-Object -Last 15 } }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Push finished: $outcome" -ForegroundColor Green
}

# Scenario id for a world: Worlds/X.ent -> "{GUID}Missions/X.conf" from its .meta.
function Get-TestMissionId($world) {
    $name = [IO.Path]::GetFileNameWithoutExtension($world)
    $meta = Join-Path $ProjectRoot ('Missions/' + $name + '.conf.meta')
    if (!(Test-Path -LiteralPath $meta)) { return $null }
    $m = Select-String -LiteralPath $meta -Pattern 'Name\s+"(\{[0-9A-F]{16}\}Missions/[^"]+)"' | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value }
    return $null
}

# Runs a test preset on a hidden dedicated server (own profile, port 2302).
function Invoke-ServerTest($id, $preset, $config) {
    $time = Get-Date -Format 'HH:mm:ss'
    if ($config.Workbench) { Write-Result $id @('status=refused', 'reason=inspect cannot run on the server'); return }
    # Never two test servers at once (an old one is closed first).
    if ($script:TestServerPid) { Stop-Process -Id $script:TestServerPid -ErrorAction SilentlyContinue; $script:TestServerPid = 0 }
    $server = Find-SteamApp 'Arma Reforger Server' 'ArmaReforgerServer.exe'
    if (!$server) { Write-Result $id @('status=refused', 'reason=Arma Reforger Server not found'); return }
    $world = 'Worlds/ACR_TestRange.ent'
    if ($config.World) { $world = $config.World }
    New-Item -ItemType Directory -Force $TestServerProfile | Out-Null
    # -bindPort is ignored in -server mode (it listens on 2001 and clashes with a hosted
    # server). The start line and server config come from tools/testserver-args.txt and
    # tools/testserver.json, re-read per request so they can change without a restart.
    # Placeholders: {world} {mission} {profile} {addonsDir} {config}.
    $mission = Get-TestMissionId $world
    $argsFile = Join-Path $PSScriptRoot 'testserver-args.txt'
    $template = '-server "{world}" -addonsDir "{addonsDir}" -addons AC230926A0010001 -profile "{profile}" -maxFPS 60 -noBackend'
    if ($mission -and (Test-Path -LiteralPath $argsFile)) {
        $line = Get-Content -LiteralPath $argsFile | Where-Object { $_ -match '\S' -and $_ -notmatch '^\s*#' } | Select-Object -First 1
        if ($line) { $template = $line.Trim() }
    }
    $cfgPath = Join-Path $TestServerProfile 'testserver.json'
    $cfgTemplate = Join-Path $PSScriptRoot 'testserver.json'
    if ($template -match '\{config\}' -and (Test-Path -LiteralPath $cfgTemplate)) {
        $json = (Get-Content -LiteralPath $cfgTemplate -Raw).Replace('{mission}', $mission)
        Set-Content -LiteralPath $cfgPath -Value $json -Encoding ASCII
    }
    $serverArgs = $template.Replace('{world}', $world).Replace('{mission}', [string]$mission).Replace('{profile}', $TestServerProfile).Replace('{addonsDir}', $AddonsDir).Replace('{config}', $cfgPath) + ' ' + $config.Flags
    Write-Host "[$time] Running '$preset' on a hidden test server..." -ForegroundColor Cyan
    $started = Get-Date
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -WindowStyle Hidden -PassThru
    $script:TestServerPid = $proc.Id
    $serverLogs = Join-Path $TestServerProfile 'logs'
    $outcome = 'timeout'
    $log = $null
    while ($true) {
        Start-Sleep -Seconds 3
        if (!$log -and (Test-Path -LiteralPath $serverLogs)) {
            $dir = Get-ChildItem -LiteralPath $serverLogs -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } |
                   Sort-Object CreationTime -Descending | Select-Object -First 1
            if ($dir) { $log = Join-Path $dir.FullName 'console.log' }
        }
        if ($proc.HasExited) { $outcome = 'server exited'; break }
        if ($log -and (Test-Path -LiteralPath $log)) {
            $text = Get-Content -LiteralPath $log -Raw -ErrorAction SilentlyContinue
            $text = Remove-KnownNoise $text
            if ($text -match 'SCRIPT\s+\(E\)') { Start-Sleep -Seconds 3; $outcome = 'script error'; break }
            if ($text -match $config.DoneWhen) { Start-Sleep -Seconds 2; $outcome = 'completed'; break }
        }
        if (((Get-Date) - $started).TotalSeconds -gt ($config.TimeoutSec + 30)) { break }
    }
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue }
    $script:TestServerPid = 0
    $result = @("status=$outcome", "preset=srv:$preset", "started=$($started.ToString('s'))", "seconds=$([int]((Get-Date) - $started).TotalSeconds)", "log=$log", '---')
    if ($log -and (Test-Path -LiteralPath $log)) {
        $result += Get-Content -LiteralPath $log | Where-Object { $_ -match '\[ACR\]|SCRIPT\s+\(E\)|SCRIPT\s+\(W\).*ACR|Can.t compile' }
    } else {
        $result += 'No server console.log was found for this run.'
    }
    Write-Result $id $result
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 'srv:$preset' finished: $outcome" -ForegroundColor Green
}

# Known harmless vanilla errors, shared by game and server tests.
function Remove-KnownNoise($text) {
    # Deaths caused by Game Master effect modules.
    $text = ($text -split "`n" | Where-Object { $_ -notmatch 'SCR_InstigatorContextData: instigatorType' }) -join "`n"
    # Vanilla AI bug: a group whose members all died processes a failed move (null leader).
    $text = [regex]::Replace($text, "SCRIPT\s+\(E\): Virtual Machine Exception\s+Reason: NULL pointer[^\r\n]*\s+Class:\s+'SCR_AIProcessFailedMovementResult'", 'vanilla AI null-leader exception (ignored)')
    $text = [regex]::Replace($text, "SCRIPT\s+\(E\): Virtual Machine Exception\s+Reason: No suppression volume provided!", 'vanilla suppression exception (ignored)')
    # Vanilla SCR_AIGetFireTimes: RandomFloat(0.1, m_fShotSpan * 1.9) when the shot span is 0.
    $text = [regex]::Replace($text, "SCRIPT\s+\(E\): Math\.RandomFloat: invalid parameters min = 0\.100000 max = 0\.000000", 'vanilla AI fire-time warning (ignored)')
    # Vanilla ammo storage (built composition) on a dedicated server: its resupply action has no storage manager.
    $text = ($text -split "`n" | Where-Object { $_ -notmatch "SCR_ResupplySelfSupportStationAction.*no storage manager found" }) -join "`n"
    return $text
}

function Write-Result($id, $lines) {
    $path = Join-Path $RequestDir "$id.result.txt"
    $lines | Set-Content -LiteralPath $path -Encoding UTF8
}

function Invoke-Request($file) {
    $id = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    $preset = (Get-Content -LiteralPath $file.FullName -Raw).Trim().ToLower()
    $onServer = $false
    if ($preset.StartsWith('srv:')) { $onServer = $true; $preset = $preset.Substring(4) }
    Remove-Item -LiteralPath $file.FullName
    $time = Get-Date -Format 'HH:mm:ss'
    # Extra test presets from tools\test-presets.txt ("name|-acrSomethingTest 1|timeoutSec"),
    # re-read on every request so new tests don't need a launcher restart. Only
    # -acr...Test flags are accepted, so this can't be used to pass other options.
    $extraFile = Join-Path $PSScriptRoot 'test-presets.txt'
    if (Test-Path -LiteralPath $extraFile) {
        foreach ($line in Get-Content -LiteralPath $extraFile) {
            # optional 4th field: world, e.g. Worlds/ACR_Arland.ent
            $parts = $line.Split('|')
            if ($parts.Count -lt 3 -or $parts[1] -notmatch '^-acr[A-Za-z]+Test 1$') { continue }
            $world = 'Worlds/ACR_TestRange.ent'
            if ($parts.Count -ge 4 -and $parts[3] -match '^Worlds/[A-Za-z0-9_]+\.ent$') { $world = $parts[3] }
            $Presets[$parts[0].Trim().ToLower()] = @{ Flags = $parts[1]; TimeoutSec = [int]$parts[2]; DoneWhen = '\[ACR\] TEST COMPLETE'; World = $world }
        }
    }
    if ($preset -eq 'publish') { Invoke-Publish $id; return }
    if ($preset -eq 'push') { Invoke-Push $id; return }
    if (!$Presets.ContainsKey($preset)) {
        Write-Host "[$time] Refused unknown preset '$preset'" -ForegroundColor Yellow
        Write-Result $id @("status=refused", "reason=unknown preset '$preset'")
        return
    }
    if ($onServer) { Invoke-ServerTest $id $preset $Presets[$preset]; return }
    # A game the previous test just stopped can take a few seconds to close.
    for ($w = 0; $w -lt 15 -and (Get-Process -Name 'ArmaReforgerSteam', 'ArmaReforger_BE' -ErrorAction SilentlyContinue); $w++) { Start-Sleep -Seconds 2 }
    if (Get-Process -Name 'ArmaReforgerSteam', 'ArmaReforger_BE' -ErrorAction SilentlyContinue) {
        Write-Host "[$time] Refused '$preset': Reforger is already running" -ForegroundColor Yellow
        Write-Result $id @("status=refused", "reason=Reforger is already running")
        return
    }

    $config = $Presets[$preset]
    if ($config.Workbench) { Invoke-Inspect $id; return }
    $world = 'Worlds/ACR_TestRange.ent'
    if ($config.World) { $world = $config.World }
    $game = Find-Game
    $gameArgs = '-addonsDir "' + $AddonsDir + '" -addons AC230926A0010001 -profile "' + $ProfileDir + '" -noSplash -noBackend -world "' + $world + '" ' + $config.Flags
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
            $text = Remove-KnownNoise $text
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

Write-Host 'AC: Actual test launcher is running. Leave this window open; close it to stop.' -ForegroundColor Green
Write-Host "Watching $RequestDir"
while ($true) {
    try {
        Get-ChildItem -LiteralPath $RequestDir -Filter '*.request' | Sort-Object CreationTime | ForEach-Object { Invoke-Request $_ }
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    }
    Start-Sleep -Seconds 2
}
