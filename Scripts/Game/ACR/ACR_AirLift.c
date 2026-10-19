// Air lift support (board aca-f22, 14 Oct). Jacob: an off-map transport helicopter, on every map
// and generated layout, for players and (later) the AI commander, which pays half (it can't fly
// bought helicopters). The commander selects a squad and right-clicks where it should land:
// one or two helicopters (enough cargo seats) fly in, land on clear ground near the squad, the
// squad boards, they fly to a clear spot near the clicked point, the squad gets out and holds
// there, and the helicopters leave the way they came.
//
// Stages: INBOUND (spool up off-map, fly to the pickup spots, squad walks there) -> BOARDING
// (seat whoever reaches a helicopter; stragglers are placed in after FORCE_BOARD_MS, the lift
// leaves with whoever is aboard after BOARD_WAIT_MS) -> TRANSIT -> UNLOADING (Get out; anyone
// still aboard after UNLOAD_WAIT_MS is put out) -> LEAVING. While a squad has an air lift, map
// orders to it are refused and the AI commander leaves it alone (ACR_AirLift.Busy).

// Open, level ground for a helicopter: a swept obstacle volume covering the rotor disk
// and tail, plus a survey of the ground beneath the whole footprint.
class ACR_LandingZone
{
 static const float CLEAR_RADIUS = 18;
 static const float MAX_RISE = 2.5;   // m between the highest and lowest ground in the footprint
 static const float SPACING = 40;     // m between two helicopters' spots
 static const float RING_STEP = 20;
 static int s_iChecks;                // spots examined (benchmark)
 static int s_iRejTree, s_iRejRay, s_iRejGround, s_iRejBox; // benchmark: where spots were refused

 // Up to count spots, nearest the centre first. Returns how many were found.
 // Searches that found nothing, remembered for a while (Jacob 18 Oct: a failed search over flat
 // forest took up to 4.7 s and the AI commander repeated it for every long move).
 static ref map<string, float> s_Failed = new map<string, float>();
 static const float FAILED_MEMORY_MS = 120000;

 static const int SEARCH_BUDGET_MS = 120; // a search gives up after this (it runs inside one server tick)

 static int Find(vector center, float maxRadius, int count, notnull array<vector> spots)
 {
  spots.Clear();
  int started = System.GetTickCount();
  float now = GetGame().GetWorld().GetWorldTime();
  string key = string.Format("%1:%2:%3:%4", (int)(center[0] / 10), (int)(center[2] / 10), (int)maxRadius, count);
  float failedAt;
  if (s_Failed.Find(key, failedAt) && now - failedAt < FAILED_MEMORY_MS) return 0;
  for (float ring = 0; ring <= maxRadius && spots.Count() < count && System.GetTickCount() - started <= SEARCH_BUDGET_MS; ring += RING_STEP)
  {
   int steps = 1;
   if (ring > 0)
   {
    float around = ring * Math.PI2 / 25.0;
    steps = Math.Max(8, (int)around);
   }
   for (int i = 0; i < steps && spots.Count() < count; i++)
   {
    float angle = Math.PI2 * i / steps;
    vector spot = Vector(center[0] + Math.Cos(angle) * ring, 0, center[2] + Math.Sin(angle) * ring);
    bool apart = true;
    foreach (vector taken : spots) if (vector.DistanceXZ(taken, spot) < SPACING) apart = false;
    if (System.GetTickCount() - started > SEARCH_BUDGET_MS) break;
    if (!apart || !Clear(spot)) continue;
    spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
    spots.Insert(spot);
   }
  }
  if (spots.IsEmpty())
  {
   if (s_Failed.Count() > 500) s_Failed.Clear();
   s_Failed.Set(key, now);
  }
  return spots.Count();
 }

 // Sample the ground separately from obstacles. A continuous box sweep catches narrow
 // trunks, poles and overhanging branches that the old spaced pencil rays missed.
 protected static vector s_vTreeProbe;
 protected static bool s_bTreeHit;
 protected static bool BlockingTree(IEntity e)
 {
  if (!Tree.Cast(e)) return true;
  if (vector.DistanceXZ(e.GetOrigin(), s_vTreeProbe) > CLEAR_RADIUS - 1) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  if (maxs[1] - mins[1] < 3) return true;
#ifdef ENABLE_DESTRUCTION
  SCR_DestructibleTreeV2 tree = SCR_DestructibleTreeV2.Cast(e);
  if (tree && tree.GetIsDestroyed()) return true;
#endif
  s_bTreeHit = true;
  return false;
 }

 static bool Clear(vector spot)
 {
  s_iChecks++;
  if (!ACR_Manager.OnMap(spot)) return false;
  BaseWorld world = GetGame().GetWorld();
  float floorY = world.GetSurfaceY(spot[0], spot[2]);
  if (floorY < BBB_Terrain.SEA_LEVEL) return false;
  // Cheapest rejection in woods: a standing tree inside the footprint (its trunk would stop
  // the swept box below). The query stops at the first one.
  s_vTreeProbe = spot;
  s_bTreeHit = false;
  world.QueryEntitiesBySphere(Vector(spot[0], floorY + 2, spot[2]), CLEAR_RADIUS, BlockingTree, null, EQueryEntitiesFlags.STATIC);
  if (s_bTreeHit) { s_iRejTree++; return false; }
  // Cheap first: thin vertical rays inside the swept volume. Anything they hit, the box would
  // hit too, and in a forest one of them nearly always finds a tree before the costly sweep
  // (Jacob 18 Oct: failed searches over flat forest took up to 4.7 s).
  TraceParam ray = new TraceParam();
  ray.Flags = TraceFlags.ENTS;
  for (int r = 0; r < 17; r++)
  {
   float px = spot[0];
   float pz = spot[2];
   if (r > 0)
   {
    float ringR = 9;
    int k = r - 1;
    if (k >= 8) k -= 8;
    float a = Math.PI2 * k / 8.0;
    if (r > 8) { ringR = CLEAR_RADIUS - 1; a += 0.39; }
    px += Math.Cos(a) * ringR;
    pz += Math.Sin(a) * ringR;
   }
   ray.Start = Vector(px, floorY + 52, pz);
   ray.End = Vector(px, floorY + 0.4, pz);
   if (world.TraceMove(ray, null) < 1) { s_iRejRay++; return false; }
  }
  float low = 100000;
  float high = -100000;
  for (float dx = -CLEAR_RADIUS; dx <= CLEAR_RADIUS; dx += 2)
  {
   for (float dz = -CLEAR_RADIUS; dz <= CLEAR_RADIUS; dz += 2)
   {
    if (dx * dx + dz * dz > CLEAR_RADIUS * CLEAR_RADIUS) continue;
    float ground = world.GetSurfaceY(spot[0] + dx, spot[2] + dz);
    if (ground < BBB_Terrain.SEA_LEVEL) { s_iRejGround++; return false; }
    low = Math.Min(low, ground);
    high = Math.Max(high, ground);
    if (high - low > MAX_RISE) { s_iRejGround++; return false; }
   }
  }
  TraceBox clearance = new TraceBox();
  clearance.Mins = Vector(-CLEAR_RADIUS, 0.4, -CLEAR_RADIUS);
  clearance.Maxs = Vector(CLEAR_RADIUS, 12, CLEAR_RADIUS);
  clearance.Start = Vector(spot[0], floorY + 40, spot[2]);
  clearance.End = Vector(spot[0], floorY, spot[2]);
  clearance.Flags = TraceFlags.ENTS;
  bool clear = world.TraceMove(clearance, null) >= 1;
  if (!clear) s_iRejBox++;
  return clear;
 }
}

// One air lift helicopter: the support helicopter's flying, landing at a spot and leaving.
class ACR_LiftHeli : ACR_HeliRun
{
 vector m_Spot;      // where it is landing (pickup, then drop-off)
 bool m_bDown;       // on the ground at m_Spot, rotors turning
 bool m_bLeaving;
 bool m_bAway;       // gone off the map
 protected float m_fLeftAt;
 static int s_iTestSeats;  // tests: cargo seats per helicopter (0 = all), to make a squad need two

 override string StationName() { return "Air lift"; }

 // One frame. False once it is going down or has flown off.
 bool Step(ACR_Supports supports, float dt, float now)
 {
  if (!m_Vehicle || m_bDestroyed || m_bAway) return false;
  if (IsDestroyed() || MovementDamage() >= CRIPPLED_MOVEMENT_DAMAGE)
  {
   m_bDestroyed = true;
   Physics wreck = m_Vehicle.GetPhysics();
   if (wreck) wreck.EnableGravity(true);
   PrintFormat("[ACR] AIRLIFT helicopter going down (destroyed=%1 movement damage=%2)", IsDestroyed(), MovementDamage());
   return false;
  }
  Spool();
  if (StillSpooling(now)) return true;
  if (m_bLifting) return Lift(dt);
  if (m_bLeaving) return FlyAway(dt, now);
  if (m_bDown)
  {
   Place(vector.Zero, 0, 0);
   return true;
  }
  if (FlyToSpot(supports, m_Spot, dt, now))
  {
   m_bDown = true;
   m_Pos = m_Spot;
   Place(vector.Zero, 0, 0);
   PrintFormat("[ACR] AIRLIFT helicopter down at %1", m_Spot);
  }
  return true;
 }

 bool Spooled() { return m_bSpooled; }

 // Off the ground, then on to the next spot (or away).
 void TakeOff(vector next, bool leaving)
 {
  m_bDown = false;
  m_bLeaving = leaving;
  m_Spot = next;
  vector toward = next - m_Pos;
  if (leaving) toward = m_Start - m_Pos;
  toward[1] = 0;
  if (toward.Length() > 1) m_Dir = toward.Normalized();
  m_bLifting = true;
  m_fLeftAt = GetGame().GetWorld().GetWorldTime();
 }

 // Back the way it came: off the map (or 90 s out), then gone.
 protected bool FlyAway(float dt, float now)
 {
  vector flat = m_Start - m_Pos;
  flat[1] = 0;
  float d = flat.Length();
  if (d < 150 || now - m_fLeftAt > 90000)
  {
   m_bAway = true;
   return false;
  }
  vector dir = flat.Normalized();
  float want = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  m_fYaw = Wrap(m_fYaw + Math.Clamp(Wrap(want - m_fYaw), -24 * dt, 24 * dt));
  m_fBodyYaw = m_fYaw;
  vector forward = Heading(m_fYaw);
  float wantedY = FlightClearance(forward, ALT_TRANSIT + m_iStack * STACK_STEP, now);
  m_fSpeed = ApproachSpeed(m_fSpeed, Math.Min(SPEED_RUN, m_fTerrainSpeed), dt);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (m_fSpeed * dt);
  float rate = Math.Clamp((wantedY - m_Pos[1]) * 0.6, -6, 8);
  m_fClimb += (rate - m_fClimb) * Math.Min(1, dt * 1.5);
  m_Pos[1] = previous[1] + m_fClimb * dt;
  float ground = GetGame().GetWorld().GetSurfaceY(m_Pos[0], m_Pos[2]);
  if (m_Pos[1] < ground + 20)
  {
   m_Pos[0] = previous[0];
   m_Pos[2] = previous[2];
   m_fSpeed = 0;
  }
  GuardFlightMove(previous);
  vector velocity = vector.Zero;
  if (dt > 0) velocity = (m_Pos - previous) * (1 / dt);
  Place(velocity, -Math.Clamp(m_fSpeed * 0.1, 0, 6), 0);
  return true;
 }

 int FreeSeats(out BaseCompartmentSlot first, array<BaseCompartmentSlot> claimed = null)
 {
  first = null;
  array<BaseCompartmentSlot> seats = {};
  FreeSeatList(seats, claimed);
  if (!seats.IsEmpty()) first = seats[0];
  return seats.Count();
 }

 // Free cargo seats, leaving out any already given to someone this second (claimed).
 void FreeSeatList(notnull array<BaseCompartmentSlot> result, array<BaseCompartmentSlot> claimed = null)
 {
  result.Clear();
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int used;
  foreach (BaseCompartmentSlot seat : seats)
  {
   if (seat.GetOccupant() || seat.IsReserved()) { used++; continue; }
   if (claimed && claimed.Contains(seat)) { used++; continue; }
   result.Insert(seat);
  }
  if (s_iTestSeats > 0) while (result.Count() > 0 && result.Count() + used > s_iTestSeats) result.Remove(result.Count() - 1);
 }

 int CargoSeats()
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  if (s_iTestSeats > 0) return Math.Min(s_iTestSeats, seats.Count());
  return seats.Count();
 }
}

class ACR_AirLift : ACR_SupportRun
{
 static const int INBOUND = 0;
 static const int BOARDING = 1;
 static const int TRANSIT = 2;
 static const int UNLOADING = 3;
 static const int LEAVING = 4;
 static const float MIN_DISTANCE = 400;     // m: closer than this, walking is about as quick
 static const float PICKUP_SEARCH = 250;    // m round the squad for a clear landing spot
 static const float DROP_SEARCH = 200;      // m round the clicked point
 static const float BOARD_RANGE = 8;        // m: a soldier this close climbs in
 static const float NEAR_RANGE = 40;        // m: placed in after NEAR_BOARD_MS on the ground
 static const float FORCE_RANGE = 400;      // m: placed in after FORCE_BOARD_MS
 static float NEAR_BOARD_MS = 10000;
 static float FORCE_BOARD_MS = 60000;
 static float BOARD_WAIT_MS = 120000;       // then it leaves with whoever is aboard
 static float UNLOAD_WAIT_MS = 25000;       // then anyone still aboard is put out
 static const float TIME_LIMIT_MS = 900000;

 static ref array<ACR_AirLift> s_Active = {};   // weak: a finished lift drops out by itself

 ACR_Squad m_Squad;
 string m_sFaction;
 string m_sSquadName;
 bool m_bByAI;
 int m_iStage;
 ref array<ref ACR_LiftHeli> m_Helis = {};
 ref array<vector> m_Pickups = {};
 ref array<vector> m_Drops = {};
 ResourceName m_sFinalPrefab;
 vector m_FinalTarget;
 float m_fFinalRadius;
 string m_sFinalOrder;
 int m_iFinalKind;
 protected float m_fStarted;
 protected float m_fStageAt;
 protected float m_fNextStep;
 protected bool m_bToldDown;
 protected float m_fOutAt = -1;

 // A squad with an air lift on the way, boarding, in the air or getting out.
 static bool Busy(ACR_Squad squad)
 {
  if (!squad) return false;
  foreach (ACR_AirLift lift : s_Active)
   if (lift && !lift.m_bDone && lift.m_Squad == squad && lift.m_iStage < LEAVING) return true;
  return false;
 }

 static ACR_AirLift For(ACR_Squad squad)
 {
  foreach (ACR_AirLift lift : s_Active)
   if (lift && !lift.m_bDone && lift.m_Squad == squad) return lift;
  return null;
 }

 static int Alive(ACR_Squad squad)
 {
  int alive;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents) if (agent && ACR_Respawn.IsAlive(agent.GetControlledEntity())) alive++;
  return alive;
 }

 // The support call (from ACR_Supports.Call). Nothing is spent unless helicopters can come.
 static string Call(ACR_Supports supports, string factionKey, ACR_SupportEntry entry, vector target, bool byAI, ACR_Squad squad, ResourceName finalPrefab = "", vector finalTarget = vector.Zero, float finalRadius = -1, string finalOrder = "", int finalKind = 0)
 {
  for (int i = s_Active.Count() - 1; i >= 0; i--) if (!s_Active[i]) s_Active.Remove(i);
  if (!squad || !squad.m_Group) return "Select the squad to fly first, then call the air lift";
  string name = squad.GetName();
  if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != factionKey) return "That squad belongs to another faction";
  if (Busy(squad)) return name + " already has an air lift";
  if (squad.HasPlayer()) return name + ": an air lift can't carry a squad a player is in";
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) return name + " is in a vehicle: order it out first";
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return name + " is not ready";
  if (!ACR_Manager.OnMap(target)) return "That landing zone is off the map";
  float distance = vector.DistanceXZ(anchor.GetOrigin(), target);
  if (distance < MIN_DISTANCE) return string.Format("%1 is only %2 m away: walk it (air lifts from %3 m)", name, (int)distance, (int)MIN_DISTANCE);
  array<vector> pickups = {};
  if (ACR_LandingZone.Find(anchor.GetOrigin(), PICKUP_SEARCH, 2, pickups) == 0) return string.Format("No clear landing spot within %1 m of %2 (trees, buildings, slopes or water)", (int)PICKUP_SEARCH, name);
  array<vector> drops = {};
  if (ACR_LandingZone.Find(target, DROP_SEARCH, 2, drops) == 0) return string.Format("No clear landing spot within %1 m of that point (trees, buildings, slopes or water)", (int)DROP_SEARCH);
  int cost = entry.CostFor(byAI);
  if (!supports.m_Manager.m_Bases.Spend(factionKey, cost)) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, cost);
  float now = GetGame().GetWorld().GetWorldTime();
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  ACR_AirLift lift = new ACR_AirLift();
  lift.m_Squad = squad;
  lift.m_sFaction = factionKey;
  lift.m_sSquadName = name;
  lift.m_bByAI = byAI;
  lift.m_sFinalPrefab = finalPrefab;
  lift.m_FinalTarget = finalTarget;
  lift.m_fFinalRadius = finalRadius;
  lift.m_sFinalOrder = finalOrder;
  lift.m_iFinalKind = finalKind;
  lift.m_Pickups.Copy(pickups);
  lift.m_Drops.Copy(drops);
  if (!lift.Begin(supports, now))
  {
   supports.m_Manager.m_Bases.GetFaction(factionKey).m_iRP += cost;
   entry.m_fReadyAt = now;
   return "No helicopter available";
  }
  supports.AddRun(lift);
  s_Active.Insert(lift);
  // They walk to a point beside the landing spot, not under the rotors.
  vector meet = pickups[0];
  vector side = anchor.GetOrigin() - pickups[0];
  side[1] = 0;
  if (side.Length() > 1) meet = ACR_Manager.Ground(pickups[0] + side.Normalized() * 15);
  squad.MoveTo(meet);
  squad.SetOrder("Air lift: to the landing zone", ACR_EOrderState.MOVE);
  ACR_LiftHeli first = lift.m_Helis[0];
  // Include spool-up, gentle acceleration, braking and the final vertical descent.
  float seconds = vector.DistanceXZ(first.m_Start, pickups[0]) / ACR_HeliRun.SPEED_RUN + ACR_HeliRun.SPOOL_MAX_SECONDS;
  seconds += ACR_HeliRun.SPEED_RUN / (ACR_HeliRun.FLIGHT_ACCEL * 2) + ACR_HeliRun.SPEED_RUN / (ACR_HeliRun.FLIGHT_BRAKE * 2);
  int eta = (int)(seconds + ACR_HeliRun.ALT_TRANSIT / ACR_HeliRun.LIFT_RATE);
  string message = string.Format("%1 for %2: %3 helicopter(s) landing at %4, ETA about %5 s.", entry.m_sName, name, lift.m_Helis.Count(), ACR_Manager.Grid(pickups[0]), (int)(Math.Round(eta / 15.0) * 15));
  PrintFormat("[ACR] AIRLIFT %1 %2: %3 helicopter(s), %4 soldiers, pickup %5, drop %6 (%7 m), cost %8%9", factionKey, name, lift.m_Helis.Count(), Alive(squad), pickups[0], drops[0], (int)vector.DistanceXZ(pickups[0], drops[0]), cost, AIText(byAI));
  if (!byAI) supports.m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 protected static string AIText(bool byAI)
 {
  if (byAI) return " (AI commander)";
  return "";
 }

 // Spawns the helicopters off the map: a second one when the first hasn't seats for everyone
 // (and both ends have room for two).
 bool Begin(ACR_Supports supports, float now)
 {
  m_fStarted = now;
  m_fStageAt = now;
  ResourceName prefab = supports.TransportHelicopterFor(m_sFaction);
  if (!SpawnHeli(supports, prefab, 0, now)) return false;
  int seats = m_Helis[0].CargoSeats();
  if (seats < Alive(m_Squad) && m_Pickups.Count() > 1 && m_Drops.Count() > 1) SpawnHeli(supports, prefab, 1, now);
  PrintFormat("[ACR] AIRLIFT %1: %2 cargo seats per helicopter, %3 helicopter(s)", m_sSquadName, seats, m_Helis.Count());
  return true;
 }

 protected bool SpawnHeli(ACR_Supports supports, ResourceName prefab, int index, float now)
 {
  ACR_LiftHeli heli = new ACR_LiftHeli();
  heli.m_sFaction = m_sFaction;
  heli.m_bDoorGuns = true; // no rockets
  heli.m_Target = m_Pickups[index];
  heli.m_Spot = m_Pickups[index];
  heli.m_Dir = supports.ApproachDirection(m_sFaction, m_Pickups[index]);
  heli.m_iStack = index;
  if (!heli.Start(supports, prefab, now)) return false;
  m_Helis.Insert(heli);
  return true;
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (now - m_fStarted > TIME_LIMIT_MS)
  {
   PrintFormat("[ACR] AIRLIFT %1: out of time in stage %2", m_sSquadName, m_iStage);
   return false;
  }
  int flying;
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (heli.Step(supports, dt, now)) { flying++; continue; }
   if (heli.m_bDestroyed && !m_bToldDown)
   {
    m_bToldDown = true;
    Tell(supports, "Air lift helicopter is going down!");
   }
  }
  if (flying == 0) return false;
  if (now < m_fNextStep) return true;
  m_fNextStep = now + 1000;
  bool squadGone = !m_Squad || !m_Squad.m_Group || Alive(m_Squad) == 0;
  switch (m_iStage)
  {
   case INBOUND:
   {
    if (squadGone) { Leave(supports, now, "the squad is gone"); break; }
    if (!AllDown()) break;
    Stage(BOARDING, now);
    Tell(supports, string.Format("Air lift landed for %1 at %2.", m_sSquadName, ACR_Manager.Grid(m_Pickups[0])));
    break;
   }
   case BOARDING:
   {
    if (squadGone) { Leave(supports, now, "the squad is gone"); break; }
    int aboard = Board(now);
    int alive = Alive(m_Squad);
    if (aboard >= alive || (now - m_fStageAt > BOARD_WAIT_MS && aboard > 0))
    {
     if (aboard < alive) PrintFormat("[ACR] AIRLIFT %1: leaving with %2 of %3 aboard", m_sSquadName, aboard, alive);
     m_Squad.Halt("Air lift: in the air", ACR_EOrderState.HOLD);
     foreach (int i, ACR_LiftHeli up : m_Helis)
     {
      if (up.m_bDestroyed || up.m_bAway) continue;
      if (Passengers(up) == 0) up.TakeOff(up.m_Start, true);
      else up.TakeOff(m_Drops[Math.Min(i, m_Drops.Count() - 1)], false);
     }
     Stage(TRANSIT, now);
     PrintFormat("[ACR] AIRLIFT %1: %2 of %3 aboard, taking off", m_sSquadName, aboard, alive);
    }
    else if (now - m_fStageAt > BOARD_WAIT_MS)
    {
     m_Squad.Halt("Holding");
     Leave(supports, now, "nobody reached the landing zone");
     Tell(supports, string.Format("Air lift left: %1 never reached the landing zone.", m_sSquadName));
    }
    break;
   }
   case TRANSIT:
   {
    bool allDown = true;
    int carrying;
    foreach (ACR_LiftHeli carrier : m_Helis)
    {
     if (carrier.m_bDestroyed || carrier.m_bAway || carrier.m_bLeaving) continue;
     carrying++;
     if (!carrier.m_bDown) allDown = false;
    }
    if (carrying == 0) return false;
    if (!allDown) break;
    Stage(UNLOADING, now);
    if (m_Squad && m_Squad.m_Group)
    {
     if (!m_sFinalPrefab.IsEmpty())
     {
      vector dest = m_FinalTarget;
      if (dest == vector.Zero) dest = m_Drops[0];
      m_Squad.ContinueAfterDismount(m_sFinalPrefab, dest, m_fFinalRadius, m_sFinalOrder, m_iFinalKind);
     }
     m_Squad.GetOut();
    }
    PrintFormat("[ACR] AIRLIFT %1: down at the drop-off, getting out", m_sSquadName);
    break;
   }
   case UNLOADING:
   {
    int still = Aboard();
    if (still > 0 && now - m_fStageAt > UNLOAD_WAIT_MS) PutOut();
    if (still > 0 && now - m_fStageAt <= UNLOAD_WAIT_MS + 5000) break;
    // Out (or left 5 s to get clear of a forced exit): hold the landing zone, then go.
    if (m_Squad && m_Squad.m_Group && Alive(m_Squad) > 0)
    {
     if (!m_sFinalPrefab.IsEmpty())
     {
      vector finalDest = m_FinalTarget;
      if (finalDest == vector.Zero) finalDest = m_Drops[0];
      m_Squad.RestoreOrder(ACR_OrderSnapshot.Make(m_sFinalPrefab, finalDest, m_fFinalRadius, m_sFinalOrder, ACR_Squad.StateOfLabel(m_sFinalOrder), m_iFinalKind));
     }
     else
     {
      m_Squad.Defend(m_Drops[0]);
     }
    }
    Tell(supports, string.Format("%1 landed at %2.", m_sSquadName, ACR_Manager.Grid(m_Drops[0])));
    Leave(supports, now, "done");
    break;
   }
  }
  return true;
 }

 protected void Stage(int stage, float now)
 {
  m_iStage = stage;
  m_fStageAt = now;
 }

 protected void Tell(ACR_Supports supports, string message)
 {
  PrintFormat("[ACR] AIRLIFT %1", message);
  if (!m_bByAI) supports.m_Manager.NotifyFaction(m_sFaction, message);
 }

 protected void Leave(ACR_Supports supports, float now, string why)
 {
  PrintFormat("[ACR] AIRLIFT %1: helicopters leaving (%2)", m_sSquadName, why);
  foreach (ACR_LiftHeli heli : m_Helis)
   if (!heli.m_bDestroyed && !heli.m_bAway && !heli.m_bLeaving) heli.TakeOff(heli.m_Start, true);
  Stage(LEAVING, now);
 }

 protected bool AllDown()
 {
  int live;
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (heli.m_bDestroyed || heli.m_bAway) continue;
   live++;
   if (!heli.m_bDown) return false;
  }
  return live > 0;
 }

 // Seats squad members in the landed helicopters: those close climb in; those near are placed
 // in after a while; stragglers after FORCE_BOARD_MS. Returns how many are aboard.
 protected int Board(float now)
 {
  float waited = now - m_fStageAt;
  int aboard;
  array<BaseCompartmentSlot> claimed = {};
  array<AIAgent> agents = {};
  m_Squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !ACR_Respawn.IsAlive(character)) continue;
   IEntity inVehicle = ACR_Transport.VehicleOf(character);
   if (inVehicle && Ours(inVehicle)) { aboard++; continue; }
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access || access.IsGettingIn()) continue;
   ACR_LiftHeli best;
   BaseCompartmentSlot seat;
   float bestDistance = float.MAX;
   foreach (ACR_LiftHeli heli : m_Helis)
   {
    if (heli.m_bDestroyed || heli.m_bAway || !heli.m_bDown) continue;
    BaseCompartmentSlot free;
    if (heli.FreeSeats(free, claimed) == 0) continue;
    float d = vector.DistanceXZ(character.GetOrigin(), heli.m_Vehicle.GetOrigin());
    if (d < bestDistance) { bestDistance = d; best = heli; seat = free; }
   }
   if (!best) continue;
   bool close = bestDistance <= BOARD_RANGE;
   bool near = bestDistance <= NEAR_RANGE && waited > NEAR_BOARD_MS;
   bool forced = bestDistance <= FORCE_RANGE && waited > FORCE_BOARD_MS;
   if (!close && !near && !forced) continue;
   // Some cargo seats refuse a soldier (seen 14 Oct: one UH-1H seat, every try): try them all.
   array<BaseCompartmentSlot> seats = {};
   best.FreeSeatList(seats, claimed);
   bool ok = false;
   string seatName = "";
   foreach (BaseCompartmentSlot trySeat : seats)
   {
    ok = access.GetInVehicle(best.m_Vehicle, trySeat, !close, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
    seatName = trySeat.GetCompartmentName();
    if (ok) { claimed.Insert(trySeat); break; }
   }
   if (!ok && !close) ok = access.GetInVehicle(best.m_Vehicle, seat, false, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
   PrintFormat("[ACR] AIRLIFT board %1: %2 m, placed %3, seat %4 -> %5", m_sSquadName, (int)bestDistance, !close, seatName, ok);
  }
  return aboard;
 }

 protected bool Ours(IEntity vehicle)
 {
  foreach (ACR_LiftHeli heli : m_Helis) if (heli.m_Vehicle == vehicle) return true;
  return false;
 }

 // Squad members in a helicopter's cargo seats.
 protected int Passengers(ACR_LiftHeli heli)
 {
  int count;
  if (!m_Squad) return 0;
  array<AIAgent> agents = {};
  m_Squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity character = agent.GetControlledEntity();
   if (character && ACR_Respawn.IsAlive(character) && ACR_Transport.VehicleOf(character) == heli.m_Vehicle) count++;
  }
  return count;
 }

 protected int Aboard()
 {
  int count;
  foreach (ACR_LiftHeli heli : m_Helis) if (heli.m_Vehicle) count += Passengers(heli);
  return count;
 }

 // Whoever is still in a cargo seat (any group) is put out on the ground beside the helicopter.
 protected void PutOut()
 {
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (!heli.m_Vehicle) continue;
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   if (!compartments) continue;
   array<BaseCompartmentSlot> seats = {};
   compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
   foreach (BaseCompartmentSlot seat : seats)
   {
    ChimeraCharacter passenger = ChimeraCharacter.Cast(seat.GetOccupant());
    if (!passenger) continue;
    CompartmentAccessComponent access = passenger.GetCompartmentAccessComponent();
    if (!access || access.IsGettingOut()) continue;
    bool ok = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    PrintFormat("[ACR] AIRLIFT %1: put a passenger out -> %2", m_sSquadName, ok);
   }
  }
 }

 override void Finish()
 {
  m_bDone = true;
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   if (!heli.m_Vehicle || heli.m_bDestroyed) continue;
   if (Passengers(heli) > 0) PrintFormat("[ACR] AIRLIFT %1: %2 still aboard when it ended", m_sSquadName, Passengers(heli));
   heli.Finish();
  }
  PrintFormat("[ACR] AIRLIFT %1 finished (stage %2)", m_sSquadName, m_iStage);
 }
}
