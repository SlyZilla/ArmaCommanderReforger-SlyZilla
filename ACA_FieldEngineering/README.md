# ACA Field Engineering

A script-only add-on for **Arma Commander: Actual** (ACA 0.5.6, needs Better Battle Buddies 0.1.5).
From the ACA command map you can order AI squads to build roadblocks and checkpoints, lay
minefields and clear mines. It works like ACA's own *Establish FOB* and *FOB Defences*: you pay up
front, the squad travels to the site, works for a while (paused when the site is contested,
twice as fast with engineers), and the vanilla composition or mines appear.

Version 0.1.1, game 1.8.0.13. Addon GUID `F4EF6A0A51B808A8`. Loaded unpacked from this folder
(no Workbench needed).

## For testers

**Requirements:** Arma Reforger 1.8 on PC (Steam), and from the in-game Workshop **Arma Commander:
Actual 0.5.6** and **Better Battle Buddies 0.1.5** (subscribe, then start the game once so they
download). Nothing else.

**Install:** unzip anywhere (the Desktop is fine) and keep the `ACA_FieldEngineering` folder as it
is. Double-click **`Play-ACA-FieldEngineering.cmd`** and pick a map; it finds Arma Reforger through
Steam. If it can't, run `Start-FieldEngineering.ps1 -GamePath "<path to ArmaReforgerSteam.exe>"`
from PowerShell.

ACA developers: put the folder next to your `ArmaCommanderReforger` and `BetterBattleBuddies`
clones (e.g. the Workbench addons folder) and the launcher uses those instead of the Workshop
copies. It never mounts both copies of ACA (same GUID).

**What to try:**
1. Take command, select a squad, open **Field engineering** under its Orders, pick **Checkpoint
   Medium**, right-click near a road. Watch it travel, work (label "Building medium checkpoint
   (45%)") and build the checkpoint across the road.
2. **Roadblock** on a narrow and on a wide road (the barricade size follows the road width).
3. **Minefield AP / AT / Mixed**, then walk or drive an enemy into it; your own AI walk through
   unharmed by default. **Clear mines** lifts them again.
4. Give a working squad another order: the job is cancelled and refunded.
5. On the setup screen, **Field engineering settings...** (bottom): try the **realistic** preset
   (engineers only, truck materials, mines from kit, no friendly safety).
6. Launcher option **5**: the enemy AI commander builds and mines often, shown on your map.

**Logs:** `Documents\My Games\ArmaReforger\logs\logs_<date>\console.log`. Search for `[ACAFE]`:
orders, refusals with the reason, arrivals, progress events, completions, refunds, the AI's
decisions. Script errors show as `SCRIPT (E)`.

**Tested so far** (single player, Kolguyev and the ACA test range): all player orders and the
cancel/refund, the enemy AI commander (8 jobs), the settings page. The realistic rules compile and
switch correctly, but orders under them haven't been played yet.

## Starting the game

Double-click **`Play-ACA-FieldEngineering.cmd`** and pick a map. It starts Reforger with ACA, BBB
and this addon and loads the map straight away, then ACA's setup screen appears as usual.

Inside a game started by the launcher you can also go back to the main menu and pick another ACA
scenario: the addon stays loaded. A game started normally from Steam doesn't have it (it is not a
Workshop mod). To add more Workshop mods (RHS...), run
`Start-FieldEngineering.ps1 -Extra "<GUID>,<GUID>"`.

Option **5** is the enemy AI test: Kolguyev with `-acafeAITest` (see below).

## Orders (command map, a friendly squad selected, while you are commander)

Under the squad's **Orders** there is a **Field engineering** button. Click it to open the list:

| Button | What the squad does |
|---|---|
| Checkpoint **Small / Medium / Large** | Builds the vanilla checkpoint on the nearest road (within 60 m of your click), turned along the road. |
| **Roadblock** | Builds a barricade across the nearest road, sized to the road. |
| Minefield **AP** | Lays 30 anti-personnel mines in a 30 m square, off the road. |
| Minefield **AT** | Lays 12 anti-tank mines on the road through the area (anywhere if there is no road). |
| Minefield **Mixed** | 8 AT mines on the road and 20 AP mines round it. |
| **Clear mines** | Sweeps 25 m round the click and lifts every armed mine it finds (anyone's). |

Pick an order (its button shows in `[brackets]`), then **right-click the map**. Click the same
button again to cancel. Only the first selected squad gets the order.

- Faction look: US for the west / RHS US, USSR for the east / RHS AFRF, log barricades for FIA.
  Mines: M14 / M15 for the west, PMN-4 / TM-62M for the east and FIA.
- The squad drives if it is a vehicle crew, asks ACA for a ride when it is on foot and 600 m+ away,
  otherwise walks. Its map label shows the progress (`Building medium checkpoint (45%)`,
  `Laying AP minefield (12/30)`).
- Work counts while any soldier of the squad is within 40 m of the site, stops while enemies are
  near (contested), and is twice as fast for sapper/engineer squads or with a construction truck.
- Giving the squad any other order cancels the job and refunds it. So do 3 minutes without
  progress, or failing to get there. A half-laid minefield stays and is not refunded.
- When done, your squad stays and holds the site (Defend). Mine clearing just holds.
- Your command map shows your minefields as red danger rings ("Minefield AP (30)"), work sites
  and what you built as pale rings.

### Paying (standard settings)

A **construction truck** within 50 m of the squad pays in supplies (vanilla prices: checkpoint
75 / 225 / 350, roadblock 40, minefields 300 / 400 / 500). Otherwise the side's **RP** pays:
roadblock 6, checkpoints 8 / 15 / 22, minefields 15 / 20 / 25, mine clearing 3.

### Friendly fire (standard settings)

Your own **AI** never set off your own mines (they move through as if they knew the lanes).
**You** still do, also in a possessed soldier or a vehicle you drive: the field is marked on your
map and with minefield signs at its corners. Enemy mines are hidden and real for everyone.

## Match settings (ACA's setup screen)

On ACA's match setup screen (before Start) click **Field engineering settings...** at the bottom.
Each row cycles when clicked:

| Row | Choices |
|---|---|
| Preset | **standard** / **realistic** (sets the next four rules at once; any change shows "custom") |
| Field engineering | on / off (off hides the panel buttons) |
| Builders | any squad / **engineers or truck** (engineers and sappers only, or a squad with a construction truck) |
| Roadblock materials | RP or truck supplies / **truck supplies only** (roadblocks and checkpoints need construction truck supplies) |
| Mines | RP or truck supplies / **soldiers' kit, then truck** (the mines the soldiers carry, the rest from a construction truck) |
| Own mines | safe for our AI / safe for our side / **dangerous to all** |
| Costs | normal / half / double / free |
| Work time | normal / half / double |
| Enemy AI commander uses it | off / on |

**Realistic** = the four bold choices. Mines from kit: each mine a soldier carries is taken out of
his kit as it goes in the ground; the rest come off a construction truck at 10 supplies per AP and
30 per AT mine. Note the vanilla loadouts: **FIA sappers carry 5 PMN-4 each, US and USSR sappers
carry demolition charges, not mines**, so US/USSR need a construction truck (or soldiers you gave
mines to). Fewer mines make a smaller, quicker field. The enemy AI follows the same rules (it then
sends only engineer squads, and only lays what it has).

The same settings work as launch options (`Start-FieldEngineering.ps1 -Options "-acafeMode realistic"`):
`-acafeMode realistic`, `-acafe 0`, `-acafeBuilders engineers`, `-acafeMaterials truck`,
`-acafeMines kit`, `-acafeOwnMines ai|side|real`, `-acafeCost 0.5`, `-acafeTime 2`, `-acafeAI 1`.
Without a setup screen (the test range) they come from these options, then the config defaults.

## Tuning

Everything else is in **`Scripts/Game/ACAFE/ACAFE_Config.c`**: the standard preset's defaults,
costs, timers, engineer speed, give-up time, road snap distance, minefield size and mine counts,
supplies per kit-mode truck mine, signs. Edit, save, start the game again (scripts recompile on
every load).

- Enemy AI use: a checkpoint on the road from a frontline base towards the enemy, AP mines on the
  enemy side of its FOBs, paying RP and keeping `AI_RP_RESERVE` for troops, about every 3 minutes.
- `-acafeAITest` (launcher option 5): the AI commander considers it every 30 s and always acts,
  keeps no RP reserve, mines round a frontline base when it has no FOB yet, logs why it didn't act
  (`[ACAFE] AI ...`), and its work sites, checkpoints and minefields show on your map as
  "AI USSR ..." rings. For testing only: it gives away enemy positions.
- `ROAD_YAW_OFFSET`: if checkpoints ever stand along the road instead of across it, set 90.
- `HOLD_AFTER_BUILD = false`: player squads go back to plain holding instead.

## Logs

`Documents\My Games\ArmaReforger\logs\logs_<date>\console.log`: script compile errors show as
`SCRIPT (E)` lines; this addon logs `[ACAFE]` lines (orders, refusals, arrivals, completions,
refunds), next to ACA's `[ACR]` lines.

## Files

- `ACAFE_Config.c` all tunables (and the standard preset)
- `ACAFE_Settings.c` per-match settings: setup screen rows, presets, launch options
- `ACAFE_Kit.c` mines carried in kit, construction truck supplies
- `ACAFE_Orders.c` order names, prices, timers, faction prefabs
- `ACAFE_Jobs.c` the jobs: planning, payment, travel, work, completion
- `ACAFE_Mines.c` minefields, signs, friendly-fire rule, clearing
- `ACAFE_Roads.c` road snapping
- `ACAFE_Panel.c` the command panel buttons and the map right-click
- `ACAFE_Hooks.c` the three small hooks into ACA and the game
- `ACAFE_AI.c` optional AI commander use

Hooks into ACA (kept small; ACA is alpha): `ACR_Manager.Tick`, `ACR_Mortars.Ranges` (map rings),
`ACR_CommandPanel.Create/Destroy`, `ACR_SetupDialog.Fill/OnRow` (settings page), `SCR_PlayerController.ACR_SendSetup / ACR_OnPanelAction /
ACR_HandleMapRightClick / ACR_UpdatePanel`, `ACR_AICommander.Tick`; in the game:
`SCR_PressureTriggerComponent.EOnContact` (friendly mines).
