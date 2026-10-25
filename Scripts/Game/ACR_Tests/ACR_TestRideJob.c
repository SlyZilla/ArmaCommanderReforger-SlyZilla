// -acrRideJobTest 1 (server-capable, ~2 min): a job ordered to a squad still aboard a ride (playtest
// 25 Oct: an AI minefield order to a squad on a truck was paid, then cancelled and refunded in the
// same second as the ride ended). A truck taxi carries Alpha towards a road point; once Alpha is
// aboard it is ordered to lay an AP minefield (as a player's order does): the ride ends, Alpha
// gets out, and the minefield order must still stand, paid. Then a second ride, and a roadblock
// order the same way; a third, and a Fortify order (review of #18: it was still refunded).
class ACR_TestRideJob
{
 static const int CHECKS = 3;
 static ACR_Squad s_Taxi;
 static vector s_Dest;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRP;
 static string s_sDetail;

 static ACR_Squad Crew(ACR_Manager manager)
 {
  if (!s_Taxi)
  {
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Taxi 1") s_Taxi = squad;
  }
  return s_Taxi;
 }

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
  if (tick > 300) ACR_TestRegress.Abort("ridejob", string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;
  if (tick == 4)
  {
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("Taxi 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", ACR_Manager.Ground(at + Vector(25, 0, 15)), ACR_Manager.Ground(at + Vector(30, 0, 20)));
   return;
  }
  if (tick < 10) return;
  ACR_Squad crew = Crew(manager);
  if (!crew) return;
  us.m_iRP = 500;

  // Steps 0 / 3 / 6: a ride for Alpha; 1 / 4 / 7: once aboard, the job; 2 / 5 / 8: 15 s on, the
  // job must stand.
  if (s_iStep == 0 || s_iStep == 3 || s_iStep == 6)
  {
   if (s_Dest == vector.Zero) s_Dest = Destination(alpha);
   if (s_Dest == vector.Zero)
   {
    ACR_TestRegress.Abort("ridejob", "no road destination");
    return;
   }
   manager.m_Rides.CancelFor(alpha);
   manager.m_Rides.CancelFor(crew);
   alpha.Halt("Holding");
   crew.Halt("Holding");
   string why;
   ACR_Ride ride = manager.m_Rides.Request(alpha, s_Dest, 0, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   PrintFormat("[ACR] RIDEJOB step %1: ride requested: %2 (%3)", s_iStep, ride != null, why);
   NextStep(tick);
   return;
  }
  if (s_iStep == 1 || s_iStep == 4 || s_iStep == 7)
  {
   s_sDetail = string.Format("Alpha '%1' aboard %2", alpha.m_Order, alpha.IsMountedAsCargo());
   if (!alpha.IsMountedAsCargo() && waited < 90) return;
   int before = us.m_iRP;
   string why;
   bool ordered;
   if (s_iStep == 1)
   {
    vector field = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(0, 0, -120));
    ordered = ACR_Minefields.Order(alpha, ACR_EMineKind.AP, field, why);
   }
   else if (s_iStep == 4)
   {
    vector site;
    float yaw;
    ordered = ACR_Roadblocks.ValidateSite(s_Dest, site, yaw, why, "US") && alpha.EstablishRoadblock(site, yaw, ACR_ERoadblockType.MEDIUM, false);
   }
   else
   {
    // A spot off the road the Fortify footprint fits (the first of a ring round the drop point).
    for (int k = 0; k < 12 && !ordered; k++)
    {
     float angle = Math.PI2 * k / 12;
     ordered = ACR_Fortify.Order(alpha, ACR_Manager.Ground(s_Dest + Vector(Math.Cos(angle) * 40, 0, Math.Sin(angle) * 40)), why);
    }
   }
   s_iRP = us.m_iRP;
   s_sDetail = string.Format("ordered aboard %1: %2 (%3), rp %4 -> %5", alpha.IsMountedAsCargo(), ordered, why, before, s_iRP);
   PrintFormat("[ACR] RIDEJOB step %1: %2", s_iStep, s_sDetail);
   NextStep(tick);
   return;
  }
  if (s_iStep == 2 || s_iStep == 5 || s_iStep == 8)
  {
   if (waited < 15) return;
   // (A refund is what ends a paid job: the job still being there means it wasn't refunded. RP
   // alone can't say: income ticks.)
   bool riding = manager.m_Rides.RideOf(alpha) != null;
   bool gotOut = !alpha.IsMountedAsCargo();
   string now = string.Format("%1; 15 s on: order '%2', aboard %3, ride %4, rp %5", s_sDetail, alpha.m_Order, !gotOut, riding, us.m_iRP);
   if (s_iStep == 2)
   {
    ACR_TestRegress.Check("ridejob: a minefield ordered to a squad aboard a ride still stands once it is out, paid", alpha.m_MinePlan != null && gotOut, now);
    alpha.Halt("Holding");
   }
   else if (s_iStep == 5)
   {
    ACR_TestRegress.Check("ridejob: a roadblock ordered to a squad aboard a ride still stands once it is out, paid", (alpha.RoadblockPending() || alpha.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK) && gotOut, now);
    alpha.Halt("Holding");
   }
   else
   {
    ACR_TestRegress.Check("ridejob: a Fortify order given to a squad aboard a ride still stands once it is out, paid", alpha.m_FortifyPlan != null && gotOut, now);
    alpha.Halt("Holding");
    ACR_TestRegress.Complete("ridejob", CHECKS);
   }
   NextStep(tick);
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

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
