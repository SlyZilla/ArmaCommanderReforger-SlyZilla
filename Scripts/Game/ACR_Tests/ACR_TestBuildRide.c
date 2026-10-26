// -acrBuildRideTest 1 (server-capable, up to ~6 min): a far builder rides out (RideToBuild). The
// roadblock site of our first front base is found; Alpha (all its men) is put RIDE_OUT m from it
// (past the Balanced doctrine's ride distance, 900 m) with a truck taxi beside it, every other
// squad of ours busy. ManageRoadblocks must send Alpha by
// truck, the job held; once the ride is over and Alpha is out, the held job is given: Alpha builds
// the roadblock at that site.
class ACR_TestBuildRide
{
 static const int CHECKS = 2;
 static const float RIDE_OUT = 1050;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static ref ACR_AICommander s_AI;
 static vector s_Site;
 static vector s_Spot;
 static int s_iStep;
 static int s_iStepAt;
 static string s_sDetail;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!alpha || !alpha.GetAnchor() || !us || tick < 4) return;
  if (tick > 480) ACR_TestRegress.Abort("buildride", string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  int waited = tick - s_iStepAt;

  // 0: the site, a spot RIDE_OUT m from it, the taxi there; 1: Alpha moved there; 2: dispatched by
  // truck; 3: out of the ride, building.
  if (s_iStep == 0)
  {
   s_AI = new ACR_AICommander(manager, "US");
   s_AI.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
   array<ACR_Base> fronts = {};
   array<vector> enemies = {};
   s_AI.FrontBasesForTest(fronts, enemies);
   if (fronts.IsEmpty())
   {
    ACR_TestRegress.Abort("buildride", "no US front base");
    return;
   }
   int considered;
   string none;
   ACR_RoadSite site = ACR_RoadblockSiting.Best(fronts[0].GetOrigin(), enemies[0], "US", false, considered, none);
   if (!site)
   {
    ACR_TestRegress.Abort("buildride", "no road site: " + none);
    return;
   }
   s_Site = site.m_vPos;
   s_Spot = SpotFrom(s_Site, fronts);
   if (s_Spot == vector.Zero)
   {
    ACR_TestRegress.Abort("buildride", "no dry spot RIDE_OUT m from the site and 650 m from every front base");
    return;
   }
   manager.SpawnUnit("Taxi 1", "", TRUCK, true, "", ACR_Manager.Ground(s_Spot + Vector(20, 0, 10)), ACR_Manager.Ground(s_Spot + Vector(25, 0, 15)));
   PrintFormat("[ACR] BUILDRIDE site %1 at %2, Alpha to %3 (%4 m)", fronts[0].m_sName, s_Site, s_Spot, (int)vector.DistanceXZ(s_Spot, s_Site));
   NextStep(tick);
   return;
  }
  if (s_iStep == 1)
  {
   if (waited < 4) return;
   foreach (ACR_Squad other : manager.m_Squads)
   {
    if (!other || other == alpha || other.GetName() == "Taxi 1" || !other.GetFaction() || other.GetFaction().GetFactionKey() != "US") continue;
    other.m_iAITask = ACR_EAITask.ATTACK;
   }
   alpha.Halt("Holding");
   alpha.m_iAITask = ACR_EAITask.FREE;
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (int i, AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body) body.SetOrigin(ACR_Manager.Ground(s_Spot + Vector(i * 2, 0, 0)));
   }
   NextStep(tick);
   return;
  }
  if (s_iStep == 2)
  {
   if (waited < 3) return;
   us.m_iRP = 500;
   s_AI.StartTickForTest();
   s_AI.ResetBuildTimersForTest();
   s_AI.ManageRoadblocks();
   bool pending = s_AI.PendingBuildForTest(alpha);
   bool riding = manager.m_Rides.RideOf(alpha) != null;
   ACR_TestRegress.Check("buildride: a builder past the ride distance is sent by truck, the job held", pending && riding && !alpha.RoadblockPending(), string.Format("pending %1, ride %2, task %3, order '%4'", pending, riding, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_Order));
   if (!pending)
   {
    ACR_TestRegress.Complete("buildride", CHECKS);
    s_iStep = 9;
    return;
   }
   NextStep(tick);
   return;
  }
  if (s_iStep == 3)
  {
   s_AI.StartTickForTest();
   s_AI.ManagePendingBuildsForTest();
   bool building = alpha.m_iAITask == ACR_EAITask.ROADBLOCK && (alpha.RoadblockPending() || alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK);
   ACR_Ride ride = manager.m_Rides.RideOf(alpha);
   string state = "over";
   if (ride) state = typename.EnumToString(ACR_ERideState, ride.m_State);
   s_sDetail = string.Format("ride %1, mounted %2, pending %3, %4 m from the site, order '%5'", state, alpha.IsMounted(), s_AI.PendingBuildForTest(alpha), (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Site), alpha.m_Order);
   if (!building && waited < 440) return;
   bool atSite = vector.DistanceXZ(alpha.m_vRoadblockSite, s_Site) < 2;
   ACR_TestRegress.Check("buildride: out of the ride, the held job is given: it builds the roadblock at the site", building && atSite, string.Format("%1 s on: %2, building %3 at %4", waited, s_sDetail, building, alpha.m_vRoadblockSite));
   ACR_TestRegress.Complete("buildride", CHECKS);
   s_iStep = 9;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // A dry spot RIDE_OUT m from the site, 650 m or more from every front base (none near) and within
 // FAR_BUILDER of the first.
 protected static vector SpotFrom(vector site, notnull array<ACR_Base> fronts)
 {
  for (int k = 0; k < 16; k++)
  {
   float angle = k * 22.5 * Math.DEG2RAD;
   vector at = ACR_Manager.Ground(site + Vector(Math.Sin(angle) * RIDE_OUT, 0, Math.Cos(angle) * RIDE_OUT));
   if (vector.DistanceXZ(at, fronts[0].GetOrigin()) > ACR_AICommander.FAR_BUILDER - 50) continue;
   if (at[1] < 2) continue;
   bool clear = true;
   foreach (ACR_Base front : fronts)
   {
    if (vector.DistanceXZ(at, front.GetOrigin()) < 650) clear = false;
   }
   if (clear) return at;
  }
  return vector.Zero;
 }
}
