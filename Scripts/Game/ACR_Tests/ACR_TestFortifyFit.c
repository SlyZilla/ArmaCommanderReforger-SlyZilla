// -acrFortifyFitTest 1 (server-capable, ~1.5 min): upgrades that fit on real ground (playtest 25
// Oct: tier II refused on placement six times, "spot 4 blocked (on a road)", after the RP was paid;
// an AI nest refused with every place "blocked 5-6 m out by" rising ground).
// - A tier I with a road just behind it (the covered post's spot and every near place on the
//   road) upgrades to tier II: the order passes, the covered post goes in off the road.
// - A nest whose near places are all blocked takes one in its wider ring.
// - An upgrade that can't go in is refused when ordered (nothing charged, the reason given),
//   for a blocked place and for a gun with no line of fire; the AI then leaves the position
//   alone for a while, a player can still order it.
// - The AI's site check (tiers II and III fit) finds a site near Alpha, and says no when blocked.
// - A gun aims up a gentle rise a level trace runs into.
// - A refusal after the work (something changed) is said in the side's feed, refunded.
// - Refused twice for one reason, the AI gives a position's upgrades up (players still may).
// - The playtest's two refused sites (Arland, facings from its log): the Airfield tier I's tier II
//   goes in now; at Heide (a nest facing an 11 degree rise) the order and the build agree.
class ACR_TestFortifyFit
{
 static const int CHECKS = 13;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRP;
 static vector s_vLate;
 static ACR_Fortification s_Late;
 static string s_sWhy;
 static vector s_vRoadFacing;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("fortifyfit", "no Alpha squad");
   return;
  }
  if (tick > 190) ACR_TestRegress.Abort("fortifyfit", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   ACR_Fortify.WORK_SCALE = 0.15;
   us.m_iRP = 500;
   PlaytestSites(alpha);
   RoadChecks(alpha);
   NestChecks(alpha, us);
   SiteAndAimChecks(alpha);
   // A refusal after the work: the order passes, then the guns' line of fire is blocked before
   // the pieces go in.
   ACR_FortifyPlan first = ACR_Fortify.Plan(alpha, s_vLate, s_sWhy);
   if (first) s_Late = ACR_Fortify.Build(first, s_sWhy);
   s_iRP = us.m_iRP;
   bool ordered = s_Late && ACR_Fortify.Order(alpha, s_vLate, s_sWhy);
   ACR_Fortify.s_bBlockFireForTest = true;
   PrintFormat("[ACR] FORTIFYFIT late refusal: tier I %1, upgrade ordered %2 (%3), rp %4 -> %5", s_Late != null, ordered, s_sWhy, s_iRP, us.m_iRP);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (alpha.m_FortifyPlan && waited < 150) return;
   ACR_Fortify.s_bBlockFireForTest = false;
   ACR_Intel intel = manager.IntelFor("US");
   string said;
   if (intel)
   {
    // Newest first; nothing else in this test finishes a Fortify order.
    foreach (string line : intel.m_Feed)
     if (said.IsEmpty() && line.Contains("could not fortify")) said = line;
   }
   bool kept = s_Late && s_Late.m_iTier == 1;
   // Income may tick during the walk: the refund shows in the feed line and the RP being back.
   ACR_TestRegress.Check("fortifyfit: a refusal after the work is said in the side's feed, refunded in full, the AI holding off", !alpha.m_FortifyPlan && us.m_iRP >= s_iRP && kept && said.Contains("(15 RP refunded)") && ACR_Fortify.UpgradesHeld(s_Late), string.Format("after %1 s: rp %2 (before %3), tier kept %4, feed '%5'", waited, us.m_iRP, s_iRP, kept, said));
   ACR_TestRegress.Complete("fortifyfit", CHECKS);
   s_iStep = 2;
  }
 }


 // A straight road; the position 10 m off it facing away, so the covered post's spot (10 m
 // behind) and every near place round it are on the road.
 protected static void RoadChecks(ACR_Squad alpha)
 {
  vector site;
  string detail;
  bool found = FindRoadSite(alpha, site, detail);
  ACR_Fortification fort;
  string why;
  if (found)
  {
   ACR_FortifyPlan first = ACR_Fortify.Plan(alpha, site, why, s_vRoadFacing);
   if (first) fort = ACR_Fortify.Build(first, why);
  }
  ACR_TestRegress.Check("fortifyfit: fixture: a tier I with the covered post's spot and all its near places on a road", found && fort && fort.m_iTier == 1, string.Format("%1, tier I %2 (%3)", detail, fort != null, why));
  if (!fort)
  {
   ACR_TestRegress.Check("fortifyfit: the tier II order passes (a wider ring for the covered post)", false, "no fixture");
   ACR_TestRegress.Check("fortifyfit: tier II goes in whole, its covered post off the road behind the line", false, "no fixture");
   return;
  }
  ACR_FortifyPlan upgrade = ACR_Fortify.Plan(alpha, site, why);
  ACR_TestRegress.Check("fortifyfit: the tier II order passes (a wider ring for the covered post)", upgrade && upgrade.m_iTier == 2 && upgrade.m_Target == fort, why);
  ACR_Fortification built;
  string buildWhy;
  if (upgrade) built = ACR_Fortify.Build(upgrade, buildWhy);
  IEntity post = fort.m_aPieces[4];
  IEntity nest = fort.m_aPieces[2];
  if (post) PrintFormat("[ACR] FORTIFYFIT covered post (tier II) reaches %1 m from its centre", Reach(post, post.GetOrigin()));
  bool offRoad = post && ClearOfRoads(post.GetOrigin(), ACR_Fortify.SpotRadius(4));
  float behind;
  if (post) behind = LocalZ(fort, post.GetOrigin());
  string nestPrefab;
  if (nest && nest.GetPrefabData()) nestPrefab = nest.GetPrefabData().GetPrefabName();
  ACR_TestRegress.Check("fortifyfit: tier II goes in whole, its covered post off the road behind the line", built && fort.m_iTier == 2 && offRoad && behind < -5 && nestPrefab == ACR_FobDefences.NEST_US, string.Format("'%1', tier %2, covered post off the road %3 at z %4, nest %5", buildWhy, fort.m_iTier, offRoad, behind, FilePath.StripPath(nestPrefab)));
 }


 protected static bool FindRoadSite(ACR_Squad alpha, out vector site, out string detail)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager())
  {
   detail = "no road network";
   return false;
  }
  vector origin = alpha.GetAnchor().GetOrigin();
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(origin - Vector(1500, 1000, 1500), origin + Vector(1500, 1000, 1500), roads);
  int tried, onRoad;
  array<float> signs = {1, -1};
  foreach (BaseRoad road : roads)
  {
   array<vector> nodes = {};
   road.GetPoints(nodes);
   for (int i = 1; i < nodes.Count(); i++)
   {
    vector along = nodes[i] - nodes[i - 1];
    along[1] = 0;
    if (along.Length() < 12) continue;
    along.Normalize();
    vector centre = (nodes[i] + nodes[i - 1]) * 0.5;
    foreach (float sign : signs)
    {
     if (tried >= 300) break;
     tried++;
     vector forward = Vector(along[2], 0, -along[0]) * sign;
     vector at = ACR_Manager.Ground(centre + forward * 10);
     float facing = Math.Atan2(forward[0], forward[2]) * Math.RAD2DEG;
     array<vector> near = {};
     string why;
     if (ACR_Fortify.FindSpots(at, facing, 4, null, near, why, 1, -1, false) || why != "on a road") continue;
     onRoad++;
     array<vector> wide = {};
     if (!ACR_Fortify.FindSpots(at, facing, 4, null, wide, why, 1, -1, true)) continue;
     string planWhy;
     vector towards = at + forward * 100;
     ACR_FortifyPlan plan = ACR_Fortify.Plan(alpha, at, planWhy, towards);
     if (!plan || plan.m_iTier != 1) continue;
     site = at;
     s_vRoadFacing = towards;
     detail = string.Format("road %1 m wide at %2, %3 sites tried, %4 with the covered post on the road", road.GetWidth(), ACR_Manager.Ground(centre), tried, onRoad);
     return true;
    }
   }
  }
  detail = string.Format("%1 roads, %2 sites tried, %3 with the covered post on the road, none usable", roads.Count(), tried, onRoad);
  return false;
 }

 protected static bool ClearOfRoads(vector pos, float radius)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  BaseRoad road;
  float distance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, distance);
  // Fortify's rule: the footprint keeps off the road surface (it may reach the edge).
  return !road || distance - road.GetWidth() * 0.5 >= radius;
 }

 protected static float LocalZ(ACR_Fortification fort, vector pos)
 {
  float rad = fort.m_fYaw * Math.DEG2RAD;
  vector offset = pos - fort.m_vPos;
  return offset[0] * Math.Sin(rad) + offset[2] * Math.Cos(rad);
 }

 // A second position: its nest's near places blocked, the order-time refusals, the AI's hold.
 protected static void NestChecks(ACR_Squad alpha, ACR_FactionData us)
 {
  vector site;
  ACR_Fortification fort;
  string why;
  if (FindSite(alpha, site, false))
  {
   ACR_FortifyPlan first = ACR_Fortify.Plan(alpha, site, why);
   if (first) fort = ACR_Fortify.Build(first, why);
  }
  ACR_Fortification built;
  ACR_Fortify.s_iBlockNearForTest = 2;
  ACR_FortifyPlan upgrade;
  if (fort) upgrade = ACR_Fortify.Plan(alpha, site, why);
  if (upgrade) built = ACR_Fortify.Build(upgrade, why);
  ACR_Fortify.s_iBlockNearForTest = -1;
  IEntity nest;
  float x, z;
  if (fort) nest = fort.m_aPieces[2];
  if (nest)
  {
   x = ACR_Fortify.LocalX(fort.m_vPos, fort.m_fYaw, nest.GetOrigin());
   z = LocalZ(fort, nest.GetOrigin());
  }
  bool wideRing = nest && (Math.AbsFloat(x) > 5 || z < 1 || z > 6);
  int tier;
  if (fort) tier = fort.m_iTier;
  ACR_TestRegress.Check("fortifyfit: a nest whose near places are all blocked takes a place in its wider ring", built && tier == 2 && wideRing, string.Format("'%1', tier %2, nest at x %3 z %4", why, tier, x, z));
  if (!fort || fort.m_iTier != 2)
  {
   ACR_TestRegress.Check("fortifyfit: an upgrade with no place is refused when ordered, nothing charged, the reason given", false, "no tier II");
   ACR_TestRegress.Check("fortifyfit: the AI leaves a refused position alone for a while, a player can still order it", false, "no tier II");
   ACR_TestRegress.Check("fortifyfit: a gun with no line of fire is refused when ordered, nothing charged", false, "no tier II");
   return;
  }
  int rp = us.m_iRP;
  ACR_Fortify.s_bBlockSpotsForTest = true;
  string placeWhy;
  bool placeOrdered = ACR_Fortify.Order(alpha, site, placeWhy);
  ACR_Fortify.s_bBlockSpotsForTest = false;
  ACR_TestRegress.Check("fortifyfit: an upgrade with no place is refused when ordered, nothing charged, the reason given", !placeOrdered && us.m_iRP == rp && !alpha.m_FortifyPlan && placeWhy.StartsWith("upgrade not possible here: the "), string.Format("'%1', rp %2 -> %3", placeWhy, rp, us.m_iRP));
  bool held = ACR_Fortify.UpgradesHeld(fort);
  ACR_FortifyPlan still = ACR_Fortify.Plan(alpha, site, why);
  ACR_TestRegress.Check("fortifyfit: the AI leaves a refused position alone for a while, a player can still order it", held && still && still.m_iTier == 3, string.Format("held %1, planned again %2 (%3)", held, still != null, why));
  ACR_Fortify.s_bBlockFireForTest = true;
  string fireWhy;
  bool fireOrdered = ACR_Fortify.Order(alpha, site, fireWhy);
  ACR_Fortify.s_bBlockFireForTest = false;
  ACR_TestRegress.Check("fortifyfit: a gun with no line of fire is refused when ordered, nothing charged", !fireOrdered && us.m_iRP == rp && !alpha.m_FortifyPlan && fireWhy.Contains("no clear line of fire"), string.Format("'%1', rp %2 -> %3", fireWhy, rp, us.m_iRP));
  // The same refusal again: the AI gives the position up for good (playtest: an AI upgrade by big
  // rocks refused every 15 min all game); a player can still order it.
  bool heldBefore = fort.m_fUpgradeHeldUntil < float.MAX;
  ACR_Fortify.s_bBlockFireForTest = true;
  string againWhy;
  ACR_Fortify.Order(alpha, site, againWhy);
  ACR_Fortify.s_bBlockFireForTest = false;
  bool givenUp = fort.m_fUpgradeHeldUntil == float.MAX;
  ACR_FortifyPlan playerStill = ACR_Fortify.Plan(alpha, site, why);
  ACR_TestRegress.Check("fortifyfit: refused twice for the same reason the AI gives the upgrade up for good, a player can still order it", heldBefore && givenUp && playerStill != null, string.Format("held for a while before %1, given up %2 after %3 refusals ('%4'), planned for a player %5", heldBefore, givenUp, fort.m_iUpgradeRefusals, fort.m_sUpgradeRefused, playerStill != null));
 }

 // The AI's site check, a gun aiming up a rise, and the site for the late refusal.
 protected static void SiteAndAimChecks(ACR_Squad alpha)
 {
  bool found = FindSite(alpha, s_vLate, true);
  float yaw;
  bool blockedFits = true;
  if (found)
  {
   string why;
   ACR_FortifyPlan plan = ACR_Fortify.Plan(alpha, s_vLate, why);
   if (plan) yaw = plan.m_fYaw;
   ACR_Fortify.s_bBlockSpotsForTest = true;
   blockedFits = ACR_Fortify.UpgradesFit(s_vLate, yaw);
   ACR_Fortify.s_bBlockSpotsForTest = false;
  }
  ACR_TestRegress.Check("fortifyfit: the AI's site check finds a site near Alpha where tiers II and III fit, and says no when blocked", found && !blockedFits, string.Format("site %1 at %2, blocked fits %3", found, s_vLate, blockedFits));
  string detail;
  ACR_TestRegress.Check("fortifyfit: a gun aims up a gentle rise that a level line of fire runs into", AimFixture(alpha, detail), detail);
 }

 // A place near Alpha that takes a new position (and, with 'fits', its upgrades too).
 protected static bool FindSite(ACR_Squad alpha, out vector site, bool fits)
 {
  vector from = alpha.GetAnchor().GetOrigin();
  for (int r = 0; r < 6; r++)
  {
   for (int a = 0; a < 12; a++)
   {
    float angle = Math.PI2 * a / 12;
    vector point = ACR_Manager.Ground(from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (50 + r * 25));
    string why;
    ACR_FortifyPlan plan = ACR_Fortify.Plan(alpha, point, why);
    if (!plan || plan.m_iTier != 1) continue;
    if (fits && !ACR_Fortify.UpgradesFit(point, plan.m_fYaw)) continue;
    site = point;
    return true;
   }
  }
  return false;
 }

 // Ground rising 4-13 degrees over FIRE_CLEAR, above the muzzle within it: a level trace hits the
 // terrain, the gun aiming up is clear.
 protected static bool AimFixture(ACR_Squad alpha, out string detail)
 {
  BaseWorld world = GetGame().GetWorld();
  vector from = alpha.GetAnchor().GetOrigin();
  int candidates;
  for (int r = 1; r <= 24; r++)
  {
   for (int a = 0; a < 16; a++)
   {
    float angle = Math.PI2 * a / 16;
    vector place = from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (r * 25);
    place[1] = world.GetSurfaceY(place[0], place[2]);
    for (int d = 0; d < 8; d++)
    {
     float facing = d * 45;
     vector dir = Vector(Math.Sin(facing * Math.DEG2RAD), 0, Math.Cos(facing * Math.DEG2RAD));
     float muzzle = place[1] + ACR_Fortify.MUZZLE_UP;
     vector far = place + dir * ACR_Fortify.FIRE_CLEAR;
     float rise = world.GetSurfaceY(far[0], far[2]) + 1.0 - muzzle;
     float up = Math.Atan2(rise, ACR_Fortify.FIRE_CLEAR) * Math.RAD2DEG;
     if (up < 4 || up > 13) continue;
     bool above;
     for (float s = 3; s < ACR_Fortify.FIRE_CLEAR; s += 2)
     {
      vector p = place + dir * s;
      if (world.GetSurfaceY(p[0], p[2]) > muzzle + 0.1) above = true;
     }
     if (!above) continue;
     candidates++;
     string level;
     ACR_Fortify.s_bNoAimUpForTest = true;
     bool levelClear = ACR_Fortify.LineOfFireAt(place, facing, null, level);
     ACR_Fortify.s_bNoAimUpForTest = false;
     if (levelClear || !level.Contains("Terrain")) continue;
     string aimed;
     if (!ACR_Fortify.LineOfFireAt(place, facing, null, aimed)) continue;
     detail = string.Format("at %1 facing %2: %3 deg up; level: %4; aimed up: clear (%5 candidates)", place, facing, up, level, candidates);
     return true;
    }
   }
  }
  detail = string.Format("no fixture (%1 candidates with rising ground)", candidates);
  return false;
 }

 // The two sites refused in the 25 Oct playtest (logs_2026-10-25_19-46-30), checked first while
 // nothing else stands near them.
 protected static void PlaytestSites(ACR_Squad alpha)
 {
  string detail;
  ACR_Fortification airfield = TierOneAt(alpha, "1288.13 36.5863 2993.74", 71, detail);
  string why;
  ACR_FortifyPlan upgrade;
  if (airfield) upgrade = ACR_Fortify.Plan(alpha, airfield.m_vPos, why);
  ACR_Fortification built;
  string buildWhy;
  if (upgrade) built = ACR_Fortify.Build(upgrade, buildWhy);
  string roads = RoadClearance("1288.13 36.5863 2993.74", 71);
  ACR_TestRegress.Check("fortifyfit: playtest site, Airfield: tier II goes in now (its covered post was on the road)", built && built.m_iTier == 2, string.Format("%1; order '%2', build '%3'; %4", detail, why, buildWhy, roads));
  ACR_Fortification heide = TierOneAt(alpha, "3034.92 19.8646 2682.11", -126, detail);
  string heideWhy;
  ACR_FortifyPlan heideUpgrade;
  if (heide) heideUpgrade = ACR_Fortify.Plan(alpha, heide.m_vPos, heideWhy);
  string heideBuildWhy;
  ACR_Fortification heideBuilt;
  if (heideUpgrade) heideBuilt = ACR_Fortify.Build(heideUpgrade, heideBuildWhy);
  // Passed when ordered: the build goes in. Refused when ordered: nothing paid, the AI holds off.
  bool agree = heide && ((heideUpgrade && heideBuilt) || (!heideUpgrade && heideWhy.StartsWith("upgrade not possible here") && ACR_Fortify.UpgradesHeld(heide)));
  string outcome = "refused when ordered";
  if (heideUpgrade) outcome = "passed when ordered";
  ACR_TestRegress.Check("fortifyfit: playtest site, Heide: the order's check and the build agree (no paid upgrade refused later)", agree, string.Format("%1; %2 '%3', build '%4'", detail, outcome, heideWhy, heideBuildWhy));
 }

 // Diagnostics: the best road clearance (distance to the road's edge) among the covered post's
 // places, near and wide, at a site.
 protected static string RoadClearance(vector at, float yaw)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return "no road network";
  vector site = ACR_Manager.Ground(at);
  array<vector> offsets = {};
  array<float> across = {0, 2, -2, 4, -4};
  array<float> along = {0, -2, 2};
  foreach (float dz : along)
  {
   foreach (float dx : across) offsets.Insert(Vector(dx, 0, dz));
  }
  ACR_Fortify.WideOffsets(4, offsets);
  float best = -100;
  vector bestOffset;
  float atSpot;
  foreach (int k, vector offset : offsets)
  {
   vector pos = ACR_Fortify.ToWorld(site, yaw, ACR_Fortify.SpotOffset(4) + offset);
   BaseRoad road;
   float distance;
   aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, distance);
   float clear = 100;
   if (road) clear = distance - road.GetWidth() * 0.5;
   if (k == 0) atSpot = clear;
   if (clear <= best) continue;
   best = clear;
   bestOffset = offset;
  }
  return string.Format("covered post road clearance: %1 m at its spot, best %2 m at offset %3 (needs %4)", atSpot, best, bestOffset, ACR_Fortify.SpotRadius(4));
 }

 // How far a composition's parts reach from 'centre' on the ground (its root has no bounds).
 protected static float Reach(IEntity e, vector centre)
 {
  float most;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  if (maxs[0] > mins[0])
  {
   array<vector> corners = {Vector(mins[0], 0, mins[2]), Vector(mins[0], 0, maxs[2]), Vector(maxs[0], 0, mins[2]), Vector(maxs[0], 0, maxs[2])};
   foreach (vector c : corners) most = Math.Max(most, vector.DistanceXZ(c, centre));
  }
  IEntity child = e.GetChildren();
  while (child)
  {
   most = Math.Max(most, Reach(child, centre));
   child = child.GetSibling();
  }
  return most;
 }

 protected static ACR_Fortification TierOneAt(ACR_Squad alpha, vector at, float yaw, out string detail)
 {
  vector site = ACR_Manager.Ground(at);
  float rad = yaw * Math.DEG2RAD;
  string why;
  ACR_FortifyPlan plan = ACR_Fortify.Plan(alpha, site, why, site + Vector(Math.Sin(rad), 0, Math.Cos(rad)) * 100);
  ACR_Fortification fort;
  if (plan && plan.m_iTier == 1) fort = ACR_Fortify.Build(plan, why);
  detail = string.Format("tier I at %1 facing %2: %3 (%4)", site, yaw, fort != null, why);
  return fort;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
