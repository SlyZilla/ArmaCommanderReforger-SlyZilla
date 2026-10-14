// -acrSetupTest 1 (server, Arland, ~40 s): the setup phase (ACR_Setup). Runs from ACR_Mode's
// one-second timer, because the manager's tick is held while the phase lasts.
// Checks: nothing starts before Start (no AI, no layout, host options unread), rows are
// offered, a stranger can't change or start anything, options cycle on the server, and
// Start applies them (threat high reaches the game, starting units spawn).
class ACR_TestSetup
{
 static const int CHECKS = 16;
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
   // Any faction the world lists can play (13 Oct): Arland lists FIA; pick it for the red side.
   array<string> keys = {};
   ACR_FactionSelection.Candidates(keys);
   string listed;
   foreach (string k : keys) listed += k + " ";
   ACR_TestRegress.Check("setup: playable factions come from the world's faction manager", keys.Contains("US") && keys.Contains("USSR") && keys.Contains("FIA") && !keys.Contains("CIV"), listed);
   string red = ACR_Setup.Cycle("east");
   ACR_TestRegress.Check("setup: the red side cycles to FIA without applying yet", ACR_FactionSelection.East() == "FIA" && ACR_FactionSelection.West() == "US" && !ACR_FactionSelection.Applied() && red.Contains("FIA"), red);
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
   FactionManager factions = GetGame().GetFactionManager();
   SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey("FIA"));
   SCR_Faction ussr = SCR_Faction.Cast(factions.GetFactionByKey("USSR"));
   ACR_TestRegress.Check("setup: Start makes FIA the red side", ACR_FactionSelection.Applied() && ACR_FactionSelection.East() == "FIA" && fia && fia.IsPlayable() && ussr && !ussr.IsPlayable(), "");
   int fiaBases, ussrBases;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == "FIA") fiaBases++;
    if (base.m_sOwner == "USSR") ussrBases++;
   }
   ACR_TestRegress.Check("setup: the red side's bases belong to FIA", fiaBases > 0 && ussrBases == 0, string.Format("FIA %1, USSR %2", fiaBases, ussrBases));
   int buys = -1;
   if (manager) buys = manager.m_Bases.GetFaction("FIA").m_aRequisitions.Count();
   ACR_TestRegress.Check("setup: FIA gets settings and a buy list from its catalog", ACR_FactionSettings.Find("FIA") != null && buys > 0, string.Format("buy list %1", buys));
   int fiaSquads;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "FIA") fiaSquads++;
   ACR_TestRegress.Check("setup: the red side's starting units spawn as FIA", fiaSquads > 0, string.Format("FIA squads %1", fiaSquads));
   array<ref SCR_BasePlayerLoadout> loadouts = {};
   int officers = GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(fia, loadouts);
   ACR_TestRegress.Check("setup: FIA has a command unit loadout", officers > 0, string.Format("%1 loadouts", officers));
   // AI reserve (Jacob 14 Oct): give US every neutral non-HQ base for a moment; the reserve must
   // pick a secure one behind the front, not the HQ.
   array<ACR_Base> lent = {};
   foreach (ACR_Base neutral : ACR_Base.s_All)
    if (neutral.m_sOwner.IsEmpty()) { neutral.m_sOwner = "US"; lent.Insert(neutral); }
   ACR_AICommander reserveAI = new ACR_AICommander(manager, "US");
   ACR_Base reserveAt = reserveAI.ReserveBase();
   string reserveText = "none";
   if (reserveAt) reserveText = string.Format("%1 (HQ %2, value %3, radius %4, %5 m from the front)", reserveAt.m_sName, reserveAI.IsHQ(reserveAt), reserveAt.m_iValue, reserveAt.m_fRadius, Math.Round(reserveAI.FrontDistance(reserveAt)));
   ACR_TestRegress.Check("setup: the AI reserve waits at a secure non-HQ base behind the front", reserveAt && !reserveAI.IsHQ(reserveAt) && reserveAI.ReserveFits(reserveAt), string.Format("%1 of %2 lent bases", reserveText, lent.Count()));
   foreach (ACR_Base back : lent) back.m_sOwner = "";
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

 // -acrReserveTest 1 (server, Everon, ~1 min): where the AI keeps its reserve on a big map.
 // US is given every neutral base north of the split line (z > 6650); the reserve must sit at a
 // secure non-HQ base, not the Airbase HQ. Logs the five best candidates.
 // -acrSetupGenTest 1 (server, Kolguyev, ~40 s): the layout preview during setup (aca-f07).
 static const int GEN_CHECKS = 5;
 protected static ref array<string> s_PreviewNames = {};
 static bool GenWanted() { return System.IsCLIParam("acrSetupGenTest"); }
 static void RunGen(int second)
 {
  if (second == 6)
  {
   ACR_Setup.UpdatePreview();
   string rows = ACR_Setup.Rows();
   int first = ACR_Setup.s_PreviewNames.Count();
   ACR_TestRegress.Check("setup preview: a generated layout is planned before Start", !ACR_Setup.Started() && first >= 15 && ACR_Base.s_All.IsEmpty(), string.Format("%1 bases planned, %2 built", first, ACR_Base.s_All.Count()));
   ACR_TestRegress.Check("setup preview: rows offer the seed and the map", rows.Contains("seed\tLayout seed: ") && rows.Contains("map\tShow the layout on the map"), rows);
   string seedBefore;
   ACR_Options.Get("acrSeed", seedBefore);
   string firstNames = "";
   foreach (string n : ACR_Setup.s_PreviewNames) firstNames += n + ";";
   ACR_Setup.Cycle("seed");
   ACR_Setup.Cycle("layout"); // gen:full -> gen:half
   ACR_Setup.UpdatePreview();
   int half = ACR_Setup.s_PreviewNames.Count();
   string seedAfter;
   ACR_Options.Get("acrSeed", seedAfter);
   ACR_TestRegress.Check("setup preview: a new seed and the half size re-plan it", seedAfter != seedBefore && half > 0 && half < first, string.Format("seed %1 -> %2, %3 -> %4 bases", seedBefore, seedAfter, first, half));
   s_PreviewNames.Clear();
   s_PreviewNames.Copy(ACR_Setup.s_PreviewNames);
   ACR_Setup.Start("the setup preview test");
  }
  if (second == 14)
  {
   array<string> built = {};
   foreach (ACR_Base base : ACR_Base.s_All) built.Insert(base.m_sName);
   int same;
   foreach (string planned : s_PreviewNames) if (built.Contains(planned)) same++;
   ACR_TestRegress.Check("setup preview: Start builds exactly the previewed layout", ACR_Generator.Built() && same == s_PreviewNames.Count() && built.Count() == s_PreviewNames.Count(), string.Format("%1 of %2 previewed, %3 built", same, s_PreviewNames.Count(), built.Count()));
   int ownedHQs;
   foreach (ACR_Base hq : ACR_Base.s_All) if (hq.m_sOwner != "") ownedHQs++;
   ACR_TestRegress.Check("setup preview: two HQs owned after Start", ownedHQs == 2, string.Format("%1 owned", ownedHQs));
   ACR_TestRegress.Complete("setupgen", GEN_CHECKS);
  }
 }

 static bool ReserveWanted() { return System.IsCLIParam("acrReserveTest"); }
 static void RunReserve(int second)
 {
  if (second != 12) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  int lent;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner.IsEmpty() && base.GetOrigin()[2] > 6650) { base.m_sOwner = "US"; lent++; }
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ACR_Base at = ai.ReserveBase();
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (ai.ReserveFits(candidate)) PrintFormat("[ACR] RESERVE candidate %1 value %2 radius %3 front %4 m", candidate.m_sName, candidate.m_iValue, candidate.m_fRadius, Math.Round(ai.FrontDistance(candidate)));
  string text = "none";
  if (at) text = string.Format("%1 (HQ %2, value %3, radius %4, %5 m from the front)", at.m_sName, ai.IsHQ(at), at.m_iValue, at.m_fRadius, Math.Round(ai.FrontDistance(at)));
  ACR_TestRegress.Check("reserve: Everon reserve at a secure non-HQ base", at && !ai.IsHQ(at) && ai.ReserveFits(at), string.Format("%1; %2 bases lent", text, lent));
  ACR_TestRegress.Complete("reserve", 1);
 }
}
