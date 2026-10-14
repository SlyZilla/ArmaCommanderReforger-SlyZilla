// Test entry points (R17, Jacob 14 Oct: the player download leaves the test scripts out).
// make_package.py skips Scripts/Game/ACR_Tests and Scripts/Game/BBB/*Test.c; game code
// reaches the tests only through this class, which ACR_Tests/ACR_TestHooksImpl.c mods.
// Without the tests every hook does nothing.
class ACR_TestHooks
{
 protected static ref ACR_TestHooks s_Instance;

 static ACR_TestHooks Get()
 {
  if (!s_Instance) s_Instance = new ACR_TestHooks();
  return s_Instance;
 }

 // Test range entity started (server) / removed.
 void RangeStarted(ACR_TestRange range) {}
 void RangeEnded(ACR_TestRange range) {}
 // Every second of a scenario (ACR_Mode.Second).
 void ScenarioSecond(int seconds) {}
 // Layout a skirmish test asks for, "" = none.
 string SkirmishLayout() { return ""; }
}
