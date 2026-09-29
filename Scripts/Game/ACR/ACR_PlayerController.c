// Map markers for soldiers and group leaders. Clicking one requests possession.
class ACR_UnitMarker : SCR_MapMarkerBase
{
 RplId m_UnitId;
 string m_Label;
 int m_State;          // 0 AI, 1 player-controlled, 2 dead
 bool m_Original;      // the player's command unit
 bool m_Selected;
 RplId m_GroupId;      // squad this marker belongs to
 int m_BaseIndex = -1; // >= 0: this marker is a base
 ref ACR_MarkerClick m_Click;
 ref SCR_MilitarySymbol m_GroupSymbol;
 override void OnCreateMarker(bool skipProfanityFilter = false)
 {
  // Always update: units move into view without the map being panned.
  m_bTestVisibleFrame = false;
  super.OnCreateMarker(true);
  if (!m_wRoot) return;
  m_Click = new ACR_MarkerClick();
  m_Click.m_Marker = this;
  m_wRoot.AddHandler(m_Click);
  RefreshStyle();
 }
 void ACR_SetColor(Color color)
 {
  if (m_MarkerWidgetComp) m_MarkerWidgetComp.SetColor(color);
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
  else if (m_BaseIndex >= 0) pc.ACR_ClickBase(m_Label);
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

 // ---------------------------------------------------------------- gunner drives
 protected IEntity m_ACR_DriveVehicle;
 protected float m_ACR_DriveThrottle;
 protected float m_ACR_DriveSteer;
 protected float m_ACR_DriveSent;

 // Called every frame by the local manager; sends key changes plus a keep-alive while held.
 void ACR_UpdateGunnerDrive(IEntity vehicle, float throttle, float steer)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (vehicle != m_ACR_DriveVehicle)
  {
   if (m_ACR_DriveVehicle && (m_ACR_DriveThrottle != 0 || m_ACR_DriveSteer != 0))
    Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(m_ACR_DriveVehicle), 0, 0);
   m_ACR_DriveVehicle = vehicle;
   m_ACR_DriveThrottle = 0;
   m_ACR_DriveSteer = 0;
  }
  if (!vehicle) return;
  bool changed = throttle != m_ACR_DriveThrottle || steer != m_ACR_DriveSteer;
  bool held = throttle != 0 || steer != 0;
  if (!changed && !(held && now - m_ACR_DriveSent > 250)) return;
  m_ACR_DriveThrottle = throttle;
  m_ACR_DriveSteer = steer;
  m_ACR_DriveSent = now;
  Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(vehicle), throttle, steer);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DriveServer(RplId vehicleId, float throttle, float steer)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.GunnerDriveInput(this, SCR_EntityHelper.RplIdToEntity(vehicleId), throttle, steer);
 }
 // Test hook: drive as if the keys were held.
 void ACR_TestDrive(IEntity vehicle, float throttle, float steer)
 {
  Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(vehicle), throttle, steer);
 }

 // Server: make a new character this player's command unit while they keep
 // controlling a possessed soldier (their old command unit died).
 void ACR_SetMainEntity(IEntity entity)
 {
  RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
  if (!rpl) return;
  rpl.GiveExt(RplIdentity.Local(), false);
  m_MainEntityID = rpl.Id();
  OnRplMainEntityFromID();
  Replication.BumpMe();
  SCR_PossessingManagerComponent possessing = SCR_PossessingManagerComponent.GetInstance();
  if (possessing) possessing.SetMainEntity(GetPlayerId(), GetControlledEntity(), entity, true);
 }

 void ACR_Notify(string message)
 {
  Rpc(ACR_FeedbackOwner, message);
 }

 // ---------------------------------------------------------------- command status
 protected bool m_ACR_IsCommander;
 protected bool m_ACR_SharedCommand;
 protected string m_ACR_CommandText;
 protected int m_ACR_RP;
 protected int m_ACR_Income;
 protected int m_ACR_IncomeSeconds;
 void ACR_SendCommandStatus(bool isCommander, string text, bool shared, int rp, int income, int seconds)
 {
  Rpc(ACR_CommandStatusOwner, isCommander, text, shared, rp, income, seconds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CommandStatusOwner(bool isCommander, string text, bool shared, int rp, int income, int seconds)
 {
  m_ACR_RP = rp;
  m_ACR_Income = income;
  m_ACR_IncomeSeconds = seconds;
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
   foreach (ACR_UnitMarker marker : m_ACR_Markers) manager.ACR_RemoveMarker(marker);
   foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) manager.ACR_RemoveMarker(groupMarker);
   foreach (ACR_UnitMarker baseMarker : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(baseMarker);
   foreach (ACR_UnitMarker zoneMarker : m_ACR_ZoneMarkers) manager.ACR_RemoveMarker(zoneMarker);
  }
  m_ACR_ZoneMarkers.Clear();
  m_ACR_BaseMarkers.Clear();
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
  ACR_DrawOrders(mapEntity);
  // Leaders are drawn by their group marker, so they get no plain soldier marker.
  array<int> shown = {};
  foreach (int rosterIndex, RplId unitId : m_ACR_Ids)
   if (!m_ACR_LeaderIds.Contains(unitId)) shown.Insert(rosterIndex);
  int count = shown.Count();
  if (m_ACR_Markers.Count() != count + 1)
  {
   foreach (ACR_UnitMarker old : m_ACR_Markers) manager.ACR_RemoveMarker(old);
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
    // Not "owned" by the player, so vanilla doesn't open the marker edit dialog on click.
    marker.SetMarkerOwnerID(-1);
   }
  }
  for (int i = 0; i < count; i++)
  {
   int r = shown[i];
   ACR_UnitMarker marker = m_ACR_Markers[i];
   marker.m_UnitId = m_ACR_Ids[r];
   marker.m_State = m_ACR_States[r];
   marker.m_Label = m_ACR_Labels[r];
   marker.m_GroupId = m_ACR_UnitGroups[r];
   marker.m_Selected = m_ACR_SelectedGroup.IsValid() && marker.m_GroupId == m_ACR_SelectedGroup;
   if (marker.m_State == 1) marker.m_Label += " [PLAYER]";
   vector pos = m_ACR_Positions[r];
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
  ACR_UpdateBaseMarkers(manager);
  ACR_UpdateZoneMarkers(manager);
 }
 void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_GroupMarkers.Count() != m_ACR_LeaderIds.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_GroupMarkers) manager.ACR_RemoveMarker(old);
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
    // Not "owned" by the player, so vanilla doesn't open the marker edit dialog on click.
    marker.SetMarkerOwnerID(-1);
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
 }
 void ACR_PossessMapCursor()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen()) return;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  float best = 24;
  ACR_UnitMarker selected;
  array<ACR_UnitMarker> candidates = {};
  foreach (ACR_UnitMarker unitMarker : m_ACR_Markers) candidates.Insert(unitMarker);
  foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) candidates.Insert(groupMarker);
  foreach (ACR_UnitMarker marker : candidates)
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
  m_ACR_SelectedBase = "";
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
  // Full-screen canvas for order lines; ignores the cursor so map clicks pass through.
  m_ACR_OrderCanvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, root));
  if (m_ACR_OrderCanvas)
  {
   FrameSlot.SetAnchorMin(m_ACR_OrderCanvas, 0, 0);
   FrameSlot.SetAnchorMax(m_ACR_OrderCanvas, 1, 1);
   FrameSlot.SetOffsets(m_ACR_OrderCanvas, 0, 0, 0, 0);
   m_ACR_OrderCanvas.SetZOrder(900);
  }
  SCR_MapEntity.GetOnSelection().Insert(ACR_OnMapClick);
 }

 protected void ACR_ClosePanel()
 {
  SCR_MapEntity.GetOnSelection().Remove(ACR_OnMapClick);
  if (m_ACR_Panel) m_ACR_Panel.Destroy();
  m_ACR_Panel = null;
  if (m_ACR_OrderCanvas) m_ACR_OrderCanvas.RemoveFromHierarchy();
  m_ACR_OrderCanvas = null;
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 // Left-click on empty map clears the selection. Clicks on markers or the panel don't.
 protected void ACR_OnMapClick(vector screenPos)
 {
  if (System.GetTickCount() - m_ACR_LastUiClick < 400) return;
  if (m_ACR_Panel && m_ACR_Panel.Contains(WidgetManager.GetWidgetUnderCursor())) return;
  m_ACR_SelectedGroup = RplId.Invalid();
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 protected void ACR_UpdatePanel()
 {
  if (!m_ACR_Panel) return;
  bool canTake = !m_ACR_IsCommander && (m_ACR_SharedCommand || m_ACR_CommandText == "Command: AI");
  m_ACR_Panel.SetCommand(m_ACR_CommandText, m_ACR_IsCommander, canTake);
  m_ACR_Panel.SetEconomy(string.Format("RP %1   +%2 in %3s", m_ACR_RP, m_ACR_Income, m_ACR_IncomeSeconds));
  if (!m_ACR_IsCommander) m_ACR_PendingEntry = -1;
  if (!m_ACR_IsCommander) m_ACR_PendingSupport = -1;
  m_ACR_Panel.SetRequisitions(m_ACR_IsCommander, m_ACR_RequisitionOpen, m_ACR_ReqNames, m_ACR_ReqCosts, m_ACR_ReqRemaining, m_ACR_RP, m_ACR_PendingEntry);
  m_ACR_Panel.SetSupports(m_ACR_IsCommander, m_ACR_SupportOpen, m_ACR_SupNames, m_ACR_SupCosts, m_ACR_SupCooldowns, m_ACR_RP, m_ACR_PendingSupport);
  if (m_ACR_PendingSupport >= 0 && m_ACR_PendingSupport < m_ACR_SupNames.Count())
  {
   m_ACR_Panel.SetBase(m_ACR_SupNames[m_ACR_PendingSupport], "Right-click the target on the map. Click the entry again to cancel.");
   return;
  }
  if (m_ACR_PendingEntry >= 0 && m_ACR_PendingEntry < m_ACR_ReqNames.Count())
  {
   m_ACR_Panel.SetBase(m_ACR_ReqNames[m_ACR_PendingEntry], "Click one of your bases to deploy. Right-click to cancel.");
   return;
  }
  if (!m_ACR_SelectedBase.IsEmpty())
  {
   int baseIndex = m_ACR_BaseNames.Find(m_ACR_SelectedBase);
   if (baseIndex >= 0)
   {
    m_ACR_Panel.SetBase(m_ACR_BaseNames[baseIndex], m_ACR_BaseDetails[baseIndex]);
    return;
   }
   m_ACR_SelectedBase = "";
  }
  int index = -1;
  if (m_ACR_SelectedGroup.IsValid()) index = m_ACR_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0)
  {
   // Selection vanished (squad wiped out, or not in the latest snapshot yet).
   m_ACR_Panel.SetGroup("", "", false, false, 0, 0, "");
   return;
  }
  bool friendly = index < m_ACR_Commandable.Count() && m_ACR_Commandable[index] == 1;
  string hint;
  if (!friendly) hint = "Player group: not commandable";
  else if (!m_ACR_IsCommander) hint = "Take command to give orders";
  else if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) hint = "Right-click: defend there (click Defend again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) hint = "Right-click near a vehicle: get in (click Get in again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.LZ) hint = "Right-click the landing zone: they jump out when their ride sets down there";
  else if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) hint = "Right-click the destination: a free truck nearby picks them up and drops them there";
  else hint = "Right-click: move there";
  m_ACR_Panel.SetGroup(m_ACR_GroupNames[index], m_ACR_GroupDetails[index], friendly, m_ACR_IsCommander, m_ACR_GroupSymbols[index * 4 + 3], m_ACR_CursorMode, hint);
  bool missing = m_ACR_GroupDetails[index].Contains("missing");
  m_ACR_Panel.SetReinforce("Reinforce (3 RP per soldier)", missing);
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
   // Defend / Get in arm the next right-click; pressing the same button again
   // cancels back to a plain move.
   case ACR_EPanelAction.DEFEND:
    if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.DEFEND;
    return;
   case ACR_EPanelAction.GET_IN:
    if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.GET_IN;
    return;
   case ACR_EPanelAction.SET_LZ:
    if (m_ACR_CursorMode == ACR_ECursorMode.LZ) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.LZ;
    return;
   case ACR_EPanelAction.RIDE:
    if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.RIDE;
    return;
   case ACR_EPanelAction.GET_OUT: ACR_Request(ACR_ERequest.ORDER_GET_OUT, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.UNLOAD: ACR_Request(ACR_ERequest.ORDER_UNLOAD, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.REINFORCE: ACR_Request(ACR_ERequest.REINFORCE, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.TOGGLE_REQUISITION:
    m_ACR_RequisitionOpen = !m_ACR_RequisitionOpen;
    if (m_ACR_RequisitionOpen) m_ACR_SupportOpen = false;
    m_ACR_PendingEntry = -1;
    m_ACR_PendingSupport = -1;
    return;
   case ACR_EPanelAction.TOGGLE_SUPPORTS:
    m_ACR_SupportOpen = !m_ACR_SupportOpen;
    if (m_ACR_SupportOpen) m_ACR_RequisitionOpen = false;
    m_ACR_PendingEntry = -1;
    m_ACR_PendingSupport = -1;
    return;
  }
  if (action >= ACR_EPanelAction.SUPPORT_ENTRY)
  {
   int support = action - ACR_EPanelAction.SUPPORT_ENTRY;
   if (m_ACR_PendingSupport == support) m_ACR_PendingSupport = -1;
   else m_ACR_PendingSupport = support;
   m_ACR_PendingEntry = -1;
   return;
  }
  if (action >= ACR_EPanelAction.REQUISITION_ENTRY)
  {
   int entry = action - ACR_EPanelAction.REQUISITION_ENTRY;
   if (m_ACR_PendingEntry == entry) m_ACR_PendingEntry = -1; // click again to cancel
   else m_ACR_PendingEntry = entry;
  }
 }

 // Called by the map radial menu hook. Returns true if the right-click was used.
 bool ACR_HandleMapRightClick()
 {
  if (!ACR_Manager.GetInstance()) return false;
  if (m_ACR_PendingSupport >= 0)
  {
   SCR_MapEntity supportMap = SCR_MapEntity.GetMapInstance();
   if (!supportMap) return false;
   float sx, sz;
   supportMap.GetMapCursorWorldPosition(sx, sz);
   Rpc(ACR_SupportServer, m_ACR_PendingSupport, Vector(sx, 0, sz));
   m_ACR_PendingSupport = -1;
   return true;
  }
  if (m_ACR_PendingEntry >= 0)
  {
   m_ACR_PendingEntry = -1;
   return true;
  }
  if (!m_ACR_SelectedGroup.IsValid()) return false;
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
  if (m_ACR_CursorMode == ACR_ECursorMode.LZ) command = ACR_ERequest.ORDER_SET_LZ;
  if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) command = ACR_ERequest.ORDER_RIDE;
  ACR_Request(command, m_ACR_SelectedGroup, Vector(x, 0, z));
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  return true;
 }

 // ---------------------------------------------------------------- order lines
 // A line from each friendly squad's leader to its current destination, ending
 // in a dot, like the Arma 3 mod. Colour shows the order; the selected squad's
 // line is brighter and thicker.
 protected ref array<RplId> m_ACR_OrderGroups = {};
 protected ref array<vector> m_ACR_OrderTargets = {};
 protected ref array<int> m_ACR_OrderKinds = {};
 protected CanvasWidget m_ACR_OrderCanvas;
 protected ref array<ref CanvasWidgetCommand> m_ACR_OrderCommands = {};

 void ACR_SendOrders(array<RplId> groupIds, array<vector> targets, array<int> kinds)
 {
  Rpc(ACR_OrdersOwner, groupIds, targets, kinds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_OrdersOwner(array<RplId> groupIds, array<vector> targets, array<int> kinds)
 {
  m_ACR_OrderGroups.Copy(groupIds);
  m_ACR_OrderTargets.Copy(targets);
  m_ACR_OrderKinds.Copy(kinds);
 }

 protected int ACR_OrderColor(int kind, bool selected)
 {
  float alpha = 0.75;
  if (selected) alpha = 1;
  Color color = new Color(0.30, 0.65, 1.00, alpha);               // move: blue
  if (kind == 1) color = new Color(0.35, 0.90, 0.40, alpha);      // defend: green
  if (kind == 2) color = new Color(1.00, 0.85, 0.25, alpha);      // get in: yellow
  if (kind == 3) color = new Color(1.00, 0.55, 0.20, alpha);      // get out: orange
  return color.PackToInt();
 }

 protected void ACR_DrawOrders(SCR_MapEntity mapEntity)
 {
  if (!m_ACR_OrderCanvas) return;
  m_ACR_OrderCommands.Clear();
  ACR_DrawBaseAreas(mapEntity);
  ACR_DrawZones(mapEntity);
  foreach (int i, RplId groupId : m_ACR_OrderGroups)
  {
   int g = m_ACR_GroupIds.Find(groupId);
   if (g < 0) continue;
   vector from = m_ACR_LeaderPositions[g];
   IEntity leader = SCR_EntityHelper.RplIdToEntity(m_ACR_LeaderIds[g]);
   if (leader) from = leader.GetOrigin();
   vector to = m_ACR_OrderTargets[i];
   int sx0, sy0, sx1, sy1;
   mapEntity.WorldToScreen(from[0], from[2], sx0, sy0, true);
   mapEntity.WorldToScreen(to[0], to[2], sx1, sy1, true);
   float x0 = sx0;
   float y0 = sy0;
   float x1 = sx1;
   float y1 = sy1;
   bool selected = m_ACR_SelectedGroup.IsValid() && groupId == m_ACR_SelectedGroup;
   int color = ACR_OrderColor(m_ACR_OrderKinds[i], selected);

   LineDrawCommand line = new LineDrawCommand();
   line.m_iColor = color;
   line.m_fWidth = 2;
   if (selected) line.m_fWidth = 3;
   line.m_Vertices = {x0, y0, x1, y1};
   m_ACR_OrderCommands.Insert(line);

   PolygonDrawCommand dot = new PolygonDrawCommand();
   dot.m_iColor = color;
   dot.m_Vertices = {};
   m_ACR_OrderCanvas.TessellateCircle(Vector(x1, y1, 0), 6, 12, dot.m_Vertices);
   m_ACR_OrderCommands.Insert(dot);
  }
  m_ACR_OrderCanvas.SetDrawCommands(m_ACR_OrderCommands);
 }

 // ---------------------------------------------------------------- bases
 protected ref array<vector> m_ACR_BasePositions = {};
 protected ref array<string> m_ACR_BaseNames = {};
 protected ref array<string> m_ACR_BaseOwners = {};
 protected ref array<string> m_ACR_BaseDetails = {};
 protected ref array<float> m_ACR_BaseRadii = {};
 protected ref array<int> m_ACR_BaseContested = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_BaseMarkers = {};
 // Support danger areas (friendly fire missions), red rings with a countdown label.
 protected ref array<vector> m_ACR_ZonePositions = {};
 protected ref array<float> m_ACR_ZoneRadii = {};
 protected ref array<string> m_ACR_ZoneLabels = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_ZoneMarkers = {};

 void ACR_SendZones(array<vector> positions, array<float> radii, array<string> labels)
 {
  Rpc(ACR_ZonesOwner, positions, radii, labels);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_ZonesOwner(array<vector> positions, array<float> radii, array<string> labels)
 {
  m_ACR_ZonePositions.Copy(positions);
  m_ACR_ZoneRadii.Copy(radii);
  m_ACR_ZoneLabels.Copy(labels);
 }

 protected void ACR_UpdateZoneMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_ZoneMarkers.Count() != m_ACR_ZoneLabels.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_ZoneMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_ZoneMarkers.Clear();
   for (int i = 0; i < m_ACR_ZoneLabels.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_ZoneMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
  foreach (int i, ACR_UnitMarker marker : m_ACR_ZoneMarkers)
  {
   marker.m_Label = m_ACR_ZoneLabels[i];
   vector pos = m_ACR_ZonePositions[i];
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   if (marker.GetRootWidget()) marker.ACR_SetColor(new Color(1, 0.2, 0.15, 1));
  }
 }

 protected void ACR_DrawZones(SCR_MapEntity mapEntity)
 {
  foreach (int i, vector pos : m_ACR_ZonePositions)
  {
   int cx, cy, ex, ey;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   mapEntity.WorldToScreen(pos[0] + m_ACR_ZoneRadii[i], pos[2], ex, ey, true);
   float radius = Math.Max(Math.AbsFloat(ex - cx), 6);
   float fx = cx;
   float fy = cy;
   LineDrawCommand ring = new LineDrawCommand();
   Color red = new Color(1, 0.15, 0.1, 0.95);
   ring.m_iColor = red.PackToInt();
   ring.m_fWidth = 3;
   ring.m_bShouldEnclose = true;
   ring.m_Vertices = {};
   m_ACR_OrderCanvas.TessellateCircle(Vector(fx, fy, 0), radius, 48, ring.m_Vertices);
   m_ACR_OrderCommands.Insert(ring);
  }
 }
 protected string m_ACR_SelectedBase;

 void ACR_SendBases(array<vector> positions, array<string> names, array<string> owners, array<string> details, array<float> radii, array<int> contested)
 {
  Rpc(ACR_BasesOwner, positions, names, owners, details, radii, contested);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_BasesOwner(array<vector> positions, array<string> names, array<string> owners, array<string> details, array<float> radii, array<int> contested)
 {
  m_ACR_BasePositions.Copy(positions);
  m_ACR_BaseNames.Copy(names);
  m_ACR_BaseOwners.Copy(owners);
  m_ACR_BaseDetails.Copy(details);
  m_ACR_BaseRadii.Copy(radii);
  m_ACR_BaseContested.Copy(contested);
 }

 void ACR_SelectBase(string label)
 {
  m_ACR_SelectedGroup = RplId.Invalid();
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  m_ACR_SelectedBase = label;
 }

 // Friendly blue, enemy red, unclaimed yellow (the Arma 3 mod's colours).
 protected Color ACR_BaseColor(int index, float alpha)
 {
  string owner = m_ACR_BaseOwners[index];
  if (owner.IsEmpty()) return new Color(0.85, 0.72, 0.1, alpha);
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  if (mine && mine.GetFactionKey() == owner) return new Color(0.2, 0.5, 1.0, alpha);
  return new Color(0.9, 0.15, 0.15, alpha);
 }

 protected void ACR_UpdateBaseMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_BaseMarkers.Count() != m_ACR_BaseNames.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_BaseMarkers.Clear();
   for (int i = 0; i < m_ACR_BaseNames.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    marker.m_BaseIndex = i;
    m_ACR_BaseMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
  foreach (int i, ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   marker.m_BaseIndex = i;
   marker.m_Label = m_ACR_BaseNames[i];
   marker.m_Selected = m_ACR_SelectedBase == marker.m_Label;
   vector pos = m_ACR_BasePositions[i];
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   if (!marker.m_Selected && marker.GetRootWidget())
   {
    // RefreshStyle colours by unit state; bases use ownership instead.
    marker.ACR_SetColor(ACR_BaseColor(i, 1));
   }
  }
 }

 // Capture radius circles; contested bases get a thicker outline.
 protected void ACR_DrawBaseAreas(SCR_MapEntity mapEntity)
 {
  foreach (int i, vector pos : m_ACR_BasePositions)
  {
   int cx, cy, ex, ey;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   mapEntity.WorldToScreen(pos[0] + m_ACR_BaseRadii[i], pos[2], ex, ey, true);
   float radius = Math.AbsFloat(ex - cx);
   if (radius < 3) continue;
   float fx = cx;
   float fy = cy;
   LineDrawCommand ring = new LineDrawCommand();
   ring.m_iColor = ACR_BaseColor(i, 0.9).PackToInt();
   ring.m_fWidth = 2;
   if (m_ACR_BaseContested[i] == 1) ring.m_fWidth = 4;
   ring.m_bShouldEnclose = true;
   ring.m_Vertices = {};
   m_ACR_OrderCanvas.TessellateCircle(Vector(fx, fy, 0), radius, 48, ring.m_Vertices);
   m_ACR_OrderCommands.Insert(ring);
  }
 }

 // ---------------------------------------------------------------- requisition
 protected ref array<string> m_ACR_ReqNames = {};
 protected ref array<int> m_ACR_ReqCosts = {};
 protected ref array<int> m_ACR_ReqRemaining = {};
 protected bool m_ACR_RequisitionOpen;
 protected int m_ACR_PendingEntry = -1;

 void ACR_SendRequisitions(array<string> names, array<int> costs, array<int> remaining)
 {
  Rpc(ACR_RequisitionsOwner, names, costs, remaining);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RequisitionsOwner(array<string> names, array<int> costs, array<int> remaining)
 {
  m_ACR_ReqNames.Copy(names);
  m_ACR_ReqCosts.Copy(costs);
  m_ACR_ReqRemaining.Copy(remaining);
 }

 // ---------------------------------------------------------------- supports
 protected ref array<string> m_ACR_SupNames = {};
 protected ref array<int> m_ACR_SupCosts = {};
 protected ref array<int> m_ACR_SupCooldowns = {};
 protected bool m_ACR_SupportOpen;
 protected int m_ACR_PendingSupport = -1;

 void ACR_SendSupports(array<string> names, array<int> costs, array<int> cooldowns)
 {
  Rpc(ACR_SupportsOwner, names, costs, cooldowns);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_SupportsOwner(array<string> names, array<int> costs, array<int> cooldowns)
 {
  m_ACR_SupNames.Copy(names);
  m_ACR_SupCosts.Copy(costs);
  m_ACR_SupCooldowns.Copy(cooldowns);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SupportServer(int index, vector target)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  Rpc(ACR_FeedbackOwner, manager.CallSupport(this, index, target));
 }
 // Test hook: call a support without the UI.
 void ACR_TestSupport(int index, vector target)
 {
  Rpc(ACR_SupportServer, index, target);
 }

 // Base marker clicked: deploy the armed purchase there, otherwise select the base.
 void ACR_ClickBase(string label)
 {
  if (m_ACR_PendingEntry >= 0)
  {
   Rpc(ACR_PurchaseServer, m_ACR_PendingEntry, label);
   m_ACR_PendingEntry = -1;
   return;
  }
  ACR_SelectBase(label);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_PurchaseServer(int entryIndex, string baseLabel)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  Rpc(ACR_FeedbackOwner, manager.Purchase(this, entryIndex, baseLabel));
 }

 // Test hook: buy entry at base without the UI.
 void ACR_TestPurchase(int entryIndex, string baseLabel)
 {
  Rpc(ACR_PurchaseServer, entryIndex, baseLabel);
 }
 int ACR_TestFindEntry(string namePart)
 {
  foreach (int i, string entryName : m_ACR_ReqNames)
   if (entryName.Contains(namePart)) return i;
  return -1;
 }
}
