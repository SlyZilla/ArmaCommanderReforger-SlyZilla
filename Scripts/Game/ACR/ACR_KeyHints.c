// Per-player UI choices kept in the profile: where the ACA panel and INTEL stand on the command map
// (dragged there, ACR_CommandPanel) and whether the key reminders show.
class ACR_UiPrefs
{
 static const string PREFS = "$profile:ACA_CommandUi.txt";
 static const float PANEL_HOME_X = -24; // from the screen's right edge
 static const float PANEL_HOME_Y = 96;
 static const float FEED_HOME_X = 110;  // clear of the map toolbar on the left
 static const float FEED_HOME_Y = 96;
 static const float GRAB = 40;          // this much of a panel stays on the screen, to grab it by
 static float s_fPanelX = PANEL_HOME_X;
 static float s_fPanelY = PANEL_HOME_Y;
 static float s_fFeedX = FEED_HOME_X;
 static float s_fFeedY = FEED_HOME_Y;
 static bool s_bKeyHints = true;
 protected static bool s_bRead;

 static bool PanelMoved() { return s_fPanelX != PANEL_HOME_X || s_fPanelY != PANEL_HOME_Y; }
 static bool FeedMoved() { return s_fFeedX != FEED_HOME_X || s_fFeedY != FEED_HOME_Y; }

 static void Load()
 {
  if (s_bRead) return;
  s_bRead = true;
  if (!FileIO.FileExists(PREFS)) return;
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.READ);
  if (!file) return;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   line = line.Trim();
   if (line == "keys=0") s_bKeyHints = false;
   if (line == "keys=1") s_bKeyHints = true;
   if (line.StartsWith("panelx=")) s_fPanelX = line.Substring(7, line.Length() - 7).ToFloat();
   if (line.StartsWith("panely=")) s_fPanelY = line.Substring(7, line.Length() - 7).ToFloat();
   if (line.StartsWith("feedx=")) s_fFeedX = line.Substring(6, line.Length() - 6).ToFloat();
   if (line.StartsWith("feedy=")) s_fFeedY = line.Substring(6, line.Length() - 6).ToFloat();
  }
  file.Close();
 }

 static void Save()
 {
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.WRITE);
  if (!file) return;
  if (s_bKeyHints) file.WriteLine("keys=1");
  else file.WriteLine("keys=0");
  file.WriteLine("panelx=" + s_fPanelX.ToString());
  file.WriteLine("panely=" + s_fPanelY.ToString());
  file.WriteLine("feedx=" + s_fFeedX.ToString());
  file.WriteLine("feedy=" + s_fFeedY.ToString());
  file.Close();
 }

 // The panel stands from the right edge (x <= 0). A smaller screen than it was dragged on brings it
 // back on.
 static float ClampPanelX(float x, float screenW, float panelW) { return Math.Clamp(x, Math.Min(panelW - screenW, 0), 0); }
 static float ClampFeedX(float x, float screenW, float feedW) { return Math.Clamp(x, 0, Math.Max(screenW - feedW, 0)); }
 static float ClampY(float y, float screenH) { return Math.Clamp(y, 0, Math.Max(screenH - GRAB, 0)); }

 // Tests: the defaults again, reading the file on the next Load.
 static void ResetForTest()
 {
  s_fPanelX = PANEL_HOME_X;
  s_fPanelY = PANEL_HOME_Y;
  s_fFeedX = FEED_HOME_X;
  s_fFeedY = FEED_HOME_Y;
  s_bKeyHints = true;
  s_bRead = false;
 }
}

// The F keys as a reminder along the top of the screen, off the map (a player's request: they're easy
// to forget). The game draws each key as it is bound, rebound ones included. On by default; the
// button on the ACA panel or a key of the player's choosing (Controls, unbound by default) turns it
// off for good.
class ACR_KeyHints
{
 protected static Widget s_Box;
 protected static RichTextWidget s_Text;
 protected static string s_sShown;
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";

 protected static string Key(string action, string words)
 {
  return string.Format("<action name='%1' scale='1.1'/> %2", action, words);
 }

 // What the keys do now. Squad orders and the camera are the commander's.
 static string Line(bool commander, bool inVehicle)
 {
  string line = Key("ACR_Map", "Map");
  line += "     " + Key("ACR_Possess", "Take over") + "     " + Key("ACR_Return", "Command unit");
  if (!commander) return line;
  line += "     " + Key("ACR_Move", "Squad ahead");
  if (inVehicle) line += "     " + Key("ACR_Rally", "Drop passengers");
  else line += "     " + Key("ACR_Rally", "Squad rally");
  line += "     " + Key("ACR_RTSCamera", "3D camera");
  return line;
 }

 static void Toggle()
 {
  ACR_UiPrefs.Load();
  ACR_UiPrefs.s_bKeyHints = !ACR_UiPrefs.s_bKeyHints;
  ACR_UiPrefs.Save();
 }

 // Every frame (ACR_Manager, the local player).
 static void Update(bool commander, bool mapOpen, IEntity body)
 {
  ACR_UiPrefs.Load();
  bool wanted = ACR_UiPrefs.s_bKeyHints && !mapOpen && body && !ACR_RTSCameraManager.IsActive() && !GetGame().GetMenuManager().IsAnyMenuOpen();
  if (!wanted)
  {
   if (s_Box) s_Box.SetVisible(false);
   return;
  }
  if (!s_Box && !Build()) return;
  s_Box.SetVisible(true);
  string line = Line(commander, ACR_Transport.VehicleOf(body) != null);
  if (line == s_sShown) return;
  s_sShown = line;
  s_Text.SetText(line);
 }

 protected static bool Build()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget root = ACR_Hud.Root();
  if (!ws || !root) return false;
  s_Box = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, root);
  if (!s_Box) return false;
  FrameSlot.SetAnchorMin(s_Box, 0.5, 0);
  FrameSlot.SetAnchorMax(s_Box, 0.5, 0);
  FrameSlot.SetAlignment(s_Box, 0.5, 0);
  FrameSlot.SetPos(s_Box, 0, 6);
  FrameSlot.SetSizeToContent(s_Box, true);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, Color.FromRGBA(16, 22, 28, 150), 0, s_Box);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  s_Text = RichTextWidget.Cast(ws.CreateWidgets(LAYOUT_TEXT, s_Box));
  if (!s_Text)
  {
   ACR_Hud.Remove(s_Box);
   s_Box = null;
   return false;
  }
  AlignableSlot.SetPadding(s_Text, 12, 3, 12, 4);
  s_sShown = "";
  return true;
 }

 static bool ShownForTest() { return s_Box && s_Box.IsVisible(); }

 // The HUD layer went with the mission (ACR_Hud.Clear).
 static void Forget()
 {
  s_Box = null;
  s_Text = null;
  s_sShown = "";
 }
}
