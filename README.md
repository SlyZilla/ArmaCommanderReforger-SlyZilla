# Arma Commander Actual (AC: Actual)

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Development stage: Alpha](https://img.shields.io/badge/Status-Alpha-yellow?style=flat-square)](#development-limits)
[![Requires Better Battle Buddies](https://img.shields.io/badge/Requires-Better_Battle_Buddies-teal?style=flat-square)](#maps-and-match-options)
[![License: APL-SA](https://img.shields.io/badge/License-APL--SA-orange?style=flat-square)](LICENSE.md)

Command squads from the map, then take control of a friendly AI soldier and join
the fight. Capture bases, requisition units and coordinate support across Arland,
Everon and Kolguyev. Play solo against AI commanders or share command with friends.

An unofficial Reforger rebuild of Arma 3's **Arma Commander**, licensed under
APL-SA. Original authors and contributors are credited below and in [LICENSE.md](LICENSE.md).

This is the main public source, development and [issue tracker](https://github.com/propst-design/ArmaCommanderReforger/issues) for AC: Actual.
Install and update through the in-game Workshop for normal play. The older
[ACActual repository](https://github.com/propst-design/ACActual) retains historical
documentation, existing issues and legacy ZIP downloads/updaters. File new bug
reports and feature requests in this repository's issue tracker.

This README covers the development checkout as well as the basics of playing.
The changelog lists **0.5.4 as prepared** and **0.5.3 as previously published**;
development features are not proof of a Workshop update. See [CHANGELOG.md](CHANGELOG.md).

## Playing from Workshop

Workshop is the normal install route: open Workshop in Reforger, search for **Arma Commander
Actual**, download and enable it, then select an included AC: Actual scenario in Scenarios or Host. The host/admin picks the sides and match options on the setup screen, then presses Start. No
external launcher is needed for Workshop play. Report bugs and issues at
https://github.com/propst-design/ArmaCommanderReforger/issues (include the map, mod set, steps to reproduce and
logs if available).

## Maps and match options

Everon, Arland and Kolguyev are included. Everon has a hand-built full-island layout, north/south
halves and three flashpoints. Arland has a hand-built layout and is the main automated test map. All
three support generated layouts; Kolguyev always generates its bases and HQs from named map
locations. Generated layouts can be previewed, rerolled and adjusted by clicking a base and then its
new position on the in-game setup map. Other terrains need an AC: Actual scenario setup; installing
a terrain alone does not add one.

Capture bases for RP, requisition squads and vehicles, and call mortar, artillery and air support.
Construction trucks build service stations, forward deploy points and helipads. There are no victory
conditions or saved matches. This development tree now requires the separate Better Battle Buddies
addon (`BBB0261015000001`). Keep its checkout beside this one in Workbench/addons; source is in
`propst-design/BetterBattleBuddies` (private). BBB 0.1.3 is released; ACA's documented
0.5.3 Workshop build still bundles the older AI code. The separate dependency belongs
to the prepared split release. Verify the exact BBB dependency before publishing ACA.

## Legacy ZIP / development launchers

The release ZIP includes **Play-ACA.cmd**, **Host-ACA.cmd**, **Join-ACA.cmd** and a **READ ME
FIRST.txt** guide. Play and Host ask only for the map and optional mods before loading; choose
playable sides and match settings on the in-game startup screen, then press **Start**.
New paired ZIPs use **ACActual-WithBBB.zip** and contain both addon folders. Users of the older
single-addon ZIP must download/extract the complete paired ZIP once; the old updater cannot install
BBB. After that migration, published paired ZIPs self-update. Keep custom work in a separate addon or use a development copy
without the release updater source.

## Playing the test range

Close Reforger, then double-click **Play-Test-Range.cmd** (offline, solo). In the development
checkout, **Host-Test-Range.cmd** opens the normal game menu with the addon loaded. Unpacked local
addons do not appear in the game's Scenarios/Host lists; use **Host-ACA.cmd** for a local
multiplayer scenario instead. The ZIP does not include Host-Test-Range.cmd.

## Factions and mods

The host setup menu can run vanilla US vs USSR, RHS US vs RHS Russia, RHS Russia vs Ukraine, or RHS
US vs Ukraine. RHS and Ukraine Armed Forces RHS are optional; the launcher adds the required addon
IDs when selected; it does not download those mods. The current ZIP/development launcher expects
optional faction mods in `E:\ArmaReforgerMods\addons`; it is configured for the development PC, so
other installations may need their addon search paths adjusted. Ukraine Armed Forces RHS replaces
the vanilla US faction for that session, so choose a pair containing **UA** to play it. ACE All in
One is a separate optional toggle. Play/Host/Join also automatically load All-in-One Command Menu if
its Workshop copy is found locally; it is a separate optional mod and joiners need it when the host
loads it. Joiners should select the same faction pair and ACE setting as the host; a mod mismatch
can prevent joining, and a different faction choice can produce the wrong setup. The join menu
explains the selections to check.

**Other faction mods (no promises).** AC: Actual can play any two eligible factions a map lists,
picked as Blue side / Red side on the in-game setup screen (or `-acrFactions WEST,EAST`). The three
scenario worlds list the game's US, USSR and FIA, plus RHS US and RHS Russia when RHS is loaded.
Another faction mod works only if the map lists it (one line in the map's faction manager) and the
mod sets up its factions the standard way (Game Master catalogs of soldiers, groups and vehicles).
Even then it may look rough: mixed uniforms, a squad leader as your command unit, vanilla stand-ins
for what the mod lacks. Only RHS and Ukraine Armed Forces RHS are hand-fitted. Want a faction mod
supported properly? Ask on GitHub: https://github.com/propst-design/ArmaCommanderReforger/issues

## Command map and controls

Open with M or F9:

- Left-click a squad or soldier marker to select that squad. Left-click empty map to deselect.
- The panel on the right shows the squad. **Switch to squad** takes over its leader (or another free soldier).
- Commander only: **Combat / Fast / Silent** set how the squad moves; **right-click** the map to move it there.
  **Defend** or **Get in**, then right-click, to defend a spot or board a vehicle near the click.
  **Get out** dismounts it; **Unload** dismounts other squads riding in its vehicles.
- Multiple commanders can share a side, its squads and its requisition pool.
  **Leave command** gives up command; **Take command** joins it when available.

| Key | Action |
| --- | --- |
| F7 | Possess the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to your command unit |
| F9 | Open/close the command map (no map item needed) |
| F6 (in a vehicle) | Drop every passenger squad now (a second press within 3 s overrides the "too high/fast" warning) |
| F5 | Move selected squad to cursor (map) or 35 m ahead |
| W/A/S/D in a turret | Gunner drive with an AI driver (a map order hands control back) |
| Command menu key (default: backtick) | Native squad orders, only while possessing a squad leader |

F7 only takes over living soldiers in your side's commandable squads, never another player's soldier
or command unit. Keys can be rebound under **Settings > Controls > AC: Actual**. PC keyboard/mouse
is the tested control scheme; console/controller play remains unverified.

Intel comes from AI perception, player line of sight within 300 m, and automatic reports inside an
owned base's radius plus 60 m (no line-of-sight requirement). Contacts turn suspected after 30 s and
expire after 90 s without an update. The AI commander uses its side's intel.

Logs: automated tests write to `.local-game/logs/<date>/console.log`; normal play uses `Documents/My
Games/ArmaReforger/logs`. Mod lines use `[ACR]` and `[BBB]`; ZIP-host server logs use
`ArmaCommanderReforger/.local-server/logs`.

## Code layout (`Scripts/Game`)

| File | Role |
| --- | --- |
| `ACR/ACR_Manager.c` | Core. Registers every AI squad automatically (including squads spawned mid-game), handles possession/return/order requests on the server, and handles local hotkeys |
| `ACR/ACR_Squad.c` | One commandable squad: roster, designated leader, native command handoff, orders |
| `ACR/ACR_CommandMenu.c` | Restricts the native command menu to possessed squad leaders and routes orders to their squad |
| `ACR/ACR_GunnerDrive.c` | Gunner drives: server-side throttle/steer override while a player mans a turret with an AI driver |
| `ACR/ACR_Supports.c` | Supports: virtual rocket strike and bomb run, script-flown gunship gun run / loiter; mortar calls go to ACR_Mortars |
| `ACR/ACR_Mortars.c` | Mortar teams: set up / pack up, ground check, range, fire missions through the game's artillery AI |
| `ACR/ACR_Respawn.c` | Command-unit respawn at the nearest owned base; possessed-soldier death handling |
| `ACR/ACR_Transport.c` | Passenger squads, "vehicle is down" rule, queued LZ unloads |
| `ACR/ACR_MapFeed.c` | Server-to-player map snapshots (soldiers, group leaders) |
| `ACR/ACR_PlayerController.c` | Player RPCs, map markers, aimed possession |
| `ACR/ACR_MapHandoff.c` | Cleans up map gadget, focus, input and view flags when you leave a body |
| `ACR/ACR_TestRange.c` | Test scenario only: starts the manager and spawns Alpha 1 |
| `ACR_Tests/ACR_TestHarness.c` | Optional automated runs (`-acrAutoTest 1`, `-acrCommandTest 1`, `-acrMapHandoffTest 1`) |

Every AI group that is not a player group and has soldiers is registered as a squad. It
belongs to its faction, and only players of that faction can possess or command it.

Mission makers: see [MISSION-MAKING.md](MISSION-MAKING.md). Docs and past validation evidence are in `docs/`.

## Working on the mod

Start with [documentation](documentation), then the relevant sections of [documentation](documentation).
The documentation covers architecture, engine constraints, validation, current work and
release procedures. [MISSION-MAKING.md](MISSION-MAKING.md) explains how to build a
scenario in your own addon so updates do not overwrite your work.

Keep the BBB checkout beside this one under Workbench's `addons` directory.
Production mode code lives under `Scripts/Game/ACR`; the code map above gives
entry points. BBB owns the general AI systems in its separate addon.

For maintainers on the shared Windows setup, **Mod-Release-Console.cmd** prepares
checked ACA/BBB/AiO snapshots, opens the selected project and verifies downloaded
content. Its details are in the documentation. **Publish-Release.cmd** is the separate
legacy GitHub ZIP publisher. The public GitHub README is maintained in
`tools/package/REPO-README.md`; this file is the main source repository's guide.

### Development limits

Off-map air lift has passed automated loaded-transport, clearance and terrain
checks. Broader live and multiplayer acceptance remains open. Generated-layout editing has automated coverage; visual and multiplayer
acceptance remain open. Keep custom changes in your own addon or a development
copy without the release updater.

## Made with AI

This project is openly vibecoded. Nearly all of the code, launchers and documentation
were written by **Claude** (Anthropic's AI model, working in Claude Cowork), directed,
designed and play-tested by **Jacob** ([propst-design](https://github.com/propst-design)).
Since 5 Oct **Codex** (OpenAI's coding agent) has contributed part of the code and tests too,
working in the same repository under Jacob's direction. Since 14 Oct **Antigravity**
(Google's agentic coding environment, whichever model it runs) contributes as well.
Expect rough edges; bug reports with logs help a lot.

Playtesters: **PoppiPoppins**, **Hexxyz** and **Frost**.
