// -acrRideLavTest 1 (server-capable, ~2 min; -acrRideTruckTest 1 for the truck control): a ride's
// passengers get out at its end in an LAV as in a truck (25 Oct test runs: Alpha's men stayed in an
// LAV taxi, "GET OUT" issued again and again, while a truck's got out). A taxi carries Alpha about
// 500 m down a road (the LAV's turret crew taken out first: rides never take a vehicle with a
// manned gun); once the ride is over, every soldier of Alpha's must be out within 45 s.
// How the turret is emptied (Jacob, #23: deleting the crew may be what re-tasks the driver):
// -acrRideLavTest deletes them, -acrRideLavKillTest kills them in their seats (as a fight would)
// and -acrRideLavGetOutTest has them get out through the game's own exit (as an order would).
class ACR_TestRideLav
{
 static const int CHECKS = 1;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName LAV = "{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et";
 static ACR_Squad s_Taxi;
 static vector s_Dest;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iAboardMax;
 static string s_sDetail;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor()) return;
  string kind = "LAV";
  ResourceName prefab = LAV;
  if (System.IsCLIParam("acrRideTruckTest"))
  {
   kind = "truck";
   prefab = TRUCK;
  }
  else if (System.IsCLIParam("acrRideLavKillTest")) kind = "LAV, turret crew killed";
  else if (System.IsCLIParam("acrRideLavGetOutTest")) kind = "LAV, turret crew got out";
  if (tick > 330) ACR_TestRegress.Abort("ridelav", string.Format("%1: timed out in step %2 (%3)", kind, s_iStep, s_sDetail));
  int waited = tick - s_iStepAt;
  if (tick == 4)
  {
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("Taxi 1", "", prefab, true, "", ACR_Manager.Ground(at + Vector(25, 0, 15)), ACR_Manager.Ground(at + Vector(30, 0, 20)));
   return;
  }
  if (tick < 10) return;
  if (!s_Taxi)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Taxi 1") s_Taxi = squad;
   if (!s_Taxi) return;
  }

  // 0: the ride; 1: on the way until it is over; 2: everyone out within 45 s.
  if (s_iStep == 0)
  {
   if (prefab == LAV && TurretCrewOut(h, s_Taxi, tick) > 0) return;
   s_Dest = Destination(alpha);
   if (s_Dest == vector.Zero)
   {
    ACR_TestRegress.Abort("ridelav", "no road destination");
    return;
   }
   alpha.Halt("Holding");
   string why;
   ACR_Ride ride = manager.m_Rides.Request(alpha, s_Dest, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   PrintFormat("[ACR] RIDELAV %1: ride requested %2 (%3) to %4, %5 m", kind, ride != null, why, s_Dest, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest));
   if (!ride)
   {
    ACR_TestRegress.Abort("ridelav", string.Format("%1: no ride (%2)", kind, why));
    return;
   }
   NextStep(tick);
   return;
  }
  int aboard = Aboard(alpha);
  if (aboard > s_iAboardMax) s_iAboardMax = aboard;
  if (s_iStep == 1)
  {
   ACR_Ride on = manager.m_Rides.RideOf(alpha);
   s_sDetail = string.Format("ride state %1, %2 aboard, %3 m from the drop", RideState(on), aboard, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest));
   if (on && waited < 220) return;
   PrintFormat("[ACR] RIDELAV %1: ride over after %2 s (%3; most aboard %4)", kind, waited, s_sDetail, s_iAboardMax);
   NextStep(tick);
   return;
  }
  if (s_iStep == 2)
  {
   if (aboard > 0 && waited < 45) return;
   bool gotOut = s_iAboardMax > 0 && aboard == 0;
   s_sDetail += ", still aboard in: " + Seats(alpha) + "; the LAV's crew in: " + Seats(s_Taxi);
   ACR_TestRegress.Check(string.Format("ridelav: a ride's passengers get out at its end (%1)", kind), gotOut, string.Format("%1; %2 s on: %3 of Alpha's soldiers still aboard (most aboard %4), order '%5', %6 m from the drop", s_sDetail, waited, aboard, s_iAboardMax, alpha.m_Order, (int)vector.DistanceXZ(alpha.GetAnchor().GetOrigin(), s_Dest)));
   ACR_TestRegress.Complete("ridelav", CHECKS);
   NextStep(tick);
  }
 }

 // The taxi crew's live men in turret seats taken out (a ride never takes a vehicle with a manned
 // gun): deleted, killed or told to get out (see the header); how many are still in. A getting-out
 // that never ends aborts the test.
 static bool s_bExitAsked;
 static int s_iExitAt;
 protected static int TurretCrewOut(ACR_TestHarness h, ACR_Squad taxi, int tick)
 {
  bool kill = System.IsCLIParam("acrRideLavKillTest");
  bool getOut = System.IsCLIParam("acrRideLavGetOutTest");
  if (getOut && s_bExitAsked && tick - s_iExitAt > 40) ACR_TestRegress.Abort("ridelav", "the turret crew never got out");
  int count;
  array<AIAgent> agents = {};
  taxi.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
   if (!slot || slot.GetType() != ECompartmentType.TURRET || !ACR_Respawn.IsAlive(character)) continue;
   count++;
   if (kill) h.Kill(character);
   else if (!getOut) SCR_EntityHelper.DeleteEntityAndChildren(character);
   else if (!s_bExitAsked) character.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
  }
  if (count > 0 && (!getOut || !s_bExitAsked)) PrintFormat("[ACR] RIDELAV LAV: %1 turret crew %2", count, TurretVerb(kill, getOut));
  if (getOut && count > 0 && !s_bExitAsked)
  {
   s_bExitAsked = true;
   s_iExitAt = tick;
  }
  return count;
 }

 protected static string TurretVerb(bool kill, bool getOut)
 {
  if (kill) return "killed in their seats";
  if (getOut) return "told to get out";
  return "deleted from the taxi";
 }

 // The compartment types Alpha's men still aboard sit in (cargo, turret, pilot).
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
   else seats += "? ";
  }
  return seats;
 }

 protected static int Aboard(ACR_Squad squad)
 {
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle()) count++;
  }
  return count;
 }

 protected static string RideState(ACR_Ride ride)
 {
  if (!ride) return "over";
  return typename.EnumToString(ACR_ERideState, ride.m_State);
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // A road point 450-850 m out that the road route reaches without a long detour.
 protected static vector Destination(ACR_Squad alpha)
 {
  vector from = alpha.GetAnchor().GetOrigin();
  for (int k = 0; k < 48; k++)
  {
   int slot = k % 24;
   int ring = k / 24;
   float angle = Math.PI2 * slot / 24;
   float reach = 450 + ring * 200;
   vector probe = from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
   vector road;
   if (!BBB_Terrain.RoadPointNear(probe, 120, road)) continue;
   array<vector> legs = {};
   int nodes, millis;
   BBB_Roads.Route(from, road, legs, nodes, millis);
   if (BBB_Roads.s_bLastDetour || BBB_Roads.s_fLastLength > vector.DistanceXZ(from, road) * 1.6) continue;
   return road;
  }
  return vector.Zero;
 }
}
