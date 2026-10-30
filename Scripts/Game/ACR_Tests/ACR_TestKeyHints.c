// srv:keyhints (about 30 s): the key reminders' text and the UI prefs behind the movable panels. Checks:
// what a commander and anyone else are reminded of, the vehicle's wording; the file keeps the places
// and the choice; a place off the screen comes back on. The widgets and the drag need a client.
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

  ACR_UiPrefs.ResetForTest();
  ACR_TestRegress.Check("keyhints: nothing moved and reminders on by default", !ACR_UiPrefs.PanelMoved() && !ACR_UiPrefs.FeedMoved() && ACR_UiPrefs.s_bKeyHints, "");
  ACR_UiPrefs.s_fPanelX = -400;
  ACR_UiPrefs.s_fPanelY = 250;
  ACR_UiPrefs.s_fFeedX = 300;
  ACR_UiPrefs.s_fFeedY = 500;
  ACR_UiPrefs.s_bKeyHints = false;
  ACR_UiPrefs.Save();
  ACR_UiPrefs.ResetForTest();
  ACR_UiPrefs.Load();
  bool kept = ACR_UiPrefs.s_fPanelX == -400 && ACR_UiPrefs.s_fPanelY == 250 && ACR_UiPrefs.s_fFeedX == 300 && ACR_UiPrefs.s_fFeedY == 500 && !ACR_UiPrefs.s_bKeyHints;
  ACR_TestRegress.Check("keyhints: the file keeps the places and the choice", kept && ACR_UiPrefs.PanelMoved() && ACR_UiPrefs.FeedMoved(), string.Format("%1 %2 %3 %4", ACR_UiPrefs.s_fPanelX, ACR_UiPrefs.s_fPanelY, ACR_UiPrefs.s_fFeedX, ACR_UiPrefs.s_fFeedY));
  FileIO.DeleteFile(ACR_UiPrefs.PREFS);
  ACR_UiPrefs.ResetForTest();

  // A 1920 x 1080 screen, the panel 340 wide.
  bool inside = ACR_UiPrefs.ClampPanelX(-5000, 1920, 340) == -1580 && ACR_UiPrefs.ClampPanelX(50, 1920, 340) == 0 && ACR_UiPrefs.ClampPanelX(-400, 1920, 340) == -400;
  ACR_TestRegress.Check("keyhints: the panel stays on the screen", inside, "");
  bool feed = ACR_UiPrefs.ClampFeedX(-20, 1920, 330) == 0 && ACR_UiPrefs.ClampFeedX(5000, 1920, 330) == 1590;
  ACR_TestRegress.Check("keyhints: INTEL stays on the screen", feed, "");
  ACR_TestRegress.Check("keyhints: enough stays below to grab it", ACR_UiPrefs.ClampY(5000, 1080) == 1040 && ACR_UiPrefs.ClampY(-5, 1080) == 0, "");
  ACR_TestRegress.Complete("keyhints", 8);
 }
}
