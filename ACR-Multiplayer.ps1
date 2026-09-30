# Arma Commander Reforger - multiplayer launcher for the Arland scenario.
#   Host: starts the Arma Reforger Server tool on this PC with the mod, waits for
#         the scenario to load, then joins it with the game.
#   Join: connects the game to a host's address.
param([ValidateSet('Host', 'Join')][string]$Mode = 'Join', [string]$Address = '', [int]$Port = 2001, [switch]$Online)
$ErrorActionPreference = 'Stop'

function Find-SteamApp([string]$folder, [string]$exe) {
  $steamRoot = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  $libraries = @($steamRoot, 'C:\Program Files (x86)\Steam', 'D:\SteamLibrary', 'E:\SteamLibrary', 'F:\SteamLibrary')
  if ($steamRoot) {
    $libraryFile = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryFile) {
      foreach ($m in [regex]::Matches((Get-Content -LiteralPath $libraryFile -Raw), '"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\', '\') }
    }
  }
  foreach ($library in $libraries) {
    if (!$library) { continue }
    $candidate = Join-Path $library "steamapps\common\$folder\$exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }
  }
  return $null
}


. (Join-Path $PSScriptRoot 'ACR-Update.ps1')

try {
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
    $serverArgs = '-server "Worlds/ACR_Arland.ent" ' + $mod + ' -profile "' + $serverProfile + '" -bindPort ' + $Port + ' -maxFPS 60' + $backend
    # Game Master: normally only the host. Friends' sessions can share it.
    $shareGM = Read-Host 'Give Game Master (Y key) to everyone who joins, not just you? (y/N)'
    if ($shareGM -match '^[yY]') { $serverArgs += ' -acrGameMasterAll 1'; Write-Host 'Everyone who joins gets Game Master.' -ForegroundColor Yellow }
    Write-Host 'Starting the server (a second window opens; leave it running)...' -ForegroundColor Cyan
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -PassThru

    Write-Host ''
    Write-Host 'Tell your friend one of these addresses (and port' $Port 'UDP):' -ForegroundColor Yellow
    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
      Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' } |
      ForEach-Object { Write-Host ('   ' + $_.IPAddress + '   (' + $_.InterfaceAlias + ')') }
    Write-Host '   Over the internet without a VPN: your public IP (search "what is my ip") + port forwarding.'
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
    if (!$Address) { $Address = Read-Host 'Host address (e.g. 26.12.34.56 or 192.168.1.20)' }
    $Address = $Address.Trim()
    if (!$Address) { throw 'No address given.' }
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
