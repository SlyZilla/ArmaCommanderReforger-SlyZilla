// -acrFortifyTest 1 (server, Everon, ~40 s): the fortification planner (ACR_Fortify) on every
// Everon base. Prints each base's plan (FORTIFY lines) and, for the two towns with the most
// corner walls, their buildings and roads (FORTGEO lines) so a plan can be drawn and checked.
// Checks: plans exist for the towns (all four kinds), most are dressed, corner walls face a threat and leave a lane, no
// piece stands inside a building, planning time, and the preview spawns and clears its props.
class ACR_TestFortify
{
 static const int EXPECTED = 13;
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
  int bases, corners, squares, edges, houses, withCorners, maxMs, badLane, badThreat, inside, pieces, dressed, positions, clips;
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
   array<ref ACR_FortifyPos> plan = {};
   string summary;
   int started = System.GetTickCount();
   ACR_Fortify.Plan(base, owner, plan, summary);
   int ms = System.GetTickCount() - started;
   if (ms > maxMs) { maxMs = ms; worst = base.m_sName; }
   PrintFormat("[ACR] FORTIFY plan %1 (%2 ms)", summary, ms);
   ACR_Fortify.Log(base, plan);
   int here = 0;
   array<IEntity> buildings = {};
   ACR_Fortify.Buildings(base.GetOrigin(), ACR_Fortify.SearchRadius(base) + 20, buildings);
   foreach (ACR_FortifyPos p : plan)
   {
    if (p.m_eKind == ACR_EFortifyKind.CORNER)
    {
     corners++;
     here++;
     if (p.m_fLane < ACR_Fortify.LANE - 0.05) badLane++;
     if (p.m_fThreat < ACR_Fortify.MIN_THREAT) badThreat++;
    }
    else if (p.m_eKind == ACR_EFortifyKind.SQUARE) squares++;
    else if (p.m_eKind == ACR_EFortifyKind.EDGE) houses++;
    else edges++;
    positions++;
    if (p.m_aPiecePos.Count() > p.Walls()) dressed++;
    foreach (vector at : p.m_aPiecePos)
    {
     pieces++;
     if (ACR_Fortify.BuildingGap(buildings, at, 25) < 0.2)
     {
      inside++;
      PrintFormat("[ACR] FORTIFY inside a building: %1 %2 at %3", base.m_sName, ACR_Fortify.KindName(p.m_eKind), at);
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
  // What the world clearance check runs into most (for tuning ACR_Fortify.WorldClear).
  foreach (string what, int n : ACR_Fortify.s_mBlockers)
   if (n >= 5) PrintFormat("[ACR] FORTIFY blocker %1 x%2", what, n);
  Geometry(most);
  Geometry(second);
  Measure(most.GetOrigin());
  SurveyTypes();

  ACR_TestRegress.Check("fortify: planned the Everon bases", bases >= 10, string.Format("%1 bases", bases));
  ACR_TestRegress.Check("fortify: corner walls in several towns", corners >= 5 && withCorners >= 3, string.Format("%1 corners at %2 bases", corners, withCorners));
  ACR_TestRegress.Check("fortify: square redoubts found", squares >= 1, string.Format("%1 squares", squares));
  ACR_TestRegress.Check("fortify: perimeter posts found", edges >= 5, string.Format("%1 posts", edges));
  ACR_TestRegress.Check("fortify: house corners found", houses >= 5, string.Format("%1 house corners", houses));
  ACR_TestRegress.Check("fortify: positions are dressed", positions > 0 && dressed * 4 >= positions * 3, string.Format("%1 of %2 have props besides sandbags", dressed, positions));
  ACR_TestRegress.Check("fortify: corner walls leave a vehicle lane", badLane == 0, string.Format("%1 too narrow", badLane));
  ACR_TestRegress.Check("fortify: corner walls face a threat", badThreat == 0, string.Format("%1 facing away", badThreat));
  ACR_TestRegress.Check("fortify: no piece inside a building", inside == 0, string.Format("%1 of %2 pieces", inside, pieces));
  ACR_TestRegress.Check("fortify: no piece clips the world or another position", clips == 0, string.Format("%1 clipping pieces", clips));
  ACR_TestRegress.Check("fortify: planning time", maxMs < 500, string.Format("max %1 ms (%2)", maxMs, worst));

  // The preview of the town with the most corners: one prop per planned piece.
  if (!most)
  {
   ACR_TestRegress.Check("fortify: preview spawns its pieces", false, "no base");
   return;
  }
  string result = ACR_Fortify.Preview(most, Owner(most));
  Print("[ACR] FORTIFY preview: " + result);
  // The preview planned with no props standing; a fresh plan after clearing must match it.
  int planned = 0;
  array<ref ACR_FortifyPos> again = {};
  string unused;
  s_aSpawned.Clear();
  foreach (IEntity e : ACR_Fortify.s_aPreview) s_aSpawned.Insert(e);
  // One of each piece's model bounds (ACR_Fortify.LONG_LEN is the long piece's X length).
  array<string> measured = {};
  foreach (IEntity e2 : s_aSpawned)
  {
   if (!e2 || !e2.GetPrefabData()) continue;
   string prefabName = FilePath.StripPath(e2.GetPrefabData().GetPrefabName());
   if (measured.Contains(prefabName)) continue;
   measured.Insert(prefabName);
   vector mins, maxs;
   e2.GetBounds(mins, maxs);
   PrintFormat("[ACR] FORTIFY piece %1 bounds %2 .. %3", prefabName, mins, maxs);
  }
  ACR_Fortify.ClearPreview();
  ACR_Fortify.Plan(most, Owner(most), again, unused);
  foreach (ACR_FortifyPos q : again) planned += q.m_aPiecePos.Count();
  ACR_TestRegress.Check("fortify: preview spawns its pieces", s_aSpawned.Count() == planned && planned > 0, string.Format("%1 spawned, %2 planned (%3)", s_aSpawned.Count(), planned, most.m_sName));
 }

 // Pieces that clip a world object (re-checked from scratch) or another position's piece.
 static int Clips(ACR_Base base, array<ref ACR_FortifyPos> plan)
 {
  int bad = 0;
  foreach (int a, ACR_FortifyPos p : plan)
  {
   foreach (int i, vector at : p.m_aPiecePos)
   {
    bool hit = !ACR_Fortify.WorldClear(at, p.m_aPieceYaw[i], ACR_Fortify.HalfSize(p.m_aPiecePrefab[i]));
    foreach (int b, ACR_FortifyPos q : plan)
    {
     if (b <= a) continue;
     foreach (int j, vector other : q.m_aPiecePos)
      if (ACR_Fortify.PiecesOverlap(p, i, q, j, 0.0)) hit = true;
    }
    if (!hit) continue;
    bad++;
    PrintFormat("[ACR] FORTIFY clip: %1 %2 %3 at %4", base.m_sName, ACR_Fortify.KindName(p.m_eKind), FilePath.StripPath(p.m_aPiecePrefab[i]), at);
   }
  }
  return bad;
 }

 // Every kit prop's model bounds (ACR_Fortify.HalfSize is built from these).
 static void Measure(vector near)
 {
  array<ResourceName> all = {};
  array<string> sides = {"US", "USSR", "FIA"};
  foreach (string side : sides)
  {
   ACR_FortifyKit kit = ACR_Fortify.Kit(side);
   array<ResourceName> mine = {kit.m_sLong, kit.m_sEnd, kit.m_sRound, kit.m_sLight, kit.m_sNet, ACR_Fortify.DUCKBOARD, ACR_Fortify.COIL};
   foreach (ResourceName r : kit.m_aClutter) mine.Insert(r);
   foreach (ResourceName m : mine) if (!all.Contains(m)) all.Insert(m);
  }
  foreach (int i, ResourceName prefab : all)
  {
   Resource res = Resource.Load(prefab);
   if (!res.IsValid()) { PrintFormat("[ACR] FORTIFY measure: cannot load %1", prefab); continue; }
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   params.Transform[3] = near + Vector(i * 6, 300, 0);
   IEntity e = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
   if (!e) continue;
   vector mins, maxs;
   e.GetBounds(mins, maxs);
   vector half = ACR_Fortify.HalfSize(prefab);
   PrintFormat("[ACR] FORTIFY measure %1 bounds %2 .. %3 (HalfSize %4)", FilePath.StripPath(prefab), mins, maxs, half);
   SCR_EntityHelper.DeleteEntityAndChildren(e);
  }
 }

 protected static ref array<IEntity> s_aTypes = {};
 protected static bool CollectType(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (!SCR_DestructibleBuildingEntity.Cast(e) && !BaseBuilding.Cast(e)) return true;
  if (ACR_Fortify.ModelName(e).Contains("Powerline")) return true;
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
    string prefab = ACR_Fortify.ModelName(e);
    counts.Set(prefab, counts.Get(prefab) + 1);
   }
   PrintFormat("[ACR] FORTTYPE %1 | radius %2 | %3 buildings", base.m_sName, (int)base.m_fRadius, s_aTypes.Count());
   foreach (string model, int n : counts) PrintFormat("[ACR] FORTTYPE %1 | %2 | %3", base.m_sName, model, n);
  }
  s_aTypes.Clear();
 }

 // -acrFortifySurveyTest 1 (server, any world, ~30 s): only the building-model survey.
 static void RunSurvey(int tick)
 {
  if (tick != 8) return;
  SurveyTypes();
  ACR_TestRegress.Check("fortify survey: bases surveyed", !ACR_Base.s_All.IsEmpty(), string.Format("%1 bases", ACR_Base.s_All.Count()));
  ACR_TestRegress.Complete("fortifysurvey", 1);
 }

 static void Cleared()
 {
  int left = 0;
  foreach (IEntity e : s_aSpawned) if (e) left++;
  ACR_TestRegress.Check("fortify: clear removes the preview", left == 0 && ACR_Fortify.s_aPreview.IsEmpty(), string.Format("%1 left", left));
  ACR_TestRegress.Complete("fortify", EXPECTED);
 }

 // Buildings (four corners) and road segments around a base, for drawing its plan.
 static void Geometry(ACR_Base base)
 {
  if (!base) return;
  float radius = ACR_Fortify.SearchRadius(base);
  vector c = base.GetOrigin();
  PrintFormat("[ACR] FORTGEO %1 center %2 %3 radius %4", base.m_sName, (int)c[0], (int)c[2], (int)radius);
  array<IEntity> buildings = {};
  ACR_Fortify.Buildings(c, radius, buildings);
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
  array<ref ACR_FortifySeg> segs = {};
  ACR_Fortify.Roads(c, radius, segs);
  foreach (ACR_FortifySeg s : segs)
   PrintFormat("[ACR] FORTGEO R %1 %2 %3 %4 %5", s.m_vA[0].ToString(-1, 1), s.m_vA[2].ToString(-1, 1), s.m_vB[0].ToString(-1, 1), s.m_vB[2].ToString(-1, 1), s.m_fWidth.ToString(-1, 1));
  array<ref ACR_FortifyPos> plan = {};
  string summary;
  ACR_Fortify.Plan(base, Owner(base), plan, summary);
  foreach (ACR_FortifyPos p : plan)
  {
   foreach (int i, vector at : p.m_aPiecePos)
    PrintFormat("[ACR] FORTGEO P %1 %2 %3 %4 %5 %6", ACR_Fortify.KindName(p.m_eKind), at[0].ToString(-1, 1), at[2].ToString(-1, 1), p.m_aPieceYaw[i].ToString(-1, 0), FilePath.StripExtension(FilePath.StripPath(p.m_aPiecePrefab[i])), p.m_aPieceWall[i]);
  }
 }
}
