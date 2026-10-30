// srv:keyhints (about 30 s): the key reminders' text and the UI prefs behind the movable panels. Checks:
// what a commander and anyone else are reminded of, the vehicle's wording; the file keeps every panel's
// spot and the choices, and a file from before every panel could move still reads; a double click's
// forgetting; a place off the screen comes back on, by any anchor; the player's real file is left
// alone. The widgets and the drag need a client (ACR_TestPanelShots).
class ACR_TestKeyHints
{
 static void Run(int second)
 {
  if (second != 5) return;
  string plain = ACR_KeyHints.Line(false, false);
  string leader = ACR_KeyHints.Line(true, false);
  string driving = ACR_KeyHints.Line(true, true);
  ACR_TestRegress.Check("keyhints: anyone is reminded of the map and taking over", plain.Contains("ACR_Map") && plain.Contains("ACR_Possess") && plain.Contains("ACR_Return"), plain);
  ACR_TestRegress.Check("keyhints: squad orders and the camera are the commander's", !plain.Contains("ACR_Move") && !plain.Contains("ACR_RTSCamera") && leader.Contains("ACR_Move") && leader.Contains("ACR_RTSCamera"), leader);
  ACR_TestRegress.Check("keyhints: in a vehicle F6 drops the passengers", driving.Contains("Drop passengers") && !driving.Contains("Squad rally") && leader.Contains("Squad rally"), driving);

  // The test's own file: the player's stays as it was, whatever happens to this test.
  bool realBefore = FileIO.FileExists(ACR_UiPrefs.PREFS);
  ACR_UiPrefs.UseTestFileForTest();
  ACR_TestRegress.Check("keyhints: nothing moved and reminders on by default", !ACR_UiPrefs.Moved(ACR_UiPrefs.PANEL) && !ACR_UiPrefs.Moved(ACR_UiPrefs.FEED) && ACR_UiPrefs.s_bKeyHints && !ACR_UiPrefs.s_bBuildOpen, "");

  array<string> ids = {ACR_UiPrefs.PANEL, ACR_UiPrefs.FEED, ACR_UiPrefs.SQUADS, ACR_UiPrefs.LAYERS, ACR_UiPrefs.DEBUG_BOX, ACR_UiPrefs.CARD, ACR_UiPrefs.CHANNEL, ACR_UiPrefs.TWOIC_PANEL};
  foreach (int i, string id : ids) ACR_UiPrefs.SetSpot(id, -400.5 - i, 250 + i);
  ACR_UiPrefs.s_bKeyHints = false;
  ACR_UiPrefs.s_bBuildOpen = true;
  ACR_UiPrefs.Save();
  ACR_UiPrefs.ResetForTest();
  ACR_TestRegress.Check("keyhints: a reset forgets until the file is read", !ACR_UiPrefs.Moved(ACR_UiPrefs.PANEL), "");
  ACR_UiPrefs.Load();
  bool kept = !ACR_UiPrefs.s_bKeyHints && ACR_UiPrefs.s_bBuildOpen;
  string keptWhy = "";
  foreach (int k, string keptId : ids)
  {
   float keptX, keptY;
   if (ACR_UiPrefs.Spot(keptId, keptX, keptY) && keptX == -400.5 - k && keptY == 250 + k) continue;
   kept = false;
   keptWhy += string.Format("%1 %2,%3 ", keptId, keptX, keptY);
  }
  ACR_TestRegress.Check("keyhints: the file keeps every panel's spot and the choices", kept, keptWhy);

  ACR_UiPrefs.Forget(ACR_UiPrefs.SQUADS);
  float gone;
  ACR_UiPrefs.Save();
  ACR_UiPrefs.ResetForTest();
  ACR_UiPrefs.Load();
  ACR_TestRegress.Check("keyhints: a panel sent home stays forgotten in the file", !ACR_UiPrefs.Spot(ACR_UiPrefs.SQUADS, gone, gone) && ACR_UiPrefs.Moved(ACR_UiPrefs.FEED), "");

  // A file as the last version wrote it: the player's own, from the playtest.
  WriteFile({"keys=1", "build=1", "panelx=-6", "panely=9", "feedx=110", "feedy=96"});
  ACR_UiPrefs.ResetForTest();
  ACR_UiPrefs.Load();
  float oldX, oldY;
  bool oldPanel = ACR_UiPrefs.Spot(ACR_UiPrefs.PANEL, oldX, oldY) && oldX == -6 && oldY == 9;
  ACR_TestRegress.Check("keyhints: an old file's panel place still reads", oldPanel && ACR_UiPrefs.s_bKeyHints && ACR_UiPrefs.s_bBuildOpen, string.Format("%1 %2", oldX, oldY));
  ACR_TestRegress.Check("keyhints: an old file's home place isn't a move", !ACR_UiPrefs.Moved(ACR_UiPrefs.FEED), "");
  // ... and it comes out in the new form, losing nothing.
  ACR_UiPrefs.Save();
  ACR_UiPrefs.ResetForTest();
  ACR_UiPrefs.Load();
  float newX, newY;
  ACR_TestRegress.Check("keyhints: a save moves an old file to the new form", ACR_UiPrefs.Spot(ACR_UiPrefs.PANEL, newX, newY) && newX == -6 && newY == 9 && !FileHas("panelx"), "");
  // Garbage reads as nothing, and an unknown panel's line (a newer version's) survives a save.
  WriteFile({"place.=1,2", "place.aca=7", "place.aca", "place.future=5,6", "nonsense"});
  ACR_UiPrefs.ResetForTest();
  ACR_UiPrefs.Load();
  ACR_UiPrefs.Save();
  float futureX, futureY;
  ACR_TestRegress.Check("keyhints: a bad line is skipped, an unknown panel kept", !ACR_UiPrefs.Moved(ACR_UiPrefs.PANEL) && ACR_UiPrefs.Spot("future", futureX, futureY) && futureX == 5 && FileHas("place.future=5"), "");
  FileIO.DeleteFile(ACR_UiPrefs.TEST_PREFS);
  ACR_UiPrefs.ResetForTest();

  // A 1920 x 1080 screen, the panel 340 wide, by its anchor.
  bool right = ACR_UiPrefs.ClampX(-5000, 1, 1920, 340) == -1580 && ACR_UiPrefs.ClampX(50, 1, 1920, 340) == 0 && ACR_UiPrefs.ClampX(-400, 1, 1920, 340) == -400;
  ACR_TestRegress.Check("keyhints: a panel from the right edge stays on the screen", right, "");
  bool left = ACR_UiPrefs.ClampX(-20, 0, 1920, 330) == 0 && ACR_UiPrefs.ClampX(5000, 0, 1920, 330) == 1590;
  ACR_TestRegress.Check("keyhints: a panel from the left edge stays on the screen", left, "");
  bool middle = ACR_UiPrefs.ClampX(-1000, 0.5, 1920, 1560) == -180 && ACR_UiPrefs.ClampX(1000, 0.5, 1920, 1560) == 180 && ACR_UiPrefs.ClampX(40, 0.5, 1920, 1560) == 40;
  ACR_TestRegress.Check("keyhints: a panel from the middle stays on the screen", middle, "");
  ACR_TestRegress.Check("keyhints: enough stays below to grab it", ACR_UiPrefs.ClampY(5000, 0, 1080, 300) == 1040 && ACR_UiPrefs.ClampY(-5, 0, 1080, 300) == 0, "");
  // From the bottom (the switches, the channel): y = -40 stands its bottom 40 above the screen's.
  ACR_TestRegress.Check("keyhints: a panel from the bottom edge keeps its top on the screen", ACR_UiPrefs.ClampY(-5000, 1, 1080, 300) == -780 && ACR_UiPrefs.ClampY(5000, 1, 1080, 300) == 260, "");
  ACR_TestRegress.Check("keyhints: a panel wider than the screen shows its left edge", ACR_UiPrefs.ClampX(-5000, 0, 1000, 1200) == 0 && ACR_UiPrefs.ClampX(-5000, 1, 1000, 1200) == 200, "");
  // Spots from a bigger screen come back on, and fit: a tall panel stands higher.
  ACR_TestRegress.Check("keyhints: a tall panel stands high enough to show whole", ACR_UiPrefs.FitY(700, 0, 1080, 600) == 480 && ACR_UiPrefs.FitY(100, 0, 1080, 600) == 100 && ACR_UiPrefs.FitY(-10, 0, 1080, 600) == 0, "");
  ACR_TestRegress.Check("keyhints: taller than the screen, its top shows", ACR_UiPrefs.FitY(9, 0, 1080, 1250) == 0 && ACR_UiPrefs.FitY(-96, 1, 1080, 1250) == 170, "");
  ACR_TestRegress.Check("keyhints: from the bottom edge too", ACR_UiPrefs.FitY(50, 1, 1080, 200) == 0 && ACR_UiPrefs.FitY(-5000, 1, 1080, 200) == -880 && ACR_UiPrefs.FitY(-96, 1, 1080, 200) == -96, "");
  bool edges = ACR_UiPrefs.ToEdge(-24, 1, 1920, 340) == 1556 && ACR_UiPrefs.FromEdge(1556, 1, 1920, 340) == -24 && ACR_UiPrefs.ToEdge(0, 0.5, 1920, 1560) == 180 && ACR_UiPrefs.ToEdge(110, 0, 1920, 390) == 110;
  ACR_TestRegress.Check("keyhints: a spot's edge and back", edges, "");

  ACR_TestRegress.Check("keyhints: the player's own file is left as it was", FileIO.FileExists(ACR_UiPrefs.PREFS) == realBefore, "");
  ACR_TestRegress.Complete("keyhints", 20);
 }

 protected static void WriteFile(array<string> lines)
 {
  FileHandle file = FileIO.OpenFile(ACR_UiPrefs.TEST_PREFS, FileMode.WRITE);
  if (!file) return;
  foreach (string line : lines) file.WriteLine(line);
  file.Close();
 }

 protected static bool FileHas(string text)
 {
  FileHandle file = FileIO.OpenFile(ACR_UiPrefs.TEST_PREFS, FileMode.READ);
  if (!file) return false;
  bool found;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   if (line.Contains(text)) found = true;
  }
  file.Close();
  return found;
 }
}
