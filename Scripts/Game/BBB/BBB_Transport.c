// Who sits where in a vehicle, and is a vehicle "down" enough to drop its
// passengers. The rule is deliberately loose (slow and within a few metres of
// whatever is below: ground, a rooftop or water), so hover drops on roofs and
// helocasting work.
class BBB_Transport
{
 static const float SETTLED_SPEED = 3;    // m/s
 static const float SETTLED_HEIGHT = 3;   // m from the underside (skids/wheels) to ground, roof or water

 static IEntity VehicleOf(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) return null;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  if (!slot) return null;
  return slot.GetVehicle();
 }

 static ECompartmentType SeatOf(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !character.GetCompartmentAccessComponent()) return ECompartmentType.CARGO;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  if (!slot) return ECompartmentType.CARGO;
  return slot.GetType();
 }

 // The vehicle a group crews (driver/pilot/gunners), if it does.
 static IEntity CrewedVehicle(SCR_AIGroup group)
 {
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   IEntity vehicle = VehicleOf(body);
   if (vehicle && SeatOf(body) != ECompartmentType.CARGO) return vehicle;
  }
  return null;
 }

 protected static IEntity s_TraceVehicle;
 protected static bool IgnoreOwnVehicle(notnull IEntity entity)
 {
  if (ChimeraCharacter.Cast(entity)) return false;
  return entity != s_TraceVehicle && entity.GetRootParent() != s_TraceVehicle;
 }

 static bool Settled(IEntity vehicle, out float height, out float speed)
 {
  speed = 0;
  Physics physics = vehicle.GetPhysics();
  if (physics) speed = physics.GetVelocity().Length();
  vector pos = vehicle.GetOrigin();
  BaseWorld world = GetGame().GetWorld();
  float below = Math.Max(world.GetSurfaceY(pos[0], pos[2]), 0); // sea level is 0
  TraceParam trace = new TraceParam();
  trace.Start = pos;
  trace.End = pos - Vector(0, 80, 0);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  s_TraceVehicle = vehicle;
  float fraction = world.TraceMove(trace, IgnoreOwnVehicle);
  if (fraction < 1) below = Math.Max(below, pos[1] - 80 * fraction);
  // Measure from the vehicle's underside, not its model origin.
  vector mins, maxs;
  vehicle.GetBounds(mins, maxs); // model space: mins[1] is how far the underside sits below the origin
  float bottom = pos[1];
  if (maxs[1] > mins[1] && mins[1] < 0 && mins[1] > -10) bottom = pos[1] + mins[1];
  height = Math.Max(0, bottom - below);
  if (System.IsCLIParam("acrTransportTest")) PrintFormat("[ACR] SETTLE origin %1 underside %2 below %3 -> %4 m", pos[1], bottom, below, height);
  return speed < SETTLED_SPEED && height < SETTLED_HEIGHT;
 }

 static ref map<IEntity, float> s_mLastVehicleHit = new map<IEntity, float>();

 static void OnVehicleHit(IEntity vehicle)
 {
  if (!vehicle || !GetGame() || !GetGame().GetWorld()) return;
  s_mLastVehicleHit.Set(vehicle, GetGame().GetWorld().GetWorldTime());
 }

 static bool VehicleHitRecently(IEntity vehicle, float windowMs = 6000)
 {
  if (!vehicle || !GetGame() || !GetGame().GetWorld() || !s_mLastVehicleHit.Contains(vehicle)) return false;
  return GetGame().GetWorld().GetWorldTime() - s_mLastVehicleHit.Get(vehicle) < windowMs;
 }

 static void ClearHits()
 {
  s_mLastVehicleHit.Clear();
 }
}
