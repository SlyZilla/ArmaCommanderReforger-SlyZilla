// -acrSquadAITest 1 (server-capable, ~2 min): hand a squad to the AI. No AI commander on either
// side (as when players command both). Nothing is handed by default; Alpha handed over gets an
// AI task while Bravo is left alone; the map shows Alpha as [AI] (and its panel flag); an
// order of the player's takes it back (a mode change doesn't) and the AI leaves it alone
// after; the toggle takes it back halted; a handed squad that is wiped out is dropped cleanly.
class ACR_TestSquadAI
{
 static const int CHECKS = 8;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iBravoSerial;
 static int s_iAlphaSerial;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !alpha.GetAnchor() || (!bravo && s_iStep < 3))
  {
   if (tick == 60) ACR_TestRegress.Abort("squadai", "no Alpha or Bravo squad");
   return;
  }
  if (tick > 180) ACR_TestRegress.Abort("squadai", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   ACR_TestRegress.Check("squadai: nothing is handed to the AI by default", !alpha.m_bAICommanded && !bravo.m_bAICommanded && !ACR_SquadAI.AnyFor("US"), "");
   string why;
   bool handed = ACR_SquadAI.CanHand(alpha, why);
   ACR_SquadAI.Hand(alpha);
   s_iBravoSerial = bravo.m_iOrderSerial;
   PrintFormat("[ACR] SQUADAI TEST Alpha handed: %1 (%2), task %3", handed, why, alpha.m_iAITask);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   bool tasked = alpha.m_iAITask != ACR_EAITask.FREE;
   if (!tasked && waited < 50) return;
   bool bravoAlone = !bravo.m_bAICommanded && bravo.m_iAITask == ACR_EAITask.FREE && bravo.m_iOrderSerial == s_iBravoSerial;
   ACR_TestRegress.Check("squadai: handed over, Alpha gets an AI task; Bravo is left alone", tasked && bravoAlone, string.Format("Alpha task %1 after %2 s, order '%3'; Bravo task %4, serial %5 -> %6", typename.EnumToString(ACR_EAITask, alpha.m_iAITask), waited, alpha.m_Order, typename.EnumToString(ACR_EAITask, bravo.m_iAITask), s_iBravoSerial, bravo.m_iOrderSerial));
   // The map: Alpha as [AI], its panel flag; Bravo neither.
   ACR_FeedFrame frame = ACR_MapFeed.Build(manager, GetGame().GetFactionManager().GetFactionByKey("US"));
   bool alphaTag, bravoTag, alphaFlag;
   foreach (int i, string groupName : frame.m_GroupNames)
   {
    if (groupName.StartsWith(alpha.GetName())) { alphaTag = groupName.Contains("[AI]"); if (i * 4 + 3 < frame.m_Symbols.Count()) alphaFlag = (frame.m_Symbols[i * 4 + 3] & 1024) != 0; }
    if (groupName.StartsWith(bravo.GetName())) bravoTag = groupName.Contains("[AI]");
   }
   ACR_TestRegress.Check("squadai: the map shows Alpha as [AI] (and the panel flag), Bravo not", alphaTag && alphaFlag && !bravoTag, string.Format("Alpha tag %1 flag %2, Bravo tag %3", alphaTag, alphaFlag, bravoTag));
   // A mode change keeps it with the AI; an order takes it back.
   bool modeKeeps = !ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MODE_FAST) && alpha.m_bAICommanded;
   // The AI ran it Fast (its attacks and rides do); handed back, it is on its own mode again.
   ACR_EMoveMode modeBefore = ACR_EMoveMode.COMBAT;
   alpha.SetMode(ACR_EMoveMode.FAST);
   bool orderTakes = ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MOVE_TO) && !alpha.m_bAICommanded && alpha.m_iAITask == ACR_EAITask.FREE;
   ACR_TestRegress.Check("squadai: taken back, it is on the mode it had before the AI (not the AI's Fast)", alpha.m_Mode == modeBefore, string.Format("mode %1", typename.EnumToString(ACR_EMoveMode, alpha.m_Mode)));
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-15, 0, 0));
   s_iAlphaSerial = alpha.m_iOrderSerial;
   ACR_AICommander ai = manager.FobAICommanderFor("US");
   bool forgotten = ai && !ai.PlansFor(alpha);
   ACR_TestRegress.Check("squadai: an order takes it back (a mode change doesn't), out of the AI's plans", modeKeeps && orderTakes && forgotten, string.Format("mode keeps %1, order takes %2, forgotten %3", modeKeeps, orderTakes, forgotten));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   // Taken back, the AI leaves it alone (two strategy ticks).
   if (waited < 32) return;
   ACR_TestRegress.Check("squadai: taken back, the AI leaves it alone", !alpha.m_bAICommanded && alpha.m_iAITask == ACR_EAITask.FREE && alpha.m_iOrderSerial == s_iAlphaSerial, string.Format("task %1, order '%2', serial %3 -> %4", typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_Order, s_iAlphaSerial, alpha.m_iOrderSerial));
   // The toggle: handed, then back halted.
   string handedMsg = ACR_SquadAI.Toggle(alpha);
   string backMsg = ACR_SquadAI.Toggle(alpha);
   ACR_TestRegress.Check("squadai: the toggle hands it over and takes it back halted, with no AI task", handedMsg.Contains("the AI commands it now") && backMsg.Contains("back under your command") && !alpha.m_bAICommanded && alpha.m_iAITask == ACR_EAITask.FREE && alpha.OrderState() == ACR_EOrderState.HOLD, string.Format("'%1' / '%2', order '%3'", handedMsg, backMsg, alpha.m_Order));
   // Bravo handed and wiped out.
   ACR_SquadAI.Hand(bravo);
   array<AIAgent> agents = {};
   bravo.GetAgents(agents);
   foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   bool gone = !bravo || !manager.m_Squads.Contains(bravo) || bravo.CountAlive() == 0;
   if ((!gone || ACR_SquadAI.CountFor("US") > 0) && waited < 40) return;
   ACR_TestRegress.Check("squadai: a handed squad that is wiped out is dropped (the AI keeps running)", gone && ACR_SquadAI.CountFor("US") == 0, string.Format("Bravo gone %1, handed left %2 after %3 s", gone, ACR_SquadAI.CountFor("US"), waited));
   ACR_TestRegress.Complete("squadai", CHECKS);
   s_iStep = 4;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
