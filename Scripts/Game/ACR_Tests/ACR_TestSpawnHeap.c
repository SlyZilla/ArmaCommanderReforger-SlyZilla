// #87 (srv:regress): squads don't spawn inside sand and gravel piles, nor vehicles in hangars. A
// sand pile, a large gravel pile and a hangar go down on open ground of the test range; their centres must read as buried while
// open ground beside them doesn't, the spot finders must step off them, and a squad filled
// onto the piles must stand clear. The old checks are counted for comparison only.
class ACR_TestSpawnHeap
{
 static const ResourceName SAND = "{DEB57429CD49782D}Prefabs/Props/Construction/GravelPile_01/SandPile_01.et";
 static const ResourceName GRAVEL = "{19A3C48FC2665A3F}Prefabs/Props/Construction/GravelPile_01/GravelPile_01_large.et";
 static const ResourceName HANGAR = "{CEB1F71D46592F3C}Prefabs/Structures/Airport/Hangar_01/Hangar_01.et";
 static const ResourceName US_RIFLEMAN = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 static ref array<IEntity> s_Heaps = {};
 static SCR_AIGroup s_Group;
 static vector s_vSand;
 static vector s_vGravel;

 static vector s_vHangar;

 static void Place(vector sand, vector gravel, vector hangar)
 {
  s_vHangar = ACR_Manager.Ground(hangar);
  s_Heaps.Insert(ACR_Manager.SpawnPrefab(HANGAR, s_vHangar));
  s_vSand = ACR_Manager.Ground(sand);
  s_vGravel = ACR_Manager.Ground(gravel);
  s_Heaps.Insert(ACR_Manager.SpawnPrefab(SAND, s_vSand));
  s_Heaps.Insert(ACR_Manager.SpawnPrefab(GRAVEL, s_vGravel));
 }

 // A tick after Place: the spot checks, then a squad of four filled onto the sand pile.
 static void CheckSpots()
 {
  string name = "spawn spots step off sand and gravel piles (#87)";
  if (s_Heaps.Count() < 3 || !s_Heaps[0] || !s_Heaps[1] || !s_Heaps[2])
  {
   ACR_TestRegress.Skip(name, "the piles did not spawn");
   return;
  }
  vector open = s_vSand + Vector(25, 0, -25);
  bool sandBuried = ACR_BaseSystem.Buried(s_vSand);
  bool gravelBuried = ACR_BaseSystem.Buried(s_vGravel);
  bool openBuried = ACR_BaseSystem.Buried(ACR_Manager.Ground(open));
  // The game's finder, asked for room right at the pile's centre.
  // The old checks round both piles, for comparison: the game's finder from spots on the pile,
  // and #73's room-and-roof check right on it.
  int oldBuried = 0;
  int roomRoofPassed = 0;
  for (int k = 0; k < 20; k++)
  {
   vector pile = s_vSand;
   if (k % 2 == 1) pile = s_vGravel;
   vector onPile = ACR_Manager.Ground(pile + Vector(Math.RandomFloat(-3, 3), 0, Math.RandomFloat(-3, 3)));
   vector old;
   if (SCR_WorldTools.FindEmptyTerrainPosition(old, onPile, 5, 0.6, 2) && ACR_BaseSystem.Buried(ACR_Manager.Ground(old))) oldBuried++;
   if (ACR_BaseSystem.Buried(onPile) && ACR_BaseSystem.RoomAt(onPile) && !ACR_BaseSystem.Roofed(onPile)) roomRoofPassed++;
  }
  vector clear;
  bool found = ACR_BaseSystem.ClearGround(clear, s_vSand, 15, 0.6);
  int badSoldier = 0;
  for (int i = 0; i < 10; i++)
  {
   vector spot = ACR_BaseSystem.SoldierSpot(s_vGravel, 1, ACR_Garrison.AnyAgent());
   if (ACR_BaseSystem.Covered(spot)) badSoldier++;
  }
  // A vehicle's spot (4 m clear) asked for at the hangar's middle, as a moved starting unit's.
  vector truckOld;
  bool oldInHangar = SCR_WorldTools.FindEmptyTerrainPosition(truckOld, s_vHangar, 8, 4, 2.5) && ACR_BaseSystem.Covered(ACR_Manager.Ground(truckOld));
  vector truck;
  bool truckFound = ACR_BaseSystem.ClearGround(truck, s_vHangar, 8, 4) || ACR_BaseSystem.ClearGround(truck, s_vHangar, 60, 4);
  bool truckOk = truckFound && !ACR_BaseSystem.Covered(truck);
  bool ok = sandBuried && gravelBuried && !openBuried && found && !ACR_BaseSystem.Covered(clear) && badSoldier == 0 && truckOk;
  ACR_TestRegress.Check(name, ok, string.Format("buried: sand %1 gravel %2 open %3; ClearGround %4 %5 m out; soldier spots on the gravel %6 of 10 bad; old: the game's finder gave a spot in a pile %7 of 20, room-and-roof passed one %8 times; hangar: a vehicle spot %9 m from its middle", sandBuried, gravelBuried, openBuried, found, (int)vector.DistanceXZ(clear, s_vSand), badSoldier, oldBuried, roomRoofPassed, (int)vector.DistanceXZ(truck, s_vHangar)) + string.Format(", clear %1 (the old finder's under its roof: %2)", truckOk, oldInHangar));

  SCR_AIGroup.IgnoreSpawning(true);
  s_Group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(ACR_BaseSystem.GROUP_BASE, s_vSand));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!s_Group) return;
  s_Group.SetMaxUnitsToSpawn(0);
  array<ResourceName> men = {US_RIFLEMAN, US_RIFLEMAN, US_RIFLEMAN, US_RIFLEMAN};
  // Starts a metre short of the sand's centre: the line runs over the pile.
  if (!ACR_Requisition.FillGroup(s_Group, men, s_vSand - Vector(2, 0, 1))) s_Group = null;
 }

 // A few ticks later: nobody of that squad stands inside a pile; then everything is removed.
 static void CheckSquad()
 {
  string name = "a squad spawned onto a sand pile stands clear of it (#87)";
  if (!s_Group)
  {
   ACR_TestRegress.Check(name, false, "the squad did not spawn");
   Cleanup();
   return;
  }
  array<AIAgent> agents = {};
  s_Group.GetAgents(agents);
  int buried = 0;
  float nearest = float.MAX;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   if (ACR_BaseSystem.Covered(body.GetOrigin())) buried++;
   nearest = Math.Min(nearest, vector.DistanceXZ(body.GetOrigin(), s_vSand));
  }
  ACR_TestRegress.Check(name, agents.Count() == 4 && buried == 0, string.Format("%1 men, %2 inside something, nearest %3 m from the pile's centre", agents.Count(), buried, (int)nearest));
  Cleanup();
 }

 static void Cleanup()
 {
  if (s_Group)
  {
   array<AIAgent> agents = {};
   s_Group.GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
   SCR_EntityHelper.DeleteEntityAndChildren(s_Group);
  }
  foreach (IEntity heap : s_Heaps) if (heap) SCR_EntityHelper.DeleteEntityAndChildren(heap);
  s_Heaps.Clear();
 }
}
