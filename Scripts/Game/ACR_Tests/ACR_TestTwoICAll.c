// -acrTwoICAllTest 1 (server-capable, ~3 min): the lazy commander's 2IC (-acr2IC 2). No AI commander
// on either side; the test says a player commands US (ACR_TestHooks.PlayerCommandsForTest). The 2IC
// then runs the whole of US: it may spend the side's RP down to the player's keep (-acr2ICKeep,
// 100), buys squads with it and takes them on, and leaves a squad the player ordered alone. A side
// nobody commands keeps the plain 2IC, and back on -acr2IC 1 it buys nothing.
class ACR_TestTwoICAll
{
 static const int CHECKS = 7;
 static string s_sCommanded;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iSquadsAt;
 static int s_iLowestRP;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Unset("acr2IC");
   ACR_Options.Unset("acr2ICKeep");
   // The range's Farm is a plain base: let the 2IC field squads there.
   ACR_Options.Set("acrDeployment", "all");
   s_sCommanded = "";
   s_iStep = 0;
   s_iLowestRP = 100000;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoicall", "no Alpha or Bravo squad");
   return;
  }
  if (tick > 250) ACR_TestRegress.Abort("twoicall", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (!us || !ai)
  {
   ACR_TestRegress.Abort("twoicall", "no US faction data or AI");
   return;
  }

  if (s_iStep == 0)
  {
   ACR_Options.Set("acr2IC", "2");
   s_sCommanded = "US";
   bool runs = ACR_SquadAI.RunsAll(manager, "US") && !ACR_SquadAI.RunsAll(manager, "USSR");
   ACR_TestRegress.Check("twoicall: -acr2IC 2 runs the whole side only where a player commands", runs, string.Format("US %1, USSR %2", ACR_SquadAI.RunsAll(manager, "US"), ACR_SquadAI.RunsAll(manager, "USSR")));
   us.m_iRP = 400;
   int richAll = ai.PurseForTest(true);
   int richPlain = ai.PurseForTest(false);
   us.m_iRP = 80;
   int poorAll = ai.PurseForTest(true);
   ACR_TestRegress.Check("twoicall: it may spend the side's RP down to the player's 100 RP keep", richAll == 300 && richPlain == 400 && poorAll == 0, string.Format("400 RP: %1 (as the AI in command %2); 80 RP: %3", richAll, richPlain, poorAll));
   // A player's order claims Alpha: the 2IC must leave it alone from here on.
   ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MOVE_TO);
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-15, 0, 0));
   us.m_iRP = 600;
   s_iSquadsAt = CountSide(manager, "US");
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   s_iLowestRP = Math.Min(s_iLowestRP, us.m_iRP);
   int now = CountSide(manager, "US");
   if (now <= s_iSquadsAt && waited < 100) return;
   ACR_TestRegress.Check("twoicall: the 2IC buys squads for the side", now > s_iSquadsAt, string.Format("US squads %1 -> %2 after %3 s, %4 RP left", s_iSquadsAt, now, waited, us.m_iRP));
   ACR_TestRegress.Check("twoicall: its purchases never take the side under the player's keep", s_iLowestRP >= ACR_SquadAI.TwoICKeep(), string.Format("lowest %1 RP (keep %2)", s_iLowestRP, ACR_SquadAI.TwoICKeep()));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   // The bought squad is adopted on the next turn.
   int adopted = 0;
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad && squad != alpha && squad != bravo && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US" && squad.m_bAICommanded) adopted++;
   if (adopted == 0 && waited < 15) return;
   ACR_TestRegress.Check("twoicall: it takes on the squads it bought", adopted > 0, string.Format("%1 bought squad(s) the 2IC's after %2 s", adopted, waited));
   ACR_TestRegress.Check("twoicall: a squad the player ordered stays the player's", !alpha.m_bAICommanded && alpha.m_bClaimed && bravo.m_bAICommanded, string.Format("Alpha handed %1 claimed %2, Bravo handed %3", alpha.m_bAICommanded, alpha.m_bClaimed, bravo.m_bAICommanded));
   // Back to the plain 2IC: it buys nothing.
   ACR_Options.Set("acr2IC", "1");
   us.m_iRP = 600;
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // A purchase already under way may still arrive: count from 10 s on.
   if (waited == 10) s_iSquadsAt = CountSide(manager, "US");
   if (waited < 50) return;
   int after = CountSide(manager, "US");
   ACR_TestRegress.Check("twoicall: the plain 2IC (-acr2IC 1) buys nothing", after <= s_iSquadsAt && us.m_iRP >= 600, string.Format("US squads %1 -> %2, %3 RP", s_iSquadsAt, after, us.m_iRP));
   s_sCommanded = "";
   ACR_Options.Unset("acr2IC");
   ACR_Options.Unset("acrDeployment");
   ACR_TestRegress.Complete("twoicall", CHECKS);
   NextStep(tick);
  }
 }

 protected static int CountSide(ACR_Manager manager, string faction)
 {
  int count;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction && squad.CountAlive() > 0) count++;
  return count;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
