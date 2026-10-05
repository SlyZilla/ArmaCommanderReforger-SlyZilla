# Session settings menu (host options), shared by the multiplayer host and singleplayer:
# chosen before the game or server starts, remembered in host-settings.txt (singleplayer:
# play-settings.txt). Returns the extra -acr... arguments and the world. In singleplayer the
# game itself is the server, so the same options apply.
function Show-HostSetup([switch]$Solo) {
  $name = 'host-settings.txt'
  if ($Solo) { $name = 'play-settings.txt' }
  $file = Join-Path $PSScriptRoot $name
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Players = 'any'; StartRP = '100'; Income = '1'; AI = 'both'; Skill = 'Veteran';
                   Reaction = '1.6'; SoldierCap = '100'; UnitCap = '8'; Respawn = '15'; ShareGM = 'no'; SharedCommand = 'yes'; Threat = 'normal'; Skirmish = 'full' }
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
    if ($Solo) { Write-Host ('  2  Your side               ' + $s.Players + '   (any = pick in game; or US / USSR)') }
    else { Write-Host ('  2  Players'' side           ' + $s.Players + '   (any = each player picks; or US / USSR)') }
    Write-Host ('  3  AI commanders           ' + $s.AI + '   (both / none / one faction key, e.g. USSR)')
    Write-Host ('  4  Starting RP             ' + $s.StartRP)
    Write-Host ('  5  Income                  x' + $s.Income)
    Write-Host ('  6  AI skill                ' + $s.Skill + '   (Rookie / Regular / Veteran / Expert)')
    Write-Host ('  7  AI reaction speed       ' + $s.Reaction + '   (1 = game default, higher = faster spotting)')
    Write-Host ('  8  Soldier limit per side  ' + $s.SoldierCap)
    Write-Host ('  9  AI squad limit          ' + $s.UnitCap)
    Write-Host (' 10  Commander respawn       ' + $s.Respawn + ' s')
    if (!$Solo) {
      Write-Host (' 11  Game Master for all     ' + $s.ShareGM)
      Write-Host (' 12  Shared command          ' + $s.SharedCommand + '   (yes = several commanders per side)')
    }
    Write-Host (' 13  Threat level            ' + $s.Threat + '   (low = small infantry war, high = all-out war)')
    $skirmishes = Get-SkirmishChoices $s.World
    if ($skirmishes -notcontains $s.Skirmish) { $s.Skirmish = 'full' }
    if ($skirmishes.Count -gt 1) { Write-Host (' 14  Skirmish size           ' + $s.Skirmish + '   (full map, half of it, or a small flashpoint)') }
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
      '11' { if ($Solo) { Write-Host 'Not used in singleplayer.' -ForegroundColor Yellow } elseif ($s.ShareGM -eq 'yes') { $s.ShareGM = 'no' } else { $s.ShareGM = 'yes' } }
      '12' { if ($Solo) { Write-Host 'Not used in singleplayer.' -ForegroundColor Yellow } elseif ($s.SharedCommand -eq 'yes') { $s.SharedCommand = 'no' } else { $s.SharedCommand = 'yes' } }
      '13' { $levels = @('low', 'normal', 'high'); $i = [array]::IndexOf($levels, $s.Threat); $s.Threat = $levels[($i + 1) % $levels.Count] }
      '14' { if ($skirmishes.Count -gt 1) { $i = [array]::IndexOf($skirmishes, $s.Skirmish); $s.Skirmish = $skirmishes[($i + 1) % $skirmishes.Count] } else { Write-Host 'This map has only the full size.' -ForegroundColor Yellow } }
      default { Write-Host 'Type a number from the list, or press Enter to start.' -ForegroundColor Yellow }
    }
  }
  $s.Keys | ForEach-Object { $_ + '=' + $s[$_] } | Set-Content -LiteralPath $file
  $skill = $skills[$s.Skill]
  if (!$skill) { $skill = 70 }
  $extra = ' -acrStartRP ' + $s.StartRP + ' -acrIncome ' + $s.Income + ' -acrAI ' + $s.AI + ' -acrSkill ' + $skill +
           ' -acrReaction ' + $s.Reaction + ' -acrSoldierCap ' + $s.SoldierCap + ' -acrUnitCap ' + $s.UnitCap + ' -acrRespawnSeconds ' + $s.Respawn + ' -acrPlayers ' + $s.Players
  if ($s.ShareGM -eq 'yes') { $extra += ' -acrGameMasterAll 1' }
  $extra += ' -acrThreat ' + $s.Threat
  # Skirmish: -acrSize half|flashpoint, plus -acrLayout <name> (spaces as _) for a chosen one.
  if ($s.Skirmish -ne 'full' -and (Get-SkirmishChoices $s.World) -contains $s.Skirmish) {
    $parts = $s.Skirmish -split ': ', 2
    $size = ($parts[0] -split ' ')[0]
    $extra += ' -acrSize ' + $size
    if ($parts.Count -eq 2) { $extra += ' -acrLayout ' + ($parts[1] -replace '[^A-Za-z0-9-]', '_') }
  }
  if ($s.SharedCommand -eq 'no') { $extra += ' -acrSharedCommand 0' } else { $extra += ' -acrSharedCommand 1' }
  return @{ Args = $extra; World = $s.World }
}
