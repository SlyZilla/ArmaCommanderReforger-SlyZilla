// -acrMineSitingTest 1 (server-capable, ~40 s): the AI's minefield spots (ACR_MineSiting). With a
// (pretend) roadblock of ours at the best road site of our first front base: the best spot is on
// the road ahead of it, mixed; no spot lies within DEFENCE_STANDOFF of it; the AI lays its field at
// one of the best spots. With no defence of ours there, the old ring still gives spots and the AI
// still lays a field (never fewer). And the score's terms: covered, dead ground, the enemy's route.
class ACR_TestMineSiting
{
 static const int CHECKS = 5;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!us)
  {
   ACR_TestRegress.Abort("minesiting", "no US faction data");
   return;
  }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("fortress"));
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  ai.FrontBasesForTest(fronts, enemies);
  if (fronts.IsEmpty())
  {
   ACR_TestRegress.Abort("minesiting", "no US front base");
   return;
  }
  vector basePos = fronts[0].GetOrigin();
  vector enemyPos = enemies[0];
  string aheadName = "minesiting: the best minefield spot is on the road ahead of our roadblock, mixed";
  string apartName = "minesiting: no minefield spot within 60 m of our roadblock";
  string laidName = "minesiting: the AI lays its field at one of the best spots";

  // 1-3. A roadblock of ours at the base's best road site (a real entity: ValidateSite prunes
  // roadblocks without one).
  int considered;
  string none;
  ACR_RoadSite road = ACR_RoadblockSiting.Best(basePos, enemyPos, "US", false, considered, none);
  if (!road)
  {
   ACR_TestRegress.Skip(aheadName, "no road site at " + fronts[0].m_sName + ": " + none);
   ACR_TestRegress.Skip(apartName, "no road site");
   ACR_TestRegress.Skip(laidName, "no road site");
  }
  else
  {
   ACR_Roadblock pretend = new ACR_Roadblock();
   pretend.m_sFaction = "US";
   pretend.m_vPos = road.m_vPos;
   pretend.m_fYaw = road.m_fYaw;
   pretend.m_Entity = ACR_Manager.SpawnPrefab(ACR_Roadblocks.LOGS_1, road.m_vPos);
   ACR_Roadblocks.s_aRoadblocks.Insert(pretend);
   array<ref ACR_MineSite> sites = {};
   int crowded;
   int t0 = System.GetTickCount();
   ACR_MineSiting.Candidates(ai, "US", basePos, enemyPos, sites, crowded);
   int cost = System.GetTickCount() - t0;
   for (int k = 0; k < sites.Count() && k < 5; k++) PrintFormat("[ACR] MINESITING %1: %2 at %3, %4 m from our roadblock", k, ACR_MineSiting.Breakdown(sites[k]), sites[k].m_vPos, (int)vector.DistanceXZ(sites[k].m_vPos, pretend.m_vPos));
   ACR_MineSite top;
   if (!sites.IsEmpty()) top = sites[0];
   bool ahead = top && top.m_sSource == "roadblock" && top.m_eKind == ACR_EMineKind.MIXED;
   string topDetail = "no spot";
   if (top) topDetail = string.Format("%1 at %2, %3 m from the roadblock at %4", ACR_MineSiting.Breakdown(top), top.m_vPos, (int)vector.DistanceXZ(top.m_vPos, pretend.m_vPos), pretend.m_vPos);
   ACR_TestRegress.Check(aheadName, ahead, string.Format("%1; %2 spots scored in %3 ms", topDetail, sites.Count(), cost));
   float nearest = float.MAX;
   foreach (ACR_MineSite site : sites) nearest = Math.Min(nearest, vector.DistanceXZ(site.m_vPos, pretend.m_vPos));
   ACR_TestRegress.Check(apartName, nearest >= ACR_AICommander.DEFENCE_STANDOFF, string.Format("nearest spot %1 m; %2 spots, %3 too near our defences or bases left out", (int)nearest, sites.Count(), crowded));

   ACR_Options.Set("acrMinesAI", "1");
   us.m_iRP = 500;
   ai.StartTickForTest();
   ai.ResetBuildTimersForTest();
   ai.ManageMinefields();
   ACR_Options.Unset("acrMinesAI");
   vector at = LayingAt(manager);
   int index = -1;
   foreach (int s, ACR_MineSite listed : sites)
   {
    if (index < 0 && vector.DistanceXZ(listed.m_vPos, at) < 2) index = s;
   }
   ACR_TestRegress.Check(laidName, at != vector.Zero && index >= 0 && index < 3, string.Format("laid at %1: spot %2 of the list", at, index));
   StopLaying(manager);
   ACR_Roadblocks.s_aRoadblocks.RemoveItem(pretend);
   if (pretend.m_Entity) SCR_EntityHelper.DeleteEntityAndChildren(pretend.m_Entity);
  }

  // 4. No defence of ours there: the ring still gives spots, and the AI still lays a field.
  array<ref ACR_MineSite> bare = {};
  int bareCrowded;
  ACR_MineSiting.Candidates(ai, "US", basePos, enemyPos, bare, bareCrowded);
  int rings;
  foreach (ACR_MineSite plain : bare)
  {
   if (plain.m_sSource == "ring") rings++;
  }
  ACR_Options.Set("acrMinesAI", "1");
  us.m_iRP = 500;
  ai.StartTickForTest();
  ai.ResetBuildTimersForTest();
  ai.ManageMinefields();
  ACR_Options.Unset("acrMinesAI");
  vector bareAt = LayingAt(manager);
  StopLaying(manager);
  ACR_TestRegress.Check("minesiting: with no defence of ours there the old ring still gives spots and the AI lays a field", rings > 0 && bareAt != vector.Zero, string.Format("%1 spots (%2 from the ring), laid at %3", bare.Count(), rings, bareAt));

  // 5. The score's terms.
  ACR_MineSite open = new ACR_MineSite();
  open.m_sSource = "ring";
  open.m_eKind = ACR_EMineKind.AP;
  float openScore = ACR_MineSiting.Score(open);
  open.m_bDead = true;
  float deadScore = ACR_MineSiting.Score(open);
  open.m_fCover = 90;
  float coveredScore = ACR_MineSiting.Score(open);
  ACR_MineSite onRoad = new ACR_MineSite();
  onRoad.m_sSource = "ring";
  onRoad.m_eKind = ACR_EMineKind.MIXED;
  float roadScore = ACR_MineSiting.Score(onRoad);
  onRoad.m_bOnRoute = true;
  float routeScore = ACR_MineSiting.Score(onRoad);
  bool terms = deadScore > openScore && coveredScore > deadScore + 3 && roadScore > openScore && routeScore > roadScore;
  ACR_TestRegress.Check("minesiting: dead ground, cover and the enemy's route each score more", terms, string.Format("AP open %1, dead %2, dead and covered %3; mixed %4, on the route %5", openScore, deadScore, coveredScore, roadScore, routeScore));
  ACR_TestRegress.Complete("minesiting", CHECKS);
 }

 protected static vector LayingAt(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_MinePlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") return squad.m_MinePlan.m_vSite;
  }
  return vector.Zero;
 }

 protected static void StopLaying(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_MinePlan) squad.Halt("Holding");
  }
 }
}
