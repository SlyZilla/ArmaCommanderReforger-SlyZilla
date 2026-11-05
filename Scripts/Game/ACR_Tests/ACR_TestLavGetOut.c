// -acrLavGetOutTest 1 (server-capable, ~2 min; -acrLavGetOutOldTest 1 is the control, without the
// drop's sweeps): a player's Get out from an LAV-25 (pn10b, 26 Oct: two of Fire Team 2 stayed in its
// cargo seats, "check 1-5, 2 still aboard, 0 sent out", until the stuck-soldier fallback set them
// down). As the player did: Alpha gets in a crewed LAV-25 with its turret manned (the map panel's
// Get in), the LAV drives about 200 m and stops, its own crew is given Get out (BBB may then seat
// one of Alpha's men in the empty turret, as it did in the playtest), then Alpha is given Get out
// (ACR_Squad.GetOut, the map panel's order): every soldier of Alpha's must be out within 25 s. The
// control passes when someone is still aboard by then, i.e. when the case reproduces.
class ACR_TestLavGetOut
{
 static const int CHECKS = 1;
 static const int LIMIT_S = 25;
 static ACR_Squad s_Lav;
 static IEntity s_Vehicle;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iAboardMax;
 static vector s_vFrom;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  bool old = System.IsCLIParam("acrLavGetOutOldTest");
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Squad.s_bNoGetOutSweeps = old;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick > 220) ACR_TestRegress.Abort("lavgetout", string.Format("timed out in step %1", s_iStep));
  if (tick == 4)
  {
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("LAV 1", "", ACR_TestRideLav.LAV, true, "", ACR_Manager.Ground(at + Vector(15, 0, 0)), ACR_Manager.Ground(at + Vector(20, 0, 5)));
   return;
  }
  if (tick < 10) return;
  if (!s_Lav)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "LAV 1") s_Lav = squad;
   if (!s_Lav) return;
  }
  int waited = tick - s_iStepAt;
  int aboard = ACR_TestGetOut.Aboard(alpha);
  if (aboard > s_iAboardMax) s_iAboardMax = aboard;

  // 0: Alpha gets in.
  if (s_iStep == 0)
  {
   array<IEntity> vehicles = {};
   s_Lav.GetVehicles(vehicles);
   if (vehicles.IsEmpty()) return;
   s_Vehicle = vehicles[0];
   PrintFormat("[ACR] LAVGETOUT sweeps %1; Alpha get in: %2", !old, alpha.GetIn(s_Vehicle.GetOrigin()));
   Next(tick);
   return;
  }
  // 1: aboard (all of Alpha, or as many as get in within 60 s), then the LAV drives off.
  if (s_iStep == 1)
  {
   if (aboard < alpha.CountAlive() && waited < 60) return;
   if (aboard == 0)
   {
    ACR_TestRegress.Abort("lavgetout", "Alpha never got in");
    return;
   }
   s_vFrom = s_Vehicle.GetOrigin();
   vector road = RoadAhead(s_vFrom);
   bool driving = s_Lav.RouteTo(road, ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
   PrintFormat("[ACR] LAVGETOUT %1 of %2 aboard after %3 s (in: %4); the LAV drives to %5, %6 m: %7", aboard, alpha.CountAlive(), waited, Seats(alpha), road, (int)vector.DistanceXZ(s_vFrom, road), driving);
   Next(tick);
   return;
  }
  // 2: driven 50 m or more and stopped (or 90 s on), then the LAV's own crew gets out.
  if (s_iStep == 2)
  {
   float moved = vector.DistanceXZ(s_vFrom, s_Vehicle.GetOrigin());
   float speed;
   if (s_Vehicle.GetPhysics()) speed = s_Vehicle.GetPhysics().GetVelocity().Length();
   if ((moved < 50 || speed > 0.5) && waited < 90) return;
   bool crewOut = s_Lav.GetOut();
   PrintFormat("[ACR] LAVGETOUT drove %1 m in %2 s; %3 aboard (in: %4); the crew's Get out: %5", (int)moved, waited, aboard, Seats(alpha), crewOut);
   Next(tick);
   return;
  }
  // 3: the crew out (or 20 s on), then Alpha's Get out.
  if (s_iStep == 3)
  {
   if (ACR_TestGetOut.Aboard(s_Lav) > 0 && waited < 20) return;
   bool leaving = alpha.GetOut();
   PrintFormat("[ACR] LAVGETOUT crew aboard %1 (in: %2) after %3 s; Alpha %4 aboard (in: %5); Alpha's Get out: %6", ACR_TestGetOut.Aboard(s_Lav), Seats(s_Lav), waited, aboard, Seats(alpha), leaving);
   Next(tick);
   return;
  }
  // 4: everyone out within LIMIT_S.
  if (s_iStep == 4)
  {
   if (aboard > 0 && waited < LIMIT_S) return;
   string detail = string.Format("%1 s after Get out: %2 of Alpha's soldiers still aboard (most aboard %3), still in: %4; the LAV's crew in: %5; order '%6'", waited, aboard, s_iAboardMax, Seats(alpha), Seats(s_Lav), alpha.m_Order);
   if (old) ACR_TestRegress.Check("lavgetout (control, the game's exits only): soldiers stay in the LAV-25 after Get out", aboard > 0, detail);
   else ACR_TestRegress.Check("lavgetout: a Get out order empties an LAV-25's cargo seats within 25 s", aboard == 0, detail);
   ACR_TestRegress.Complete("lavgetout", CHECKS);
   Next(tick);
  }
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // A road point 150-350 m from 'from' (the first found round it), or 200 m east.
 protected static vector RoadAhead(vector from)
 {
  for (int k = 0; k < 24; k++)
  {
   int slot = k % 12;
   int ring = k / 12;
   float angle = Math.PI2 * slot / 12;
   float reach = 200 + ring * 120;
   vector road;
   if (BBB_Terrain.RoadPointNear(from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach), 80, road) && vector.DistanceXZ(from, road) > 150) return road;
  }
  return ACR_Manager.Ground(from + Vector(200, 0, 0));
 }

 // The compartment types the squad's men aboard sit in.
 protected static string Seats(ACR_Squad squad)
 {
  string seats = "";
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
   if (slot) seats += typename.EnumToString(ECompartmentType, slot.GetType()) + " ";
  }
  if (seats.IsEmpty()) return "none";
  return seats;
 }
}
