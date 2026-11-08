// -acrTruckParkTest 1 (server, about 2.5 min; Chernarus Minus): where idle service trucks wait
// at a base (ACR_Logistics.ParkSpot; Jacob, 7 Nov: at Zub, the castle on its hill, US trucks were
// re-sent every 15 s for 34 min; 100-200 m east at the foot would have done). First run (7 Nov):
// the road point trucks were already driven to (ACR_Squad.RouteTo) is 214 m east, 47 m below the
// castle, so they got there; only "parked" meant inside the 45 m ring, so they were sent again.
// Zub's park must be off the hilltop and within reach; a flat town (Chernogorsk) keeps a spot in
// its ring. Then a crewed arsenal truck 600 m out drives to Zub's park and must end up parked
// there (within ACR_AICommander.PARK_ARRIVE), where the rule before 7 Nov would have re-sent it.
// A second truck sent to the castle centre the old way is logged for comparison.
class ACR_TestTruckPark
{
 static const ResourceName TRUCK = "{1FF144F8A209239D}Prefabs/Vehicles/Wheeled/M923A1/M923A1_arsenal.et";
 static const vector ZUB = "6542 0 5589";
 static const float ZUB_RADIUS = 45;
 static const vector TOWN = "6717 0 2568";
 static const float TOWN_RADIUS = 80;
 protected static vector s_vPark, s_vOld, s_vStart;
 protected static bool s_bPark;
 protected static ACR_Squad s_New, s_OldWay;
 protected static float s_fNewBest, s_fOldBest;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (second == 1) foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
  if (second == 6) Spots(manager);
  if (second == 14) Send(manager);
  if (second > 14 && second < 150) Watch();
  if (second == 150) Verdict();
 }

 protected static void Spots(ACR_Manager manager)
 {
  if (!GetGame().GetWorldFile().Contains("Chernarus"))
  {
   ACR_TestRegress.Abort("truckpark", "needs Chernarus Minus");
   return;
  }
  BaseWorld world = GetGame().GetWorld();
  float castle = world.GetSurfaceY(ZUB[0], ZUB[2]);
  vector east = "1 0 0";
  vector parkAny;
  bool anySide = ACR_Logistics.ParkSpot(ZUB, ZUB_RADIUS, vector.Zero, parkAny);
  s_bPark = ACR_Logistics.ParkSpot(ZUB, ZUB_RADIUS, east, s_vPark);
  BBB_Terrain.RoadPointNear(ZUB, 250, s_vOld);
  PrintFormat("[ACR] TRUCKPARK Zub castle ground %1 m", castle);
  PrintFormat("[ACR] TRUCKPARK Zub park (rear east) %1: %2 m out, %3 m below the castle, %4 m over the ground round it", s_vPark, vector.DistanceXZ(s_vPark, ZUB), castle - s_vPark[1], ACR_SiteScore.Elevation(s_vPark, 60));
  PrintFormat("[ACR] TRUCKPARK Zub park (no side) %1 (%2): %3 m out, %4 m below the castle", parkAny, anySide, vector.DistanceXZ(parkAny, ZUB), castle - parkAny[1]);
  PrintFormat("[ACR] TRUCKPARK Zub before 7 Nov: road point %1, %2 m out, %3 m below the castle, %4 m over the ground round it", s_vOld, vector.DistanceXZ(s_vOld, ZUB), castle - s_vOld[1], ACR_SiteScore.Elevation(s_vOld, 60));
  ACR_TestRegress.Check("truck park: Zub's is off the castle hill, within reach", s_bPark && vector.DistanceXZ(s_vPark, ZUB) <= ACR_Logistics.PARK_REACH && ACR_SiteScore.Elevation(s_vPark, 60) <= ACR_Logistics.PARK_HILL && castle - s_vPark[1] >= 10, string.Format("%1 m out, %2 m below the castle", vector.DistanceXZ(s_vPark, ZUB), castle - s_vPark[1]));
  vector town;
  bool townFound = ACR_Logistics.ParkSpot(TOWN, TOWN_RADIUS, vector.Zero, town);
  ACR_TestRegress.Check("truck park: a flat town keeps a spot in its ring", townFound && vector.DistanceXZ(town, TOWN) <= TOWN_RADIUS, string.Format("Chernogorsk: %1 m from the centre", vector.DistanceXZ(town, TOWN)));
  // The trucks start on a road 600 m east of the castle.
  if (!BBB_Terrain.RoadPointNear(ZUB + Vector(600, 0, 0), 300, s_vStart)) s_vStart = ACR_Manager.Ground(ZUB + Vector(600, 0, 0));
  manager.SpawnUnit("Park Truck", "", TRUCK, true, "", s_vStart, ACR_Manager.Ground(s_vStart + Vector(5, 0, 5)));
  vector besideStart = ACR_Manager.Ground(s_vStart + Vector(0, 0, 30));
  manager.SpawnUnit("Old Way Truck", "", TRUCK, true, "", besideStart, ACR_Manager.Ground(besideStart + Vector(5, 0, 5)));
  s_fNewBest = float.MAX;
  s_fOldBest = float.MAX;
 }

 protected static void Send(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetName() == "Park Truck") s_New = squad;
   if (squad.GetName() == "Old Way Truck") s_OldWay = squad;
  }
  if (!s_New || !s_OldWay || !ACR_Transport.CrewedVehicle(s_New) || !ACR_Transport.CrewedVehicle(s_OldWay))
  {
   ACR_TestRegress.Abort("truckpark", "the trucks didn't spawn crewed");
   return;
  }
  PrintFormat("[ACR] TRUCKPARK start %1, %2 m from the park", s_vStart, vector.DistanceXZ(s_vStart, s_vPark));
  s_New.RouteTo(s_vPark, ACR_Squad.WP_MOVE, -1, "Moving", 0);
  s_OldWay.RouteTo(ACR_Manager.Ground(ZUB), ACR_Squad.WP_MOVE, -1, "Moving", 0);
 }

 protected static void Watch()
 {
  if (s_New && ACR_Transport.CrewedVehicle(s_New)) s_fNewBest = Math.Min(s_fNewBest, vector.DistanceXZ(ACR_Transport.CrewedVehicle(s_New).GetOrigin(), s_vPark));
  if (s_OldWay && ACR_Transport.CrewedVehicle(s_OldWay)) s_fOldBest = Math.Min(s_fOldBest, vector.DistanceXZ(ACR_Transport.CrewedVehicle(s_OldWay).GetOrigin(), ZUB));
 }

 protected static void Verdict()
 {
  string oldEnd = "gone";
  if (s_OldWay && ACR_Transport.CrewedVehicle(s_OldWay)) oldEnd = string.Format("%1, %2 m from the castle centre", ACR_Transport.CrewedVehicle(s_OldWay).GetOrigin(), vector.DistanceXZ(ACR_Transport.CrewedVehicle(s_OldWay).GetOrigin(), ZUB));
  PrintFormat("[ACR] TRUCKPARK the way before 7 Nov: closest %1 m to the castle centre (ring %2 m); ended at %3", s_fOldBest, ZUB_RADIUS, oldEnd);
  float endPark = float.MAX;
  float endCentre = float.MAX;
  if (s_New && ACR_Transport.CrewedVehicle(s_New))
  {
   endPark = vector.DistanceXZ(ACR_Transport.CrewedVehicle(s_New).GetOrigin(), s_vPark);
   endCentre = vector.DistanceXZ(ACR_Transport.CrewedVehicle(s_New).GetOrigin(), ZUB);
  }
  ACR_TestRegress.Check("truck park: a truck drives to Zub's park and counts as parked", endPark < ACR_AICommander.PARK_ARRIVE, string.Format("ended %1 m from the park (closest %2 m), %3 m from the castle centre: the rule before 7 Nov (inside the %4 m ring) would re-send it", endPark, s_fNewBest, endCentre, ZUB_RADIUS));
  ACR_TestRegress.Complete("truckpark", 3);
 }
}
