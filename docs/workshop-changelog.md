0.5.11

Hotfix for 0.5.10: the period jets can be seen and heard, Project Outbreak zombies no longer lock you out of respawning, and generated maps put their objectives in the towns. Works with Better Battle Buddies 0.1.8.

FIXED

- Period jets (-acrPeriodJets 1) are visible and audible. The 0.5.9 and 0.5.10 Workshop builds left out their models and engine sounds, so recon flights and bomb runs flew invisible and silent for everyone.
- With Project Outbreak's zombies in the match, a commander killed in his command unit could be stuck on the deploy screen with nothing to pick. Zombies no longer hold, contest or take bases; they still bring the garrison out to fight.
- A side is never left with nowhere to deploy: if every deploy base is under attack and there is no HQ spawn point, its best base stays open.
- Objectives on generated maps sit on their towns' houses, not on the map label beside them (13 places on Chernarus Minus, 4 hamlets on Everon).
- An AI truck giving a squad a ride no longer turns back halfway to the service truck and leaves its riders on the road.
- Soldiers no longer spawn inside sealed attics, cellars or thick castle walls.
- Content packs: soldier swaps are rolled for each garrison soldier, FIA cell member and pocket fighter, so one unlucky roll no longer leaves a whole match without the pack's kit.
- AI commanders no longer plan attacks around helicopter rides they never get. Far squads were counted as flying in, then walked 6 km and the attack was called off, over and over; on big generated maps the front froze. A squad left walking far now leaves the attack at once.
- AI vehicle crews that got out under fire get back into their vehicle instead of being sent off on foot and leaving it behind.

NEW

- Battlefield size: a quarter of the map (-acrSize quarter). On Chernarus Minus that is 15-19 objectives with the HQs 4-8 km apart.
- Generated battles run in a different direction on each seed instead of always corner to corner.

BETTER

- I'm the 2IC (Experimental): while you command, the AI commander runs your side as if nobody did and spends its RP; you keep every command power and the squads you lead or order.
- Game Master hears the period jets from the Game Master camera.
- The host debug menu names the side every button acts on, shows both sides' RP, and says when a click is ignored.
