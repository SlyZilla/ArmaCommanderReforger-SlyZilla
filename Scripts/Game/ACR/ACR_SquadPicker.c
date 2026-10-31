// The squads behind a "×N" on the calm map (playtest 30 Oct: "when many units are in the same place
// we get the 2x, 3x ... it's still hard to choose the right unit"). Before, each click on the icon
// selected the next squad of the pile in turn (ACR_CycleOverlap), with no name until it was picked.
// Now a click opens this list beside the icon: the base, then one numbered row per squad with its
// name, what kind it is (its symbol: infantry, motorized, armour ...), what it is doing (moving,
// fighting, holding: the icon's corner mark) and its health and ammo, the selected ones marked, and
// Select all. A row, or its number key (1-9), selects that squad (Shift adds it to the selection, as
// Shift + a marker click does); Select all selects them all. Hovering a row rings where that squad
// really stands (the icon sits under the base). It closes on a pick, a click elsewhere on the map, or
// when the map closes (SCR_PlayerController.ACR_PickGroup in ACR_PlayerMapDeclutter). While it is
// open a base marker under the cursor keeps its short caption (ACR_MapMarkers).
class ACR_SquadPicker : Managed
{
 static const float WIDTH = 470;
 static const int NAME_CHARS = 22;  // a longer name is cut short to fit the row
 static const int KEYS = 9;         // rows 1-9 have a number key
 static bool s_bOpen;

 Widget m_Root;
 protected SCR_PlayerController m_Owner;
 protected ref array<RplId> m_Ids = {};
 protected ref map<SCR_ButtonBaseComponent, int> m_Rows = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<ref ACR_SquadPickerHover> m_Hovers = {};

 // What a squad's symbol says it is, in a word.
 static string TypeWord(int icons)
 {
  if (icons & EMilitarySymbolIcon.ROTARY_WING) return "helicopter";
  if (icons & EMilitarySymbolIcon.FIXED_WING) return "aircraft";
  if (icons & EMilitarySymbolIcon.ARMOR) return "armour";
  if (icons & EMilitarySymbolIcon.SUPPLY) return "supply";
  if (icons & EMilitarySymbolIcon.MEDICAL) return "medical";
  if (icons & EMilitarySymbolIcon.MAINTENANCE) return "repair";
  if (icons & EMilitarySymbolIcon.MORTAR) return "mortar";
  if (icons & EMilitarySymbolIcon.ARTILLERY) return "artillery";
  if (icons & EMilitarySymbolIcon.MOTORIZED) return "motorized";
  if (icons & EMilitarySymbolIcon.RECON) return "recon";
  if (icons & EMilitarySymbolIcon.SNIPER) return "sniper";
  if (icons & EMilitarySymbolIcon.ANTITANK) return "AT";
  if (icons & EMilitarySymbolIcon.MACHINEGUN) return "MG";
  if (icons & EMilitarySymbolIcon.INFANTRY) return "infantry";
  return "";
 }

 // health, ammo: percent, -1 unknown. "HP 80% · ammo 35%".
 static string Strength(int health, int ammo)
 {
  string text = "";
  if (health >= 0) text = string.Format("HP %1%%", health);
  if (ammo >= 0)
  {
   if (!text.IsEmpty()) text += "  ·  ";
   text += string.Format("ammo %1%%", ammo);
  }
  return text;
 }

 // x, y: the cursor in native pixels.
 void Create(Widget parent, SCR_PlayerController owner, string title, array<RplId> ids, array<string> names, array<string> types, array<string> doings, array<string> strengths, array<bool> selected, float x, float y)
 {
  m_Owner = owner;
  m_Ids.Copy(ids);
  s_bOpen = true;
  Widget column = ACR_TwoICStyle.PanelBox(parent, WIDTH, 8, 8, m_Root, ACR_TwoICStyle.Rgb(16, 19, 23, 0.97));
  FrameSlot.SetAnchorMin(m_Root, 0, 0);
  FrameSlot.SetAnchorMax(m_Root, 0, 0);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1050);
  TextWidget head = ACR_TwoICStyle.Text(column, title, ACR_TwoICStyle.Grey(), 16, true);
  if (head) AlignableSlot.SetPadding(head, 6, 2, 6, 6);
  foreach (int i, string name : names)
  {
   if (name.Length() > NAME_CHARS) name = name.Substring(0, NAME_CHARS - 3) + "...";
   string key = "   ";
   if (i < KEYS) key = (i + 1).ToString() + "  ";
   string mark = "";
   if (i < selected.Count() && selected[i]) mark = "● ";
   string label = key + mark + name;
   if (i < types.Count() && !types[i].IsEmpty()) label += "  ·  " + types[i];
   if (i < doings.Count() && !doings[i].IsEmpty()) label += "  ·  " + doings[i];
   if (i < strengths.Count() && !strengths[i].IsEmpty()) label += "    " + strengths[i];
   SCR_ButtonTextComponent row = ACR_TwoICStyle.Button(column, label, ACR_TwoICStyle.BUTTON_MENU, WIDTH - 16, 16, 38);
   if (!row) continue;
   row.m_OnClicked.Insert(OnRow);
   m_Rows.Insert(row, i);
   Widget rowWidget = row.GetRootWidget();
   if (rowWidget)
   {
    ACR_SquadPickerHover hover = new ACR_SquadPickerHover();
    hover.m_Picker = this;
    hover.m_iIndex = i;
    rowWidget.AddHandler(hover);
    m_Hovers.Insert(hover);
   }
  }
  SCR_ButtonTextComponent all = ACR_TwoICStyle.Button(column, string.Format("Select all %1", ids.Count()), ACR_TwoICStyle.BUTTON_OUTLINE, WIDTH - 16, 16, 38);
  if (all) all.m_OnClicked.Insert(OnAll);
  Place(x, y);
 }

 // Beside the cursor, clear of the clicked icon and its "×N", kept on screen (as the ping picker:
 // ACR_CommandPanel.ShowPicker).
 protected void Place(float x, float y)
 {
  if (!m_Root) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float ux = ws.DPIUnscale(x);
  float uy = ws.DPIUnscale(y);
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  float w = WIDTH;
  float h = 60 + 42 * (m_Ids.Count() + 1);
  float px = ux + 60;
  float py = uy - 30;
  if (px + w > screenW - 8) px = ux - 60 - w;
  if (py + h > screenH - 8) py = screenH - 8 - h;
  px = Math.Clamp(px, 8, Math.Max(8, screenW - w - 8));
  py = Math.Clamp(py, 8, Math.Max(8, screenH - h - 8));
  FrameSlot.SetPos(m_Root, px, py);
 }

 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 void Destroy()
 {
  s_bOpen = false;
  if (m_Owner) m_Owner.ACR_SquadPickerHover(RplId.Invalid());
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
 }

 RplId IdAt(int i)
 {
  if (i < 0 || i >= m_Ids.Count()) return RplId.Invalid();
  return m_Ids[i];
 }

 // A number key: its row.
 void ChooseIndex(int i)
 {
  if (m_Owner && i >= 0 && i < m_Ids.Count()) m_Owner.ACR_SquadPickerChoose(m_Ids[i]);
 }

 void Hover(int i)
 {
  if (!m_Owner) return;
  if (i >= 0 && i < m_Ids.Count()) m_Owner.ACR_SquadPickerHover(m_Ids[i]);
  else m_Owner.ACR_SquadPickerHover(RplId.Invalid());
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  int i;
  if (m_Rows.Find(button, i)) ChooseIndex(i);
 }

 protected void OnAll(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_SquadPickerAll(m_Ids);
 }
}

// A row's hover: rings where its squad stands.
class ACR_SquadPickerHover : ScriptedWidgetEventHandler
{
 ACR_SquadPicker m_Picker;
 int m_iIndex;

 override bool OnMouseEnter(Widget w, int x, int y)
 {
  if (m_Picker) m_Picker.Hover(m_iIndex);
  return false;
 }

 override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
 {
  if (m_Picker) m_Picker.Hover(-1);
  return false;
 }
}
