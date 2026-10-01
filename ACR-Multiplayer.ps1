# AC: Actual (Arma Commander Actual) - multiplayer launcher for the Arland scenario.
#   Host: starts the Arma Reforger Server tool on this PC with the mod, waits for
#         the scenario to load, then joins it with the game.
#   Join: connects the game to a host's address.
#   Leave: leaves the ZeroTier network (see ACR-ZeroTier.ps1).
param([ValidateSet('Host', 'Join', 'Leave')][string]$Mode = 'Join', [string]$Address = '', [int]$Port = 2001, [switch]$Online)
$ErrorActionPreference = 'Stop'
# Everything this window shows also goes to launcher-log.txt (send it if something fails).
try { Start-Transcript -Path (Join-Path $PSScriptRoot 'launcher-log.txt') -Force | Out-Null } catch {}

. (Join-Path $PSScriptRoot 'ACR-Find.ps1')


. (Join-Path $PSScriptRoot 'ACR-Update.ps1')

. (Join-Path $PSScriptRoot 'ACR-ZeroTier.ps1')

# Host setup menu: session settings chosen before the server starts (remembered
# in host-settings.txt). Returns the extra server arguments and the world.
function Show-HostSetup {
  $file = Join-Path $PSScriptRoot 'host-settings.txt'
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Players = 'any'; StartRP = '100'; Income = '1'; AI = 'both'; Skill = 'Veteran';
                   Reaction = '1.6'; SoldierCap = '100'; UnitCap = '8'; Respawn = '15'; ShareGM = 'no'; SharedCommand = 'yes' }
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -eq 2 -and $s.Contains($kv[0].Trim())) { $s[$kv[0].Trim()] = $kv[1].Trim() }
    }
  }
  $worlds = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Worlds') -Filter 'ACR_*.ent' -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -notlike '*TestRange*' } | ForEach-Object { 'Worlds/' + $_.Name })
  if ($worlds.Count -eq 0) { $worlds = @('Worlds/ACR_Arland.ent') }
  if ($worlds -notcontains $s.World) { $s.World = $worlds[0] }
  $skills = [ordered]@{ Rookie = 20; Regular = 50; Veteran = 70; Expert = 80 }
  while ($true) {
    $mapName = [IO.Path]::GetFileNameWithoutExtension($s.World) -replace '^ACR_', ''
    Write-Host ''
    Write-Host '=== HOST SETUP ===  (type a number to change, Enter to start)' -ForegroundColor Cyan
    Write-Host ('  1  Map / scenario          ' + $mapName)
    Write-Host ('  2  Players'' side           ' + $s.Players + '   (any = each player picks; or US / USSR)')
    Write-Host ('  3  AI commanders           ' + $s.AI + '   (both / none / one faction key, e.g. USSR)')
    Write-Host ('  4  Starting RP             ' + $s.StartRP)
    Write-Host ('  5  Income                  x' + $s.Income)
    Write-Host ('  6  AI skill                ' + $s.Skill + '   (Rookie / Regular / Veteran / Expert)')
    Write-Host ('  7  AI reaction speed       ' + $s.Reaction + '   (1 = game default, higher = faster spotting)')
    Write-Host ('  8  Soldier limit per side  ' + $s.SoldierCap)
    Write-Host ('  9  AI squad limit          ' + $s.UnitCap)
    Write-Host (' 10  Commander respawn       ' + $s.Respawn + ' s')
    Write-Host (' 11  Game Master for all     ' + $s.ShareGM)
    Write-Host (' 12  Shared command          ' + $s.SharedCommand + '   (yes = several commanders per side)')
    $choice = (Read-Host 'Choice').Trim()
    if ($choice -eq '') { break }
    switch ($choice) {
      '1' { if ($worlds.Count -gt 1) { $i = [array]::IndexOf($worlds, $s.World); $s.World = $worlds[($i + 1) % $worlds.Count] } else { Write-Host 'Only one map is available so far.' -ForegroundColor Yellow } }
      '2' {
        # any -> US -> USSR -> any. Picking a side gives the AI the other one.
        $sides = @('any', 'US', 'USSR')
        $i = [array]::IndexOf($sides, $s.Players)
        $s.Players = $sides[($i + 1) % $sides.Count]
        if ($s.Players -eq 'US') { $s.AI = 'USSR' } elseif ($s.Players -eq 'USSR') { $s.AI = 'US' } else { $s.AI = 'both' }
        Write-Host ('AI commanders set to ' + $s.AI + ' (change with 3).') -ForegroundColor DarkGray
      }
      '3' { $v = Read-Host 'AI commanders: both, none, or the faction key that gets one (US, USSR, ...)'; if ($v.Trim()) { $s.AI = $v.Trim() } }
      '4' { $v = Read-Host 'Starting RP (e.g. 100)'; if ($v -match '^\d+$') { $s.StartRP = $v } }
      '5' { $v = Read-Host 'Income multiplier (e.g. 0.5, 1, 2)'; if ($v -match '^\d+(\.\d+)?$') { $s.Income = $v } }
      '6' { $v = Read-Host 'AI skill: Rookie, Regular, Veteran or Expert'; foreach ($k in $skills.Keys) { if ($k -like ($v.Trim() + '*') -and $v.Trim()) { $s.Skill = $k } } }
      '7' { $v = Read-Host 'AI reaction speed (0.5 - 3)'; if ($v -match '^\d+(\.\d+)?$') { $s.Reaction = $v } }
      '8' { $v = Read-Host 'Soldier limit per side (e.g. 100)'; if ($v -match '^\d+$') { $s.SoldierCap = $v } }
      '9' { $v = Read-Host 'AI squad limit (e.g. 8)'; if ($v -match '^\d+$') { $s.UnitCap = $v } }
      '10' { $v = Read-Host 'Commander respawn delay in seconds'; if ($v -match '^\d+$') { $s.Respawn = $v } }
      '11' { if ($s.ShareGM -eq 'yes') { $s.ShareGM = 'no' } else { $s.ShareGM = 'yes' } }
      '12' { if ($s.SharedCommand -eq 'yes') { $s.SharedCommand = 'no' } else { $s.SharedCommand = 'yes' } }
      default { Write-Host 'Type a number from the list, or press Enter to start.' -ForegroundColor Yellow }
    }
  }
  $s.Keys | ForEach-Object { $_ + '=' + $s[$_] } | Set-Content -LiteralPath $file
  $skill = $skills[$s.Skill]
  if (!$skill) { $skill = 70 }
  $extra = ' -acrStartRP ' + $s.StartRP + ' -acrIncome ' + $s.Income + ' -acrAI ' + $s.AI + ' -acrSkill ' + $skill +
           ' -acrReaction ' + $s.Reaction + ' -acrSoldierCap ' + $s.SoldierCap + ' -acrUnitCap ' + $s.UnitCap + ' -acrRespawnSeconds ' + $s.Respawn + ' -acrPlayers ' + $s.Players
  if ($s.ShareGM -eq 'yes') { $extra += ' -acrGameMasterAll 1' }
  if ($s.SharedCommand -eq 'no') { $extra += ' -acrSharedCommand 0' } else { $extra += ' -acrSharedCommand 1' }
  return @{ Args = $extra; World = $s.World }
}

try {
  if ($Mode -eq 'Leave') {
    Disconnect-ZeroTier
    Read-Host 'Press Enter to close'
    exit 0
  }
  if (Update-FromGitHub) {
    # Run the freshly downloaded launcher instead of this old copy.
    $again = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-Mode', $Mode, '-Port', $Port)
    if ($Address) { $again += @('-Address', $Address) }
    if ($Online) { $again += '-Online' }
    & powershell.exe @again
    exit $LASTEXITCODE
  }
  $addonsPath = Split-Path $PSScriptRoot -Parent
  $mod = '-addonsDir "' + $addonsPath + '" -addons AC230926A0010001'
  $backend = ' -noBackend'
  if ($Online) { $backend = '' }
  $game = Find-SteamApp 'Arma Reforger' 'ArmaReforgerSteam.exe'
  if (!$game) { throw 'Arma Reforger was not found in your Steam libraries.' }

  if ($Mode -eq 'Host') {
    $server = Find-SteamApp 'Arma Reforger Server' 'ArmaReforgerServer.exe'
    if (!$server) { throw 'The "Arma Reforger Server" tool is not installed. In Steam: Library > (filter) Tools > Arma Reforger Server > Install. It is free. Then run this again.' }
    $serverProfile = Join-Path $PSScriptRoot '.local-server'
    $started = Get-Date
    $setup = Show-HostSetup
    $zt = Connect-ZeroTier
    $serverArgs = '-server "' + $setup.World + '" ' + $mod + ' -profile "' + $serverProfile + '" -bindPort ' + $Port + ' -maxFPS 60' + $backend + $setup.Args
    Write-Host 'Starting the server (a second window opens; leave it running)...' -ForegroundColor Cyan
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -PassThru

    Write-Host ''
    # Public IP for friends coming over the internet (needs UDP port forwarding).
    $public = $null
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}
    foreach ($url in 'https://api.ipify.org', 'https://checkip.amazonaws.com') {
      try {
        $r = (Invoke-RestMethod -Uri $url -TimeoutSec 5 -UseBasicParsing).ToString().Trim()
        if ($r -match '^\d{1,3}(\.\d{1,3}){3}$') { $public = $r; break }
      } catch {}
    }
    Write-Host ('Tell your friends ONE of these addresses (port ' + $Port + ' UDP):') -ForegroundColor Yellow
    if ($zt) {
      Write-Host ('   ' + $zt + '   <- friends on ZeroTier (they also need your network ID, in zerotier.txt)') -ForegroundColor Yellow
    }
    if ($public) {
      Write-Host ('   ' + $public + '   <- friends over the internet without ZeroTier (needs UDP ' + $Port + ' forwarded on your router)') -ForegroundColor Yellow
    } else {
      Write-Host '   (Could not look up your public IP: search "what is my ip". Needs port forwarding.)'
    }
    Write-Host 'Other addresses of this PC (same house, other VPN apps):'
    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
      Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and $_.IPAddress -ne $zt } |
      ForEach-Object { Write-Host ('   ' + $_.IPAddress + '   (' + $_.InterfaceAlias + ')') }
    Write-Host ''

    # Wait until the scenario has loaded on the server (it prints "[ACR] Scenario started").
    $logRoot = Join-Path $serverProfile 'logs'
    $ready = $false
    for ($i = 0; $i -lt 90 -and !$ready; $i++) {
      Start-Sleep -Seconds 2
      if ($proc.HasExited) { throw 'The server closed while loading. Its log is in ' + $logRoot }
      if (Test-Path -LiteralPath $logRoot) {
        $dir = Get-ChildItem -LiteralPath $logRoot -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } | Sort-Object CreationTime -Descending | Select-Object -First 1
        if ($dir) {
          $log = Join-Path $dir.FullName 'console.log'
          if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern '\[ACR\] Scenario started' -Quiet)) { $ready = $true }
        }
      }
      Write-Host -NoNewline '.'
    }
    Write-Host ''
    if (!$ready) { Write-Host 'The server is taking long; joining anyway.' -ForegroundColor Yellow }
    Start-Sleep -Seconds 5
    $Address = '127.0.0.1'
  }
  else {
    $zt = Connect-ZeroTier
    # Ask until we get something that looks like an address (an IP like 26.12.34.56,
    # or a name with a dot), remembering the last one used.
    $lastFile = Join-Path $PSScriptRoot 'last-host.txt'
    $last = ''
    if (Test-Path -LiteralPath $lastFile) { $last = (Get-Content -LiteralPath $lastFile -Raw).Trim() }
    while ($true) {
      if (!$Address) {
        Write-Host ''
        Write-Host 'Type the HOST''S ADDRESS: the IP the host''s window shows, e.g. 10.147.17.5' -ForegroundColor Yellow
        Write-Host 'or 192.168.1.20 (same house). Not "y" -- the address.'
        $prompt = 'Host address'
        if ($last) { $prompt += ' [Enter = ' + $last + ']' }
        $Address = Read-Host $prompt
        if (!$Address.Trim() -and $last) { $Address = $last }
      }
      $Address = $Address.Trim()
      if ($Address -match '^(\d{1,3}(\.\d{1,3}){3}|localhost|[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+)(:\d+)?$') { break }
      Write-Host ('"' + $Address + '" is not an address. It should look like 10.147.17.5') -ForegroundColor Red
      $Address = ''
    }
    Set-Content -LiteralPath $lastFile -Value $Address
  }

  if ($Address -notmatch ':\d+$') { $Address = $Address + ':' + $Port }
  # The game uses the player's normal profile (controls, graphics, name).
  $clientArgs = '-client ' + $Address + ' ' + $mod + ' -noSplash' + $backend
  Write-Host ('Joining ' + $Address + ' ...') -ForegroundColor Cyan
  Start-Process -FilePath $game -ArgumentList $clientArgs -WorkingDirectory (Split-Path $game -Parent)
  if ($Mode -eq 'Host') {
    Write-Host ''
    Write-Host 'Game started. Keep this window and the server window open while you play.' -ForegroundColor Green
    Write-Host 'To end the session: quit the game, then close the server window.'
    Read-Host 'Press Enter to close this window (the server keeps running)'
  }
} catch {
  Write-Host $_.Exception.Message -ForegroundColor Red
  Read-Host 'Press Enter to close'
  exit 1
}
