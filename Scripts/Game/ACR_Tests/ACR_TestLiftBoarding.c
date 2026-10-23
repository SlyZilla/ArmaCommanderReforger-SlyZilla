// Grounded pickup fixture: native nine-person rifle squad and two real six-seat
// Hueys. Only flight/landing is staged; soldiers must navigate and board unaided.
class ACR_TestLiftBoarding
{
 static ref ACR_AirLift s_Lift;
 static SCR_AIGroup s_Group;
 static ref array<vector> s_Spots = {};
 static ref array<ChimeraCharacter> s_Overflow = {};
 static ref array<vector> s_OverflowStart = {};
 static float s_fNear;
 static float s_fForce;
 static bool s_bFinished;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || s_bFinished) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (tick == 1)
  {
   if (ACR_LandingZone.Find(h.m_Range.m_Rally, 250, 2, s_Spots) < 2)
   { ACR_TestRegress.Abort("liftboarding", "two clear native pickup spots required"); s_bFinished = true; return; }
   s_Group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab("Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", ACR_Manager.Ground(s_Spots[0] + Vector(0, 0, 22))));
  }
  if (tick == 5)
  {
   ACR_Squad squad = manager.FindSquadByGroup(s_Group);
   if (!squad || ACR_AirLift.Alive(squad) != 9)
   { ACR_TestRegress.Abort("liftboarding", "native nine-man vanilla US rifle squad required"); s_bFinished = true; return; }
   // Fixture placement happens before any boarding task: all nine start on
   // the first carrier's side, with the second carrier over 40 m away.
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   vector away = s_Spots[0] - s_Spots[1];
   away[1] = 0;
   away.Normalize();
   vector across = Vector(away[2], 0, -away[0]);
   foreach (int index, AIAgent agent : agents)
   {
    int row = index / 3;
    int column = index % 3;
    agent.GetControlledEntity().SetOrigin(ACR_Manager.Ground(s_Spots[0] + away * (22 + row * 3) + across * ((column - 1) * 3)));
   }
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   s_Lift = new ACR_AirLift();
   s_Lift.m_Squad = squad;
   s_Lift.m_sFaction = "US";
   s_Lift.m_sSquadName = "Natural boarding fixture";
   s_Lift.m_Pickups.Copy(s_Spots);
   s_Lift.m_Drops.Copy(s_Spots);
   ACR_LiftHeli.s_iTestSeats = 0;
   if (!s_Lift.Begin(manager.m_Supports, GetGame().GetWorld().GetWorldTime()) || s_Lift.m_Helis.Count() != 2)
   { ACR_TestRegress.Abort("liftboarding", "two native carriers failed to spawn"); s_bFinished = true; return; }
   ACR_AirLift.s_Active.Insert(s_Lift);
   s_Lift.TestGroundPickup(manager.m_Supports);
   squad.MoveTo(ACR_Manager.Ground(s_Spots[0] + Vector(0, 0, 15)));
   squad.SetOrder("Air lift: to the landing zone", ACR_EOrderState.MOVE);
   s_fNear = ACR_AirLift.NEAR_BOARD_MS;
   s_fForce = ACR_AirLift.FORCE_BOARD_MS;
   ACR_AirLift.NEAR_BOARD_MS = 600000;
   ACR_AirLift.FORCE_BOARD_MS = 600000;
   ACR_TestRegress.Check("lift boarding: actual nine men and two six-seat Hueys", ACR_AirLift.Alive(squad) == 9 && s_Lift.m_Helis[0].CargoSeats() == 6 && s_Lift.m_Helis[1].CargoSeats() == 6);
   s_Lift.TestReservationDeparture(manager.m_Supports);
  }
  if (tick < 6 || !s_Lift) return;
  s_Lift.TestBoardNaturally();
  if (s_Overflow.IsEmpty()) s_Lift.TestRememberOverflow(s_Overflow, s_OverflowStart);
  int occupants = s_Lift.TestPhysicalOccupants();
  if (occupants == 9 || tick >= 95)
  {
   int walked;
   foreach (int i, ChimeraCharacter character : s_Overflow)
    if (character && vector.DistanceXZ(character.GetOrigin(), s_OverflowStart[i]) > 10) walked++;
   ACR_TestRegress.Check("lift boarding: overflow assigned to second carrier", s_Overflow.Count() == 3, s_Overflow.Count().ToString());
   ACR_TestRegress.Check("lift boarding: overflow physically moved with placement disabled", walked == 3, walked.ToString());
   ACR_TestRegress.Check("lift boarding: all nine physically occupy cargo seats without placement", occupants == 9, occupants.ToString());
   s_Lift.TestReleasePickup();
   ACR_AirLift.NEAR_BOARD_MS = s_fNear;
   ACR_AirLift.FORCE_BOARD_MS = s_fForce;
   s_Lift.Finish();
   s_Lift = null;
   s_bFinished = true;
   ACR_TestRegress.Complete("liftboarding", 6);
  }
 }
}

modded class ACR_AirLift
{
 void TestGroundPickup(ACR_Supports supports)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_LiftHeli heli : m_Helis)
  {
   heli.m_Pos = heli.m_Spot;
   heli.m_bDown = true;
   heli.Step(supports, 0.05, now);
  }
  Stage(BOARDING, now);
 }

 void TestReservationDeparture(ACR_Supports supports)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  m_bSequential = true;
  m_Helis[0].m_iLeg = 1;
  m_Helis[0].m_fLegAt = now;
  m_Helis[1].m_bHold = true;
  SequentialStep(supports, now);
  ACR_TestRegress.Check("lift boarding: six reservations cannot launch an empty chalk", m_Boarding.Count() == 6 && Passengers(m_Helis[0]) == 0 && m_Helis[0].m_iLeg == 1 && m_Helis[0].m_bDown);
  ClearBoarding();
  bool released = m_Boarding.IsEmpty();
  foreach (ACR_LiftHeli carrier : m_Helis)
  {
   array<BaseCompartmentSlot> free = {};
   carrier.FreeSeatList(free);
   if (free.Count() != 6) released = false;
  }
  ACR_TestRegress.Check("lift boarding: cancelled pending tasks release their seat reservations", released);
  m_bSequential = false;
  m_Helis[1].m_bHold = false;
 }

 void TestBoardNaturally() { Board(GetGame().GetWorld().GetWorldTime()); }

 void TestRememberOverflow(notnull array<ChimeraCharacter> members, notnull array<vector> positions)
 {
  foreach (ACR_LiftBoarding pending : m_Boarding)
  {
   if (pending.m_Heli != m_Helis[1]) continue;
   members.Insert(pending.m_Character);
   positions.Insert(pending.m_Character.GetOrigin());
  }
 }

 int TestPhysicalOccupants()
 {
  int count;
  foreach (ACR_LiftHeli carrier : m_Helis)
  {
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(carrier.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartmentsOfType(slots, ECompartmentType.CARGO);
   foreach (BaseCompartmentSlot slot : slots)
    if (slot.GetOccupant() && ACR_Respawn.IsAlive(slot.GetOccupant())) count++;
  }
  return count;
 }

 void TestReleasePickup() { ClearBoarding(); PutOut(); }
}
