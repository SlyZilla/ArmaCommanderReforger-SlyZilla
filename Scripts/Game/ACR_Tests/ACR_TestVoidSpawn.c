// -acrVoidSpawnTest 1 (server, about 1 min; Chernarus Minus or any map): soldiers never appear in
// sealed attics, cellars or hollow walls (Jacob, 6 Nov, Chernarus: Zub, Devil's Castle). At castle
// and town centres (Chernarus: fixed sites; elsewhere the bases with most buildings round them)
// ACR_BaseSystem.SoldierSpot hands out SPOTS spots each; every one must stand on the terrain, open to
// the sky, with room, and walk out to open ground (ReachesOpen). The way before 6 Nov (OldSpot: the
// nearest navmesh point once ten tries failed) is judged the same way, for comparison only. The walk
// must also pass plain open ground (fields round each site), cheaply.
// The game's own group spawn (bought squads, starting units, riders: SCR_AIGroup.SpawnGroupMember)
// puts each man on the navmesh point nearest his formation slot within 10 m, up and down too:
// beside a house, an attic floor, a cellar lifted to the terrain or a wall's hollow. Rifle squads
// spawned on the houses round each site show where the game puts them (CONTROL, left alone) and
// where they stand with ACR's look at each man as the game makes him (SCR_AIGroup.SpawnGroupMember,
// ACR_BaseSystem.SettleMan): every man usable.
class ACR_TestVoidSpawn
{
 static const int SPOTS = 25;
 static const int FIELD_SPOTS = 10;
 protected static IEntity s_Agent;
 protected static ref array<vector> s_aSites;
 protected static ref array<string> s_aNames;
 protected static int s_iNew, s_iNewBad, s_iNewJudged, s_iOld, s_iOldBad, s_iOldFellBack;
 protected static int s_iField, s_iFieldPassed, s_iFieldHops, s_iFieldDeep, s_iFieldShut;
 protected static int s_iSettledCrowded;
 protected static int s_iLastResortBefore, s_iSealedBefore;
 static const ResourceName SQUAD = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const int HOUSES_PER_SITE = 2;
 protected static ref array<SCR_AIGroup> s_aSquads;
 protected static ref array<vector> s_aHouses;
 protected static int s_iControlMen, s_iControlBad, s_iSettledMen, s_iSettledBad;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (second == 1) foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
  if (second == 6) Prepare();
  if (second >= 40 && second < 60 && s_aSites && second - 40 < s_aSites.Count()) Measure(second - 40);
  if (second == 62) SpawnSquads(false);
  if (second == 70) JudgeSquads(false);
  if (second == 72) SpawnSquads(true);
  if (second == 81) JudgeSquads(true);
  if (second == 83) Verdict();
 }

 protected static void Prepare()
 {
  s_Agent = ACR_Garrison.AnyAgent();
  s_aSites = {};
  s_aNames = {};
  s_iNew = 0; s_iNewBad = 0; s_iNewJudged = 0; s_iOld = 0; s_iOldBad = 0; s_iOldFellBack = 0;
  s_iField = 0; s_iFieldPassed = 0; s_iFieldHops = 0; s_iFieldDeep = 0; s_iFieldShut = 0; s_iSettledCrowded = 0;
  s_iControlMen = 0; s_iControlBad = 0; s_iSettledMen = 0; s_iSettledBad = 0;
  s_aSquads = {};
  s_aHouses = {};
  s_iLastResortBefore = ACR_BaseSystem.s_iSpotLastResort;
  s_iSealedBefore = ACR_BaseSystem.s_iSpotSealed;
  if (GetGame().GetWorldFile().Contains("Chernarus"))
  {
   Site("Zub", "6542 0 5589");
   Site("Devil's Castle", "6888 0 11430");
   Site("Chernogorsk", "6717 0 2568");
   Site("Elektrozavodsk", "10366 0 2062");
   Site("Soapograd", "7955 0 12658");
   Site("Stary Sobor", "6174 0 7753");
   Site("Vybor", "3799 0 8860");
   Site("Gorka", "9596 0 8857");
  }
  else
  {
   array<ACR_Base> bases = {};
   array<int> counts = {};
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    array<IEntity> buildings = {};
    ACR_Garrison.BuildingsWithin(base.GetOrigin(), 40, buildings);
    int at = 0;
    while (at < counts.Count() && counts[at] >= buildings.Count()) at++;
    counts.InsertAt(buildings.Count(), at);
    bases.InsertAt(base, at);
   }
   foreach (int i, ACR_Base b : bases) if (i < 6) Site(b.m_sName, b.GetOrigin());
  }
  AIPathfindingComponent pathing = ACR_BaseSystem.Pathing(s_Agent);
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  // The navmesh streams in round AI; load it round every site (garrisons spawn with enemies near).
  if (mesh) foreach (vector site : s_aSites)
   for (float dx = -160; dx <= 160; dx += 40)
    for (float dz = -160; dz <= 160; dz += 40)
     mesh.LoadTileIn(site + Vector(dx, 0, dz));
  PrintFormat("[ACR] VOIDSPAWN %1 sites, navmesh agent %2", s_aSites.Count(), s_Agent);
 }

 protected static void Site(string name, vector at)
 {
  s_aNames.Insert(name);
  s_aSites.Insert(ACR_Manager.Ground(at));
 }

 // The judge's walk: ten times the hops, so a spot it calls shut in is shut in.
 protected static bool Walks(vector spot, AIPathfindingComponent pathing)
 {
  int hops = ACR_BaseSystem.OPEN_HOPS;
  ACR_BaseSystem.OPEN_HOPS = hops * 5;
  bool open = ACR_BaseSystem.ReachesOpen(spot, pathing);
  ACR_BaseSystem.OPEN_HOPS = hops;
  return open;
 }

 // Another man of these squads this close: crowded, not walled in (he steps aside).
 protected static bool Crowded(IEntity body)
 {
  foreach (SCR_AIGroup group : s_aSquads)
  {
   if (!group) continue;
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity other = agent.GetControlledEntity();
    if (other && other != body && vector.DistanceXZ(other.GetOrigin(), body.GetOrigin()) < 0.8) return true;
   }
  }
  return false;
 }

 // A spot a soldier stands on that he can't use: off the terrain (a floor, a roof), under a roof
 // or in a heap, without room, or shut in.
 protected static string Fault(vector spot, AIPathfindingComponent pathing)
 {
  float above = spot[1] - GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
  if (above > 1) return string.Format("%1 m above the terrain", above);
  if (ACR_BaseSystem.Covered(spot)) return "roofed or buried";
  if (!ACR_BaseSystem.RoomAt(spot)) return "no room";
  if (!Walks(spot, pathing)) return ACR_BaseSystem.s_sOpenWhy;
  return "";
 }

 protected static void Measure(int index)
 {
  AIPathfindingComponent pathing = ACR_BaseSystem.Pathing(s_Agent);
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  vector centre = s_aSites[index];
  string name = s_aNames[index];
  bool loaded = mesh && mesh.IsTileLoaded(centre);
  array<IEntity> buildings = {};
  ACR_Garrison.BuildingsWithin(centre, 40, buildings);
  int houses;
  foreach (IEntity building : buildings)
  {
   if (houses >= HOUSES_PER_SITE) break;
   string kind = ACR_Garrison.Describe(building);
   if (!kind.Contains("House") && !kind.Contains("Castle") && !kind.Contains("Villa") && !kind.Contains("Barn") && !kind.Contains("Tower") && !kind.Contains("Church")) continue;
   s_aHouses.Insert(ACR_Manager.Ground(building.GetOrigin()));
   houses++;
   PrintFormat("[ACR] VOIDSPAWN %1 squad house %2 at %3", name, kind, building.GetOrigin());
  }
  int newBad, oldBad, fellBack, judged;
  string example = "";
  for (int i = 0; i < SPOTS; i++)
  {
   vector spot = ACR_BaseSystem.SoldierSpot(centre, 25, s_Agent);
   s_iNew++;
   if (mesh && mesh.IsTileLoaded(spot)) judged++;
   string fault = Fault(spot, pathing);
   if (fault != "")
   {
    newBad++;
    PrintFormat("[ACR] VOIDSPAWN %1 new spot %2: %3", name, spot, fault);
   }
   bool fell;
   vector old = OldSpot(centre, 25, pathing, fell);
   s_iOld++;
   if (fell) fellBack++;
   string oldFault = Fault(old, pathing);
   if (oldFault != "")
   {
    oldBad++;
    if (example == "") example = string.Format(" (e.g. %1: %2)", old, oldFault);
   }
  }
  // Open fields round the site: clear, uncovered terrain 80-150 m out.
  int fieldPassed, fieldHops, fields;
  for (int f = 0; f < FIELD_SPOTS * 3 && fields < FIELD_SPOTS; f++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float distance = Math.RandomFloat(80, 150);
   vector probe = ACR_Manager.Ground(centre + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
   if (BBB_Terrain.IsWaterAt(probe) || !mesh || !mesh.IsTileLoaded(probe)) continue;
   vector free;
   if (!SCR_WorldTools.FindEmptyTerrainPosition(free, probe, 4, 1.5, 2)) continue;
   free = ACR_Manager.Ground(free);
   vector onMesh;
   if (ACR_BaseSystem.Covered(free) || !pathing.GetClosestPositionOnNavmesh(free, Vector(1, 1.5, 1), onMesh) || vector.DistanceXZ(onMesh, free) > 1) continue;
   fields++;
   int before = ACR_BaseSystem.s_iOpenHops;
   if (ACR_BaseSystem.ReachesOpen(onMesh, pathing))
   {
    fieldPassed++;
    fieldHops += ACR_BaseSystem.s_iOpenHops - before;
   }
   else
   {
    string why = ACR_BaseSystem.s_sOpenWhy;
    int hops = ACR_BaseSystem.OPEN_HOPS;
    ACR_BaseSystem.OPEN_HOPS = 320;
    bool deep = ACR_BaseSystem.ReachesOpen(onMesh, pathing);
    ACR_BaseSystem.OPEN_HOPS = hops;
    PrintFormat("[ACR] VOIDSPAWN %1 field %2 judged %3; with 320 hops: %4", name, onMesh, why, ACR_BaseSystem.s_sOpenWhy);
    if (deep) s_iFieldDeep++;
    else s_iFieldShut++;
   }
  }
  s_iNewBad += newBad;
  s_iNewJudged += judged;
  s_iOldBad += oldBad;
  s_iOldFellBack += fellBack;
  s_iField += fields;
  s_iFieldPassed += fieldPassed;
  s_iFieldHops += fieldHops;
  string fieldsText = string.Format("fields %1/%2 open, %3 hops", fieldPassed, fields, fieldHops);
  PrintFormat("[ACR] VOIDSPAWN %1 at %2 (navmesh %3): new %4/%5 bad (%6 judged); old %7/%5 bad, %8 fell back; %9", name, centre, loaded, newBad, SPOTS, judged, oldBad, fellBack, fieldsText);
  if (example != "") PrintFormat("[ACR] VOIDSPAWN %1 old%2", name, example);
 }

 // SoldierSpot as it was before 6 Nov: after ten tries, the navmesh point nearest the centre.
 protected static vector OldSpot(vector center, float spread, AIPathfindingComponent pathing, out bool fellBack)
 {
  fellBack = false;
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  vector onMesh;
  for (int attempt = 0; attempt < 10; attempt++)
  {
   float reach = spread + attempt * 4;
   vector candidate = ACR_Manager.Ground(center + Vector(Math.RandomFloat(-reach, reach), 0, Math.RandomFloat(-reach, reach)));
   vector free;
   if (!SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 6, 0.6, 2)) continue;
   free = ACR_Manager.Ground(free);
   if (!ACR_BaseSystem.RoomAt(free) || ACR_BaseSystem.Covered(free)) continue;
   if (!pathing || (mesh && !mesh.IsTileLoaded(free))) return free;
   if (!pathing.GetClosestPositionOnNavmesh(free, Vector(1, 1.5, 1), onMesh)) continue;
   if (vector.DistanceXZ(onMesh, free) > 1) continue;
   return free;
  }
  fellBack = true;
  vector near = ACR_Manager.Ground(center);
  if (pathing && pathing.GetClosestPositionOnNavmesh(near, Vector(30, 10, 30), onMesh)) return onMesh + Vector(0, 0.15, 0);
  return near;
 }

 // A rifle squad by the game's own spawn on every house picked (settle false: ACR's look switched off).
 protected static void SpawnSquads(bool settle)
 {
  ACR_BaseSystem.s_bTestNoSettle = !settle;
  foreach (vector house : s_aHouses)
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(SQUAD, house));
   if (!group) continue;
   s_aSquads.Insert(group);
  }
  PrintFormat("[ACR] VOIDSPAWN %1 squads spawned on houses (settle %2)", s_aSquads.Count(), settle);
 }

 // Where their men stand: under a roof or in a heap, without room, or shut in. Then they go.
 protected static void JudgeSquads(bool settled)
 {
  AIPathfindingComponent pathing = ACR_BaseSystem.Pathing(s_Agent);
  int men, bad;
  foreach (SCR_AIGroup group : s_aSquads)
  {
   if (!group) continue;
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body) continue;
    men++;
    vector at = body.GetOrigin();
    string fault = "";
    if (ACR_BaseSystem.Covered(at)) fault = "roofed or buried";
    else if (!Walks(at, pathing)) fault = ACR_BaseSystem.s_sOpenWhy;
    // Seconds after spawning the squad has formed up: a man pressed to a fence or a mate is no void.
    else if (settled && !ACR_BaseSystem.RoomAt(at, body)) s_iSettledCrowded++;
    else if (!settled && !ACR_BaseSystem.RoomAt(at, body) && !Crowded(body)) fault = "no room";
    if (fault == "") continue;
    bad++;
    PrintFormat("[ACR] VOIDSPAWN man at %1 (%2 m above the terrain, settled %3): %4", at, at[1] - GetGame().GetWorld().GetSurfaceY(at[0], at[2]), settled, fault);
   }
   foreach (AIAgent gone : agents) if (gone.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(gone.GetControlledEntity());
   SCR_EntityHelper.DeleteEntityAndChildren(group);
  }
  s_aSquads.Clear();
  ACR_BaseSystem.s_bTestNoSettle = false;
  if (settled) { s_iSettledMen = men; s_iSettledBad = bad; }
  else { s_iControlMen = men; s_iControlBad = bad; }
  PrintFormat("[ACR] VOIDSPAWN squads on houses, settled %1: %2 of %3 men unusable", settled, bad, men);
 }

 protected static void Verdict()
 {
  int lastResort = ACR_BaseSystem.s_iSpotLastResort - s_iLastResortBefore;
  int refused = ACR_BaseSystem.s_iSpotSealed - s_iSealedBefore;
  string detail = string.Format("%1 sites: new %2/%3 bad (%4 on loaded navmesh, %5 last resort, %6 shut-in spots refused); before 6 Nov %7/%8 bad, %9 fell back", s_aSites.Count(), s_iNewBad, s_iNew, s_iNewJudged, lastResort, refused, s_iOldBad, s_iOld, s_iOldFellBack);
  ACR_TestRegress.Check("void spawn: soldiers appear on open ground they can walk out of", s_iNew > 0 && s_iNewBad == 0 && s_iNewJudged * 5 >= s_iNew, detail);
  // A field spot shut in even with 320 hops is a walled yard (Saint-Philippe has them): not open ground.
  int open = s_iField - s_iFieldShut;
  ACR_TestRegress.Check("void spawn: the walk passes open fields", open >= 10 && s_iFieldPassed * 100 >= open * 95, string.Format("%1 of %2 open field spots passed (%3 only with 10x the hops; %4 more were walled yards)", s_iFieldPassed, open, s_iFieldDeep, s_iFieldShut));
  float hops = s_iFieldHops;
  if (s_iFieldPassed > 0) hops = hops / s_iFieldPassed;
  ACR_TestRegress.Check("void spawn: the walk is cheap on open ground", s_iFieldPassed > 0 && hops <= 8, string.Format("%1 hops per open field spot", hops));
  ACR_TestRegress.Check("void spawn: squads the game spawns on houses end up where they can walk out", s_iSettledMen >= 20 && s_iSettledBad == 0, string.Format("settled: %1 of %2 men roofed, buried or shut in (%3 up against something); the game's own placement: %4 of %5 unusable", s_iSettledBad, s_iSettledMen, s_iSettledCrowded, s_iControlBad, s_iControlMen));
  ACR_TestRegress.Complete("voidspawn", 4);
 }
}
