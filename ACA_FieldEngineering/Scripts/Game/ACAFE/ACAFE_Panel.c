// The "Field engineering" orders on ACA's command panel, under the squad's Orders. Pick an
// order, then right-click the map: the first selected squad gets it (like Establish FOB).
modded class ACR_CommandPanel
{
 protected SCR_ButtonTextComponent m_ACAFE_Toggle;
 protected Widget m_wACAFE_Toggle;
 protected Widget m_ACAFE_List;
 protected ref array<SCR_ButtonTextComponent> m_aACAFE_Buttons = {};
 protected ref array<int> m_aACAFE_Orders = {};
 protected string m_sACAFE_Shown = "-";

 override void Create(Widget parent, SCR_PlayerController owner)
 {
  super.Create(parent, owner);
  ACAFE_CreateSection();
 }

 protected void ACAFE_CreateSection()
 {
  if (!m_OrderSection) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_aACAFE_Buttons.Clear();
  m_aACAFE_Orders.Clear();
  m_ACAFE_Toggle = AddButton(m_OrderSection, "Field engineering", ACAFE_Orders.PANEL_BASE);
  m_wACAFE_Toggle = m_LastButtonWidget;
  m_ACAFE_List = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  if (!m_ACAFE_List) return;
  AddText(m_ACAFE_List, LAYOUT_TEXT, "Checkpoint");
  Widget checkpoints = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_ACAFE_List);
  ACAFE_AddOrder(checkpoints, ACAFE_EOrder.CHECKPOINT_S);
  ACAFE_AddOrder(checkpoints, ACAFE_EOrder.CHECKPOINT_M);
  ACAFE_AddOrder(checkpoints, ACAFE_EOrder.CHECKPOINT_L);
  AddText(m_ACAFE_List, LAYOUT_TEXT, "Minefield");
  Widget mines = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_ACAFE_List);
  ACAFE_AddOrder(mines, ACAFE_EOrder.MINES_AP);
  ACAFE_AddOrder(mines, ACAFE_EOrder.MINES_AT);
  ACAFE_AddOrder(mines, ACAFE_EOrder.MINES_MIXED);
  Widget other = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_ACAFE_List);
  ACAFE_AddOrder(other, ACAFE_EOrder.ROADBLOCK);
  ACAFE_AddOrder(other, ACAFE_EOrder.CLEAR_MINES);
 }

 protected void ACAFE_AddOrder(Widget row, int order)
 {
  SCR_ButtonTextComponent button = AddButton(row, ACAFE_ButtonText(order), ACAFE_Orders.PANEL_BASE + order, true);
  m_aACAFE_Buttons.Insert(button);
  m_aACAFE_Orders.Insert(order);
 }

 static string ACAFE_ButtonText(int order)
 {
  switch (order)
  {
   case ACAFE_EOrder.CHECKPOINT_S: return "Small";
   case ACAFE_EOrder.CHECKPOINT_M: return "Medium";
   case ACAFE_EOrder.CHECKPOINT_L: return "Large";
   case ACAFE_EOrder.MINES_AP: return "AP";
   case ACAFE_EOrder.MINES_AT: return "AT";
   case ACAFE_EOrder.MINES_MIXED: return "Mixed";
   case ACAFE_EOrder.ROADBLOCK: return "Roadblock";
   case ACAFE_EOrder.CLEAR_MINES: return "Clear mines";
  }
  return ACAFE_Orders.ShortName(order);
 }

 // enabled: field engineering is on in this match; open: the list is expanded; armed: the
 // order waiting for a right-click (0 none).
 void ACAFE_SetState(bool enabled, bool open, int armed)
 {
  string key = string.Format("%1|%2|%3", enabled, open, armed);
  if (key == m_sACAFE_Shown) return;
  m_sACAFE_Shown = key;
  if (m_wACAFE_Toggle) m_wACAFE_Toggle.SetVisible(enabled);
  if (!enabled)
  {
   if (m_ACAFE_List) m_ACAFE_List.SetVisible(false);
   return;
  }
  if (m_ACAFE_Toggle)
  {
   if (open) m_ACAFE_Toggle.SetText("Field engineering  [close]");
   else m_ACAFE_Toggle.SetText("Field engineering");
  }
  if (m_ACAFE_List) m_ACAFE_List.SetVisible(open);
  foreach (int i, SCR_ButtonTextComponent button : m_aACAFE_Buttons)
  {
   if (!button) continue;
   string label = ACAFE_ButtonText(m_aACAFE_Orders[i]);
   if (m_aACAFE_Orders[i] == armed) label = "[" + label + "]";
   button.SetText(label);
  }
 }

 void ACAFE_SetHint(string hint)
 {
  if (m_Hint) m_Hint.SetText(hint);
 }

 override void Destroy()
 {
  super.Destroy();
  m_aACAFE_Buttons.Clear();
  m_aACAFE_Orders.Clear();
  m_ACAFE_Toggle = null;
  m_wACAFE_Toggle = null;
  m_ACAFE_List = null;
  m_sACAFE_Shown = "-";
 }
}

modded class SCR_PlayerController
{
 protected int m_iACAFE_Armed;                      // ACAFE_EOrder waiting for a right-click, 0 none
 protected RplId m_ACAFE_ArmedGroup = RplId.Invalid();
 protected bool m_bACAFE_Open;

 override void ACR_OnPanelAction(int action)
 {
  if (action < ACAFE_Orders.PANEL_BASE || action >= ACAFE_Orders.PANEL_BASE + ACAFE_Orders.COUNT)
  {
   super.ACR_OnPanelAction(action);
   return;
  }
  ACR_MarkUiClick();
  int order = action - ACAFE_Orders.PANEL_BASE;
  if (order == ACAFE_EOrder.NONE)
  {
   m_bACAFE_Open = !m_bACAFE_Open;
   if (!m_bACAFE_Open) m_iACAFE_Armed = 0;
   return;
  }
  // Same button again cancels, like ACA's own Defend / Establish FOB.
  if (m_iACAFE_Armed == order)
  {
   m_iACAFE_Armed = 0;
   return;
  }
  m_iACAFE_Armed = order;
  m_ACAFE_ArmedGroup = m_ACR_SelectedGroup;
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 override protected void ACR_UpdatePanel()
 {
  super.ACR_UpdatePanel();
  if (!m_ACR_Panel) return;
  bool enabled = ACAFE_Settings.Enabled();
  // A new selection, another armed order of ACA's, a buy or a support: ours is dropped.
  if (m_iACAFE_Armed != 0)
  {
   bool keep = enabled && m_ACR_IsCommander && m_ACR_SelectedGroup.IsValid() && m_ACR_SelectedGroup == m_ACAFE_ArmedGroup && m_ACR_CursorMode == ACR_ECursorMode.MOVE;
   keep = keep && m_ACR_PendingSupport < 0 && m_ACR_PendingEntry < 0 && m_ACR_SelectedBase.IsEmpty();
   if (!keep) m_iACAFE_Armed = 0;
  }
  m_ACR_Panel.ACAFE_SetState(enabled, m_bACAFE_Open, m_iACAFE_Armed);
  if (m_iACAFE_Armed != 0) m_ACR_Panel.ACAFE_SetHint(ACAFE_Orders.Hint(m_iACAFE_Armed));
 }

 override bool ACR_HandleMapRightClick()
 {
  if (m_iACAFE_Armed != 0 && ACAFE_SendArmed()) return true;
  return super.ACR_HandleMapRightClick();
 }

 // The armed order for the first selected squad at the cursor. false: let ACA handle the click.
 protected bool ACAFE_SendArmed()
 {
  if (!ACR_Manager.GetInstance() || ACR_EditorOpen() || ACR_ModifierHeld("ACR_MapAlt")) return false;
  if (m_ACR_PendingSupport >= 0 || m_ACR_PendingEntry >= 0 || !m_ACR_IsCommander) return false;
  if (!m_ACR_SelectedGroup.IsValid()) return false;
  int index = m_ACR_Data.m_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0 || index >= m_ACR_Data.m_Commandable.Count() || m_ACR_Data.m_Commandable[index] != 1) return false;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  vector point = Vector(x, 0, z);
  Rpc(ACAFE_OrderServer, m_iACAFE_Armed, m_ACR_SelectedGroup, point);
  ACR_AddPending(m_ACR_Data.LeaderPos(index), point);
  m_iACAFE_Armed = 0;
  return true;
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACAFE_OrderServer(int order, RplId groupId, vector point)
 {
  string message = ACAFE_Engineering.Order(this, order, groupId, point);
  if (!message.IsEmpty()) ACR_Feedback(message);
 }

 // ---------------------------------------------------------------- match setup page
 // ACA's setup screen gets our rows appended (server side, to hosts/admins only); a click on
 // one comes back through our own request, then every host/admin gets the rows again.
 protected string m_sACAFE_SetupInfo;

 override void ACR_SendSetup(bool active, bool allowed, string rows, string info)
 {
  m_sACAFE_SetupInfo = info;
  if (active && allowed && !rows.IsEmpty()) rows += ACAFE_Settings.Rows();
  super.ACR_SendSetup(active, allowed, rows, info);
 }

 string ACAFE_SetupInfo() { return m_sACAFE_SetupInfo; }

 void ACAFE_RequestSetting(string key)
 {
  Rpc(ACAFE_SettingServer, key);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACAFE_SettingServer(string key)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 150) return;
  m_ACR_LastCommand = now;
  string result = ACAFE_Settings.Handle(GetPlayerId(), key);
  ACR_Feedback(result);
  if (!ACR_Setup.Wanted() || ACR_Setup.Started()) return;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   if (!ACR_Manager.IsHostOrAdmin(id)) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (pc) pc.ACR_SendSetup(true, true, ACR_Setup.Rows(), pc.ACAFE_SetupInfo());
  }
 }
}

// The setup dialog: our rows live on their own page ("Field engineering settings..."), the way
// ACA keeps its logistics settings on one.
modded class ACR_SetupDialog
{
 protected bool m_bACAFE_Page;
 protected string m_sACAFE_Rows;

 override void Fill(string rows, string info)
 {
  // Our rows out of the list ACA builds (ACA re-fills with its own copy, which lacks them).
  string aca = "";
  string ours = "";
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   if (line.StartsWith("acafe:")) ours += line + "\n";
   else aca += line + "\n";
  }
  if (!ours.IsEmpty()) m_sACAFE_Rows = ours;
  if (m_bACAFE_Page) m_bLogisticsPage = false;
  super.Fill(aca, info);
  if (!m_wRows || m_sACAFE_Rows.IsEmpty()) return;
  if (m_bACAFE_Page)
  {
   ACAFE_ShowPage();
   return;
  }
  if (!m_bLogisticsPage) ACAFE_AddRow("acafe_page", "Field engineering settings...");
 }

 protected void ACAFE_ShowPage()
 {
  while (m_wRows.GetChildren()) m_wRows.GetChildren().RemoveFromHierarchy();
  m_Keys.Clear();
  array<string> lines = {};
  m_sACAFE_Rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   ACAFE_AddRow(parts[0], parts[1]);
  }
  ACAFE_AddRow("acafe_page", "Back to match settings");
 }

 protected void ACAFE_AddRow(string key, string text)
 {
  Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, m_wRows);
  if (!widget) return;
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return;
  button.SetText(text);
  button.m_OnClicked.Insert(OnRow);
  m_Keys.Set(button, key);
 }

 override protected void OnRow(SCR_ButtonBaseComponent button)
 {
  string key;
  if (m_Keys.Find(button, key))
  {
   if (key == "acafe_page")
   {
    m_bACAFE_Page = !m_bACAFE_Page;
    Fill(m_sRows, m_sInfo);
    return;
   }
   if (key.StartsWith("acafe:"))
   {
    SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
    if (pc) pc.ACAFE_RequestSetting(key);
    return;
   }
  }
  super.OnRow(button);
 }
}
