// -acrPanelShotsTest 1 (a game client, not srv; ~2 min): the movable panels and the key reminders, with
// shots. Real mouse input comes from outside: each "[ACR] PANELSHOTS DO" line asks for a drag or click
// in screen pixels of the game's window. Checks: the strip shows off the map; the panel and INTEL move
// when dragged and the place is saved; the button still clicks and turns the strip off and on; a
// double click puts each back.
class ACR_TestPanelShots
{
 static const float DRAG_X = -700;
 static const float DRAG_Y = 240;

 static void Run(ACR_TestHarness h, int tick)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || tick < 4) return;
  if (tick == 4)
  {
   FileIO.DeleteFile(ACR_UiPrefs.PREFS);
   ACR_UiPrefs.ResetForTest();
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  ACR_CommandPanel panel = pc.ACR_CommandPanelForTest();
  if (tick == 20)
  {
   ACR_TestRegress.Check("panelshots: the key strip shows off the map", ACR_KeyHints.ShownForTest(), "");
   Shot("panel_1_strip");
  }
  if (tick == 22) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 24 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  if (tick == 28)
  {
   Shot("panel_2_home");
   if (panel) Ask("drag", panel.GripForTest(false), ws.DPIScale(DRAG_X), ws.DPIScale(DRAG_Y));
  }
  if (tick == 38)
  {
   ACR_TestRegress.Check("panelshots: a drag moves the panel", ACR_UiPrefs.PanelMoved(), string.Format("%1, %2", ACR_UiPrefs.s_fPanelX, ACR_UiPrefs.s_fPanelY));
   ACR_TestRegress.Check("panelshots: the place is saved", FileIO.FileExists(ACR_UiPrefs.PREFS), "");
   if (panel) Ask("drag", panel.GripForTest(true), ws.DPIScale(300), ws.DPIScale(420));
  }
  if (tick == 46)
  {
   ACR_TestRegress.Check("panelshots: a drag on INTEL's header moves it", ACR_UiPrefs.FeedMoved(), string.Format("%1, %2", ACR_UiPrefs.s_fFeedX, ACR_UiPrefs.s_fFeedY));
   Shot("panel_3_moved");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == 52)
  {
   ACR_TestRegress.Check("panelshots: the button still clicks: reminders off", !ACR_UiPrefs.s_bKeyHints, "");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == 58)
  {
   ACR_TestRegress.Check("panelshots: and on again", ACR_UiPrefs.s_bKeyHints, "");
   if (panel) Ask("double", panel.GripForTest(false), 0, 0);
  }
  if (tick == 64)
  {
   ACR_TestRegress.Check("panelshots: a double click puts the panel back", !ACR_UiPrefs.PanelMoved(), string.Format("%1, %2", ACR_UiPrefs.s_fPanelX, ACR_UiPrefs.s_fPanelY));
   if (panel) Ask("double", panel.GripForTest(true), 0, 0);
  }
  if (tick == 70)
  {
   ACR_TestRegress.Check("panelshots: and INTEL too", !ACR_UiPrefs.FeedMoved(), string.Format("%1, %2", ACR_UiPrefs.s_fFeedX, ACR_UiPrefs.s_fFeedY));
   Shot("panel_4_back");
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  }
  if (tick == 76)
  {
   ACR_TestRegress.Check("panelshots: the strip is back after the map", ACR_KeyHints.ShownForTest(), "");
   FileIO.DeleteFile(ACR_UiPrefs.PREFS);
   ACR_TestRegress.Complete("panelshots", 9);
  }
 }

 // The middle of the rectangle, and how far to drag from it.
 protected static void Ask(string what, vector rect, float dx, float dy)
 {
  PrintFormat("[ACR] PANELSHOTS DO %1 %2 %3 %4 %5", what, (int)rect[0], (int)rect[1], (int)dx, (int)dy);
 }

 protected static void Shot(string name)
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  if (!FileIO.FileExists("$profile:acr-shots")) return;
  System.MakeScreenshot("$profile:acr-shots/" + name);
  PrintFormat("[ACR] PANELSHOTS ready %1", name);
 }
}
