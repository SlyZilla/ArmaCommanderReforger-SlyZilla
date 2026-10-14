// Connects the tests to the game code (see ACR/ACR_TestHooks.c). Not in the player package.
modded class ACR_TestHooks
{
 protected ref ACR_TestHarness m_Harness;

 override void RangeStarted(ACR_TestRange range)
 {
  m_Harness = ACR_TestHarness.CreateFromCommandLine(range);
  if (m_Harness) GetGame().GetCallqueue().CallLater(m_Harness.Tick, 1000, true);
  // A scenario test that landed on the range (wrong world): end at once.
  if (System.IsCLIParam("acrScenarioTest")) ACR_TestRegress.Abort("scenario", "wrong world: restart the test launcher");
 }

 override void RangeEnded(ACR_TestRange range)
 {
  if (m_Harness && GetGame()) GetGame().GetCallqueue().Remove(m_Harness.Tick);
  m_Harness = null;
 }

 override void ScenarioSecond(int seconds)
 {
  // -acrPadsTest: helipads on this scenario's map (ACR_TestPads).
  if (System.IsCLIParam("acrPadsTest") || System.IsCLIParam("acrPadsLevieTest") || System.IsCLIParam("acrPadsMontignacTest") || System.IsCLIParam("acrPadsPhilippeTest")) ACR_TestPads.Run(null, seconds);
  // -acrSurveyTest: object/terrain dump for placing bases and pads (ACR_TestSurvey).
  if (System.IsCLIParam("acrSurveyTest")) ACR_TestSurvey.Run(null, seconds);
  // -acrPlacesTest: named map locations, for laying out a new map (ACR_TestPlaces).
  if (System.IsCLIParam("acrPlacesTest")) ACR_TestPlaces.Run(seconds);
  // -acrSetupTest: the setup phase (ACR_TestSetup).
  if (ACR_TestSetup.Wanted()) ACR_TestSetup.Run(seconds);
  if (ACR_TestSetup.UiWanted()) ACR_TestSetup.RunUi(seconds);
  if (ACR_TestSetup.ReserveWanted()) ACR_TestSetup.RunReserve(seconds);
  if (ACR_TestSetup.GenWanted()) ACR_TestSetup.RunGen(seconds);
  if (System.IsCLIParam("acrEveronSurveyTest")) ACR_TestPlaces.SurveyEveron(seconds);
  if (System.IsCLIParam("acrEveronPadsTest")) ACR_TestPads.SurveyEveron(seconds);
  // -acrSkirmishTest / -acrSkirmishSouthTest: skirmish layouts on Everon (ACR_TestSkirmish).
  if (System.IsCLIParam("acrSkirmishTest") || System.IsCLIParam("acrSkirmishSouthTest")) ACR_TestSkirmish.Run(seconds);
  // -acrGenerateTest / -acrGenerateEveronTest: the scenario generator (ACR_TestGenerate).
  if (System.IsCLIParam("acrGenerateTest")) ACR_TestGenerate.Run(seconds);
  if (System.IsCLIParam("acrGenerateEveronTest")) ACR_TestGenerate.RunEveron(seconds);
 }

 override string SkirmishLayout()
 {
  if (ACR_TestSkirmish.Wanted()) return ACR_TestSkirmish.Layout();
  return "";
 }
}
