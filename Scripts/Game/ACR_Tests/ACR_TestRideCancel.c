// -acrRideCancelTest 1 (server-capable, ~4 min): a ride called off mid-trip (Jacob 6 Nov live,
// Chernarus: Naval Infantry Fire Team 1's UAZ, low on fuel 1.2 km short of Soapograd, was sent back
// to the fuel truck; the team got out by the roadblock and walked west, away from its waypoint east).
// A truck carries Alpha 700-1100 m down a road, a fuel truck parked where it set off:
// - on the way, the taxi runs low on fuel and the AI commander's logistics run: it finishes the
//   ride first (not sent back to the fuel truck with Alpha aboard);
// - then the taxi is sent back anyway (as a player's order or a bail-out would): Alpha is out within
//   30 s, the truck isn't left among its group's vehicles (the game's AI walks back to a vehicle of
//   its own before a long move), and a minute on foot later it is nearer its destination, not the truck.
class ACR_TestRideCancel
{
 static const int CHECKS = 4;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName TANKER = "{2BE1F8B9299B67C1}Prefabs/Vehicles/Wheeled/M923A1/M923A1_tanker.et";
 static ref ACR_AICommander s_AI;
 static ACR_Squad s_Taxi;
 static IEntity s_Tanker;
 static vector s_Dest;
 static float s_fDestAtOut;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iOutAt = -1;
 static int s_iRP = -1;
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
  if (tick > 320) Fail(manager, string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  if (s_iStep >= 9) return;
  vector at = alpha.GetAnchor().GetOrigin();
  if (tick == 4)
  {
   manager.SpawnUnit("Taxi 1", "", TRUCK, true, "", ACR_Manager.Ground(at + Vector(25, 0, 15)), ACR_Manager.Ground(at + Vector(30, 0, 20)));
   manager.SpawnUnit("Fuel Truck", "", TANKER, true, "", ACR_Manager.Ground(at + Vector(-30, 0, -20)), ACR_Manager.Ground(at + Vector(-34, 0, -24)));
   return;
  }
  if (tick < 12) return;
  if (!manager.m_Rides) manager.m_Rides = new ACR_Rides(manager);
  if (!s_Taxi || !s_Tanker)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() == "Taxi 1") s_Taxi = squad;
    if (squad.GetName() == "Fuel Truck") s_Tanker = ACR_Transport.CrewedVehicle(squad);
   }
   if ((!s_Taxi || !s_Tanker) && tick > 40) Fail(manager, "the taxi or the fuel truck never turned up");
   return;
  }
  int waited = tick - s_iStepAt;
  ACR_Ride ride = manager.m_Rides.RideOf(alpha);
  int aboard = ACR_TestGetOut.Aboard(alpha);

  // 0: the fuel truck known as a station; the ride.
  if (s_iStep == 0)
  {
   array<IEntity> stations = {};
   if (manager.m_Logistics) manager.m_Logistics.Stations("US", ACR_EService.FUEL, stations);
   s_sDetail = string.Format("%1 fuel stations", stations.Count());
   if (stations.IsEmpty())
   {
    if (waited > 40) Fail(manager, "the fuel truck never counted as a fuel station");
    return;
   }
   s_Dest = Destination(at);
   if (s_Dest == vector.Zero)
   {
    Fail(manager, "no road destination 700-1100 m off");
    return;
   }
   ACR_FactionData data = manager.m_Bases.GetFaction("US");
   s_iRP = data.m_iRP;
   data.m_iRP = 0;
   s_AI = new ACR_AICommander(manager, "US");
   alpha.Halt("Holding");
   string why;
   ACR_Ride asked = manager.m_Rides.Request(alpha, s_Dest, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   PrintFormat("[ACR] RIDECANCEL ride requested %1 (%2) to %3, %4 m", asked != null, why, s_Dest, (int)vector.DistanceXZ(at, s_Dest));
   if (!asked || asked.m_Taxi != s_Taxi)
   {
    Fail(manager, string.Format("no ride in Taxi 1 (%1)", why));
    return;
   }
   Next(tick);
   return;
  }
  // 1: under way: low on fuel, the logistics run leaves the taxi on its ride.
  if (s_iStep == 1)
  {
   s_sDetail = string.Format("%1 aboard", aboard);
   if (!ride || ride.m_State != ACR_ERideState.TO_DROP || aboard == 0)
   {
    if (!ride || waited > 150) Fail(manager, "the ride never got under way: " + s_sDetail);
    return;
   }
   if (vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), s_Tanker.GetOrigin()) < 120 && waited < 120) return;
   SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(ride.m_Vehicle.FindComponent(SCR_FuelManagerComponent));
   if (fuel) fuel.SetTotalFuelPercentage(0.1);
   s_Taxi.SetAITask(ACR_EAITask.FREE);
   int serial = s_Taxi.m_iOrderSerial;
   s_AI.ManageLogisticsForTest();
   ACR_Ride after = manager.m_Rides.RideOf(alpha);
   bool kept = after == ride && s_Taxi.m_iOrderSerial == serial;
   ACR_TestRegress.Check("ridecancel: a taxi low on fuel mid-ride finishes the ride (not sent back to the fuel truck with its riders aboard)", kept,
    string.Format("%1; fuel %2, ride on %3, taxi order '%4' (serial %5 -> %6)", s_sDetail, ACR_Logistics.Fuel(ride.m_Vehicle), after == ride, s_Taxi.m_Order, serial, s_Taxi.m_iOrderSerial));
   if (fuel) fuel.SetTotalFuelPercentage(1);
   // Sent back anyway, as the live match had it (the AI commander's Travel to the fuel truck).
   s_AI.TravelForTest(s_Taxi, s_Tanker.GetOrigin(), false);
   PrintFormat("[ACR] RIDECANCEL taxi sent back to the fuel truck at %1 (%2 m from the destination, %3 aboard)", ACR_Manager.Grid(ride.m_Vehicle.GetOrigin()), (int)vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), s_Dest), aboard);
   Next(tick);
   return;
  }
  // 2: out within 30 s, the truck not one of the group's vehicles any more.
  if (s_iStep == 2)
  {
   s_sDetail = string.Format("%1 aboard, ride %2, order '%3'", aboard, ride != null, alpha.m_Order);
   if (aboard > 0 && waited < 30) return;
   ACR_TestRegress.Check("ridecancel: a ride called off on the way, the riders are out within 30 s while the truck drives on", aboard == 0,
    string.Format("%1 after %2 s", s_sDetail, waited));
   bool usable = StillUsable(alpha, ACR_Transport.CrewedVehicle(s_Taxi));
   ACR_TestRegress.Check("ridecancel: the called-off truck isn't left among the riders' group vehicles (they'd go back to it)", !usable,
    string.Format("%1; the truck usable for Alpha's group %2", s_sDetail, usable));
   s_fDestAtOut = vector.DistanceXZ(at, s_Dest);
   s_iOutAt = tick;
   Next(tick);
   return;
  }
  // 3: a minute on foot later: nearer the destination.
  if (s_iStep == 3)
  {
   IEntity truck = ACR_Transport.CrewedVehicle(s_Taxi);
   float toTruck = -1;
   if (truck) toTruck = vector.DistanceXZ(at, truck.GetOrigin());
   float toDest = vector.DistanceXZ(at, s_Dest);
   if (waited % 5 == 0)
    PrintFormat("[ACR] RIDECANCEL +%1 s: Alpha at %2, %3 m from the destination (%4 when out), %5 m from the truck, order '%6', %7 aboard, truck usable %8", waited, at, (int)toDest, (int)s_fDestAtOut, (int)toTruck, alpha.m_Order, aboard, StillUsable(alpha, truck));
   if (waited < 60) return;
   bool onward = toDest < s_fDestAtOut - 40 && aboard == 0;
   ACR_TestRegress.Check("ridecancel: a minute after getting out, the riders are nearer their destination (walking on to it, not after the truck)", onward,
    string.Format("%1 m from the destination now, %2 when out; %3 m from the truck; order '%4'", (int)toDest, (int)s_fDestAtOut, (int)toTruck, alpha.m_Order));
   ACR_TestRegress.Complete("ridecancel", CHECKS);
   Finish(manager);
  }
 }

 // The vehicle still on the group's list of its own vehicles (SCR_AIGroupUtilityComponent).
 protected static bool StillUsable(ACR_Squad squad, IEntity vehicle)
 {
  if (!squad || !squad.m_Group || !vehicle) return false;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
  return utility && usage && utility.IsUsableVehicle(usage);
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 protected static void Fail(ACR_Manager manager, string why)
 {
  if (s_iStep >= 9) return;
  ACR_TestRegress.Abort("ridecancel", why);
  Finish(manager);
 }

 protected static void Finish(ACR_Manager manager)
 {
  s_iStep = 9;
  if (s_iRP >= 0) manager.m_Bases.GetFaction("US").m_iRP = s_iRP;
 }

 // A road point 700-1100 m out that the road route reaches without a long detour.
 protected static vector Destination(vector from)
 {
  for (int k = 0; k < 48; k++)
  {
   int slot = k % 24;
   int ring = k / 24; // int-math ok (rings)
   float angle = Math.PI2 * slot / 24;
   float reach = 750 + ring * 250;
   vector road;
   if (!BBB_Terrain.RoadPointNear(from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach), 120, road)) continue;
   float d = vector.DistanceXZ(from, road);
   if (d < 700 || d > 1100) continue;
   array<vector> legs = {};
   int nodes, millis;
   BBB_Roads.Route(from, road, legs, nodes, millis);
   if (BBB_Roads.s_bLastDetour || BBB_Roads.s_fLastLength > d * 1.6) continue;
   return road;
  }
  return vector.Zero;
 }
}
