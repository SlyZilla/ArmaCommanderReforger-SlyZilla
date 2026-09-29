# Arma Commander: Reforger (development)

A Reforger rebuild of the Arma 3 **Arma Commander** hybrid RTS/FPS mode. The original (by
Martin Hájek; ported from the fork by Will and Anarch Cassius) is APL-SA licensed, so this
derivative stays APL-SA with credit to the original authors. See LICENSE.md.

## Playing the test range

Close Reforger, then double-click **Play-Test-Range.cmd** (offline, solo).
**Host-Test-Range.cmd** opens the normal menu with the addon loaded, for hosting.

**Command map** (M or F9):

- Left-click a squad or soldier marker to select that squad. Left-click empty map to deselect.
- The panel on the right shows the squad. **Switch to squad** takes over its leader (or another free soldier).
- Commander only: **Combat / Fast / Silent** set how the squad moves; **right-click** the map to move it there.
  **Defend** or **Get in**, then right-click, to defend a spot or board a vehicle near the click.
  **Get out** dismounts it; **Unload** dismounts other squads riding in its vehicles.
- One commander per side (the first player, automatically). **Leave command** hands it to the next player.
  With nobody left in command, anyone can **Take command**.

| Key | Action |
| --- | --- |
| F7 | Possess the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to your command unit |
| F9 | Open/close the command map (no map item needed) |
| F6 (in a vehicle) | Drop every passenger squad now (a second press within 3 s overrides the "too high/fast" warning) |
| F5 | Test hotkey: move selected squad to cursor (map) or 35 m ahead |
| W/A/S/D in a turret | Gunner drives: steer the vehicle's AI driver (a map order hands control back) |
| Command menu key (default: backtick) | Native squad orders, only while possessing a squad leader |

Logs: automated tests write to `.local-game/logs/<date>/console.log`; normal play uses `Documents/My Games/ArmaReforger/logs`. Lines are tagged `[ACR]`.

## Code layout (`Scripts/Game`)

| File | Role |
| --- | --- |
| `ACR/ACR_Manager.c` | Core. Registers every AI squad automatically (including squads spawned mid-game), handles possession/return/order requests on the server, and handles local hotkeys |
| `ACR/ACR_Squad.c` | One commandable squad: roster, designated leader, native command handoff, orders |
| `ACR/ACR_CommandMenu.c` | Restricts the native command menu to possessed squad leaders and routes orders to their squad |
| `ACR/ACR_GunnerDrive.c` | Gunner drives: server-side throttle/steer override while a player mans a turret with an AI driver |
| `ACR/ACR_Supports.c` | Off-map supports: mortar/smoke/flare effect modules, virtual rocket strike and bomb run, script-flown gunship gun run / loiter |
| `ACR/ACR_Respawn.c` | Command-unit respawn at the nearest owned base; possessed-soldier death handling |
| `ACR/ACR_Transport.c` | Passenger squads, "vehicle is down" rule, queued LZ unloads |
| `ACR/ACR_MapFeed.c` | Server-to-player map snapshots (soldiers, group leaders) |
| `ACR/ACR_PlayerController.c` | Player RPCs, map markers, aimed possession |
| `ACR/ACR_MapHandoff.c` | Cleans up map gadget, focus, input and view flags when you leave a body |
| `ACR/ACR_TestRange.c` | Test scenario only: starts the manager and spawns Alpha 1 |
| `ACR_Tests/ACR_TestHarness.c` | Optional automated runs (`-acrAutoTest 1`, `-acrCommandTest 1`, `-acrMapHandoffTest 1`) |

Every AI group that is not a player group and has soldiers is registered as a squad. It
belongs to its faction, and only players of that faction can possess or command it.

Docs and past validation evidence are in `docs/`.
