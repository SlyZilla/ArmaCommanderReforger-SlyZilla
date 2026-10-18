# Changelog

## 0.5.5 — unreleased

Development changes since the released 0.5.4. Live/multiplayer acceptance remains pending.

- Generated battlefields have fewer crowded objectives and fewer passive-income sites. Home-region income is protected; tactical points still support control and deployment. Starting funding and reinforcement prices are unchanged.
- Generated HQ placement checks rear approaches and access around the opposing HQ. Approach checks now reject steep terrain and recognize roads and bridge decks over water.
- Squad health, ammunition and vehicle-status scans are spread across five seconds, reducing simultaneous update spikes while keeping each squad on its existing refresh interval.

- Generated-layout editing now updates moved CP names from nearby places, allows deleting non-HQ CPs before Start, and restores moves and deletions with Undo layout edits.
- Map markers identify CP value and hidden sites after discovery; setup previews show site type and value with a clearer selection label. Base labels stay visible without hovering.
- Opening the command map keeps its pan and zoom instead of jumping to your soldier.
- Medics and teammates deploy smoke cover toward enemy threats before advancing to treat exposed casualties.
- Cargo passengers can dismount from a slowing ground vehicle under fire and continue toward their objective. Fighting-vehicle crews remain aboard to fight.
- Helicopter air lifts check a wider landing area and rotor clearance, accelerate more gently, and slow down to climb over steep terrain.
- Fixed lift helicopters missing from collision avoidance; carriers now keep separate cruising heights during arrival, transit and departure.
- Play and Host launchers now choose only the map and optional mods; match settings are chosen in game before Start. Old saved launcher match settings no longer override the match.
- Development launchers keep local ACA and BBB ahead of installed Workshop copies while still loading the Workshop command menu.
- Improved speed-aware stalled-vehicle detection and reduced the wait after a taxi driver is lost under fire.

## 0.5.4 — released

Workshop publication confirmed by Jacob on 2026-10-17 (confirmation date, not the release date).

- Moved shared AI improvements into the required standalone Better Battle Buddies addon. Workshop installations now resolve BBB as a dependency.
- Updated installation guidance for Workshop and the separate BBB dependency. Legacy ZIP installations require the complete paired package once.

## 0.5.3 — previously published

- Published the corrected build with the return control and input handling for leaving the generated-layout preview map during match setup.

Earlier releases predate this versioned changelog; their development records remain in Git and the handoff log.
