# Changelog

## 0.5.5 — unreleased

### FOBs and logistics

- Built FOBs start with an empty 1,000-supply store. Order 500 supplies for 20 RP, delivered after 60 uncontested seconds, then build from the FOB without keeping a construction truck nearby.
- A completed friendly radio relay enables FOB unit deployment and reinforcement. Losing it disables both until rebuilt; building, deliveries and supply-paid services can continue.
- Supply depots increase FOB storage to 3,000, with one storage bonus per FOB. Hospitals, ammo stores, maintenance bays and fuel buildings use local supplies and pause when the FOB is empty or contested. FOB supplies cannot be loaded into trucks, and FOBs no longer refill service trucks for free. Instant vehicle service still costs RP; instant infantry service at FOBs has a configurable RP fee.
- Fixed paid deliveries failing to reach the FOB's construction store. The engineering table now opens the funded build menu with service buildings and supply depots available.
- Heavy maintenance bays repair 40% more per service step at the same supply cost. Small and large living quarters add 11 and 19 garrison defenders; multiple quarters stack, casualties persist, and losing quarters reduces capacity.
- On generated battlefields, settlements provide infantry replacements; HQs, military sites, airfields and eligible FOBs deploy new squads and vehicles. The new Logistics setup page lets hosts adjust deployment, replacement and service rules. Mission makers also have header and per-base controls.

### Battlefields and requisitions

- Generated layouts have fewer crowded objectives and passive-income sites, while protecting home-region income. HQ placement better accounts for rear access, steep terrain, roads and bridges.
- Rare-unit stock now scales correctly with threat under stepped pricing. High threat provides four times the stock; rare-unit prices stay fixed.

### Command map

- Reinforcement results and refusal reasons remain visible in the command panel for 12 seconds, with several recent results shown together.
- Map labels are shorter, with full site details available on hover. Fixed duplicate markers and labels appearing briefly in the map corner.
- Opening the command map preserves its pan and zoom.
- Before Start, moved bases take nearby place names, non-HQ bases can be deleted, and Undo layout edits restores moves and deletions.

### AI and transport

- Medics and teammates use smoke cover before approaching exposed casualties. Passengers can dismount from a slowing vehicle under fire while fighting crews stay aboard.
- Improved helicopter air-lift landing clearance, acceleration and climbs over steep terrain. Lift helicopters now participate in collision avoidance and use separate cruising heights.
- Improved stalled-vehicle detection and reduced the wait after a taxi driver is lost under fire.
- Spread squad status updates over time to reduce simultaneous processing spikes.

### Setup and mission making

- Play and Host launchers now leave match settings to the in-game setup screen. Old saved launcher settings no longer override the match.
- Expanded mission-making guidance and added a dedicated Workshop section linking to the GitHub setup and sharing guide.

## 0.5.4 — released

Workshop publication confirmed by Jacob on 2026-10-17 (confirmation date, not the release date).

- Moved shared AI improvements into the required standalone Better Battle Buddies addon. Workshop installations now resolve BBB as a dependency.
- Updated installation guidance for Workshop and the separate BBB dependency. Legacy ZIP installations require the complete paired package once.

## 0.5.3 — previously published

- Published the corrected build with the return control and input handling for leaving the generated-layout preview map during match setup.

Earlier releases predate this versioned changelog; their development records remain in Git and the handoff log.
