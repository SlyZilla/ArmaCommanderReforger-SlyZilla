# Map and optional mod loading for Play/Host/Join. Match settings are chosen in game.
# Map/mod choices are remembered in host-settings.txt, play-settings.txt or join-settings.txt.
function Get-PairFactionKeys([string]$pair) {
  switch ($pair) {
    'ru_us' { return @('RHS_USAF', 'RHS_AFRF') }
    'ru_ua' { return @('US', 'RHS_AFRF') }
    'us_ua' { return @('RHS_USAF', 'US') }
    default { return @('US', 'USSR') }
  }
}

function Get-PairLabel([string]$pair) {
  switch ($pair) {
    'ru_us' { return 'RHS Russia vs RHS US' }
    'ru_ua' { return 'RHS Russia vs UA (UA replaces vanilla US)' }
    'us_ua' { return 'RHS US vs UA (UA replaces vanilla US)' }
    default { return 'Vanilla US vs USSR' }
  }
}

function Get-ACRAddonList([string]$pair, [string]$ace) {
  $ids = @('AC230926A0010001')
  if ($pair -ne 'vanilla') {
    $ids += @('595F2BF2F44836FB', '1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E')
    if ($pair -eq 'ru_ua' -or $pair -eq 'us_ua') { $ids += '65F92D51845AC237' }
  }
  if ($ace -eq 'yes') { $ids += '60C4E0B49618CC62' }
  return ($ids -join ',')
}

function Show-HostSetup([switch]$Solo) {
  $name = 'host-settings.txt'
  if ($Solo) { $name = 'play-settings.txt' }
  $file = Join-Path $PSScriptRoot $name
  # Only pre-load choices belong here. Ignore old saved match settings.
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Factions = 'vanilla'; ACE = 'no'; FireManeuver = 'no'; CombatManeuver = 'no'; CrewTurnout = 'no'; DriverAlign = 'no' }
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
  if (@('vanilla', 'ru_us', 'ru_ua', 'us_ua') -notcontains $s.Factions) { $s.Factions = 'vanilla' }
  if (@('yes', 'no') -notcontains $s.ACE) { $s.ACE = 'no' }
  if (@('yes', 'no') -notcontains $s.FireManeuver) { $s.FireManeuver = 'no' }
  if (@('yes', 'no') -notcontains $s.CombatManeuver) { $s.CombatManeuver = 'no' }
  if (@('yes', 'no') -notcontains $s.CrewTurnout) { $s.CrewTurnout = 'no' }
  if (@('yes', 'no') -notcontains $s.DriverAlign) { $s.DriverAlign = 'no' }
  while ($true) {
    $mapName = [IO.Path]::GetFileNameWithoutExtension($s.World) -replace '^ACR_', ''
    Write-Host ''
    $title = '=== HOST: MAP AND MODS ==='
    if ($Solo) { $title = '=== PLAY: MAP AND MODS ===' }
    Write-Host ($title + '  (type a number to change, Enter to launch)') -ForegroundColor Cyan
    Write-Host (' 1  Map                     ' + $mapName)
    Write-Host (' 2  Faction mods            ' + (Get-PairLabel $s.Factions))
    Write-Host (' 3  ACE All in One          ' + $s.ACE)
    Write-Host (' 4  BBB fire maneuver       ' + $s.FireManeuver + '   (experimental, for play-testing)')
    Write-Host (' 5  BBB combat maneuver     ' + $s.CombatManeuver + '   (experimental, for play-testing)')
    Write-Host (' 6  BBB crew turnout        ' + $s.CrewTurnout + '   (experimental, for play-testing)')
    Write-Host (' 7  BBB driver alignment    ' + $s.DriverAlign + '   (experimental, for play-testing)')
    Write-Host 'Choose the playable sides, battlefield and match settings in game, then press Start.' -ForegroundColor DarkGray
    Write-Host 'Faction mods must be loaded now; UA replaces vanilla US while loaded.' -ForegroundColor DarkGray
    if (!$Solo) { Write-Host 'Connect before friends: the first connection controls setup; access does not transfer after disconnect.' -ForegroundColor Yellow }
    $choice = (Read-Host 'Choice').Trim()
    if ($choice -eq '') { break }
    switch ($choice) {
      '1' { if ($worlds.Count -gt 1) { $i = [array]::IndexOf($worlds, $s.World); $s.World = $worlds[($i + 1) % $worlds.Count] } else { Write-Host 'Only one map is available.' -ForegroundColor Yellow } }
      '2' { $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua'); $i = [array]::IndexOf($pairs, $s.Factions); $s.Factions = $pairs[($i + 1) % $pairs.Count] }
      '3' { if ($s.ACE -eq 'yes') { $s.ACE = 'no' } else { $s.ACE = 'yes' } }
      '4' { if ($s.FireManeuver -eq 'yes') { $s.FireManeuver = 'no' } else { $s.FireManeuver = 'yes' } }
      '5' { if ($s.CombatManeuver -eq 'yes') { $s.CombatManeuver = 'no' } else { $s.CombatManeuver = 'yes' } }
      '6' { if ($s.CrewTurnout -eq 'yes') { $s.CrewTurnout = 'no' } else { $s.CrewTurnout = 'yes' } }
      '7' { if ($s.DriverAlign -eq 'yes') { $s.DriverAlign = 'no' } else { $s.DriverAlign = 'yes' } }
      default { Write-Host 'Type a number from 1 to 7, or press Enter to launch.' -ForegroundColor Yellow }
    }
  }
  $s.Keys | ForEach-Object { $_ + '=' + $s[$_] } | Set-Content -LiteralPath $file
  # World launches have no mission header: enable the in-game setup explicitly.
  $extra = ' -acrSetup 1'
  # This launcher starts a separate local server. Its owner connects first.
  # Use the existing opt-in authority rule, without changing dedicated-server defaults.
  if (!$Solo) { $extra += ' -acrFirstPlayerHost 1' }
  # Dev copies only (a git checkout, never a release): chat "/login 1" makes you admin.
  if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git')) { $extra += ' -acrAdminPassword 1' }
  # Off-by-default BBB features awaiting live acceptance (the AI runs where these args go).
  if ($s.FireManeuver -eq 'yes') { $extra += ' -bbbFireManeuver 1' }
  if ($s.CombatManeuver -eq 'yes') { $extra += ' -bbbCombatManeuver 1' }
  if ($s.CrewTurnout -eq 'yes') { $extra += ' -bbbCrewTurnout 1' }
  if ($s.DriverAlign -eq 'yes') { $extra += ' -bbbDriverAlign 1' }
  return @{ Args = $extra; World = $s.World; Addons = (Get-ACRAddonList $s.Factions $s.ACE); Factions = $s.Factions; ACE = $s.ACE }
}

function Show-JoinModSetup {
  $file = Join-Path $PSScriptRoot 'join-settings.txt'
  $pair = 'vanilla'; $ace = 'no'
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Factions') { $pair = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'ACE') { $ace = $kv[1] }
    }
  }
  while ($true) {
    Write-Host ''
    Write-Host '=== JOIN MODS ===' -ForegroundColor Cyan
    Write-Host (' 1  Factions    ' + (Get-PairLabel $pair) + '   (must match the host)')
    Write-Host (' 2  ACE         ' + $ace + '   (must match the host)')
    $choice = (Read-Host 'Choice (Enter to continue)').Trim()
    if ($choice -eq '1') {
      $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua'); $i = [array]::IndexOf($pairs, $pair); $pair = $pairs[($i + 1) % $pairs.Count]
    } elseif ($choice -eq '2') { if ($ace -eq 'yes') { $ace = 'no' } else { $ace = 'yes' } }
    else { break }
  }
  @('Factions=' + $pair, 'ACE=' + $ace) | Set-Content -LiteralPath $file
  return @{ Addons = (Get-ACRAddonList $pair $ace); Factions = $pair; ACE = $ace }
}
