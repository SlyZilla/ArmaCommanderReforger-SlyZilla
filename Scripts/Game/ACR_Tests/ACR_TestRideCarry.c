// -acrRideCarryTest 1 (server-capable, ~3 min): a player's Ride order takes the side's own APC or IFV
// (pn10b, 26 Oct: Fire Team 2's Ride was refused, "nearest: LAV-25 ..., 15 m, has a manned gun",
// which read as the LAV having no seats). A crewed LAV-25 (its turret manned) stands 15 m from
// Alpha and a crewed M1025 with its M2 manned 40 m off:
// - an AI ride still takes neither (a fighting vehicle isn't a taxi for the AI), and says so;
// - a player's Ride takes the LAV (a squad's worth of passenger seats), never the jeep;
// - the ride carries Alpha 400 m or more down a road, and everyone gets out at the drop.
class ACR_TestRideCarry
{
 static const int CHECKS = 3;
 static ACR_Squad s_Lav;
 static ACR_Squad s_Jeep;
 static vector s_Dest;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iAboardMax;
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
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick > 360) ACR_TestRegress.Abort("ridecarry", string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  if (tick == 4)
  {
   // The AI commander's opening purchase can land before tick 1 switches it off (its first buy
   // waits only for the company kit, #38): a vehicle it bought would beat ours to the Ride.
   array<ACR_Squad> bought = {};
   foreach (ACR_Squad other : manager.m_Squads)
    if (other != alpha && ACR_Transport.CrewedVehicle(other)) bought.Insert(other);
   foreach (ACR_Squad other : bought)
   {
    IEntity vehicle = ACR_Transport.CrewedVehicle(other);
    if (vehicle) SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
    if (other.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(other.m_Group);
   }
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("LAV 1", "", ACR_TestRideLav.LAV, true, "", ACR_Manager.Ground(at + Vector(15, 0, 0)), ACR_Manager.Ground(at + Vector(20, 0, 5)));
   manager.SpawnUnit("Jeep 1", "", ACR_TestDrivelessGun.M1025_M2, true, "", ACR_Manager.Ground(at + Vector(-40, 0, 0)), ACR_Manager.Ground(at + Vector(-45, 0, 5)));
   return;
  }
  if (tick < 14) return;
  if (!s_Lav || !s_Jeep)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() == "LAV 1") s_Lav = squad;
    if (squad.GetName() == "Jeep 1") s_Jeep = squad;
   }
   if (!s_Lav || !s_Jeep) return;
  }
  if (!manager.m_Rides) manager.m_Rides = new ACR_Rides(manager);
  int waited = tick - s_iStepAt;
  int aboard = ACR_TestGetOut.Aboard(alpha);
  if (aboard > s_iAboardMax) s_iAboardMax = aboard;

  // 0: the rule, the AI's refusal, then the player's Ride.
  if (s_iStep == 0)
  {
   IEntity lav = ACR_Transport.CrewedVehicle(s_Lav);
   IEntity jeep = ACR_Transport.CrewedVehicle(s_Jeep);
   if (!lav || !jeep || !s_Lav.HasGunner(lav) || !s_Jeep.HasGunner(jeep))
   {
    if (waited < 20) return;
    ACR_TestRegress.Abort("ridecarry", "the LAV or the jeep has no crew or no gunner");
    return;
   }
   string lavAI = manager.m_Rides.WhyNotTaxi(s_Lav, "US");
   string lavPlayer = manager.m_Rides.WhyNotTaxi(s_Lav, "US", true);
   string jeepPlayer = manager.m_Rides.WhyNotTaxi(s_Jeep, "US", true);
   bool rule = lavAI == "a fighting vehicle (manned gun)" && lavPlayer == "" && jeepPlayer == "a fighting vehicle (manned gun)";
   ACR_TestRegress.Check("ridecarry: a player's Ride may take an APC or IFV, an AI ride still takes no fighting vehicle, nor a player's an armed jeep", rule,
    string.Format("LAV (%1 passenger seats): AI '%2', player '%3'; jeep (%4 passenger seats): player '%5'", ACR_Rides.CargoSeats(lav), lavAI, lavPlayer, ACR_Rides.CargoSeats(jeep), jeepPlayer));

   s_Dest = RoadAway(alpha.GetAnchor().GetOrigin());
   if (s_Dest == vector.Zero)
   {
    ACR_TestRegress.Abort("ridecarry", "no road destination");
    return;
   }
   string aiWhy;
   ACR_Ride aiRide = manager.m_Rides.Request(alpha, s_Dest, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, aiWhy);
   if (aiRide) manager.m_Rides.CancelFor(alpha);
   bool says = !aiRide && aiWhy.Contains("the nearest, LAV 1, ") && aiWhy.Contains("a fighting vehicle (manned gun)");
   ACR_TestRegress.Check("ridecarry: a refused ride names the nearest vehicle and why it can't come", says, aiWhy);

   bool going;
   string said = manager.m_Requests.Ride(alpha, s_Dest, going);
   ACR_Ride ride = manager.m_Rides.RideOf(alpha);
   PrintFormat("[ACR] RIDECARRY player's Ride to %1 (%2 m): %3 ('%4'), taxi %5", s_Dest, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest), going, said, ride && ride.m_Taxi == s_Lav);
   if (!ride || ride.m_Taxi != s_Lav)
   {
    ACR_TestRegress.Check("ridecarry: the player's Ride in the LAV-25 takes Alpha there and everyone gets out", false, "the Ride didn't take the LAV: " + said);
    ACR_TestRegress.Complete("ridecarry", CHECKS);
    s_iStep = 9;
    return;
   }
   Next(tick);
   return;
  }
  // 1: on the way until the ride is over.
  if (s_iStep == 1)
  {
   ACR_Ride on = manager.m_Rides.RideOf(alpha);
   s_sDetail = string.Format("%1 aboard, %2 m from the drop", aboard, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest));
   if (on && waited < 240) return;
   PrintFormat("[ACR] RIDECARRY ride over after %1 s (%2; most aboard %3)", waited, s_sDetail, s_iAboardMax);
   Next(tick);
   return;
  }
  // 2: everyone out within 45 s, near the drop.
  if (s_iStep == 2)
  {
   if (aboard > 0 && waited < 45) return;
   float left = vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest);
   bool carried = s_iAboardMax > 0 && aboard == 0 && left < 200;
   ACR_TestRegress.Check("ridecarry: the player's Ride in the LAV-25 takes Alpha there and everyone gets out", carried,
    string.Format("%1; %2 s on: %3 still aboard (most aboard %4), %5 m from the destination, order '%6'", s_sDetail, waited, aboard, s_iAboardMax, (int)left, alpha.m_Order));
   ACR_TestRegress.Complete("ridecarry", CHECKS);
   Next(tick);
  }
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // A road point 400-700 m from 'from' that the road route reaches without a long detour.
 protected static vector RoadAway(vector from)
 {
  for (int k = 0; k < 48; k++)
  {
   int slot = k % 24;
   int ring = k / 24;
   float angle = Math.PI2 * slot / 24;
   float reach = 450 + ring * 200;
   vector road;
   if (!BBB_Terrain.RoadPointNear(from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach), 120, road)) continue;
   if (vector.DistanceXZ(from, road) < 400) continue;
   array<vector> legs = {};
   int nodes, millis;
   BBB_Roads.Route(from, road, legs, nodes, millis);
   if (BBB_Roads.s_bLastDetour || BBB_Roads.s_fLastLength > vector.DistanceXZ(from, road) * 1.6) continue;
   return road;
  }
  return vector.Zero;
 }
}
