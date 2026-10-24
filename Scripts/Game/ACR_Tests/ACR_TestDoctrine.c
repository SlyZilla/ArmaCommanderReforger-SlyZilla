// AI commander doctrines (ACR_AIDoctrine, 24 Oct), run from srv:regress at tick 4: the pick
// follows option > faction settings > test default > random, the standard doctrine keeps the
// commander's old numbers, and a new commander takes its side's pick.
class ACR_TestDoctrine
{
 static void Run(ACR_Manager manager)
 {
  array<string> keys = {"balanced", "blitz", "firepower", "fortress", "raider"};
  bool allFound = ACR_AIDoctrine.All().Count() == keys.Count();
  foreach (string key : keys) if (!ACR_AIDoctrine.Find(key)) allFound = false;
  allFound = allFound && ACR_AIDoctrine.Find("BLITZ") != null && !ACR_AIDoctrine.Find("random");
  ACR_TestRegress.Check("doctrine: five doctrines, found by key in any case", allFound, string.Format("%1 doctrines", ACR_AIDoctrine.All().Count()));

  // Balanced must stay the commander from before doctrines.
  ACR_AIDoctrine standard = ACR_AIDoctrine.Find("balanced");
  // Short chains: one long && chain is "Formula too complex" for the compiler.
  bool same = standard.m_fAttackRatio == 1 && standard.m_fDefenseRatio == 1 && standard.m_fConcedeRatio == 1;
  same = same && standard.m_iGrabs == 0 && standard.m_iExtraFronts == 3 && standard.m_iSquadsPerFront == 3;
  same = same && standard.m_fCounterNeed == 0.7 && standard.m_fCounterMinutes == 5;
  same = same && standard.m_fQuietMinutes == 4 && standard.m_fPressurePerMinute == 0.05 && standard.m_fMinPressure == 0.65;
  same = same && standard.m_iReserveSquads == 0 && standard.m_fRetreatRatio == 0.4 && standard.m_fAbortRatio == 0.35;
  same = same && standard.m_iHeavyPrep == 12 && standard.m_iRoadblocks == 2 && standard.m_iTransports == 0;
  same = same && standard.m_fVehicleWeight == 1 && standard.m_fSmallTeamWeight == 1 && !standard.m_bSmallTeamsFree;
  same = same && !standard.m_bSupportsFirst && standard.m_iSupportBudgetMax == 60;
  ACR_TestRegress.Check("doctrine: balanced keeps the standard commander's numbers", same, "");

  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string source;
  ACR_AIDoctrine plain = ACR_AIDoctrine.Choose(west, source);
  ACR_TestRegress.Check("doctrine: tests default to balanced", plain && plain.m_sKey == "balanced" && source == "test", string.Format("%1 (%2)", plain.m_sKey, source));

  ACR_Options.Set("acrDoctrineWest", "Blitz");
  ACR_Options.Set("acrDoctrineEast", "fortress");
  string westSource, eastSource;
  ACR_AIDoctrine westPick = ACR_AIDoctrine.Choose(west, westSource);
  ACR_AIDoctrine eastPick = ACR_AIDoctrine.Choose(east, eastSource);
  ACR_TestRegress.Check("doctrine: each side takes its own setup choice", westPick.m_sKey == "blitz" && eastPick.m_sKey == "fortress" && westSource == "screen",
   string.Format("west %1 %2 (%3), east %4 %5", west, westPick.m_sKey, westSource, east, eastPick.m_sKey));

  ACR_AICommander made = new ACR_AICommander(manager, west);
  ACR_TestRegress.Check("doctrine: a new AI commander fights to its side's pick", made.Doctrine() && made.Doctrine().m_sKey == "blitz", made.Doctrine().m_sName);

  ACR_Options.Set("acrDoctrineWest", "random");
  ACR_AIDoctrine rolled = ACR_AIDoctrine.Choose(west, source);
  ACR_TestRegress.Check("doctrine: an explicit random rolls one", rolled != null && source == "random", string.Format("%1 (%2)", rolled.m_sKey, source));

  // A faction's own setting beats the test default; the host option beats it.
  ACR_Options.Unset("acrDoctrineWest");
  ACR_FactionSettings settings = ACR_FactionSettings.Find(west);
  if (settings)
  {
   ACR_EAIDoctrineChoice was = settings.m_eAIDoctrine;
   settings.m_eAIDoctrine = ACR_EAIDoctrineChoice.RAIDER;
   ACR_AIDoctrine fromSettings = ACR_AIDoctrine.Choose(west, source);
   settings.m_eAIDoctrine = was;
   ACR_TestRegress.Check("doctrine: the faction's m_eAIDoctrine applies when no option is set", fromSettings.m_sKey == "raider" && source == "faction settings", string.Format("%1 (%2)", fromSettings.m_sKey, source));
  }
  else ACR_TestRegress.Skip("doctrine: the faction's m_eAIDoctrine applies when no option is set", "no settings for " + west);

  string row = ACR_Setup.ValueName("doctrineeast", "fortress");
  bool rows = ACR_Setup.Rows().Contains("doctrinewest\tBlue AI doctrine") && ACR_Setup.Option("doctrineeast") == "acrDoctrineEast" && row.StartsWith("Fortress");
  ACR_TestRegress.Check("doctrine: setup screen rows for both sides", rows, row);
  ACR_Options.Unset("acrDoctrineEast");
 }
}
