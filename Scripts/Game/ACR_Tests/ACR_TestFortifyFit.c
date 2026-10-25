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
class ACR_TestFortifyFit
{
 static const int CHECKS = 10;
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
  return !road || distance - road.GetWidth() * 0.5 >= radius + 1.0;
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

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
