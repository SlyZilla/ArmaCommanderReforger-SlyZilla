// Transport helpers for squads: who is riding in a vehicle and which vehicle a
// squad crews. Seats and the "settled" check are in BBB_Transport.
class ACR_Transport
{
 static const float LZ_RADIUS = 100;      // queued unload triggers within this of the LZ

 static IEntity VehicleOf(IEntity body) { return BBB_Transport.VehicleOf(body); }
 static ECompartmentType SeatOf(IEntity body) { return BBB_Transport.SeatOf(body); }
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
    if (VehicleOf(body) == vehicle && SeatOf(body) == ECompartmentType.CARGO)
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
   if (vehicle && SeatOf(body) != ECompartmentType.CARGO) return vehicle;
  }
  return null;
 }

}
