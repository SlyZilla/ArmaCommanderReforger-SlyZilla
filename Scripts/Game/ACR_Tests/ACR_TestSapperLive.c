// ACR_TestSapperLive: an AI sapper plants, wires and fires a real vanilla charge at an enemy
// FOB HQ over real ticks (animations, inventory moves and the detonator need frames).
// Started from srv:structures at tick 50, stepped every tick until it ends (by tick 94).
class ACR_TestSapperLive
{
 static const int CHECKS = 7;
 static int s_iCookStart, s_iSecondaryStart, s_iExplodedStart;
 static ref ACR_AIAttack s_Raid;
 static ACR_Base s_Fob;
 static ACR_Squad s_Alpha;
 static bool s_bArmedSeen, s_bWired, s_bDone, s_bOnGround;
 static float s_fChargeOffset = -1;

 static void Start(ACR_Manager manager, ACR_Squad alpha, vector home)
 {
  s_Alpha = alpha;
  vector spot;
  bool found = false;
  for (int i = 0; i < 24 && !found; i++)
  {
   float angle = (i * 15 + 7) * Math.DEG2RAD;
   vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 1150, 0, Math.Sin(angle) * 1150));
   if (!ACR_Manager.OnMap(candidate) || BBB_Terrain.IsWaterAt(candidate) || ACR_TestStructures.InsideABase(candidate, 200)) continue;
   spot = candidate;
   found = true;
  }
  if (!found)
  {
   ACR_TestRegress.Abort("structures", "no spot for the live sapper FOB");
   return;
  }
  ACR_TestStructures.Place(ACR_FobBuild.HQ_USSR, spot);
  manager.m_Structures.ACR_TestForceTick();
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), spot) < 5) s_Fob = base;
  IEntity hq = manager.m_Structures.HqEntityOf(s_Fob);
  if (!s_Fob || !hq)
  {
   ACR_TestRegress.Abort("structures", "live sapper FOB did not form");
   return;
  }
  // Alpha stands round the HQ (the approach itself is ordinary movement).
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  int n = 0;
  AIAgent firstMoved;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   // The hospital fixture's patient has his AI off: he stays at the hospital.
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) continue;
   float a = n * 50 * Math.DEG2RAD;
   // Outside the HQ's tents and sandbags (teleporting into them hurt or killed soldiers).
   body.SetOrigin(ACR_Manager.Ground(hq.GetOrigin() + Vector(Math.Cos(a) * (9 + n), 0, Math.Sin(a) * (9 + n))));
   if (!firstMoved) firstMoved = agent;
   n++;
  }
  // The patient stays behind at the hospital: the rest must not regroup on him.
  if (firstMoved && firstMoved.GetParentGroup()) firstMoved.GetParentGroup().SetNewLeader(firstMoved);
  alpha.HoldRegroupForTest(GetGame().GetWorld().GetWorldTime() + 120000);
  // Teleported 1150 m: bring the soldier navmesh in round the HQ, or the fallback has no path.
  if (!agents.IsEmpty())
  {
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agents[0].FindComponent(AIPathfindingComponent));
   NavmeshWorldComponent mesh;
   if (pathing) mesh = pathing.GetNavmeshComponent();
   if (mesh)
   {
    mesh.LoadTileIn(hq.GetOrigin());
    for (int t = 0; t < 8; t++)
    {
     float ta = t * 45 * Math.DEG2RAD;
     mesh.LoadTileIn(hq.GetOrigin() + Vector(Math.Cos(ta) * 60, 0, Math.Sin(ta) * 60));
    }
   }
  }
  alpha.m_iAITask = ACR_EAITask.ATTACK;
  ACR_SapperDemo.s_fFuseMs = 4000;
  ACR_Supports.COOKOFF_LAST_MS = 4000; // all secondaries before the final checks
  s_iCookStart = ACR_Supports.s_iCookOffs;
  s_iSecondaryStart = ACR_Supports.s_iSecondaries;
  s_iExplodedStart = ACR_Supports.s_iExploded;
  ACR_AICommander attacker = new ACR_AICommander(manager, "US");
  s_Raid = new ACR_AIAttack();
  s_Raid.m_Target = s_Fob;
  s_Raid.m_bFobRaid = true;
  s_Raid.m_Squads.Insert(alpha);
  s_Raid.m_SapperDemo = new ACR_SapperDemo(manager, attacker, s_Raid);
  PrintFormat("[ACR] SAPPERTEST live raid on %1 at %2 with %3 soldiers", s_Fob.m_sName, hq.GetOrigin(), n);
 }

 // Tick 100: the cook-off's heavy blast and secondaries have all gone off.
 static void Final()
 {
  int main = ACR_Supports.s_iExploded - s_iExplodedStart;
  int secondaries = ACR_Supports.s_iSecondaries - s_iSecondaryStart;
  ACR_TestRegress.Check("sapperlive: cook-off = one heavy blast and the secondaries", main == 1 && secondaries == ACR_Supports.COOKOFF_SECONDARIES, string.Format("heavy %1, secondaries %2", main, secondaries));
  ACR_Supports.COOKOFF_LAST_MS = 7500;
 }

 static void Step(ACR_Manager manager, int tick)
 {
  if (s_bDone || !s_Raid || !s_Raid.m_SapperDemo) return;
  ACR_SapperDemo demo = s_Raid.m_SapperDemo;
  demo.Tick(GetGame().GetWorld().GetWorldTime());
  if (tick % 4 == 0)
  {
   IEntity hqNow = manager.m_Structures.HqEntityOf(s_Fob);
   IEntity nearest;
   if (hqNow) nearest = ACR_SapperDemo.PlanterFor(s_Alpha, hqNow.GetOrigin());
   float gap = -1;
   if (nearest) gap = vector.DistanceXZ(nearest.GetOrigin(), hqNow.GetOrigin());
   int guards = 0;
   if (hqNow) guards = ACR_SapperDemo.SoldiersNear(hqNow.GetOrigin(), "USSR", 30);
   PrintFormat("[ACR] SAPPERTEST t=%1 state %2, nearest soldier %3 m from the HQ, defenders %4, order %5", tick, demo.State(), gap, guards, s_Alpha.m_Order);
   ChimeraCharacter planter = ChimeraCharacter.Cast(demo.Planter());
   if (planter && demo.DemoCharge())
   {
    CharacterControllerComponent pc = planter.GetCharacterController();
    AIControlComponent ai = AIControlComponent.Cast(planter.FindComponent(AIControlComponent));
    AIAgent pa;
    if (ai) pa = ai.GetControlAIAgent();
    string path = "no agent";
    if (pa && pa.GetMovementComponent())
    {
     array<vector> points = {};
     pa.GetMovementComponent().GetCurrentPath(points);
     path = string.Format("%1 path points", points.Count());
     if (!points.IsEmpty()) path = path + string.Format(", ends %1 m from the fallback %2 at %3, waypoint %4", (int)vector.DistanceXZ(points[points.Count() - 1], demo.FallbackPos()), demo.FallbackPos(), points[points.Count() - 1], s_Alpha.m_MoveTarget);
    }
    string wpAt = "none";
    if (s_Alpha.m_Waypoint) wpAt = s_Alpha.m_Waypoint.GetOrigin().ToString();
    PrintFormat("[ACR] SAPPERTEST planter %1 m from the charge, stance %2, using item %3, AI active %4, %5, waypoint at %6", vector.DistanceXZ(planter.GetOrigin(), demo.DemoCharge().GetOrigin()), pc && pc.GetStance(), pc && pc.IsUsingItem(), ai && ai.IsAIActivated(), path, wpAt);
   }
  }
  IEntity charge = demo.DemoCharge();
  if (demo.State() == ACR_ESapperDemoState.FALLBACK && !s_bArmedSeen)
  {
   s_bArmedSeen = true;
   SCR_ExplosiveChargeComponent comp = ACR_SapperDemo.ChargeComponent(charge);
   s_bWired = comp && comp.GetUsedFuzeType() == SCR_EFuzeType.REMOTE && demo.Detonator() != null && !demo.UsesTimedFuze();
   InventoryItemComponent item;
   if (charge) item = InventoryItemComponent.Cast(charge.FindComponent(InventoryItemComponent));
   s_bOnGround = charge && !charge.GetParent() && (!item || !item.GetParentSlot());
   if (charge) s_fChargeOffset = vector.DistanceXZ(charge.GetOrigin(), demo.ChargePos());
   PrintFormat("[ACR] SAPPERTEST armed: placed by %1, animation %2, fuze %3, detonator %4, offset %5 m", demo.PlacedBy(), demo.PlantAnimationStarted(), comp && comp.GetUsedFuzeType(), demo.Detonator() != null, s_fChargeOffset);
  }
  // The blast removes the charge; the demo notes it on its next tick.
  bool blown = demo.IsDetonated() || (!demo.FiredBy().IsEmpty() && !charge);
  bool ended = blown || demo.State() == ACR_ESapperDemoState.DETONATED || demo.State() == ACR_ESapperDemoState.FAILED || (demo.State() == ACR_ESapperDemoState.NONE && demo.IsDefused());
  if (!ended && tick < 94) return;
  s_bDone = true;
  manager.m_Structures.ACR_TestForceTick();
  PrintFormat("[ACR] SAPPERTEST end at tick %1: state %2, placed by %3, fired by %4, FOB lost %5 retired %6", tick, demo.State(), demo.PlacedBy(), demo.FiredBy(), s_Fob.m_bHqLost, s_Fob.m_bRetired);
  ACR_TestRegress.Check("sapperlive: a real charge lies on the ground by the HQ", s_bArmedSeen && s_bOnGround && s_fChargeOffset >= 0 && s_fChargeOffset < 2.5, string.Format("placed by %1, offset %2", demo.PlacedBy(), s_fChargeOffset));
  ACR_TestRegress.Check("sapperlive: the sapper plays the game's place animation", demo.PlantAnimationStarted(), demo.PlacedBy());
  ACR_TestRegress.Check("sapperlive: the charge is wired to his blasting machine", s_bWired, "");
  ACR_TestRegress.Check("sapperlive: he fires it with the detonator", demo.FiredBy() == "detonator" && blown, demo.FiredBy());
  ACR_TestRegress.Check("sapperlive: the blast destroys the HQ and the FOB goes offline", s_Fob.m_bHqLost || s_Fob.m_bRetired, "");
  ACR_TestRegress.Check("sapperlive: the HQ cooks off", ACR_Supports.s_iCookOffs == s_iCookStart + 1, string.Format("%1", ACR_Supports.s_iCookOffs - s_iCookStart));
  ACR_SapperDemo.s_fFuseMs = 45000;
  IEntity leftHq = manager.m_Structures.HqEntityOf(s_Fob);
  if (leftHq) SCR_EntityHelper.DeleteEntityAndChildren(leftHq);
  manager.m_Structures.ACR_TestForceTick();
  if (s_Alpha)
  {
   s_Alpha.m_iAITask = ACR_EAITask.FREE;
   s_Alpha.ClearOrders();
  }
 }
}
