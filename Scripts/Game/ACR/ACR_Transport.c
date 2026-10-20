// Transport helpers for squads: who is riding in a vehicle and which vehicle a
// squad crews. Seats and the "settled" check are in BBB_Transport.
class ACR_Transport
{
 static const float LZ_RADIUS = 100;      // queued unload triggers within this of the LZ

 static IEntity VehicleOf(IEntity body) { return BBB_Transport.VehicleOf(body); }
 static ECompartmentType SeatOf(IEntity body) { return BBB_Transport.SeatOf(body); }

 // A commander seat: the engine calls it CARGO, but it's wired to a commander sight
 // turret (LAV-25). Its occupant is crew, not a passenger.
 static bool CommanderSeat(BaseCompartmentSlot slot)
 {
  return slot && slot.GetType() == ECompartmentType.CARGO && !slot.GetAutoConnectTurret().IsEmpty();
 }

 // Riding in the back (a cargo seat that isn't a commander seat).
 static bool Passenger(IEntity body)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(body);
  if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) return false;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  return slot && slot.GetType() == ECompartmentType.CARGO && !CommanderSeat(slot);
 }
 static bool Settled(IEntity vehicle, out float height, out float speed) { return BBB_Transport.Settled(vehicle, height, speed); }

 // Squads with soldiers in the passenger (cargo) seats of this vehicle.
 static void PassengerSquads(ACR_Manager manager, IEntity vehicle, array<ACR_Squad> result)
 {
  result.Clear();
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (VehicleOf(body) == vehicle && Passenger(body))
    {
     result.Insert(squad);
     break;
    }
   }
  }
 }

 // The squad crewing a vehicle (driver/pilot/gunners), if the squad is one.
 static IEntity CrewedVehicle(ACR_Squad squad)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   IEntity vehicle = VehicleOf(body);
   if (vehicle && !Passenger(body)) return vehicle;
  }
  return null;
 }

}
