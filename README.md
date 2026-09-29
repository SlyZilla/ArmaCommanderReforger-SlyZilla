# Arma Commander: pre-made test range

Close Arma Reforger first, then double-click **Play-Test-Range.cmd** in this folder.
Steam and the installed PC version of Arma Reforger are required. No Workbench or
manual scenario assembly is needed. Allow time for Arland and the squad to load.
This launcher uses offline mode and a separate local game profile.

You spawn as a US soldier near a friendly four-person squad on Arland.

| Key | Action |
| --- | --- |
| F5 | On foot: advance to the test destination. On map: move squad to cursor |
| F6 | Rally the squad to its starting position |
| F7 | Possess the soldier under your crosshair, or the marker under the map cursor |
| F8 | Return to the original soldier, even after several swaps |
| F9 | Open/close the command map, without needing a map item |

Aim directly at a living, unoccupied member of the test squad within 250 metres
and press F7. You can do this again while possessing a soldier: there is no need
to return first. Walls/terrain block the aiming ray. Enemies, occupied soldiers
and soldiers outside the test squad are rejected by the server.

On the command map, click a squad marker to possess that soldier, or hover its
marker and press F7. Blue markers identify available squad soldiers, amber marks
player-controlled soldiers, and grey marks dead soldiers. The green ORIGINAL
marker returns you to your original body. The map starts centred on your current
soldier; use normal map zoom/pan controls when markers overlap.

Map positions update from the server once per second, with smoother positioning
for soldiers already loaded locally. Point at a destination and press F5 to order
the entire test squad there. This prototype limits orders to the test area
(Arland coordinates X/Z 1400-3400). It has no squad-selection interface yet.

Possess the **Alpha 1 leader** to use Reforger's native squad command menu
(default: hold the grave/backtick key below Esc, or your configured command-menu
binding). Orders go to the spawned fireteam. Swapping to a rifleman or returning
to your original soldier releases that leadership and restores your prior player
group. Only that leader can open the squad command menu; swapping bodies closes
an open menu. While possessed, the leader is separated from the AI movement
formation so the other soldiers can carry out orders. The four-person roster and
leader marker remain intact, and the leader rejoins on release.

Friendly group leaders now have named military-symbol markers, including AI and
player leaders. Alpha 1 uses its spawned infantry fireteam symbol; other groups
use their authored symbol, with an infantry squad fallback. Leader markers show
AI/player status and replace the duplicate numbered soldier marker. Clicking
Alpha 1 selects its leader. Other friendly groups are visible, but possession
is still restricted to the four-person test squad.

Swapping or returning from the map now stows the outgoing soldier's physical map
and clears gadget focus and retained look state. The command map stays open for
continued selection.

The range is unopposed; death and disconnect recovery have not been validated.

## Trying multiplayer

**Host-Test-Range.cmd** opens the normal online game with the addon loaded.
Use the game's hosting menu and select **Arma Commander - Test Range**. A second
player must have this same project folder and launch with Host-Test-Range.cmd
before joining. Keep the entire folder together when copying it. This development
addon is not published to Workshop, so automatic download is not provided.

The mission header specifies four players. The hosting menu, remote joins and
remote command/possession behavior remain unverified. All US players currently
share the one test squad; there are no commander roles yet. The Play launcher is
for a solo test and does not start an online listen server.

## What this checkpoint includes

A pre-made world, automatic player spawning, vanilla AI navigation, aimed and chained soldier control, live map markers,
map movement orders, and server-side command validation.
The scenario ran in the installed regular game, version 1.8.0.13. Automated runtime
checks verified movement, possession, return and rally. See VALIDATION.md.

This is an interaction prototype, not the full Arma Commander port. Purchases,
territorial income, the AI commander and the full RTS interface remain to be implemented;
they remain required for the intended full experience.

## Troubleshooting

- If Reforger is already running, close it before starting either launcher.
- Wait for the spawn and initial squad creation before pressing command keys.
- On keyboards with media function keys, use Fn with F5-F9 if required.
- Runtime logs are in .local-game/logs inside this folder.
- If automatic game discovery fails, run Start-Test-Range.ps1 with -GamePath
  pointing to your ArmaReforgerSteam.exe.
- Open-Workbench.ps1 is optional and intended for development.

No original Arma 3 scripts or game assets are bundled. The project uses assets
from your installed Reforger game. Do not treat this development package as a
finished public release.

Input fix: if you ran an earlier version and could only use F5-F8, fully close the
game and relaunch this updated folder. The test keys now allow normal movement,
mouse look and weapon controls to remain active.

Movement-state handling now clears movement input on swaps and map/menu transitions,
and command shortcuts no longer outrank inventory/dialog controls. The reported
continued walking after key release has not been independently reproduced, so
this is a targeted mitigation rather than a confirmed complete fix. To check it,
walk and release W, then repeat after F7/F8, opening/closing F9, and inventory.

Shortcut update: F7-F9 now activate independently of the controlled soldier's
movement context. After a swap, the confirmation names the unit (US 1-4), matching
its map label. Aim with the centre of your view; aiming down sights makes this
clearer. No eligible hit means no swap, rather than choosing another squad member.
