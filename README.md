# Arma Commander: Reforger (development)

A Reforger rebuild of the Arma 3 **Arma Commander** hybrid RTS/FPS mode. The original is
APL-SA licensed, so this derivative must stay APL-SA with credit to the original authors.

## Playing the test range

Close Reforger, then double-click **Play-Test-Range.cmd** (offline, solo).
**Host-Test-Range.cmd** opens the normal menu with the addon loaded, for hosting.

| Key | Action |
| --- | --- |
| F5 | On foot: selected squad advances 35 m. On map: selected squad moves to cursor |
| F6 | Selected squad rallies to where it was first registered |
| F7 | Possess the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to your command unit |
| F9 | Open/close the command map (no map item needed) |
| Command menu key (default: backtick) | Native squad orders, only while possessing a squad leader |

The "selected squad" is the squad you are in, otherwise the one you were last in. Proper
map selection and the full order set (Move, Move Fast, Defend, Get In, Get Out) come next.

Logs: `.local-game/logs/<date>/console.log`, tagged `[ACR]`.

## Code layout (`Scripts/Game`)

| File | Role |
| --- | --- |
| `ACR/ACR_Manager.c` | Core. Registers every AI squad automatically (including squads spawned mid-game), handles possession/return/order requests on the server, and handles local hotkeys |
| `ACR/ACR_Squad.c` | One commandable squad: roster, designated leader, native command handoff, orders |
| `ACR/ACR_CommandMenu.c` | Restricts the native command menu to possessed squad leaders and routes orders to their squad |
| `ACR/ACR_MapFeed.c` | Server-to-player map snapshots (soldiers, group leaders) |
| `ACR/ACR_PlayerController.c` | Player RPCs, map markers, aimed possession |
| `ACR/ACR_MapHandoff.c` | Cleans up map gadget, focus, input and view flags when you leave a body |
| `ACR/ACR_TestRange.c` | Test scenario only: starts the manager and spawns Alpha 1 |
| `ACR_Tests/ACR_TestHarness.c` | Optional automated runs (`-acrAutoTest 1`, `-acrCommandTest 1`, `-acrMapHandoffTest 1`) |

Every AI group that is not a player group and has soldiers is registered as a squad. It
belongs to its faction, and only players of that faction can possess or command it.

Docs and past validation evidence are in `docs/`.
