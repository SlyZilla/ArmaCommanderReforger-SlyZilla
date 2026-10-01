# AC: Actual - ZeroTier helper for the multiplayer launcher.
# ZeroTier is a free "virtual LAN" app: every player joins the same ZeroTier network
# and the game connects over it as if everyone were on one home network. The launcher
# explains what will happen, asks first, then installs ZeroTier and joins the group's
# network. Choices are kept in zerotier.txt next to this file (Use=yes|no,
# Network=<16 hex id>). That file is never part of a release: players get the network
# ID from the host.

$script:ZtFile = Join-Path $PSScriptRoot 'zerotier.txt'
$script:ZtMsiUrls = @('https://download.zerotier.com/dist/ZeroTierOne.msi',
                      'https://download.zerotier.com/dist/ZeroTier%20One.msi')

function Read-ZtSettings {
  $s = @{ Use = ''; Network = '' }
  if (Test-Path -LiteralPath $script:ZtFile) {
    foreach ($line in Get-Content -LiteralPath $script:ZtFile) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -eq 2 -and $s.ContainsKey($kv[0].Trim())) { $s[$kv[0].Trim()] = $kv[1].Trim() }
    }
  }
  return $s
}

function Save-ZtSettings($s) {
  @(('Use=' + $s.Use), ('Network=' + $s.Network)) | Set-Content -LiteralPath $script:ZtFile
}

function Test-ZtInstalled {
  return [bool](Get-Service -Name 'ZeroTierOneService' -ErrorAction SilentlyContinue)
}

# This PC's address on the given ZeroTier network, or $null. Windows names each
# ZeroTier adapter "ZeroTier One [<network id>]".
function Get-ZtAddress([string]$network) {
  if (!$network) { return $null }
  $aliases = @(Get-NetAdapter -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like ('*' + $network + '*') -or $_.InterfaceDescription -like ('*' + $network + '*') } |
    ForEach-Object { $_.Name })
  $ip = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { ($_.InterfaceAlias -like ('*' + $network + '*') -or $aliases -contains $_.InterfaceAlias) -and
                   $_.IPAddress -notlike '169.254.*' } |
    Select-Object -First 1
  if ($ip) { return $ip.IPAddress }
  return $null
}

function Show-ZtExplanation {
  Write-Host ''
  Write-Host '=== CONNECTING OVER THE INTERNET WITH ZEROTIER ===' -ForegroundColor Cyan
  Write-Host ''
  Write-Host 'WHY: To play together, your PC and the host''s PC must be able to reach each'
  Write-Host 'other directly. Home routers normally block that. ZeroTier is a free app that'
  Write-Host 'puts the players'' PCs on one small private network, as if you were all on the'
  Write-Host 'same home Wi-Fi. The game then connects over it.'
  Write-Host ''
  Write-Host 'WHAT THIS LAUNCHER WILL DO:' -ForegroundColor Cyan
  Write-Host ' 1. Download ZeroTier One from zerotier.com and check that the installer is'
  Write-Host '    digitally signed by ZeroTier.'
  Write-Host ' 2. Install it. This adds a virtual network adapter and a background service'
  Write-Host '    that starts with Windows.'
  Write-Host ' 3. Join ONE network: the ID the host sends you. Only people who have that ID'
  Write-Host '    can join it.'
  Write-Host 'Steps 2 and 3 need administrator rights, so Windows will ask "Do you want to'
  Write-Host 'allow this app to make changes?" once. If ZeroTier is already installed, only'
  Write-Host 'step 3 happens.'
  Write-Host ''
  Write-Host 'WHAT IT MEANS FOR YOUR PC:' -ForegroundColor Cyan
  Write-Host ' - Traffic on the network is encrypted.'
  Write-Host ' - Your normal internet traffic does NOT go through it or through anyone else.'
  Write-Host ' - People on the network can reach your PC like someone on your home Wi-Fi.'
  Write-Host '   Windows treats it as a "Public" network, so file sharing stays off.'
  Write-Host ''
  Write-Host 'HOW TO UNDO IT:' -ForegroundColor Cyan
  Write-Host ' - Leave the network: double-click Leave-ZeroTier.cmd (next to Join-Arland.cmd).'
  Write-Host ' - Remove ZeroTier completely: Windows Settings > Apps > ZeroTier One > Uninstall.'
  Write-Host ''
  Write-Host 'Choose N if you are in the same house as the host, or already have another way'
  Write-Host 'to connect (port forwarding, Radmin, Tailscale...).'
}

# Runs one ZeroTier step with administrator rights (one Windows prompt).
# Action: join or leave. Msi: installer to run first, or ''.
function Invoke-ZtAdmin([string]$action, [string]$network, [string]$msi) {
  $temp = Join-Path $env:TEMP ('acr-zerotier-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force $temp | Out-Null
  $step = Join-Path $temp 'step.ps1'
  $log = Join-Path $temp 'step.log'
  @'
param([string]$Action, [string]$Network, [string]$Msi, [string]$Log)
function Say($m) { Add-Content -LiteralPath $Log -Value $m }
try {
  if ($Msi) {
    Say 'Installing ZeroTier One ...'
    $p = Start-Process -FilePath 'msiexec.exe' -ArgumentList ('/i "' + $Msi + '" /qn /norestart') -Wait -PassThru
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { Say ('The ZeroTier installer failed (code ' + $p.ExitCode + ').'); exit 1 }
    Say 'Installed.'
  }
  $svc = $null
  for ($i = 0; $i -lt 30 -and !$svc; $i++) { $svc = Get-Service -Name 'ZeroTierOneService' -ErrorAction SilentlyContinue; if (!$svc) { Start-Sleep 1 } }
  if (!$svc) { Say 'ZeroTier''s background service was not found.'; exit 1 }
  if ($svc.Status -ne 'Running') { Start-Service -Name 'ZeroTierOneService' }
  $exe = $null
  for ($i = 0; $i -lt 30 -and !$exe; $i++) {
    $exe = Get-ChildItem -LiteralPath (Join-Path $env:ProgramData 'ZeroTier\One') -Filter 'zerotier-one_*.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$exe) { Start-Sleep 1 }
  }
  if (!$exe) { Say 'ZeroTier''s command-line tool was not found.'; exit 1 }
  # Wait until the service answers (it can take a few seconds after installing).
  for ($i = 0; $i -lt 30; $i++) {
    $info = (& $exe.FullName -q info 2>&1 | Out-String)
    if ($info -match '^200') { break }
    Start-Sleep 1
  }
  if ($Action -eq 'leave') { Say ('Leaving network ' + $Network + ' ...') } else { Say ('Joining network ' + $Network + ' ...') }
  $out = (& $exe.FullName -q $Action $Network 2>&1 | Out-String).Trim()
  if ($out -notmatch '^200') { Say ('ZeroTier answered: ' + $out); exit 1 }
  Say 'Done.'
  exit 0
} catch { Say $_.Exception.Message; exit 1 }
'@ | Set-Content -LiteralPath $step
  $argLine = '-NoProfile -ExecutionPolicy Bypass -File "' + $step + '" -Action ' + $action + ' -Network ' + $network + ' -Log "' + $log + '"'
  if ($msi) { $argLine += ' -Msi "' + $msi + '"' }
  $ok = $false
  try {
    $p = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList $argLine
    $ok = ($p.ExitCode -eq 0)
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host ('   ' + $_) } }
  } catch {
    Write-Host '   The administrator prompt was declined, so nothing was changed.' -ForegroundColor Yellow
  }
  Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
  return $ok
}

# Downloads the ZeroTier installer and checks its signature. Returns its path.
function Get-ZtInstaller {
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  $old = $ProgressPreference
  $ProgressPreference = 'SilentlyContinue'   # the progress bar makes downloads very slow
  $dir = Join-Path $env:TEMP ('acr-zerotier-msi-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force $dir | Out-Null
  $msi = Join-Path $dir 'ZeroTierOne.msi'
  foreach ($url in $script:ZtMsiUrls) {
    try { Invoke-WebRequest -Uri $url -OutFile $msi -UseBasicParsing; break }
    catch { Remove-Item -LiteralPath $msi -ErrorAction SilentlyContinue }
  }
  $ProgressPreference = $old
  if (!(Test-Path -LiteralPath $msi)) { throw 'Could not download ZeroTier from zerotier.com. Check your internet connection, or install it by hand from zerotier.com/download and run this again.' }
  $sig = Get-AuthenticodeSignature -LiteralPath $msi
  if ($sig.Status -ne 'Valid' -or !$sig.SignerCertificate -or $sig.SignerCertificate.Subject -notmatch 'ZeroTier') {
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    throw 'The downloaded installer is not signed by ZeroTier, so it was NOT run. Install ZeroTier by hand from zerotier.com/download instead.'
  }
  $signer = ($sig.SignerCertificate.Subject -split ',')[0] -replace '^CN=', ''
  Write-Host ('   Checked: the installer is digitally signed by "' + $signer + '".') -ForegroundColor Green
  return $msi
}

# Makes sure this PC is on the group's ZeroTier network, asking first.
# Returns this PC's ZeroTier address, or $null when ZeroTier isn't used.
function Connect-ZeroTier {
  $s = Read-ZtSettings
  if ($s.Use -eq 'no') {
    Write-Host 'ZeroTier: not used on this PC. (To set it up, delete zerotier.txt in the ArmaCommanderReforger folder.)' -ForegroundColor DarkGray
    return $null
  }
  if ($s.Use -ne 'yes') {
    Show-ZtExplanation
    $a = (Read-Host 'Set up ZeroTier? Type Y (yes) or N (no)').Trim()
    if ($a -notlike 'y*') {
      $s.Use = 'no'
      Save-ZtSettings $s
      Write-Host 'OK, ZeroTier was not touched. You will need another way to connect (see READ ME FIRST.txt).' -ForegroundColor Yellow
      return $null
    }
    $s.Use = 'yes'
  }
  while ($s.Network -notmatch '^[0-9a-f]{16}$') {
    $n = (Read-Host 'Paste the ZeroTier network ID the host sent you (16 characters)').Trim().ToLower()
    if ($n -match '^[0-9a-f]{16}$') { $s.Network = $n }
    else { Write-Host 'That is not a network ID. It looks like 1a2b3c4d5e6f7a8b: 16 characters, digits 0-9 and letters a-f.' -ForegroundColor Red }
  }
  Save-ZtSettings $s

  $addr = Get-ZtAddress $s.Network
  if (!$addr) {
    Write-Host ''
    $msi = ''
    if (!(Test-ZtInstalled)) {
      Write-Host 'Downloading ZeroTier One from zerotier.com ...' -ForegroundColor Cyan
      $msi = Get-ZtInstaller
      $what = 'install ZeroTier One and join network ' + $s.Network
    } else {
      $what = 'join network ' + $s.Network + ' (ZeroTier is already installed)'
    }
    Write-Host ''
    Write-Host ('Windows will now ask for administrator permission. It is only to ' + $what + '. Click Yes.') -ForegroundColor Yellow
    Write-Host 'If Windows then asks whether this PC should be discoverable on a new network, click No.'
    [void](Read-Host 'Press Enter to continue')
    $ok = Invoke-ZtAdmin 'join' $s.Network $msi
    if ($msi) { Remove-Item -LiteralPath (Split-Path $msi -Parent) -Recurse -Force -ErrorAction SilentlyContinue }
    if (!$ok) {
      Write-Host 'ZeroTier setup did not finish (see the lines above). Continuing without it.' -ForegroundColor Yellow
      return $null
    }
    Write-Host -NoNewline 'Waiting for the network to give this PC an address'
    for ($i = 0; $i -lt 30 -and !$addr; $i++) {
      Start-Sleep -Seconds 2
      Write-Host -NoNewline '.'
      $addr = Get-ZtAddress $s.Network
    }
    Write-Host ''
    if (!$addr) {
      Write-Host 'No address yet. If the host''s network is set to Private, the host must approve this PC' -ForegroundColor Yellow
      Write-Host 'at my.zerotier.com first (it appears there as a new member). Continuing anyway.' -ForegroundColor Yellow
      return $null
    }
  }
  Write-Host ('ZeroTier: connected to network ' + $s.Network + '. This PC''s address there: ' + $addr) -ForegroundColor Green
  Write-Host '   It stays connected, so next time starts straight away. To disconnect, double-click' -ForegroundColor DarkGray
  Write-Host '   Leave-ZeroTier.cmd. To remove ZeroTier: Windows Settings > Apps > ZeroTier One > Uninstall.' -ForegroundColor DarkGray
  return $addr
}

# Leaves the group's ZeroTier network (ZeroTier itself stays installed).
function Disconnect-ZeroTier {
  $s = Read-ZtSettings
  Write-Host ''
  Write-Host '=== LEAVE THE ZEROTIER NETWORK ===' -ForegroundColor Cyan
  if (!(Test-ZtInstalled)) { Write-Host 'ZeroTier is not installed on this PC, so there is nothing to leave.'; return }
  if ($s.Network -notmatch '^[0-9a-f]{16}$') {
    $n = (Read-Host 'Which network ID should this PC leave? (16 characters)').Trim().ToLower()
    if ($n -notmatch '^[0-9a-f]{16}$') { Write-Host 'That is not a network ID. Nothing was changed.' -ForegroundColor Yellow; return }
    $s.Network = $n
  }
  Write-Host ('This disconnects this PC from ZeroTier network ' + $s.Network + '.')
  Write-Host 'ZeroTier itself stays installed. To remove it completely: Windows Settings > Apps >'
  Write-Host 'ZeroTier One > Uninstall. The next time you host or join, the launcher explains'
  Write-Host 'ZeroTier again and asks before reconnecting.'
  $a = (Read-Host 'Leave the network? Type Y (yes) or N (no)').Trim()
  if ($a -notlike 'y*') { Write-Host 'Nothing was changed.'; return }
  Write-Host 'Windows will now ask for administrator permission. It is only to leave the network. Click Yes.' -ForegroundColor Yellow
  if (Invoke-ZtAdmin 'leave' $s.Network '') {
    $s.Use = ''
    Save-ZtSettings $s
    Write-Host 'Left the network.' -ForegroundColor Green
  } else {
    Write-Host 'Leaving did not finish (see the lines above).' -ForegroundColor Yellow
  }
}
