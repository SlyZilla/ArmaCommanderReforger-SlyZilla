# Arma Commander Actual (AC: Actual)

An unofficial fan port of the Arma 3 **Arma Commander** hybrid RTS/FPS game mode to Arma Reforger.

**Download:** [ACActual-Arland.zip](https://github.com/propst-design/ACActual/releases/latest/download/ACActual-Arland.zip).
Unzip it and open "READ ME FIRST.txt". The Host and Join launchers update themselves from this page.

## Help test it

Multiplayer has had very little testing. If you play it with friends, these are the things we most want to hear about, working or not:

- **Shared commanding** (two or more commanders on a side): each commander's orders show on the others' maps as dashed lines with a name tag, the squad panel says who gave an order, and re-tasking an order under 30 s old asks first.
- **Pings**: Alt+click on the map pings "look here"; Alt+right-click opens a picker for Look, Attack, Defend and Support (click, or keys 1 to 4). Pings show on your side's HUD with a distance. The 3D ping key starts unbound under Controls.
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
3. **Version**: the vX at the top of [Releases](https://github.com/propst-design/ACActual/releases), or the launcher window.
4. **Host or client**, and how many players were on.
5. **Logs**: the newest folder in `Documents\My Games\ArmaReforger\logs` (zip it). If you hosted, also the newest folder in `ArmaCommanderReforger\.local-server\logs`. Lines with `[ACR]` or `SCRIPT (E)` are the useful ones.

Screenshots of the map help too.

## Made with AI

This project is openly vibecoded. Nearly all of the code, launchers and documentation
were written by **Claude** (Anthropic's AI model, working in Claude Cowork), directed,
designed and play-tested by **Jacob** ([propst-design](https://github.com/propst-design)).
Expect rough edges; bug reports with logs help a lot.

Playtesters: **PoppiPoppins** and **Hexxyz**.

## Credits and license

Based on **Arma Commander** for Arma 3 by **Martin Hájek**
([original project](https://gitlab.com/silliaris/arma-commander)), ported from the community
fork by **Anarch Cassius**, **Ilyushkius** and **Will**. Rebuilt for Arma Reforger with new features; see
[LICENSE.md](LICENSE.md) for what changed.

Licensed under the [Arma Public License Share Alike (APL-SA)](https://www.bohemia.net/community/licenses/arma-public-license-share-alike),
the same license as the original: attribution, noncommercial, share alike, Arma games only.
Provided as-is, without warranty.
