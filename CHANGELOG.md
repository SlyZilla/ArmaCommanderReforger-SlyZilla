# Changelog

## 0.5.6 — unreleased

### Forward bases (FOBs)

- Engineers can establish forward bases: select a squad with a construction truck carrying 500 supplies, choose Establish FOB and right-click a clear site. The site check covers the headquarters and the upgrade spots around it, and searches up to 50 m for a better spot. Building takes 90 seconds with the squad on site. Cancelling refunds the supplies to the truck; losing the truck loses them. Builders keep fighting while they work, and give up with a refund after three minutes without progress.
- A built FOB has eight upgrade spots around its headquarters: field hospital, maintenance (light, then heavy), ammo storage, fuel storage, helipad, radio relay, barracks (small, then large) and supply depot. Order upgrades from the FOB panel. They cost the FOB's own supplies and finish faster with engineers nearby. Work pauses while the FOB is contested. Vehicles parked on a finished spot hold it until they move.
- FOBs can be fortified with a separate Defences upgrade in the FOB panel, in three tiers: sandbag positions (400 supplies), then machine-gun nests (255), then bunkers and guard towers in place of the sandbags (680). The positions stand round the edge of the FOB, facing out. A blocked position is skipped. The defences are lost only when every position is gone, and change hands when the FOB is captured. The AI commander builds them too. AI defenders man the machine guns only when Better Battle Buddies' static weapons option is on.
- Each upgrade spot is checked on its own. A blocked spot shows as blocked in the panel, and a matching building already inside the FOB counts as that upgrade.
- FIA heavy maintenance and large barracks keep the small buildings (FIA has no larger models) but give the full upgrade effect.
- The AI commander now builds FOBs when its front is far from a deployment base, upgrades them, orders supplies while keeping an RP reserve, and rebuilds lost headquarters.
- AI sappers can raid enemy FOBs. Once no defender is within 30 m of the headquarters, a sapper crouches, sets a real demolition charge with the game's own animations, wires it to his blasting machine and falls back, then fires it 45 seconds later. Defenders can disarm the charge with the normal Disarm action; AI defenders are sent to stand at it. The blast destroys only the headquarters.
- A destroyed FOB headquarters cooks off: a heavy blast follows the charge, then several smaller secondary explosions round the tent over the next few seconds. Stay clear: the main blast is lethal close in and wounds out to about 30 m. AI sappers wait until their squad is 35 m away before firing.
- Players can destroy an enemy FOB headquarters too: any M112 or TSh-400 charge that goes off within 10 m of it does the job, on a timer or a detonator.
- Dismantling an enemy FOB headquarters with a shovel now needs the FOB contested and no defenders within 30 m.
- A FOB whose headquarters is destroyed goes offline: no deployment, reinforcement, services, deliveries or upgrades. Its buildings remain, and can be captured with the FOB. An Establish FOB order inside it rebuilds the headquarters and brings it back online.
- Capturing a FOB now transfers its buildings and upgrades to the new owner.

### Fixes

- Fixed FOB upgrades being invisible to joined players and destroyed radio antennas blocking replacement through the upgrade panel.
- Prevented overlapping FOB construction orders from wasting truck supplies; blocked construction now refunds its reserved cargo.
- Planted demolition charges keep their fuse when command changes hands or their attack plan ends.
- Fixed the headquarters spawning on top of the construction truck; trucks now park clear of the base layout.
- The AI commander no longer buys repeated construction trucks while one is already on its way.
- Buildings without a side no longer repeat a log message every two seconds.

### Feedback

- Bugs and ideas can now be reported on Discord (discord.gg/WK8xGSztB): #issues for bugs, #suggestions for ideas, no GitHub account needed. The match setup screen links it too. GitHub Issues remain available.

## 0.5.5 — released

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
