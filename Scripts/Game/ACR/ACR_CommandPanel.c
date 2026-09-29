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
 UNLOAD
}

enum ACR_ECursorMode
{
 MOVE,
 DEFEND,
 GET_IN
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
 protected Widget m_Content;
 protected TextWidget m_Status;
 protected SCR_ButtonTextComponent m_CommandButton;
 protected Widget m_GroupSection;
 protected TextWidget m_GroupName;
 protected TextWidget m_GroupDetail;
 protected SCR_ButtonTextComponent m_SwitchButton;
 protected Widget m_OrderSection;
 protected ref array<SCR_ButtonTextComponent> m_ModeButtons = {};
 protected TextWidget m_Hint;
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

  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0, 0, 0, 0.78), 0, m_Root);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);

  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_Root));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(WIDTH);
  m_Content = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  AlignableSlot.SetPadding(m_Content, 14, 12, 14, 12);

  AddText(m_Content, LAYOUT_HEADING, "ARMA COMMANDER");
  m_Status = AddText(m_Content, LAYOUT_TEXT, "");
  m_CommandButton = AddButton(m_Content, "Take command", ACR_EPanelAction.TOGGLE_COMMAND);

  m_GroupSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content);
  AlignableSlot.SetPadding(m_GroupSection, 0, 14, 0, 0);
  m_GroupName = AddText(m_GroupSection, LAYOUT_HEADING, "");
  m_GroupDetail = AddText(m_GroupSection, LAYOUT_TEXT, "");
  m_SwitchButton = AddButton(m_GroupSection, "Switch to squad", ACR_EPanelAction.SWITCH);

  m_OrderSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_GroupSection);
  AlignableSlot.SetPadding(m_OrderSection, 0, 10, 0, 0);
  AddText(m_OrderSection, LAYOUT_TEXT, "Movement");
  Widget modes = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_ModeButtons.Insert(AddButton(modes, "Combat", ACR_EPanelAction.MODE_COMBAT, true));
  m_ModeButtons.Insert(AddButton(modes, "Fast", ACR_EPanelAction.MODE_FAST, true));
  m_ModeButtons.Insert(AddButton(modes, "Silent", ACR_EPanelAction.MODE_SILENT, true));
  AddText(m_OrderSection, LAYOUT_TEXT, "Orders");
  Widget orders1 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  AddButton(orders1, "Defend", ACR_EPanelAction.DEFEND, true);
  AddButton(orders1, "Get in", ACR_EPanelAction.GET_IN, true);
  Widget orders2 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  AddButton(orders2, "Get out", ACR_EPanelAction.GET_OUT, true);
  AddButton(orders2, "Unload", ACR_EPanelAction.UNLOAD, true);
  m_Hint = AddText(m_GroupSection, LAYOUT_TEXT, "");
  if (m_Hint) AlignableSlot.SetPadding(m_Hint, 0, 8, 0, 0);
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
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  if (fill) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
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
 void SetGroup(string name, string detail, bool friendly, bool isCommander, int mode, string hint)
 {
  if (!m_GroupSection) return;
  m_GroupSection.SetVisible(!name.IsEmpty());
  if (name.IsEmpty()) return;
  m_GroupName.SetText(name);
  m_GroupDetail.SetText(detail);
  if (m_SwitchButton) m_SwitchButton.SetEnabled(friendly, false);
  m_OrderSection.SetVisible(friendly && isCommander);
  foreach (int i, SCR_ButtonTextComponent button : m_ModeButtons)
  {
   if (!button) continue;
   string label = ACR_Squad.ModeName(i);
   if (i == mode) label = "[" + label + "]";
   button.SetText(label);
  }
  if (m_Hint) m_Hint.SetText(hint);
 }

 void Destroy()
 {
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
  m_Actions.Clear();
  m_ModeButtons.Clear();
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
