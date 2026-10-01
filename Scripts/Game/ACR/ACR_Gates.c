// Two small helpers for AI vehicles, ticked once a second on the server.
// Gates: the game's AI driver stops, gets out and opens a closed gate on its
// route, which often wrecked rides (the transport counted as lost). AI-driven
// vehicles now open gates just ahead of them, without stopping.
// Truck beds: soldiers getting out of cargo seats sometimes end up standing on
// the bed and never get down. One still up there after 4 s is set down on
// the ground beside the vehicle.
class ACR_Gates : Managed
{
 static const float LOOK_AHEAD = 9;     // m in front of the vehicle
 static const float LOOK_RADIUS = 12;
 static const float GATE_MIN_WIDTH = 2.2; // smaller doors are house doors: left alone
 static const float BED_HEIGHT = 0.9;    // m above the ground

 protected ACR_Manager m_Manager;
 protected ref array<IEntity> m_Found = {};
 protected ref map<IEntity, int> m_OnBed = new map<IEntity, int>();

 void ACR_Gates(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 void Tick()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   OpenGatesFor(squad);
   GetOffBeds(squad);
  }
 }

 // ---------------------------------------------------------------- gates

 protected bool CollectDoor(IEntity entity)
 {
  if (entity.FindComponent(BaseDoorComponent) && !m_Found.Contains(entity)) m_Found.Insert(entity);
  return true;
 }

 protected void OpenGatesFor(ACR_Squad squad)
 {
  if (!squad.m_Group) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (!vehicle || m_Manager.IsGunnerDriven(vehicle)) return;
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver || !ACR_Respawn.IsAlive(driver) || ACR_Squad.FindController(driver)) return;
  array<AIWaypoint> waypoints = {};
  squad.m_Group.GetWaypoints(waypoints);
  if (waypoints.IsEmpty()) return;
  vector ahead = vehicle.GetOrigin() + vehicle.GetTransformAxis(2) * LOOK_AHEAD;
  m_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(ahead, LOOK_RADIUS, CollectDoor);
  foreach (IEntity entity : m_Found)
  {
   BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
   if (!door || door.IsOpen() || door.IsOpening()) continue;
   vector mins, maxs;
   entity.GetBounds(mins, maxs);
   if (Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) < GATE_MIN_WIDTH) continue;
   door.SetControlValue(1, RplId.Invalid());
   PrintFormat("[ACR] GATE opened ahead of %1 at %2", squad.GetName(), entity.GetOrigin());
  }
 }

 // ---------------------------------------------------------------- truck beds

 protected IEntity m_BedVehicle;
 protected vector m_BedSoldier;
 protected bool CollectVehicleUnder(IEntity entity)
 {
  if (!Vehicle.Cast(entity)) return true;
  vector mins, maxs;
  entity.GetWorldBounds(mins, maxs);
  if (m_BedSoldier[0] < mins[0] || m_BedSoldier[0] > maxs[0] || m_BedSoldier[2] < mins[2] || m_BedSoldier[2] > maxs[2]) return true;
  m_BedVehicle = entity;
  return false;
 }

 protected void GetOffBeds(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || !ACR_Respawn.IsAlive(soldier) || ACR_Transport.VehicleOf(soldier) || ACR_Squad.FindController(soldier)) { m_OnBed.Remove(soldier); continue; }
   vector at = soldier.GetOrigin();
   if (at[1] - GetGame().GetWorld().GetSurfaceY(at[0], at[2]) < BED_HEIGHT) { m_OnBed.Remove(soldier); continue; }
   m_BedVehicle = null;
   m_BedSoldier = at;
   GetGame().GetWorld().QueryEntitiesBySphere(at, 6, CollectVehicleUnder);
   if (!m_BedVehicle) { m_OnBed.Remove(soldier); continue; }
   int seen = m_OnBed.Get(soldier) + 1;
   m_OnBed.Set(soldier, seen);
   if (seen < 4) continue; // getting out takes a moment: only those still up there after 4 s
   m_OnBed.Remove(soldier);
   // Down beside the vehicle, on the side he is nearer to.
   vector side = m_BedVehicle.GetTransformAxis(0);
   if (vector.Dot(at - m_BedVehicle.GetOrigin(), side) < 0) side = side * -1;
   vector mins, maxs;
   m_BedVehicle.GetBounds(mins, maxs);
   vector ground = m_BedVehicle.GetOrigin() + side * (Math.Max(maxs[0], -mins[0]) + 1.2);
   ground[1] = GetGame().GetWorld().GetSurfaceY(ground[0], ground[2]);
   vector transform[4];
   soldier.GetWorldTransform(transform);
   transform[3] = ground;
   soldier.Teleport(transform);
   PrintFormat("[ACR] %1: soldier stuck on a vehicle set down beside it", squad.GetName());
  }
 }
}
