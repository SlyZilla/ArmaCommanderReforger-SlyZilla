// -acrSetupTest 1 (server, Arland, ~40 s): the setup phase (ACR_Setup). Runs from ACR_Mode's
// one-second timer, because the manager's tick is held while the phase lasts.
// Checks: nothing starts before Start (no AI, no layout, host options unread), rows are
// offered, a stranger can't change or start anything, options cycle on the server, and
// Start applies them (threat high reaches the game, starting units spawn).
class ACR_TestSetup
{
 static const int CHECKS = 8;
 protected static int s_iStartedAt = -1;

 static bool Wanted() { return System.IsCLIParam("acrSetupTest"); }

 protected static int Agents()
 {
  array<AIAgent> agents = {};
  if (GetGame().GetAIWorld()) GetGame().GetAIWorld().GetAIAgents(agents);
  return agents.Count();
 }

 static void Run(int second)
 {
  if (second == 6)
  {
   int agents = Agents();
   ACR_TestRegress.Check("setup: nothing starts before Start", ACR_Setup.Wanted() && !ACR_Setup.Started() && !ACR_Skirmish.Applied() && agents == 0, string.Format("wanted %1, started %2, layout applied %3, AI agents %4", ACR_Setup.Wanted(), ACR_Setup.Started(), ACR_Skirmish.Applied(), agents));
   string rows = ACR_Setup.Rows();
   ACR_TestRegress.Check("setup: the dialog rows list the options", rows.Contains("threat\tThreat: normal") && rows.Contains("ai\tAI commanders") && rows.Contains("shared\tShared command: on"), rows);
   string refused = ACR_Setup.Handle(9999, "threat");
   string refusedStart = ACR_Setup.Handle(9999, "start");
   ACR_TestRegress.Check("setup: a player who isn't host or admin can't change or start", refused.Contains("Only the host") && refusedStart.Contains("Only the host") && !ACR_Setup.Started(), refused);
   string first = ACR_Setup.Cycle("threat");
   string value;
   ACR_Options.Get("acrThreat", value);
   ACR_TestRegress.Check("setup: a click cycles threat normal -> high on the server", value == "high" && ACR_Options.Source("acrThreat") == "screen" && first.Contains("high"), first);
   ACR_Setup.Cycle("unitcap");
   string cap;
   ACR_Options.Get("acrUnitCap", cap);
   ACR_Setup.Cycle("startrp");
   ACR_Setup.Cycle("startrp");
   string rp;
   ACR_Options.Get("acrStartRP", rp);
   ACR_TestRegress.Check("setup: numeric options cycle from the scenario default", cap == "4" && rp == "100", string.Format("unit cap %1, start RP %2", cap, rp));
   ACR_Setup.Start("the setup test");
   s_iStartedAt = second;
  }
  if (s_iStartedAt > 0 && second == s_iStartedAt + 8)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   ACR_TestRegress.Check("setup: Start applies the chosen threat", ACR_Setup.Started() && ACR_Manager.ThreatOption() == 2, string.Format("threat %1", ACR_Manager.ThreatOption()));
   int rp = -1;
   if (manager && !ACR_FactionSettings.s_All.IsEmpty()) rp = manager.m_Bases.GetRP(ACR_FactionSettings.s_All[0].m_sFaction);
   ACR_TestRegress.Check("setup: buy lists and RP are built with the chosen start RP", rp >= 100 && rp < 200, string.Format("RP %1", rp));
   int agents = Agents();
   ACR_TestRegress.Check("setup: the layout is applied and starting units spawn after Start", ACR_Skirmish.Applied() && agents > 0, string.Format("AI agents %1, squads %2", agents, manager.m_Squads.Count()));
   ACR_TestRegress.Complete("setup", CHECKS);
  }
 }

 // -acrSetupUiTest 1 (GAME, Arland, ~40 s, on Jacob's screen): the host's dialog itself.
 static bool UiWanted() { return System.IsCLIParam("acrSetupUiTest"); }
 static void RunUi(int second)
 {
  if (second == 10)
  {
   ACR_SetupDialog dialog = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup UI: the host sees the setup dialog with the options", dialog != null && ACR_SetupClient.s_bAllowed && ACR_SetupClient.s_sRows.Contains("Threat: normal") && !ACR_Setup.Started(), string.Format("dialog %1, allowed %2", dialog != null, ACR_SetupClient.s_bAllowed));
   ACR_SetupClient.Request("threat");
  }
  if (second == 12)
   ACR_TestRegress.Check("setup UI: a click reaches the server and the dialog refreshes", ACR_SetupClient.s_sRows.Contains("Threat: high") && ACR_Manager.ThreatOption() == 2, ACR_SetupClient.s_sRows);
  if (second == 13) ACR_SetupClient.Request("start");
  if (second == 25)
  {
   ACR_SetupDialog open = ACR_SetupDialog.Cast(SCR_ConfigurableDialogUi.GetCurrentDialog());
   ACR_TestRegress.Check("setup UI: Start closes the dialog and the match begins", ACR_Setup.Started() && !ACR_SetupClient.s_bActive && open == null && ACR_Skirmish.Applied(), string.Format("started %1, dialog open %2", ACR_Setup.Started(), open != null));
   PlayerController pc = GetGame().GetPlayerController();
   ACR_TestRegress.Check("setup UI: the host spawns after Start", pc && pc.GetControlledEntity() != null, "");
   ACR_TestRegress.Complete("setupui", 4);
  }
 }
}
