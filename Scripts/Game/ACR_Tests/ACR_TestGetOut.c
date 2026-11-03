// -acrGetOutTest 1 (server-capable): a squad that boarded a vehicle crewed by
// another squad (a requisitioned Humvee's driver) must get out on Get out.
// Live 1 Oct MP: the passengers stayed "mounted" after Get out from the panel.
// Then #187, a job given aboard (as ACR_Rides does for riders: the job, then GetOutForJob, whose
// sweeps put out whoever is still aboard 3, 6 and 9 s on), the men's exits not carried out: left
// alone, the sweeps get them out; replaced by Get in before the first sweep, the men boarding
// again are not put out by the old job's sweeps.
class ACR_TestGetOut
{
 static IEntity s_Taxi;
 static int s_iHeld;
 static int s_iReseated;
 static int s_iShort;
 static string s_sJob;

 static int Aboard(ACR_Squad squad)
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

 static void Run(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !harness.m_Range) return;
  ACR_Squad scout = manager.FindSquadByGroup(harness.m_Range.m_Group);
  ACR_Squad alpha = scout;
  if (tick == 1)
  {
   ACR_Squad.s_bNoGetOutFallback = System.IsCLIParam("acrGetOutOldTest");
   PrintFormat("[ACR] GETOUT game waypoint only: %1", ACR_Squad.s_bNoGetOutFallback);
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha) return;
  if (tick == 5)
  {
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("Taxi", "", "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et", true, "", ACR_Manager.Ground(at + Vector(12, 0, 0)), ACR_Manager.Ground(at + Vector(16, 0, 4)));
  }
  if (tick == 8)
  {
   s_Taxi = alpha.FindVehicleNear(alpha.GetAnchor().GetOrigin() + Vector(12, 0, 0), 8);
   PrintFormat("[ACR] GETOUT taxi %1, Alpha get in: %2", s_Taxi != null, s_Taxi && alpha.GetIn(s_Taxi.GetOrigin()));
  }
  if (tick > 8 && tick <= 40 && tick % 5 == 0)
   PrintFormat("[ACR] GETOUT t%1 boarding: aboard %2 | %3", tick, Aboard(alpha), alpha.Describe());
  // Live case: Jacob was in the driver's body (possessed), so the driver had no AI.
  if (tick == 41 && s_Taxi)
  {
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(s_Taxi.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> pilots = {};
   if (seats) seats.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
   IEntity driver;
   if (!pilots.IsEmpty()) driver = pilots[0].GetOccupant();
   AIControlComponent control;
   if (driver) control = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
   if (control) control.DeactivateAI();
   PrintFormat("[ACR] GETOUT driver AI off (like a possessed driver): %1", control != null);
   // ...and he had driven off, leaving the man who didn't fit far behind.
   vector away = ACR_Manager.Ground(s_Taxi.GetOrigin() + Vector(0, 0, 250));
   s_Taxi.SetOrigin(away + Vector(0, 1, 0));
   Physics physics = s_Taxi.GetPhysics();
   if (physics) physics.SetVelocity(vector.Zero);
   PrintFormat("[ACR] GETOUT taxi moved 250 m from the man left behind");
  }
  if (tick == 42)
  {
   PrintFormat("[ACR] GETOUT before Get out: aboard %1 | %2", Aboard(alpha), alpha.Describe());
   PrintFormat("[ACR] GETOUT Get out: %1", alpha.GetOut());
  }
  if (tick > 42 && tick <= 72 && tick % 5 == 0)
   PrintFormat("[ACR] GETOUT t%1 after: aboard %2 | %3", tick, Aboard(alpha), alpha.Describe());
  if (tick == 73)
  {
   int left = Aboard(alpha);
   // getoutold is the control run, the game's Get out waypoint alone (no EnsureGetOut): there the
   // passengers staying aboard is what shows the fallback is needed (#54, as crewold in #56).
   if (ACR_Squad.s_bNoGetOutFallback)
   {
    ACR_TestRegress.Check("control: the game's Get out waypoint alone leaves passengers in another squad's vehicle aboard", left > 0, string.Format("still aboard %1", left));
    ACR_TestRegress.Complete("getout", 1);
    return;
   }
   ACR_TestRegress.Check("passengers in another squad's vehicle get out on Get out", left == 0, string.Format("still aboard %1", left));
  }
  if (ACR_Squad.s_bNoGetOutFallback || !s_Taxi) return;
  // #187: everyone back in (the taxi is still 250 m off), then a minefield ordered aboard.
  if (tick == 74 || tick == 90)
  {
   alpha.Halt("Holding"); // (ends what was left: Get out's checks, the first job)
   PrintFormat("[ACR] GETOUT job t%1: %2 seated", tick, alpha.BoardCargo(s_Taxi, 400, true, true));
  }
  if (tick == 76)
  {
   OrderJob(alpha);
   PrintFormat("[ACR] GETOUT job t76: %1, aboard %2 | %3", s_sJob, Aboard(alpha), alpha.Describe());
  }
  if (tick == 77)
  {
   s_iHeld = Aboard(alpha);
   PrintFormat("[ACR] GETOUT job t77: aboard %1", s_iHeld);
  }
  if (tick >= 79 && tick <= 89)
   PrintFormat("[ACR] GETOUT job t%1: aboard %2 | %3", tick, Aboard(alpha), alpha.Describe());
  if (tick == 89)
   ACR_TestRegress.Check("a job given aboard still sweeps out the men whose exits didn't happen", s_iHeld > 0 && Aboard(alpha) == 0 && alpha.m_MinePlan != null, string.Format("%1; aboard 1 s on %2, 13 s on %3, mine job %4", s_sJob, s_iHeld, Aboard(alpha), alpha.m_MinePlan != null));
  // The second job half a second off the ticks (its sweeps at 95.5, 98.5 and 101.5, between the
  // checks), replaced half a second later by Get in to the taxi (GetIn's own pick once a seat is
  // free; while they sit in all of them it would board the nearest other vehicle).
  if (tick == 92) GetGame().GetCallqueue().CallLater(ACR_TestGetOut.OrderJob, 500, false, alpha);
  if (tick == 93)
  {
   bool boarding = alpha.BoardVehicle(Vehicle.Cast(s_Taxi), s_Taxi.GetOrigin());
   PrintFormat("[ACR] GETOUT job t93: %1, aboard %2, Get in: %3", s_sJob, Aboard(alpha), boarding);
  }
  // The game's AI walks them out about 3 s on anyway (the job let go of the taxi: ForgetVehicles):
  // back in as they come out, 5.5, 6.5 and 7.5 s on, as the Get in boards them, so they are
  // aboard when the second and third sweeps came. Nobody seated may be put out again.
  if (tick >= 99 && tick <= 106)
  {
   int aboard = Aboard(alpha);
   if (s_iReseated - aboard > s_iShort) s_iShort = s_iReseated - aboard;
   PrintFormat("[ACR] GETOUT job t%1: aboard %2 of %3 seated | %4", tick, aboard, s_iReseated, alpha.Describe());
  }
  if (tick >= 98 && tick <= 100)
  {
   s_iReseated += alpha.BoardCargo(s_Taxi, 20, true, true);
   PrintFormat("[ACR] GETOUT job t%1: %2 back in so far", tick, s_iReseated);
  }
  if (tick == 107)
  {
   ACR_TestRegress.Check("a Get in given right after a job's dismount keeps its men aboard past the job's sweeps", s_iReseated > 0 && s_iShort == 0, string.Format("%1; back in 5.5-7.5 s on %2, most of them out again 6.5-13.5 s on %3, aboard now %4 | %5", s_sJob, s_iReseated, s_iShort, Aboard(alpha), alpha.Describe()));
   ACR_TestRegress.Complete("getout", 3);
  }
 }

 // A minefield 120 m off (the first spot of a ring that takes one), then out as a ride's riders
 // given a job aboard get out (ACR_Rides.Step), but nobody's exit carried out: the men still
 // aboard when the sweeps come (an exit refused or never done, as an LAV's cargo seats, #23) are
 // who the sweeps are for, and whom a stale sweep threw out of a Get in.
 static void OrderJob(ACR_Squad alpha)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_FactionData side = manager.m_Bases.GetFaction(alpha.GetFaction().GetFactionKey());
  if (side) side.m_iRP = 500;
  string why;
  bool ordered;
  for (int k = 0; k < 8 && !ordered; k++)
  {
   float angle = Math.PI2 * k / 8;
   ordered = ACR_Minefields.Order(alpha, ACR_EMineKind.AP, ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(Math.Cos(angle) * 120, 0, Math.Sin(angle) * 120)), why);
  }
  s_sJob = string.Format("minefield ordered %1 (%2)", ordered, why);
  if (!ordered) return;
  alpha.GetOutForJob();
  array<AIAgent> agents = {};
  alpha.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle() && character.GetCompartmentAccessComponent())
    character.GetCompartmentAccessComponent().InterruptVehicleActionQueue(true, true, true);
  }
 }
}
