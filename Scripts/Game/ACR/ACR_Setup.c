// Setup phase (Workshop host options, step 2, 13 Oct). With acrSetup on (our shipped
// scenarios' headers turn it on; the launchers pass -acrSetup 0, tests start from a world
// without a header), nothing starts until the host or an admin presses Start: no ticks,
// no skirmish layout, no starting units, no player spawns. Meanwhile they see a dialog
// with the match options; each click cycles one option on the server (ACR_Options.Set).
// A dedicated server with nobody allowed to start begins by itself AUTO_START_S after the
// first player joined.
class ACR_Setup
{
 static const int AUTO_START_S = 300;
 protected static bool s_bDecided;
 protected static bool s_bWanted;
 protected static bool s_bStarted;
 protected static float s_fFirstPlayerAt = -1;
 protected static ref map<int, string> s_Sent = new map<int, string>();

 static bool Wanted()
 {
  if (!s_bDecided)
  {
   s_bDecided = true;
   string value;
   s_bWanted = ACR_Options.Get("acrSetup", value) && value == "1";
   if (System.IsCLIParam("acrSetupTest") || System.IsCLIParam("acrSetupUiTest")) s_bWanted = true;
   if (s_bWanted) Print("[ACR] SETUP phase: waiting for the host or an admin to press Start");
  }
  return s_bWanted;
 }

 // Gameplay runs (server). Always true without a setup phase.
 static bool Started()
 {
  if (!Replication.IsServer()) return true;
  return s_bStarted || !Wanted();
 }

 static void Reset()
 {
  s_bDecided = false;
  s_bWanted = false;
  s_bStarted = false;
  s_fFirstPlayerAt = -1;
  s_Sent.Clear();
 }

 // Tests: force the phase on or off.
 static void ForceForTest(bool wanted)
 {
  s_bDecided = true;
  s_bWanted = wanted;
  s_bStarted = false;
 }

 static void Start(string who)
 {
  if (s_bStarted) return;
  s_bStarted = true;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.m_Bases.ForgetFactions(); // buy lists and RP are built with the chosen options
  PrintFormat("[ACR] SETUP started by %1", who);
  ACR_Options.Log();
  Broadcast();
 }

 // Every second on the server while the phase lasts (from ACR_Manager.Tick).
 static void Tick()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  float now = GetGame().GetWorld().GetWorldTime();
  if (!players.IsEmpty() && s_fFirstPlayerAt < 0) s_fFirstPlayerAt = now;
  if (!GetGame().GetPlayerController() && s_fFirstPlayerAt >= 0 && now - s_fFirstPlayerAt >= AUTO_START_S * 1000)
  {
   Start("the timer (nobody pressed Start)");
   return;
  }
  Broadcast();
 }

 static int SecondsLeft()
 {
  if (GetGame().GetPlayerController() || s_fFirstPlayerAt < 0) return -1;
  return Math.Max(0, AUTO_START_S - (GetGame().GetWorld().GetWorldTime() - s_fFirstPlayerAt) / 1000);
 }

 // Each player's view of the phase, sent when it changes.
 protected static void Broadcast()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  string rows = Rows();
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   bool active = !s_bStarted;
   bool allowed = ACR_Manager.IsHostOrAdmin(id);
   string text = rows;
   if (!allowed) text = "";
   int left = SecondsLeft();
   string info = Info(left);
   string signature = string.Format("%1|%2|%3|%4", active, allowed, text, info);
   string sent;
   if (s_Sent.Find(id, sent) && sent == signature) continue;
   s_Sent.Set(id, signature);
   pc.ACR_SendSetup(active, allowed, text, info);
  }
 }

 protected static string Info(int left)
 {
  string info = string.Format("Factions: %1 v %2 (set when the server starts).", ACR_FactionSelection.DisplayName(ACR_FactionSelection.West()), ACR_FactionSelection.DisplayName(ACR_FactionSelection.East()));
  if (left >= 0) info += string.Format(" Starts by itself in %1 s.", left);
  return info;
 }

 // ---------------------------------------------------------------- options
 // The rows the dialog shows: "key\tLabel: value" lines.
 static string Rows()
 {
  string rows;
  array<string> keys = {"layout", "threat", "pricing", "ai", "unitcap", "startrp", "income", "shared"};
  foreach (string key : keys)
  {
   if (key == "layout" && ACR_Skirmish.s_All.IsEmpty()) continue;
   rows += key + "\t" + Label(key) + ": " + ValueName(key, Current(key)) + "\n";
  }
  return rows;
 }

 static string Label(string key)
 {
  if (key == "layout") return "Battlefield";
  if (key == "threat") return "Threat";
  if (key == "pricing") return "Prices";
  if (key == "ai") return "AI commanders";
  if (key == "unitcap") return "AI squad cap";
  if (key == "startrp") return "Starting RP";
  if (key == "income") return "Income";
  if (key == "shared") return "Shared command";
  return key;
 }

 // The values a row cycles through ("" = the scenario's default).
 static array<string> Values(string key)
 {
  array<string> values = {};
  if (key == "layout")
  {
   values.Insert("full");
   bool half, flash;
   foreach (ACR_Skirmish layout : ACR_Skirmish.s_All)
   {
    if (layout.m_eSize == ACR_ESkirmishSize.HALF) half = true;
    else flash = true;
   }
   if (half) values.Insert("size:half");
   if (flash) values.Insert("size:flashpoint");
   foreach (ACR_Skirmish named : ACR_Skirmish.s_All) values.Insert("name:" + named.m_sName);
  }
  else if (key == "threat") values = {"low", "normal", "high"};
  else if (key == "pricing") values = {"stepped", "stock"};
  else if (key == "ai")
  {
   values = {"both", "none"};
   values.Insert(ACR_FactionSelection.West());
   values.Insert(ACR_FactionSelection.East());
  }
  else if (key == "unitcap") values = {"", "4", "6", "8", "12", "16", "24"};
  else if (key == "startrp") values = {"", "50", "100", "200", "400", "800"};
  else if (key == "income") values = {"", "0.5", "1", "1.5", "2", "3"};
  else if (key == "shared") values = {"1", "0"};
  return values;
 }

 static string Option(string key)
 {
  if (key == "threat") return "acrThreat";
  if (key == "pricing") return "acrPricing";
  if (key == "ai") return "acrAI";
  if (key == "unitcap") return "acrUnitCap";
  if (key == "startrp") return "acrStartRP";
  if (key == "income") return "acrIncome";
  if (key == "shared") return "acrSharedCommand";
  return "";
 }

 static string Current(string key)
 {
  string value;
  if (key == "layout")
  {
   if (ACR_Options.Get("acrLayout", value) && value != "") return "name:" + value;
   if (ACR_Options.Get("acrSize", value) && value != "" && value != "full") return "size:" + value;
   return "full";
  }
  if (!ACR_Options.Get(Option(key), value)) value = "";
  value.ToLower();
  if (key == "threat" && value == "") return "normal";
  if (key == "pricing" && value == "") return "";
  if (key == "ai" && value == "") return "";
  if (key == "shared" && value == "") return "1";
  return value;
 }

 static string ValueName(string key, string value)
 {
  if (value == "")
  {
   if (key == "pricing" || key == "ai") return "scenario's choice";
   return "scenario default";
  }
  if (key == "layout")
  {
   if (value == "full") return "whole map";
   if (value == "size:half") return "a random half";
   if (value == "size:flashpoint") return "a random flashpoint";
   return value.Substring(5, value.Length() - 5);
  }
  if (key == "pricing" && value == "stock") return "fixed stock";
  if (key == "pricing") return "stepped";
  if (key == "ai")
  {
   if (value == "both" || value == "none") return value;
   return ACR_FactionSelection.DisplayName(value) + " only";
  }
  if (key == "income") return "x" + value;
  if (key == "shared")
  {
   if (value == "0") return "off";
   return "on";
  }
  return value;
 }

 // Next value of a row (server, host/admin checked by the caller).
 static string Cycle(string key)
 {
  if (s_bStarted) return "The match has started";
  array<string> values = Values(key);
  if (values.IsEmpty()) return "Unknown option";
  string current = Current(key);
  int index = -1;
  foreach (int i, string candidate : values)
  {
   string lower = candidate;
   lower.ToLower();
   string currentLower = current;
   currentLower.ToLower();
   if (lower == currentLower) index = i;
  }
  string next = values[(index + 1) % values.Count()];
  SetValue(key, next);
  Broadcast();
  return Label(key) + ": " + ValueName(key, next);
 }

 static void SetValue(string key, string value)
 {
  if (key == "layout")
  {
   if (value == "full") { ACR_Options.Set("acrLayout", ""); ACR_Options.Set("acrSize", "full"); }
   else if (value.StartsWith("size:")) { ACR_Options.Set("acrLayout", ""); ACR_Options.Set("acrSize", value.Substring(5, value.Length() - 5)); }
   else { ACR_Options.Set("acrLayout", value.Substring(5, value.Length() - 5)); ACR_Options.Set("acrSize", ""); }
   return;
  }
  string option = Option(key);
  if (option == "") return;
  // "" = back to the scenario's default: drop the screen's value (a flag or header may still set it).
  if (value == "") ACR_Options.Unset(option);
  else ACR_Options.Set(option, value);
 }

 // A client asked (server side).
 static string Handle(int playerId, string key)
 {
  if (!Wanted() || s_bStarted) return "The match has already started";
  if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Only the host or an admin can change the match setup";
  if (key == "start")
  {
   Start(string.Format("player %1", playerId));
   return "Match started";
  }
  return Cycle(key);
 }
}

// ---------------------------------------------------------------- client
// The dialog for the host/admin; a hint for everyone else.
class ACR_SetupClient
{
 static bool s_bActive;
 static bool s_bAllowed;
 static string s_sRows;
 static string s_sInfo;
 protected static ref ACR_SetupDialog s_Dialog;
 protected static float s_fHintAt = -100000;
 protected static bool s_bChanged;

 static void Receive(bool active, bool allowed, string rows, string info)
 {
  s_bActive = active;
  s_bAllowed = allowed;
  s_sRows = rows;
  s_sInfo = info;
  s_bChanged = true;
 }

 // Every frame on clients (ACR_Manager.OnFrame).
 static void Frame()
 {
  if (!s_bActive)
  {
   if (s_Dialog) { s_Dialog.Close(); s_Dialog = null; }
   return;
  }
  if (!s_bAllowed)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (now - s_fHintAt > 15000)
   {
    s_fHintAt = now;
    SCR_HintManagerComponent.ShowCustomHint("Waiting for the host or an admin to set up the match and press Start. " + s_sInfo, "AC: Actual", 14, true);
   }
   return;
  }
  if (!s_Dialog || !s_Dialog.GetRootWidget())
  {
   s_Dialog = new ACR_SetupDialog();
   SCR_ConfigurableDialogUi.CreateFromPreset(ACR_SetupDialog.PRESETS, "ACR_Setup", s_Dialog);
   s_bChanged = true;
  }
  if (s_bChanged && s_Dialog)
  {
   s_bChanged = false;
   s_Dialog.Fill(s_sRows, s_sInfo);
  }
 }

 static void Request(string key)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_RequestSetup(key);
 }

 static void Reset()
 {
  s_bActive = false;
  s_bAllowed = false;
  if (s_Dialog) s_Dialog.Close();
  s_Dialog = null;
 }
}

class ACR_SetupDialog : SCR_ConfigurableDialogUi
{
 static const ResourceName PRESETS = "{AC230926A00100F0}Configs/ACR_Dialogs.conf";
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected ref map<SCR_ButtonBaseComponent, string> m_Keys = new map<SCR_ButtonBaseComponent, string>();
 protected Widget m_wRows;

 void Fill(string rows, string info)
 {
  Widget root = GetRootWidget();
  if (!root) return;
  SetTitle("AC: Actual - match setup");
  SetMessage("Click an option to change it, then press Start. " + info);
  if (!m_wRows)
  {
   Widget content = root.FindAnyWidget("ContentVerticalLayout");
   if (!content) content = root.FindAnyWidget("ContentLayoutContainer");
   if (!content) return;
   m_wRows = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, content);
  }
  while (m_wRows.GetChildren()) m_wRows.GetChildren().RemoveFromHierarchy();
  m_Keys.Clear();
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, m_wRows);
   if (!widget) continue;
   AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
   SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
   if (!button) continue;
   button.SetText(parts[1]);
   button.m_OnClicked.Insert(OnRow);
   m_Keys.Set(button, parts[0]);
  }
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  string key;
  if (m_Keys.Find(button, key)) ACR_SetupClient.Request(key);
 }

 // "Start": the server ends the phase and closes the dialog everywhere.
 override protected void OnConfirm()
 {
  ACR_SetupClient.Request("start");
 }

 override protected void OnCancel()
 {
 }
}
