// -acrPatrolTest 1 (server-capable, ~6 min): the Patrol standing order. Refused with no point;
// Alpha moves to A, B and C are queued, Patrol loops them (A joins the loop at the end), Alpha
// walks A, B, C and back to A with the order still on and the loop intact, and a new move order
// ends it. Then the playtest flow (25 Oct): Alpha arrives and holds, a Shift + right-click sends
// it on at once (it used to queue behind the hold for good) and Patrol loops it; and a squad on
// its way to one point patrols between where it stood and that point; switched off mid-loop, it
// finishes the current leg and stops.
class ACR_TestPatrol
{
 static const int CHECKS = 8;
 static int s_iStep;
 static int s_iStepAt;
 static ref array<vector> s_aPoints = {};
 static int s_iNext;
 static vector s_vStart;
 static vector s_vLeg;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("patrol", "no Alpha squad");
   return;
  }
  if (tick > 560 && s_iStep < 6) ACR_TestRegress.Abort("patrol", string.Format("timed out in step %1 (next point %2, order '%3')", s_iStep, s_iNext, alpha.m_Order));
  if (tick < 5) return;
  int waited = tick - s_iStepAt;
  string why;

  if (s_iStep == 0)
  {
   vector home = alpha.GetAnchor().GetOrigin();
   s_aPoints.Insert(ACR_Manager.Ground(home + Vector(30, 0, 0)));
   s_aPoints.Insert(ACR_Manager.Ground(home + Vector(30, 0, 30)));
   s_aPoints.Insert(ACR_Manager.Ground(home + Vector(0, 0, 30)));
   // Nothing to walk: refused.
   alpha.Halt("Halted");
   bool alone = !alpha.TogglePatrol(why) && !alpha.m_bPatrol;
   ACR_TestRegress.Check("patrol: refused with no point to walk", alone, why);
   // A, then B and C queued: the loop is B, C, A.
   alpha.MoveTo(s_aPoints[0]);
   alpha.Queue(s_aPoints[1], false);
   alpha.Queue(s_aPoints[2], true);
   bool on = alpha.TogglePatrol(why) && alpha.m_bPatrol;
   array<vector> planned = {};
   array<int> kinds = {};
   alpha.GetPlannedPoints(planned, kinds);
   bool loop = planned.Count() == 3 && vector.DistanceXZ(planned[2], s_aPoints[0]) < 0.5;
   ACR_TestRegress.Check("patrol: on, with the point it is heading for added to the loop", on && loop, string.Format("%1, %2 planned points", why, planned.Count()));
   s_iNext = 0;
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   // The leader through A, B, C and A again (a point counts within 12 m).
   if (s_iNext < 4 && Reached(alpha, s_aPoints[s_iNext % 3], tick)) s_iNext++;
   if (s_iNext < 4 && waited < 150) return;
   array<vector> planned = {};
   array<int> kinds = {};
   alpha.GetPlannedPoints(planned, kinds);
   ACR_TestRegress.Check("patrol: walks A, B, C and back to A, still patrolling", s_iNext >= 4 && alpha.m_bPatrol && planned.Count() == 3 && alpha.m_Order == "Patrolling", string.Format("reached %1 of 4, patrol %2, %3 planned, order '%4'", s_iNext, alpha.m_bPatrol, planned.Count(), alpha.m_Order));
   // A new order ends it.
   alpha.MoveTo(s_aPoints[1]);
   planned.Clear();
   alpha.GetPlannedPoints(planned, kinds);
   ACR_TestRegress.Check("patrol: a new order ends it", !alpha.m_bPatrol && planned.IsEmpty() && alpha.m_Order == "Moving", string.Format("patrol %1, %2 planned, order '%3'", alpha.m_bPatrol, planned.Count(), alpha.m_Order));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   // The playtest flow: it arrives at B and holds there...
   if (!alpha.HoldingInPlace() && waited < 60) return;
   bool holding = alpha.HoldingInPlace();
   string held = alpha.m_Order;
   // ...a Shift + right-click on C sends it at once; A queued after it; then Patrol.
   alpha.Queue(s_aPoints[2], false);
   bool going = alpha.m_Order == "Moving" && vector.DistanceXZ(alpha.m_MoveTarget, s_aPoints[2]) < 0.5;
   alpha.Queue(s_aPoints[0], false);
   bool on = alpha.TogglePatrol(why) && alpha.m_bPatrol;
   ACR_TestRegress.Check("patrol: a squad holding where it arrived goes at once on a Shift + right-click", holding && going && on, string.Format("holding %1 (order '%2'), going to the new point %3, patrol %4 (%5)", holding, held, going, on, why));
   s_iNext = 0;
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // ...and it walks C, A, C (it used to stand still).
   array<vector> loop = {s_aPoints[2], s_aPoints[0], s_aPoints[2]};
   if (s_iNext < 3 && Reached(alpha, loop[s_iNext], tick)) s_iNext++;
   if (s_iNext < 3 && waited < 100) return;
   ACR_TestRegress.Check("patrol: after arriving, Shift-added points and Patrol: it walks the loop", s_iNext >= 3 && alpha.m_bPatrol && alpha.m_Order == "Patrolling", string.Format("reached %1 of 3, patrol %2, order '%3'", s_iNext, alpha.m_bPatrol, alpha.m_Order));
   // One point only: on its way to B, Patrol walks between where it stands and B.
   s_vStart = alpha.GetAnchor().GetOrigin();
   alpha.MoveTo(s_aPoints[1]);
   bool on = alpha.TogglePatrol(why) && alpha.m_bPatrol;
   PrintFormat("[ACR] PATROL TEST one point: patrol %1 (%2)", on, why);
   s_iNext = 0;
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   array<vector> shuttle = {s_aPoints[1], s_vStart, s_aPoints[1]};
   if (s_iNext < 3 && Reached(alpha, shuttle[s_iNext], tick)) s_iNext++;
   if (s_iNext < 3 && waited < 100) return;
   ACR_TestRegress.Check("patrol: with one point it walks between where it stood and that point", s_iNext >= 3 && alpha.m_bPatrol, string.Format("reached %1 of 3, patrol %2, order '%3'", s_iNext, alpha.m_bPatrol, alpha.m_Order));
   // Off mid-loop (#65 review): it finishes the leg it is on, then stops (it used to walk a lap more).
   s_vLeg = alpha.m_MoveTarget;
   alpha.TogglePatrol(why);
   s_iNext = 0;
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   if (s_iNext == 0 && Reached(alpha, s_vLeg, tick)) { s_iNext = 1; s_iStepAt = tick; return; }
   if (s_iNext == 0 && waited < 100) return;
   // Arrived: a few seconds on, it hasn't set off for another point.
   if (s_iNext == 1 && waited < 10) return;
   array<vector> planned = {};
   array<int> kinds = {};
   alpha.GetPlannedPoints(planned, kinds);
   bool stopped = s_iNext == 1 && !alpha.m_bPatrol && planned.IsEmpty() && vector.DistanceXZ(alpha.m_MoveTarget, s_vLeg) < 0.5;
   ACR_TestRegress.Check("patrol: off mid-loop, it finishes the current leg and stops there", stopped, string.Format("reached the leg's end %1, patrol %2, %3 planned, order '%4'", s_iNext == 1, alpha.m_bPatrol, planned.Count(), alpha.m_Order));
   alpha.Halt("Halted");
   ACR_TestRegress.Complete("patrol", CHECKS);
   s_iStep = 6;
  }
 }

 protected static bool Reached(ACR_Squad squad, vector point, int tick)
 {
  IEntity leader = squad.GetLeader();
  if (!leader || vector.DistanceXZ(leader.GetOrigin(), point) >= 12) return false;
  PrintFormat("[ACR] PATROL test: reached %1 at tick %2, order '%3'", point, tick, squad.m_Order);
  return true;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
