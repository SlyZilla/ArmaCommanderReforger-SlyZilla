// -acrCrewTest 1 (server-capable, ~1 min): an armed vehicle crew keeps its seats through a
// Defend order and the order after it (the game's Defend waypoint ordered turret crews out:
// LAV crew bailing out, 2 Oct live), and Reinforce's vehicle service prices and refills it.
class ACR_TestCrew
{
 static const ResourceName LAV = "{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et";
 static IEntity s_Lav;

 static ACR_Squad Crew(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (ACR_Transport.CrewedVehicle(squad) == s_Lav && s_Lav) return squad;
  return null;
 }

 static vector s_From;
 static IEntity DriverOf(IEntity vehicle)
 {
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetPilotCompartmentSlot()) return null;
  return car.GetPilotCompartmentSlot().GetOccupant();
 }

 static int Seated(ACR_Squad squad)
 {
  int seated;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter c = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (c && c.IsInVehicle()) seated++;
  }
  return seated;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_Squad.s_bGameDefend = System.IsCLIParam("acrCrewOldTest");
   PrintFormat("[ACR] CREW game Defend waypoint for crews: %1", ACR_Squad.s_bGameDefend);
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  vector home = alpha.m_Home;
  if (tick == 4)
  {
   s_Lav = ACR_Manager.SpawnPrefab(LAV, home + Vector(40, 0, 20));
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(s_Lav.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<ECompartmentType> types = {ECompartmentType.PILOT, ECompartmentType.TURRET};
   if (seats) seats.SpawnDefaultOccupants(types);
  }
  ACR_Squad crew = Crew(manager);
  if (tick == 10)
  {
   if (!crew) { ACR_TestRegress.Check("crew: LAV crew registered", false, ""); return; }
   PrintFormat("[ACR] CREW %1 seated %2, defend: %3", crew.GetName(), Seated(crew), crew.Defend(ACR_Manager.Ground(home + Vector(70, 0, 40))));
   ACR_TestRegress.Check("crew: a vehicle crew's Defend is a plain move for the game", SCR_DefendWaypoint.Cast(crew.m_Waypoint) == null, string.Format("%1", crew.m_Waypoint));
  }
  if (tick == 30 && crew)
  {
   PrintFormat("[ACR] CREW after Defend: seated %1 | %2", Seated(crew), crew.Describe());
   ACR_TestRegress.Check("crew: still aboard while defending", Seated(crew) >= 2, string.Format("%1 seated", Seated(crew)));
   crew.MoveTo(ACR_Manager.Ground(home + Vector(120, 0, 60)));
  }
  if (tick == 50 && crew)
  {
   PrintFormat("[ACR] CREW after the next order: seated %1 | %2", Seated(crew), crew.Describe());
   ACR_TestRegress.Check("crew: still aboard after the order replacing Defend", Seated(crew) >= 2, string.Format("%1 seated", Seated(crew)));
   // Reinforce's vehicle service: empty guns and some damage cost RP, then all is full again.
   array<BaseMagazineComponent> magazines = {};
   ACR_Logistics.TurretMagazines(s_Lav, magazines);
   foreach (BaseMagazineComponent magazine : magazines) magazine.SetAmmoCount(magazine.GetMaxAmmoCount() / 4);
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(s_Lav.FindComponent(SCR_DamageManagerComponent));
   if (damage) damage.SetHealthScaled(0.6);
   int cost = ACR_Requisition.ServiceCost(s_Lav);
   array<IEntity> list = {s_Lav};
   int served = manager.m_Requisition.ServiceVehicles(list);
   float ammo = ACR_Logistics.VehicleAmmo(s_Lav);
   PrintFormat("[ACR] CREW service: %1 magazines, cost %2 RP, served %3, ammo now %4, health %5", magazines.Count(), cost, served, ammo, ACR_Logistics.Health(s_Lav));
   ACR_TestRegress.Check("crew: rearm + repair priced by what's missing", cost > 5, string.Format("%1 RP", cost));
   ACR_TestRegress.Check("crew: vehicle rearmed and repaired", served == 1 && ammo > 0.99 && ACR_Logistics.Health(s_Lav) > 0.98, string.Format("ammo %1 health %2", ammo, ACR_Logistics.Health(s_Lav)));
   // As if a player had dropped control and left the driver's and the vehicle's AI parked.
   ChimeraCharacter driver = ChimeraCharacter.Cast(DriverOf(s_Lav));
   AIControlComponent driverAI;
   if (driver) driverAI = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
   if (driverAI && driverAI.IsAIActivated()) driverAI.DeactivateAI();
   AIControlComponent vehicleAI = AIControlComponent.Cast(s_Lav.FindComponent(ChimeraAIVehicleControlComponent));
   if (vehicleAI && vehicleAI.IsAIActivated()) vehicleAI.DeactivateAI();
  }
  if (tick == 52 && crew)
  {
   crew.WakeCrew("test");
   ChimeraCharacter woken = ChimeraCharacter.Cast(DriverOf(s_Lav));
   AIControlComponent woke;
   if (woken) woke = AIControlComponent.Cast(woken.FindComponent(AIControlComponent));
   AIControlComponent vehicleWoke = AIControlComponent.Cast(s_Lav.FindComponent(ChimeraAIVehicleControlComponent));
   ACR_TestRegress.Check("crew: parked driver and vehicle AI back on after a player leaves", woke && woke.IsAIActivated() && (!vehicleWoke || vehicleWoke.IsAIActivated()), "");
   s_From = s_Lav.GetOrigin();
   crew.MoveTo(ACR_Manager.Ground(home + Vector(40, 0, 140)));
  }
  if (tick == 75 && crew)
  {
   float moved = vector.DistanceXZ(s_Lav.GetOrigin(), s_From);
   ACR_TestRegress.Check("crew: the woken LAV takes the next order", moved > 20, string.Format("%1 m", moved));
   Print("[ACR] TEST COMPLETE");
  }
 }
}
