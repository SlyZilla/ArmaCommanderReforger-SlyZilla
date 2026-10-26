// -acrAttackReachTest 1 (server-capable, ~40 s): which squads can get to an attack (#27, long
// attacks stalled). A crewed truck stands by Alpha; no RP, so no air lift. Balanced (ride distance 900 m): 600 m out
// Alpha goes on foot; beyond WALK_REACH (a dry point 2.2-2.6 km out) it rides the truck, and the
// truck isn't counted for a second squad; with the truck busy (the control) Alpha is out of reach,
// where before it was sent to walk there; target choice counts only the strength that can get there; and the forming up
// waits for the slowest squad's travel (at least STAGING_TIMEOUT, at most STAGE_TRAVEL_MAX).
class ACR_TestAttackReach
{
 static const int CHECKS = 6;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static ACR_Squad s_Taxi;
 static bool s_bDone;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor()) return;
  vector at = alpha.GetAnchor().GetOrigin();
  if (tick == 4)
  {
   manager.SpawnUnit("Taxi 1", "", TRUCK, true, "", ACR_Manager.Ground(at + Vector(25, 0, 15)), ACR_Manager.Ground(at + Vector(30, 0, 20)));
   return;
  }
  if (tick < 12 || s_bDone) return;
  if (!s_Taxi)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Taxi 1") s_Taxi = squad;
  }
  // Wait for the truck to be a free taxi (crewed, holding).
  string why = "not spawned";
  if (s_Taxi) why = manager.m_Rides.WhyNotTaxi(s_Taxi, "US");
  if (!why.IsEmpty())
  {
   if (tick > 60) ACR_TestRegress.Abort("attackreach", "the truck isn't a free taxi: " + why);
   return;
  }
  s_bDone = true;

  // No RP, so no air lift is ready: the only ride is the truck.
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  int rp = data.m_iRP;
  data.m_iRP = 0;
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  // A dry point on the map beyond WALK_REACH, and one 600 m out the same way.
  vector farTarget = vector.Zero;
  vector nearTarget = vector.Zero;
  float farDistance = 0;
  vector farDir = vector.Zero;
  for (float reach = 2600; reach >= 2200 && farTarget == vector.Zero; reach -= 200)
  {
   for (int k = 0; k < 16 && farTarget == vector.Zero; k++)
   {
    float angle = k * 22.5 * Math.DEG2RAD;
    vector d = Vector(Math.Sin(angle), 0, Math.Cos(angle));
    vector farPoint = ACR_Manager.Ground(at + d * reach);
    vector nearPoint = ACR_Manager.Ground(at + d * 600);
    if (!ACR_Manager.OnMap(farPoint) || BBB_Terrain.IsWaterAt(farPoint) || BBB_Terrain.IsWaterAt(nearPoint)) continue;
    farTarget = farPoint;
    nearTarget = nearPoint;
    farDistance = reach;
    farDir = d;
   }
  }
  if (farTarget == vector.Zero)
  {
   data.m_iRP = rp;
   ACR_TestRegress.Abort("attackreach", "no dry point on the map 2.2-2.6 km from Alpha");
   return;
  }

  float walkSeconds;
  array<ACR_Squad> none = {};
  int walk = ai.ReachForTest(alpha, nearTarget, none, false, walkSeconds);
  ACR_TestRegress.Check("attackreach: a squad 600 m out goes on foot", walk == ACR_AICommander.REACH_WALK, string.Format("how %1, %2 s", walk, (int)walkSeconds));

  float rideSeconds;
  array<ACR_Squad> taxis = {};
  int ride = ai.ReachForTest(alpha, farTarget, taxis, false, rideSeconds);
  ACR_TestRegress.Check("attackreach: beyond walking reach, with a free truck in pickup range, it rides", ride == ACR_AICommander.REACH_TAXI && taxis.Contains(s_Taxi), string.Format("%1 m out: how %2, %3 s, taxis %4", farDistance, ride, (int)rideSeconds, taxis.Count()));

  float againSeconds;
  int again = ai.ReachForTest(alpha, farTarget, taxis, false, againSeconds);
  bool notTwice = again != ACR_AICommander.REACH_TAXI || (taxis.Count() >= 2 && taxis[1] != s_Taxi);
  ACR_TestRegress.Check("attackreach: the truck isn't counted for a second squad of the attack", notTwice, string.Format("second look: how %1, taxis %2", again, taxis.Count()));

  // The control: the truck busy, so no ride to be had.
  s_Taxi.MoveTo(ACR_Manager.Ground(at + farDir * -300));
  float busySeconds;
  array<ACR_Squad> noTaxis = {};
  int busy = ai.ReachForTest(alpha, farTarget, noTaxis, false, busySeconds);
  ACR_TestRegress.Check("attackreach: with no ride to be had, a squad beyond walking reach is out of reach (before, it was sent to walk)", busy == ACR_AICommander.REACH_NONE, string.Format("%1 m out: how %2", farDistance, busy));

  array<ACR_Squad> free = {alpha};
  int farStrength = ai.ReachableStrengthForTest(free, farTarget);
  int nearStrength = ai.ReachableStrengthForTest(free, nearTarget);
  ACR_TestRegress.Check("attackreach: target choice counts only the strength that can get there", farStrength == 0 && nearStrength == alpha.Strength(), string.Format("%1 m out %2, 600 m out %3 (Alpha %4)", farDistance, farStrength, nearStrength, alpha.Strength()));

  float shortStage = ACR_AICommander.StageSeconds(100);
  float longStage = ACR_AICommander.StageSeconds(900);
  float capped = ACR_AICommander.StageSeconds(5000);
  ACR_TestRegress.Check("attackreach: the forming up waits for the slowest squad's travel", shortStage == ACR_AICommander.STAGING_TIMEOUT && longStage == 960 && capped == ACR_AICommander.STAGE_TRAVEL_MAX, string.Format("100 s -> %1, 900 s -> %2, 5000 s -> %3", shortStage, longStage, capped));
  s_Taxi.Halt("Holding");
  data.m_iRP = rp;
  ACR_TestRegress.Complete("attackreach", CHECKS);
 }
}
