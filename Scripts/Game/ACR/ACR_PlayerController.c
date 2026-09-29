// Map markers for soldiers and group leaders. Clicking one requests possession.
class ACR_UnitMarker : SCR_MapMarkerBase
{
 RplId m_UnitId;
 string m_Label;
 int m_State;          // 0 AI, 1 player-controlled, 2 dead
 bool m_Original;      // the player's command unit
 bool m_Selected;
 RplId m_GroupId;      // squad this marker belongs to
 ref ACR_MarkerClick m_Click;
 ref SCR_MilitarySymbol m_GroupSymbol;
 override void OnCreateMarker(bool skipProfanityFilter = false)
 {
  super.OnCreateMarker(true);
  if (!m_wRoot) return;
  m_Click = new ACR_MarkerClick();
  m_Click.m_Marker = this;
  m_wRoot.AddHandler(m_Click);
  RefreshStyle();
 }
 void RefreshStyle()
 {
  if (!m_MarkerWidgetComp) return;
  m_MarkerWidgetComp.SetText(m_Label, true);
  m_MarkerWidgetComp.SetTextVisible(true);
  Color color = new Color(0.1, 0.65, 1, 1);
  if (m_State == 1) color = new Color(1, 0.75, 0.1, 1);
  if (m_State == 2) color = new Color(0.5, 0.5, 0.5, 1);
  if (m_Original) color = new Color(0.2, 1, 0.3, 1);
  if (m_Selected) color = new Color(1, 1, 1, 1);
  m_MarkerWidgetComp.SetColor(color);
  if (m_GroupSymbol) m_MarkerWidgetComp.UpdateMilitarySymbol(m_GroupSymbol);
 }
 // Left-click: select this marker's squad (Arma Commander style). The command
 // unit marker instead returns you to your command unit.
 void Select()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc) return;
  pc.ACR_MarkUiClick();
  if (m_Original) pc.ACR_Request(ACR_ERequest.RETURN);
  else pc.ACR_SelectGroup(m_GroupId);
 }
}
class ACR_MarkerClick : ScriptedWidgetEventHandler
{
 ACR_UnitMarker m_Marker;
 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Marker) return false;
  m_Marker.Select();
  return true;
 }
}

modded class SCR_PlayerController
{
 // ---------------------------------------------------------------- menu control flags
 protected IEntity m_ACR_MenuControlBody;
 // Native UpdateControls only reapplies menu-disabled view/weapon/movement
 // flags when menu state changes; move them with the body on possession.
 override protected void UpdateControls()
 {
  super.UpdateControls();
  if (!ACR_Manager.GetInstance() || this != GetGame().GetPlayerController()) return;
  IEntity body = GetControlledEntity();
  if (body == m_ACR_MenuControlBody) return;
  ACR_MapHandoff.SetMenuControls(m_ACR_MenuControlBody, false);
  ACR_MapHandoff.SetMenuControls(body, GetGame().GetMenuManager().IsAnyMenuOpen());
  m_ACR_MenuControlBody = body;
 }

 // ---------------------------------------------------------------- requests
 protected float m_ACR_LastCommand = -1000;
 void ACR_Request(int command, RplId targetId = RplId.Invalid(), vector point = vector.Zero)
 {
  if (!ACR_Manager.GetInstance()) return;
  if (command == ACR_ERequest.POSSESS || command == ACR_ERequest.RETURN) ACR_MapHandoff.Release(this, GetControlledEntity(), false);
  Rpc(ACR_CommandServer, command, targetId, point);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_CommandServer(int command, RplId targetId, vector point)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  string message = manager.HandleRequest(this, command, targetId, point);
  Rpc(ACR_FeedbackOwner, message);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_FeedbackOwner(string message)
 {
  Print("[ACR] " + message);
  SCR_HintManagerComponent.ShowCustomHint(message, "Arma Commander", 5, true);
 }

 void ACR_Notify(string message)
 {
  Rpc(ACR_FeedbackOwner, message);
 }

 // ---------------------------------------------------------------- command status
 protected bool m_ACR_IsCommander;
 protected bool m_ACR_SharedCommand;
 protected string m_ACR_CommandText;
 void ACR_SendCommandStatus(bool isCommander, string text, bool shared)
 {
  Rpc(ACR_CommandStatusOwner, isCommander, text, shared);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CommandStatusOwner(bool isCommander, string text, bool shared)
 {
  m_ACR_IsCommander = isCommander;
  m_ACR_CommandText = text;
  m_ACR_SharedCommand = shared;
 }

 void ACR_PossessAimed()
 {
  vector camera[4];
  GetGame().GetWorld().GetCurrentCamera(camera);
  IEntity hit = ACR_TraceSoldier(camera[3], camera[2]);
  if (!hit)
  {
   SCR_HintManagerComponent.ShowCustomHint("Aim directly at a friendly soldier within 250 metres", "Possess soldier", 4, true);
   return;
  }
  RplId targetId = SCR_EntityHelper.EntityToRplId(hit);
  int index = m_ACR_Ids.Find(targetId);
  if (index < 0)
  {
   SCR_HintManagerComponent.ShowCustomHint("The soldier under the centre of your view is not in a friendly squad", "No eligible target", 4, true);
   return;
  }
  PrintFormat("[ACR] AIM requested %1 id=%2", m_ACR_Labels[index], targetId);
  ACR_Request(ACR_ERequest.POSSESS, targetId);
 }
 IEntity ACR_TraceSoldier(vector start, vector direction)
 {
  TraceParam trace = new TraceParam();
  trace.Start = start;
  trace.End = start + direction * 250;
  trace.Exclude = GetControlledEntity();
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  GetGame().GetWorld().TraceMove(trace, null);
  IEntity hit = trace.TraceEnt;
  while (hit && !ChimeraCharacter.Cast(hit)) hit = hit.GetParent();
  return hit;
 }

 // ---------------------------------------------------------------- soldier roster
 protected ref array<RplId> m_ACR_Ids = {};
 protected ref array<vector> m_ACR_Positions = {};
 protected ref array<int> m_ACR_States = {};
 protected ref array<string> m_ACR_Labels = {};
 protected ref array<RplId> m_ACR_UnitGroups = {};
 protected vector m_ACR_OriginalPos;
 protected ref array<ref ACR_UnitMarker> m_ACR_Markers = {};
 protected bool m_ACR_MapWasOpen;

 void ACR_SendRoster(array<RplId> ids, array<vector> positions, array<int> states, array<string> labels, array<RplId> groupIds, vector original)
 {
  Rpc(ACR_RosterOwner, ids, positions, states, labels, groupIds, original);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RosterOwner(array<RplId> ids, array<vector> positions, array<int> states, array<string> labels, array<RplId> groupIds, vector original)
 {
  m_ACR_UnitGroups.Copy(groupIds);
  m_ACR_Ids.Copy(ids);
  m_ACR_Positions.Copy(positions);
  m_ACR_States.Copy(states);
  m_ACR_Labels.Copy(labels);
  m_ACR_OriginalPos = original;
 }

 // ---------------------------------------------------------------- group leaders
 protected ref array<RplId> m_ACR_LeaderIds = {};
 protected ref array<vector> m_ACR_LeaderPositions = {};
 protected ref array<string> m_ACR_GroupNames = {};
 protected ref array<int> m_ACR_GroupSymbols = {};
 protected ref array<int> m_ACR_LeaderStates = {};
 protected ref array<int> m_ACR_Commandable = {};
 protected ref array<RplId> m_ACR_GroupIds = {};
 protected ref array<string> m_ACR_GroupDetails = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_GroupMarkers = {};

 void ACR_SendGroupMarkers(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states, array<int> commandable, array<RplId> groupIds, array<string> details)
 {
  Rpc(ACR_GroupMarkersOwner, leaders, positions, names, symbols, states, commandable, groupIds, details);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_GroupMarkersOwner(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states, array<int> commandable, array<RplId> groupIds, array<string> details)
 {
  m_ACR_GroupIds.Copy(groupIds);
  m_ACR_GroupDetails.Copy(details);
  m_ACR_LeaderIds.Copy(leaders);
  m_ACR_LeaderPositions.Copy(positions);
  m_ACR_GroupNames.Copy(names);
  m_ACR_GroupSymbols.Copy(symbols);
  m_ACR_LeaderStates.Copy(states);
  m_ACR_Commandable.Copy(commandable);
 }

 // Client-side hint for the command menu; the server re-checks on execution.
 bool ACR_CanCommandSquad()
 {
  int index = m_ACR_LeaderIds.Find(SCR_EntityHelper.EntityToRplId(GetControlledEntity()));
  return index >= 0 && index < m_ACR_Commandable.Count() && m_ACR_Commandable[index] == 1;
 }

 // ---------------------------------------------------------------- map
 void ACR_ClearMarkers()
 {
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (manager)
  {
   foreach (ACR_UnitMarker marker : m_ACR_Markers) manager.RemoveStaticMarker(marker);
   foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) manager.RemoveStaticMarker(groupMarker);
  }
  m_ACR_Markers.Clear();
  m_ACR_GroupMarkers.Clear();
 }
 void ACR_CenterMap()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !GetControlledEntity()) return;
  vector pos = GetControlledEntity().GetOrigin();
  mapEntity.ZoomPanSmooth(4, pos[0], pos[2]);
 }
 void ACR_UpdateMap()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool open = mapEntity && mapEntity.IsOpen();
  if (!open)
  {
   if (m_ACR_MapWasOpen)
   {
    ACR_ClearMarkers();
    ACR_ClosePanel();
   }
   m_ACR_MapWasOpen = false;
   return;
  }
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  if (!m_ACR_MapWasOpen)
  {
   GetGame().GetCallqueue().CallLater(ACR_CenterMap, 100, false);
   ACR_OpenPanel(mapEntity);
  }
  m_ACR_MapWasOpen = true;
  ACR_UpdatePanel();
  int count = m_ACR_Ids.Count();
  if (m_ACR_Markers.Count() != count + 1)
  {
   foreach (ACR_UnitMarker old : m_ACR_Markers) manager.RemoveStaticMarker(old);
   m_ACR_Markers.Clear();
   for (int i = 0; i <= count; i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    marker.m_Original = i == count;
    m_ACR_Markers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
   }
  }
  for (int i = 0; i < count; i++)
  {
   ACR_UnitMarker marker = m_ACR_Markers[i];
   marker.m_UnitId = m_ACR_Ids[i];
   marker.m_State = m_ACR_States[i];
   marker.m_Label = m_ACR_Labels[i];
   marker.m_GroupId = m_ACR_UnitGroups[i];
   marker.m_Selected = m_ACR_SelectedGroup.IsValid() && marker.m_GroupId == m_ACR_SelectedGroup;
   if (marker.m_State == 1) marker.m_Label += " [PLAYER]";
   if (marker.m_State == 2) marker.m_Label += " [DEAD]";
   vector pos = m_ACR_Positions[i];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(marker.m_UnitId);
   if (streamed) pos = streamed.GetOrigin();
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
  }
  ACR_UnitMarker original = m_ACR_Markers[count];
  original.m_Label = "COMMAND UNIT [F8]";
  original.SetWorldPos(m_ACR_OriginalPos[0], m_ACR_OriginalPos[2]);
  original.RefreshStyle();
  ACR_UpdateGroupMarkers(manager);
 }
 void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_GroupMarkers.Count() != m_ACR_LeaderIds.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_GroupMarkers) manager.RemoveStaticMarker(old);
   m_ACR_GroupMarkers.Clear();
   for (int i = 0; i < m_ACR_LeaderIds.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    int icons = m_ACR_GroupSymbols[i * 4];
    int dimension = m_ACR_GroupSymbols[i * 4 + 1];
    SCR_MapMarkerBase prepared = manager.PrepareMilitaryMarker(EMilitarySymbolIdentity.BLUFOR, dimension, icons);
    if (prepared)
    {
     marker.SetType(prepared.GetType());
     marker.SetMarkerConfigID(prepared.GetMarkerConfigID());
     marker.SetFlags(prepared.GetFlags());
    }
    else
    {
     marker.SetType(SCR_EMapMarkerType.SIMPLE);
     marker.SetMarkerConfigID(1);
    }
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_GroupMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
   }
  }
  for (int i = 0; i < m_ACR_GroupMarkers.Count(); i++)
  {
   ACR_UnitMarker marker = m_ACR_GroupMarkers[i];
   marker.m_UnitId = m_ACR_LeaderIds[i];
   marker.m_Label = m_ACR_GroupNames[i];
   marker.m_GroupId = m_ACR_GroupIds[i];
   marker.m_Selected = m_ACR_SelectedGroup.IsValid() && marker.m_GroupId == m_ACR_SelectedGroup;
   marker.m_State = m_ACR_LeaderStates[i];
   if (marker.m_State == 1) marker.m_Label += " [PLAYER LEADER]";
   else marker.m_Label += " [AI LEADER]";
   vector pos = m_ACR_LeaderPositions[i];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(marker.m_UnitId);
   if (streamed) pos = streamed.GetOrigin();
   marker.SetWorldPos(pos[0], pos[2]);
   if (!marker.m_GroupSymbol) marker.m_GroupSymbol = new SCR_MilitarySymbol();
   marker.m_GroupSymbol.Set(EMilitarySymbolIdentity.BLUFOR, m_ACR_GroupSymbols[i * 4 + 1], m_ACR_GroupSymbols[i * 4], m_ACR_GroupSymbols[i * 4 + 2]);
   marker.RefreshStyle();
  }
  // The leader marker replaces that soldier's plain marker.
  foreach (ACR_UnitMarker unit : m_ACR_Markers)
  {
   if (unit.m_Original) continue;
   bool leader = m_ACR_LeaderIds.Contains(unit.m_UnitId);
   unit.SetServerDisabled(leader);
   unit.SetVisible(!leader);
  }
 }
 void ACR_PossessMapCursor()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen()) return;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  float best = 24;
  ACR_UnitMarker selected;
  foreach (ACR_UnitMarker marker : m_ACR_Markers)
  {
   int pos[2];
   marker.GetWorldPos(pos);
   float dx = x - pos[0];
   float dz = z - pos[1];
   float pixels = Math.Sqrt(dx * dx + dz * dz) * mapEntity.GetMapWidget().PixelPerUnit();
   if (pixels >= best) continue;
   best = pixels;
   selected = marker;
  }
  if (selected && selected.m_Original) ACR_Request(ACR_ERequest.RETURN);
  else if (selected) ACR_Request(ACR_ERequest.POSSESS, selected.m_UnitId);
  else SCR_HintManagerComponent.ShowCustomHint("Place the cursor over a unit marker, then press F7", "Possess soldier", 4, true);
 }

 // ---------------------------------------------------------------- test harness hooks
 bool ACR_SelectMarker(RplId id, bool groupMarker)
 {
  array<ref ACR_UnitMarker> markers = m_ACR_Markers;
  if (groupMarker) markers = m_ACR_GroupMarkers;
  foreach (ACR_UnitMarker marker : markers)
   if (marker.m_UnitId == id) { ACR_Request(ACR_ERequest.POSSESS, marker.m_UnitId); return true; }
  return false;
 }
 string ACR_DescribeGroupMarker(RplId id)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
   if (marker.m_UnitId == id)
    return string.Format("label=%1 playerState=%2 widget=%3", marker.m_Label, marker.m_State, marker.GetRootWidget() != null);
  return "missing";
 }

 // ---------------------------------------------------------------- selection + command panel
 protected RplId m_ACR_SelectedGroup = RplId.Invalid();
 protected ACR_ECursorMode m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 protected ref ACR_CommandPanel m_ACR_Panel;
 protected int m_ACR_LastUiClick;

 RplId ACR_GetSelectedGroup() { return m_ACR_SelectedGroup; }
 void ACR_MarkUiClick() { m_ACR_LastUiClick = System.GetTickCount(); }

 void ACR_SelectGroup(RplId groupId)
 {
  m_ACR_SelectedGroup = groupId;
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 protected void ACR_OpenPanel(SCR_MapEntity mapEntity)
 {
  ACR_ClosePanel();
  Widget root = mapEntity.GetMapMenuRoot();
  if (!root) return;
  m_ACR_Panel = new ACR_CommandPanel();
  m_ACR_Panel.Create(root, this);
  SCR_MapEntity.GetOnSelection().Insert(ACR_OnMapClick);
 }

 protected void ACR_ClosePanel()
 {
  SCR_MapEntity.GetOnSelection().Remove(ACR_OnMapClick);
  if (m_ACR_Panel) m_ACR_Panel.Destroy();
  m_ACR_Panel = null;
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 // Left-click on empty map clears the selection. Clicks on markers or the panel don't.
 protected void ACR_OnMapClick(vector screenPos)
 {
  if (System.GetTickCount() - m_ACR_LastUiClick < 400) return;
  if (m_ACR_Panel && m_ACR_Panel.Contains(WidgetManager.GetWidgetUnderCursor())) return;
  m_ACR_SelectedGroup = RplId.Invalid();
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 protected void ACR_UpdatePanel()
 {
  if (!m_ACR_Panel) return;
  bool canTake = !m_ACR_IsCommander && (m_ACR_SharedCommand || m_ACR_CommandText == "Command: AI");
  m_ACR_Panel.SetCommand(m_ACR_CommandText, m_ACR_IsCommander, canTake);
  int index = -1;
  if (m_ACR_SelectedGroup.IsValid()) index = m_ACR_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0)
  {
   // Selection vanished (squad wiped out, or not in the latest snapshot yet).
   m_ACR_Panel.SetGroup("", "", false, false, 0, "");
   return;
  }
  bool friendly = index < m_ACR_Commandable.Count() && m_ACR_Commandable[index] == 1;
  string hint;
  if (!friendly) hint = "Player group: not commandable";
  else if (!m_ACR_IsCommander) hint = "Take command to give orders";
  else if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) hint = "Right-click: defend that position";
  else if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) hint = "Right-click near a vehicle: get in";
  else hint = "Right-click: move there";
  m_ACR_Panel.SetGroup(m_ACR_GroupNames[index], m_ACR_GroupDetails[index], friendly, m_ACR_IsCommander, m_ACR_GroupSymbols[index * 4 + 3], hint);
 }

 void ACR_OnPanelAction(int action)
 {
  ACR_MarkUiClick();
  switch (action)
  {
   case ACR_EPanelAction.TOGGLE_COMMAND:
    if (m_ACR_IsCommander) ACR_Request(ACR_ERequest.LEAVE_COMMAND);
    else ACR_Request(ACR_ERequest.TAKE_COMMAND);
    return;
   case ACR_EPanelAction.SWITCH: ACR_Request(ACR_ERequest.SWITCH_TO_GROUP, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.MODE_COMBAT: ACR_Request(ACR_ERequest.MODE_COMBAT, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.MODE_FAST: ACR_Request(ACR_ERequest.MODE_FAST, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.MODE_SILENT: ACR_Request(ACR_ERequest.MODE_SILENT, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.DEFEND: m_ACR_CursorMode = ACR_ECursorMode.DEFEND; return;
   case ACR_EPanelAction.GET_IN: m_ACR_CursorMode = ACR_ECursorMode.GET_IN; return;
   case ACR_EPanelAction.GET_OUT: ACR_Request(ACR_ERequest.ORDER_GET_OUT, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.UNLOAD: ACR_Request(ACR_ERequest.ORDER_UNLOAD, m_ACR_SelectedGroup); return;
  }
 }

 // Called by the map radial menu hook. Returns true if the right-click was used.
 bool ACR_HandleMapRightClick()
 {
  if (!ACR_Manager.GetInstance() || !m_ACR_SelectedGroup.IsValid()) return false;
  int index = m_ACR_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0 || index >= m_ACR_Commandable.Count() || m_ACR_Commandable[index] != 1) return false;
  if (!m_ACR_IsCommander)
  {
   SCR_HintManagerComponent.ShowCustomHint("Only the commander can give map orders. Take command in the panel.", "Arma Commander", 4, true);
   return true;
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  int command = ACR_ERequest.MOVE_TO;
  if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) command = ACR_ERequest.ORDER_DEFEND;
  if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) command = ACR_ERequest.ORDER_GET_IN;
  ACR_Request(command, m_ACR_SelectedGroup, Vector(x, 0, z));
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  return true;
 }
}
