enum ACR_EPanelAction
{
 TOGGLE_COMMAND,
 SWITCH,
 MODE_COMBAT,
 MODE_FAST,
 MODE_SILENT,
 DEFEND,
 GET_IN,
 GET_OUT,
 UNLOAD,
 REINFORCE,
 SET_LZ,
 TOGGLE_REQUISITION,
 TOGGLE_SUPPORTS,
 RIDE,
 TAKE_GUNNER,
 REQUISITION_ENTRY = 100, // + entry index (below 200)
 SUPPORT_ENTRY = 200     // + entry index
}

enum ACR_ECursorMode
{
 MOVE,
 DEFEND,
 GET_IN,
 LZ,
 RIDE
}

// Arma Commander side panel on the fullscreen map. Built from script out of the
// game's own widget-library text and button layouts, so it matches vanilla UI.
// Pure view: ACR_PlayerController decides what it shows and handles clicks.
class ACR_CommandPanel : Managed
{
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 protected static const ResourceName LAYOUT_HEADING = "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout";
 protected static const float WIDTH = 340;

 Widget m_Root;
 protected Widget m_FeedRoot;
 protected TextWidget m_FeedText;
 protected ref array<TextWidget> m_FeedLines = {};
 protected string m_sFeedShown = "-";
 protected Widget m_Content;
 protected TextWidget m_Status;
 protected TextWidget m_Economy;
 protected Widget m_SwitchWidget;
 protected Widget m_LastButtonWidget;
 protected SCR_ButtonTextComponent m_CommandButton;
 protected Widget m_GroupSection;
 protected TextWidget m_GroupName;
 protected TextWidget m_GroupDetail;
 protected SCR_ButtonTextComponent m_SwitchButton;
 protected Widget m_GunnerWidget;
 protected Widget m_OrderSection;
 protected ref array<SCR_ButtonTextComponent> m_ModeButtons = {};
 protected TextWidget m_Hint;
 protected SCR_ButtonTextComponent m_DefendButton;
 protected SCR_ButtonTextComponent m_ReinforceButton;
 protected SCR_ButtonTextComponent m_RequisitionButton;
 protected Widget m_RequisitionWidget;
 protected Widget m_RequisitionSection;
 protected Widget m_RequisitionBox;
 protected ref array<SCR_ButtonTextComponent> m_EntryButtons = {};
 protected SCR_ButtonTextComponent m_SupportButton;
 protected Widget m_SupportWidget;
 protected Widget m_SupportSection;
 protected Widget m_SupportBox;
 protected ref array<SCR_ButtonTextComponent> m_SupportButtons = {};
 protected SCR_ButtonTextComponent m_GetInButton;
 protected SCR_ButtonTextComponent m_LZButton;
 protected SCR_ButtonTextComponent m_RideButton;
 protected ref map<SCR_ButtonBaseComponent, int> m_Actions = new map<SCR_ButtonBaseComponent, int>();
 protected SCR_PlayerController m_Owner;

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_Root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_Root, 1, 0);
  FrameSlot.SetAnchorMax(m_Root, 1, 0);
  FrameSlot.SetAlignment(m_Root, 1, 0);
  FrameSlot.SetPos(m_Root, -24, 96);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1000);

  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0.02, 0.13, 0.30, 0.86), 0, m_Root);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);

  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_Root));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(WIDTH);
  AlignableSlot.SetPadding(size, 14, 12, 14, 16);
  m_Content = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);

  AddText(m_Content, LAYOUT_HEADING, "AC: ACTUAL");
  m_Economy = AddText(m_Content, LAYOUT_TEXT, "");
  m_Status = AddText(m_Content, LAYOUT_TEXT, "");
  m_CommandButton = AddButton(m_Content, "Take command", ACR_EPanelAction.TOGGLE_COMMAND);
  m_RequisitionButton = AddButton(m_Content, "Requisition", ACR_EPanelAction.TOGGLE_REQUISITION);
  m_RequisitionWidget = m_LastButtonWidget;
  // Long buy lists scroll inside a fixed-height box.
  SizeLayoutWidget listBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  listBox.EnableHeightOverride(true);
  listBox.SetHeightOverride(340);
  AlignableSlot.SetPadding(listBox, 0, 6, 0, 0);
  m_RequisitionBox = listBox;
  // Clip, or the buttons of a long list draw over the whole panel.
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  m_RequisitionSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, scroll);

  m_SupportButton = AddButton(m_Content, "Supports", ACR_EPanelAction.TOGGLE_SUPPORTS);
  m_SupportWidget = m_LastButtonWidget;
  SizeLayoutWidget supportBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  supportBox.EnableHeightOverride(true);
  supportBox.SetHeightOverride(300);
  AlignableSlot.SetPadding(supportBox, 0, 6, 0, 0);
  m_SupportBox = supportBox;
  Widget supportScroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, supportBox);
  m_SupportSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, supportScroll);

  m_GroupSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content);
  AlignableSlot.SetPadding(m_GroupSection, 0, 14, 0, 0);
  m_GroupName = AddText(m_GroupSection, LAYOUT_HEADING, "");
  m_GroupDetail = AddText(m_GroupSection, LAYOUT_TEXT, "");
  m_SwitchButton = AddButton(m_GroupSection, "Switch to squad", ACR_EPanelAction.SWITCH);
  m_SwitchWidget = m_LastButtonWidget;
  AddButton(m_GroupSection, "Take over gunner", ACR_EPanelAction.TAKE_GUNNER);
  m_GunnerWidget = m_LastButtonWidget;
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(false);

  m_OrderSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_GroupSection);
  AlignableSlot.SetPadding(m_OrderSection, 0, 10, 0, 0);
  AddText(m_OrderSection, LAYOUT_TEXT, "Movement");
  Widget modes = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_ModeButtons.Insert(AddButton(modes, "Combat", ACR_EPanelAction.MODE_COMBAT, true));
  m_ModeButtons.Insert(AddButton(modes, "Fast", ACR_EPanelAction.MODE_FAST, true));
  m_ModeButtons.Insert(AddButton(modes, "Silent", ACR_EPanelAction.MODE_SILENT, true));
  AddText(m_OrderSection, LAYOUT_TEXT, "Orders");
  Widget orders1 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_DefendButton = AddButton(orders1, "Defend", ACR_EPanelAction.DEFEND, true);
  m_GetInButton = AddButton(orders1, "Get in", ACR_EPanelAction.GET_IN, true);
  Widget orders2 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  AddButton(orders2, "Get out", ACR_EPanelAction.GET_OUT, true);
  AddButton(orders2, "Unload", ACR_EPanelAction.UNLOAD, true);
  Widget orders3 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_RideButton = AddButton(orders3, "Ride", ACR_EPanelAction.RIDE, true);
  m_LZButton = AddButton(orders3, "Unload at LZ", ACR_EPanelAction.SET_LZ, true);
  m_ReinforceButton = AddButton(m_OrderSection, "Reinforce", ACR_EPanelAction.REINFORCE);
  m_Hint = AddText(m_GroupSection, LAYOUT_TEXT, "");
  if (m_Hint) AlignableSlot.SetPadding(m_Hint, 0, 8, 0, 6);
  CreateFeed(parent);
 }

 // Intel feed: a small box at the top left, newest sighting first. Clicks go
 // through it to the map.
 protected void CreateFeed(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_FeedRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_FeedRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_FeedRoot, 0, 0);
  FrameSlot.SetAlignment(m_FeedRoot, 0, 0);
  FrameSlot.SetPos(m_FeedRoot, 110, 96); // clear of the map toolbar on the left
  FrameSlot.SetSizeToContent(m_FeedRoot, true);
  m_FeedRoot.SetZOrder(1000);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0.02, 0.13, 0.30, 0.80), 0, m_FeedRoot);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_FeedRoot));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(380);
  AlignableSlot.SetPadding(size, 12, 8, 12, 10);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, size);
  AddText(column, LAYOUT_HEADING, "INTEL");
  // One text widget per line: a single multi-line text doesn't grow the
  // box, so later lines ran off the bottom of the background.
  m_FeedText = AddText(column, LAYOUT_TEXT, "No contacts reported");
  m_FeedLines.Clear();
  for (int i = 0; i < 6; i++)
  {
   TextWidget line = AddText(column, LAYOUT_TEXT, "");
   if (!line) continue;
   line.SetVisible(false);
   m_FeedLines.Insert(line);
  }
 }

 void SetFeed(array<string> lines)
 {
  if (!m_FeedText) return;
  string key = "";
  if (lines) foreach (string l : lines) key += l + "|";
  if (key == m_sFeedShown) return;
  m_sFeedShown = key;
  bool none = !lines || lines.IsEmpty();
  m_FeedText.SetVisible(none);
  foreach (int i, TextWidget line : m_FeedLines)
  {
   bool used = !none && i < lines.Count();
   line.SetVisible(used);
   if (used) line.SetText(lines[i]);
  }
 }

 protected TextWidget AddText(Widget parent, ResourceName layout, string text)
 {
  TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(layout, parent));
  if (widget) widget.SetText(text);
  return widget;
 }

 protected SCR_ButtonTextComponent AddButton(Widget parent, string text, int action, bool fill = false)
 {
  Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, parent);
  if (!widget) return null;
  m_LastButtonWidget = widget;
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  if (fill) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
  // Arma Commander blue instead of the vanilla orange.
  Color normal = new Color(0.16, 0.42, 0.80, 0.40);
  button.SetBackgroundColors(normal, new Color(0.26, 0.56, 0.96, 0.70), new Color(0.30, 0.62, 1.0, 0.95), new Color(0.36, 0.68, 1.0, 1.0), new Color(0.30, 0.62, 1.0, 0.95));
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  Widget border = widget.FindAnyWidget("Border");
  if (border) border.SetColor(new Color(0.45, 0.72, 1.0, 1.0));
  button.m_OnClicked.Insert(OnButton);
  m_Actions.Insert(button, action);
  return button;
 }

 protected void OnButton(SCR_ButtonBaseComponent button)
 {
  int action;
  if (m_Owner && m_Actions.Find(button, action)) m_Owner.ACR_OnPanelAction(action);
 }

 // True if the widget is part of this panel (used to ignore map clicks on it).
 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 void SetCommand(string status, bool isCommander, bool canTake)
 {
  if (m_Status) m_Status.SetText(status);
  if (!m_CommandButton) return;
  if (isCommander) m_CommandButton.SetText("Leave command");
  else if (canTake) m_CommandButton.SetText("Take command");
  else m_CommandButton.SetText("Cannot take command");
  m_CommandButton.SetEnabled(isCommander || canTake, false);
 }

 // name empty = nothing selected
 void SetGroup(string name, string detail, bool friendly, bool isCommander, int mode, int cursorMode, string hint)
 {
  if (!m_GroupSection) return;
  m_GroupSection.SetVisible(!name.IsEmpty());
  if (name.IsEmpty()) return;
  m_GroupName.SetText(name);
  m_GroupDetail.SetText(detail);
  if (m_SwitchWidget) m_SwitchWidget.SetVisible(true);
  if (m_SwitchButton) m_SwitchButton.SetEnabled(friendly, false);
  m_OrderSection.SetVisible(friendly && isCommander);
  foreach (int i, SCR_ButtonTextComponent button : m_ModeButtons)
  {
   if (!button) continue;
   string label = ACR_Squad.ModeName(i);
   if (i == mode) label = "[" + label + "]";
   button.SetText(label);
  }
  // Armed right-click order is shown in brackets, like the active movement mode.
  if (m_DefendButton)
  {
   if (cursorMode == ACR_ECursorMode.DEFEND) m_DefendButton.SetText("[Defend]");
   else m_DefendButton.SetText("Defend");
  }
  if (m_GetInButton)
  {
   if (cursorMode == ACR_ECursorMode.GET_IN) m_GetInButton.SetText("[Get in]");
   else m_GetInButton.SetText("Get in");
  }
  if (m_LZButton)
  {
   if (cursorMode == ACR_ECursorMode.LZ) m_LZButton.SetText("[Unload at LZ]");
   else m_LZButton.SetText("Unload at LZ");
  }
  if (m_RideButton)
  {
   if (cursorMode == ACR_ECursorMode.RIDE) m_RideButton.SetText("[Ride]");
   else m_RideButton.SetText("Ride");
  }
  if (m_Hint) m_Hint.SetText(hint);
 }

 // Buy list: one button per entry, rebuilt when the entry count changes.
 // selected = entry armed for deployment (-1 none), open = list expanded.
 void SetRequisitions(bool visible, bool open, array<string> names, array<int> costs, array<int> remaining, int rp, int selected)
 {
  if (!m_RequisitionSection) return;
  if (m_RequisitionWidget) m_RequisitionWidget.SetVisible(visible);
  m_RequisitionSection.SetVisible(visible && open);
  if (m_RequisitionBox) m_RequisitionBox.SetVisible(visible && open);
  if (m_RequisitionButton)
  {
   if (open) m_RequisitionButton.SetText("Requisition  [close]");
   else m_RequisitionButton.SetText("Requisition");
  }
  if (!visible || !open) return;
  if (m_EntryButtons.Count() != names.Count())
  {
   Widget child = m_RequisitionSection.GetChildren();
   while (child)
   {
    Widget next = child.GetSibling();
    child.RemoveFromHierarchy();
    child = next;
   }
   foreach (SCR_ButtonTextComponent old : m_EntryButtons) if (old) m_Actions.Remove(old);
   m_EntryButtons.Clear();
   foreach (int i, string entryName : names)
    m_EntryButtons.Insert(AddButton(m_RequisitionSection, entryName, ACR_EPanelAction.REQUISITION_ENTRY + i));
  }
  foreach (int i, SCR_ButtonTextComponent button : m_EntryButtons)
  {
   if (!button) continue;
   string label = string.Format("%1   %2 RP   (%3 left)", names[i], costs[i], remaining[i]);
   if (i == selected) label = "[" + label + "]";
   button.SetText(label);
   button.SetEnabled(remaining[i] > 0 && costs[i] <= rp, false);
  }
 }

 // Support list: cost and cooldown per entry. selected = support armed for the next right-click.
 void SetSupports(bool visible, bool open, array<string> names, array<int> costs, array<int> cooldowns, int rp, int selected)
 {
  if (!m_SupportSection) return;
  if (m_SupportWidget) m_SupportWidget.SetVisible(visible && !names.IsEmpty());
  m_SupportSection.SetVisible(visible && open);
  if (m_SupportBox) m_SupportBox.SetVisible(visible && open);
  if (m_SupportButton)
  {
   if (open) m_SupportButton.SetText("Supports  [close]");
   else m_SupportButton.SetText("Supports");
  }
  if (!visible || !open) return;
  if (m_SupportButtons.Count() != names.Count())
  {
   Widget child = m_SupportSection.GetChildren();
   while (child)
   {
    Widget next = child.GetSibling();
    child.RemoveFromHierarchy();
    child = next;
   }
   foreach (SCR_ButtonTextComponent old : m_SupportButtons) if (old) m_Actions.Remove(old);
   m_SupportButtons.Clear();
   foreach (int i, string supportName : names)
    m_SupportButtons.Insert(AddButton(m_SupportSection, supportName, ACR_EPanelAction.SUPPORT_ENTRY + i));
  }
  foreach (int i, SCR_ButtonTextComponent button : m_SupportButtons)
  {
   if (!button) continue;
   string state = "ready";
   if (cooldowns[i] > 0) state = cooldowns[i].ToString() + " s";
   string label = string.Format("%1   %2 RP   (%3)", names[i], costs[i], state);
   if (i == selected) label = "[" + label + "]";
   button.SetText(label);
   button.SetEnabled(cooldowns[i] <= 0 && costs[i] <= rp, false);
  }
 }

 void SetGunner(bool visible)
 {
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(visible);
 }

 void SetReinforce(string text, bool enabled)
 {
  if (!m_ReinforceButton) return;
  m_ReinforceButton.SetText(text);
  m_ReinforceButton.SetEnabled(enabled, false);
 }

 void SetEconomy(string text)
 {
  if (m_Economy) m_Economy.SetText(text);
 }

 // A selected base: info only, no squad buttons.
 void SetBase(string name, string detail)
 {
  if (!m_GroupSection) return;
  m_GroupSection.SetVisible(true);
  m_GroupName.SetText(name);
  m_GroupDetail.SetText(detail);
  if (m_SwitchWidget) m_SwitchWidget.SetVisible(false);
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(false);
  m_OrderSection.SetVisible(false);
  if (m_Hint) m_Hint.SetText("");
 }

 void Destroy()
 {
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
  if (m_FeedRoot) m_FeedRoot.RemoveFromHierarchy();
  m_FeedRoot = null;
  m_FeedText = null;
  m_FeedLines.Clear();
  m_Actions.Clear();
  m_ModeButtons.Clear();
  m_EntryButtons.Clear();
 }
}

// Right-click on the map with a squad selected issues an order instead of
// opening the vanilla contextual radial menu.
modded class SCR_MapRadialUI
{
 override protected void OnInputMenuOpen(float value, EActionTrigger reason)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc && pc.ACR_HandleMapRightClick()) return;
  super.OnInputMenuOpen(value, reason);
 }
}
