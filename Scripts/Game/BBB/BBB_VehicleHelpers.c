// Three helpers for AI vehicles, ticked once a second on the server (BBB.Tick).
// Gates: the game's AI driver stops, gets out and opens a closed gate on its
// route, which often wrecked rides (the transport counted as lost). AI-driven
// vehicles now open gates just ahead of them, without stopping.
// Truck beds: soldiers getting out of cargo seats sometimes end up standing on
// the bed and never get down. One still up there after 4 s is set down on
// the ground beside the vehicle.
// Doors & hatches: AI crews close vehicle doors and hatches once back aboard.
class BBB_VehicleHelpers : Managed
{
 static const float LOOK_AHEAD = 25;    // m in front of the vehicle: the AI driver stops
 static const float LOOK_RADIUS = 25;   // for a gate up to ~45 m ahead
 static const float GATE_MIN_WIDTH = 2.2; // smaller doors are house doors: left alone
 static const float BED_HEIGHT = 0.9;    // m above the ground

 protected ref array<IEntity> m_Found = {};
 protected ref map<IEntity, int> m_OnBed = new map<IEntity, int>();

 protected static bool s_bDoors = true;

 void BBB_VehicleHelpers()
 {
  string val;
  if (System.GetCLIParam("bbbDoors", val) && val == "0") s_bDoors = false;
 }

 void Tick()
 {
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  array<IEntity> handledVehicles = {};
  foreach (SCR_AIGroup group : groups)
  {
   if (!group) continue;
   OpenGatesFor(group);
   GetOffBeds(group);
   CloseDoors(group, handledVehicles);
  }
 }

 // ---------------------------------------------------------------- gates

 protected bool CollectDoor(IEntity entity)
 {
  if (entity.FindComponent(BaseDoorComponent) && !m_Found.Contains(entity)) m_Found.Insert(entity);
  return true;
 }

 protected void OpenGatesFor(SCR_AIGroup group)
 {
  IEntity vehicle = BBB_Transport.CrewedVehicle(group);
  // The driver may already be out, walking to the gate: still open it, he gets back in.
  IEntity own = BBB.s_Hooks.OwnVehicleOf(group);
  IEntity anchor = BBB.s_Hooks.AnchorOf(group);
  if (!vehicle && own && BBB.Health(own) > 0 && anchor && vector.DistanceXZ(own.GetOrigin(), anchor.GetOrigin()) < 80) vehicle = own;
  if (!vehicle || BBB.s_Hooks.LeaveAlone(vehicle) || BBB.HasPlayer(group)) return;
  ChimeraCharacter driver = BBB.DriverOf(vehicle);
  if (driver && BBB.PlayerOf(driver)) return;
  array<AIWaypoint> waypoints = {};
  group.GetWaypoints(waypoints);
  if (waypoints.IsEmpty()) return;
  vector ahead = vehicle.GetOrigin() + vehicle.GetTransformAxis(2) * LOOK_AHEAD;
  m_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(ahead, LOOK_RADIUS, CollectDoor);
  // The obstacle the game's AI driver is stopping for, if it says.
  AICarMovementComponent movement = AICarMovementComponent.Cast(vehicle.FindComponent(AICarMovementComponent));
  IEntity navlink;
  if (movement) navlink = movement.GetLastNavlinkEntity();
  if (navlink && vector.Distance(navlink.GetOrigin(), vehicle.GetOrigin()) < 80)
  {
   CollectDoor(navlink);
   IEntity child = navlink.GetChildren();
   while (child)
   {
    CollectDoor(child);
    child = child.GetSibling();
   }
  }
  foreach (IEntity entity : m_Found)
  {
   BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
   if (!door || door.IsOpen() || door.IsOpening()) continue;
   vector mins, maxs;
   entity.GetBounds(mins, maxs);
   if (Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) < GATE_MIN_WIDTH) continue;
   door.SetControlValue(1, RplId.Invalid());
   PrintFormat("[ACR] GATE opened ahead of %1 at %2", BBB.s_Hooks.NameOf(group), entity.GetOrigin());
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

 protected void GetOffBeds(SCR_AIGroup group)
 {
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || !BBB.IsAlive(soldier) || BBB_Transport.VehicleOf(soldier) || BBB.PlayerOf(soldier)) { m_OnBed.Remove(soldier); continue; }
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
   PrintFormat("[ACR] %1: soldier stuck on a vehicle set down beside it", BBB.s_Hooks.NameOf(group));
  }
 }

 // ---------------------------------------------------------------- vehicle doors & hatches

 protected void CloseDoors(SCR_AIGroup group, notnull array<IEntity> handledVehicles)
 {
  if (!s_bDoors || !group || BBB.HasPlayer(group)) return;
  IEntity vehicle = BBB_Transport.CrewedVehicle(group);
  if (!vehicle)
  {
   array<AIAgent> agents = {};
   BBB.s_Hooks.GetMembers(group, agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    vehicle = BBB_Transport.VehicleOf(body);
    if (vehicle) break;
   }
  }
  if (!vehicle || handledVehicles.Contains(vehicle) || BBB.s_Hooks.LeaveAlone(vehicle)) return;
  handledVehicles.Insert(vehicle);
  CloseVehicleDoors(vehicle);
 }

 static int CloseVehicleDoors(IEntity vehicle)
 {
  if (!s_bDoors || !vehicle) return 0;
  BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
  if (!manager) return 0;

  array<BaseCompartmentSlot> compartments = {};
  manager.GetCompartments(compartments);
  if (compartments.IsEmpty()) return 0;

  CompartmentAccessComponent actorAccess;
  foreach (BaseCompartmentSlot comp : compartments)
  {
   if (comp.IsGetInLocked()) continue;
   IEntity occ = comp.GetOccupant();
   ChimeraCharacter ch = ChimeraCharacter.Cast(occ);
   if (ch)
   {
    CompartmentAccessComponent acc = ch.GetCompartmentAccessComponent();
    if (acc)
    {
     if (acc.IsGettingIn() || acc.IsGettingOut()) continue;
     if (!actorAccess && !BBB.PlayerOf(ch) && BBB.IsAlive(ch)) actorAccess = acc;
    }
   }
  }
  if (!actorAccess) return 0;

  int closed = 0;
  array<int> checkedDoors = {};
  foreach (BaseCompartmentSlot comp : compartments)
  {
   if (comp.IsGetInLocked()) continue;
   IEntity occ = comp.GetOccupant();
   ChimeraCharacter ch = ChimeraCharacter.Cast(occ);
   if (ch && BBB.PlayerOf(ch)) continue;

   CompartmentAccessComponent access = actorAccess;
   if (ch && BBB.IsAlive(ch))
   {
    CompartmentAccessComponent occAcc = ch.GetCompartmentAccessComponent();
    if (occAcc)
    {
     if (occAcc.IsGettingIn() || occAcc.IsGettingOut()) continue;
     access = occAcc;
    }
   }

   int turnOut = comp.GetTurnOutDoorIndex();
   if (turnOut >= 0 && !checkedDoors.Contains(turnOut))
   {
    checkedDoors.Insert(turnOut);
    if (!comp.IsOccupantTurnedOut() && manager.IsDoorOpen(turnOut) && !manager.IsDoorFake(turnOut) && !manager.GetDoorUser(turnOut))
    {
     bool didClose = false;
     if (access.CanAccessDoor(vehicle, manager, turnOut))
      didClose = access.CloseDoor(vehicle, ECharacterDoorAnimType.NO_CHARACTER_ANIM, turnOut);
     if (!didClose && access != actorAccess && actorAccess.CanAccessDoor(vehicle, manager, turnOut))
      didClose = actorAccess.CloseDoor(vehicle, ECharacterDoorAnimType.NO_CHARACTER_ANIM, turnOut);
     if (didClose) closed++;
    }
   }

   array<int> doorIndices = {};
   comp.GetAvailableDoorIndices(doorIndices);
   foreach (int doorIndex : doorIndices)
   {
    if (doorIndex < 0 || checkedDoors.Contains(doorIndex)) continue;
    checkedDoors.Insert(doorIndex);
    if (manager.IsDoorOpen(doorIndex) && !manager.IsDoorFake(doorIndex) && !manager.GetDoorUser(doorIndex))
    {
     bool didCloseDoor = false;
     if (access.CanAccessDoor(vehicle, manager, doorIndex))
      didCloseDoor = access.CloseDoor(vehicle, ECharacterDoorAnimType.NO_CHARACTER_ANIM, doorIndex);
     if (!didCloseDoor && access != actorAccess && actorAccess.CanAccessDoor(vehicle, manager, doorIndex))
      didCloseDoor = actorAccess.CloseDoor(vehicle, ECharacterDoorAnimType.NO_CHARACTER_ANIM, doorIndex);
     if (didCloseDoor) closed++;
    }
   }
  }
  return closed;
 }
}
