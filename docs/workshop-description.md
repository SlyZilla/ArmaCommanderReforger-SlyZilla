# Arma Commander: Actual

Plan the attack. Call in support. Then grab a rifle and see how your plan survives contact.

Command squads from the map, then take over a friendly AI soldier and fight alongside them.
Capture territory across Arland, Everon and Kolguyev, solo against AI commanders or sharing
command with friends. An unofficial, openly vibecoded Reforger port of Arma 3's Arma Commander.
Requires the separate Better Battle Buddies (BBB) addon. No faction mods are needed for vanilla sides.

## In 0.5.5

- FOBs start with empty 1,000-supply stores. Order 500 supplies for 20 RP, delivered after 60
  uncontested seconds. Build from the CP without keeping a construction truck there.
- A completed friendly radio relay enables FOB deployment and RP reinforcement. Losing it disables
  both until rebuilt; construction, deliveries and supply-paid services continue.
- One depot expands storage to 3,000. Hospital, ammo, maintenance and fuel buildings consume local
  supplies. Paid stock cannot be loaded into trucks; built FOBs do not refill trucks for free.
- Generated settlements send infantry replacements; military sites, HQs and relay-equipped FOBs
  deploy new formations. RP reinforcement remains available at eligible sites.
- Startup Logistics settings control deployment, replacements, the relay requirement, service
  payment, storage and costs. Mission makers also have header and per-base controls.
- Clearer reinforcement refusal messages, compact map captions with details on hover, and improved
  generated layouts, terrain access, rare-unit stock and helicopter clearance.

## Command and fight

- Shared command map with squad status, spotted enemies and radio reports.
- Move, defend, hunt, queue waypoints and garrison buildings. Stalled advancing infantry can flank.
- Capture bases for RP; buy infantry, vehicles, mortar crews and observers.
- Call artillery, combat helicopters and off-map squad air lifts.
- Build service stations, FOBs and helipads with construction trucks.
- BBB improves AI combat priorities, medics, vehicle behavior and quiet marching.

## Pick your battlefield

Choose an AC: Actual scenario in Scenarios or Host. Before Start, the host/admin selects sides,
battlefield, threat, prices, AI commanders, caps, starting RP, income and logistics options.
Friends joining during setup wait for the host. No external launcher is needed.

Everon includes a hand-built island layout, halves and flashpoints. Procedural layouts generate
bases and HQs from named locations: preview, reroll the seed, or move bases before Start.
Kolguyev always uses a generated layout. Other terrains need an AC: Actual scenario setup;
installing a map mod alone does not add a scenario. Mission-maker guide:
https://github.com/propst-design/ArmaCommanderReforger/blob/main/MISSION-MAKING.md

On generated layouts, build a helipad before buying helicopters. Off-map air lift needs no pad.
There are no victory conditions or saved matches. Intel includes automatic enemy reports within
an owned base's radius plus 60 m, without line of sight. Air-lift clearance remains a live-check item.

## Controls

- M / F9: command map. Left-click a squad, right-click a destination.
- F7: possess a friendly soldier under the crosshair or map cursor.
- F8: return to your command unit.
- F6 in a vehicle: dismount passengers; repeat within three seconds to override warnings.
- W/A/S/D in a turret with an AI driver: gunner drive.
- Backtick while possessing a squad leader: native squad orders.

Rebind keys in Settings > Controls > AC: Actual. Tested on PC; console/controller play is unverified.

## Factions and compatibility

Our maps list US, USSR and FIA, RHS US/Russia when RHS is loaded, and UA through Ukraine Armed
Forces RHS. RHS and UA are optional, hand-fitted faction mods. Joiners need the server's mod set.
Other factions need a map entry and standard Game Master catalogs; expect mixed kit or fallbacks.
ACE All in One and the optional All-in-One Command Menu have been used in development.
Expect rough edges.

## Credits and licence

Original Arma Commander: Martin Hájek. Community fork: Anarch Cassius, Ilyushkius and Will.
Reforger port directed, designed and play-tested by Jacob (propst-design). Nearly all code and
documentation by Claude (Anthropic), with code/test contributions from Codex (OpenAI) and
contributions from Antigravity (Google). Playtesters: PoppiPoppins, Hexxyz and Frost.

Arma Public License Share Alike (APL-SA); see LICENSE.md. Unofficial fan project, not affiliated
with or endorsed by Bohemia Interactive or the original authors.

## Issues and feedback

https://github.com/propst-design/ArmaCommanderReforger/issues
Include the map, mod set, what happened, reproduction steps and logs. Faction requests welcome.
