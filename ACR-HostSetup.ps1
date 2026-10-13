# Session settings menu (host options), shared by the multiplayer host and singleplayer:
# chosen before the game or server starts, remembered in host-settings.txt (singleplayer:
# play-settings.txt). Returns the extra -acr... arguments and the world. In singleplayer the
# game itself is the server, so the same options apply.
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
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Factions = 'vanilla'; ACE = 'no'; Players = 'any'; StartRP = '100'; Income = '1'; AI = 'both'; Skill = 'Veteran';
                   Reaction = '1.6'; SoldierCap = '100'; UnitCap = '8'; Respawn = '15'; ShareGM = 'no'; FirstPlayerHost = 'no'; SharedCommand = 'yes'; Threat = 'normal'; Skirmish = 'full' }
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
  # Skirmish sizes: the whole map, or a layout (ACR_Skirmish) placed in the world's layer.
  # Choices: full, 'half' / 'flashpoint' (one at random), or one layout by name.
  function Get-SkirmishChoices([string]$world) {
    $choices = @('full')
    $layer = Join-Path $PSScriptRoot ($world -replace '\.ent$', '_Layers/default.layer')
    if (!(Test-Path -LiteralPath $layer)) { return ,$choices }
    $text = [IO.File]::ReadAllText($layer)
    $halves = @(); $points = @()
    foreach ($m in [regex]::Matches($text, '(?s)ACR_Skirmish \w+ \{(.*?)\r?\n\}')) {
      $body = $m.Groups[1].Value
      $n = [regex]::Match($body, 'm_sName "([^"]+)"').Groups[1].Value
      $size = [regex]::Match($body, 'm_eSize (\d)').Groups[1].Value
      if (!$n) { continue }
      if ($size -eq '0') { $halves += ('half: ' + $n) } else { $points += ('flashpoint: ' + $n) }
    }
    if ($halves.Count -gt 0) { $choices += 'half (random)'; $choices += $halves }
    if ($points.Count -gt 0) { $choices += 'flashpoint (random)'; $choices += $points }
    return ,$choices
  }
  while ($true) {
    $mapName = [IO.Path]::GetFileNameWithoutExtension($s.World) -replace '^ACR_', ''
    Write-Host ''
    $title = '=== HOST SETUP ==='
    if ($Solo) { $title = '=== GAME SETUP ===' }
    Write-Host ($title + '  (type a number to change, Enter to start)') -ForegroundColor Cyan
    Write-Host ('  1  Map / scenario          ' + $mapName)
    Write-Host ('  2  Factions                ' + (Get-PairLabel $s.Factions) + '   (vanilla / RU + US / RU + UA / US + UA)')
    if ($Solo) { Write-Host ('  3  Your side               ' + $s.Players + '   (any = pick in game)') }
    else { Write-Host ('  3  Players'' side           ' + $s.Players + '   (any = each player picks)') }
    Write-Host ('  4  AI commanders           ' + $s.AI + '   (both / none / one faction key)')
    Write-Host ('  5  Starting RP             ' + $s.StartRP)
    Write-Host ('  6  Income                  x' + $s.Income)
    Write-Host ('  7  AI skill                ' + $s.Skill + '   (Rookie / Regular / Veteran / Expert)')
    Write-Host ('  8  AI reaction speed       ' + $s.Reaction + '   (1 = game default, higher = faster spotting)')
    Write-Host ('  9  Soldier limit per side  ' + $s.SoldierCap)
    Write-Host (' 10  AI squad limit          ' + $s.UnitCap)
    Write-Host (' 11  Commander respawn       ' + $s.Respawn + ' s')
    if (!$Solo) {
      Write-Host (' 12  Game Master for all     ' + $s.ShareGM)
      Write-Host (' 13  Shared command          ' + $s.SharedCommand + '   (yes = several commanders per side)')
    }
    Write-Host (' 14  Threat level            ' + $s.Threat + '   (low = small infantry war, high = all-out war)')
    $skirmishes = Get-SkirmishChoices $s.World
    if ($skirmishes -notcontains $s.Skirmish) { $s.Skirmish = 'full' }
    if ($skirmishes.Count -gt 1) { Write-Host (' 15  Skirmish size           ' + $s.Skirmish + '   (full map, half of it, or a small flashpoint)') }
    if (!$Solo) { Write-Host (' 16  First player is host    ' + $s.FirstPlayerHost + '   (trusted fallback on dedicated servers; no transfer after disconnect)') }
    Write-Host (' 17  ACE All in One          ' + $s.ACE + ' (optional, independent of factions)')
    $choice = (Read-Host 'Choice').Trim()
    if ($choice -eq '') { break }
    switch ($choice) {
      '1' { if ($worlds.Count -gt 1) { $i = [array]::IndexOf($worlds, $s.World); $s.World = $worlds[($i + 1) % $worlds.Count] } else { Write-Host 'Only one map is available so far.' -ForegroundColor Yellow } }
      '2' {
        $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua')
        $i = [array]::IndexOf($pairs, $s.Factions)
        $s.Factions = $pairs[($i + 1) % $pairs.Count]
        $valid = @('any') + (Get-PairFactionKeys $s.Factions)
        if ($valid -notcontains $s.Players) { $s.Players = 'any' }
        $s.AI = 'both'
      }
      '3' {
        # any or either selected faction. Picking a side gives the AI the other one.
        $sides = @('any') + (Get-PairFactionKeys $s.Factions)
        $i = [array]::IndexOf($sides, $s.Players)
        $s.Players = $sides[($i + 1) % $sides.Count]
        if ($s.Players -eq $sides[1]) { $s.AI = $sides[2] } elseif ($s.Players -eq $sides[2]) { $s.AI = $sides[1] } else { $s.AI = 'both' }
        Write-Host ('AI commanders set to ' + $s.AI + ' (change with 3).') -ForegroundColor DarkGray
      }
      '4' { $v = Read-Host 'AI commanders: both, none, or one selected faction key'; if ($v.Trim()) { $s.AI = $v.Trim() } }
      '5' { $v = Read-Host 'Starting RP (e.g. 100)'; if ($v -match '^\d+$') { $s.StartRP = $v } }
      '6' { $v = Read-Host 'Income multiplier (e.g. 0.5, 1, 2)'; if ($v -match '^\d+(\.\d+)?$') { $s.Income = $v } }
      '7' { $v = Read-Host 'AI skill: Rookie, Regular, Veteran or Expert'; foreach ($k in $skills.Keys) { if ($k -like ($v.Trim() + '*') -and $v.Trim()) { $s.Skill = $k } } }
      '8' { $v = Read-Host 'AI reaction speed (0.5 - 3)'; if ($v -match '^\d+(\.\d+)?$') { $s.Reaction = $v } }
      '9' { $v = Read-Host 'Soldier limit per side (e.g. 100)'; if ($v -match '^\d+$') { $s.SoldierCap = $v } }
      '10' { $v = Read-Host 'AI squad limit (e.g. 8)'; if ($v -match '^\d+$') { $s.UnitCap = $v } }
      '11' { $v = Read-Host 'Commander respawn delay in seconds'; if ($v -match '^\d+$') { $s.Respawn = $v } }
      '12' { if ($Solo) { Write-Host 'Not used in singleplayer.' -ForegroundColor Yellow } elseif ($s.ShareGM -eq 'yes') { $s.ShareGM = 'no' } else { $s.ShareGM = 'yes' } }
      '13' { if ($Solo) { Write-Host 'Not used in singleplayer.' -ForegroundColor Yellow } elseif ($s.SharedCommand -eq 'yes') { $s.SharedCommand = 'no' } else { $s.SharedCommand = 'yes' } }
      '14' { $levels = @('low', 'normal', 'high'); $i = [array]::IndexOf($levels, $s.Threat); $s.Threat = $levels[($i + 1) % $levels.Count] }
      '15' { if ($skirmishes.Count -gt 1) { $i = [array]::IndexOf($skirmishes, $s.Skirmish); $s.Skirmish = $skirmishes[($i + 1) % $skirmishes.Count] } else { Write-Host 'This map has only the full size.' -ForegroundColor Yellow } }
      '16' { if ($Solo) { Write-Host 'Not used in singleplayer.' -ForegroundColor Yellow } elseif ($s.FirstPlayerHost -eq 'yes') { $s.FirstPlayerHost = 'no' } else { $s.FirstPlayerHost = 'yes' } }
      '17' { if ($s.ACE -eq 'yes') { $s.ACE = 'no' } else { $s.ACE = 'yes' } }
      default { Write-Host 'Type a number from the list, or press Enter to start.' -ForegroundColor Yellow }
    }
  }
  $s.Keys | ForEach-Object { $_ + '=' + $s[$_] } | Set-Content -LiteralPath $file
  $skill = $skills[$s.Skill]
  if (!$skill) { $skill = 70 }
  $extra = ' -acrStartRP ' + $s.StartRP + ' -acrIncome ' + $s.Income + ' -acrAI ' + $s.AI + ' -acrSkill ' + $skill +
           ' -acrReaction ' + $s.Reaction + ' -acrSoldierCap ' + $s.SoldierCap + ' -acrUnitCap ' + $s.UnitCap + ' -acrRespawnSeconds ' + $s.Respawn + ' -acrPlayers ' + $s.Players
  if ($s.ShareGM -eq 'yes') { $extra += ' -acrGameMasterAll 1' }
  if ($s.FirstPlayerHost -eq 'yes') { $extra += ' -acrFirstPlayerHost 1' }
  $extra += ' -acrThreat ' + $s.Threat
  # Skirmish: -acrSize half|flashpoint, plus -acrLayout <name> (spaces as _) for a chosen one.
  if ($s.Skirmish -ne 'full' -and (Get-SkirmishChoices $s.World) -contains $s.Skirmish) {
    $parts = $s.Skirmish -split ': ', 2
    $size = ($parts[0] -split ' ')[0]
    $extra += ' -acrSize ' + $size
    if ($parts.Count -eq 2) { $extra += ' -acrLayout ' + ($parts[1] -replace '[^A-Za-z0-9-]', '_') }
  }
  if ($s.SharedCommand -eq 'no') { $extra += ' -acrSharedCommand 0' } else { $extra += ' -acrSharedCommand 1' }
  $extra += ' -acrFactions ' + $s.Factions
  # The launcher already asked: skip the in-game setup screen (ACR_Setup).
  $extra += ' -acrSetup 0'
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
