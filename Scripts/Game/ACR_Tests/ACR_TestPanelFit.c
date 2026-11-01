// -acrPanelFitTest 1 (a game client, not srv; ~15 min): the ACA panel never runs off the screen and every
// button on it can be reached. Real mouse input comes from outside, as for ACR_TestPanelShots ("[ACR]
// PANELSHOTS DO" lines). First on the screen the game started on, through the real panel code: a squad,
// then a FOB: the squad's button goes, the built upgrades' words go, the upgrades fold by a real click and
// the file remembers it. Radio lines stand clear of the panels on a map opened a second time and with a
// hint up. The wheel scrolls the panel and doesn't zoom the map, a drag on the panel's face moves it. Then,
// with the test alone setting what the panel shows (held), every state in both UI sizes in each window size
// of Screen: the panel stands wholly on the screen, and each button is wholly in view at some point of the
// scroll. A FOB with everything open, in each of them, and a squad's in one: from the top the wheel alone
// reaches the end of a long scroll, past the buttons under the cursor; the last button was out of view and
// now shows, and a real click presses it. Prefs go through a test file.
// -acrPanelFitClassicTest 1 (~3 min): the same on the classic map, in the first window and size only.
class ACR_TestPanelFit
{
 static bool s_bHeld;    // ACR_UpdatePanel leaves the panel to Show
 static bool s_bFakeFob; // the real panel code, on a base dressed as a FOB
 static int s_iLastAction = -1;

 protected static const int NOTHING = 0;
 protected static const int SQUAD = 1;
 protected static const int SQUAD_BUILD = 2;
 protected static const int SQUADS = 3;
 protected static const int BASE = 4;
 protected static const int FOB = 5;
 protected static const int FOB_OPEN = 6;
 protected static const int REQUISITION = 7;
 protected static const int SUPPORTS = 8;
 protected static const int REPORT = 9;
 protected static const int ALL_OPEN = 10;
 protected static const int FOB_ALL = 11;
 protected static const int STATES = 12;

 protected static const int REAL_AT = 34;
 protected static const int CHAT_AT = REAL_AT + 15;
 protected static const int MOUSE_AT = CHAT_AT + 20;
 protected static const int SWEEP_AT = MOUSE_AT + 22;

 protected static int s_iState;
 protected static float s_fZoom;
 protected static int s_iWantAction;
 protected static bool s_bLastHidden;
 protected static int s_iNotches;
 protected static vector s_Before;

 // The sweep.
 protected static int s_iScreen;
 protected static int s_iSize;
 protected static int s_iSweepState;
 protected static int s_iStep;
 protected static int s_iWait;
 protected static int s_iScroll;
 protected static bool s_bSweepDone;
 protected static int s_iWindowMode;
 protected static int s_iWindowW;
 protected static int s_iWindowH;
 protected static bool s_bWindowRead;

 protected static string StateName(int state)
 {
  array<string> names = {"nothing", "squad", "squad_build", "squads", "base", "fob", "fob_open", "requisition", "supports", "report", "all_open", "fob_all"};
  return names[state];
 }

 // Width, height; the first entry (0, 0) is the screen the game started on.
 protected static int ScreenCount()
 {
  if (!ACR_MapDeclutter.On()) return 1;
  return 4;
 }

 // The classic map has one size.
 protected static int SizeCount()
 {
  if (!ACR_MapDeclutter.On()) return 1;
  return 2;
 }

 protected static void Screen(int i, out int w, out int h)
 {
  array<int> widths = {0, 1531, 1280, 1024};
  array<int> heights = {0, 1339, 720, 768};
  w = widths[i];
  h = heights[i];
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (tick == 2)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "1");
   if (System.IsCLIParam("acrPanelFitClassicTest"))
   {
    ACR_Options.Set(ACR_MapDeclutter.OPTION, "0");
    ACR_MapDeclutter.SetOn(false);
   }
  }
  if (!pc || tick < 4) return;
  if (tick == 4)
  {
   ACR_UiPrefs.UseTestFileForTest();
   ACR_Manager manager = ACR_Manager.GetInstance();
   ACR_Intel intel = null;
   if (manager) intel = manager.IntelFor("US");
   if (intel && h.m_Range)
   {
    intel.AddFeed(GetGame().GetWorld().GetWorldTime(), "Alpha 1: enemy rifle squad near the test range", h.m_Range.m_Rally + Vector(300, 0, 300));
    intel.AddFeed(GetGame().GetWorld().GetWorldTime(), "Bravo 2: enemy vehicle on the road north of Farm", h.m_Range.m_Rally + Vector(-300, 0, 300));
   }
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (tick == 22) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (tick == 24 && mapEntity) mapEntity.ZoomPanSmooth(ws.GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  if (tick == 28)
  {
   pc.ACR_TestTwoICShown(true);
   if (!pc.ACR_IsCommanderForHints()) pc.ACR_OnPanelAction(ACR_EPanelAction.TOGGLE_COMMAND);
  }
  ACR_CommandPanel panel = pc.ACR_CommandPanelForTest();
  if (s_bHeld && panel) Show(panel, s_iState);
  if (s_bFakeFob) pc.ACR_FitPokeFob();
  RunReal(pc, panel, tick - REAL_AT);
  RunChat(h, pc, mapEntity, tick - CHAT_AT);
  RunMouse(panel, mapEntity, tick - MOUSE_AT);
  if (tick >= SWEEP_AT) RunSweep(pc, panel);
 }

 // ---------------------------------------------------------------- the real panel code
 protected static void RunReal(SCR_PlayerController pc, ACR_CommandPanel panel, int at)
 {
  if (at < 0 || at > 14 || !panel) return;
  if (at == 0)
  {
   ACR_SquadsList list = pc.ACR_SquadsListForTest();
   if (list && list.RowsForTest() > 0) pc.ACR_SquadsListPick(list.IdForTest(0));
  }
  if (at == 3)
  {
   Log(panel, "real squad");
   Shot("real_squad");
   s_bFakeFob = true;
   pc.ACR_FitPickBase();
  }
  if (at == 7)
  {
   Log(panel, "real fob");
   ACR_TestRegress.Check("panelfit: a FOB shows no squad's button", !panel.FitSquadAIShown(), "");
   ACR_TestRegress.Check("panelfit: a FOB's built upgrades aren't listed in words as well", !panel.FitDetail().Contains("(built)") && panel.FitDetail().Contains("Ammo Storage (40%)"), panel.FitDetail());
   Shot("real_fob");
   Ask("click", panel.FitUpgradesFold(), 0, 0);
  }
  if (at == 12)
  {
   Log(panel, "real fob open");
   ACR_TestRegress.Check("panelfit: a real click unfolds the FOB's upgrades", ACR_UiPrefs.s_bUpgradesOpen && panel.FitUpgradesShown(), "");
   Shot("real_fob_open");
   ACR_UiPrefs.ResetForTest();
   ACR_UiPrefs.Load();
   ACR_TestRegress.Check("panelfit: and the file remembers it", ACR_UiPrefs.s_bUpgradesOpen, "");
   ACR_UiPrefs.s_bUpgradesOpen = false;
  }
  if (at == 14)
  {
   s_bFakeFob = false;
   pc.ACR_FitClearSelection();
  }
 }

 // ---------------------------------------------------------------- radio lines and INTEL
 protected static void RunChat(ACR_TestHarness h, SCR_PlayerController pc, SCR_MapEntity mapEntity, int at)
 {
  if (at < 0 || at > 19) return;
  if (at == 0 || at == 11) Radio();
  if (at == 2)
  {
   float first = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelfit: radio lines stand clear of the panels", first >= 0, string.Format("%1", first));
   Shot("chat_first");
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
  }
  if (at == 6) GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  if (at == 8 && mapEntity) mapEntity.ZoomPanSmooth(GetGame().GetWorkspace().GetWidth() / 2400.0, h.m_Range.m_Rally[0], h.m_Range.m_Rally[2], 0.1);
  if (at == 13)
  {
   float again = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelfit: and on a map opened a second time", again >= 0, string.Format("%1", again));
   Shot("chat_reopened");
   SCR_HintManagerComponent.ShowCustomHint("Play as character: the hint a player saw over INTEL, long enough to need more than one line of the box.", "AC: Actual", 30, true);
   Radio();
  }
  if (at == 17)
  {
   float withHint = ACR_PanelMover.ChatLeftForTest();
   ACR_TestRegress.Check("panelfit: and with a hint up", withHint >= 0, string.Format("%1", withHint));
   ACR_TestRegress.Check("panelfit: the hint stands clear of INTEL and the panel", ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.FEED) && ACR_PanelMover.ClearOfHintForTest(ACR_UiPrefs.PANEL), ACR_PanelMover.HintPathForTest());
   Shot("chat_hint");
   SCR_HintManagerComponent.HideHint();
  }
 }

 protected static void Radio()
 {
  SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
  if (!chat) return;
  for (int line = 1; line <= 5; line++) chat.OnNewMessage(string.Format("[Radio] 1st Squad: contact, %1 infantry, 150 m E of Farm.", line));
 }

 // ---------------------------------------------------------------- real mouse
 protected static void RunMouse(ACR_CommandPanel panel, SCR_MapEntity mapEntity, int at)
 {
  if (at < 0 || at > 20 || !panel) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (at == 0)
  {
   s_bHeld = true;
   s_iState = ALL_OPEN;
  }
  if (at == 3)
  {
   Log(panel, "mouse all open");
   if (mapEntity) s_fZoom = mapEntity.GetCurrentZoom();
   Ask("wheel", panel.FitBodyMiddle(), -3, 0);
  }
  if (at == 8)
  {
   ACR_TestRegress.Check("panelfit: the wheel scrolls the panel", panel.FitSlider() > 0.01, string.Format("%1", panel.FitSlider()));
   ACR_TestRegress.Check("panelfit: and doesn't zoom the map", mapEntity && Math.AbsFloat(mapEntity.GetCurrentZoom() - s_fZoom) < 0.0001, string.Format("%1 -> %2", s_fZoom, mapEntity.GetCurrentZoom()));
   ACR_TestRegress.Check("panelfit: and the panel stays open", !panel.DeclCollapsed(), "");
   Shot("mouse_wheeled");
   panel.FitScrollTo(0);
   s_iState = NOTHING;
  }
  if (at == 11)
  {
   s_Before = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.PANEL);
   Ask("drag", panel.FitBodyText(), ws.DPIScale(-140), ws.DPIScale(90));
  }
  if (at == 16)
  {
   vector moved = ACR_PanelMover.TopLeftForTest(ACR_UiPrefs.PANEL);
   ACR_TestRegress.Check("panelfit: a drag on the panel's face moves it", ACR_UiPrefs.Moved(ACR_UiPrefs.PANEL) && Math.AbsFloat(moved[0] - s_Before[0] + 140) < 30, string.Format("%1 -> %2", s_Before, moved));
   Ask("double", panel.FitBodyText(), 0, 0);
  }
  if (at == 20) ACR_TestRegress.Check("panelfit: a double click on its face sends it home", !ACR_UiPrefs.Moved(ACR_UiPrefs.PANEL), "");
 }

 // ---------------------------------------------------------------- every state, size and window
 protected static void RunSweep(SCR_PlayerController pc, ACR_CommandPanel panel)
 {
  if (s_bSweepDone || !panel) return;
  if (s_iWait > 0)
  {
   s_iWait--;
   return;
  }
  switch (s_iStep)
  {
   case 0: // a window size
    SetScreen(s_iScreen);
    s_iSize = 0;
    s_iStep = 1;
    s_iWait = 5;
    return;
   case 1: // a UI size
    pc.ACR_DeclSetCompact(s_iSize == 0);
    s_iSweepState = 0;
    s_iStep = 2;
    s_iWait = 2;
    return;
   case 2: // a state
    s_iState = s_iSweepState;
    panel.FitScrollTo(0);
    s_iScroll = 0;
    panel.FitSeenClear();
    s_iStep = 3;
    s_iWait = 1;
    return;
   case 3: // what it looks like, and on the screen
    Measure(panel);
    panel.FitSeenAdd();
    if (panel.FitOverflows()) s_iStep = 4;
    else s_iStep = 5;
    return;
   case 4: // down its scroll
    panel.FitSeenAdd();
    s_iScroll++;
    if (s_iScroll > 4)
    {
     if (s_iSweepState == ALL_OPEN) Shot(ShotName("_end"));
     s_iStep = 5;
     return;
    }
    panel.FitScrollTo(s_iScroll * 0.25);
    return;
   case 5:
    string missing = panel.FitUnseen();
    ACR_TestRegress.Check(string.Format("panelfit: every button can be reached: %1", ShotName("")), missing.IsEmpty(), missing);
    if (Deep())
    {
     panel.FitScrollTo(0);
     s_iStep = 6;
     s_iWait = 1;
     return;
    }
    Advance(pc);
    return;
   case 6: // back at the top: the last button, then the wheel alone
    panel.FitLastButton(s_iWantAction);
    s_bLastHidden = !panel.FitActionInView(s_iWantAction);
    s_iNotches = 0;
    s_iStep = 7;
    return;
   case 7:
    if (panel.FitAtEnd() || s_iNotches >= 240)
    {
     s_iStep = 8;
     s_iWait = 1;
     return;
    }
    Ask("wheel", panel.FitBodyMiddle(), -6, 0);
    s_iNotches += 6;
    s_iWait = 1;
    return;
   case 8:
    string deepName = ShotName("");
    ACR_TestRegress.Check("panelfit: the wheel alone reaches the end of a long scroll: " + deepName, panel.FitAtEnd() && panel.FitOverflow() > 300, string.Format("%1 notches, %2 reference px", s_iNotches, panel.FitOverflow()));
    ACR_TestRegress.Check("panelfit: the last button was out of view, and now shows: " + deepName, s_bLastHidden && s_iWantAction == LastAction() && panel.FitActionInView(s_iWantAction), string.Format("action %1, hidden %2", s_iWantAction, s_bLastHidden));
    Shot(ShotName("_wheeled_end"));
    s_iLastAction = -1;
    Ask("click", panel.FitActionMiddle(s_iWantAction), 0, 0);
    s_iStep = 9;
    s_iWait = 4;
    return;
   case 9:
    ACR_TestRegress.Check("panelfit: a real click presses it: " + ShotName(""), s_iLastAction == s_iWantAction && s_iWantAction >= 0, string.Format("%1, wanted %2", s_iLastAction, s_iWantAction));
    pc.ACR_FitClearCursor();
    panel.FitScrollTo(0);
    Advance(pc);
    return;
  }
 }

 // Where the real wheel and click are tried: a FOB in every window and size, a squad in the smallest
 // window that keeps the reference height of 1080.
 protected static bool Deep()
 {
  return s_iSweepState == FOB_ALL || (s_iSweepState == ALL_OPEN && s_iScreen == 2);
 }

 protected static int DeepCount()
 {
  int deep = ScreenCount() * SizeCount();
  if (ScreenCount() > 2) deep += SizeCount();
  return deep;
 }

 protected static int LastAction()
 {
  if (s_iSweepState == FOB_ALL) return ACR_EPanelAction.FOB_UPGRADE + ACR_FobSlots.SLOT_DEFENCES;
  return ACR_EPanelAction.FORTIFY;
 }

 protected static void Advance(SCR_PlayerController pc)
 {
  s_iSweepState++;
  s_iStep = 2;
  if (s_iSweepState < STATES) return;
  s_iSize++;
  s_iStep = 1;
  if (s_iSize < SizeCount()) return;
  s_iScreen++;
  s_iStep = 0;
  if (s_iScreen < ScreenCount()) return;
  s_bSweepDone = true;
  RestoreScreen();
  pc.ACR_DeclSetCompact(true);
  s_bHeld = false;
  FileIO.DeleteFile(ACR_UiPrefs.TEST_PREFS);
  ACR_Options.Unset(ACR_MapDeclutter.OPTION);
  ACR_TestRegress.Complete("panelfit", 13 + ScreenCount() * SizeCount() * STATES * 2 + DeepCount() * 3);
 }

 protected static string ShotName(string end)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  string size = "normal";
  if (s_iSize == 0) size = "compact";
  return string.Format("fit_%1x%2_%3_%4%5", ws.GetWidth(), ws.GetHeight(), size, StateName(s_iSweepState), end);
 }

 protected static void Measure(ACR_CommandPanel panel)
 {
  string name = ShotName("");
  Log(panel, name);
  string why;
  ACR_TestRegress.Check("panelfit: all of it on the screen: " + name, panel.FitOnScreen(why), why);
  // The 4K shots are large: only the FOB's there.
  if (GetGame().GetWorkspace().GetWidth() < 2000 || s_iSweepState == FOB || s_iSweepState == FOB_OPEN || s_iSweepState == FOB_ALL) Shot(name);
 }

 protected static void Log(ACR_CommandPanel panel, string name)
 {
  string why;
  panel.FitOnScreen(why);
  PrintFormat("[ACR] PANELFIT %1: %2", name, why);
 }

 // What the panel shows, whatever the game says.
 protected static void Show(ACR_CommandPanel panel, int state)
 {
  bool all = state == ALL_OPEN;
  bool fobAll = state == FOB_ALL;
  ACR_UiPrefs.s_bBuildOpen = state == SQUAD_BUILD || all;
  ACR_UiPrefs.s_bUpgradesOpen = state == FOB_OPEN || fobAll;
  ACR_UiPrefs.s_bMoreOpen = state == REPORT || state == FOB_OPEN || all || fobAll;
  panel.SetCommand("Command: you", true, false);
  panel.SetEconomy("RP 480   +12 in 34s");
  panel.SetRoster("");
  panel.SetReinforceFeedback("");
  array<string> names = {"Rifle Squad", "Machine Gun Team", "Anti-Tank Team", "Sniper Team", "Engineer Squad", "Medic Team", "Truck", "Humvee", "Humvee (M2)", "LAV-25", "Mortar Team", "Supply Truck", "Transport Helicopter", "Gunship"};
  array<int> costs = {};
  array<int> left = {};
  foreach (string entry : names)
  {
   costs.Insert(20);
   left.Insert(6);
  }
  panel.SetRequisitions(true, state == REQUISITION || all || fobAll, names, costs, left, 480, -1);
  array<string> supports = {"Mortar Barrage", "Smoke Barrage", "Artillery", "Gunship Strafe", "Air Lift", "Supply Drop", "Reveal Enemy", "UAV"};
  array<int> supportCosts = {};
  array<int> cooldowns = {};
  foreach (string support : supports)
  {
   supportCosts.Insert(30);
   cooldowns.Insert(0);
  }
  panel.SetSupports(true, state == SUPPORTS, supports, supportCosts, cooldowns, 480, -1);
  string report = "After-action report";
  for (int r = 1; r <= 30; r++) report += string.Format("\n%1:%2  a line of the report, long enough to run on to a second line of the panel", 10 + r, r);
  panel.SetReport(state == REPORT || all || fobAll, report);
  bool squad = state == SQUAD || state == SQUAD_BUILD || state == REQUISITION || state == SUPPORTS || state == REPORT || all;
  if (squad)
  {
   panel.SetEstablishFob(true);
   panel.SetRoadblock(true, true);
   panel.SetMines(true);
   panel.SetFortify(true);
   panel.SetGroup("1st Squad", "Rifle Squad | 8 men, 2 missing | ammo 74%\nMoving to Farm, 1.2 km\nOrdered by the commander", true, true, 0, ACR_ECursorMode.MOVE, "Right-click: move there  |  Shift+right-click: add waypoint  |  Ctrl+drag: select squads");
   panel.SetGunner(true);
   panel.SetSquadAI(true, false);
   panel.SetPatrol(false);
   panel.SetReinforce("Reinforce (3 RP/soldier; FOB instant service extra)", true);
   return;
  }
  if (state == SQUADS)
  {
   panel.SetEstablishFob(false);
   panel.SetRoadblock(false, true);
   panel.SetMines(false);
   panel.SetFortify(false);
   panel.SetGroup("12 squads selected", "1st Squad, 2nd Squad, 3rd Squad, 4th Squad, Weapons Squad, Recon Team, Engineer Squad, Armour 1, Armour 2, Transport 1, Transport 2, Mortar Team | orders go to all (switch: first)", true, true, 0, ACR_ECursorMode.MOVE, "Right-click: move there  |  Shift+right-click: add waypoint  |  Ctrl+drag: select squads");
   panel.SetGunner(false);
   panel.SetSquadAI(false, false);
   panel.SetPatrol(false);
   panel.SetReinforce("Reinforce (3 RP/soldier; FOB instant service extra)", true);
   return;
  }
  if (state == BASE)
  {
   panel.SetBase("Farm [2]", "Held by US | garrison 12 | 2 vehicles\nDeploy base, income +3");
   panel.SetFob(false, false, false);
   return;
  }
  if (state == FOB || state == FOB_OPEN || fobAll)
  {
   string words = FobWords();
   array<int> slots = FobSlots();
   // Nothing being built, or every upgrade button is greyed out and takes no click.
   if (fobAll)
   {
    words.Replace("\n  Ammo Storage (40%)", "");
    slots[2] = 0;
   }
   panel.SetBase("FOB 1", ACR_CommandPanel.FobDetail(words));
   panel.SetFob(true, true, true, slots, 400, true, "US");
   panel.SetFobAI(true, false);
   return;
  }
  panel.SetGroup("", "", false, false, 0, 0, "");
 }

 // The words a FOB comes with (ACR_FobSupply.Describe, ACR_FobSlots.Describe), as a playtester saw them.
 static string FobWords()
 {
  return "Held by US | garrison 8\nSupplies 400 / 1000\nDelivery in 40 s\nUpgrades:\n  Field Hospital (built)\n  Light Maintenance (built)\n  Ammo Storage (40%)\n  Fuel Storage (built)\n  Radio Relay: spot blocked (a vehicle is on it)\n  Barracks (Small) (built)\n  Defences I (sandbag positions) (6 positions)";
 }

 // ACR_FobSlots.FeedState: built, blocked, building, free.
 static array<int> FobSlots()
 {
  array<int> slots = {300, 300, 50, 300, 0, 1, 300, 0, 300};
  return slots;
 }

 // ---------------------------------------------------------------- window sizes
 protected static void SetScreen(int i)
 {
  int w, h;
  Screen(i, w, h);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  BaseContainer video = GetGame().GetEngineUserSettings().GetModule("VideoUserSettings");
  if (video && !s_bWindowRead)
  {
   s_bWindowRead = true;
   video.Get("WindowMode", s_iWindowMode);
   video.Get("ScreenWidth", s_iWindowW);
   video.Get("ScreenHeight", s_iWindowH);
  }
  if (video && w > 0)
  {
   video.Set("WindowMode", EWindowMode.WINDOWED);
   video.Set("ScreenWidth", w);
   video.Set("ScreenHeight", h);
   GetGame().UserSettingsChanged();
  }
  PrintFormat("[ACR] PANELFIT window asked %1 x %2, now %3 x %4, reference %5 x %6", w, h, ws.GetWidth(), ws.GetHeight(), ws.DPIUnscale(ws.GetWidth()), ws.DPIUnscale(ws.GetHeight()));
 }

 protected static void RestoreScreen()
 {
  BaseContainer video = GetGame().GetEngineUserSettings().GetModule("VideoUserSettings");
  if (!video || !s_bWindowRead) return;
  video.Set("WindowMode", s_iWindowMode);
  video.Set("ScreenWidth", s_iWindowW);
  video.Set("ScreenHeight", s_iWindowH);
  GetGame().UserSettingsChanged();
 }

 // The middle of the rectangle, and how far to drag from it (wheel: notches in dx, down is negative).
 protected static void Ask(string what, vector rect, float dx, float dy)
 {
  if (rect == vector.Zero) return;
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

modded class SCR_PlayerController
{
 override protected void ACR_UpdatePanel()
 {
  if (ACR_TestPanelFit.s_bHeld && m_ACR_Panel)
  {
   m_ACR_Panel.TickDrag();
   return;
  }
  super.ACR_UpdatePanel();
 }

 // Tests: the first base selected.
 void ACR_FitPickBase()
 {
  array<RplId> none = {};
  ACR_SetSelection(none);
  if (m_ACR_Data && !m_ACR_Data.m_BaseNames.IsEmpty()) m_ACR_SelectedBase = m_ACR_Data.m_BaseNames[0];
 }

 void ACR_FitClearSelection() { m_ACR_SelectedBase = ""; }
 void ACR_FitClearCursor() { m_ACR_CursorMode = ACR_ECursorMode.MOVE; }

 // Tests: the selected base dressed as a FOB, each second: what the server's feed would bring.
 void ACR_FitPokeFob()
 {
  if (!m_ACR_Data || m_ACR_Data.m_BaseNames.IsEmpty()) return;
  int stride = 3 + ACR_FobSlots.UPGRADE_COUNT;
  while (m_ACR_Data.m_BaseSupply.Count() < stride) m_ACR_Data.m_BaseSupply.Insert(0);
  m_ACR_Data.m_BaseSupply[0] = 400;
  m_ACR_Data.m_BaseSupply[1] = 1000;
  m_ACR_Data.m_BaseSupply[2] = 0;
  array<int> slots = ACR_TestPanelFit.FobSlots();
  foreach (int s, int slot : slots) m_ACR_Data.m_BaseSupply[3 + s] = slot;
  m_ACR_Data.m_BaseDetails[0] = ACR_TestPanelFit.FobWords();
 }
}

modded class ACR_CommandPanel
{
 protected ref array<Widget> m_aFitSeen = {};

 override protected void OnButton(SCR_ButtonBaseComponent button)
 {
  int action;
  if (m_Actions.Find(button, action)) ACR_TestPanelFit.s_iLastAction = action;
  super.OnButton(button);
 }

 bool FitSquadAIShown() { return m_SquadAIWidget && m_SquadAIWidget.IsVisibleInHierarchy(); }

 string FitDetail()
 {
  if (m_GroupDetail) return m_GroupDetail.GetText();
  return "";
 }

 // All of the panel on the screen; its numbers, in screen pixels.
 bool FitOnScreen(out string why)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, w, h;
  m_Root.GetScreenPos(x, y);
  m_Root.GetScreenSize(w, h);
  why = string.Format("top %1 bottom %2 left %3 right %4 of %5 x %6 (scale %7), %8", y, y + h, x, x + w, ws.GetWidth(), ws.GetHeight(), ws.DPIScale(100) / 100, FitBodyWords());
  return y >= -1 && x >= -1 && y + h <= ws.GetHeight() + 1 && x + w <= ws.GetWidth() + 1;
 }

 // What must be reachable: the buttons, the box of a list that's open (its rows scroll in it), the hint.
 protected void FitElements(notnull array<Widget> elements)
 {
  foreach (SCR_ButtonBaseComponent button, int action : m_Actions)
  {
   if (action >= ACR_EPanelAction.REQUISITION_ENTRY && action < ACR_EPanelAction.FOB_UPGRADE) continue;
   if (!button || !button.GetRootWidget() || !button.GetRootWidget().IsVisibleInHierarchy() || !FitOurs(button.GetRootWidget())) continue;
   elements.Insert(button.GetRootWidget());
  }
  if (m_DeclCompact) elements.Insert(m_DeclCompact.GetRootWidget());
  if (m_DeclNormal) elements.Insert(m_DeclNormal.GetRootWidget());
  array<Widget> others = {m_RequisitionBox, m_SupportBox, m_Hint};
  foreach (Widget other : others)
  {
   if (other) elements.Insert(other);
  }
  for (int i = elements.Count() - 1; i >= 0; i--)
  {
   if (!elements[i] || !elements[i].IsVisibleInHierarchy()) elements.RemoveOrdered(i);
  }
 }

 protected bool FitOurs(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 // Wholly in view, on the screen and not clipped by the panel's scroll.
 protected bool FitInView(Widget widget)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float x, y, w, h;
  widget.GetScreenPos(x, y);
  widget.GetScreenSize(w, h);
  float top = 0;
  float bottom = ws.GetHeight();
  FitClip(top, bottom);
  return w > 0 && h > 0 && y >= top - 1 && y + h <= bottom + 1 && y >= -1 && y + h <= ws.GetHeight() + 1;
 }

 void FitSeenClear() { m_aFitSeen.Clear(); }

 void FitSeenAdd()
 {
  array<Widget> elements = {};
  FitElements(elements);
  foreach (Widget element : elements)
  {
   if (FitInView(element) && !m_aFitSeen.Contains(element)) m_aFitSeen.Insert(element);
  }
 }

 // What was never in view, by name.
 string FitUnseen()
 {
  array<Widget> elements = {};
  FitElements(elements);
  string missing = "";
  foreach (Widget element : elements)
  {
   if (m_aFitSeen.Contains(element)) continue;
   string name = element.GetName();
   SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(element);
   if (button) name = button.GetText();
   missing += "[" + name + "] ";
  }
  return missing;
 }

 // The middle of the panel's lowest button, in view or not (screen pixels), and its action.
 vector FitLastButton(out int action)
 {
  action = -1;
  float lowest = -1000000;
  vector middle = vector.Zero;
  foreach (SCR_ButtonBaseComponent button, int candidate : m_Actions)
  {
   if (candidate >= ACR_EPanelAction.REQUISITION_ENTRY && candidate < ACR_EPanelAction.FOB_UPGRADE) continue;
   Widget widget = button.GetRootWidget();
   if (!widget || !widget.IsVisibleInHierarchy() || !FitOurs(widget)) continue;
   float x, y, w, h;
   widget.GetScreenPos(x, y);
   widget.GetScreenSize(w, h);
   if (y + h <= lowest) continue;
   lowest = y + h;
   action = candidate;
   middle = Vector(x + w * 0.5, y + h * 0.5, 0);
  }
  return middle;
 }

 protected Widget FitActionWidget(int action)
 {
  foreach (SCR_ButtonBaseComponent button, int candidate : m_Actions)
  {
   if (candidate == action) return button.GetRootWidget();
  }
  return null;
 }

 bool FitActionInView(int action)
 {
  Widget widget = FitActionWidget(action);
  return widget && widget.IsVisibleInHierarchy() && FitInView(widget);
 }

 vector FitActionMiddle(int action)
 {
  Widget widget = FitActionWidget(action);
  if (!widget) return vector.Zero;
  float x, y, w, h;
  widget.GetScreenPos(x, y);
  widget.GetScreenSize(w, h);
  return Vector(x + w * 0.5, y + h * 0.5, 0);
 }

 // The foot of the rows has come up to the foot of the body.
 bool FitAtEnd()
 {
  float x, y, w, h, boxX, boxY, boxW, boxH;
  m_Content.GetScreenPos(x, y);
  m_Content.GetScreenSize(w, h);
  m_BodyBox.GetScreenPos(boxX, boxY);
  m_BodyBox.GetScreenSize(boxW, boxH);
  return y + h <= boxY + boxH + 1;
 }

 // How far the rows run past the body, in reference pixels.
 float FitOverflow()
 {
  float w, h, contentW, contentH;
  m_BodyBox.GetScreenSize(w, h);
  m_Content.GetScreenSize(contentW, contentH);
  return GetGame().GetWorkspace().DPIUnscale(contentH - h);
 }

 // A line of text on the panel's face: its command line, or below the head where the body's rows start.
 vector FitBodyText()
 {
  if (!m_Status) return vector.Zero;
  float x, y, w, h;
  m_Status.GetScreenPos(x, y);
  m_Status.GetScreenSize(w, h);
  return Vector(x + w * 0.5, y + h * 0.5, 0);
 }

 vector FitUpgradesFold()
 {
  if (!m_FobUpgradesFold) return vector.Zero;
  float x, y, w, h;
  m_FobUpgradesFold.GetRootWidget().GetScreenPos(x, y);
  m_FobUpgradesFold.GetRootWidget().GetScreenSize(w, h);
  return Vector(x + w * 0.5, y + h * 0.5, 0);
 }

 bool FitUpgradesShown() { return m_FobUpgradesBox && m_FobUpgradesBox.IsVisibleInHierarchy(); }

 // The middle of the scrolling part: over a text or a button of it, for the wheel.
 vector FitBodyMiddle()
 {
  float x, y, w, h;
  m_BodyBox.GetScreenPos(x, y);
  m_BodyBox.GetScreenSize(w, h);
  return Vector(x + w * 0.5, y + h * 0.9, 0);
 }

 protected void FitClip(out float top, out float bottom)
 {
  float x, y, w, h;
  m_BodyBox.GetScreenPos(x, y);
  m_BodyBox.GetScreenSize(w, h);
  top = y;
  bottom = y + h;
 }

 protected string FitBodyWords()
 {
  float w, h, contentW, contentH;
  m_BodyBox.GetScreenSize(w, h);
  m_Content.GetScreenSize(contentW, contentH);
  return string.Format("body %1 of %2, scrolled %3", h, contentH, FitSlider());
 }

 bool FitOverflows()
 {
  float w, h, contentW, contentH;
  m_BodyBox.GetScreenSize(w, h);
  m_Content.GetScreenSize(contentW, contentH);
  return contentH > h + 1;
 }

 void FitScrollTo(float y) { m_BodyScroll.SetSliderPos(0, y); }

 float FitSlider()
 {
  float x, y;
  m_BodyScroll.GetSliderPos(x, y);
  return y;
 }
}
