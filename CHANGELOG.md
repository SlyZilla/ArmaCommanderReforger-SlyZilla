# Changelog

## 0.5.5 — unreleased

- Added a dedicated Workshop invitation for mission makers, linking the setup and sharing guide on GitHub.
Development changes since the released 0.5.4. Outstanding live checks: air-lift clearance and RTS camera; the other existing checks were dismissed by Jacob.

- Heavy maintenance bays repair 40% more per service step than light bays, with the same supply cost, range and interval. Completed small/large living quarters inside a friendly FOB add 11/19 garrison defenders, proportional to their 275/475-supply construction costs. Multiple quarters stack; casualties persist, and losing quarters reduces garrison capacity.

- Built FOBs now require a completed friendly radio relay inside their circle for new-unit deployment and RP reinforcement, including AI purchases. Removing the relay disables both until rebuilt; construction, deliveries and supply-paid services continue. Mission makers and hosts can disable the requirement through the mission header, command line or startup Logistics settings. Captured military bases and village reinforcement retain their existing rules.

- Fixed paid FOB deliveries filling a separate store while the CP stayed empty. Construction and services now use the CP's native supply container; its engineering table opens the funded build menu with service buildings and supply depots available. Paid stock remains unavailable for truck loading.

- Rare requisitions now scale their limited stock with threat even under stepped pricing: High quadruples rare stock, including four BTRs instead of one. Low reduces larger allocations with a minimum of one; rare-unit prices remain fixed.

- Supply depots expand FOB storage from 1,000 to 3,000 supplies, with one bonus per FOB. FOB hospital/ammo/repair/fuel buildings now consume local supplies; empty or contested FOBs cannot provide automatic service. RP reinforcement and instant vehicle service remain available, with a configurable RP fee for instant infantry service at FOBs.
- Generated settlements can send infantry replacements, while new squads and vehicles deploy from HQs, military sites/airfields and FOBs. Authored missions retain existing deployment behavior unless configured otherwise. Added mission-maker controls and a startup Logistics page for deployment, infantry replacements, supplied/free services, depot capacity and service costs.

- Reinforcement results now appear directly in the command-map panel for 12 seconds, including refusals caused by enemy troops within 300 m. A short burst of squad results is shown together instead of immediately replacing the first refusal.

- Built FOBs start with an empty 1,000-supply construction store. Commanders can order 500 supplies for 20 RP with a 60-second delay that pauses while contested, then build from the FOB without keeping a construction truck there. Built FOBs no longer refill service trucks for free.

- Generated battlefields have fewer crowded objectives and fewer passive-income sites. Home-region income is protected; tactical points still support control and deployment. Starting funding and reinforcement prices are unchanged.
- Generated HQ placement checks rear approaches and access around the opposing HQ. Approach checks now reject steep terrain and recognize roads and bridge decks over water.
- Squad health, ammunition and vehicle-status scans are spread across five seconds, reducing simultaneous update spikes while keeping each squad on its existing refresh interval.

- Generated-layout editing now updates moved CP names from nearby places, allows deleting non-HQ CPs before Start, and restores moves and deletions with Undo layout edits.
- Map markers keep concise names, CP value and contested warnings visible; hover reveals full site details, including hidden status, in gameplay and setup previews. Fixed duplicate marker widgets and labels briefly appearing at the map corner.
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
