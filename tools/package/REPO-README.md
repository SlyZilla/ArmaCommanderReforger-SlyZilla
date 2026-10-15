# Arma Commander Actual (AC: Actual)

An unofficial fan port of the Arma 3 **Arma Commander** hybrid RTS/FPS game mode to Arma Reforger.

## Play through Workshop

1. Open **Workshop** in Arma Reforger and search for **Arma Commander Actual**.
2. Download and enable the mod.
3. Choose an **AC: Actual** scenario in **Scenarios** for solo play or **Multiplayer > Host** to host.
4. Pick sides and match options on the in-game setup screen, then press **Start**.

Friends join through the game's multiplayer browser. **No external launcher or ZIP download is needed.** Vanilla factions need no additional mods.

## Features

- **Command from the map.** Select squads (click, Shift+click or Ctrl+drag), then give move, defend, garrison, get in/out, ride and hunt orders, chaining up to 8 waypoints. Each squad shows health and ammo bars.
- **Jump into a friendly AI soldier** (F7), and back to your commander (F8).
- **Requisition.** Bases you own earn RP. Spend it on infantry squads and crewed vehicles, deployed at an owned base that allows spawning and is not contested.
- **Supports.** Buy mortar teams for HE, creeping barrage, smoke and illumination within range. Call a virtual howitzer battery, rocket/bomb runs, helicopter combat support, or an off-map air lift for a squad. Support helicopters can be shot down; on layouts with pads, buy and station them first.
- **Bases.** Capture a base by clearing it and holding its ring. Garrisons appear when you get close, and some enemy bases stay hidden until you find them.
- **An AI commander** for either side. It buys units, keeps a reserve, attacks from several directions with smoke and mortars, counterattacks and defends.
- **Intel.** AI perception and player line of sight reveal enemies; owned bases also automatically report enemies inside their radius plus 60 m, without line of sight. Contacts become suspected after 30 s and expire after 90 s without updates. The AI commander plans from its side's intel.
- **Logistics.** Ammo, repair, fuel and medical trucks resupply nearby units, carry limited stock and restock at your bases. Players can use arsenal, supply and construction trucks too.
- **Vehicles.** Rides by truck or helicopter to a landing zone, road-following AI drivers, and gunner drive (drive and steer from a turret with an AI driver aboard).
- **Multiplayer.** Host and join through Reforger. The host/admin chooses sides and match settings in game. Several players can command the same side together, with tagged orders, map and 3D pings, and one shared RP pool.
- **Construction.** Player construction trucks build service stations, helipads and forward deploy points. AI base building and a construction-supply pool at command posts are not implemented.
- **Game Master** for a listen host or configured server admin, to unstick vehicles and fix things.

## Maps and setup

Everon, Arland and Kolguyev are included. Everon has a hand-built full-island layout, north/south
halves and three flashpoints; Arland is also the main automated test map. Generated whole-map,
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
Menu are separate optional mods, not requirements for vanilla AC: Actual. Use GitHub issues for
faction support requests.

## Basic controls

M/F9 opens the command map: left-click a squad, then right-click its destination. F7 possesses a
living friendly AI soldier under the crosshair or map cursor; another player's soldier/command unit
is excluded. F8 returns to your command unit. F6 in a vehicle unloads passenger squads (repeat
within three seconds to override a height/speed warning). W/A/S/D from a turret enables gunner drive
with an AI driver. Backtick opens native squad orders while possessing a squad leader. Rebind mod
keys under Settings > Controls > AC: Actual.

## Help test it

Multiplayer is playable and has had friend playtests; coverage is still limited, especially for new
setup and generated-layout features. If you play it with friends, these are the things we most want
to hear about, working or not:

- **Shared commanding** (two or more commanders on a side): each commander's orders show on the others' maps as dashed lines with a name tag, the squad panel says who gave an order, and re-tasking an order under 30 s old asks first.
- **Pings**: Alt+click on the map pings "look here"; Alt+right-click opens a picker for Look, Attack, Defend and Support (click, or keys 1 to 4). Pings show on your side's HUD with a distance. The 3D ping key starts unbound under Controls > AC: Actual.
- **Support pings**: clicking a Support ping aims your next support call (mortar, helicopter, airstrike) at it.
- **Feed and roster**: RP spending and commanders joining or leaving show in the feed, and the command panel lists the side's commanders.
- **Service trucks**: arsenal, ammo, repair and construction trucks used by players and AI squads drain the same supplies; the construction truck can build.
- **Gunner drive**: a player gunner driving the vehicle with the AI driver; it should be smooth with sensible gear changes.
- **Intel feed**: spotted enemies appear on the map by type and fade after about 90 s.
- **Joining and sides**: players land on the side the host chose, and late joiners get a working map.
- **Long games**: anything that gets stuck, slows down or stops working after an hour or more.

## Reporting bugs

[Open an issue](https://github.com/propst-design/ACActual/issues/new/choose) and include:

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

Workshop is the normal way to install and play. The [ZIP download](https://github.com/propst-design/ACActual/releases/latest/download/ACActual-Arland.zip)
remains a Windows Steam fallback for local/development use. Its historical filename includes
Arland, Everon and Kolguyev. Read its **READ ME FIRST.txt** for Play/Host/Join instructions.
These launchers choose options outside the game and skip the in-game setup screen; published ZIPs
self-update from GitHub. Optional faction mod paths are currently configured for the development
PC (`E:\ArmaReforgerMods\addons`). They do not download those mods. Locally detected Workshop
copies of All-in-One Command Menu are automatically loaded, so joiners need that mod too.
