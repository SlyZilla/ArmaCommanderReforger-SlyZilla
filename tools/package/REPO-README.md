# Arma Commander Actual (AC: Actual)

> [!IMPORTANT]
> **AC: Actual source development has moved to the [main project repository](https://github.com/propst-design/ArmaCommanderReforger).**
> Visit the main repository for current source and development documentation. Report bugs in [Discord #issues](https://discord.gg/WK8xGSztB) or the [main issue tracker](https://github.com/propst-design/ArmaCommanderReforger/issues).
> **Install and update the mod through the in-game Workshop.** This repository is retained for historical documentation, existing issues and legacy ZIP downloads/updaters. GitHub ZIPs and external launchers are a legacy/development fallback and may lag behind Workshop.

[![Game: Arma Reforger](https://img.shields.io/badge/Arma-Reforger-556b2f?style=flat-square)](https://reforger.armaplatform.com/)
[![Development stage: Alpha](https://img.shields.io/badge/Status-Alpha-yellow?style=flat-square)](#feedback)
[![BBB required by the split release](https://img.shields.io/badge/Split_release-Requires_BBB-teal?style=flat-square)](#play-through-workshop)
[![License: APL-SA](https://img.shields.io/badge/License-APL--SA-orange?style=flat-square)](LICENSE.md)

An unofficial fan port of the Arma 3 **Arma Commander** hybrid RTS/FPS game mode to Arma Reforger.

## Play through Workshop

1. Open **Workshop** in Arma Reforger and search for **Arma Commander Actual**.
2. Download and enable the mod.
3. Choose an **AC: Actual** scenario in **Scenarios** for solo play or **Multiplayer > Host** to host.
4. Pick sides and match options on the in-game setup screen, then press **Start**.

Friends join through the game's multiplayer browser. **No external launcher or ZIP download is needed.**

The current Workshop release is **ACA 0.5.6**, with **Better Battle Buddies 0.1.5**
available separately on Workshop. Allow Workshop to install BBB when prompted.
Legacy ZIP downloads may contain an older version; use Workshop for current releases.

## What you can do

- **Command and fight.** Give squads map orders, queue waypoints and hold buildings. Press F7 to take over a friendly AI soldier, then F8 to return to your commander.
- **Build your force.** Capture bases for requisition points (RP), then buy infantry and crewed vehicles. An AI commander can lead either side.
- **Bring support.** Use mortar teams, artillery, combat helicopters and off-map squad air lift.
- **Keep the front supplied.** Service trucks carry ammunition, fuel and medical supplies. Construction trucks establish forward bases, with service buildings, helipads and perimeter defences. AI commanders can build and supply FOBs too.
- **Share command.** Several players can order the same side's squads, share an RP pool and coordinate with map/3D pings.
- **Fight over what you know.** AI perception and player sight reveal contacts. Owned bases also report nearby enemies without line of sight; stale contacts fade from the map.

## Maps and setup

Everon, Arland and Kolguyev are included. Everon has a hand-built full-island layout, north/south
halves and three flashpoints; Arland also has a hand-built layout. Generated whole-map,
half-map and flashpoint layouts use the terrain's named places. Kolguyev always generates its
layout. Other terrains require an AC: Actual scenario setup; installing a map mod alone does not add
one. There are no victory conditions or saved matches.

For an installed Workshop copy, enable the mod and choose an AC: Actual scenario in Scenarios or
Host. The host/admin picks sides and match options before Start. The in-game setup map lets you
preview generated layouts, reroll the seed and move bases by clicking a base and then its new
position. The Workshop route needs no external launcher. Console/controller play remains unverified.

## Factions and optional mods

Vanilla US, USSR and FIA are available in the scenario worlds. The in-game setup screen can pick any
two eligible factions listed by the world. RHS and Ukraine Armed Forces RHS are optional and are the
only hand-fitted faction mods. UA replaces the vanilla US faction for that session. Other mods need
a faction entry in the world and standard Game Master catalogs; mixed uniforms, substitute equipment
and a squad leader as the command unit may result.

Enable any optional faction mods before starting; the setup screen selects among factions already
loaded by the scenario. Joiners need the server's mod set. ACE All in One and All-in-One Command
Menu are separate optional mods, not requirements for vanilla AC: Actual. Use
[Discord #suggestions](https://discord.gg/WK8xGSztB) or GitHub issues for faction support requests.

## Basic controls

M/F9 opens the command map: left-click a squad, then right-click its destination. F7 possesses a
living friendly AI soldier under the crosshair or map cursor; another player's soldier/command unit
is excluded. F8 returns to your command unit. F6 in a vehicle unloads passenger squads (repeat
within three seconds to override a height/speed warning). W/A/S/D from a turret enables gunner drive
with an AI driver. Backtick opens native squad orders while possessing a squad leader. Rebind mod
keys under Settings > Controls > AC: Actual.

## Feedback

Found a problem or have a suggestion? Join [Discord](https://discord.gg/WK8xGSztB):
**#issues** for bugs (choose **AC: Actual**), **#suggestions** for ideas. No GitHub account needed.
The main project's [issue tracker](https://github.com/propst-design/ArmaCommanderReforger/issues) remains available.

## Reporting bugs

Post in [Discord #issues](https://discord.gg/WK8xGSztB) or
[open a GitHub issue](https://github.com/propst-design/ArmaCommanderReforger/issues/new/choose) and include:

1. **What happened**, and what you expected instead.
2. **How to make it happen again**, if you know.
3. **Version and install route**: Workshop version, or the ZIP tag from [Releases](https://github.com/propst-design/ACActual/releases)/the launcher window.
4. **Host or client**, and how many players were on.
5. **Map and mod set**, including faction choices.
6. **Logs**: the newest folder in `Documents\My Games\ArmaReforger\logs` (zip it). For a ZIP-launcher host, also include `ArmaCommanderReforger\.local-server\logs`; for another dedicated server, use its configured profile log folder. Lines with `[ACR]` or `SCRIPT (E)` are the useful ones.

Screenshots of the map help too.

## Made with AI

This project is openly vibecoded. Nearly all of the code, launchers and documentation were written
by **Claude** (Anthropic's AI model, working in Claude Cowork), directed, designed and play-tested
by **Jacob** ([propst-design](https://github.com/propst-design)). Codex (OpenAI) contributes code
and tests; Antigravity (Google's agentic coding environment) also contributes. Expect rough edges;
bug reports with logs help a lot.

Playtesters: **PoppiPoppins**, **Hexxyz** and **Frost**.

## Making scenarios

Want AC: Actual on another map or in your own scenario? See [MISSION-MAKING.md](MISSION-MAKING.md).

## Credits and license

Based on **Arma Commander** for Arma 3 by **Martin Hájek**
([original project](https://gitlab.com/silliaris/arma-commander)), ported from the community
fork by **Anarch Cassius**, **Ilyushkius** and **Will**. Rebuilt for Arma Reforger with new features; see
[LICENSE.md](LICENSE.md) for what changed.

Licensed under the [Arma Public License Share Alike (APL-SA)](https://www.bohemia.net/community/licenses/arma-public-license-share-alike),
the same license as the original: attribution, noncommercial, share alike, Arma games only.
Provided as-is, without warranty.

## Legacy ZIP / development launchers

Workshop is the normal way to install and play. The [ZIP download](https://github.com/propst-design/ACActual/releases/latest/download/ACActual-WithBBB.zip)
remains a Windows Steam fallback for local/development use, covering Arland, Everon and
Kolguyev. Read its **READ ME FIRST.txt** for Play/Host/Join instructions.
These launchers choose only the map and optional mods before loading. Choose playable sides and
match settings on the in-game startup screen, then press **Start**. Host connects first to control
setup on its separate local server; that access does not transfer after disconnect. Published ZIPs
self-update from GitHub. ZIP launcher users may need to adjust addon search paths to match their installation.
The launchers do not download optional mods. Locally detected Workshop
copies of All-in-One Command Menu are automatically loaded, so joiners need that mod too.

The paired ZIP contains both ArmaCommanderReforger and BetterBattleBuddies. Older single-addon ZIP users must download and extract it once; their updater cannot install BBB. Keep both folders together.
