// Scenario generator tests (aca-f07).
// -acrGenerateTest 1 (srv:generate, Kolguyev, ~140 s): the world has no bases, so the generator
//   builds one at start. Checks the built layout, starting units and spawn points, that the same
//   seed plans the same layout, and the half / flashpoint sizes.
// -acrGenerateEveronTest 1 (srv:generateeveron, ~110 s): plans a layout on Everon (without building
//   it) and compares it with Jacob's hand-built bases.
class ACR_TestGenerate
{
 static const int CHECKS = 40;
 static const int EVERON_CHECKS = 23;

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
  if (tick >= 21 && tick <= 52) SeedBatch(tick - 21);
  if (tick == 53) { FinishBatch(); ACR_TestRegress.Complete("generate", CHECKS); }
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
  // Other seeds vary actual HQ pairs, ignoring faction swaps.
  array<string> pairs = {};
  for (int seed = 1; seed <= 6; seed++)
  {
   array<ref ACR_GenSite> other = {};
   array<ACR_GenSite> otherHQs = {};
   if (!ACR_Generator.Plan(seed * 7919, -1, other, otherHQs)) continue;
   array<string> pairIds = {otherHQs[0].m_sId, otherHQs[1].m_sId};
   pairIds.Sort();
   string pair = pairIds[0] + "/" + pairIds[1];
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
  BeginBatch();
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
  ACR_TestRegress.Check("generator: half-map seeds produce repeatable reduced layouts", planned && repeatable, "45289, 37941 and four additional seeds");
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

 // Compare actual non-landmark geography, ignoring array order and faction swaps.
 protected static ref array<string> s_Footprints = {};
 protected static ref array<string> s_Pairs = {};
 protected static ref array<ref array<string>> s_Objectives = {};
 protected static ref array<ref array<string>> s_Regions = {};
 protected static bool s_BatchValid, s_BatchRepeat, s_BatchSafe;
 protected static float s_MinOverlap;
 protected static int s_MaxPlanMs;
 protected static ref ACR_GenAccess s_BatchAccess;
 protected static bool s_BatchReachable;
 protected static ref array<string> s_FullIncome = {}, s_FlashShapes = {}, s_FixedIncome = {}, s_TacticalShapes = {};
 protected static ref array<int> s_FlashCounts = {};
 protected static bool s_FullValid, s_FlashValid, s_BudgetValid, s_OpeningValid;
 protected static ref array<ref ACR_GenSite> s_Fixture = {};
 protected static ref array<ACR_GenSite> s_FixtureHQs = {};
 protected static int s_FixtureValue, s_FixtureEarners;
 protected static ref array<ref ACR_GenSite> s_Local = {};
 protected static ref array<ACR_GenSite> s_LocalHQs = {};
 protected static ref array<string> s_LocalShapes = {};

 protected static void BeginBatch()
 {
  s_Footprints.Clear(); s_Pairs.Clear(); s_Objectives.Clear(); s_Regions.Clear();
  s_BatchValid = true; s_BatchRepeat = true; s_BatchSafe = true;
  s_MinOverlap = 1; s_MaxPlanMs = 0; s_BatchReachable = true;
  array<ref ACR_GenSite> surveyed = {};
  ACR_Generator.Survey(surveyed);
  s_BatchAccess = new ACR_GenAccess();
  s_BatchAccess.Build(surveyed);
  s_FullIncome.Clear(); s_FlashShapes.Clear(); s_FixedIncome.Clear(); s_TacticalShapes.Clear(); s_FlashCounts.Clear();
  s_LocalShapes.Clear();
  bool localOk = ACR_TestGeneratorChoices.LocalFixture(surveyed, s_Local, s_LocalHQs);
  s_FullValid = true; s_FlashValid = localOk; s_BudgetValid = true; s_OpeningValid = true;
  array<ref ACR_GenSite> fixturePlan = {};
  array<ACR_GenSite> fixturePlanHQs = {};
  ACR_Generator.Plan(4242, -1, fixturePlan, fixturePlanHQs, s_Fixture);
  s_FixtureHQs.Clear();
  foreach (ACR_GenSite anchor : fixturePlanHQs)
   foreach (ACR_GenSite snapshot : s_Fixture)
    if (snapshot.m_sId == anchor.m_sId) s_FixtureHQs.Insert(snapshot);
  ACR_Generator.AssignEconomy(s_Fixture, s_FixtureHQs, false);
  s_FixtureValue = 0; s_FixtureEarners = 0;
  foreach (ACR_GenSite baseline : s_Fixture) if (baseline.m_bIncome) { s_FixtureValue += baseline.m_iValue; s_FixtureEarners++; }
  PrintFormat("[ACR] GEN INCOME FIXTURE %1 sites, %2 earning, RP %3", s_Fixture.Count(), s_FixtureEarners, s_FixtureValue);
  foreach (ACR_GenSite diagnostic : s_Fixture) PrintFormat("[ACR] GEN INCOME CANDIDATE %1 | %2 %3 | value %4 income %5 opening %6", diagnostic.m_sId, (int)diagnostic.m_vPos[0], (int)diagnostic.m_vPos[2], diagnostic.m_iValue, diagnostic.m_bIncome, diagnostic.m_iOpeningSide);
 }

 protected static void SeedBatch(int index)
 {
  // Consecutive seeds as well as widely spaced values; no favourable-seed selection.
  int seed = index + 1;
  if (index >= 16) seed = (index - 15) * 7919;
  array<ref ACR_GenSite> layout = {}, again = {};
  array<ACR_GenSite> hqs = {}, againHQs = {};
  int started = System.GetTickCount();
  bool ok = ACR_Generator.Plan(seed, ACR_ESkirmishSize.HALF, layout, hqs);
  int elapsed = System.GetTickCount() - started;
  s_MaxPlanMs = Math.Max(s_MaxPlanMs, elapsed);
  if (!ok || hqs.Count() != 2 || layout.Count() < 6) { s_BatchValid = false; return; }
  // All seeds reproduce, including income and faction assignment.
  if (!ACR_Generator.Plan(seed, ACR_ESkirmishSize.HALF, again, againHQs) || Signature(layout, hqs) != Signature(again, againHQs)) s_BatchRepeat = false;
  array<string> ids = {}, pair = {hqs[0].m_sId, hqs[1].m_sId};
  pair.Sort();
  string pairKey = pair[0] + "/" + pair[1];
  if (!s_Pairs.Contains(pairKey)) s_Pairs.Insert(pairKey);
  array<int> westReach = {}, eastReach = {};
  s_BatchAccess.Reach(hqs[0].m_vPos, hqs[1].m_vPos, westReach);
  s_BatchAccess.Reach(hqs[1].m_vPos, hqs[0].m_vPos, eastReach);
  foreach (ACR_GenSite site : layout)
  {
   if (!hqs.Contains(site) && (!s_BatchAccess.CanReach(site.m_vPos, westReach, hqs[1].m_vPos) || !s_BatchAccess.CanReach(site.m_vPos, eastReach, hqs[0].m_vPos))) s_BatchReachable = false;
   if (site.m_sKind != "landmark") ids.Insert(site.m_sId);
   if (BBB_Terrain.IsWater(site.m_vPos[0], site.m_vPos[2])) s_BatchSafe = false;
   if (hqs.Contains(site))
   {
    if (site.m_sKind == "landmark" || !site.m_bIncome || site.m_iValue != 2) s_BatchSafe = false;
   }
   else if (!ACR_Generator.BetweenHQs(site.m_vPos, hqs[0].m_vPos, hqs[1].m_vPos)) s_BatchSafe = false;
   foreach (ACR_GenSite other : layout)
    if (other != site && vector.DistanceXZ(site.m_vPos, other.m_vPos) < Math.Max(250, site.m_fRadius + other.m_fRadius)) s_BatchSafe = false;
  }
  ids.Sort();
  string footprint = "";
  foreach (string id : ids) footprint += id + ";";
  if (!s_Footprints.Contains(footprint)) s_Footprints.Insert(footprint);
  foreach (array<string> prior : s_Objectives)
  {
   float common = 0;
   foreach (string current : ids) if (prior.Contains(current)) common++;
   float unionCount = ids.Count() + prior.Count() - common;
   if (unionCount > 0) s_MinOverlap = Math.Min(s_MinOverlap, common / unionCount);
  }
  bool differentRegion = true;
  foreach (array<string> region : s_Regions)
  {
   float shared = 0;
   foreach (string member : ids) if (region.Contains(member)) shared++;
   float combined = ids.Count() + region.Count() - shared;
   if (combined > 0 && shared / combined > 0.8) differentRegion = false;
  }
  if (differentRegion) s_Regions.Insert(ids);
  s_Objectives.Insert(ids);
  ObjectiveBatch(seed);
  PrintFormat("[ACR] GEN BATCH seed %1: %2 sites, HQ %3, plan %4 ms, footprint %5", seed, layout.Count(), pairKey, elapsed, footprint);
 }

 protected static string SiteKey(array<ref ACR_GenSite> layout, bool incomeOnly)
 {
  array<string> ids = {};
  foreach (ACR_GenSite site : layout)
   if (site.m_sKind != "landmark" && (!incomeOnly || site.m_bIncome)) ids.Insert(site.m_sId);
  ids.Sort();
  string key = "";
  foreach (string id : ids) key += id + ";";
  return key;
 }

 protected static bool OpeningIncome(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, bool flash)
 {
  foreach (ACR_GenSite hq : hqs)
   if (!hq.m_bIncome || hq.m_iValue != 2 || hq.m_sKind == "landmark") return false;
  foreach (ACR_GenSite site : layout)
  {
   if (site.m_sKind == "landmark" && site.m_bIncome) return false;
   if (hqs.Contains(site) || site.m_sKind == "landmark") continue;
   int home = 0;
   float distance = vector.DistanceXZ(site.m_vPos, hqs[0].m_vPos);
   if (vector.DistanceXZ(site.m_vPos, hqs[1].m_vPos) < distance) { home = 1; distance = vector.DistanceXZ(site.m_vPos, hqs[1].m_vPos); }
   int closer = 0;
   foreach (ACR_GenSite other : layout)
   {
    if (hqs.Contains(other) || other.m_sKind == "landmark") continue;
    float away = vector.DistanceXZ(other.m_vPos, hqs[home].m_vPos);
    if (away < distance && away <= vector.DistanceXZ(other.m_vPos, hqs[1 - home].m_vPos)) closer++;
   }
   int starters = 4;
   if (flash) starters = 2;
   if ((!flash && distance <= 2000 || closer < starters) && !site.m_bIncome) { PrintFormat("[ACR] GEN OPENING FAIL %1: flash %2 home %3 distance %4 closer %5 HQ %6 / %7", site.m_sId, flash, home, distance, closer, hqs[0].m_sId, hqs[1].m_sId); return false; }
  }
  return true;
 }

 protected static void ObjectiveBatch(int seed)
 {
  array<ref ACR_GenSite> full = {}, flash = {}, repeat = {};
  array<ACR_GenSite> fullHQs = {}, flashHQs = {}, repeatHQs = {};
  bool fullOk = ACR_Generator.Plan(seed, -1, full, fullHQs);
  if (!fullOk || fullHQs.Count() != 2 || !OpeningIncome(full, fullHQs, false)) s_FullValid = false;
  string fullKey = SiteKey(full, true);
  if (!s_FullIncome.Contains(fullKey)) s_FullIncome.Insert(fullKey);
  // Full-map repeats supplement all 32 exact half/flash repeats without tripling graph cost.
  if (seed <= 4 && (!ACR_Generator.Plan(seed, -1, repeat, repeatHQs) || Signature(full, fullHQs) != Signature(repeat, repeatHQs))) s_FullValid = false;
  bool flashOk = ACR_Generator.Plan(seed, ACR_ESkirmishSize.FLASHPOINT, flash, flashHQs);
  if (!flashOk || flashHQs.Count() != 2 || flash.Count() < 5 || flash.Count() > 7) { s_FlashValid = false; return; }
  if (!ACR_Generator.Plan(seed, ACR_ESkirmishSize.FLASHPOINT, repeat, repeatHQs) || Signature(flash, flashHQs) != Signature(repeat, repeatHQs) || !OpeningIncome(flash, flashHQs, true)) s_FlashValid = false;
  array<int> westReach = {}, eastReach = {};
  s_BatchAccess.Reach(flashHQs[0].m_vPos, flashHQs[1].m_vPos, westReach, 100);
  s_BatchAccess.Reach(flashHQs[1].m_vPos, flashHQs[0].m_vPos, eastReach, 100);
  foreach (ACR_GenSite site : flash)
  {
   if (BBB_Terrain.IsWater(site.m_vPos[0], site.m_vPos[2])) s_FlashValid = false;
   if (!flashHQs.Contains(site) && (!ACR_Generator.BetweenHQs(site.m_vPos, flashHQs[0].m_vPos, flashHQs[1].m_vPos) || !s_BatchAccess.CanReach(site.m_vPos, westReach, flashHQs[1].m_vPos, 100) || !s_BatchAccess.CanReach(site.m_vPos, eastReach, flashHQs[0].m_vPos, 100))) s_FlashValid = false;
   foreach (ACR_GenSite neighbour : flash)
    if (site != neighbour && (vector.DistanceXZ(site.m_vPos, neighbour.m_vPos) < site.m_fRadius + neighbour.m_fRadius || vector.DistanceXZ(site.m_vPos, neighbour.m_vPos) > 3601)) s_FlashValid = false;
  }
  string flashKey = SiteKey(flash, false);
  if (!s_FlashShapes.Contains(flashKey)) s_FlashShapes.Insert(flashKey);
  if (!s_FlashCounts.Contains(flash.Count())) s_FlashCounts.Insert(flash.Count());
  // Hold positions and HQs fixed: randomness must change actual later income sites,
  // with exactly the baseline count/value and every original opening still present.
  array<ref ACR_GenSite> fixture = {};
  array<ACR_GenSite> fixtureHQs = {};
  foreach (ACR_GenSite original : s_Fixture)
  {
   ACR_GenSite copy = original.Copy();
   fixture.Insert(copy);
   if (s_FixtureHQs.Contains(original)) fixtureHQs.Insert(copy);
  }
  ACR_Generator.AssignEconomy(fixture, fixtureHQs, false, seed);
  int earners, value;
  foreach (ACR_GenSite current : fixture)
  {
   if (current.m_bIncome) { earners++; value += current.m_iValue; }
   if (current.m_bIncome && current.m_iOpeningSide < 0 && !fixtureHQs.Contains(current))
    foreach (ACR_GenSite neighbourIncome : fixture)
     if (neighbourIncome != current && neighbourIncome.m_bIncome && vector.DistanceXZ(current.m_vPos, neighbourIncome.m_vPos) < 800) s_BudgetValid = false;
   foreach (ACR_GenSite protectedSite : s_Fixture)
    if (protectedSite.m_sId == current.m_sId && protectedSite.m_iOpeningSide >= 0 && !current.m_bIncome) { s_OpeningValid = false; PrintFormat("[ACR] GEN PROTECTED LOST seed %1 site %2", seed, current.m_sId); }
  }
  if (value != s_FixtureValue || earners != s_FixtureEarners) s_BudgetValid = false;
  string fixedKey = SiteKey(fixture, true);
  if (!s_FixedIncome.Contains(fixedKey)) s_FixedIncome.Insert(fixedKey);
  RandomGenerator rng = new RandomGenerator(); rng.SetSeed(seed);
  ACR_TestGeneratorChoices.Select(fixture, fixtureHQs, rng);
  string tacticalKey = SiteKey(fixture, false);
  if (!s_TacticalShapes.Contains(tacticalKey)) s_TacticalShapes.Insert(tacticalKey);
  if (!OpeningIncome(fixture, fixtureHQs, false)) s_OpeningValid = false;
  int retainedEarners, retainedValue;
  foreach (ACR_GenSite retained : fixture) if (retained.m_bIncome) { retainedEarners++; retainedValue += retained.m_iValue; }
  if (retainedEarners != earners || retainedValue != value) s_BudgetValid = false;
  array<ref ACR_GenSite> local = {};
  array<ACR_GenSite> localHQs = {};
  foreach (ACR_GenSite localOriginal : s_Local)
  {
   ACR_GenSite localCopy = localOriginal.Copy();
   local.Insert(localCopy);
   if (s_LocalHQs.Contains(localOriginal)) localHQs.Insert(localCopy);
  }
  if (localHQs.Count() == 2)
  {
   ACR_Generator.AssignEconomy(local, localHQs, true);
   array<string> protectedLocal = {};
   foreach (ACR_GenSite opening : local) if (opening.m_iOpeningSide >= 0) protectedLocal.Insert(opening.m_sId);
   rng.SetSeed(seed);
   ACR_TestGeneratorChoices.SelectFlash(local, localHQs, rng);
   if (local.Count() < 5 || local.Count() > 7 || !OpeningIncome(local, localHQs, true)) s_FlashValid = false;
   foreach (string required : protectedLocal)
   {
    bool found = false;
    foreach (ACR_GenSite kept : local) if (kept.m_sId == required && kept.m_bIncome) found = true;
    if (!found) s_FlashValid = false;
   }
   string localKey = SiteKey(local, false);
   if (!s_LocalShapes.Contains(localKey)) s_LocalShapes.Insert(localKey);
  }
  else s_FlashValid = false;
  PrintFormat("[ACR] GEN CONTESTED seed %1: full income %2 | fixed income %3 | flash %4 sites %5", seed, fullKey, fixedKey, flash.Count(), flashKey);
 }

 protected static void IncomeSearchChecks()
 {
  // Neither C nor D can replace A or B alone: each is <800 m from BOTH originals.
  // Together C/D are valid, and trade the 1+3 value mix for 2+2 without changing any value.
  array<ref ACR_GenSite> fixture = {};
  array<ACR_GenSite> hqs = {};
  array<vector> positions = {Vector(-5000, 0, 0), Vector(5000, 0, 0), Vector(0, 0, 0), Vector(1000, 0, 0), Vector(500, 0, 600), Vector(500, 0, -600)};
  array<int> values = {2, 2, 1, 3, 2, 2};
  for (int i = 0; i < positions.Count(); i++)
  {
   ACR_GenSite site = new ACR_GenSite();
   site.m_sId = i.ToString(); site.m_sKind = "industry"; site.m_vPos = positions[i]; site.m_iValue = values[i];
   site.m_bIncome = i < 4;
   fixture.Insert(site);
   if (i < 2) hqs.Insert(site);
  }
  string before = Signature(fixture, hqs);
  ACR_TestGeneratorChoices.Income(fixture, hqs, 7919, 0);
  bool fallback = before == Signature(fixture, hqs);
  bool joint = false, budgets = true;
  for (int seed = 1; seed <= 32; seed++)
  {
   for (int reset = 0; reset < fixture.Count(); reset++) fixture[reset].m_bIncome = reset < 4;
   ACR_TestGeneratorChoices.Income(fixture, hqs, seed, 4096);
   if (fixture[4].m_bIncome && fixture[5].m_bIncome && !fixture[2].m_bIncome && !fixture[3].m_bIncome) joint = true;
   int count = 0, value = 0;
   foreach (int n, ACR_GenSite earning : fixture)
   {
    if (earning.m_bIncome) { count++; value += earning.m_iValue; }
    if (earning.m_iValue != values[n]) budgets = false;
   }
   if (count != 4 || value != 8 || !fixture[0].m_bIncome || !fixture[1].m_bIncome) budgets = false;
  }
  ACR_TestRegress.Check("income: joint relocation escapes single-swap blockage with exact budget", joint && budgets, "A+B 1+3 -> C+D 2+2, all site values and HQs unchanged");
  ACR_TestRegress.Check("income: exhausted search leaves original flags intact", fallback, "zero-node budget, atomic replacement only on success");
 }

 protected static void IncomeCycleChecks()
 {
  array<ref ACR_GenSite> fixture = {};
  array<ACR_GenSite> hqs = {};
  foreach (ACR_GenSite original : s_Fixture)
  {
   ACR_GenSite copy = original.Copy(); fixture.Insert(copy);
   if (s_FixtureHQs.Contains(original)) hqs.Insert(copy);
  }
  array<string> patterns = {};
  bool safe = true;
  for (int seed = 1; seed <= 76; seed++)
  {
   ACR_Generator.AssignEconomy(fixture, hqs, false, seed);
   int count = 0, value = 0;
   foreach (ACR_GenSite site : fixture) if (site.m_bIncome) { count++; value += site.m_iValue; }
   if (count != s_FixtureEarners || value != s_FixtureValue || !OpeningIncome(fixture, hqs, false)) safe = false;
   string key = SiteKey(fixture, true);
   if (!patterns.Contains(key)) patterns.Insert(key);
  }
  PrintFormat("[ACR] GEN INCOME CYCLE 76 consecutive seeds: %1 distinct, %2 earning, RP %3, safe %4", patterns.Count(), s_FixtureEarners, s_FixtureValue, safe);
  ACR_TestRegress.Check("income: consecutive seeds cover a broad fixed-HQ portfolio cycle", safe && patterns.Count() >= 32, patterns.Count().ToString());
 }

 protected static void FinishBatch()
 {
  string summary = string.Format("32 seeds: %1 footprints, %2 unordered HQ pairs, min overlap %3, max plan %4 ms", s_Footprints.Count(), s_Pairs.Count(), s_MinOverlap, s_MaxPlanMs);
  IncomeSearchChecks();
  IncomeCycleChecks();
  PrintFormat("[ACR] GEN DIVERSITY %1; %2 regions at <=80 percent objective overlap", summary, s_Regions.Count());
  ACR_TestRegress.Check("seed batch: all half layouts remain playable", s_BatchValid && s_Objectives.Count() == 32, summary);
  ACR_TestRegress.Check("seed batch: access from both sides avoids enemy HQ", s_BatchReachable, summary);
  ACR_TestRegress.Check("seed batch: exact repeatability", s_BatchRepeat, summary);
  ACR_TestRegress.Check("seed batch: dry spaced forward objectives and valid HQs", s_BatchSafe, summary);
  ACR_TestRegress.Check("seed batch: at least 16 distinct non-landmark footprints", s_Footprints.Count() >= 16 && s_MinOverlap < 0.5, summary);
  ACR_TestRegress.Check("seed batch: at least 8 substantially different regions", s_Regions.Count() >= 8, s_Regions.Count().ToString());
  ACR_TestRegress.Check("seed batch: at least 6 HQ pairs without counting side swaps", s_Pairs.Count() >= 6, summary);
  string contested = string.Format("32 seeds: %1 full income sets, %2 fixed-HQ income sets, %3 tactical subsets, %4 flash layouts, %5 flash sizes", s_FullIncome.Count(), s_FixedIncome.Count(), s_TacticalShapes.Count(), s_FlashShapes.Count(), s_FlashCounts.Count());
  PrintFormat("[ACR] GEN CONTESTED DIVERSITY %1; %2 fixed-local flash layouts", contested, s_LocalShapes.Count());
  ACR_TestRegress.Check("contested: full-map openings and repeats", s_FullValid, contested);
  ACR_TestRegress.Check("contested: full-map economic objectives vary", s_FullIncome.Count() >= 8, contested);
  ACR_TestRegress.Check("contested: exact fixed-layout income budget survives selection", s_BudgetValid, contested);
  ACR_TestRegress.Check("contested: every fixed opening retains income", s_OpeningValid, contested);
  ACR_TestRegress.Check("contested: fixed HQs still vary economic destinations", s_FixedIncome.Count() >= 16, contested);
  ACR_TestRegress.Check("contested: fixed region still varies tactical objectives", s_TacticalShapes.Count() >= 3, contested);
  ACR_TestRegress.Check("flash: all 32 plans repeat with safe compact accessible openings", s_FlashValid, contested);
  ACR_TestRegress.Check("flash: at least 16 distinct ordinary objective sets", s_FlashShapes.Count() >= 16, contested);
  ACR_TestRegress.Check("flash: same neighbourhood and HQs vary ordinary objectives", s_LocalShapes.Count() >= 3, s_LocalShapes.Count().ToString());
  ACR_TestRegress.Check("flash: objective count varies within 5-7", s_FlashCounts.Count() >= 2, contested);
 }

 // Everon: plan a full layout and compare it with the hand-built one.
 static void RunEveron(int tick)
 {
  if (tick >= 6 && tick <= 37) SeedBatch(tick - 6);
  if (tick == 38) { FinishBatch(); ACR_TestRegress.Complete("generateeveron", EVERON_CHECKS); }
  if (tick != 5) return;
  array<ref ACR_GenSite> layout = {};
  array<ACR_GenSite> hqs = {};
  bool ok = ACR_Generator.Plan(4242, -1, layout, hqs);
  int matched, authored;
  float authoredMinX = float.MAX, authoredMinZ = float.MAX, authoredMaxX = -float.MAX, authoredMaxZ = -float.MAX;
  float planMinX = float.MAX, planMinZ = float.MAX, planMaxX = -float.MAX, planMaxZ = -float.MAX;
  foreach (ACR_GenSite extent : layout)
  {
   if (extent.m_sKind == "landmark") continue;
   planMinX = Math.Min(planMinX, extent.m_vPos[0]); planMaxX = Math.Max(planMaxX, extent.m_vPos[0]);
   planMinZ = Math.Min(planMinZ, extent.m_vPos[2]); planMaxZ = Math.Max(planMaxZ, extent.m_vPos[2]);
  }
  string missing = "";
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bBuilt) continue;
   authored++;
   vector authoredAt = base.GetOrigin();
   authoredMinX = Math.Min(authoredMinX, authoredAt[0]); authoredMaxX = Math.Max(authoredMaxX, authoredAt[0]);
   authoredMinZ = Math.Min(authoredMinZ, authoredAt[2]); authoredMaxZ = Math.Max(authoredMaxZ, authoredAt[2]);
   bool found = false;
   foreach (ACR_GenSite site : layout) if (vector.DistanceXZ(site.m_vPos, base.GetOrigin()) < 120) found = true;
   if (found) matched++;
   else missing += base.m_sName + ", ";
  }
  foreach (ACR_GenSite s : layout) PrintFormat("[ACR] GEN EVERON %1 | %2 | %3 %4 | v%5 r%6", s.m_sName, s.m_sKind, (int)s.m_vPos[0], (int)s.m_vPos[2], s.m_iValue, (int)s.m_fRadius);
  if (hqs.Count() == 2) PrintFormat("[ACR] GEN EVERON HQs %1 / %2 (%3 m)", hqs[0].m_sName, hqs[1].m_sName, (int)vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos));
  PrintFormat("[ACR] GEN EVERON not generated: %1", missing);
  ACR_TestRegress.Check("Everon plan made", ok && layout.Count() >= 30, string.Format("%1 sites", layout.Count()));
  // Selective objectives deliberately omit authored CPs; retain at least half the named
  // matches and >=75% of the authored extent on BOTH axes, plus the existing HQ span check.
  bool broad = 0.75 * (authoredMaxX - authoredMinX) <= planMaxX - planMinX && 0.75 * (authoredMaxZ - authoredMinZ) <= planMaxZ - planMinZ;
  ACR_TestRegress.Check("Everon sampled objectives retain broad authored geography", authored > 0 && 0.5 * authored <= matched && layout.Count() <= authored && broad, string.Format("%1 of %2 locations, broad %3", matched, authored, broad));
  ACR_TestRegress.Check("Everon HQs far apart", hqs.Count() == 2 && vector.DistanceXZ(hqs[0].m_vPos, hqs[1].m_vPos) >= 5000, "");
  BeginBatch();
 }

 // -acrGenerateMapTest 1 (srv:genmap<name>, any world without bases, ~60 s): the generator's
 // basic promises on a map nobody tuned it for (third-party terrains in the ACA map add-ons).
 // Thresholds scale with the surveyed extent; no names or positions are assumed.
 static void RunAnyMap(int tick)
 {
  if (tick == 2) ACR_TestRegress.Check("map: generator built a layout", ACR_Generator.Built() && ACR_Base.s_All.Count() >= 8, string.Format("%1 bases, seed %2, %3", ACR_Base.s_All.Count(), ACR_Generator.s_iSeed, GetGame().GetWorldFile()));
  if (tick == 21) ACR_TestRegress.Complete("generatemap", MAP_CHECKS);
  if (tick != 20) return;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  ACR_Base westHQ, eastHQ;
  bool wet, overlap;
  float minX = float.MAX, minZ = float.MAX, maxX = -float.MAX, maxZ = -float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   vector at = base.GetOrigin();
   if (base.m_sOwner == west) westHQ = base;
   else if (base.m_sOwner == east) eastHQ = base;
   if (BBB_Terrain.IsWater(at[0], at[2])) wet = true;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other != base && vector.DistanceXZ(other.GetOrigin(), at) < other.m_fRadius + base.m_fRadius) overlap = true;
   minX = Math.Min(minX, at[0]); maxX = Math.Max(maxX, at[0]);
   minZ = Math.Min(minZ, at[2]); maxZ = Math.Max(maxZ, at[2]);
   PrintFormat("[ACR] GEN MAP %1 | %2 %3 | v%4 r%5 income %6 hidden %7 owner %8", base.m_sName, (int)at[0], (int)at[2], base.m_iValue, (int)base.m_fRadius, base.m_bIncome, base.m_bHidden, base.m_sOwner);
  }
  float span = Math.Max(maxX - minX, maxZ - minZ);
  float apart = 0;
  if (westHQ && eastHQ) apart = vector.DistanceXZ(westHQ.GetOrigin(), eastHQ.GetOrigin());
  ACR_TestRegress.Check("map: one HQ per side, far apart", westHQ && eastHQ && apart >= 1000 && apart >= 0.4 * span, string.Format("%1 m apart, layout span %2 m", (int)apart, (int)span));
  ACR_TestRegress.Check("map: bases on dry ground, circles apart", !wet && !overlap, string.Format("wet %1, overlap %2", wet, overlap));
  int earning;
  foreach (ACR_Base e : ACR_Base.s_All) if (e.m_bIncome && e.m_sOwner == "") earning++;
  ACR_TestRegress.Check("map: neutral objectives earn income", earning >= 2, string.Format("%1 earning", earning));
  ACR_Manager manager = ACR_Manager.GetInstance();
  int westNear, eastNear;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.m_Group || !squad.GetFaction()) continue;
   vector sq = squad.m_Group.GetOrigin();
   string squadSide = squad.GetFaction().GetFactionKey();
   if (squadSide == west && westHQ && vector.DistanceXZ(sq, westHQ.GetOrigin()) < 250) westNear++;
   if (squadSide == east && eastHQ && vector.DistanceXZ(sq, eastHQ.GetOrigin()) < 250) eastNear++;
  }
  ACR_TestRegress.Check("map: starting units at each HQ", westNear >= 3 && eastNear >= 3, string.Format("west %1, east %2", westNear, eastNear));
  int spawnsOk, spawns;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   spawns++;
   ACR_Base hq = westHQ;
   if (point.GetFactionKey() == east) hq = eastHQ;
   if (hq && vector.DistanceXZ(point.GetOrigin(), hq.GetOrigin()) < 60) spawnsOk++;
  }
  ACR_TestRegress.Check("map: spawn points at their HQ", spawns >= 2 && spawnsOk == spawns, string.Format("%1 of %2", spawnsOk, spawns));
  array<ref ACR_GenSite> again = {};
  array<ACR_GenSite> againHQs = {};
  bool same = ACR_Generator.Plan(ACR_Generator.s_iSeed, -1, again, againHQs) && again.Count() == ACR_Generator.s_Sites.Count();
  foreach (int i, ACR_GenSite a : again) if (same && (a.m_sName != ACR_Generator.s_Sites[i].m_sName || vector.DistanceXZ(a.m_vPos, ACR_Generator.s_Sites[i].m_vPos) > 1)) same = false;
  ACR_TestRegress.Check("map: same seed plans the same layout", same, string.Format("%1 vs %2 sites", again.Count(), ACR_Generator.s_Sites.Count()));
  array<string> pairs = {};
  for (int seed = 1; seed <= 6; seed++)
  {
   array<ref ACR_GenSite> other = {};
   array<ACR_GenSite> otherHQs = {};
   if (!ACR_Generator.Plan(seed * 7919, -1, other, otherHQs) || otherHQs.Count() != 2) continue;
   array<string> pairIds = {otherHQs[0].m_sId, otherHQs[1].m_sId};
   pairIds.Sort();
   string pair = pairIds[0] + "/" + pairIds[1];
   if (!pairs.Contains(pair)) pairs.Insert(pair);
  }
  ACR_TestRegress.Check("map: seeds vary the HQs", pairs.Count() >= 2, string.Format("%1 HQ pairs in 6 seeds", pairs.Count()));
  // Half and flashpoint on 8 fixed seeds each: islands and narrow maps fail only on some draws.
  int halfGood, flashGood, halfMin = int.MAX;
  for (int s = 1; s <= 8; s++)
  {
   array<ref ACR_GenSite> half = {};
   array<ACR_GenSite> halfHQs = {};
   if (ACR_Generator.Plan(s * 104729, ACR_ESkirmishSize.HALF, half, halfHQs) && half.Count() >= 5 && halfHQs.Count() == 2 && halfHQs[0] != halfHQs[1]) { halfGood++; halfMin = Math.Min(halfMin, half.Count()); }
   array<ref ACR_GenSite> flash = {};
   array<ACR_GenSite> flashHQs = {};
   if (ACR_Generator.Plan(s * 104729, ACR_ESkirmishSize.FLASHPOINT, flash, flashHQs) && flash.Count() >= 5 && flash.Count() <= 7 && flashHQs.Count() == 2) flashGood++;
  }
  ACR_TestRegress.Check("map: half size on 8 seeds", halfGood == 8, string.Format("%1 of 8 planned, smallest %2 of %3 sites", halfGood, halfMin, ACR_Generator.s_Sites.Count()));
  ACR_TestRegress.Check("map: flashpoint size on 8 seeds", flashGood == 8, string.Format("%1 of 8 planned", flashGood));
  int unnamed;
  foreach (ACR_Base named : ACR_Base.s_All) if (named.m_sName == "" || named.m_sName.StartsWith("Base ") || named.m_sName.StartsWith("#")) unnamed++;
  ACR_TestRegress.Check("map: bases named from the map", unnamed == 0, string.Format("%1 unnamed", unnamed));
 }
 static const int MAP_CHECKS = 11;
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

// Fixed real-map fixtures isolate objective variation from changing HQs or battlefield shape.
class ACR_TestGeneratorChoices : ACR_Generator
{
 static void Income(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, int seed, int steps)
 {
  VaryIncome(layout, hqs, false, seed, steps);
 }
 static void Select(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, RandomGenerator rng)
 {
  SelectContested(layout, hqs, false, rng);
 }
 static void SelectFlash(array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs, RandomGenerator rng)
 {
  SelectContested(layout, hqs, true, rng);
 }
 // Actual surveyed, dry, non-overlapping sites and real access screening. Keep a sufficiently
 // rich local pool fixed to distinguish local selection from merely choosing a different town.
 static bool LocalFixture(array<ref ACR_GenSite> survey, array<ref ACR_GenSite> layout, array<ACR_GenSite> hqs)
 {
  array<ref ACR_GenSite> spaced = {};
  for (int rank = 5; rank >= 0; rank--)
   foreach (ACR_GenSite source : survey)
   {
    if (source.m_iRank != rank || source.m_sKind == "landmark") continue;
    ACR_GenSite copy = source.Copy();
    if (Dry(copy) && Fit(spaced, copy)) spaced.Insert(copy);
   }
  RandomGenerator rng = new RandomGenerator(); rng.SetSeed(4242);
  for (int attempt = 0; attempt < 20; attempt++)
  {
   array<ACR_GenSite> pool = {};
   foreach (ACR_GenSite candidate : spaced) pool.Insert(candidate);
   pool = Flashpoint(pool, rng);
   ACR_GenSite west, east;
   PickHQs(pool, true, rng, west, east);
   if (!west || !east) continue;
   hqs.Clear(); hqs.Insert(west); hqs.Insert(east);
   FairObjectives(pool, hqs, true);
   if (pool.Count() < 8) continue;
   layout.Clear();
   foreach (ACR_GenSite kept : pool) layout.Insert(kept);
   return true;
  }
  return false;
 }
}
