# Make an AC: Actual map

This guide shows how to set up AC: Actual on a different map, test your changes, and play a custom
map with a friend. It also lists the settings you can change in Workbench.

> AC: Actual is an early alpha. Arland is the most tested map; Everon is a first pass. Expect to
> report bugs, and send logs: every mode line in the log starts with `[ACR]`.

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
- **The AC: Actual addon.** Until AC: Actual is on the Workshop, get the `ArmaCommanderReforger` folder
  from the [latest release](https://github.com/propst-design/ACActual/releases/latest) zip and add it to
  Workbench with **Add Existing Project**.
- **Your own addon** (your own mod project). Set AC: Actual, GUID `AC230926A0010001`, as a dependency so
  your map can use AC: Actual's game mode and settings. Keep your work in your own addon so an AC: Actual
  update does not replace it.
- **Licence:** AC: Actual is APL-SA (see [LICENSE.md](LICENSE.md)). Scenarios that include or modify its
  files must stay APL-SA and credit the original authors.

In this guide, a **world** is the map and the objects placed on it. An **addon** is the mod folder that
contains and shares your work. A **mission header** gives your world a name in the game's scenario list.
**RP** means requisition points: the money sides spend on units and support.

## Quick start: copy Arland

The quickest way to make a new map is to copy the working Arland setup and move its objects:

1. In your addon, create a world using the terrain you want (for example, Everon).
2. In AC: Actual's `Worlds/ACR_Arland.ent`, copy everything from the `default` layer and paste it into
   your new world's `default` layer. These objects set up the game mode, bases, and units.
3. Move the spawn point, bases, faction settings, and starting units to suit your map.
4. Set the AI world to use this terrain's navigation files. These files tell soldiers and vehicles where
   they can move; see [AI world and navmesh](#ai-world-and-navmesh).
5. Create a mission header so the game can show your map by name. See [Mission header](#mission-header).
6. Play it from Workbench and check that bases, units, and the command panel appear. See
   [Testing your scenario](#testing-your-scenario).

To play a custom map with a friend today, see [Play a custom map with a friend](#play-a-custom-map-with-a-friend).

## What a world needs

Put these objects in the world's `default` layer. A **prefab** is a ready-made object you choose from
Workbench's browser. Paths starting with `ACR_` are part of AC: Actual; the others come with the game.

| Entity | Prefab / class | Needed? | Why |
|---|---|---|---|
| Object | Prefab / class to add | Needed? | What it does |
|---|---|---|---|
| Game mode | `Prefabs/MP/Modes/Plain/GameMode_Plain.et` | Yes | Lets the game handle spawning, the map, and construction components (below) |
| Faction manager | e.g. `Prefabs/MP/Managers/Factions/FactionManager_USxUSSR.et` | Yes | Sets the sides in the match. Their faction keys must match the AC: Actual settings |
| Loadout manager | e.g. `Prefabs/MP/Managers/Loadouts/LoadoutManager_USxUSSR.et` | Yes | Sets what players spawn with (AC: Actual uses officer gear; see [Current limits](#current-limits)) |
| Player spawn point | e.g. `Prefabs/MP/Spawning/SpawnPoint_US.et` | Yes, one per playable side | Shows where that side's HQ is. Command units respawn at the nearest base it owns |
| AI world | `Prefabs/AI/SCR_AIWorld.et` with the map's navigation files | Yes | Gives AI soldiers and vehicles routes they can use |
| Perception manager | `Prefabs/World/Game/PerceptionManager.et` | Yes | Lets AI notice other units |
| Radio manager | `Prefabs/Systems/Radio/RadioManager.et` | Yes | Lets units send radio reports |
| **`ACR_Mode`** | class `ACR_Mode` | Yes, exactly one | Starts the mode |
| **`ACR_FactionSettings`** | class `ACR_FactionSettings` | Recommended, one per side | RP, buy list, AI commander, supports |
| **`ACR_Base`** | class `ACR_Base` | Yes, several | Bases to capture; owned bases are where units deploy |
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
  AI squads that need ammo, repairs, fuel or healing also head for these.

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

AC: Actual uses faction keys to tell sides apart. A key containing `US` (but not `USSR`), `NATO`, or
`BLUFOR` counts as a western side for settings such as mortar choice.

#### Factions and mods

The host setup menu offers four side pairings: the game's US vs USSR, RHS US vs RHS AFRF, UA vs RHS AFRF,
or RHS US vs UA. The menu loads the required RHS or UA mods for the chosen pair. The UA mod replaces the
game's US side for that session. ACE All in One is a separate optional choice. Everyone joining must use
the same side pairing and ACE choice as the host. With no extra faction mods selected, the game uses its
usual US vs USSR sides.

You can use `m_aPrefabMappings` to replace a unit with one from another mod. Add a row for the original
prefab and choose its replacement from the prefab browser. AC: Actual also tries to match starting units
by their catalog name or role. If it cannot find a replacement, it uses the original unit and writes a
`[ACR] FACTION fallback` line in the server log.

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
| `m_eBombRunDelivery` | `VIRTUAL` | Bomb-run delivery: existing virtual blasts, a flyover prefab plus virtual blasts, or an editable call-in prefab |
| `m_sBombRunPrefab` | empty | Aircraft/flyover prefab (`FLYOVER`) or call-in spawner prefab (`EDITABLE_CALL_IN`) |
| `m_fBombRunAltitude` | 150 m | Flyover height above the target; clamped to 30-1,000 m |
| `m_fBombRunSpeed` | 110 m/s | Flyover speed; sets bomb spacing in time in `FLYOVER` mode; clamped to 20-400 m/s |
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
| `m_iStock` | Base purchases per match (-1 = current default; normal threat scaling still applies) |
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

### ACR_Skirmish

This optional object defines a smaller match area on a large map: half the map, or a small fight around
about five bases. Add one object for each layout; its position does not matter. The host can choose the
layout by name (`-acrLayout`) or choose a size (`-acrSize`) and get a random layout of that size. If no
layout is chosen, the whole map is used. Everon has north and south halves, plus small areas around
Saint-Philippe, Montignac, and Levie. Each layout has two helipads at its HQ. Add one or two
`ACR_Helipad` objects for each HQ you list so both sides can park support helicopters.

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

Create a **Mission Header** in Workbench and choose your saved world. This gives the map a name in the
game's scenario list. Workbench fills in most of the details for you; this is what the saved file looks like:

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

The header only adds your world to the scenario list; it does not package or copy the world. Workbench
creates the `.meta` files beside the world and header. Keep them there and do not edit them. When you
choose the world in the header, Workbench fills in its resource ID and path for you. The world's ID is
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
3. To play alone, run `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"`. The setup menu opens
   with your map selected. Choose the settings and press Enter. `Play-ACA.cmd` opens the same menu,
   starting with Arland selected; you can switch to your map there.
4. To host a multiplayer game, run `Host-ACA.cmd` from the AC: Actual development project and select
   your map in the host menu. The launcher starts the server and then joins it on your PC. Your friend
   runs `Join-ACA.cmd` from a matching copy of that project and enters the address shown in your host
   window. You both need the same map files and the same faction-mod choices.

These launchers use the files in the AC: Actual project directly, so there is no Workshop upload or
package build for a local test. They start the selected world file directly; the mission header is for
the game's scenario list. To let a friend play, give them a matching copy of the development project
with your map included. Use that project copy, not the release zip: the release launcher updates itself
and can replace your custom map with the released files. The helper also loads only AC: Actual and its
chosen faction mods; a custom terrain mod is not added automatically.

### Run a map kept in your own addon

Keeping your map in your own addon is best for protecting your work when AC: Actual updates. Workbench
can test it there. You can also publish your addon so it appears in the game's **Scenarios** list. In
Workbench, publish the addon with AC: Actual listed as a dependency, then enable both addons in the game.
The [scenario setup guide](https://community.bistudio.com/wiki/Arma_Reforger:Capture_%26_Hold_Setup)
shows how the header makes a scenario appear in that list.

For your own first test, **Private** visibility is fine. Private addons are only visible to you. To let a
friend find the addon, publish it as **Unlisted** and send them its link, or use **Test** if you want it
listed with other test mods. See Bohemia's
[publishing guide](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Publishing_Process).

**Important for playing together:** the current AC: Actual launcher only loads AC: Actual and its chosen
faction mods. It does not load a separate mission-maker addon, so that map will not appear in its map
menu. If both addons are published and available to the game, the normal steps are **Multiplayer** >
**Host**, choose the scenario and its required mods, then start the server. Your friend joins that server.
AC: Actual is not currently on the Workshop, so the game cannot download it as a dependency; both players
need the same AC: Actual files installed. If the game's host screen cannot select that local AC: Actual
addon, this route cannot host your separate addon today. For co-op today, use the AC: Actual development
project route above.

## Host options

The host can change some match settings when starting the game, without editing the map. These are
called launch options. The AC: Actual setup menu sets them for you, both when hosting and when playing
alone (the game also acts as the server in singleplayer). The table is here for mission makers who want
to set an option directly; most hosts can just use the menu.

| Option | What it does |
|---|---|
| `-acrPlayers US` / `USSR` / `any` | Put every player on one side. The mode places a spawn point for that side at one of its bases |
| `-acrFactions vanilla` / `ru_us` / `ru_ua` / `us_ua` | Select the playable faction pair (the launcher sets this from its Factions menu) |
| `-acrAI both` / `none` / `<faction>` | Which sides an AI commander leads (overrides `m_bAICommander`) |
| `-acrUnitCap N` | AI commander squad cap for every side (overrides `m_iAIUnitCap`) |
| `-acrThreat low` / `normal` / `high` | Threat level. Low: prices climb +25% per purchase (stepped pricing; fixed stock: half the infantry stock, a third of the vehicles, at least one each), 0.75x soldier limit, 0.6x AI squad cap, one AI attack at a time, AI buys few vehicles. High: prices climb +8% (fixed stock: double stock), 1.5x soldier limit and AI squad cap, one more AI attack at a time. Normal: +15%, nothing else changes |
| `-acrSize full` / `half` / `flashpoint` | Skirmish size: the whole map (default) or a random `ACR_Skirmish` layout of that size. Worlds without one play in full |
| `-acrLayout <name>` | Play this `ACR_Skirmish` layout (spaces may be written as `_`); overrides `-acrSize` |
| `-acrPricing stepped` / `stock` | Overrides every side's `m_ePricing` (stepped prices or fixed stock) |
| `-acrStartRP N` | Starting RP for every side (overrides `m_iStartingRP`) |
| `-acrIncome X` | Income multiplier (default 1) |
| `-acrSkill N` | AI skill (overrides `m_iAISkill`) |
| `-acrReaction X` | AI spotting speed (overrides `m_fAIPerception`) |
| `-acrSoldierCap N` | Living AI soldiers per side before purchases stop (default 100). The game's active-AI limit is raised to 2 x this + 64 so bought squads are never left empty |
| `-acrSharedCommand 0` / `1` | Overrides `m_bSharedCommand` |
| `-acrRespawnSeconds N` | Overrides `m_iRespawnSeconds` |
| `-acrGameMasterAll` | Game Master for every player, not just host and admins |
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

- **The game's US and USSR sides** are the most tested. The host menu also offers RHS and UA pairings;
  see [Factions and mods](#factions-and-mods). Other custom sides may not have player command gear yet.
  AI-only sides such as FIA may work, but have not been tested.
- **Arland and Everon.** Everon (`Worlds/ACR_Everon.ent`) is a first pass: travel, rides and the AI
  commander over 13 km are still being tuned.
- **No victory conditions**, by design: the match runs until the host ends it.
- **The AI commander can't fly**: aircraft are for players only (support helicopters are script-flown).
- Settings may still change between alpha builds. This page is updated with them.

## Testing your scenario

1. Start the map from Workbench. For a map saved inside the AC: Actual project, you can also use
   `Start-Test-Range.ps1 -World "Worlds/ACR_<Name>.ent"` to play alone, or
   `ACR-Multiplayer.ps1 -Mode Host` to host it. See [Save, export and run](#save-export-and-run).
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
scenario can use the game's normal Multiplayer > Host flow only when both players can install and enable
the mission addon and its AC: Actual dependency. Since AC: Actual is not on Workshop yet, both players
need the same AC: Actual release files. See [Run a map kept in your own addon](#run-a-map-kept-in-your-own-addon)
for the current limitations and options.
