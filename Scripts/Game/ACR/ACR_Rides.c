// Rides: an idle transport (a crewed ground vehicle with no gun of its own, like
// a truck) picks a squad up where it stands, drives it to a drop-off point and
// lets it carry on from there on foot, then heads back to a safe base.
// Used by the AI commander for long trips and by players with the "Ride" order.
// A transport is only fetched from nearby: never more than MAX_PICKUP metres
// away, and never farther than half the trip itself.
enum ACR_ERideState
{
 TO_PICKUP,
 BOARDING,
 TO_DROP
}

class ACR_Ride
{
 ACR_Squad m_Rider;
 ACR_Squad m_Taxi;
 Vehicle m_Vehicle;
 string m_sFaction;
 vector m_Drop;
 // what the riders do once dropped (no prefab: take positions there)
 ResourceName m_sFinalPrefab;
 vector m_Final;
 float m_fFinalRadius;
 string m_sFinalOrder;
 int m_iFinalKind;
 int m_State;
 float m_fStateSince;
 float m_fDriveBudgetMs;
 int m_iRiderSerial;
 int m_iTaxiSerial;
 float m_fTaxiLostSince = -1;   // crew briefly out (seat swaps, a stumble): only give up after a while
}

class ACR_Rides : Managed
{
 static const float WASTED_SEAT_M = 30;        // m: penalty per empty seat when picking a transport
 static const float MAX_PICKUP = 800;          // m: farthest a transport is fetched from
 static const float PICKUP_SHARE = 0.5;        // ... and no farther than this share of the trip
 static const float MIN_TRIP = 400;            // m: shorter than this, just walk
 static const float PICKUP_REACHED = 35;       // m from the squad to stop and let them board
 static const float DROP_REACHED = 60;         // m from the drop-off point
 static const float PICKUP_TIMEOUT_MS = 240000;
 static const float BOARDING_TIMEOUT_MS = 75000;
 static const float STRAGGLER_WAIT_MS = 20000;  // then leave without riders this far away
 static const float STRAGGLER_DISTANCE = 60;
 // Leave once this share is aboard and the rest have had this long (1 Oct MP: two rides
 // gave up after 75 s with 7 and 8 of 9 aboard, the last ones stuck at the truck).
 static const float MOSTLY_ABOARD_SHARE = 0.75;
 static const float STUCK_BOARDING_MS = 20000; // then place the stragglers within 40 m straight in
 static const float MOSTLY_ABOARD_WAIT_MS = 30000;

 protected ACR_Manager m_Manager;
 ref array<ref ACR_Ride> m_Rides = {};
 ref map<string, float> m_NoTransportAt = new map<string, float>(); // faction -> last time a ride found no transport

 void ACR_Rides(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 // ---------------------------------------------------------------- queries

 static string FactionOf(ACR_Squad squad)
 {
  if (!squad || !squad.GetFaction()) return "";
  return squad.GetFaction().GetFactionKey();
 }

 static int FreeSeats(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int free;
  foreach (BaseCompartmentSlot seat : seats)
   if (!seat.GetOccupant()) free++;
  return free;
 }

 ACR_Ride RideOf(ACR_Squad squad)
 {
  foreach (ACR_Ride ride : m_Rides)
   if (ride.m_Rider == squad || ride.m_Taxi == squad) return ride;
  return null;
 }

 // A squad that can serve as a taxi: crews a drivable ground vehicle with no
 // manned gun, AI-driven, not busy with anything but holding or heading home.
 bool IsFreeTransport(ACR_Squad squad, string faction)
 {
  if (FactionOf(squad) != faction || squad.HasPlayer() || RideOf(squad)) return false;
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (!vehicle || squad.HasGunner(vehicle)) return false;
  if (ACR_Logistics.ServiceOf(vehicle) != 0) return false; // ammo/repair/fuel trucks have their own job
  if (!CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent))) return false; // ground vehicles only
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver || !ACR_Respawn.IsAlive(driver) || ACR_Squad.FindController(driver) != 0) return false;
  if (m_Manager.IsGunnerDriven(vehicle)) return false;
  string order = squad.m_Order;
  if (order == "Holding" || order == "Holding position" || order == "Defending" || order == "Returning") return true;
  return squad.m_Group != null && squad.m_Group.GetCurrentWaypoint() == null;
 }

 // Nearest free transport with room for everyone, within the fetch limits.
 ACR_Squad FindTransport(string faction, vector pickup, float trip, int seatsNeeded, out float distance)
 {
  float limit = Math.Min(MAX_PICKUP, trip * PICKUP_SHARE);
  ACR_Squad best;
  distance = float.MAX;
  float bestScore = float.MAX;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!IsFreeTransport(squad, faction)) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   float d = vector.DistanceXZ(vehicle.GetOrigin(), pickup);
   if (d > limit) continue;
   int seats = FreeSeats(vehicle);
   if (seats < seatsNeeded) continue;
   // Right-sized first: every empty seat counts as WASTED_SEAT_M metres farther,
   // so two men take the jeep next door rather than a truck just as close.
   float score = d + (seats - seatsNeeded) * WASTED_SEAT_M;
   if (score >= bestScore) continue;
   bestScore = score;
   distance = d;
   best = squad;
  }
  return best;
 }

 // Recently needed a transport and found none (the AI commander buys one).
 bool ShortOfTransport(string faction)
 {
  if (!m_NoTransportAt.Contains(faction)) return false;
  return GetGame().GetWorld().GetWorldTime() - m_NoTransportAt.Get(faction) < 180000;
 }

 int CountTransports(string faction)
 {
  int count;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (FactionOf(squad) != faction) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (vehicle && !squad.HasGunner(vehicle) && FreeSeats(vehicle) >= 4) count++;
  }
  return count;
 }

 // ---------------------------------------------------------------- requests

 // Starts a ride, or returns null with the reason in why.
 // dropShort: how far short of the destination to get out.
 ACR_Ride Request(ACR_Squad rider, vector destination, float dropShort, ResourceName finalPrefab, float finalRadius, string finalOrder, int finalKind, out string why)
 {
  why = "";
  IEntity anchor = rider.GetAnchor();
  string faction = FactionOf(rider);
  if (!anchor || faction.IsEmpty()) { why = "not ready"; return null; }
  if (rider.IsMounted()) { why = "already mounted"; return null; }
  CancelFor(rider);
  vector from = anchor.GetOrigin();
  float trip = vector.DistanceXZ(from, destination);
  if (trip < MIN_TRIP) { why = string.Format("too close to need a ride (under %1 m)", (int)MIN_TRIP); return null; }
  int seats = rider.CountAlive();
  float fetch;
  ACR_Squad taxi = FindTransport(faction, from, trip, seats, fetch);
  if (!taxi)
  {
   m_NoTransportAt.Set(faction, GetGame().GetWorld().GetWorldTime());
   why = string.Format("no free transport with %1 seats within %2 m", seats, (int)Math.Min(MAX_PICKUP, trip * PICKUP_SHARE));
   return null;
  }
  // Only worth it if driving beats walking. Where the roads (which the game's own
  // drivers follow too) go a long way round, a truck took 3 minutes and 2 km for a
  // 600 m hop: then they walk.
  array<vector> roadLegs = {};
  int roadNodes, roadMillis;
  ACR_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis);
  if (ACR_Roads.s_bLastDetour)
  {
   float driveSeconds = fetch / 4 + 45 + ACR_Roads.s_fLastLength / 4; // trucks average about 4 m/s door to door
   float walkSeconds = trip / 1.4;
   if (driveSeconds > walkSeconds * 0.8)
   {
    why = string.Format("no quick road there (%1 m by road for %2 m): walking is faster", (int)ACR_Roads.s_fLastLength, (int)trip);
    return null;
   }
  }
  ACR_Ride ride = new ACR_Ride();
  ride.m_Rider = rider;
  ride.m_Taxi = taxi;
  ride.m_Vehicle = Vehicle.Cast(ACR_Transport.CrewedVehicle(taxi));
  ride.m_sFaction = faction;
  vector dir = destination - from;
  dir[1] = 0;
  dir.Normalize();
  ride.m_Drop = ACR_Manager.Ground(destination - dir * Math.Min(dropShort, trip * 0.5));
  // Pick up and drop off on roads when there is one nearby: the game's AI
  // drivers often can't reach off-road spots and abandon the vehicle.
  vector road;
  if (ACR_Terrain.RoadPointNear(ride.m_Drop, 350, road)) ride.m_Drop = road;
  vector pickup = from;
  if (ACR_Terrain.RoadPointNear(from, 250, road)) pickup = road;
  ride.m_sFinalPrefab = finalPrefab;
  ride.m_Final = destination;
  ride.m_fFinalRadius = finalRadius;
  ride.m_sFinalOrder = finalOrder;
  ride.m_iFinalKind = finalKind;
  ride.m_fDriveBudgetMs = (trip / 4 + 150) * 1000; // generous: 4 m/s average plus slack
  if (pickup == from) rider.Halt("Waiting for ride");
  else
  {
   rider.MoveTo(pickup);
   rider.m_Order = "Waiting for ride";
  }
  taxi.SetMode(ACR_EMoveMode.FAST);
  taxi.RouteTo(pickup, ACR_Squad.WP_MOVE, -1, "Picking up " + rider.GetName(), 0);
  ride.m_iRiderSerial = rider.m_iOrderSerial;
  ride.m_iTaxiSerial = taxi.m_iOrderSerial;
  SetState(ride, ACR_ERideState.TO_PICKUP);
  m_Rides.Insert(ride);
  PrintFormat("[ACR] RIDE %1 -> %2: %3 fetched from %4 m, trip %5 m, pickup %6 drop %7", rider.GetName(), ACR_Manager.Grid(destination), taxi.GetName(), (int)fetch, (int)trip, pickup, ride.m_Drop);
  return ride;
 }

 // A new order for either squad ends its ride (riders already aboard get out).
 void CancelFor(ACR_Squad squad)
 {
  for (int i = m_Rides.Count() - 1; i >= 0; i--)
  {
   ACR_Ride ride = m_Rides[i];
   if (ride.m_Rider != squad && ride.m_Taxi != squad) continue;
   m_Rides.RemoveOrdered(i);
   if (ride.m_Taxi && ride.m_Taxi != squad) SendHome(ride);
  }
 }

 // ---------------------------------------------------------------- per second

 void Tick()
 {
  for (int i = m_Rides.Count() - 1; i >= 0; i--)
  {
   ACR_Ride ride = m_Rides[i];
   if (!Step(ride)) m_Rides.RemoveOrdered(i);
  }
 }

 protected void SetState(ACR_Ride ride, int state)
 {
  ride.m_State = state;
  ride.m_fStateSince = GetGame().GetWorld().GetWorldTime();
 }

 protected int Aboard(ACR_Ride ride, out int alive)
 {
  alive = 0;
  int aboard;
  array<AIAgent> agents = {};
  ride.m_Rider.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ride.m_Vehicle && ACR_Transport.VehicleOf(body) == ride.m_Vehicle) aboard++;
  }
  return aboard;
 }

 // Riders not aboard, none of them anywhere near the transport (stuck at a spawn,
 // wandered off): the ride shouldn't wait for them.
 protected int Stragglers(ACR_Ride ride)
 {
  int distant;
  array<AIAgent> agents = {};
  ride.m_Rider.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body) == ride.m_Vehicle) continue;
   if (vector.DistanceXZ(body.GetOrigin(), ride.m_Vehicle.GetOrigin()) <= STRAGGLER_DISTANCE) return -1;
   distant++;
  }
  return distant;
 }

 // Returns false when the ride is over.
 protected bool Step(ACR_Ride ride)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  bool riderOk = ride.m_Rider != null && m_Manager.m_Squads.Contains(ride.m_Rider) && ride.m_Rider.IsAlive() && !ride.m_Rider.HasPlayer();
  ChimeraCharacter driver = ACR_Squad.DriverOf(ride.m_Vehicle);
  bool taxiOk = ride.m_Taxi != null && ride.m_Vehicle != null && driver != null && m_Manager.m_Squads.Contains(ride.m_Taxi);
  if (taxiOk) taxiOk = ACR_Respawn.IsAlive(driver) && ACR_Transport.CrewedVehicle(ride.m_Taxi) == ride.m_Vehicle && !ride.m_Taxi.HasPlayer();
  if (!riderOk)
  {
   ReleaseTaxi(ride);
   return false;
  }
  int alive;
  int aboard = Aboard(ride, alive);
  if (taxiOk) ride.m_fTaxiLostSince = -1;
  else if (ride.m_Vehicle && ACR_Logistics.Health(ride.m_Vehicle) > 0 && ride.m_Taxi && m_Manager.m_Squads.Contains(ride.m_Taxi))
  {
   if (ride.m_fTaxiLostSince < 0) ride.m_fTaxiLostSince = now;
   // (45 s: the game's AI driver may hop out to open a gate the gate helper missed)
   float patience = 15000;
   if (ride.m_Taxi.IsAlive() && !ride.m_Taxi.HasPlayer()) patience = 45000;
   if (now - ride.m_fTaxiLostSince < patience) return true;
  }
  if (!taxiOk)
  {
   Log(ride, string.Format("transport lost (driver %1 alive %2, crewing %3), continuing on foot", driver != null, (driver != null && ACR_Respawn.IsAlive(driver)), (ride.m_Taxi != null && ACR_Transport.CrewedVehicle(ride.m_Taxi) == ride.m_Vehicle)));
   // The crew still has our pickup/drop route: stop it, or the vehicle would
   // carry on empty once its driver gets back in.
   ReleaseTaxi(ride);
   Finish(ride, aboard > 0);
   return false;
  }
  // Someone else gave either squad a new order: the ride is off.
  if (ride.m_Rider.m_iOrderSerial != ride.m_iRiderSerial || ride.m_Taxi.m_iOrderSerial != ride.m_iTaxiSerial)
  {
   // (the transport crew bailing out also lands here, with the riders already told to get out)
   bool riderReordered = ride.m_Rider.m_iOrderSerial != ride.m_iRiderSerial && ride.m_Rider.m_Order != "Dismounting";
   if (riderReordered) Log(ride, "cancelled: the riders got a new order");
   else Log(ride, "cancelled: the transport crew was given other orders or bailed out");
   if (ride.m_Taxi.m_iOrderSerial == ride.m_iTaxiSerial) SendHome(ride);
   // Riders thrown out on the way (the transport got stuck or hit) still go on to their objective.
   ACR_Squad rider = ride.m_Rider;
   if (ride.m_State == ACR_ERideState.TO_DROP && rider.m_Order == "Dismounting")
    rider.ContinueAfterDismount(ride.m_sFinalPrefab, ride.m_Final, ride.m_fFinalRadius, ride.m_sFinalOrder, ride.m_iFinalKind);
   // Riders given a new order while aboard: out first, then carry it out on foot.
   else if (aboard > 0 && riderReordered && rider.m_sOrderPrefab != ACR_Squad.WP_GET_OUT)
    rider.DismountAndContinue(rider.m_sOrderPrefab, rider.m_MoveTarget, rider.m_fOrderRadius, rider.m_Order, rider.m_OrderKind);
   // The transport let them down, not a new order: carry on to the objective on foot
   // instead of standing around where the ride ended.
   else if (!riderReordered)
    Finish(ride, aboard > 0);
   return false;
  }
  float inState = now - ride.m_fStateSince;
  IEntity riderAnchor = ride.m_Rider.GetAnchor();
  switch (ride.m_State)
  {
   case ACR_ERideState.TO_PICKUP:
   {
    if (riderAnchor && vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), riderAnchor.GetOrigin()) < PICKUP_REACHED)
    {
     ride.m_Taxi.Halt("Loading " + ride.m_Rider.GetName());
     ride.m_Rider.MoveTo(ride.m_Vehicle.GetOrigin());
     ride.m_Rider.m_Order = "Boarding";
     ride.m_iRiderSerial = ride.m_Rider.m_iOrderSerial;
     ride.m_iTaxiSerial = ride.m_Taxi.m_iOrderSerial;
     SetState(ride, ACR_ERideState.BOARDING);
     Log(ride, "transport arrived, boarding");
     return true;
    }
    if (inState > PICKUP_TIMEOUT_MS)
    {
     Log(ride, "transport never arrived, walking");
     SendHome(ride);
     Finish(ride, false);
     return false;
    }
    return true;
   }
   case ACR_ERideState.BOARDING:
   {
    if (alive > 0 && aboard < alive)
     ride.m_Rider.BoardCargo(ride.m_Vehicle, 40, inState > 8000, inState > STUCK_BOARDING_MS);
    int leftBehind = 0;
    if (alive > 0 && aboard > 0 && aboard < alive && inState > STRAGGLER_WAIT_MS) leftBehind = Stragglers(ride);
    bool mostly = alive > 0 && aboard < alive && inState > MOSTLY_ABOARD_WAIT_MS && MostlyAboard(aboard, alive);
    if (alive > 0 && (aboard >= alive || leftBehind > 0 || mostly))
    {
     if (leftBehind > 0) Log(ride, string.Format("leaving without %1 rider(s) more than %2 m away", leftBehind, STRAGGLER_DISTANCE));
     else if (mostly) Log(ride, string.Format("leaving with %1/%2 aboard", aboard, alive));
     ride.m_Rider.Halt("Riding");
     ride.m_iRiderSerial = ride.m_Rider.m_iOrderSerial;
     ride.m_Taxi.RouteTo(ride.m_Drop, ACR_Squad.WP_MOVE, -1, "Driving " + ride.m_Rider.GetName(), 0);
     ride.m_Rider.m_Order = "Riding";
     ride.m_iTaxiSerial = ride.m_Taxi.m_iOrderSerial;
     SetState(ride, ACR_ERideState.TO_DROP);
     Log(ride, "aboard, on the way");
     return true;
    }
    if (inState > BOARDING_TIMEOUT_MS)
    {
     Log(ride, string.Format("boarding took too long (%1/%2 aboard), walking", aboard, alive));
     SendHome(ride);
     Finish(ride, aboard > 0);
     return false;
    }
    return true;
   }
   case ACR_ERideState.TO_DROP:
   {
    if (aboard == 0)
    {
     Log(ride, "riders are out early, continuing on foot");
     SendHome(ride);
     Finish(ride, false);
     return false;
    }
    bool arrived = vector.DistanceXZ(ride.m_Vehicle.GetOrigin(), ride.m_Drop) < DROP_REACHED;
    if (arrived || inState > ride.m_fDriveBudgetMs)
    {
     if (arrived) Log(ride, "dropped off");
     else Log(ride, "drive took too long, getting out here");
     Finish(ride, true);
     SendHome(ride);
     return false;
    }
    return true;
   }
  }
  return false;
 }

 // Riders carry out their final order (getting out first if aboard).
 protected void Finish(ACR_Ride ride, bool aboard)
 {
  ACR_Squad rider = ride.m_Rider;
  if (aboard)
  {
   rider.DismountAndContinue(ride.m_sFinalPrefab, ride.m_Final, ride.m_fFinalRadius, ride.m_sFinalOrder, ride.m_iFinalKind);
   string faction = FactionOf(rider);
   if (!faction.IsEmpty()) m_Manager.NotifyFaction(faction, rider.GetName() + ": dropped off at " + ACR_Manager.Grid(ride.m_Vehicle.GetOrigin()));
   return;
  }
  if (ride.m_sFinalPrefab.IsEmpty())
  {
   IEntity anchor = rider.GetAnchor();
   if (anchor) rider.Defend(anchor.GetOrigin());
   return;
  }
  if (ride.m_sFinalPrefab == ACR_Squad.WP_DEFEND) rider.Defend(ride.m_Final);
  else if (ride.m_sFinalPrefab == ACR_Squad.WP_SEARCH_DESTROY) rider.Assault(ride.m_Final);
  else rider.MoveTo(ride.m_Final);
 }

 // Ride over: if the transport is still following this ride's orders, send it
 // home (or hold, when its crew is out of the vehicle).
 protected void ReleaseTaxi(ACR_Ride ride)
 {
  ACR_Squad taxi = ride.m_Taxi;
  if (!taxi || !m_Manager.m_Squads.Contains(taxi) || taxi.m_iOrderSerial != ride.m_iTaxiSerial) return;
  if (ride.m_Vehicle && ACR_Transport.CrewedVehicle(taxi) == ride.m_Vehicle) SendHome(ride);
  else taxi.Halt("Holding");
 }

 // Empty transport back to the nearest safe base of its side (or where it is).
 protected void SendHome(ACR_Ride ride)
 {
  ACR_Squad taxi = ride.m_Taxi;
  if (!taxi || !ride.m_Vehicle || !m_Manager.m_Squads.Contains(taxi)) return;
  vector here = ride.m_Vehicle.GetOrigin();
  ACR_Base home;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != ride.m_sFaction || base.m_bContested) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), here);
   if (d < best) { best = d; home = base; }
  }
  if (!home || best < 80)
  {
   taxi.Halt("Holding");
   return;
  }
  taxi.MoveTo(ACR_Manager.Ground(home.GetOrigin()));
  taxi.m_Order = "Returning";
 }

 static bool MostlyAboard(int aboard, int alive)
 {
  // Compare as floats: with an int on the left, Enforce did the sum and the comparison in ints.
  float share = aboard;
  share /= alive;
  return alive > 0 && share >= MOSTLY_ABOARD_SHARE;
 }

 // (Either squad may already be gone: the taxi's crew can die with the ride under way.)
 protected void Log(ACR_Ride ride, string what)
 {
  string rider = "?", taxi = "?";
  if (ride.m_Rider) rider = ride.m_Rider.GetName();
  if (ride.m_Taxi) taxi = ride.m_Taxi.GetName();
  PrintFormat("[ACR] RIDE %1 (%2): %3", rider, taxi, what);
 }
}
