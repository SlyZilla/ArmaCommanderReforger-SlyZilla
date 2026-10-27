// -acrAimTest 1 (server-capable, ~20 s): drag to aim, the server half (the drag itself and its
// arrow are client UI). Outside a player's request there is no facing (the AI keeps the
// threat); a dragged one is used, nonsense is not; a roadblock faces the way dragged; a new
// Fortify position faces it, an upgrade keeps its facing; a minefield off the road lies square
// to it, one on a road runs along it the way dragged, and without a drag nothing changes.
// And the map configs still build their cursor module and radial menu (the right-drag
// gesture mods both); one right-click gives one order (the client's gate, the server's guard).
class ACR_TestAim
{
 static const int CHECKS = 9;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (tick != 8) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || !alpha.GetFaction())
  {
   ACR_TestRegress.Abort("aim", "no Alpha squad");
   return;
  }
  string us = alpha.GetFaction().GetFactionKey();
  vector home = alpha.GetAnchor().GetOrigin();
  ACR_Fortify.s_bSkipSpotChecksForTest = true;
  // Plans check the side can pay (the AI's first purchases at the start can leave it short).
  ACR_FactionData side = manager.m_Bases.GetFaction(us);
  if (side) side.m_iRP = 1000;

  // 1. No facing outside a player's request.
  ACR_TestRegress.Check("aim: no facing outside a player's request (the AI keeps the threat)", ACR_Requests.s_vAim == vector.Zero && ACR_Requests.FacingFor(us, home) == ACR_Intel.ThreatPoint(us, home), "");

  // 2. A dragged facing is used; one too close or too far is not.
  ACR_Requests.s_vAim = home + Vector(0, 0, 100);
  bool used = ACR_Requests.AimFor(home) == ACR_Requests.s_vAim && ACR_Requests.FacingFor(us, home) == ACR_Requests.s_vAim;
  ACR_Requests.s_vAim = home + Vector(2, 0, 0);
  bool tooClose = ACR_Requests.AimFor(home) == vector.Zero;
  ACR_Requests.s_vAim = home + Vector(5000, 0, 0);
  bool tooFar = ACR_Requests.AimFor(home) == vector.Zero;
  ACR_Requests.s_vAim = vector.Zero;
  ACR_TestRegress.Check("aim: a dragged facing is used, one 2 m or 5 km off is not", used && tooClose && tooFar, string.Format("used %1, too close %2, too far %3", used, tooClose, tooFar));

  // 3. A roadblock faces the way dragged along its road.
  vector road, roadDir;
  bool roadFound = FindRoad(home, road, roadDir);
  string why;
  vector pos;
  float yawAhead, yawBack;
  bool ahead = roadFound && ACR_Roadblocks.ValidateSite(road, pos, yawAhead, why, us, road + roadDir * 50);
  bool back = roadFound && ACR_Roadblocks.ValidateSite(road, pos, yawBack, why, us, road - roadDir * 50);
  float roadYaw = Math.Atan2(roadDir[0], roadDir[2]) * Math.RAD2DEG;
  ACR_TestRegress.Check("aim: a roadblock faces the way dragged", ahead && back && Turn(yawAhead, roadYaw) < 2 && Math.AbsFloat(Turn(yawAhead, yawBack) - 180) < 2, string.Format("road %1 (%2), ahead %3 at %4 (road %5), back %6 at %7", roadFound, why, ahead, yawAhead, roadYaw, back, yawBack));

  // 4/5. A new Fortify position faces it; an upgrade keeps its facing.
  vector site;
  bool siteFound = FindSite(alpha, site);
  ACR_FortifyPlan plan;
  if (siteFound) plan = ACR_Fortify.Plan(alpha, site, why, site + Vector(100, 0, 0));
  float planYaw = -1;
  if (plan) planYaw = plan.m_fYaw;
  bool east = plan && Turn(planYaw, 90) < 1;
  ACR_TestRegress.Check("aim: a new Fortify position faces the way dragged", east, string.Format("site %1, plan %2 (%3), yaw %4", siteFound, plan != null, why, planYaw));
  ACR_Fortification fort;
  if (plan) fort = ACR_Fortify.Build(plan, why);
  ACR_FortifyPlan upgrade;
  if (fort) upgrade = ACR_Fortify.Plan(alpha, fort.m_vPos, why, fort.m_vPos + Vector(0, 0, 100));
  float fortYaw = -1;
  float upgradeYaw = -1;
  if (fort) fortYaw = fort.m_fYaw;
  if (upgrade) upgradeYaw = upgrade.m_fYaw;
  bool kept = fort && upgrade && upgrade.m_Target == fort && Turn(upgradeYaw, fortYaw) < 0.1 && Turn(fortYaw, 90) < 1;
  ACR_TestRegress.Check("aim: a Fortify upgrade keeps the position's facing", kept, string.Format("position %1, upgrade %2 (%3), yaw %4 -> %5", fort != null, upgrade != null, why, fortYaw, upgradeYaw));

  // 6. Minefields.
  vector open;
  bool openFound = FindOpen(home, open);
  ACR_MinePlan offAimed, offPlain, onAimed, onPlain;
  if (openFound)
  {
   offAimed = ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, open, why, open + Vector(0, 0, 80));
   offPlain = ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, open, why);
  }
  if (roadFound)
  {
   onAimed = ACR_Minefields.Plan(alpha, ACR_EMineKind.AT, road, why, road - roadDir * 60);
   onPlain = ACR_Minefields.Plan(alpha, ACR_EMineKind.AT, road, why);
  }
  vector approach = open - home;
  approach[1] = 0;
  approach.Normalize();
  bool square = offAimed && offAimed.m_bAimed && vector.Dot(offAimed.m_vForward, Vector(0, 0, 1)) > 0.99;
  bool plain = offPlain && !offPlain.m_bAimed && vector.Dot(offPlain.m_vForward, approach) > 0.99;
  bool along = onAimed && vector.Dot(onAimed.m_vForward, roadDir * -1) > 0.99;
  bool roadPlain = onPlain && Math.AbsFloat(vector.Dot(onPlain.m_vForward, roadDir)) > 0.99;
  ACR_TestRegress.Check("aim: a minefield lies square to the drag off the road, along it on a road, as before without one", square && plain && along && roadPlain, string.Format("open %1: aimed %2 plain %3; road %4: aimed %5 plain %6 (%7)", openFound, square, plain, roadFound, along, roadPlain, why));
  ACR_Fortify.s_bSkipSpotChecksForTest = false;

  // 7. The map configs still build. A modded config class that doesn't repeat vanilla's
  // container attribute drops out of every config naming it ("Unknown class"): the map then
  // opens with no cursor module, so no pan or zoom (playtest 25 Oct).
  string mapWhy;
  bool vanillaMap = MapBuilds("{1B8AC767E06A0ACD}Configs/Map/MapFullscreen.conf", mapWhy);
  bool acaMap = MapBuilds("{AC230926A0010098}Configs/Map/ACR_MapFullscreen.conf", mapWhy);
  ACR_TestRegress.Check("aim: the map configs still build their cursor module and radial menu", vanillaMap && acaMap, mapWhy);

  // 8. One right-click, one order (25 Oct: a click sent the armed order and then a move, which
  // ended it): the click right after a release that ordered is swallowed, once; a click later,
  // or with no release order before it, is not.
  ACR_ClickGate gate = new ACR_ClickGate();
  bool freeClick = !gate.SwallowClick(5000);
  gate.ReleaseOrdered(10000);
  bool samePress = gate.SwallowClick(10014);
  bool onlyOnce = !gate.SwallowClick(10020);
  gate.ReleaseOrdered(20000);
  bool later = !gate.SwallowClick(20400);
  ACR_TestRegress.Check("aim: the click right after a release that ordered is swallowed, once; other clicks order", freeClick && samePress && onlyOnce && later, string.Format("free click %1, same press %2, only once %3, later click %4", freeClick, samePress, onlyOnce, later));

  // 9. The server's guard: a move for the squad within 150 ms of an order that takes a facing
  // for it is dropped; not for another squad, not later, not after a plain move.
  ACR_SameClickGuard guard = new ACR_SameClickGuard();
  RplId alphaId = SCR_EntityHelper.EntityToRplId(alpha.m_Group);
  RplId bravoId = SCR_EntityHelper.EntityToRplId(h.m_Range.m_Bravo);
  guard.Note(ACR_ERequest.ORDER_FORTIFY, alphaId, 1000);
  bool dropped = guard.Drop(ACR_ERequest.MOVE_TO, alphaId, 1014) && guard.Drop(ACR_ERequest.QUEUE_MOVE, alphaId, 1030);
  bool otherSquad = !guard.Drop(ACR_ERequest.MOVE_TO, bravoId, 1014);
  bool notMove = !guard.Drop(ACR_ERequest.ORDER_FORTIFY, alphaId, 1014);
  bool afterWindow = !guard.Drop(ACR_ERequest.MOVE_TO, alphaId, 1200);
  guard.Note(ACR_ERequest.MOVE_TO, alphaId, 2000);
  bool afterMove = !guard.Drop(ACR_ERequest.MOVE_TO, alphaId, 2010);
  ACR_TestRegress.Check("aim: the server drops a move right after an order that takes a facing for the same squad, nothing else", alphaId != bravoId && dropped && otherSquad && notMove && afterWindow && afterMove, string.Format("same squad %1, other squad kept %2, not a move kept %3, after 200 ms kept %4, after a move kept %5", dropped, otherSquad, notMove, afterWindow, afterMove));
  ACR_TestRegress.Complete("aim", CHECKS);
 }

 // The fullscreen map config builds as SCR_MapEntity builds it, with a cursor module and a
 // radial menu.
 protected static bool MapBuilds(ResourceName path, inout string why)
 {
  Resource container = BaseContainerTools.LoadContainer(path);
  if (!container || !container.IsValid())
  {
   why += path + " doesn't load; ";
   return false;
  }
  SCR_MapConfig config = SCR_MapConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
  if (!config)
  {
   why += path + " builds no SCR_MapConfig; ";
   return false;
  }
  bool cursor, radial;
  if (config.m_aModules)
  {
   foreach (SCR_MapModuleBase module : config.m_aModules)
   {
    if (SCR_MapCursorModule.Cast(module)) cursor = true;
   }
  }
  if (config.m_aUIComponents)
  {
   foreach (SCR_MapUIBaseComponent component : config.m_aUIComponents)
   {
    if (SCR_MapRadialUI.Cast(component)) radial = true;
   }
  }
  why += string.Format("%1: cursor module %2, radial %3; ", path, cursor, radial);
  return cursor && radial;
 }

 // Degrees between two yaws (0-180).
 protected static float Turn(float a, float b)
 {
  float d = Math.AbsFloat(a - b);
  while (d > 360) d -= 360;
  if (d > 180) d = 360 - d;
  return d;
 }

 protected static bool NearBase(vector p, float distance)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && vector.DistanceXZ(p, base.GetOrigin()) < distance + base.m_fRadius) return true;
  return false;
 }

 // A road point on its own (one road within 70 m), clear of bases.
 protected static bool FindRoad(vector origin, out vector point, out vector dir)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(origin - Vector(1500, 1000, 1500), origin + Vector(1500, 1000, 1500), roads);
  float width;
  foreach (BaseRoad road : roads)
  {
   array<vector> points = {};
   road.GetPoints(points);
   foreach (vector p : points)
   {
    if (NearBase(p, 120) || BBB_Terrain.IsWaterAt(p)) continue;
    array<BaseRoad> near = {};
    aiWorld.GetRoadNetworkManager().GetRoadsInAABB(p - Vector(70, 50, 70), p + Vector(70, 50, 70), near);
    if (near.Count() != 1) continue;
    if (ACR_Minefields.Snap(p, 2, point, dir, width)) return true;
   }
  }
  return false;
 }

 // Open ground (no road within 60 m), clear of bases.
 protected static bool FindOpen(vector origin, out vector open)
 {
  for (int r = 60; r <= 400; r += 40)
  {
   for (int a = 0; a < 8; a++)
   {
    vector p = ACR_Manager.Ground(origin + Vector(Math.Sin(a * 0.785) * r, 0, Math.Cos(a * 0.785) * r));
    if (ACR_Minefields.RoadClearance(p) < 60 || NearBase(p, 60) || BBB_Terrain.IsWaterAt(p)) continue;
    open = p;
    return true;
   }
  }
  return false;
 }

 protected static bool FindSite(ACR_Squad squad, out vector site)
 {
  vector from = squad.GetAnchor().GetOrigin();
  for (int r = 0; r < 4; r++)
  {
   for (int a = 0; a < 12; a++)
   {
    float angle = Math.PI2 * a / 12;
    vector point = ACR_Manager.Ground(from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (50 + r * 20));
    string why;
    if (!ACR_Fortify.Plan(squad, point, why)) continue;
    site = point;
    return true;
   }
  }
  return false;
 }
}
