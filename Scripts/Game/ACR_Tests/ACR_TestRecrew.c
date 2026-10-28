// -acrRecrewTest 1 (server-capable, ~70 s): Jacob's 27 Oct game. A Typhoon's driver and gunner
// got out on their own, only the gunner went back, and the AI sent the driverless vehicle on
// move after move for 37 min, each failing where it stood (Recrew only took crews back into an
// empty vehicle). Two armed Humvees: in the first the driver gets out, in the second the gunner.
// The AI commander's Recrew puts the first one's driver back in his seat (the group's Get in
// waypoint seats nobody while the gunner is aboard); the second truck it leaves alone.
class ACR_TestRecrew
{
 static ACR_Squad s_Driverless, s_Gunnerless;
 static IEntity s_DriverlessVehicle, s_GunnerlessVehicle;

 static void Eject(ACR_Squad squad, ECompartmentType seat)
 {
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (body && ACR_Transport.SeatOf(body) == seat) body.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
 }

 static string State(ACR_Squad squad, IEntity vehicle)
 {
  if (!squad || !vehicle) return "missing";
  return string.Format("%1: driver %2, mounted %3, bailed flag %4, order '%5'", squad.GetName(), ACR_Squad.DriverOf(vehicle) != null, squad.IsMounted(), squad.m_bDriverBailed, squad.m_Order);
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (tick == 5)
  {
   manager.SpawnUnit("Recrew Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1390 0 2795"), h.Ground("1395 0 2790"));
   manager.SpawnUnit("Gunner Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1420 0 2795"), h.Ground("1425 0 2790"));
  }
  if (tick == 14)
  {
   s_Driverless = h.NamedSquad(manager, "Recrew Truck");
   s_Gunnerless = h.NamedSquad(manager, "Gunner Truck");
   if (s_Driverless) s_DriverlessVehicle = s_Driverless.m_OwnVehicle;
   if (s_Gunnerless) s_GunnerlessVehicle = s_Gunnerless.m_OwnVehicle;
   if (!s_DriverlessVehicle || !s_GunnerlessVehicle || !ACR_Squad.DriverOf(s_DriverlessVehicle)) { ACR_TestRegress.Abort("recrew", "a truck or its crew is missing"); return; }
   Eject(s_Driverless, ECompartmentType.PILOT);
   Eject(s_Gunnerless, ECompartmentType.TURRET);
  }
  if (!s_Driverless || !s_Gunnerless || tick <= 14) return;
  if (tick % 5 == 0) PrintFormat("[ACR] RECREW t=%1: %2 | %3", tick, State(s_Driverless, s_DriverlessVehicle), State(s_Gunnerless, s_GunnerlessVehicle));
  if (tick == 18)
  {
   bool noticed = s_Driverless.m_bDriverBailed && !ACR_Squad.DriverOf(s_DriverlessVehicle) && s_Driverless.IsMounted() && !s_Gunnerless.m_bDriverBailed;
   ACR_TestRegress.Check("recrew: a driver who got out on his own is noticed (a gunner isn't taken for one)", noticed, State(s_Driverless, s_DriverlessVehicle) + " | " + State(s_Gunnerless, s_GunnerlessVehicle));
   string gunnerOrder = s_Gunnerless.m_Order;
   string driverlessOrder = s_Driverless.m_Order;
   ACR_AICommander ai = manager.FobAICommanderFor("US");
   if (ai) ai.RecrewForTest();
   // The group's Get in seated nobody here (first run: 50 s on 'Getting in'): one man goes for
   // the driver's seat, and both squads keep their orders.
   bool going = s_Driverless.m_Order == driverlessOrder && s_Gunnerless.m_Order == gunnerOrder;
   ACR_TestRegress.Check("recrew: the AI sends a man back to the driver's seat, and leaves the other truck be", going, State(s_Driverless, s_DriverlessVehicle) + " | " + State(s_Gunnerless, s_GunnerlessVehicle));
  }
  // Not in after 30 s: the next try (the AI ticks Recrew) places him.
  if (tick == 50 && s_Driverless.m_bDriverBailed)
  {
   ACR_AICommander retry = manager.FobAICommanderFor("US");
   if (retry) retry.RecrewForTest();
  }
  if (tick == 65)
  {
   bool back = ACR_Squad.DriverOf(s_DriverlessVehicle) != null && !s_Driverless.m_bDriverBailed;
   ACR_TestRegress.Check("recrew: someone drives the truck again and the flag is cleared", back, State(s_Driverless, s_DriverlessVehicle));
   ACR_TestRegress.Complete("recrew", 3);
  }
  if (tick == 67) Print("[ACR] TEST COMPLETE");
 }
}
