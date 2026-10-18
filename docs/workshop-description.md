# Arma Commander: Actual

Plan the attack. Call in support. Then grab a rifle and see how your plan survives contact.

Command squads from the map, then take over a friendly AI soldier and fight alongside them.
Capture territory across Arland, Everon and Kolguyev, solo against AI commanders or sharing
command with friends. An unofficial, openly vibecoded Reforger port of Arma 3's Arma Commander.

ACA 0.5.4 is released and requires the separate Better Battle Buddies (BBB) addon.
Pending development changes target 0.5.5; the standalone BBB split is already released.
Development 0.5.5 adds paid FOB construction supplies: empty 1,000 capacity, 500 supplies for
20 RP after 60 uncontested seconds, and construction from the FOB without a parked truck.
Built FOBs no longer refill trucks for free. Live/multiplayer acceptance remains pending.
Depots expand FOB storage to 3,000; service buildings consume its supplies. Generated settlements
send infantry replacements; military sites/HQs/FOBs deploy new formations. RP reinforcement remains
available. Startup Logistics settings let hosts choose deployment/replacement policies, free or
supplied service, depot capacity and service costs; mission makers also have header/base controls.
No faction mods are required for vanilla sides.

## Command and fight

- Command map with squad status, spotted enemies and radio reports. Several commanders can
  share a side, orders and a requisition pool.
- Move, defend, hunt, queue waypoints and garrison buildings. Advancing infantry stalled
  against a spotted contact can try a flank.
- Capture bases for requisition income; buy infantry, vehicles, mortar crews and observers.
- Bring down artillery, call combat helicopters, or try an off-map squad air lift.
- Build field service stations, forward spawn points and helipads with construction trucks.
- The Better Battle Buddies dependency improves AI combat priorities, medic behavior, vehicle doors
  and quiet marching. Expect rough edges: this is an actively developed project.

## Pick your battlefield

Everon, Arland and Kolguyev scenarios are included. Before the match, the host/admin chooses
the sides, battlefield, threat, prices, AI commanders, squad cap, starting RP and income.
Everon offers a hand-built whole-island layout, halves and flashpoints. Procedural layouts
generate bases and HQs from a terrain's named locations; preview the plan, reroll its seed,
or click a base and then its new position before pressing Start. Kolguyev always uses a
generated layout. Other terrains require a small AC: Actual scenario setup; installing a
map mod alone does not add an AC: Actual entry to the scenario list.

Mission makers can create scenarios on other terrains with generated or hand-placed bases,
custom factions and starting forces. Workbench setup and configuration guide:
https://github.com/propst-design/ArmaCommanderReforger/blob/main/MISSION-MAKING.md

On generated layouts, build a helipad before buying helicopters. Off-map air lift needs no pad.
There are no victory conditions or saved matches; play for the fight and the territory.
Off-map air lift has passed automated transport/clearance tests; broader multiplayer acceptance remains open.
Intel includes automatic enemy reports within an owned base's radius plus 60 m, without line of sight.

## Getting started and controls

Enable the mod, select an included AC: Actual scenario in Scenarios or Host, choose your
options on the setup screen, then press Start. Friends joining during setup wait for the host;
multiplayer setup and generated-layout acceptance checks are still ongoing.
The Workshop version needs no external launcher.

- M / F9: command map. Left-click a squad, right-click a destination.
- F7: possess a friendly soldier under the crosshair or map cursor.
- F8: return to your command unit.
- F6 in a vehicle: dismount passenger squads (repeat within three seconds to override warnings).
- W/A/S/D in a turret with an AI driver: gunner drive.
- Backtick while possessing a squad leader: native squad orders.

Keys can be rebound in Settings > Controls > AC: Actual. This release is tested on PC;
controller and console play have not been validated.

## Faction mods and compatibility

AC: Actual plays any two eligible factions a map lists, picked on the setup screen. Our maps list
the game's US, USSR and FIA, plus RHS US and RHS Russia when RHS is loaded, and UA through
Ukraine Armed Forces RHS. RHS and UA are optional and are the only hand-fitted faction mods.
Players joining a modded server need its mod set.

Other faction mods are not guaranteed: the map must list the faction and the mod must provide
standard Game Master catalogs. Even then, expect mixed uniforms, a squad leader as your command
unit, or vanilla stand-ins for missing equipment. ACE All in One and the All-in-One Command
Menu have been used in development; compatibility and multiplayer acceptance checks continue.
All-in-One Command Menu is a separate optional mod, not a dependency.

## Credits and licence

Original Arma Commander: Martin Hájek. Community fork: Anarch Cassius, Ilyushkius and Will.
Reforger port directed, designed and play-tested by Jacob (propst-design). Nearly all code
and documentation written by Claude (Anthropic), with code and test contributions from
Codex (OpenAI) and contributions from Antigravity (Google's agentic coding environment).
Playtesters: PoppiPoppins, Hexxyz and Frost.

Arma Public License Share Alike (APL-SA); see the included LICENSE.md. Unofficial fan project,
not affiliated with or endorsed by Bohemia Interactive or the original authors.

## Issues and feedback

Report bugs and issues to the GitHub repository: https://github.com/propst-design/ArmaCommanderReforger/issues
Please include what happened, the map and mod set, steps to reproduce it, and logs if available.
Faction support requests and feedback are welcome there too.
