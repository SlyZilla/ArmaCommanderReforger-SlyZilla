// -acrAmbushTest 1 (server-capable, ~3 min): the Ambush order. Plans: a spot near a road watches
// the road; a spot on the road makes it the kill zone and the squad hides off it (15-40 m out, on
// the navmesh, in sight of the kill zone), on the far side from a known enemy; away from roads it
// faces the threat or is refused.
// Then Alpha lies in wait 60 m from a kill zone: on the way it only returns fire and its side
// (only) sees the kill zone; in place it holds fire and goes prone; an enemy fire team (holding
// its fire) outside the kill zone doesn't spring it, one in the kill zone does; a second ambush
// is sprung by a loss; a new order ends it. Defend clicked on a road lays the ambush there (#12);
// 30 m off it, a plain Defend. In place each man takes his own spot (SPACING apart, in sight of the
// kill zone), and by a road the spots split across both sides of it, staggered.
class ACR_TestAmbush
{
 static const int CHECKS = 13;
 static int s_iStep;
 static int s_iStepAt;
 static vector s_vSite;
 static vector s_vKillZone;
 static string s_sEnemy;
 static SCR_AIGroup s_Enemy;
 static ref ACR_AmbushPlan s_Plan;
 static vector s_vRoad;
 static vector s_vRoadDir;
 static int s_iWaitingAt;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (System.IsCLIParam("acrAmbushSlopeTest"))
  {
   RunSlope(h, tick);
   return;
  }
  if (System.IsCLIParam("acrAmbushRoadTest") || System.IsCLIParam("acrAmbushRoadCommandedTest") || System.IsCLIParam("acrAmbushDragTest"))
  {
   RunRoad(h, tick);
   return;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   // The layout checks want one exact pick: no variety (ACR_Variety).
   ACR_Variety.s_bOff = true;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || !alpha.GetFaction())
  {
   if (tick == 60) ACR_TestRegress.Abort("ambush", "no Alpha squad");
   return;
  }
  if (tick > 345) ACR_TestRegress.Abort("ambush", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  string us = alpha.GetFaction().GetFactionKey();
  int waited = tick - s_iStepAt;
  // The enemy holds its fire throughout: only the ambush's own triggers count.
  if (s_Enemy) HoldFire(s_Enemy);

  if (s_iStep == 0)
  {
   CheckPlans(alpha, us);
   s_sEnemy = "USSR";
   if (us == "USSR") s_sEnemy = "US";
   // Alpha lies in wait 15 m from home, the kill zone 60 m beyond.
   vector home = alpha.GetAnchor().GetOrigin();
   s_vSite = ACR_Manager.Ground(home + Vector(15, 0, 0));
   s_vKillZone = ACR_Manager.Ground(s_vSite + Vector(0, 0, 60));
   bool ordered = alpha.Ambush(s_vSite, s_vKillZone);
   s_Plan = alpha.m_Ambush;
   bool returnFire = CombatMode(alpha) == EAIGroupCombatMode.RETURN_FIRE;
   bool ours = HasRing(us);
   bool theirs = HasRing(s_sEnemy);
   ACR_TestRegress.Check("ambush: on the way it returns fire only, and only its own side sees the kill zone", ordered && s_Plan && alpha.m_Order == "Ambush: moving in" && returnFire && ours && !theirs, string.Format("ordered %1, order '%2', combat mode %3, ring for %4 %5 / %6 %7", ordered, alpha.m_Order, typename.EnumToString(EAIGroupCombatMode, CombatMode(alpha)), us, ours, s_sEnemy, theirs));
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   // In place: each man to his spot (up to 45 s), then holding fire and prone (given 15 s to get down).
   if ((s_Plan.m_ePhase == ACR_EAmbushPhase.MOVING || s_Plan.m_ePhase == ACR_EAmbushPhase.POSITIONING) && waited < 150) return;
   if (s_iWaitingAt == 0) s_iWaitingAt = tick;
   int prone, alive;
   CountProne(alpha, prone, alive);
   if (prone < alive && tick - s_iWaitingAt < 15) return;
   bool holding = CombatMode(alpha) == EAIGroupCombatMode.HOLD_FIRE;
   ACR_TestRegress.Check("ambush: in place it holds its fire and gets down (prone, or crouched where prone sees nothing)", s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING && alpha.m_Order == "Ambush: waiting" && holding && s_Plan.m_bProne && prone * 2 >= alive && alive > 0, string.Format("phase %1, order '%2', combat mode %3, %4 of %5 prone", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), alpha.m_Order, typename.EnumToString(EAIGroupCombatMode, CombatMode(alpha)), prone, alive));
   CheckSpots(alpha);
   // An enemy fire team off to the side: 75 m from the spot, 96 m from the kill zone.
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, s_sEnemy), ACR_Manager.Ground(s_vSite + Vector(-75, 0, 0))));
   PrintFormat("[ACR] AMBUSH TEST enemy fire team outside the kill zone: %1", s_Enemy != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (waited < 12 && s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING) return;
   ACR_TestRegress.Check("ambush: enemies outside the kill zone don't spring it", s_Enemy && s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING, string.Format("enemy %1, phase %2, sprung by '%3'", s_Enemy != null, typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), s_Plan.m_sSprung));
   KillGroup(h, s_Enemy);
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, s_sEnemy), s_vKillZone));
   PrintFormat("[ACR] AMBUSH TEST enemy fire team in the kill zone: %1", s_Enemy != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING && waited % 5 == 0) PrintFormat("[ACR] AMBUSH TEST %1 s: Alpha detects %2 hostile(s)", waited, Detected(alpha));
   if (s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING && waited < 45) return;
   bool firing = CombatMode(alpha) == EAIGroupCombatMode.FIRE_AT_WILL;
   bool fed = Fed(manager, us, alpha.GetName() + " sprang its ambush");
   ACR_TestRegress.Check("ambush: an enemy in the kill zone springs it: fire at will, no longer prone, the side told", s_Plan.m_ePhase == ACR_EAmbushPhase.SPRUNG && s_Plan.m_sSprung.Contains("in the kill zone") && alpha.m_Order == "Ambush: sprung" && firing && !s_Plan.m_bProne && fed, string.Format("phase %1 after %2 s, sprung by '%3', order '%4', combat mode %5, prone setting %6, feed %7", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), waited, s_Plan.m_sSprung, alpha.m_Order, typename.EnumToString(EAIGroupCombatMode, CombatMode(alpha)), s_Plan.m_bProne, fed));
   KillGroup(h, s_Enemy);
   s_Enemy = null;
   // A second ambush from the same spot: sprung by a loss.
   alpha.Ambush(s_vSite, s_vKillZone);
   s_Plan = alpha.m_Ambush;
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   if (s_Plan.m_ePhase == ACR_EAmbushPhase.MOVING && waited < 60) return;
   if (s_iWaitingAt >= 0)
   {
    s_iWaitingAt = -tick;
    // One of them is shot (as if by a sniper nobody saw).
    ChimeraCharacter victim;
    array<AIAgent> agents = {};
    alpha.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     ChimeraCharacter c = ChimeraCharacter.Cast(agent.GetControlledEntity());
     if (c && ACR_Respawn.IsAlive(c) && c != alpha.GetLeader()) victim = c;
    }
    PrintFormat("[ACR] AMBUSH TEST second ambush phase %1, killing one: %2", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), victim != null);
    if (victim) h.Kill(victim);
    return;
   }
   if (s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING && tick + s_iWaitingAt < 8) return;
   ACR_TestRegress.Check("ambush: a loss springs it", s_Plan.m_ePhase == ACR_EAmbushPhase.SPRUNG && s_Plan.m_sSprung == "took a loss", string.Format("phase %1, sprung by '%2'", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), s_Plan.m_sSprung));
   // A new order ends it.
   ACR_AmbushPlan ended = alpha.m_Ambush;
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-10, 0, 0));
   bool gone = !alpha.m_Ambush && ended && !ended.m_bProne && CombatMode(alpha) == EAIGroupCombatMode.FIRE_AT_WILL && !HasRing(us);
   ACR_TestRegress.Check("ambush: a new order ends it (usual combat mode, no ring)", gone && alpha.m_Order == "Moving", string.Format("ambush %1, combat mode %2, ring %3, order '%4'", alpha.m_Ambush != null, typename.EnumToString(EAIGroupCombatMode, CombatMode(alpha)), HasRing(us), alpha.m_Order));
   ACR_TestRegress.Complete("ambush", CHECKS);
   s_iStep = 5;
  }
 }

 // A spot near a road watches the road; a spot on it hides the squad off it, away from a known
 // enemy; away from roads the kill zone faces the side's threat, or the order is refused
 // without one.
 // In place: every living man on his own spot (one straggler allowed), the spots SPACING apart
 // and most of them in sight of the kill zone.
 protected static void CheckSpots(ACR_Squad alpha)
 {
  int men = s_Plan.m_aMen.Count();
  int alive, seeing;
  foreach (IEntity man : s_Plan.m_aMen) if (man && ACR_Respawn.IsAlive(man)) alive++;
  int on = ACR_Ambush.OnSpots(s_Plan);
  foreach (vector spot : s_Plan.m_aSpots) if (ACR_Ambush.SeesKillZone(spot, s_Plan.m_vKillZone)) seeing++;
  float closest = MinSpacing(s_Plan.m_aSpots);
  bool spread = men > 1 && s_Plan.m_aSpots.Count() == men && on >= alive - 1 && closest >= ACR_Ambush.SPACING - 0.1 && seeing * 2 >= men;
  ACR_TestRegress.Check("ambush: in place each man takes his own spot, SPACING apart, in sight of the kill zone", spread, string.Format("%1 men, %2 spots: %3 spots taken (%4 alive), closest two %5 m apart, %6 in sight of the kill zone", men, s_Plan.m_aSpots.Count(), on, alive, closest, seeing));
 }

 // -acrAmbushRoadTest 1 (~2 min): as the pn11 playtest laid it: Defend clicked on a road near Alpha,
 // Alpha on Fast (its orders forced), the spots on both sides of the road; every man must reach his
 // own. -acrAmbushRoadCommandedTest 1: Alpha under a (pretend) player's command too.
 // -acrAmbushSlopeTest 1: pn12d (27 Oct) had a Defend dragged to a road put all three men across it,
 // on its lower side where they couldn't see it, and 100-170 m apart. A dragged ambush keeps every
 // post on the pressed side, by the pressed spot (within DRAG_SPREAD), higher ground first; pressed
 // on the low side it still never crosses. A plain one (no drag) may use both sides (logged).
 protected static void RunSlope(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Variety.s_bOff = true;
  }
  if (tick != 8) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   ACR_TestRegress.Abort("ambush", "no Alpha");
   return;
  }
  // The road near the range that slopes most across it: its high side.
  vector from = alpha.GetAnchor().GetOrigin();
  vector road, dir, high;
  float width;
  float drop = 0;
  for (int x = -600; x <= 600; x += 50)
  {
   for (int z = -600; z <= 600; z += 50)
   {
    vector r, d;
    float w;
    if (!ACR_Minefields.Snap(from + Vector(x, 0, z), 30, r, d, w)) continue;
    vector side = Vector(d[2], 0, -d[0]);
    vector a = ACR_Manager.Ground(r + side * (w * 0.5 + 50));
    vector b = ACR_Manager.Ground(r - side * (w * 0.5 + 50));
    if (BBB_Terrain.IsWaterAt(a) || BBB_Terrain.IsWaterAt(b)) continue;
    float dh = a[1] - b[1];
    if (Math.AbsFloat(dh) <= drop) continue;
    drop = Math.AbsFloat(dh);
    road = r;
    dir = d;
    width = w;
    high = side;
    if (dh < 0) high = side * -1;
   }
  }
  if (drop < 2)
  {
   ACR_TestRegress.Check("ambush (dragged, sloped road): a road sloping 2 m or more across near the range", false, string.Format("the most %1 m", drop));
   ACR_TestRegress.Complete("ambush", 3);
   return;
  }
  vector killZone = ACR_Manager.Ground(road);
  PrintFormat("[ACR] AMBUSH SLOPE road at %1 (%2 m wide), %3 m higher 50 m out on its high side", road, width, drop);
  string highWhy, lowWhy;
  float highUp, lowUp;
  bool highOk = PinnedSide(alpha, road, width, killZone, high, highUp, highWhy) && highUp > 0;
  ACR_TestRegress.Check("ambush (dragged, sloped road): pressed on the high side, every post stays on it, above the road, by the pressed spot", highOk, highWhy);
  bool lowOk = PinnedSide(alpha, road, width, killZone, high * -1, lowUp, lowWhy);
  ACR_TestRegress.Check("ambush (dragged, sloped road): pressed on the low side, every post stays on that side: never across", lowOk, lowWhy);
  // A plain Defend on that road (no drag): both sides allowed.
  vector hide = ACR_Manager.Ground(road + high * (width * 0.5 + 20));
  array<vector> spots = {};
  array<int> sides = {};
  string plainWhy;
  int placed = ACR_Ambush.PickSpots(alpha, hide, killZone, 4, spots, sides, plainWhy, "slope plain");
  int onHigh;
  float plainSpread = 0;
  foreach (vector s : spots)
  {
   if (vector.Dot(s - road, high) > 0) onHigh++;
   plainSpread = Math.Max(plainSpread, vector.DistanceXZ(s, spots[0]));
  }
  bool plainOk = placed == 4 && plainSpread <= ACR_Ambush.PLAIN_SPREAD + ACR_Ambush.SPACING;
  ACR_TestRegress.Check("ambush (plain, sloped road): a plain Defend still places its men by the road, either side, within the spread cap", plainOk, string.Format("%1 placed, %2 on the high side, spread %3 m; %4", placed, onHigh, plainSpread, plainWhy));
  ACR_TestRegress.Complete("ambush", 3);
 }

 // A dragged ambush pressed 60 m out on 'side' of the road: true when every post is on that side,
 // within DRAG_SPREAD (a round-the-posts fallback SPACING more) of the first.
 protected static bool PinnedSide(ACR_Squad alpha, vector road, float width, vector killZone, vector side, out float meanUp, out string detail)
 {
  vector site = ACR_Manager.Ground(road + side * (width * 0.5 + 60));
  array<vector> spots = {};
  array<int> sides = {};
  string why;
  int placed = ACR_Ambush.PickSpots(alpha, site, killZone, 4, spots, sides, why, "slope pinned", true);
  bool ok = placed == 4;
  int wrong;
  float spread = 0;
  meanUp = 0;
  foreach (vector s : spots)
  {
   if (vector.Dot(s - road, side) < width * 0.5) wrong++;
   meanUp += s[1] - killZone[1];
   spread = Math.Max(spread, vector.DistanceXZ(s, spots[0]));
  }
  if (placed > 0) meanUp = meanUp / placed;
  ok = ok && wrong == 0 && spread <= ACR_Ambush.DRAG_SPREAD + ACR_Ambush.SPACING;
  detail = string.Format("%1 posts, %2 off the pressed side, %3 m above the road on average, spread %4 m; %5", placed, wrong, meanUp, spread, why);
  return ok;
 }

 protected static void RunRoad(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   // The layout checks want one exact pick: no variety (ACR_Variety).
   ACR_Variety.s_bOff = true;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || tick < 5) return;
  bool drag = System.IsCLIParam("acrAmbushDragTest");
  if (tick > 200 && !drag) ACR_TestRegress.Abort("ambush", string.Format("road: timed out in step %1", s_iStep));
  if (tick > 345 && drag && s_iStep < 12) ACR_TestRegress.Abort("ambush", string.Format("drag: timed out in step %1", s_iStep));
  int waited = tick - s_iStepAt;
  // 0: the road nearest Alpha; far off, Alpha's men are put 45 m beside it (the playtest's squad
  // walked up to the road it ambushed), the navmesh brought in there.
  if (s_iStep == 0)
  {
   vector near;
   float width;
   bool found = BBB_Terrain.RoadPointNear(alpha.GetAnchor().GetOrigin(), 250, near) && ACR_Minefields.Snap(near, 4, s_vRoad, s_vRoadDir, width);
   if (!found && !FindRoad(alpha.GetAnchor().GetOrigin(), s_vRoad, s_vRoadDir))
   {
    ACR_TestRegress.Abort("ambush", "no road near the range");
    return;
   }
   float off = vector.DistanceXZ(s_vRoad, alpha.GetAnchor().GetOrigin());
   if (off > 150)
   {
    vector side = Vector(s_vRoadDir[2], 0, -s_vRoadDir[0]);
    array<AIAgent> agents = {};
    alpha.GetAgents(agents);
    foreach (int n, AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (body && ACR_Respawn.IsAlive(body)) body.SetOrigin(ACR_Manager.Ground(s_vRoad + side * 45 + s_vRoadDir * (n * 3 - 4)));
     AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
     if (pathing && pathing.GetNavmeshComponent()) pathing.GetNavmeshComponent().LoadTileIn(s_vRoad);
    }
    alpha.HoldRegroupForTest(GetGame().GetWorld().GetWorldTime() + 400000);
   }
   PrintFormat("[ACR] AMBUSH TEST road %1 m from Alpha (moved %2)", (int)off, off > 150);
   NextStep(tick);
   return;
  }
  // 1: (8 s on) Defend clicked on the road, as the playtest's player did.
  if (s_iStep == 1 && drag)
  {
   if (waited < 8) return;
   RunDrag(h, alpha, tick);
   return;
  }
  if (s_iStep >= 10)
  {
   RunDrag(h, alpha, tick);
   return;
  }
  if (s_iStep == 1)
  {
   if (waited < 8) return;
   alpha.SetMode(ACR_EMoveMode.FAST);
   if (System.IsCLIParam("acrAmbushRoadCommandedTest")) alpha.m_Commander = 1;
   vector site, killZone;
   string why;
   bool laid = ACR_Ambush.DefendOnRoad(alpha, s_vRoad, site, killZone, why);
   s_Plan = alpha.m_Ambush;
   PrintFormat("[ACR] AMBUSH TEST road: laid %1 (%2), forced %3, commander %4, Alpha %5 m from the site", laid, why, alpha.m_bForced, alpha.m_Commander, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), site));
   if (!laid || !s_Plan)
   {
    ACR_TestRegress.Abort("ambush", "road: no ambush laid: " + why);
    return;
   }
   NextStep(tick);
   return;
  }
  if (s_iStep == 2)
  {
   if ((s_Plan.m_ePhase == ACR_EAmbushPhase.MOVING || s_Plan.m_ePhase == ACR_EAmbushPhase.POSITIONING) && waited < 170) return;
   int men = s_Plan.m_aMen.Count();
   int on = ACR_Ambush.OnSpots(s_Plan);
   int across;
   foreach (int side : s_Plan.m_aSides) if (side < 0) across++;
   string kind = "Fast";
   if (System.IsCLIParam("acrAmbushRoadCommandedTest")) kind = "Fast, a player's squad";
   ACR_TestRegress.Check(string.Format("ambush (road, %1): every man reaches his own spot, on both sides of the road", kind), men > 1 && on == men && across > 0, string.Format("phase %1: %2 of %3 on their spots, %4 across the road, after %5 s", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), on, men, across, waited));
   ACR_TestRegress.Complete("ambush", 1);
   NextStep(tick);
  }
 }

 // -acrAmbushDragTest: Defend dragged (drag to aim, ACR_Ambush.DefendAimed) from 30 m beside the
 // road. To the road: an ambush with that road as the kill zone, every man on a post, waiting with
 // its fire held. Then away from the road: a Defend on posts facing the drag, firing at will.
 protected static void RunDrag(ACR_TestHarness h, ACR_Squad alpha, int tick)
 {
  int waited = tick - s_iStepAt;
  vector side = Vector(s_vRoadDir[2], 0, -s_vRoadDir[0]);
  vector from = ACR_Manager.Ground(s_vRoad + side * 30);
  if (s_iStep == 1)
  {
   alpha.SetMode(ACR_EMoveMode.FAST);
   vector site, killZone;
   bool ambush;
   string why;
   bool laid = ACR_Ambush.DefendAimed(alpha, from, s_vRoad + s_vRoadDir * 10, site, killZone, ambush, why);
   s_Plan = alpha.m_Ambush;
   PrintFormat("[ACR] AMBUSH TEST drag to the road: laid %1 (%2), ambush %3, kill zone %4 m from the road", laid, why, ambush, (int)vector.DistanceXZ(killZone, s_vRoad));
   if (!laid || !s_Plan)
   {
    ACR_TestRegress.Abort("ambush", "drag: nothing laid: " + why);
    return;
   }
   s_iStep = 10;
   s_iStepAt = tick;
   return;
  }
  if (s_iStep == 10)
  {
   if (s_Plan.m_ePhase != ACR_EAmbushPhase.WAITING && waited < 170) return;
   int on = ACR_Ambush.OnSpots(s_Plan);
   int men = s_Plan.m_aMen.Count();
   bool held = CombatMode(alpha) == EAIGroupCombatMode.HOLD_FIRE;
   ACR_TestRegress.Check("ambush (dragged to the road): an ambush on that road, every man on a post, holding fire", !s_Plan.m_bDefend && s_Plan.m_ePhase == ACR_EAmbushPhase.WAITING && on == men && men > 1 && held && alpha.m_Order.StartsWith("Ambush"),
    string.Format("phase %1, %2 of %3 on posts, hold fire %4, order '%5', after %6 s", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), on, men, held, alpha.m_Order, waited));
   // Then away from the road (from the same spot, the other way): a Defend facing the drag.
   vector site, killZone;
   bool ambush;
   string why;
   bool laid = ACR_Ambush.DefendAimed(alpha, from, from + side * 120, site, killZone, ambush, why);
   s_Plan = alpha.m_Ambush;
   PrintFormat("[ACR] AMBUSH TEST drag away from the road: laid %1 (%2), ambush %3", laid, why, ambush);
   if (!laid || !s_Plan)
   {
    ACR_TestRegress.Abort("ambush", "drag away: nothing laid: " + why);
    return;
   }
   s_iStep = 11;
   s_iStepAt = tick;
   return;
  }
  if (s_iStep == 11)
  {
   if (s_Plan.m_ePhase != ACR_EAmbushPhase.SPRUNG && waited < 120) return;
   int posts = ACR_Ambush.OnSpots(s_Plan);
   int count = s_Plan.m_aMen.Count();
   int mode = CombatMode(alpha);
   vector facing = s_Plan.m_vKillZone - s_Plan.m_vSite;
   facing[1] = 0;
   facing.Normalize();
   float along = vector.Dot(facing, side);
   ACR_TestRegress.Check("ambush (dragged away from the road): a Defend on posts facing the drag, firing at will", s_Plan.m_bDefend && s_Plan.m_ePhase == ACR_EAmbushPhase.SPRUNG && posts == count && count > 1 && mode != EAIGroupCombatMode.HOLD_FIRE && along > 0.9 && alpha.m_Order.StartsWith("Defend"),
    string.Format("phase %1, %2 of %3 on posts, combat mode %4, facing %5 along the drag, order '%6', after %7 s", typename.EnumToString(ACR_EAmbushPhase, s_Plan.m_ePhase), posts, count, typename.EnumToString(EAIGroupCombatMode, mode), along, alpha.m_Order, waited));
   ACR_TestRegress.Complete("ambush", 2);
   s_iStep = 12;
  }
 }

 protected static float MinSpacing(array<vector> spots)
 {
  float closest = 1000;
  foreach (int a, vector first : spots)
  {
   foreach (int b, vector second : spots)
   {
    if (b > a && vector.DistanceXZ(first, second) < closest) closest = vector.DistanceXZ(first, second);
   }
  }
  return closest;
 }

 protected static void CheckPlans(ACR_Squad alpha, string us)
 {
  vector road, dir;
  bool found = FindRoad(alpha.GetAnchor().GetOrigin(), road, dir);
  string why;
  vector site, killZone;
  if (found)
  {
   vector side = Vector(dir[2], 0, -dir[0]);
   vector click = ACR_Manager.Ground(road + side * 30);
   bool planned = ACR_Ambush.Plan(alpha, click, site, killZone, why);
   ACR_TestRegress.Check("ambush: a spot 30 m off a road watches the road", planned && vector.DistanceXZ(site, click) < 0.5 && ACR_Minefields.RoadClearance(killZone) < 0.5 && Math.AbsFloat(vector.DistanceXZ(killZone, click) - 30) < 8, string.Format("planned %1 (%2), kill zone %3 m from the spot, %4 m from the road edge", planned, why, vector.DistanceXZ(killZone, click), ACR_Minefields.RoadClearance(killZone)));
   // On the road: the road there is the kill zone, the squad hides off it.
   planned = ACR_Ambush.Plan(alpha, road, site, killZone, why);
   vector snapped, snapDir;
   float width;
   ACR_Minefields.Snap(road, 5, snapped, snapDir, width);
   vector hidden;
   string hideWhy;
   bool hid = ACR_Ambush.HidingSpot(alpha, road, dir, width, killZone, hidden, hideWhy);
   float off = ACR_Minefields.RoadClearance(site);
   bool sideOk = vector.Dot(site - road, ACR_Ambush.PreferredSide(alpha, road, dir)) > 0;
   bool sees = ACR_Ambush.SeesKillZone(site, killZone);
   bool mesh = ACR_Ambush.OnMesh(alpha, site);
   ACR_TestRegress.Check("ambush: on the road, the road is the kill zone and the squad hides 15-40 m off it, on the navmesh, in sight of the kill zone", planned && hid && vector.DistanceXZ(site, hidden) < 0.5 && off >= 12 && off <= 45 && sideOk && sees && mesh && vector.DistanceXZ(killZone, road) < 1, string.Format("planned %1 (%2), %3 m off the road edge, preferred side %4, sees the kill zone %5, on the navmesh %6; %7", planned, why, off, sideOk, sees, mesh, hideWhy));
   // A known enemy off to one side of the road: the squad lies on the other side.
   bool farSide = false;
   string farDetail = "no intel";
   ACR_Intel intel = ACR_Manager.GetInstance().IntelFor(us);
   if (intel)
   {
    ACR_Contact contact = new ACR_Contact();
    contact.m_Pos = ACR_Manager.Ground(road + side * 150);
    intel.m_Contacts.Insert(contact);
    vector siteA, zoneA;
    string whyA;
    bool plannedA = ACR_Ambush.Plan(alpha, road, siteA, zoneA, whyA);
    intel.m_Contacts.RemoveItem(contact);
    contact.m_Pos = ACR_Manager.Ground(road - side * 150);
    intel.m_Contacts.Insert(contact);
    vector siteB, zoneB;
    string whyB;
    bool plannedB = ACR_Ambush.Plan(alpha, road, siteB, zoneB, whyB);
    intel.m_Contacts.RemoveItem(contact);
    float acrossA = vector.Dot(siteA - road, side);
    float acrossB = vector.Dot(siteB - road, side);
    farSide = plannedA && plannedB && acrossA < 0 && acrossB > 0;
    farDetail = string.Format("enemy on side +: squad at %1 m; enemy on side -: squad at %2 m", acrossA, acrossB);
   }
   ACR_TestRegress.Check("ambush: with a known enemy off to one side of the road the squad hides on the other side", farSide, farDetail);
   // Each man's spot by the road: both sides where both see the road, staggered, SPACING apart.
   array<vector> roadSpots = {};
   array<int> roadSides = {};
   string spotsWhy;
   int placed = ACR_Ambush.PickSpots(alpha, site, killZone, 4, roadSpots, roadSides, spotsWhy);
   int ours, across;
   foreach (int roadSide : roadSides)
   {
    if (roadSide > 0) ours++;
    else across++;
   }
   bool split = placed == 4 && ours >= 1 && across >= 1 && MinSpacing(roadSpots) >= ACR_Ambush.SPACING - 0.1;
   ACR_TestRegress.Check("ambush: by a road the men's spots split across both sides of it, SPACING apart", split, string.Format("%1 spots, %2 on the site's side, %3 across, closest two %4 m apart; %5", placed, ours, across, MinSpacing(roadSpots), spotsWhy));
   // Defend clicked on the road lays the ambush there; 30 m off it, a plain Defend (#12).
   vector defendSite, defendZone;
   string defendWhy;
   bool laid = ACR_Ambush.DefendOnRoad(alpha, road, defendSite, defendZone, defendWhy) && alpha.m_Ambush != null;
   alpha.Halt("Holding");
   vector offSite, offZone;
   string offWhy;
   bool plain = !ACR_Ambush.DefendOnRoad(alpha, click, offSite, offZone, offWhy) && alpha.m_Ambush == null;
   ACR_TestRegress.Check("ambush: Defend on a road lays an ambush there, 30 m off the road it doesn't", laid && plain, string.Format("on the road: laid %1 (%2), site %3 m off the road edge; 30 m off: '%4'", laid, defendWhy, ACR_Minefields.RoadClearance(defendSite), offWhy));
  }
  else
  {
   ACR_TestRegress.Check("ambush: a spot 30 m off a road watches the road", false, "no road near the range");
   ACR_TestRegress.Check("ambush: on the road, the road is the kill zone and the squad hides 15-40 m off it, on the navmesh, in sight of the kill zone", false, "no road near the range");
   ACR_TestRegress.Check("ambush: with a known enemy off to one side of the road the squad hides on the other side", false, "no road near the range");
   ACR_TestRegress.Check("ambush: by a road the men's spots split across both sides of it, SPACING apart", false, "no road near the range");
   ACR_TestRegress.Check("ambush: Defend on a road lays an ambush there, 30 m off the road it doesn't", false, "no road near the range");
  }
  // Away from roads.
  vector open;
  bool openFound;
  vector home = alpha.GetAnchor().GetOrigin();
  for (int r = 0; r <= 400 && !openFound; r += 50)
  {
   for (int a = 0; a < 8 && !openFound; a++)
   {
    vector p = ACR_Manager.Ground(home + Vector(Math.Sin(a * 0.785) * r, 0, Math.Cos(a * 0.785) * r));
    if (ACR_Minefields.RoadClearance(p) > ACR_Ambush.ROAD_RANGE + 20) { open = p; openFound = true; }
   }
  }
  vector threat = ACR_Intel.ThreatPoint(us, open);
  bool planned = ACR_Ambush.Plan(alpha, open, site, killZone, why);
  bool right;
  if (threat == vector.Zero) right = !planned && why.Contains("no road");
  else right = planned && vector.DistanceXZ(killZone, open) <= ACR_Ambush.THREAT_REACH + 1 && vector.Dot(killZone - open, threat - open) > 0;
  ACR_TestRegress.Check("ambush: away from roads the kill zone faces the threat (refused without one)", openFound && right, string.Format("open ground %1, threat %2, planned %3 '%4', kill zone %5 m out", openFound, threat != vector.Zero, planned, why, vector.DistanceXZ(killZone, open)));
 }

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
    if (BBB_Terrain.IsWaterAt(p)) continue;
    // One road only round the point, so 30 m off it is still nearest to the same road.
    array<BaseRoad> near = {};
    aiWorld.GetRoadNetworkManager().GetRoadsInAABB(p - Vector(70, 50, 70), p + Vector(70, 50, 70), near);
    if (near.Count() != 1) continue;
    if (ACR_Minefields.Snap(p, 2, point, dir, width)) return true;
   }
  }
  return false;
 }

 protected static int Detected(ACR_Squad squad)
 {
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent unit = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!unit || !unit.m_PerceptionComponent) continue;
   array<BaseTarget> targets = {};
   unit.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
   array<BaseTarget> detected = {};
   unit.m_PerceptionComponent.GetTargetsList(detected, ETargetCategory.DETECTED);
   count += targets.Count() + detected.Count();
  }
  return count;
 }

 protected static int CombatMode(ACR_Squad squad)
 {
  if (!squad.m_Group) return -1;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return -1;
  return utility.GetCombatModeExternal();
 }

 protected static void HoldFire(SCR_AIGroup group)
 {
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != EAIGroupCombatMode.HOLD_FIRE) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 protected static void CountProne(ACR_Squad squad, out int prone, out int alive)
 {
  prone = 0;
  alive = 0;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter c = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!c || !ACR_Respawn.IsAlive(c) || !c.GetCharacterController()) continue;
   alive++;
   if (c.GetCharacterController().GetStance() != ECharacterStance.STAND) prone++;
  }
 }

 protected static bool HasRing(string faction)
 {
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Ambush.Zones(faction, positions, radii, labels);
  foreach (int i, vector p : positions)
   if (vector.DistanceXZ(p, s_vKillZone) < 1 && labels[i].StartsWith("~Ambush")) return true;
  return false;
 }

 protected static bool Fed(ACR_Manager manager, string faction, string text)
 {
  ACR_Intel intel = manager.IntelFor(faction);
  if (!intel) return false;
  foreach (string line : intel.m_Feed)
   if (line.Contains(text)) return true;
  return false;
 }

 protected static void KillGroup(ACR_TestHarness h, SCR_AIGroup group)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
