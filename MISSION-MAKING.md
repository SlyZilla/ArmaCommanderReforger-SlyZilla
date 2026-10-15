# Make an AC: Actual map

This guide shows how to set up AC: Actual on a different map, test your changes, and play a custom
map with a friend. It also lists the settings you can change in Workbench.

> AC: Actual is an alpha. Arland is the main automated test map; Everon is the main large-map
> playtest terrain. Kolguyev uses generated layouts. New maps, faction combinations and multiplayer
> setup paths still need playtesting. Mod log lines use `[ACR]` (the included AI systems also use `[BBB]`).

## Contents

1. [Before you start](#before-you-start)
2. [Quick start: copy Arland](#quick-start-copy-arland)
3. [What a world needs](#what-a-world-needs)
4. [Entity reference](#entity-reference)
5. [Mission header](#mission-header)
6. [Save, export and run](#save-export-and-run)
7. [Host options](#host-options)
8. [Designing a good map](#designing-a-good-map)
9. [Current limits](#current-limits)
10. [Testing your scenario](#testing-your-scenario)
11. [Play a custom map with a friend](#play-a-custom-map-with-a-friend)

## Before you start

- **Arma Reforger Tools** (Workbench) from Steam. Workbench is the editor used to make and test maps.
- **The AC: Actual addon.** Use the installed Workshop project if available, or get the
  `ArmaCommanderReforger` folder from the [release ZIP](https://github.com/propst-design/ACActual/releases/latest)
  and add its `addon.gproj` to Workbench with **Add Existing Project**. Avoid loading two copies with
  the same addon ID. Keep custom work out of an installation that self-updates.
- **Your own addon** (your own mod project). Set AC: Actual, GUID `AC230926A0010001`, as a dependency so
  your map can use AC: Actual's game mode and settings. Keep your work in your own addon so an AC: Actual
  update does not replace it.
- **Licence:** AC: Actual is APL-SA (see [LICENSE.md](LICENSE.md)). Scenarios that include or modify its
  files must stay APL-SA and credit the original authors.

In this guide, a **world** is the map and the objects placed on it. An **addon** is the mod folder that
contains and shares your work. A **mission header** gives your world a name in the game's scenario list.
**RP** means requisition points: the money sides spend on units and support.

## Quick start: copy Arland

For a hand-placed layout, copy the working Arland setup and move its objects:

1. In your addon, create a world using the terrain you want (for example, Everon).
2. In AC: Actual's `Worlds/ACR_Arland.ent`, copy everything from the `default` layer and paste it into
   your new world's `default` layer. These objects set up the game mode, bases, and units.
3. Move the spawn point, bases, faction settings, and starting units to suit your map.
4. Set the AI world to use this terrain's navigation files. These files tell soldiers and vehicles where
   they can move; see [AI world and navmesh](#ai-world-and-navmesh).
5. Create a mission header so the game can show your map by name. See [Mission header](#mission-header).
6. Play it from Workbench and check that bases, units, and the command panel appear. See
   [Testing your scenario](#testing-your-scenario).

For a generated layout, use `Worlds/ACR_Kolguyev.ent` and its layer as the example instead: copy the
managers and game mode to a world on your terrain, configure its navmesh and factions, and provide a
player spawn point for each side. Leave out `ACR_Base` objects to generate bases automatically. The
terrain needs usable named map places and enough dry land to form a plan; generation is not
guaranteed on every terrain. See [Generated layouts](#generated-layouts).

To play a custom map with a friend, see [Play a custom map with a
friend](#play-a-custom-map-with-a-friend).

## What a world needs

Put these objects in the world's `default` layer. A **prefab** is a ready-made object you choose from
Workbench's browser. Paths starting with `ACR_` are part of AC: Actual; the others come with the game.

| Entity | Prefab / class | Needed? | Why |
|---|---|---|---|
| Game mode | `Prefabs/MP/Modes/Plain/GameMode_Plain.et` | Yes | Lets the game handle spawning, the map, and construction components (below) |
| Faction manager | e.g. `Prefabs/MP/Managers/Factions/FactionManager_USxUSSR.et` | Yes | Sets the sides in the match. Their faction keys must match the AC: Actual settings |
| Loadout manager | e.g. `Prefabs/MP/Managers/Loadouts/LoadoutManager_USxUSSR.et` | Yes | Sets what players spawn with (AC: Actual uses officer gear; see [Current limits](#current-limits)) |
| Player spawn point | e.g. `Prefabs/MP/Spawning/SpawnPoint_US.et` | Yes, one per playable side | Shows where that side's HQ is. Command units respawn at the nearest base it owns |
| AI world | `Prefabs/AI/SCR_AIWorld.et` with the map's navigation files | Yes | Gives AI soldiers and vehicles routes they can use |
| Perception manager | `Prefabs/World/Game/PerceptionManager.et` | Yes | Lets AI notice other units |
| Radio manager | `Prefabs/Systems/Radio/RadioManager.et` | Yes | Radio system |
| Chat entity | `Prefabs/MP/Campaign/CampaignMPChatEntity.et` | Yes for chat | Copy `ACR_Chat` from a shipped world; the plain game mode does not supply chat |
| **`ACR_Mode`** | class `ACR_Mode` | Yes, exactly one | Starts the mode |
| **`ACR_FactionSettings`** | class `ACR_FactionSettings` | Recommended, one per side | RP, buy list, AI commander, supports |
| **`ACR_Base`** | class `ACR_Base` | Hand-built layouts only | Capture/deployment points. With none, the mode generates a layout |
| **`ACR_StartingUnit`** | class `ACR_StartingUnit` | Optional | Units that exist when the match starts |

### Settings on the game mode

Add these components to the game mode object. The Arland map has working examples you can copy:

- **`SCR_RespawnSystemComponent`** with an `SCR_AutoSpawnLogic`. Set its forced faction to the side that
  has a spawn point if only one side does. Players then spawn straight into their command unit.
- **`SCR_MapConfigComponent`** (optional): gadget map config
  `{AC230926A0010098}Configs/Map/ACR_MapFullscreen.conf` gives the command map finer contour lines.
- **`SCR_CampaignBuildingManagerComponent`** (optional): lets construction trucks build. Without it,
  construction trucks are just trucks.
  Some structures players build this way do something in AC: Actual (anywhere on the map, owned by the
  builder's side, until dismantled or destroyed):
  - **Field Hospital**: soldiers of its side within 25 m are healed after about 20 s there.
  - **Vehicle Maintenance**, **Fuel Storage**: vehicles within 25 m are slowly repaired / refuelled.
  - **Ammo Storage**: soldiers within 25 m get magazines and launchers back; vehicle guns are refilled.
  - **Helipad**: a pad for support helicopters and where bought helicopters appear, for the side's nearest base.
  - **Headquarters / Player Hub**: away from existing bases it becomes a forward base ("FOB 1"...) where
    units can be bought and reinforcements deploy; hidden from the enemy until found, no income.
  AI squads that need ammo, repairs, fuel or healing also head for these. Service is slow and
  free; the Reinforce button's paid instant vehicle service remains separate. Building costs
  draw from construction-truck stock. A built command post does not yet provide a construction
  supply pool, so further building still needs a stocked truck. The AI commander does not build
  bases or helipads.

### AI world and navmesh

On the `SCR_AIWorld` object, choose the navigation files for your terrain: one for soldiers (`NavmeshWorld`),
one for vehicles (`ChimeraNavmeshWorld`), and the low-detail one if available. A navigation file is the
map's movement guide for AI. Arland and Everon use the Game Master files (`worlds/GameMaster/Navmeshes/GM_Arland*.nmn`
and `GM_Eden*.nmn`). Find the matching files in the game's data, or make them in Workbench for a custom
terrain. AC: Actual finds roads and checks for land on its own.

## Entity reference

The tables below explain the settings for each AC: Actual object. Workbench usually hides the internal
prefix: for example, `m_sName` appears as **Name**. If you leave a setting empty, AC: Actual uses the
default shown here.

### ACR_Mode

One per world. Place it anywhere.

| Setting | Default | What it does |
|---|---|---|
| `m_bHostGameMaster` | on | The host and server admins also get Game Master |
| `m_bSharedCommand` | on | Every player of a side may command all its squads. Off: one commander per side |
| `m_iAISkill` | 70 (Veteran) | Skill of all AI soldiers in the mode |
| `m_fAIPerception` | 1.6 | How fast AI spot and identify targets (game default 1) |
| `m_iRespawnSeconds` | 15 | Delay before a killed command unit respawns |

### ACR_FactionSettings

Add one for each side and place it anywhere (Arland puts each beside its HQ to keep the layer tidy).
If you leave one out, that side still gets 50 RP, the automatic buy list, and all support calls, but its
AI commander is turned off.

| Setting | Default | What it does |
|---|---|---|
| `m_sFaction` | `US` | Which side these settings are for. It must match a side in the faction manager, such as `US` or `USSR` |
| `m_iStartingRP` | 100 | Requisition points at the start |
| `m_bAICommander` | on | An AI commander leads this side whenever no player is in command |
| `m_iAIUnitCap` | 8 | Maximum number of squads the AI commander keeps in the field |
| `m_iAIAttacks` | 1 | How many base attacks the AI commander can run at once |
| `m_ePricing` | `STEPPED` | How the buy list limits purchases: `STEPPED` or `FIXED_STOCK` (see Prices below) |
| `m_bAutoFill` | on | Add the faction's eligible Game Master catalog units to the buy list at automatic prices |
| `m_aRequisitions` | empty | Units you add to the buy list yourself. An entry replaces the matching automatic item |
| `m_aExclude` | empty | Groups or vehicles to leave out of the automatic buy list |
| `m_aGarrisonSoldiers` | empty | Soldiers used to guard bases. Empty uses the faction's riflemen, machine gunners, grenadiers, and AT soldiers |
| `m_aPrefabMappings` | empty | Replace units, crews, shells, rockets, or helipad models with chosen prefabs. If no match is found, keep the original and log a fallback |
| `m_bSupports` | on | Let the commander call support |
| `m_aSupports` | empty | Support calls shown to the commander. Empty offers all calls at their default prices |
| `m_aSupportUnits` | empty | Settings for support units that can be bought. No row keeps the current choice; a row can change or hide that unit |
| `m_sSupportHelicopter` | empty | Legacy gunship airframe fallback; a `GUNSHIP` support-unit row takes precedence |
| `m_sSupportTransportHelicopter` | empty | Legacy door-gunner airframe fallback; a `DOOR_GUN_HELI` support-unit row takes precedence |
| `m_sSupportRocket` | empty | Rocket for rocket strikes and gunships. Empty = Hydra 70 / S-5 |

AC: Actual uses faction keys to tell sides apart. For default support assets, `RHS_AFRF` and keys
starting with `USSR` count as eastern; otherwise keys containing `US`, or exactly `NATO` or
`BLUFOR`, count as western. Other keys use eastern defaults unless prefab mappings replace them.

#### Factions and mods

The external ZIP/development launcher menu offers four side pairings: the game's US vs USSR, RHS US
vs RHS AFRF, UA vs RHS AFRF, or RHS US vs UA. The launcher selects the RHS/UA addon IDs but does not
download them; its current search path for optional faction mods is `E:\ArmaReforgerMods\addons`,
which may need adapting on another PC. The UA mod replaces the game's US side for that session. ACE
All in One is a separate optional choice. Everyone joining must use the same side pairing and ACE
choice as the host. With no extra faction mods selected, the game uses its usual US vs USSR sides.

**Any faction your world lists.** Besides those pairs, the in-game setup screen (Blue side / Red
side) and `-acrFactions WEST,EAST` can pick any two eligible factions in your world's faction
manager whose Game Master catalogs have soldiers and groups. The in-game screen only selects among
already-loaded mods; it does not load or download faction mods. The world's US-side bases, units and
spawns go to the first, USSR-side to the second. To offer a faction mod, add its faction config to
the `Factions` list of the world's `SCR_FactionManager` (an entry whose mod isn't loaded is simply
absent in that session):

```
  SCR_Faction "{<a new GUID>}" : "{<the mod's faction config GUID>}Configs/Factions/<Faction>.conf" {
  }
```

A faction without its own `ACR_FactionSettings` entity copies the settings of the side it replaces
(RP, AI commander, caps, prices, supports) and builds its buy list and garrisons from its catalog; its
command unit is the catalog's officer, else its squad leader. This is not hand-fitting: an untuned faction
can look mixed (several uniforms), and anything it lacks falls back to vanilla units. Only RHS and Ukraine
Armed Forces RHS are hand-fitted. Requests for proper support of a faction mod: https://github.com/propst-design/ACActual/issues

You can use `m_aPrefabMappings` to replace a unit with one from another mod. Each row is an
`ACR_PrefabMapping` with two resource pickers:

| Setting | Default | What it does |
|---|---|---|
| `m_sFrom` | empty | Exact original prefab to replace |
| `m_sTo` | empty | Replacement prefab |

Choose both from the prefab browser. AC: Actual also tries to match starting units by their catalog
name or role. If it cannot find a replacement, it uses the original unit and writes a `[ACR] FACTION
fallback` line in the server log.

**Automatic buy list.** AC: Actual adds each enabled infantry group and vehicle from the faction's Game
Master catalog. It leaves out guards, fixed weapons, and planes. Prices: infantry 3 RP per soldier (at least 6); vehicles
12 (car), 15 (truck), 45 (APC) or 60 (helicopter), +10 armed, +15 armoured, +6 crew. Cheaper units have more
stock. The AI commander never buys aircraft (it can't fly them) and buys unarmed trucks only to ferry squads.

**Prices (`m_ePricing`).** `STEPPED` (default): infantry, cars and trucks can be bought without limit, but
each purchase makes that unit cost more for that side: +15% of the base price per purchase at normal
threat, +25% at low, +8% at high. Rare units (aircraft, APCs and armour, mortar teams, support helicopters)
keep their `m_iStock` at a fixed price, untouched by threat. `FIXED_STOCK`: the older system, every unit has
its `m_iStock` per match (scaled by threat level) at a fixed price. The host can override it with
`-acrPricing`.

#### Requisition entry (`m_aRequisitions`)

Click **+** to add an item. Choose its infantry group and vehicle (if any) from the prefab browser.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name in the buy list and on the map; numbered per purchase |
| `m_sGroup` | | Infantry group to spawn |
| `m_sVehicle` | | Vehicle to spawn (optional) |
| `m_bCrewed` | on | Spawn the vehicle with its driver and gunners |
| `m_sPassengers` | | Group that rides in the vehicle (optional) |
| `m_iCost` | 20 | Cost in RP |
| `m_iStock` | 2 | How many can be bought per match (fixed-stock pricing, and rare units under stepped pricing) |

For vehicles in the faction's Game Master catalog, AC: Actual fills in whether they are aircraft,
transports, armed, or rare. A vehicle outside that catalog is treated as an armed ground vehicle.

#### Support entry (`m_aSupports`)

Each entry is one action in the commander's support menu, such as a mortar strike or helicopter run.

| Setting | Default | What it does |
|---|---|---|
| `m_eType` | | Kind of support (table below) |
| `m_bEnabled` | on | Offer this call-in; uncheck to omit it from this faction's support menu |
| `m_sName` | empty = default | Name in the command panel |
| `m_iCost` | -1 = default | Cost in RP |
| `m_iCooldown` | -1 = default | Seconds before it can be called again |
| `m_iAICost` | -1 = half the cost | `AIR_LIFT` only: RP when the AI commander calls it |
| `m_eBombRunDelivery` | `VIRTUAL` | Bomb-run delivery: existing virtual blasts, a flyover prefab plus virtual blasts, or an editable call-in prefab |
| `m_sBombRunPrefab` | empty | Aircraft/flyover prefab (`FLYOVER`) or call-in spawner prefab (`EDITABLE_CALL_IN`) |
| `m_fBombRunAltitude` | 300 m | Flyover height above the target, and never under 150 m over a hill on the way; clamped to 30-1,000 m |
| `m_fBombRunSpeed` | 180 m/s | Flyover speed; sets bomb spacing in time in `FLYOVER` mode; clamped to 20-400 m/s |
| `m_iBombRunBombs` | 4 | Virtual bombs in `VIRTUAL` or `FLYOVER` mode; clamped to 1-12 |
| `m_fBombRunSpacing` | 28 m | Distance between impact points in `FLYOVER` mode; clamped to 1-500 m |
| `m_fBombRunScatter` | 5 m | Random lateral impact scatter in metres; clamped to 0-100 m |

| Type | Default name | RP | Cooldown (s) |
|---|---|---|---|
| `MORTAR_HE_SMALL` | Mortars: HE (small) | 10 | 90 |
| `MORTAR_HE_LARGE` | Mortars: HE (large) | 25 | 180 |
| `MORTAR_CREEPING` | Mortars: creeping barrage | 25 | 180 |
| `MORTAR_SMOKE` | Mortars: smoke screen | 6 | 60 |
| `ILLUMINATION` | Illumination flares | 4 | 45 |
| `ROCKET_STRIKE` | Air: rocket strike | 25 | 180 |
| `BOMB_RUN` | Air: bomb run | 35 | 240 |
| `HELI_GUN_RUN` | Helicopter: rocket run | 30 | 180 |
| `HELI_LOITER` | Helicopter: gunship (90 s) | 45 | 300 |
| `HELI_DOOR_GUNS` | Helicopter: door gunners (120 s) | 20 | 150 |
| `ARTILLERY_BATTERY` | Artillery: howitzer battery | 60 | 300 |
| `AIR_LIFT` | Helicopter: air lift | 40 (AI commander 20) | 240 |

Set the bomb-run style in each side's support row. `VIRTUAL` uses AC: Actual's existing explosions and
does not show a plane. `FLYOVER` sends the chosen aircraft or model over the target, then uses AC: Actual's
explosions. `EDITABLE_CALL_IN` starts the chosen prefab at the target and lets that prefab handle its own
attack. This can work with mod call-ins, such as RHS CAS, without AC: Actual needing RHS-specific code.
Use `FLYOVER` for a model that only needs to fly across the map. Use `EDITABLE_CALL_IN` for a prefab that
already handles its own attack. Leave the prefab empty to keep the standard virtual bomb run.

#### Requisition support unit (`m_aSupportUnits`)

Click **+** to choose a support unit to offer for purchase. Pick its assets from the prefab browser.
Roles without a row keep their current settings. Use **Enabled** to show or hide the unit in the buy list,
and set its price and how many times it can be bought in a match. These are separate from the price and
wait time for calling in support above.

| Setting | What it does |
|---|---|
| `m_eRole` | `MORTAR_TEAM`, `OBSERVER_TEAM`, `GUNSHIP`, or `DOOR_GUN_HELI` |
| `m_bEnabled` | Offer this support unit for requisition; on by default |
| `m_sName` | Buy-list name (empty = current default) |
| `m_iCost` | Purchase cost in RP (-1 = current default) |
| `m_iStock` | Base purchases per match (-1 = current default; under fixed-stock pricing, threat scaling applies; stepped pricing keeps rare-unit stock fixed) |
| `m_sGroupPrefab` | Observer team's base group (observer role) |
| `m_sPrimaryPrefab` | Spotter character (observer) |
| `m_sSecondaryPrefab` | Radio operator character (observer) |
| `m_sVehiclePrefab` | Support helicopter airframe (gunship or door-gunner role) |
| `m_sCrewPrefab` | Crew character used for pilots and gunners, including replacement crew after servicing |

Observer characters use the gear saved on their character prefab. For a helicopter, the chosen crew
replaces the airframe's pilots and gunners when it is first parked at a base; it is also used if crew
are replaced during servicing. Empty airframe and crew fields keep the current faction choices. The older `m_sSupportHelicopter` and
`m_sSupportTransportHelicopter` values remain as airframe fallbacks when that role has no
`m_sVehiclePrefab`. Mortar-team prefab choices continue to use `m_aPrefabMappings`; its support-unit
row controls whether the team is offered and its name, cost and stock.

Mortar calls and `ILLUMINATION` need a **mortar team**. AC: Actual adds one to each side's buy list
(30 RP, up to 2 per match; M252 for western sides, 2B14 for other sides). The team sets up on its own
after stopping on clear, level ground, and packs the mortar before moving. It can only answer calls
within range (about 2.9 km for the M252 or 2.3 km for the 2B14, and at least 100 m away). If no team is
ready or the target is out of range, the call is declined without spending RP. If the mortar is destroyed,
use Reinforce (10 RP) to replace it.

Each buy list also has a "Forward observer team" (12 RP, up to 2 per match: a spotter and radio operator).
Turn on **Fire at will** in the command panel to let that team call a small mortar strike on enemies it
spots. The target must be at least 150 m away, with no friendly within 120 m. The call uses the normal
price and cooldown and can happen once every 45 seconds per side. Observer teams start with this option
on. AI commanders' observers and mortar teams always use it. Mortar teams also fire two free, scattered
rounds every 30 seconds at spotted enemies they can reach.

`AIR_LIFT` flies an infantry squad with no player-controlled member. It must be on foot and not a
vehicle crew. Capacity depends on the helicopter; one or two fly, and troops without seats stay
behind. Select the squad, pick the air lift, and right-click where it should land (at least 400 m
away). One transport helicopter (two if the squad needs more seats) comes from off the map, lands on
clear, level ground within 250 m of the squad, waits up to two minutes for it to board, and sets it
down on clear ground within 200 m of the clicked point; the squad then holds there and the
helicopters leave. No helipad is needed. The call is declined without a charge if there is no clear
landing spot at either end. While a squad is on an air lift it takes no orders until it has landed.
The AI commander pays half (`m_iAICost`), since it cannot fly bought helicopters. Support lists made
before the air lift existed get it added; add an `AIR_LIFT` row with **Enabled** off to leave it
out.

`ARTILLERY_BATTERY` needs no team and can hit anywhere on the map. It arrives 75-120 seconds after the
call: three groups of six heavy rounds, 10 seconds apart, landing within 120 m of the target. You can
hear each group coming in.

Support helicopters use **helipads** (see `ACR_Helipad` below). If the map has a helipad, each side can
buy up to four gunships (80 RP each) and four door-gunner helicopters (50 RP each). A helicopter waits
at a free pad with its crew. After a call, it starts up (about 20 seconds), flies its mission, returns to
the pad, and is repaired and rearmed (60 seconds). Buy another one if it is destroyed. If no ready
helicopter is parked at a pad at a base the side still owns, the call is declined without a charge. The
support list says whether one is ready, being serviced, or needs to be bought. Gunships handle rocket
runs and gunship calls; door-gunner helicopters handle door-gunner calls. The AI commander keeps one
gunship if a pad is free. Without a helipad, helicopters arrive from off the map and do not need to be
bought.

### ACR_Helipad

A landing pad for support helicopters. It belongs to the nearest base, or the base named in `m_sBase`.
The side that owns that base owns the pad. Leave flat, open ground around it: the Mi-8 needs about 11 m
of clear space on every side.

| Setting | Default | What it does |
|---|---|---|
| `m_sBase` | empty | Name of the base this pad belongs to, as shown on the map. Empty = the nearest base |
| `m_bModel` | on | Places a helipad model (US or USSR style, from the base's starting owner). Off when the world already has a helipad at this spot |

### ACR_Base

A base is a point the sides can capture. Its position is the centre of the capture circle.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | `Base N` | Name on the map |
| `m_sOwner` | empty | Starting owner faction key; empty = neutral |
| `m_iGarrison` | 10 | AI soldiers guarding it. They exist only while enemies are within 400 m, and are used up as they die. They start at the windows of the base's buildings within 80 m of its centre (the rest hold round the centre) |
| `m_iValue` | 1 | RP it adds to its owner's income (shown on the map as `Name [value]`) |
| `m_bIncome` | on | Produces income. Off = purely tactical point |
| `m_bHidden` | off | Hidden from other sides until one of their units enters it |
| `m_bAllowSpawn` | on | Bought units and reinforcements can deploy here (while owned and not contested) |
| `m_fRadius` | 50 | Capture radius in metres |

**Income:** every 60 seconds, each side gets 5 RP plus the value of each income base it owns.
**Capture:** soldiers inside the circle capture it for their side. More soldiers capture it faster.

### ACR_StartingUnit

A unit AC: Actual creates at this object's position 2 seconds after the match starts. Its vehicle appears
6 m away. The unit's prefab decides which side it belongs to.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name on the map, used as written (e.g. `1st Squad`) |
| `m_sGroup` | | Infantry group |
| `m_sVehicle` | | Vehicle (optional) |
| `m_bCrewed` | on | Vehicle comes with driver and gunners |
| `m_sPassengers` | | Group riding in the vehicle (optional) |

AC: Actual also finds other AI groups already on the map, or added later by Game Master, and makes them
commandable for their side. Use `ACR_StartingUnit` when you also want to give a group a name and vehicle.

### Generated layouts

AC: Actual can lay out a match from a terrain's named places when it has enough usable sites and dry
land. Choose "Generated" under Battlefield on the setup screen (or `-acrLayout generated`), with the
whole map, half of it or a flashpoint (`-acrSize`). A world without any `ACR_Base` always plays a
generated layout: a scenario for a new map then needs only the game mode, the managers, the faction
settings and one player spawn point per side (see `Worlds/ACR_Kolguyev` for a complete example).

The rules follow the hand-built Everon layout. Cities, towns, villages and settlements with houses
become 1 RP bases (capture radius 90, 75, 60 and 45 m); military sites 3 RP (60 m); harbours,
factories, power plants, sawmills, quarries and mines 2 RP (50 m); the airport 3 RP (80 m); and
about one landmark for every ten bases a hidden 1 RP base (40 m). Bases sit on their map place,
moved onto dry ground if it is in water; circles that would overlap shrink, down to 35 m. The HQs
prefer two widely separated military/airport sites, falling back to other suitable sites; a
flashpoint uses its farthest pair: 2 RP, a 6-man garrison, a 70 m circle where there is room. Other
generated bases start neutral with no garrison. Each side starts with the world's own
`ACR_StartingUnit` objects, or two rifle squads, a weapons team, a truck and a car, and its player
spawn point moves to its HQ. Hand-built helipads are not used: both sides get the off-map support
helicopters, and helicopters need a live helipad for purchase (players can build one with a
construction truck). The AI commander builds none, so it uses no bought helicopters there (it uses
the off-map air lift support call instead for long moves and detours). The setup screen's "Layout
seed" row rolls a new seed; the same seed, size and terrain data reproduce a plan within the same
version, but manual moves and later mod/terrain changes can alter it; the log lists the layout
(`[ACR] GEN BASE`). "Show and edit the layout on the map" opens the map with the planned bases:
click a base, then click where it should go. A spot is refused (with the reason) when it is off the
map, in water, too close to another base's capture circle, or would bring the two HQs within 1000 m
(300 m in a flashpoint). These are placement checks, not a guarantee of road access or good combat
terrain. Use "Return to match setup" to leave the map. "Undo base moves" puts all moves back;
changing the seed or size drops them. Moves apply to this match only; there is no saved-layout
export.

### ACR_Skirmish

This optional object defines a smaller match area on a large map: half the map, or a small fight
around about five bases. Add one object for each layout; its position does not matter. The host can
choose the layout by name (`-acrLayout`) or choose a size (`-acrSize`) and get a random layout of
that size. If no layout is chosen, the whole map is used. Everon has north and south halves, plus
small areas around Saint-Philippe, Montignac, and Levie. Put a flashpoint's HQs on bases inside it,
at opposite ends, so commanders start in the fight rather than at a far-off base. Everon's halves
have two helipads at each HQ; its flashpoints have none yet, so both sides there get the off-map
support helicopters. Add one or two `ACR_Helipad` objects for each HQ you list if you want both
sides to park support helicopters.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Layout name (the setup menu lists it; `-acrLayout` matches it ignoring spaces, `_` and `-`) |
| `m_eSize` | `FLASHPOINT` | `HALF` or `FLASHPOINT` |
| `m_sBases` | | Bases in play, by their `m_sName`, separated by commas. The HQs are added automatically |
| `m_sHQs` | | One HQ per side as `faction=base`, separated by semicolons, e.g. `US=Airbase; USSR=Power Plant` |

When the match starts with a layout, every base not in it is removed (off the map and out of play), the
HQs become their sides' owned bases (with at least the garrison of the side's HQ as placed) and other
bases that started owned turn neutral. Each side's starting units and player spawn point move to its new
HQ when it changed (a starting unit belongs to the side whose placed HQ is nearest; units appear at clear
spots in the new HQ's circle). Helipads of removed bases are out of play; a side whose layout leaves it no
helipad gets the off-map support helicopters instead of buying them for pads.

## Mission header

Create an **ACR_MissionHeader** in Workbench and choose your saved world. This supplies the scenario
metadata. The game's Scenarios/Host lists discover published Workshop scenarios; an unpacked addon
loaded with `-addonsDir` does not acquire a menu entry just by having a header. Workbench fills in
most of the details for you; this is what the saved file looks like:

```
ACR_MissionHeader {
 World "{<your world's GUID>}Worlds/<YourWorld>.ent"
 m_sName "AC: Actual - <Map>"
 m_sAuthor "<you>"
 m_sDescription "<one line>"
 m_sGameMode "Arma Commander Actual"
 m_iPlayerCount 8
 m_iAcrSetup 1
 m_sAcrSize "half"
}
```

Use the class `ACR_MissionHeader` (a plain `SCR_MissionHeader` still works, without the fields
below). Its extra fields are the scenario's default [host options](#host-options), so you can ship
several scenarios on one world (for example a full map and a flashpoint) that differ only in their
header. Leave a text field blank or a number at `-1` to keep the world's own setting. A host's
launch option beats the header; a dedicated server's config can also override header fields by name
under `gameProperties.missionHeader`. AC: Actual's custom fields have been tested through mission
headers, but their JSON override path still needs validation on an installed Workshop server. The
in-game setup screen, when enabled, overrides these defaults at Start.

| Field | Same as |
|---|---|
| `m_sAcrLayout` | `-acrLayout` |
| `m_sAcrSize` | `-acrSize` |
| `m_sAcrFactions` | `-acrFactions` |
| `m_sAcrPlayers` | `-acrPlayers` |
| `m_sAcrThreat` | `-acrThreat` |
| `m_sAcrPricing` | `-acrPricing` |
| `m_sAcrAI` | `-acrAI` |
| `m_iAcrUnitCap` | `-acrUnitCap` |
| `m_iAcrSeed` | `-acrSeed` |
| `m_iAcrSoldierCap` | `-acrSoldierCap` |
| `m_iAcrStartRP` | `-acrStartRP` |
| `m_fAcrIncome` | `-acrIncome` |
| `m_iAcrSkill` | `-acrSkill` |
| `m_fAcrReaction` | `-acrReaction` |
| `m_iAcrSharedCommand` | `-acrSharedCommand` (0 or 1) |
| `m_iAcrRespawnSeconds` | `-acrRespawnSeconds` |
| `m_iAcrGameMasterAll` | `-acrGameMasterAll` (0 or 1) |
| `m_iAcrMarksmen` | `-acrMarksmen` (0 or 1) |
| `m_iAcrSetup` | `-acrSetup` (0 or 1) |

The header describes the scenario; it does not publish, package or copy the world. Workbench creates
the `.meta` files beside the world and header. Keep them there and do not edit them. When you choose
the world in the header, Workbench fills in its resource ID and path for you. The world's ID is
different from the addon's ID shown in `addon.gproj`.

## Save, export and run

There is no separate **Export Mission** button. Workbench saves the map in your addon project, and you
can test it from there. Think of the addon as the folder you edit and eventually share.

Keep these matching resources together in the addon that owns the mission:

```text
Worlds/ACR_MyScenario.ent
Worlds/ACR_MyScenario.ent.meta
Worlds/ACR_MyScenario_Layers/default.layer
Missions/ACR_MyScenario.conf
Missions/ACR_MyScenario.conf.meta
```

Workbench makes the `.meta` files and resource IDs for you. If you copied a world, Workbench gives the
copy a new ID. Keep the world, its layer, its `.meta` file, and the mission header together in your addon.

### Run a map through the AC: Actual launcher

The included Play and Host launchers can start maps saved inside the AC: Actual project. They look for
world files named `Worlds/ACR_*.ent`, with each map's layer in
`Worlds/ACR_<Name>_Layers/default.layer`.

1. Save your map in the AC: Actual project as `Worlds/ACR_<Name>.ent`. Keep its layer at
   `Worlds/ACR_<Name>_Layers/default.layer`. The terrain can stay in a separate addon.
2. Put its header in `Missions/ACR_<Name>.conf`. Workbench fills in which world it opens.
3. To play alone, run `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"`. The setup menu opens.
   Select your map in option 1 (the menu restores remembered settings), choose the other settings
   and press Enter. `Play-ACA.cmd` opens the same menu,
   using the remembered selection (Arland on first use); you can switch to your map there.
4. To host a multiplayer game, run `Host-ACA.cmd` from the AC: Actual development project and select
   your map in the host menu. The launcher starts the server and then joins it on your PC. Your friend
   runs `Join-ACA.cmd` from a matching copy of that project and enters the address shown in your host
   window. You both need the same map files and the same faction-mod choices.

These launchers use the files in the AC: Actual project directly, so there is no Workshop upload or
package build for a local test. They start the selected world file directly; the mission header is
for the game's scenario list. To let a friend play, give them a matching copy of the development
project with your map included. Use that project copy, not the release zip: the release launcher
updates itself and can replace your custom map with the released files. The helper loads AC: Actual,
its selected faction/ACE mods, and a locally detected Workshop copy of All-in-One Command Menu. A
custom terrain or separate scenario addon is not added automatically.

### Run a map kept in your own addon

Keeping your map in your own addon is best for protecting your work when AC: Actual updates. Workbench
can test it there. You can also publish your addon so it appears in the game's **Scenarios** list. In
Workbench, publish the addon with AC: Actual listed as a dependency, then enable both addons in the game.
The [scenario setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Capture_%26_Hold_Setup)
shows how the header makes a scenario appear in that list.

Use the visibility and testing options offered by Workbench's publisher for your intended players;
check the [publishing
guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Publishing_Process). Do not assume an
uploader-only item is available to friends.

The supplied launcher does not load a separate mission-maker addon or list its worlds. For the
normal **Multiplayer > Host** flow, both players must be able to install and enable the scenario
addon, AC: Actual and any terrain/faction dependencies through the game. An unpacked local AC:
Actual copy alone is not discoverable in those scenario lists. For a local launcher test, use the
matching development-project route above, with any additional addon dependencies added to the launch
arguments.

## Host options

Options resolve from mission-header defaults, then `-acr...` launch arguments, then choices on the
in-game setup screen. Workshop scenarios enable that screen; ZIP/development Play and Host launchers
ask in an external menu and pass `-acrSetup 0`. A world launched directly has no mission header. The
table lists launch options for mission makers and server operators; most hosts use a setup menu.

| Option | What it does |
|---|---|
| `-acrPlayers US` / `USSR` / `any` | Put every player on one side. The mode places a spawn point for that side at one of its bases |
| `-acrFactions vanilla` / `ru_us` / `ru_ua` / `us_ua` / `<west>,<east>` | Select the playable faction pair: a named pair (the launcher's Factions menu) or any two faction keys your world's faction manager lists, e.g. `US,FIA`. The world's US-side bases and units go to the first, USSR-side to the second. A faction is playable when the world lists it and its catalog has soldiers and groups; a side without an `ACR_FactionSettings` entity copies the replaced side's settings (RP, AI commander, caps, supports) and builds its buy list from its catalog |
| `-acrMarksmen 0` | Disable the automatic DMR carrier in full UA/Russian rifle squads (enabled by default; keeps squad size and support weapons) |
| `-acrAI both` / `none` / `<faction>` | Which sides an AI commander leads (overrides `m_bAICommander`) |
| `-acrUnitCap N` | AI commander squad cap for every side (overrides `m_iAIUnitCap`) |
| `-acrThreat low` / `normal` / `high` | Threat level. Low: prices climb +25% per purchase (stepped pricing; fixed stock: half the infantry stock, a third of the vehicles, at least one each), 0.75x soldier limit, 0.6x AI squad cap, one AI attack at a time, AI buys few vehicles. High: prices climb +8% (fixed stock: double stock), 1.5x soldier limit and AI squad cap, one more AI attack at a time. Normal: +15%, nothing else changes |
| `-acrSize full` / `half` / `flashpoint` | Skirmish size: the whole map (default) or a random `ACR_Skirmish` layout of that size. Hand-built worlds without a matching layout play in full; generated layouts support all three sizes |
| `-acrLayout <name>` | Play this `ACR_Skirmish` layout (spaces may be written as `_`); overrides `-acrSize`. `-acrLayout generated` builds a layout from the map instead (see Generated layouts) |
| `-acrSeed N` | Seed of a generated layout: the same seed, size and terrain data give the same plan within a version (before manual moves). Unset or 0 = a new random layout each match |
| `-acrPricing stepped` / `stock` | Overrides every side's `m_ePricing` (stepped prices or fixed stock) |
| `-acrStartRP N` | Positive starting RP for every side (overrides `m_iStartingRP`; 0 does not override) |
| `-acrIncome X` | Income multiplier (default 1) |
| `-acrSkill N` | AI skill (overrides `m_iAISkill`) |
| `-acrReaction X` | AI spotting speed (overrides `m_fAIPerception`) |
| `-acrSoldierCap N` | Soft living-AI threshold per side before purchases/reinforcements stop (default 100 before threat scaling). A purchase begun below it can take the side over. The active-AI limit is raised to at least 2 x the resolved cap + 64 to avoid the vanilla 128-agent ceiling |
| `-acrSharedCommand 0` / `1` | Overrides `m_bSharedCommand` |
| `-acrRespawnSeconds N` | Overrides `m_iRespawnSeconds` |
| `-acrGameMasterAll` | Game Master for every player, not just host and admins |
| `-acrSetup 1` / `0` | Setup screen before the match. With 1, nothing spawns until the host (or a logged-in admin on a dedicated server) picks the battlefield, factions, threat, prices, AI commanders, AI squad cap, starting RP, income and shared command in a dialog and presses Start; other players wait. A dedicated server starts by itself 5 minutes after the first player joins if nobody does. Our Arland, Everon and Kolguyev scenarios set it in their header; the AC: Actual launchers pass `-acrSetup 0` because their menu already asked |
| `-acrFirstPlayerHost` | On dedicated servers, additionally trust the first connected player for this session. Off by default; configured admins remain trusted, and authority does not transfer after the first player disconnects |

## Designing a good map

- **Give each side a home base.** Set it as owned, allow units to spawn there, add a garrison, and put
  that side's spawn point nearby. Place its starting units around the base.
- **Leave room between bases.** The AI commander plans attacks outside the target base and needs space
  between that plan and other enemy bases. Bases less than about 400 m apart can confuse its plans; try
  600 m to 1.5 km between front-line bases.
- **Keep garrisons where they matter.** A base's guards appear only when enemies come within 400 m, so a
  large garrison does not use soldiers while the area is quiet.
- **Place buildings near important bases.** Guards and defending squads use nearby buildings for cover,
  including windows and doorways within 80 m of the base centre. Bases in the open are defended from cover
  near the centre instead.
- **Set each base's value to show its importance.** A value of 2 or 3 makes a useful objective. Use value
  0, no income, and no spawning for small tactical points such as a lighthouse or crossroads.
- **Use hidden bases** for rear areas and surprise supply points.
- **Check road access.** Squads use trucks on roads when that is faster than walking. Bases away from roads
  are reached on foot.
- **Keep the AI count in mind.** The default limit is about 100 soldiers per side. Very large maps can
  affect server performance.

## Current limits

- Vanilla church organs have a rare AI Easter egg: each organ has a 0.1% chance per minute
  to send one conscious AI on foot within 50 m to play its 25-second toccata. Any faction can
  play; player-controlled soldiers are excluded. Each trigger has a 30-minute cooldown.
  The group keeps its orders, and new orders or a player at the keyboard cancel the detour.
- **The game's US and USSR sides** are the most tested. The host menu also offers RHS and UA pairings;
  see [Factions and mods](#factions-and-mods). Other custom sides may not have player command gear yet.
  FIA selection, starting units, buy lists and command loadouts have automated coverage; broad
  live faction/mod combinations remain less tested.
- **Everon, Arland and Kolguyev.** Everon is the main large-map playtest terrain; travel, rides
  and AI behavior are still being tuned. Kolguyev uses generated layouts; other terrains need
  their own scenario setup and working navigation data.
- Advancing infantry in Combat mode can detour around a fresh infantry contact after a short
  stalemate. Nearby squads take turns manoeuvring; the original destination and queued orders
  remain. Defend, Suppress, Fast and Silent orders keep their assigned roles. Terrain and native
  pathfinding limit which flanks can be used; live combat acceptance is still pending.
- **No victory conditions**, by design: the match runs until the host ends it.
- **The AI commander cannot fly bought aircraft.** Combat support and off-map air lift are
  script-flown; the AI can call air lifts for long moves and road detours.
- **Player construction only:** AI base/helipad building and command-post construction supply
  pools are not implemented. There is no save/resume system.
- **Intel includes automatic base reports:** enemies within an owned base's radius plus 60 m
  are reported without line of sight. Reinforce also refuses enemies within 300 m even when unseen.
- **PC controls:** keyboard/mouse is the tested control scheme; console/controller play is unverified.
- Settings may still change between alpha builds. This page is updated with them.

## Testing your scenario

1. Start the map from Workbench. For a map saved inside the AC: Actual project, you can also use
   `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"` to play alone, or
   `ACR-Multiplayer.ps1 -Mode Host` to host it; select your map in the menu. See [Save, export and run](#save-export-and-run).
2. Open Workbench's console, or the game's log in `Documents/My Games/ArmaReforger/logs/<date>/console.log`.
   Check that:
   - `[ACR] Scenario started` appears: AC: Actual is running on the map.
   - `[ACR] Requisition list for US: N entries` has entries: the side's buy list loaded. If it says `0`,
     check that your faction key matches the faction manager.
   - `[ACR] Starting unit: <name>` appears for each starting unit you placed.
   - `[ACR] Host options: ...` shows the settings you selected.
3. Open the map with **M** or **F9**. You should see bases and squads, and be able to open the command panel.

Problems or questions: [open an issue](https://github.com/propst-design/ACActual/issues/new/choose) and
attach the log.

## Play a custom map with a friend

For a private playtest with the supplied launcher, copy your map into the AC: Actual project on both
computers. Keep the world, its layer, and its Workbench `.meta` files together. Send your friend a matching
copy of the development project with the map included. The host starts `Host-ACA.cmd` and picks the map;
the friend starts `Join-ACA.cmd` from their copy and enters the address shown in the host window. Use the
development project, not the release zip, so the updater does not replace the custom map. The package's
`READ ME FIRST.txt` explains the address and connection setup.

If you keep your map in a separate addon, you can test it in Workbench and publish it for the game's
scenario list. But the supplied AC: Actual launcher cannot load that separate addon yet. A published
scenario can use the game's normal Multiplayer > Host flow only when both players can install and
enable the mission addon and its AC: Actual dependency. Both players need a compatible AC: Actual
version and all dependencies. An unpacked local copy does not by itself create a Scenarios/Host
entry. See [Run a map kept in your own addon](#run-a-map-kept-in-your-own-addon) for the current
limitations and options.