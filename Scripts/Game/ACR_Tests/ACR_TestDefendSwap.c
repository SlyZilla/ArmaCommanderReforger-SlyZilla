// -acrDefendSwapTest 1 (server-capable, ~1 min): a new order given while a Defend activity runs
// (crash.log, 25 Oct playtests: "Defend Waypoint not provided to the node!" and "Wrong class of
// provided Waypoint!" from vanilla's Defend nodes, which read a waypoint already replaced). Alpha
// is ordered to defend a spot; once its group's Defend activity runs, it is ordered to move on in
// the same second, and back, CYCLES times. The watcher fails the run on any such node error; the
// check counts the swaps made from a running Defend.
class ACR_TestDefendSwap
{
 static const int CYCLES = 12;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iSwaps;
 static int s_iCycle;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || tick < 8) return;
  if (tick > 150) ACR_TestRegress.Abort("defendswap", string.Format("timed out in cycle %1", s_iCycle));
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   alpha.Defend(ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(15, 0, 0)));
   s_iStep = 1;
   s_iStepAt = tick;
   return;
  }
  if (s_iStep == 1)
  {
   // The group's Defend activity running: the new order now.
   if (!Defending(alpha) && waited < 10) return;
   if (Defending(alpha)) s_iSwaps++;
   alpha.MoveTo(ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(0, 0, 20)));
   s_iCycle++;
   s_iStep = 2;
   s_iStepAt = tick;
   return;
  }
  if (s_iStep == 2)
  {
   if (waited < 2) return;
   if (s_iCycle >= CYCLES)
   {
    ACR_TestRegress.Check("defendswap: a new order given while the group's Defend activity runs (no Defend node errors: the watcher fails the run on one)", s_iSwaps >= CYCLES / 2, string.Format("%1 of %2 cycles swapped from a running Defend", s_iSwaps, CYCLES));
    ACR_TestRegress.Complete("defendswap", 1);
    s_iStep = 3;
    return;
   }
   s_iStep = 0;
  }
 }

 protected static bool Defending(ACR_Squad squad)
 {
  if (!squad.m_Group) return false;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  return utility && SCR_AIDefendActivity.Cast(utility.GetCurrentAction()) != null;
 }
}
