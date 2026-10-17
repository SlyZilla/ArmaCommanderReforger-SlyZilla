// Scenario generator tests (aca-f07).
// -acrGenerateTest 1 (srv:generate, Kolguyev, ~40 s): the world has no bases, so the generator
//   builds one at start. Checks the built layout, starting units and spawn points, that the same
//   seed plans the same layout, and the half / flashpoint sizes.
// -acrGenerateEveronTest 1 (srv:generateeveron, ~40 s): plans a layout on Everon (without building
//   it) and compares it with Jacob's hand-built bases.
class ACR_TestGenerate
{
 static const int CHECKS = 20;
 static const int EVERON_CHECKS = 3;

 static string Signature(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs)
 {
  string text = "";
  foreach (ACR_GenSite site : layout) text += string.Format("%1@%2,%3:%4:%5;", site.m_sName, (int)site.m_vPos[0], (int)site.m_vPos[2], site.m_bIncome, site.m_iValue);
  if (hqs.Count() == 2) text += "HQ:" + hqs[0].m_sName + "/" + hqs[1].m_sName;
  return text;
 }

 static void Run(int tick)
 {
  if (tick == 2) ACR_TestRegress.Check("generator built a layout", ACR_Generator.Built() && ACR_Base.s_All.Count() >= 15, string.Format("%1 bases, seed %2", ACR_Base.s_All.Count(), ACR_Generator.s_iSeed));
  if (tick != 20) return;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  ACR_Base westHQ, eastHQ;
  int ownedCount;
  int hidden;
  bool hiddenRules = true;
  bool overlap;
  bool wet;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == west) { westHQ = base; ownedCount++; }
   else if (base.m_sOwner == east) { eastHQ = base; ownedCount++; }
   if (base.m_bHidden) { hidden++; if (base.m_bIncome) hiddenRules = false; }
   if (BBB_Terrain.IsWater(base.GetOrigin()[0], base.GetOrigin()[2])) wet = true;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other != base && vector.DistanceXZ(other.GetOrigin(), base.GetOrigin()) < other.m_fRadius + base.m_fRadius) overlap = true;
  }
  float apart = 0;
  if (westHQ && eastHQ) apart = vector.DistanceXZ(westHQ.GetOrigin(), eastHQ.GetOrigin());
  ACR_TestRegress.Check("one HQ per side, far apart", ownedCount == 2 && westHQ && eastHQ && apart >= 3000, string.Format("%1 ownedCount, %2 m apart", ownedCount, (int)apart));
  bool hqRules = westHQ && eastHQ && westHQ.m_iValue == 2 && eastHQ.m_iValue == 2 && westHQ.m_iGarrison == 6 && westHQ.m_fRadius >= 45 && eastHQ.m_fRadius >= 45;
  ACR_TestRegress.Check("HQs follow the Everon rules", hqRules, string.Format("value 2, garrison 6, radius %1 / %2", (int)westHQ.m_fRadius, (int)eastHQ.m_fRadius));
  ACR_TestRegress.Check("bases on dry ground, circles apart", !wet && !overlap, string.Format("wet %1, overlap %2", wet, overlap));
  ACR_TestRegress.Check("landmarks remain hidden tactical objectives", hiddenRules, string.Format("%1 accessible hidden sites, no passive income", hidden));
  // Starting units at their HQ.
  ACR_Manager manager = ACR_Manager.GetInstance();
  int westNear, eastNear;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.m_Group || !squad.GetFaction()) continue;
   vector at = squad.m_Group.GetOrigin();
   string squadSide = squad.GetFaction().GetFactionKey();
   if (squadSide == west && westHQ && vector.DistanceXZ(at, westHQ.GetOrigin()) < 250) westNear++;
   if (squadSide == east && eastHQ && vector.DistanceXZ(at, eastHQ.GetOrigin()) < 250) eastNear++;
  }
  ACR_TestRegress.Check("starting units at each HQ", westNear >= 3 && eastNear >= 3, string.Format("west %1, east %2", westNear, eastNear));
  int spawnsOk, spawns;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   spawns++;
   ACR_Base hq = westHQ;
   if (point.GetFactionKey() == east) hq = eastHQ;
   if (hq && vector.DistanceXZ(point.GetOrigin(), hq.GetOrigin()) < 60) spawnsOk++;
  }
  ACR_TestRegress.Check("spawn points at their HQ", spawns >= 2 && spawnsOk == spawns, string.Format("%1 of %2", spawnsOk, spawns));
  // Same seed, same plan.
  array<ref ACR_GenSite> again = {};
  array<ACR_GenSite> againHQs = {};
  bool planned = ACR_Generator.Plan(ACR_Generator.s_iSeed, -1, again, againHQs);
  array<ACR_GenSite> builtHQs = {};
  foreach (ACR_GenSite s : ACR_Generator.s_Sites) if ((westHQ && s.m_sName == westHQ.m_sName) || (eastHQ && s.m_sName == eastHQ.m_sName)) builtHQs.Insert(s);
  bool same = planned && again.Count() == ACR_Generator.s_Sites.Count();
  foreach (int i, ACR_GenSite a : again) if (same && (a.m_sName != ACR_Generator.s_Sites[i].m_sName || vector.DistanceXZ(a.m_vPos, ACR_Generator.s_Sites[i].m_vPos) > 1)) same = false;
  bool sameHQs = planned && againHQs.Count() == 2 && westHQ && againHQs[0].m_sName == westHQ.m_sName;
  ACR_TestRegress.Check("same seed plans the same layout", same && sameHQs, string.Format("%1 vs %2 sites", again.Count(), ACR_Generator.s_Sites.Count()));
  // Other seeds vary the HQs.
  array<string> pairs = {};
  for (int seed = 1; seed <= 6; seed++)
  {
   array<ref ACR_GenSite> other = {};
   array<ACR_GenSite> otherHQs = {};
   if (!ACR_Generator.Plan(seed * 7919, -1, other, otherHQs)) continue;
   string pair = otherHQs[0].m_sName + "/" + otherHQs[1].m_sName;
   if (!pairs.Contains(pair)) pairs.Insert(pair);
  }
  ACR_TestRegress.Check("seeds vary the HQs", pairs.Count() >= 2, string.Format("%1 HQ pairs in 6 seeds", pairs.Count()));
  // Half: one side of the map, its own HQs.
  array<ref ACR_GenSite> half = {};
  array<ACR_GenSite> halfHQs = {};
  bool halfOk = ACR_Generator.Plan(ACR_Generator.s_iSeed, ACR_ESkirmishSize.HALF, half, halfHQs);
  int fullCount = ACR_Generator.s_Sites.Count();
  ACR_TestRegress.Check("half size", halfOk && half.Count() >= 6 && half.Count() <= 0.65 * fullCount + 1 && halfHQs.Count() == 2 && halfHQs[0] != halfHQs[1], string.Format("%1 of %2 sites", half.Count(), fullCount));
  // Flashpoint: 5-7 sites close together, HQs inside it.
  array<ref ACR_GenSite> flash = {};
  array<ACR_GenSite> flashHQs = {};
  bool flashOk = ACR_Generator.Plan(ACR_Generator.s_iSeed, ACR_ESkirmishSize.FLASHPOINT, flash, flashHQs);
  float spread = 0;
  foreach (ACR_GenSite f1 : flash) foreach (ACR_GenSite f2 : flash) spread = Math.Max(spread, vector.DistanceXZ(f1.m_vPos, f2.m_vPos));
  bool hiddenHQ = (halfHQs.Count() == 2 && (halfHQs[0].m_sKind == "landmark" || halfHQs[1].m_sKind == "landmark")) || (flashHQs.Count() == 2 && (flashHQs[0].m_sKind == "landmark" || flashHQs[1].m_sKind == "landmark"));
  ACR_TestRegress.Check("flashpoint size", flashOk && flash.Count() >= 5 && flash.Count() <= 7 && spread <= 5000 && flashHQs.Count() == 2 && !hiddenHQ, string.Format("%1 sites, %2 m across, hidden HQ %3", flash.Count(), (int)spread, hiddenHQ));
  foreach (ACR_GenSite fs : flash) PrintFormat("[ACR] GEN FLASHPOINT %1 | %2 | %3 %4", fs.m_sName, fs.m_sKind, (int)fs.m_vPos[0], (int)fs.m_vPos[2]);
  if (flashHQs.Count() == 2) PrintFormat("[ACR] GEN FLASHPOINT HQs %1 / %2", flashHQs[0].m_sName, flashHQs[1].m_sName);
  foreach (ACR_GenSite hs : half) PrintFormat("[ACR] GEN HALF %1 | %2 | %3 %4", hs.m_sName, hs.m_sKind, (int)hs.m_vPos[0], (int)hs.m_vPos[2]);
  if (halfHQs.Count() == 2) PrintFormat("[ACR] GEN HALF HQs %1 / %2", halfHQs[0].m_sName, halfHQs[1].m_sName);
  // Names come from the map, not placeholders.
  int unnamed;
  foreach (ACR_Base named : ACR_Base.s_All) if (named.m_sName == "" || named.m_sName.StartsWith("Base ") || named.m_sName.StartsWith("#")) unnamed++;
  ACR_TestRegress.Check("bases named from the map", unnamed == 0, string.Format("%1 unnamed", unnamed));
  BalanceChecks(manager);
  ACR_TestRegress.Complete("generate", CHECKS);
 }

 protected static void BalanceChecks(ACR_Manager manager)
 {
  bool builtFlags = true;
  int earning, tactical;
  foreach (ACR_GenSite site : ACR_Generator.s_Sites)
  {
   if (site.m_bIncome) earning++; else tactical++;
   ACR_Base match;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == site.m_sName) match = base;
   if (!match || match.m_bIncome != site.m_bIncome || match.m_iValue != site.m_iValue) builtFlags = false;
  }
  ACR_TestRegress.Check("economy: build preserves income and tactical site flags", builtFlags && earning > 2 && tactical > 0, string.Format("%1 earning, %2 tactical", earning, tactical));
  ACR_TestRegress.Check("economy: starting funding and reinforcement fee unchanged", manager.m_Bases.m_iBasicIncome == 5 && manager.m_Bases.m_iStartingRP == 50 && ACR_Requisition.REINFORCE_COST_PER_SOLDIER == 3, "basic 5, starting 50, reinforcement 3");
  array<int> seeds = {45289, 37941, 7919, 15838, 23757, 31676};
  bool planned = true, spaced = true, forward = true, opening = true, repeatable = true;
  foreach (int seed : seeds)
  {
   array<ref ACR_GenSite> layout = {}, again = {};
   array<ACR_GenSite> hqs = {}, againHQs = {};
   if (!ACR_Generator.Plan(seed, ACR_ESkirmishSize.HALF, layout, hqs) || hqs.Count() != 2 || layout.Count() < 6) { planned = false; continue; }
   ACR_Generator.Plan(seed, ACR_ESkirmishSize.HALF, again, againHQs);
   if (Signature(layout, hqs) != Signature(again, againHQs)) repeatable = false;
   int incomeCount, value;
   foreach (ACR_GenSite p : layout)
   {
    if (p.m_bIncome) { incomeCount++; value += p.m_iValue; }
    if (!hqs.Contains(p) && p.m_sKind != "landmark")
    {
     int home = 0;
     if (vector.DistanceXZ(p.m_vPos, hqs[1].m_vPos) < vector.DistanceXZ(p.m_vPos, hqs[0].m_vPos)) home = 1;
     float homeDistance = vector.DistanceXZ(p.m_vPos, hqs[home].m_vPos);
     int closer = 0;
     foreach (ACR_GenSite neighbour : layout)
     {
      if (hqs.Contains(neighbour) || neighbour.m_sKind == "landmark") continue;
      float ownDistance = vector.DistanceXZ(neighbour.m_vPos, hqs[home].m_vPos);
      if (ownDistance <= vector.DistanceXZ(neighbour.m_vPos, hqs[1 - home].m_vPos) && ownDistance < homeDistance) closer++;
     }
     if ((homeDistance <= 2000 || closer < 4) && !p.m_bIncome) opening = false;
    }
    if (!hqs.Contains(p) && !ACR_Generator.BetweenHQs(p.m_vPos, hqs[0].m_vPos, hqs[1].m_vPos)) forward = false;
    foreach (ACR_GenSite q : layout)
    {
     if (q == p) continue;
     float gap = vector.DistanceXZ(p.m_vPos, q.m_vPos);
     if (gap < 250 || (gap < 350 && p.m_iOpeningSide < 0 && q.m_iOpeningSide < 0)) spaced = false;
    }
    if (seed == 45289) PrintFormat("[ACR] ECON HALF %1 | %2 | %3 %4 | value %5 income %6 opening %7", p.m_sName, p.m_sKind, (int)p.m_vPos[0], (int)p.m_vPos[2], p.m_iValue, p.m_bIncome, p.m_iOpeningSide);
   }
   if (incomeCount >= layout.Count()) opening = false;
   PrintFormat("[ACR] ECON PLAN seed %1: %2 objectives, %3 income, total base RP %4, HQs %5 / %6", seed, layout.Count(), incomeCount, value, hqs[0].m_sName, hqs[1].m_sName);
  }
  ACR_TestRegress.Check("generator: southern-half seeds produce repeatable reduced layouts", planned && repeatable, "45289, 37941 and four additional seeds");
  ACR_TestRegress.Check("generator: objective spacing protects opening pairs without dense clusters", spaced, "350 m normally, 250 m protected opening pairs");
  ACR_TestRegress.Check("generator: no automatic objectives directly behind the HQs", forward, "200 m tolerance, accessible flanks retained");
  ACR_TestRegress.Check("economy: home regions and four nearest objectives keep full income", opening, "2 km home regions; later sites can be tactical");
  bool cutsHQ = !ACR_GenAccess.OutsideHQ(Vector(0, 0, 0), Vector(1000, 0, 0), Vector(500, 0, 0));
  bool bypass = ACR_GenAccess.OutsideHQ(Vector(0, 0, 400), Vector(1000, 0, 400), Vector(500, 0, 0));
  ACR_TestRegress.Check("generator: access rejects crossing an HQ but allows a perimeter bypass", cutsHQ && bypass, "350 m perimeter, including edge interiors");
  bool peninsula = !ACR_Generator.BetweenHQs(Vector(1421, 0, 7481), Vector(2248, 0, 7375), Vector(7669, 0, 5061));
  bool flank = ACR_Generator.BetweenHQs(Vector(7637, 0, 4513), Vector(2248, 0, 7375), Vector(7606, 0, 5000));
  ACR_TestRegress.Check("generator: Kolguyev rear peninsula excluded without cutting the eastern flank", peninsula && flank, "West Cape behind NW HQ; Ugoldar Power Plant beside eastern HQ");
 }

 // Everon: plan a full layout and compare it with the hand-built one.
 static void RunEveron(int tick)
 {
  if (tick != 5) return;
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  bool ok = ACR_Generator.Plan(4242, -1, layout, hqs);
  int matched, authored;
  string missing = "";
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bBuilt) continue;
   authored++;
   bool found = false;
   foreach (ACR_GenSite site : layout) if (vector.DistanceXZ(site.m_vPos, base.GetOrigin()) < 120) found = true;
   if (found) matched++;
   else missing += base.m_sName + ", ";
  }
  foreach (ACR_GenSite s : layout) PrintFormat("[ACR] GEN EVERON %1 | %2 | %3 %4 | v%5 r%6", s.m_sName, s.m_sKind, (int)s.m_vPos[0], (int)s.m_vPos[2], s.m_iValue, (int)s.m_fRadius);
  if (hqs.Count() == 2) PrintFormat("[ACR] GEN EVERON HQs %1 / %2 (%3 m)", hqs[0].m_sName, hqs[1].m_sName, (int)vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos));
  PrintFormat("[ACR] GEN EVERON not generated: %1", missing);
  ACR_TestRegress.Check("Everon plan made", ok && layout.Count() >= 30, string.Format("%1 sites", layout.Count()));
  ACR_TestRegress.Check("Everon keeps a reduced spread of authored locations", authored > 0 && 0.65 * authored <= matched && layout.Count() <= authored, string.Format("%1 of %2 hand-built locations retained within 120 m", matched, authored));
  ACR_TestRegress.Check("Everon HQs far apart", hqs.Count() == 2 && vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos) >= 5000, "");
  ACR_TestRegress.Complete("generateeveron", EVERON_CHECKS);
 }
}

// Synthetic terrain fixtures exercise the real sampler and graph traversal without relying on
// a particular map's bridge assets. Physical bridge traversal still needs live acceptance.
class ACR_TestAccess : ACR_GenAccess
{
 int m_Mode;
 override protected bool Surface(vector p, bool road, out float height)
 {
  height = 10;
  if (m_Mode == 1 && p[0] >= 40 && p[0] <= 60) height = 40;
  if (m_Mode == 2) height += p[0] * 0.25;
  if (m_Mode >= 3 && p[0] >= 40 && p[0] <= 60) return road && m_Mode == 4;
  return true;
 }
 bool Leg(int mode, bool road = false)
 {
  m_Mode = mode;
  return PassableLeg(Vector(0, 0, 0), Vector(100, 0, 0), road);
 }
 void Fixture()
 {
  m_Mode = 0; m_fX = 0; m_fZ = 0; m_fCell = 100; m_iW = 3; m_iH = 1;
  m_Land = {1, 0, 1}; m_Links = {0, 0, 0};
  m_RoadPoints = {Vector(0, 10, 0), Vector(100, 10, 0), Vector(200, 10, 0)};
  m_Extra.Clear();
  AddEdge(0, 3); AddEdge(3, 4); AddEdge(4, 5); AddEdge(5, 2);
 }
 static void Run()
 {
  ACR_TestAccess access = new ACR_TestAccess();
  ACR_TestRegress.Check("access: flat and moderate slopes pass", access.Leg(0) && access.Leg(2) && access.Leg(2, true));
  ACR_TestRegress.Check("access: interior cliff rejected despite equal endpoint heights", !access.Leg(1) && !access.Leg(1, true));
  ACR_TestRegress.Check("access: water needs a verified road deck", !access.Leg(3) && !access.Leg(3, true) && !access.Leg(4) && access.Leg(4, true));
  access.Fixture();
  array<int> reached = {};
  access.Reach(Vector(0, 0, 0), Vector(100, 0, 500), reached, 20);
  bool bridge = access.CanReach(Vector(200, 0, 0), reached, Vector(100, 0, 500), 20);
  access.Reach(Vector(0, 0, 0), Vector(100, 0, 0), reached, 20);
  bool blocked = !access.CanReach(Vector(200, 0, 0), reached, Vector(100, 0, 0), 20);
  ACR_TestRegress.Check("access: bridge graph connects shores and respects enemy HQ", bridge && blocked);
  access.Reach(Vector(0, 0, 0), Vector(0, 0, 0), reached, 20);
  ACR_TestRegress.Check("access: starting connector cannot enter enemy HQ", !access.CanReach(Vector(200, 0, 0), reached, Vector(0, 0, 0), 20));
 }
}
