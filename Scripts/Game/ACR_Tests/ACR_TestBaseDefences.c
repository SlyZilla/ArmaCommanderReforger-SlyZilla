// -acrBaseDefencesTest 1 (server, Everon, ~40 s): the fortification planner (ACR_BaseDefences) on every
// Everon base. Prints each base's plan (BASEDEF lines) and, for the two towns with the most
// corner walls, their buildings and roads (FORTGEO lines) so a plan can be drawn and checked.
// Checks: plans exist for the towns (all four kinds), most are dressed, corner walls face a threat and leave a lane, no
// piece stands inside a building, planning time, and the preview spawns and clears its props.
class ACR_TestBaseDefences
{
 static const int EXPECTED = 15;
 static ref array<IEntity> s_aSpawned = {};

 // Neutral bases are planned for the US (the preview does the same with the player's side).
 static string Owner(ACR_Base base)
 {
  if (base.m_sOwner.IsEmpty()) return "US";
  return base.m_sOwner;
 }

 static void Run(int tick)
 {
  if (tick == 10) Plan();
  if (tick == 12) Cleared();
 }

 static void Plan()
 {
  int bases, corners, squares, edges, houses, posts, postPieces, withCorners, maxMs, badLane, badThreat, inside, pieces, dressed, positions, clips;
  string worst = "";
  ACR_Base most;
  ACR_Base second;
  int mostCorners = -1;
  int secondCorners = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   bases++;
   string owner = Owner(base);
   array<ref ACR_BaseDefencePos> plan = {};
   string summary;
   int started = System.GetTickCount();
   ACR_BaseDefences.Plan(base, owner, plan, summary);
   int ms = System.GetTickCount() - started;
   if (ms > maxMs) { maxMs = ms; worst = base.m_sName; }
   PrintFormat("[ACR] BASEDEF plan %1 (%2 ms)", summary, ms);
   ACR_BaseDefences.Log(base, plan);
   int here = 0;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base) + 20, buildings);
   foreach (ACR_BaseDefencePos p : plan)
   {
    if (p.m_eKind == ACR_EBaseDefenceKind.CORNER)
    {
     corners++;
     here++;
     if (p.m_fLane < ACR_BaseDefences.LANE - 0.05) badLane++;
     if (p.m_fThreat < ACR_BaseDefences.MIN_THREAT) badThreat++;
    }
    else if (p.m_eKind == ACR_EBaseDefenceKind.SQUARE) squares++;
    else if (p.m_eKind == ACR_EBaseDefenceKind.EDGE) houses++;
    else if (p.m_eKind == ACR_EBaseDefenceKind.POST)
    {
     posts++;
     postPieces += p.m_aPiecePos.Count();
     string have = "";
     foreach (ResourceName pr : p.m_aPiecePrefab) have += FilePath.StripExtension(FilePath.StripPath(pr)) + " ";
     PrintFormat("[ACR] BASEDEF post %1: %2 | %3", base.m_sName, p.m_sNote, have);
    }
    else edges++;
    positions++;
    if (p.m_aPiecePos.Count() > p.Walls()) dressed++;
    foreach (vector at : p.m_aPiecePos)
    {
     pieces++;
     if (ACR_BaseDefences.BuildingGap(buildings, at, 25, p.m_Host) < 0.2)
     {
      inside++;
      PrintFormat("[ACR] BASEDEF inside a building: %1 %2 at %3", base.m_sName, ACR_BaseDefences.KindName(p.m_eKind), at);
     }
    }
   }
   clips += Clips(base, plan);
   if (here > 0) withCorners++;
   if (here > mostCorners)
   {
    second = most;
    secondCorners = mostCorners;
    most = base;
    mostCorners = here;
   }
   else if (here > secondCorners)
   {
    second = base;
    secondCorners = here;
   }
  }
  // What the world clearance check runs into most (for tuning ACR_BaseDefences.WorldClear).
  foreach (string what, int n : ACR_BaseDefences.s_mBlockers)
   if (n >= 5) PrintFormat("[ACR] BASEDEF blocker %1 x%2", what, n);
  Geometry(most);
  Geometry(second);
  Measure(most.GetOrigin());
  SurveyTypes();

  ACR_TestRegress.Check("base defences: planned the Everon bases", bases >= 10, string.Format("%1 bases", bases));
  ACR_TestRegress.Check("base defences: corner walls in several towns", corners >= 5 && withCorners >= 3, string.Format("%1 corners at %2 bases", corners, withCorners));
  ACR_TestRegress.Check("base defences: square redoubts found", squares >= 1, string.Format("%1 squares", squares));
  ACR_TestRegress.Check("base defences: perimeter posts found", edges >= 5, string.Format("%1 posts", edges));
  ACR_TestRegress.Check("base defences: house corners found", houses >= 5, string.Format("%1 house corners", houses));
  ACR_TestRegress.Check("base defences: command posts on the post models", posts >= 10, string.Format("%1 posts", posts));
  ACR_TestRegress.Check("base defences: command posts are full", posts > 0 && postPieces >= posts * 12, string.Format("%1 pieces in %2 posts", postPieces, posts));
  ACR_TestRegress.Check("base defences: positions are dressed", positions > 0 && dressed * 4 >= positions * 3, string.Format("%1 of %2 have props besides sandbags", dressed, positions));
  ACR_TestRegress.Check("base defences: corner walls leave a vehicle lane", badLane == 0, string.Format("%1 too narrow", badLane));
  ACR_TestRegress.Check("base defences: corner walls face a threat", badThreat == 0, string.Format("%1 facing away", badThreat));
  ACR_TestRegress.Check("base defences: no piece inside a building", inside == 0, string.Format("%1 of %2 pieces", inside, pieces));
  ACR_TestRegress.Check("base defences: no piece clips the world or another position", clips == 0, string.Format("%1 clipping pieces", clips));
  ACR_TestRegress.Check("base defences: planning time", maxMs < 500, string.Format("max %1 ms (%2)", maxMs, worst));

  // The preview of the town with the most corners: one prop per planned piece.
  if (!most)
  {
   ACR_TestRegress.Check("base defences: preview spawns its pieces", false, "no base");
   return;
  }
  string result = ACR_BaseDefences.Preview(most, Owner(most));
  Print("[ACR] BASEDEF preview: " + result);
  // The preview planned with no props standing; a fresh plan after clearing must match it.
  int planned = 0;
  array<ref ACR_BaseDefencePos> again = {};
  string unused;
  s_aSpawned.Clear();
  foreach (IEntity e : ACR_BaseDefences.s_aPreview) s_aSpawned.Insert(e);
  // One of each piece's model bounds (ACR_BaseDefences.LONG_LEN is the long piece's X length).
  array<string> measured = {};
  foreach (IEntity e2 : s_aSpawned)
  {
   if (!e2 || !e2.GetPrefabData()) continue;
   string prefabName = FilePath.StripPath(e2.GetPrefabData().GetPrefabName());
   if (measured.Contains(prefabName)) continue;
   measured.Insert(prefabName);
   vector mins, maxs;
   e2.GetBounds(mins, maxs);
   PrintFormat("[ACR] BASEDEF piece %1 bounds %2 .. %3", prefabName, mins, maxs);
  }
  ACR_BaseDefences.ClearPreview();
  ACR_BaseDefences.Plan(most, Owner(most), again, unused);
  foreach (ACR_BaseDefencePos q : again) planned += q.m_aPiecePos.Count();
  ACR_TestRegress.Check("base defences: preview spawns its pieces", s_aSpawned.Count() == planned && planned > 0, string.Format("%1 spawned, %2 planned (%3)", s_aSpawned.Count(), planned, most.m_sName));
 }

 // Pieces that clip a world object (re-checked from scratch) or another position's piece.
 static int Clips(ACR_Base base, array<ref ACR_BaseDefencePos> plan)
 {
  int bad = 0;
  foreach (int a, ACR_BaseDefencePos p : plan)
  {
   foreach (int i, vector at : p.m_aPiecePos)
   {
    bool hit = !ACR_BaseDefences.WorldClear(at, p.m_aPieceYaw[i], ACR_BaseDefences.HalfSize(p.m_aPiecePrefab[i]), p.m_Host);
    foreach (int b, ACR_BaseDefencePos q : plan)
    {
     if (b <= a) continue;
     foreach (int j, vector other : q.m_aPiecePos)
      if (!ACR_BaseDefences.IsNetTop(q.m_aPiecePrefab[j]) && !ACR_BaseDefences.IsNetTop(p.m_aPiecePrefab[i]) && ACR_BaseDefences.PiecesOverlap(p, i, q, j, 0.0)) hit = true;
    }
    if (!hit) continue;
    bad++;
    PrintFormat("[ACR] BASEDEF clip: %1 %2 %3 at %4", base.m_sName, ACR_BaseDefences.KindName(p.m_eKind), FilePath.StripPath(p.m_aPiecePrefab[i]), at);
   }
  }
  return bad;
 }

 // Every kit prop's model bounds (ACR_BaseDefences.HalfSize is built from these).
 static void Measure(vector near)
 {
  array<ResourceName> all = {};
  array<string> sides = {"US", "USSR", "FIA"};
  foreach (string side : sides)
  {
   ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(side);
   array<ResourceName> mine = {kit.m_sLong, kit.m_sEnd, kit.m_sRound, kit.m_sLight, kit.m_sNet, kit.m_sNetTop, kit.m_sTable, kit.m_sRadio, ACR_BaseDefences.MAP_US, ACR_BaseDefences.MAP_EAST, ACR_BaseDefences.MAP_KOLGUYEV, ACR_BaseDefences.FLIPCHART, ACR_BaseDefences.DUCKBOARD, ACR_BaseDefences.COIL};
   foreach (ResourceName r : kit.m_aClutter) mine.Insert(r);
   foreach (ResourceName m : mine) if (!all.Contains(m)) all.Insert(m);
  }
  foreach (int i, ResourceName prefab : all)
  {
   Resource res = Resource.Load(prefab);
   if (!res.IsValid()) { PrintFormat("[ACR] BASEDEF measure: cannot load %1", prefab); continue; }
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   params.Transform[3] = near + Vector(i * 6, 300, 0);
   IEntity e = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
   if (!e) continue;
   vector mins, maxs;
   e.GetBounds(mins, maxs);
   vector half = ACR_BaseDefences.HalfSize(prefab);
   PrintFormat("[ACR] BASEDEF measure %1 bounds %2 .. %3 (HalfSize %4)", FilePath.StripPath(prefab), mins, maxs, half);
   SCR_EntityHelper.DeleteEntityAndChildren(e);
  }
 }

 protected static ref array<IEntity> s_aTypes = {};
 protected static bool CollectType(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (!SCR_DestructibleBuildingEntity.Cast(e) && !BaseBuilding.Cast(e)) return true;
  if (ACR_BaseDefences.ModelName(e).Contains("Powerline")) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  vector size = maxs - mins;
  if (size[0] < 3.5 || size[2] < 3.5 || size[1] < 2.5) return true;
  s_aTypes.Insert(e);
  return true;
 }

 // Building models inside each base's capture circle, for command-post layouts per model:
 // [ACR] FORTTYPE <base> | <prefab> | <count>
 static void SurveyTypes()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   s_aTypes.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(base.GetOrigin(), Math.Max(base.m_fRadius, 50), CollectType, null, EQueryEntitiesFlags.STATIC);
   map<string, int> counts = new map<string, int>();
   foreach (IEntity e : s_aTypes)
   {
    string prefab = ACR_BaseDefences.ModelName(e);
    counts.Set(prefab, counts.Get(prefab) + 1);
   }
   PrintFormat("[ACR] FORTTYPE %1 | radius %2 | %3 buildings", base.m_sName, (int)base.m_fRadius, s_aTypes.Count());
   foreach (string model, int n : counts) PrintFormat("[ACR] FORTTYPE %1 | %2 | %3", base.m_sName, model, n);
  }
  s_aTypes.Clear();
 }

 // -acrBaseDefencesModelsTest 1 (server, Everon or Kolguyev, ~60 s): a plan of one copy of each
 // command-post model, in the building's own frame (x right, z forward, metres from its
 // origin), for designing a layout per model. Lines:
 //   FORTMODEL <model> at <world pos> yaw <deg> bounds <mins> .. <maxs>
 //   FORTPART <model> <x> <z> <yaw> <part>        doors, windows and other child parts
 //   FORTROW <model> <z> <cells>                  0.5 m cells from x = mins-6: '.' open
 //     ground, '#' under the building's roof, '_' the building's open-air raised surface
 //     (terrace, steps, platform), 'o' something else solid (fence, prop), trees ignored
 //   FORTWALL <model> <x> <z> <what>              chest-height (1.2 m) wall hits, 'door'/'window'/'wall'
 static const string MODELS = "House_Village_E_1I02_Furniture,Barracks_USSR_01_military,Warehouse_01,House_Village_E_1L03t,House_Wooden_E_1I01,Workshop_02_grey,Barn_E_01,Barn_E_02,FarmHouse_E_1L01,House_Village_E_1I08t";
 protected static IEntity s_Building;

 protected static bool CollectAnyBuilding(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e) || BaseBuilding.Cast(e)) s_aTypes.Insert(e);
  return true;
 }

 static void SurveyModels()
 {
  array<string> wanted = {};
  MODELS.Split(",", wanted, true);
  array<string> done = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   s_aTypes.Clear();
   GetGame().GetWorld().QueryEntitiesBySphere(base.GetOrigin(), Math.Max(base.m_fRadius, 50), CollectAnyBuilding, null, EQueryEntitiesFlags.STATIC);
   foreach (IEntity b : s_aTypes)
   {
    string model = ACR_BaseDefences.ModelName(b);
    model = FilePath.StripExtension(model);
    if (!wanted.Contains(model) || done.Contains(model)) continue;
    done.Insert(model);
    DumpModel(model, b);
   }
  }
  s_aTypes.Clear();
  PrintFormat("[ACR] FORTMODEL found %1 of %2 models", done.Count(), wanted.Count());
 }

 protected static bool NotChar(notnull IEntity e)
 {
  return ChimeraCharacter.Cast(e) == null && Tree.Cast(e) == null;
 }

 static void DumpModel(string model, IEntity b)
 {
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  b.GetBounds(mins, maxs);
  PrintFormat("[ACR] FORTMODEL %1 at %2 yaw %3 bounds %4 .. %5", model, b.GetOrigin(), (int)ACR_BaseDefences.Bearing(b.VectorToParent(Vector(0, 0, 1))), mins, maxs);
  // Parts: doors, windows, everything attached.
  IEntity child = b.GetChildren();
  while (child)
  {
   string part = ACR_BaseDefences.ModelName(child);
   vector local = b.CoordToLocal(child.GetOrigin());
   float partYaw = ACR_BaseDefences.Bearing(b.VectorToLocal(child.VectorToParent(Vector(0, 0, 1))));
   PrintFormat("[ACR] FORTPART %1 %2 %3 %4 %5", model, local[0].ToString(-1, 2), local[2].ToString(-1, 2), (int)partYaw, part);
   child = child.GetSibling();
  }
  float x0 = mins[0] - 6;
  float x1 = maxs[0] + 6;
  float z0 = mins[2] - 6;
  float z1 = maxs[2] + 6;
  float top = b.CoordToParent(maxs)[1] + 10;
  for (float z = z1; z >= z0; z -= 0.5)
  {
   string row = "";
   for (float x = x0; x <= x1; x += 0.5)
   {
    vector p = b.CoordToParent(Vector(x, 0, z));
    float ground = world.GetSurfaceY(p[0], p[2]);
    // Down from high above: anything over 2 m up is roof (or a tall wall).
    TraceParam high = new TraceParam();
    high.Start = Vector(p[0], top, p[2]);
    high.End = Vector(p[0], ground - 0.5, p[2]);
    high.Flags = TraceFlags.ENTS;
    float f = world.TraceMove(high, NotChar);
    float hitY = top + (ground - 0.5 - top) * f;
    bool ours = high.TraceEnt && (high.TraceEnt == b || high.TraceEnt.GetRootParent() == b);
    string cell = ".";
    if (f < 1 && ours && hitY > ground + 2.0) cell = "#";
    else if (f < 1 && ours && hitY > ground + 0.15) cell = "_";
    else if (f < 1 && !ours && hitY > ground + 0.3) cell = "o";
    row += cell;
   }
   PrintFormat("[ACR] FORTROW %1 %2 %3", model, z.ToString(-1, 1), row);
  }
  // Chest-height walls: rays along local x, every 0.5 m of z, recording each surface met.
  for (float wz = z0 + 3; wz <= z1 - 3; wz += 0.5)
  {
   vector from = b.CoordToParent(Vector(x0 + 3, 0, wz));
   vector to = b.CoordToParent(Vector(x1 - 3, 0, wz));
   from[1] = world.GetSurfaceY(from[0], from[2]) + 1.2;
   to[1] = world.GetSurfaceY(to[0], to[2]) + 1.2;
   for (int hop = 0; hop < 12; hop++)
   {
    TraceParam ray = new TraceParam();
    ray.Start = from;
    ray.End = to;
    ray.Flags = TraceFlags.ENTS;
    float rf = world.TraceMove(ray, NotChar);
    if (rf >= 1) break;
    vector hit = from + (to - from) * rf;
    string what = "wall";
    if (ray.TraceEnt && ray.TraceEnt != b)
    {
     string hitModel = ACR_BaseDefences.ModelName(ray.TraceEnt);
     if (hitModel.Contains("Door") || hitModel.Contains("Gate")) what = "door";
     else if (hitModel.Contains("Win") || hitModel.Contains("Glass")) what = "window";
     else if (ray.TraceEnt.GetParent() != b) what = "other";
    }
    vector lh = b.CoordToLocal(hit);
    PrintFormat("[ACR] FORTWALL %1 %2 %3 %4", model, lh[0].ToString(-1, 2), lh[2].ToString(-1, 2), what);
    from = hit + (to - from).Normalized() * 0.3;
   }
  }
 }

 static void RunModels(int tick)
 {
  if (tick != 8) return;
  SurveyModels();
  ACR_TestRegress.Check("base defences models: surveyed", true, "");
  // Command posts on this map (Kolguyev: the barracks).
  int posts = 0;
  int maps = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   array<ref ACR_BaseDefencePos> plan = {};
   string summary;
   ACR_BaseDefences.Plan(base, Owner(base), plan, summary);
   foreach (ACR_BaseDefencePos p : plan)
   {
    if (p.m_eKind != ACR_EBaseDefenceKind.POST) continue;
    posts++;
    bool hasMap = p.m_aPiecePrefab.Contains(ACR_BaseDefences.MAP_KOLGUYEV);
    if (hasMap) maps++;
    PrintFormat("[ACR] BASEDEF post %1: %2, %3 pieces, Kolguyev map %4", base.m_sName, p.m_sNote, p.m_aPiecePos.Count(), hasMap);
   }
  }
  ACR_TestRegress.Check("base defences models: command posts placed", posts >= 3, string.Format("%1 posts, %2 with the Kolguyev map", posts, maps));
  ACR_TestRegress.Complete("basedefencesmodels", 2);
 }

 // -acrBaseDefencesSurveyTest 1 (server, any world, ~30 s): only the building-model survey.
 static void RunSurvey(int tick)
 {
  if (tick != 8) return;
  SurveyTypes();
  ACR_TestRegress.Check("base defences survey: bases surveyed", !ACR_Base.s_All.IsEmpty(), string.Format("%1 bases", ACR_Base.s_All.Count()));
  ACR_TestRegress.Complete("basedefencessurvey", 1);
 }

 static void Cleared()
 {
  int left = 0;
  foreach (IEntity e : s_aSpawned) if (e) left++;
  ACR_TestRegress.Check("base defences: clear removes the preview", left == 0 && ACR_BaseDefences.s_aPreview.IsEmpty(), string.Format("%1 left", left));
  ACR_TestRegress.Complete("basedefences", EXPECTED);
 }

 // Buildings (four corners) and road segments around a base, for drawing its plan.
 static void Geometry(ACR_Base base)
 {
  if (!base) return;
  float radius = ACR_BaseDefences.SearchRadius(base);
  vector c = base.GetOrigin();
  PrintFormat("[ACR] FORTGEO %1 center %2 %3 radius %4", base.m_sName, (int)c[0], (int)c[2], (int)radius);
  array<IEntity> buildings = {};
  ACR_BaseDefences.Buildings(c, radius, buildings);
  foreach (IEntity b : buildings)
  {
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   vector p0 = b.CoordToParent(Vector(mins[0], 0, mins[2]));
   vector p1 = b.CoordToParent(Vector(maxs[0], 0, mins[2]));
   vector p2 = b.CoordToParent(Vector(maxs[0], 0, maxs[2]));
   vector p3 = b.CoordToParent(Vector(mins[0], 0, maxs[2]));
   PrintFormat("[ACR] FORTGEO B %1 %2 %3 %4 %5 %6 %7 %8", p0[0].ToString(-1, 1), p0[2].ToString(-1, 1), p1[0].ToString(-1, 1), p1[2].ToString(-1, 1), p2[0].ToString(-1, 1), p2[2].ToString(-1, 1), p3[0].ToString(-1, 1), p3[2].ToString(-1, 1));
  }
  array<ref ACR_BaseDefenceSeg> segs = {};
  ACR_BaseDefences.Roads(c, radius, segs);
  foreach (ACR_BaseDefenceSeg s : segs)
   PrintFormat("[ACR] FORTGEO R %1 %2 %3 %4 %5", s.m_vA[0].ToString(-1, 1), s.m_vA[2].ToString(-1, 1), s.m_vB[0].ToString(-1, 1), s.m_vB[2].ToString(-1, 1), s.m_fWidth.ToString(-1, 1));
  array<ref ACR_BaseDefencePos> plan = {};
  string summary;
  ACR_BaseDefences.Plan(base, Owner(base), plan, summary);
  foreach (ACR_BaseDefencePos p : plan)
  {
   foreach (int i, vector at : p.m_aPiecePos)
    PrintFormat("[ACR] FORTGEO P %1 %2 %3 %4 %5 %6", ACR_BaseDefences.KindName(p.m_eKind), at[0].ToString(-1, 1), at[2].ToString(-1, 1), p.m_aPieceYaw[i].ToString(-1, 0), FilePath.StripExtension(FilePath.StripPath(p.m_aPiecePrefab[i])), p.m_aPieceWall[i]);
  }
 }
}
