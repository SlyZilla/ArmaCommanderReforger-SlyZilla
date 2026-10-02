# Making AC: Actual scenarios

How to run **Arma Commander Actual** on a new map or build a new scenario in Arma Reforger Workbench.
This guide is kept in step with the code: every World Editor setting and host option the mode reads
is listed here, and a check in the release build fails if one is missing.

> AC: Actual is an early alpha. Arland is the only map tested so far (Everon is next). Expect to
> report bugs, and send logs: every mode line in the log starts with `[ACR]`.

## Contents

1. [Before you start](#before-you-start)
2. [Quick start: copy Arland](#quick-start-copy-arland)
3. [What a world needs](#what-a-world-needs)
4. [Entity reference](#entity-reference)
5. [Mission header](#mission-header)
6. [Host options](#host-options)
7. [Designing a good map](#designing-a-good-map)
8. [Current limits](#current-limits)
9. [Testing your scenario](#testing-your-scenario)

## Before you start

- **Arma Reforger Tools** (Workbench) from Steam.
- **The AC: Actual addon.** Until it is on the Workshop, take the `ArmaCommanderReforger` folder from the
  [latest release](https://github.com/propst-design/ACActual/releases/latest) zip and add it in Workbench
  (Add Existing Project).
- **Your own addon** that lists AC: Actual (GUID `AC230926A0010001`) as a dependency. Don't edit the
  AC: Actual addon itself: its files are replaced on every update.
- **Licence:** AC: Actual is APL-SA (see [LICENSE.md](LICENSE.md)). Scenarios that include or modify its
  files must stay APL-SA and credit the original authors.

## Quick start: copy Arland

The fastest route is to copy the shipped scenario and move things around:

1. In your addon, create a new world as a **sub-scene** of the terrain you want (e.g. `worlds/Eden/Eden.ent`
   for Everon).
2. Open `Worlds/ACR_Arland.ent` from AC: Actual, select everything in its `default` layer, copy, and paste
   it into your world.
3. Fix what is map-specific:
   - the **navmesh files** on the AI world entity (see [AI world](#ai-world-and-navmesh)),
   - the positions of the **spawn point**, **bases**, **faction settings** and **starting units**.
4. Add a **mission header** (`Missions/<YourScenario>.conf`, see [Mission header](#mission-header)).
5. Play it from Workbench, open the log and check the `[ACR]` lines (see [Testing](#testing-your-scenario)).

## What a world needs

Everything goes in the world's layer. Prefab paths are vanilla unless they start with `ACR_`.

| Entity | Prefab / class | Needed? | Why |
|---|---|---|---|
| Game mode | `Prefabs/MP/Modes/Plain/GameMode_Plain.et` | Yes | Hosts respawn, map and building components (below) |
| Faction manager | e.g. `Prefabs/MP/Managers/Factions/FactionManager_USxUSSR.et` | Yes | Defines the sides; faction keys must match the AC: Actual entities |
| Loadout manager | e.g. `Prefabs/MP/Managers/Loadouts/LoadoutManager_USxUSSR.et` | Yes | Player spawning (AC: Actual swaps in officer loadouts, see [limits](#current-limits)) |
| Player spawn point | e.g. `Prefabs/MP/Spawning/SpawnPoint_US.et` | Yes, one per playable side | Marks the side's HQ: command units respawn at the owned base nearest to it |
| AI world | `Prefabs/AI/SCR_AIWorld.et` with the map's navmeshes | Yes | Without navmesh, AI can't move |
| Perception manager | `Prefabs/World/Game/PerceptionManager.et` | Yes | AI spotting |
| Radio manager | `Prefabs/Systems/Radio/RadioManager.et` | Yes | Radio reports |
| **`ACR_Mode`** | class `ACR_Mode` | Yes, exactly one | Starts the mode |
| **`ACR_FactionSettings`** | class `ACR_FactionSettings` | Recommended, one per side | RP, buy list, AI commander, supports |
| **`ACR_Base`** | class `ACR_Base` | Yes, several | Bases to capture; owned bases are where units deploy |
| **`ACR_StartingUnit`** | class `ACR_StartingUnit` | Optional | Units that exist when the match starts |

### Game mode components

Add these to the game mode entity (the Arland world shows working values):

- **`SCR_RespawnSystemComponent`** with an `SCR_AutoSpawnLogic`. Set its forced faction to the side that
  has a spawn point if only one side does. Players then spawn straight into their command unit.
- **`SCR_MapConfigComponent`** (optional): gadget map config
  `{AC230926A0010098}Configs/Map/ACR_MapFullscreen.conf` gives the command map finer contour lines.
- **`SCR_CampaignBuildingManagerComponent`** (optional): lets construction trucks build. Without it,
  construction trucks are just trucks.

### AI world and navmesh

On the `SCR_AIWorld` entity, point the `NavmeshWorldComponent`s at the terrain's navmesh files: one for
soldiers (`NavmeshWorld`), one for vehicles (`ChimeraNavmeshWorld`), and the low-res one if the map
has it. Arland uses the Game Master navmeshes (`worlds/GameMaster/Navmeshes/GM_Arland*.nmn`); look for
the matching files of your terrain in the vanilla data, or generate your own in Workbench for a custom map.
AC: Actual's road routing and land checks read the terrain directly and need no setup.

## Entity reference

Workbench shows each setting by its name without the prefix (`m_sName` appears as **Name**). Empty or
untouched settings use the defaults listed.

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

One per side. Place it anywhere (Arland puts each at its side's HQ for tidiness).
Without one, a side gets 50 RP, the automatic buy list and all supports, but **no AI commander**.

| Setting | Default | What it does |
|---|---|---|
| `m_sFaction` | `US` | Faction key this applies to; must match the faction manager (`US`, `USSR`, `FIA`...) |
| `m_iStartingRP` | 100 | Requisition points at the start |
| `m_bAICommander` | on | An AI commander leads this side whenever no player is in command |
| `m_iAIUnitCap` | 8 | Most squads the AI commander keeps in the field |
| `m_iAIAttacks` | 1 | Base attacks the AI commander runs at once |
| `m_bAutoFill` | on | Fill the buy list from the faction's Game Master catalog with automatic prices |
| `m_aRequisitions` | empty | Hand-picked buy list entries (see below). With auto fill on, an entry with the same prefab replaces the automatic one |
| `m_aExclude` | empty | Group or vehicle prefabs to leave out of the automatic list |
| `m_aGarrisonSoldiers` | empty | Soldier prefabs for base garrisons. Empty = the faction's riflemen (twice as likely), machine gunners, grenadiers and AT soldiers |
| `m_bSupports` | on | The commander can call off-map supports |
| `m_aSupports` | empty | Supports on offer (see below). Empty = all of them at default prices |
| `m_sSupportHelicopter` | empty | Gunship for helicopter supports. Empty = UH-1H for western factions, Mi-8 otherwise |
| `m_sSupportTransportHelicopter` | empty | Helicopter for the door-gunner support. Empty = armed UH-1H / armed Mi-8 |
| `m_sSupportRocket` | empty | Rocket for rocket strikes and gunships. Empty = Hydra 70 / S-5 |

"Western" means a faction key containing `US` (but not `USSR`), `NATO` or `BLUFOR`.

**Automatic buy list.** Every enabled group and vehicle in the faction's Game Master catalog, once per name.
Guards, static weapons and planes are left out. Prices: infantry 3 RP per soldier (at least 6); vehicles
12 (car), 15 (truck), 45 (APC) or 60 (helicopter), +10 armed, +15 armoured, +6 crew. Cheaper units have more
stock. The AI commander never buys aircraft (it can't fly them) and buys unarmed trucks only to ferry squads.

#### Requisition entry (`m_aRequisitions`)

Add with the **+** button; pick prefabs with the browser.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name in the buy list and on the map; numbered per purchase |
| `m_sGroup` | | Infantry group to spawn |
| `m_sVehicle` | | Vehicle to spawn (optional) |
| `m_bCrewed` | on | Spawn the vehicle with its driver and gunners |
| `m_sPassengers` | | Group that rides in the vehicle (optional) |
| `m_iCost` | 20 | Cost in RP |
| `m_iStock` | 2 | How many can be bought per match |

Hand-added vehicles don't yet get the automatic aircraft/transport/armed flags, so the AI commander treats
them as armed ground vehicles. Exclude aircraft from hand lists if an AI commander plays that side.

#### Support entry (`m_aSupports`)

| Setting | Default | What it does |
|---|---|---|
| `m_eType` | | Kind of support (table below) |
| `m_sName` | empty = default | Name in the command panel |
| `m_iCost` | -1 = default | Cost in RP |
| `m_iCooldown` | -1 = default | Seconds before it can be called again |

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

### ACR_Base

A base (control point). Its position is the centre of the capture circle.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | `Base N` | Name on the map |
| `m_sOwner` | empty | Starting owner faction key; empty = neutral |
| `m_iGarrison` | 10 | AI soldiers guarding it. They exist only while enemies are within 400 m, and are used up as they die |
| `m_iValue` | 1 | RP it adds to its owner's income (shown on the map as `Name [value]`) |
| `m_bIncome` | on | Produces income. Off = purely tactical point |
| `m_bHidden` | off | Hidden from other sides until one of their units enters it |
| `m_bAllowSpawn` | on | Bought units and reinforcements can deploy here (while owned and not contested) |
| `m_fRadius` | 50 | Capture radius in metres |

**Economy:** every 60 s each side gets 5 RP plus the value of each income base it owns.
**Capture:** soldiers inside the circle push it towards their side; more soldiers capture faster.

### ACR_StartingUnit

A unit spawned at its position 2 s after the match starts (vehicle 6 m away from the group).
The side comes from the prefab's faction.

| Setting | Default | What it does |
|---|---|---|
| `m_sName` | | Name on the map, used as written (e.g. `1st Squad`) |
| `m_sGroup` | | Infantry group |
| `m_sVehicle` | | Vehicle (optional) |
| `m_bCrewed` | on | Vehicle comes with driver and gunners |
| `m_sPassengers` | | Group riding in the vehicle (optional) |

Any other AI group placed in the world (or spawned later, e.g. by Game Master) is picked up as a
commandable squad of its faction automatically. Starting units just give it a name and a vehicle.

## Mission header

`Missions/<YourScenario>.conf` makes the scenario show up in the game's scenario list:

```
SCR_MissionHeader {
 World "{<your world's GUID>}Worlds/<YourWorld>.ent"
 m_sName "AC: Actual - <Map>"
 m_sAuthor "<you>"
 m_sDescription "<one line>"
 m_sGameMode "Arma Commander Actual"
 m_iPlayerCount 8
}
```

## Host options

Hosts can override some settings at launch without touching the world. Add them to the server's or
game's launch parameters. The AC: Actual launchers set them from their menu.

| Option | What it does |
|---|---|
| `-acrPlayers US` / `USSR` / `any` | Put every player on one side. The mode places a spawn point for that side at one of its bases |
| `-acrAI both` / `none` / `<faction>` | Which sides an AI commander leads (overrides `m_bAICommander`) |
| `-acrUnitCap N` | AI commander squad cap for every side (overrides `m_iAIUnitCap`) |
| `-acrStartRP N` | Starting RP for every side (overrides `m_iStartingRP`) |
| `-acrIncome X` | Income multiplier (default 1) |
| `-acrSkill N` | AI skill (overrides `m_iAISkill`) |
| `-acrReaction X` | AI spotting speed (overrides `m_fAIPerception`) |
| `-acrSoldierCap N` | Living AI soldiers per side before purchases stop (default 100) |
| `-acrSharedCommand 0` / `1` | Overrides `m_bSharedCommand` |
| `-acrRespawnSeconds N` | Overrides `m_iRespawnSeconds` |
| `-acrGameMasterAll` | Game Master for every player, not just host and admins |

## Designing a good map

- **Give each side an HQ:** an owned base with `m_bAllowSpawn` on, a garrison, and the side's spawn point
  near it. Place the side's starting units around it.
- **Spacing matters for the AI commander.** It stages attacks about 275 m outside the target's circle and
  keeps staging points 350 m away from other enemy bases. Bases packed closer than ~400 m make it hesitate;
  front-line bases 600 m-1.5 km apart play well.
- **Garrisons wake at 400 m.** A base's garrison only exists while enemies are within 400 m, so garrison
  size costs nothing until a fight.
- **Use values to shape the fight.** High-value bases (2-3) become objectives; zero-value, no-income,
  no-spawn bases (lighthouses, crossroads) are tactical points.
- **Hidden bases** are good for rear areas and surprise depots.
- **Roads:** squads ride in trucks along roads; rides are refused where the road detour makes walking faster.
  Bases off the road network are reached on foot.
- **Size:** about 100 AI soldiers per side is the default cap. Watch server performance on big maps.

## Current limits

- **Two sides, US and USSR**, are what has been tested. Other faction keys work in the settings, but
  player command units use **US and USSR officer loadouts only**, so a third playable faction has no
  loadout yet. AI-only factions (e.g. FIA) may work but are untested.
- **Arland only** so far. Everon support (bases, spawns, routing over 13 km) is the next milestone.
- **No victory conditions**, by design: the match runs until the host ends it.
- **The AI commander can't fly**: aircraft are for players only.
- Settings may still change between alpha builds. This page is updated with them.

## Testing your scenario

1. Play it from Workbench, or host it with the launcher.
2. Open the log (`Documents/My Games/ArmaReforger/logs/<date>/console.log`, or Workbench's console) and
   look for:
   - `[ACR] Scenario started`: `ACR_Mode` is in the world.
   - `[ACR] Requisition list for US: N entries`: the buy list was built (N = 0 means a wrong faction key).
   - `[ACR] Starting unit: <name>`: once per `ACR_StartingUnit`.
   - `[ACR] Host options: ...`: the options in force.
3. Open the map (**M** or **F9**): your bases, squads and the command panel should be there.

Problems or questions: [open an issue](https://github.com/propst-design/ACActual/issues/new/choose) and
attach the log.
