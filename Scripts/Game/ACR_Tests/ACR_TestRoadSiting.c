// -acrRoadSitingTest 1 (server-capable, ~30 s): where the AI commander puts a roadblock
// (ACR_RoadblockSiting; playtest 25 Oct: one of the AI's stood 18 m from a T-crossing, one 25 m
// from a dead end). For a US front base:
// - the best site keeps clear of junctions and dead ends, sees down the road and faces the
//   enemy (the old first-fit site is logged next to it);
// - scoring a base costs a bounded time, and the cached second look almost none;
// - with a roadblock of ours on the best site's road, a base with a second road gets its block
//   on the other road;
// - the playtest's four AI roadblocks, looked at by the scorer: at least two by a road's end or a
//   junction (the user found two);
// - never fewer roadblocks: at every base where the old first-fit found a site the new siting
//   does too; with the fan finding nothing it falls back to the first-fit, and below its floor
//   the best is still taken;
// - on the enemy's road route (BBB_Roads) a site scores more, the route's points are candidates (the
//   real routes are logged: on the test range every one is a long way round, which drivers skip);
// - ManageRoadblocks sends a free squad to the scored site.
class ACR_TestRoadSiting
{
 static const int CHECKS = 8;
 static const float MIN_SIGHT = 80;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!bravo || !bravo.GetAnchor() || !us)
  {
   ACR_TestRegress.Abort("roadsiting", "no Bravo or US faction data");
   return;
  }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  ai.FrontBasesForTest(fronts, enemies);
  if (fronts.IsEmpty())
  {
   ACR_TestRegress.Abort("roadsiting", "no US front base");
   return;
  }
  ACR_RoadblockSiting.Reset();

  // 1. The best site round the first front base, and the old first-fit one beside it.
  vector basePos = fronts[0].GetOrigin();
  vector enemyPos = enemies[0];
  int t0 = System.GetTickCount();
  int considered;
  string none;
  ACR_RoadSite best = ACR_RoadblockSiting.Best(basePos, enemyPos, "US", false, considered, none);
  int cost = System.GetTickCount() - t0;
  PrintFormat("[ACR] ROADSITING old first-fit site for %1: %2", fronts[0].m_sName, ACR_RoadblockSiting.Breakdown(ACR_RoadblockSiting.FirstFit(basePos, enemyPos, "US")));
  bool good = best && best.m_fBreak >= ACR_RoadblockSiting.JUNCTION_CLEAR && best.m_fSight >= MIN_SIGHT && best.m_fFacing > 0.3;
  ACR_TestRegress.Check("roadsiting: the AI's site keeps 30 m from junctions and road ends, sees 80 m down the road and faces the enemy", good, string.Format("%1 at %2 of %3 sites: %4 %5", fronts[0].m_sName, PosOf(best), considered, ACR_RoadblockSiting.Breakdown(best), none));

  // 1b. The enemy's road route (BBB_Roads): where the bases have a sensible one, the site is on
  // it; where the road is a long way round, BBB's drivers go direct and the fan covers that.
  string routeDetail;
  bool routeUsed = RouteUsed(routeDetail);
  PrintFormat("[ACR] ROADSITING road routes: %1 (%2)", routeUsed, routeDetail);
  // The route's part of the score, with a route made up through the best site (on the test range
  // every road route between bases is a long way round, which BBB's drivers don't take).
  bool routeScored;
  string routeScoreDetail = "no site";
  if (best)
  {
   array<vector> madeUp = {enemyPos, best.m_vPos, basePos};
   ACR_RoadSite again = ACR_RoadblockSiting.Describe(best.m_vPos, best.m_fYaw, basePos, enemyPos);
   float plain = again.m_fScore;
   ACR_RoadblockSiting.MarkRoute(again, madeUp);
   array<vector> routeSamples = {};
   ACR_RoadblockSiting.RouteSamples(madeUp, basePos, routeSamples);
   bool inBand = !routeSamples.IsEmpty();
   foreach (vector rs : routeSamples)
   {
    float fromBase = vector.DistanceXZ(rs, basePos);
    if (fromBase < ACR_RoadblockSiting.NEAR_OUT || fromBase > ACR_RoadblockSiting.FAR_OUT) inBand = false;
   }
   routeScored = again.m_bOnRoute && Math.AbsFloat(again.m_fScore - plain - ACR_RoadblockSiting.ON_ROUTE) < 0.01 && inBand;
   routeScoreDetail = string.Format("on it %1, score %2 -> %3, %4 route candidates 200-450 m out (all in the band %5)", again.m_bOnRoute, plain, again.m_fScore, routeSamples.Count(), inBand);
  }
  ACR_TestRegress.Check("roadsiting: a site on the enemy's road route scores more, and the route's points 200-450 m out are candidates", routeScored, routeScoreDetail);

  // 2. The cost: a base's first scoring, then the cached second look.
  int t1 = System.GetTickCount();
  int again;
  ACR_RoadblockSiting.Best(basePos, enemyPos, "US", false, again, none);
  int cached = System.GetTickCount() - t1;
  ACR_TestRegress.Check("roadsiting: scoring a base's sites stays under 100 ms, the cached look under 10 ms", cost < 100 && cached < 10, string.Format("first %1 ms (%2 road sites scored), cached %3 ms", cost, ACR_RoadblockSiting.s_iEvaluated, cached));

  // 3. A second road: a roadblock of ours on the best site's road, then the next pick.
  string secondDetail;
  ACR_TestRegress.Check("roadsiting: with a roadblock of ours on a base's best road, its second road gets the next one", SecondRoad(secondDetail), secondDetail);

  // 4. The playtest's AI roadblocks: USSR at Heide, Ridge and Heide Lighthouse, US at Harbor.
  array<vector> playtest = {Vector(2857.44, 61.3556, 2809.71), Vector(2035.28, 68.1975, 1223.77), Vector(3320.26, 37.9591, 2571.71), Vector(1462.32, 36.3488, 3137.88)};
  int flagged;
  string seen;
  foreach (vector at : playtest)
  {
   ACR_RoadSite site = Look(at);
   if (site && site.m_fBreak < ACR_RoadblockSiting.JUNCTION_CLEAR) flagged++;
   seen += string.Format(" | %1: %2", at, ACR_RoadblockSiting.Breakdown(site));
  }
  ACR_TestRegress.Check("roadsiting: the scorer finds a road's end or a junction by at least two of the playtest's four AI roadblocks", flagged >= 2, string.Format("%1 of 4 near one%2", flagged, seen));

  // 5. Never fewer roadblocks: every base and its nearest enemy base, old pick against new.
  string coverage;
  ACR_TestRegress.Check("roadsiting: at every base where the old first-fit found a site the new siting finds one too", Coverage(coverage), coverage);

  // 6. The ladder's lower rungs: no fan candidates -> the first-fit; a floor nothing reaches ->
  // the best anyway.
  ACR_RoadblockSiting.s_bNoFanForTest = true;
  ACR_RoadSite fallback = ACR_RoadblockSiting.Best(basePos, enemyPos, "US", false, considered, none);
  ACR_RoadblockSiting.s_bNoFanForTest = false;
  ACR_RoadSite oldPick = ACR_RoadblockSiting.FirstFit(basePos, enemyPos, "US");
  ACR_RoadblockSiting.s_fFloor = 100;
  ACR_RoadSite low = ACR_RoadblockSiting.Best(basePos, enemyPos, "US", false, considered, none);
  ACR_RoadblockSiting.s_fFloor = ACR_RoadblockSiting.FLOOR;
  bool fellBack = (!oldPick && !fallback) || (fallback && oldPick && fallback.m_sHow == "first-fit fallback" && vector.DistanceXZ(fallback.m_vPos, oldPick.m_vPos) < 1);
  bool belowTaken = low && low.m_sHow == "below the floor";
  ACR_TestRegress.Check("roadsiting: with no fan candidate it falls back to the old first-fit; below the floor the best is still taken", fellBack && belowTaken, string.Format("fallback %1 (%2), old pick %3, below the floor %4", PosOf(fallback), HowOf(fallback), PosOf(oldPick), HowOf(low)));

  // 7. ManageRoadblocks: Bravo free next to the first front base, the side able to pay.
  us.m_iRP = 500;
  bravo.m_iAITask = ACR_EAITask.FREE;
  array<AIAgent> agents = {};
  bravo.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body) body.SetOrigin(ACR_Manager.Ground(basePos + Vector(40 + i * 2, 0, 40)));
  }
  ai.StartTickForTest();
  ai.ManageRoadblocks();
  ACR_Squad sent;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad && squad.m_iAITask == ACR_EAITask.ROADBLOCK && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") sent = squad;
  }
  vector sentTo;
  string sentName = "none";
  if (sent)
  {
   sentTo = sent.m_vRoadblockSite;
   sentName = sent.GetName();
  }
  ACR_TestRegress.Check("roadsiting: ManageRoadblocks sends a free squad to the scored site", sent && best && vector.DistanceXZ(sentTo, best.m_vPos) < 1, string.Format("%1 sent to %2, scored site %3", sentName, sentTo, PosOf(best)));
  ACR_TestRegress.Complete("roadsiting", CHECKS);
 }

 protected static string PosOf(ACR_RoadSite site)
 {
  if (!site) return "none";
  return site.m_vPos.ToString();
 }

 protected static string HowOf(ACR_RoadSite site)
 {
  if (!site) return "-";
  return site.m_sHow;
 }

 protected static void LiveBases(notnull array<ACR_Base> bases)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired) bases.Insert(base);
  }
 }

 // The bases and their nearest other base: the first pair with a sensible road route must have
 // its site on that route; the pairs without one are counted.
 protected static bool RouteUsed(out string detail)
 {
  array<ACR_Base> bases = {};
  LiveBases(bases);
  int longWay;
  foreach (ACR_Base base : bases)
  {
   ACR_Base enemy = NearestOther(base, bases);
   if (!enemy) continue;
   int considered;
   string none;
   ACR_RoadSite site = ACR_RoadblockSiting.Best(base.GetOrigin(), enemy.GetOrigin(), "US", false, considered, none);
   int length, ms;
   if (!ACR_RoadblockSiting.RouteFor(base.GetOrigin(), enemy.GetOrigin(), "US", length, ms))
   {
    longWay++;
    continue;
   }
   bool onRoute = site && site.m_bOnRoute;
   detail = string.Format("%1 from %2: road route %3 m, %4 ms; site on it %5 (%6); %7 pairs before it a long way round", base.m_sName, enemy.m_sName, length, ms, onRoute, ACR_RoadblockSiting.Breakdown(site), longWay);
   return onRoute && ms < 150;
  }
  detail = string.Format("no pair of %1 bases has a sensible road route (%2 a long way round: BBB's drivers go direct, the fan covers it)", bases.Count(), longWay);
  return false;
 }

 // Every base and its nearest other base 600 m+ away: the old first-fit against the new siting.
 protected static bool Coverage(out string detail)
 {
  array<ACR_Base> bases = {};
  LiveBases(bases);
  int oldFound, newFound, lost;
  string lostAt;
  foreach (ACR_Base base : bases)
  {
   ACR_Base enemy = NearestOther(base, bases);
   if (!enemy) continue;
   ACR_RoadSite old = ACR_RoadblockSiting.FirstFit(base.GetOrigin(), enemy.GetOrigin(), "US");
   int considered;
   string none;
   ACR_RoadSite fresh = ACR_RoadblockSiting.Best(base.GetOrigin(), enemy.GetOrigin(), "US", false, considered, none);
   if (old) oldFound++;
   if (fresh) newFound++;
   if (old && !fresh)
   {
    lost++;
    lostAt += string.Format(" %1 (%2)", base.m_sName, none);
   }
  }
  detail = string.Format("%1 bases: old first-fit found %2, new siting %3, lost %4%5", bases.Count(), oldFound, newFound, lost, lostAt);
  return lost == 0 && newFound >= oldFound;
 }

 // A base with a second road: its best site gets a (pretend) roadblock of ours, then the next
 // pick must be on another road, far enough off.
 protected static bool SecondRoad(out string detail)
 {
  array<ACR_Base> bases = {};
  LiveBases(bases);
  int tried;
  foreach (ACR_Base base : bases)
  {
   ACR_Base enemy = NearestOther(base, bases);
   if (!enemy) continue;
   tried++;
   int considered;
   string none;
   ACR_RoadSite first = ACR_RoadblockSiting.Best(base.GetOrigin(), enemy.GetOrigin(), "US", false, considered, none);
   if (!first) continue;
   // A real entity: ValidateSite prunes roadblocks without one.
   ACR_Roadblock pretend = new ACR_Roadblock();
   pretend.m_sFaction = "US";
   pretend.m_vPos = first.m_vPos;
   pretend.m_Entity = ACR_Manager.SpawnPrefab(ACR_Roadblocks.LOGS_1, first.m_vPos);
   ACR_Roadblocks.s_aRoadblocks.Insert(pretend);
   ACR_RoadSite second = ACR_RoadblockSiting.Best(base.GetOrigin(), enemy.GetOrigin(), "US", true, considered, none);
   ACR_Roadblocks.s_aRoadblocks.RemoveItem(pretend);
   if (pretend.m_Entity) SCR_EntityHelper.DeleteEntityAndChildren(pretend.m_Entity);
   if (!second) continue;
   bool apart = vector.DistanceXZ(second.m_vPos, first.m_vPos) >= ACR_Roadblocks.MIN_ROADBLOCK_DIST;
   detail = string.Format("%1 (%2 bases tried): first %3, second %4 %5 m away, %6", base.m_sName, tried, first.m_vPos, second.m_vPos, (int)vector.DistanceXZ(second.m_vPos, first.m_vPos), ACR_RoadblockSiting.Breakdown(second));
   return apart && !second.m_bBlocked;
  }
  detail = string.Format("no base with a second road (%1 tried)", tried);
  return false;
 }

 protected static ACR_Base NearestOther(ACR_Base base, notnull array<ACR_Base> bases)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base other : bases)
  {
   if (other == base) continue;
   float d = vector.DistanceXZ(other.GetOrigin(), base.GetOrigin());
   if (d < 600 || d >= bestDistance) continue;
   bestDistance = d;
   best = other;
  }
  return best;
 }

 // The scorer's view of a roadblock already standing at 'at' (facing along its road towards the
 // nearest base, as ValidateSite would face it).
 protected static ACR_RoadSite Look(vector at)
 {
  at = ACR_Manager.Ground(at);
  ACR_Base nearest;
  float nearestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), at);
   if (d >= nearestDistance) continue;
   nearestDistance = d;
   nearest = base;
  }
  vector towards = at + Vector(0, 0, 100);
  if (nearest) towards = nearest.GetOrigin();
  vector pos;
  float yaw;
  string refusal;
  // ValidateSite sets the snap and facing before its spacing checks, so they hold even if it refuses.
  ACR_Roadblocks.ValidateSite(at, pos, yaw, refusal, "USSR", towards);
  if (pos == vector.Zero) return null;
  return ACR_RoadblockSiting.Describe(pos, yaw, towards, towards);
 }
}
