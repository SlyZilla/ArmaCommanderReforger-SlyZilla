0.5.12

Zombie mode. With the Project Outbreak mod loaded, the dead join the war. Needs Better Battle Buddies 0.1.10. Zombie mode needs Project Outbreak; without it nothing changes. A compat add-on, AC: Actual - Zombies, follows separately once this version is live.

NEW

- Zombies (needs Project Outbreak): the setup screen gets a Zombies row (-acrZombies few or many, off by default). Nests sit in open towns away from both starting bases and in buildings out in the country: blood on the ground, burnt patches, smouldering fires, wrecked-car junk. When soldiers of either side come near, a nest pours out and hunts by sight and sound. Full nests send hordes to other towns, so a cleared town can be overrun again, and the men they kill join them. At night they wake from further out, spread faster and raid held bases near them. While zombies are inside a base it can't be captured or secured. The AI commander clears marked zombie areas by day, keeps out of them at night and keeps supply runs away from them.
- Players versus zombies (Zombie mode row, -acrZombieMode tvz): your side alone against the dead. The other side sits out, its HQ becomes the hive, there are far more zombies, and zombies can overrun and take back bases you hold.
- Contained outbreak (Outbreak start row, -acrOutbreak contain): the outbreak starts in one town, ground zero, and spreads from there. Towns it is heading for have survivors holed up: reach them first for 5 RP each. AI commanders send squads to rescue them too. Clearing a nest pays the side that cleared it. No timer and no way to lose.
- Zombie counterattacks (idea from Poppi): take a town or clear a nest and the dead nearby come for it, fast, once per place every 15 minutes, with a warning first.
- Still to come for zombie mode: civilian evacuation, airdrops and a truce with the other side.

BETTER

- 2IC (Experimental tab): the plain 2IC and I'm the 2IC are both choices again (off, 2IC, I'm the 2IC). 2IC (-acr2IC 1): the AI runs only the squads nobody leads and the buying, logistics and FOBs stay yours. I'm the 2IC (now -acr2IC 2): the AI runs your side and you keep every command power. Hosts who set -acr2IC 1 for I'm the 2IC in 0.5.11 now set 2.

FIXED

- An AI commander at its squad limit still buys a fuel or repair truck it needs; service trucks never counted toward the limit.
- AI service trucks waiting behind the front stop being re-sent to the same base every 15 seconds: each base now has a truck park on a nearby road.
- With the experimental job allocator (-acrAIAllocator 2), an attack whose squads are moved to another job is no longer called off as beaten.
