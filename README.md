# ACA Field Engineering 0.1.1 (add-on source)

A standalone, script-only add-on for **Arma Commander: Actual** as published on the Workshop
(ACA 0.5.6 with Better Battle Buddies 0.1.5, Arma Reforger 1.8.0.13). This branch has no ACA
history: it holds only the add-on, in `ACA_FieldEngineering/`, shared so its roadblocks and
checkpoints can be compared with ACA's own `ACR_Roadblocks`.

It gives the commander map-panel orders for infantry squads:

- a roadblock (vanilla barricade) and small / medium / large checkpoints (vanilla slotted road
  compositions, US or USSR by side), snapped to the road centre and turned along it;
- AP / AT / mixed minefields and mine clearing.

Orders are paid in RP or construction truck supplies. The work is timed, pauses while the site is
contested, runs faster with engineers and is refunded when called off. The add-on adds a
per-match settings page on ACA's setup screen and can let the AI commander use the orders.

The minefields part is already ported into ACA natively, as a separate draft pull request:
https://github.com/propst-design/ArmaCommanderReforger/pull/3

## Run it

See [`ACA_FieldEngineering/README.md`](ACA_FieldEngineering/README.md). In short: subscribe to
ACA and BBB on the Workshop, start the game once, then run `Play-ACA-FieldEngineering.cmd`. The
launcher loads this folder unpacked next to the Workshop copies.

## Roadblock files

In `ACA_FieldEngineering/Scripts/Game/ACAFE/`:

- `ACAFE_Roads.c`: snapping a click to the nearest road's centre line, its heading and width,
  and off-road distance.
- `ACAFE_Orders.c`: the orders, prices and timers, the vanilla prefab each side builds, and the
  layout of a road structure across the road.
- `ACAFE_Jobs.c`: the job from order to finish: the walk, timed work, contest pause, refunds,
  and the site checks before spawning (vehicles on the spot, a structure in the way).
- `ACAFE_AI.c`: the AI commander's use of the orders (off by default).

`ACAFE_Hooks.c` holds the few `modded class` hooks into ACA. All tunables are in
`ACAFE_Config.c`, and the setup-screen options are in `ACAFE_Settings.c`.

## License

This add-on extends AC: Actual. It is shared under the same terms as the code it extends: the
**Arma Public License Share Alike (APL-SA)**
(https://www.bohemia.net/community/licenses/arma-public-license-share-alike). AC: Actual's
attribution is retained: it is a port of **Arma Commander** for Arma 3 by Martin Hájek, by way of
the "Arma Commander Fork" by Anarch Cassius, Ilyushkius and Will. See AC: Actual's
[LICENSE.md](https://github.com/propst-design/ArmaCommanderReforger/blob/main/LICENSE.md).
