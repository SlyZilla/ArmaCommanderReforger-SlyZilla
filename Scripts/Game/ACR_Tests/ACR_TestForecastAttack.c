// -acrForecastAttackTest 1 (server-capable, ~3 min): the defence forecast (ACR_DefenceForecast) against
// a real attack with a known way in and timing (Jacob, 4 Nov: better automated tests than reading soaks).
// Two enemy fire teams start outside a base of ours, past its threat ring, and walk straight at it on a
// move waypoint; two more walk across, well out, at the same time. Both are kept seen (a spotter's report,
// as the QRF test does: every man noted in our intel where he stands), so the side's real intel builds
// the contacts and their headings from real movement. The forecast runs on that intel each second.
// Checks: the base is forecast at least LEAD_LEAST s before the attackers reach the threat ring; from
// the way they come (within WAY_SLACK degrees); the self-check counts it a hit; the passers-by never
// make a way of their own.
class ACR_TestForecastAttack
{
 static const int CHECKS = 4;
 static const float START_PAST_RING = 400; // m outside the threat ring the attackers start
 static const float PASS_OUT = 500;        // m past the ring the passers-by walk across
 static const float PASS_LENGTH = 500;     // m they walk
 static const float LEAD_LEAST = 60;       // s of warning wanted before the ring
 static const float WAY_SLACK = 30;        // degrees
 static const float SPREAD = 160;          // m between the two teams of each pair
 static const int GIVE_UP = 260;           // s after the start with nobody at the ring
 static const string MOVE_WAYPOINT = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";

 static int s_iStep;
 static int s_iStartTick;
 static string s_sUs, s_sEnemy, s_sOwnerWas;
 static ACR_Base s_Base;
 static vector s_vDir;                  // from the base toward where the attackers start
 static ref array<SCR_AIGroup> s_aAttackers;
 static ref array<SCR_AIGroup> s_aPassers;
 static ref ACR_DefenceForecast s_Forecast;
 static float s_fNotedAt = -1;          // tick the base was first forecast
 static float s_fNotedBearing = -1;
 static float s_fRingAt = -1;           // tick the first attacker was inside the threat ring
 static float s_fPasserWay;             // most strength any way toward the passers-by showed

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (s_iStep == 0 && tick >= 8) Setup(h, manager, tick);
  if (s_iStep == 1) Follow(h, manager, tick);
  if (s_iStep == 2) Finish(h, manager);
 }

 protected static void Setup(ACR_TestHarness h, ACR_Manager manager, int tick)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetFaction())
  {
   ACR_TestRegress.Abort("forecastattack", "no Alpha squad");
   s_iStep = 3;
   return;
  }
  s_sUs = alpha.GetFaction().GetFactionKey();
  s_sEnemy = "USSR";
  if (s_sUs == "USSR") s_sEnemy = "US";
  // A base with dry land round it on some side: ours for the test.
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   vector dir;
   if (!DryWay(base, dir)) continue;
   s_Base = base;
   s_vDir = dir;
   break;
  }
  if (!s_Base)
  {
   ACR_TestRegress.Abort("forecastattack", "no base with dry ground round it: " + s_sDryWhy);
   s_iStep = 3;
   return;
  }
  s_sOwnerWas = s_Base.m_sOwner;
  s_Base.m_sOwner = s_sUs;
  float ring = ACR_AICommander.THREAT_DISTANCE + s_Base.m_fRadius;
  vector center = s_Base.GetOrigin();
  vector start = ACR_Manager.Ground(center + s_vDir * (ring + START_PAST_RING));
  vector across = Vector(-s_vDir[2], 0, s_vDir[0]);
  vector passFrom = ACR_Manager.Ground(center + across * (ring + PASS_OUT) - s_vDir * (PASS_LENGTH * 0.5));
  vector passTo = ACR_Manager.Ground(center + across * (ring + PASS_OUT) + s_vDir * (PASS_LENGTH * 0.5));
  s_aAttackers = {};
  s_aPassers = {};
  ResourceName team = h.FireTeamOf(manager, s_sEnemy);
  for (int i = 0; i < 2; i++)
  {
   vector side = across * (i * SPREAD); // apart past ACR_DefenceForecast.MASS_GAP: this test is about movement, not massing
   SCR_AIGroup attacker = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(team, ACR_Manager.Ground(start + side)));
   if (attacker) { s_aAttackers.Insert(attacker); MoveTo(attacker, center + side); }
   SCR_AIGroup passer = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(team, ACR_Manager.Ground(passFrom + side)));
   if (passer) { s_aPassers.Insert(passer); MoveTo(passer, passTo + side); }
  }
  if (s_aAttackers.Count() < 2 || s_aPassers.Count() < 2)
  {
   ACR_TestRegress.Abort("forecastattack", "enemy fire teams didn't spawn");
   s_iStep = 3;
   return;
  }
  s_Forecast = new ACR_DefenceForecast(s_sUs);
  s_iStartTick = tick;
  s_iStep = 1;
  PrintFormat("[ACR] FORECAST TEST base %1 (ring %2 m), attackers from %3 starting %4 m past the ring, passers-by %5 m past it", s_Base.m_sName, Math.Round(ring), ACR_DefenceForecast.CompassOf(ACR_Intel.Bearing(center, start)), START_PAST_RING, PASS_OUT);
 }

 // Dry ground along both walks (the attackers' line in, the passers' line across), in one of 8 directions.
 static string s_sDryWhy;
 protected static bool DryWay(ACR_Base base, out vector dir)
 {
  float ring = ACR_AICommander.THREAT_DISTANCE + base.m_fRadius;
  vector center = base.GetOrigin();
  for (int k = 0; k < 8; k++)
  {
   float angle = k * 45 * Math.DEG2RAD;
   vector d = Vector(Math.Sin(angle), 0, Math.Cos(angle));
   vector across = Vector(-d[2], 0, d[0]);
   int wet = 0;
   for (float m = 0; m <= ring + START_PAST_RING + 20; m += 40)
    if (BBB_Terrain.IsWaterAt(ACR_Manager.Ground(center + d * m))) wet++;
   for (float t = -0.5; t <= 0.51; t += 0.1)
    if (BBB_Terrain.IsWaterAt(ACR_Manager.Ground(center + across * (ring + PASS_OUT) + d * (PASS_LENGTH * t)))) wet++;
   s_sDryWhy += string.Format("%1 %2 wet; ", base.m_sName, wet);
   if (wet > 0) continue;
   dir = d;
   return true;
  }
  return false;
 }

 protected static void MoveTo(SCR_AIGroup group, vector where)
 {
  SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(ACR_Manager.SpawnPrefab(MOVE_WAYPOINT, ACR_Manager.Ground(where)));
  if (waypoint) group.AddWaypoint(waypoint);
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 protected static void Follow(ACR_TestHarness h, ACR_Manager manager, int tick)
 {
  ACR_Intel intel = manager.IntelFor(s_sUs);
  if (!intel) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (SCR_AIGroup a : s_aAttackers) Spot(intel, a, now);
  foreach (SCR_AIGroup p : s_aPassers) Spot(intel, p, now);
  array<ACR_Base> ours = {};
  ours.Insert(s_Base);
  array<int> defence = {0};
  array<ACR_Base> enemy = {};
  // The threat as the commander counts it: attackers inside the ring.
  vector center = s_Base.GetOrigin();
  float ring = ACR_AICommander.THREAT_DISTANCE + s_Base.m_fRadius;
  vector nearest;
  float nearestGap = -1;
  foreach (SCR_AIGroup g : s_aAttackers)
  {
   if (!g) continue;
   float gap = vector.DistanceXZ(g.GetOrigin(), center);
   if (nearestGap < 0 || gap < nearestGap) { nearestGap = gap; nearest = g.GetOrigin(); }
  }
  bool inside = nearestGap >= 0 && nearestGap <= ring;
  int threat = 0;
  if (inside) threat = 8;
  s_Forecast.NoteThreat(s_Base, threat, inside, nearest, now);
  s_Forecast.Update(intel.m_Contacts, ours, defence, enemy, 700, "", now);
  foreach (string line : s_Forecast.m_aSaid) PrintFormat("[ACR] FORECAST TEST t=%1: %2", tick - s_iStartTick, line);
  s_Forecast.m_aSaid.Clear();
  ACR_BaseForecast entry = s_Forecast.Of(s_Base);
  float passerBearing = ACR_Intel.Bearing(center, center + Vector(-s_vDir[2], 0, s_vDir[0]));
  if (entry)
  {
   if (s_fNotedAt < 0 && entry.m_fExpected >= ACR_DefenceForecast.NOTE)
   {
    s_fNotedAt = tick;
    ACR_ForecastWay lead = entry.LeadWay();
    if (lead) s_fNotedBearing = lead.m_fBearing;
   }
   foreach (ACR_ForecastWay way : entry.m_aWays)
    if (!way.m_bRemembered && ACR_DefenceForecast.AngleBetween(way.m_fBearing, passerBearing) <= 45) s_fPasserWay = Math.Max(s_fPasserWay, way.m_fStrength);
  }
  if (inside && s_fRingAt < 0) s_fRingAt = tick;
  if (s_fRingAt >= 0 && tick - s_fRingAt >= 3) s_iStep = 2;
  else if (tick - s_iStartTick >= GIVE_UP) s_iStep = 2;
 }

 protected static void Spot(ACR_Intel intel, SCR_AIGroup group, float now)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   intel.m_SeenAt.Set(body, body.GetOrigin());
   intel.m_SeenTime.Set(body, now);
  }
 }

 protected static void Finish(ACR_TestHarness h, ACR_Manager manager)
 {
  s_iStep = 3;
  vector center = s_Base.GetOrigin();
  float attackBearing = ACR_Intel.Bearing(center, center + s_vDir);
  float lead = -1;
  if (s_fNotedAt >= 0 && s_fRingAt >= 0) lead = s_fRingAt - s_fNotedAt;
  ACR_TestRegress.Check("forecastattack: an attack walking in is forecast before it reaches the threat ring", s_fRingAt >= 0 && lead >= LEAD_LEAST, string.Format("forecast at t=%1, ring at t=%2 (s after the start), lead %3 s", s_fNotedAt - s_iStartTick, s_fRingAt - s_iStartTick, lead));
  bool rightWay = s_fNotedBearing >= 0 && ACR_DefenceForecast.AngleBetween(s_fNotedBearing, attackBearing) <= WAY_SLACK;
  ACR_TestRegress.Check("forecastattack: ... from the way it comes", rightWay, string.Format("forecast way %1 deg, attack from %2 deg", Math.Round(s_fNotedBearing), Math.Round(attackBearing)));
  ACR_TestRegress.Check("forecastattack: the self-check counts it a hit", s_Forecast.m_iHits == 1 && s_Forecast.m_iUnforecast == 0, string.Format("hits %1, wrong way %2, not forecast %3", s_Forecast.m_iHits, s_Forecast.m_iWrongWay, s_Forecast.m_iUnforecast));
  ACR_TestRegress.Check("forecastattack: a squad walking past, well out, isn't taken for an attack", s_fPasserWay < 1, string.Format("most strength on the passers' side %1", s_fPasserWay));
  foreach (SCR_AIGroup a : s_aAttackers) Kill(h, a);
  foreach (SCR_AIGroup p : s_aPassers) Kill(h, p);
  s_Base.m_sOwner = s_sOwnerWas;
  ACR_TestRegress.Complete("forecastattack", CHECKS);
 }

 protected static void Kill(ACR_TestHarness h, SCR_AIGroup group)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
 }
}
