// -acrPanelShotsTest 1 (a game client, not srv; ~5 min): the movable panels and the key reminders, with
// shots. Real mouse input comes from outside: each "[ACR] PANELSHOTS DO" line asks for a drag or click
// in screen pixels of the game's window. Checks: the strip shows off the map; every movable panel moves
// when dragged by what the player would press, its spot is saved; a button still clicks and turns the
// strip off and on; a click on an INTEL line still jumps the map; a folded panel moves by its bar; a
// double click puts each back. Everything goes through a test file (UseTestFileForTest): the player's
// own prefs are never read after the start, written or deleted, even if this test is cut short.
class ACR_TestPanelShots
{
 protected static const int FIRST_DRAG = 34;
 protected static const int DRAG_TICKS = 7;
 protected static const int BACK_TICKS = 5;
 protected static vector s_Before;

 // The panels to drag, and where to (their top left corner, reference pixels on a 1920 x 1080 screen).
 protected static string Id(int i)
 {
  array<string> ids = {ACR_UiPrefs.PANEL, ACR_UiPrefs.FEED, ACR_UiPrefs.SQUADS, ACR_UiPrefs.CARD, ACR_UiPrefs.CHANNEL, ACR_UiPrefs.LAYERS, ACR_UiPrefs.DEBUG_BOX};
  if (i < ids.Count()) return ids[i];
  return "";
 }
 protected static vector Target(int i)
 {
  array<vector> spots = {Vector(1180, 60, 0), Vector(520, 60, 0), Vector(720, 300, 0), Vector(110, 300, 0), Vector(110, 620, 0), Vector(700, 1000, 0), Vector(1250, 860, 0)};
  return spots[i];
 }
 protected static const int PANELS = 7;

 static void Run(ACR_TestHarness h, int tick)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (tick == 2)
  {
   // The 2IC's card and channel need the 2IC on; the Squads list needs a squad to command.
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "1");
  }
  if (!pc || tick < 4) return;
  if (tick == 4)
  {
   ACR_UiPrefs.UseTestFileForTest();
   ACR_Manager manager = ACR_Manager.GetInstance();
   ACR_Intel intel = null;
   if (manager) intel = manager.IntelFor("US");
   if (intel && h.m_Range) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), "Alpha 1: enemy rifle squad near the test range", h.m_Range.m_Rally + Vector(300, 0, 300));
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
  if (tick == 28) pc.ACR_TestTwoICShown(true);
  if (tick == 32)
  {
   Shot("panel_2_home");
   for (int s = 0; s < PANELS; s++) ACR_TestRegress.Check("panelshots: shows at the start: " + Id(s), ACR_PanelMover.ShowsForTest(Id(s)), "");
  }

  // Each panel in turn: a drag by what the player would press, and it stands there.
  int dragAt = tick - FIRST_DRAG;
  if (dragAt >= 0 && dragAt < PANELS * DRAG_TICKS)
  {
   int i = dragAt / DRAG_TICKS;
   if (dragAt % DRAG_TICKS == 0)
   {
    s_Before = ACR_PanelMover.TopLeftForTest(Id(i));
    vector target = Target(i);
    // A real drag, so a panel that doesn't show now fails below: nothing moves.
    Ask("drag", ACR_PanelMover.GripForTest(Id(i)), ws.DPIScale(target[0] - s_Before[0]), ws.DPIScale(target[1] - s_Before[1]));
   }
   if (dragAt % DRAG_TICKS == DRAG_TICKS - 1)
   {
    vector now = ACR_PanelMover.TopLeftForTest(Id(i));
    vector want = Target(i);
    bool there = ACR_UiPrefs.Moved(Id(i)) && Math.AbsFloat(now[0] - want[0]) < 30 && Math.AbsFloat(now[1] - want[1]) < 30;
    ACR_TestRegress.Check("panelshots: a drag moves it: " + Id(i), there, string.Format("%1 -> %2, wanted %3", s_Before, now, want));
    FileHandle file = FileIO.OpenFile(ACR_UiPrefs.TEST_PREFS, FileMode.READ);
    bool saved;
    string line;
    while (file && file.ReadLine(line) >= 0)
    {
     if (line.StartsWith("place." + Id(i) + "=")) saved = true;
    }
    if (file) file.Close();
    ACR_TestRegress.Check("panelshots: its spot is saved: " + Id(i), saved, "");
   }
  }
  int after = FIRST_DRAG + PANELS * DRAG_TICKS;

  // The moved panels: their buttons and INTEL's lines still take their clicks.
  if (tick == after)
  {
   Shot("panel_3_moved");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == after + 6)
  {
   ACR_TestRegress.Check("panelshots: the button still clicks: reminders off", !ACR_UiPrefs.s_bKeyHints, "");
   if (panel) Ask("click", panel.KeyHintsButtonForTest(), 0, 0);
  }
  if (tick == after + 12)
  {
   ACR_TestRegress.Check("panelshots: and on again", ACR_UiPrefs.s_bKeyHints, "");
   if (panel && panel.FeedLineForTest() != vector.Zero) Ask("click", panel.FeedLineForTest(), 0, 0);
  }
  if (tick == after + 18)
  {
   ACR_TestRegress.Check("panelshots: a click on an INTEL line still jumps the map", pc.ACR_FeedJumpedForTest(), "");
   if (panel) ACR_TestRegress.Check("panelshots: INTEL's title bar stops short of its first line", panel.FeedTitleClearForTest(), "");
   pc.ACR_TwoICTogglePanel();
  }

  // The 2IC panel, by its header.
  if (tick == after + 21)
  {
   s_Before = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.TWOIC_PANEL);
   Ask("drag", ACR_PanelMover.GripForTest(ACR_UiPrefs.TWOIC_PANEL), ws.DPIScale(-120), ws.DPIScale(80));
  }
  if (tick == after + 27)
  {
   vector moved = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.TWOIC_PANEL);
   ACR_TestRegress.Check("panelshots: a drag moves it: the 2IC panel", ACR_UiPrefs.Moved(ACR_UiPrefs.TWOIC_PANEL) && moved != s_Before, string.Format("%1 -> %2", s_Before, moved));
   Shot("panel_4_twoic_moved");
   Ask("double", ACR_PanelMover.GripForTest(ACR_UiPrefs.TWOIC_PANEL), 0, 0);
  }
  if (tick == after + 32)
  {
   ACR_TestRegress.Check("panelshots: a double click puts it back: the 2IC panel", !ACR_UiPrefs.Moved(ACR_UiPrefs.TWOIC_PANEL), "");
   pc.ACR_TwoICTogglePanel();
   // Folded: zoomed right out each panel is a bar.
   if (mapEntity) mapEntity.ZoomPanSmooth(mapEntity.GetMinZoom(), h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  }
  if (tick == after + 37)
  {
   ACR_TestRegress.Check("panelshots: zoomed out INTEL is its bar", panel && panel.DeclIntelCollapsed(), "");
   s_Before = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.FEED);
   Ask("drag", ACR_PanelMover.GripForTest(ACR_UiPrefs.FEED), ws.DPIScale(60), ws.DPIScale(60));
  }
  if (tick == after + 43)
  {
   vector foldedNow = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.FEED);
   ACR_TestRegress.Check("panelshots: a drag on INTEL's bar moves it", foldedNow != s_Before, string.Format("%1 -> %2", s_Before, foldedNow));
   Shot("panel_5_folded");
   if (mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  }

  // A double click on each puts it back.
  int back = tick - (after + 47);
  if (back >= 0 && back < PANELS * BACK_TICKS)
  {
   int k = back / BACK_TICKS;
   if (back % BACK_TICKS == 0) Ask("double", ACR_PanelMover.GripForTest(Id(k)), 0, 0);
   if (back % BACK_TICKS == BACK_TICKS - 1) ACR_TestRegress.Check("panelshots: a double click puts it back: " + Id(k), !ACR_UiPrefs.Moved(Id(k)), "");
  }
  int done = after + 47 + PANELS * BACK_TICKS;
  if (tick == done)
  {
   Shot("panel_6_back");
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  }
  if (tick == done + 6)
  {
   ACR_TestRegress.Check("panelshots: the strip is back after the map", ACR_KeyHints.ShownForTest(), "");
   FileIO.DeleteFile(ACR_UiPrefs.TEST_PREFS);
   ACR_TestRegress.Complete("panelshots", 39);
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
