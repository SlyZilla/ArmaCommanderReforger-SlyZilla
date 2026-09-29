// Local presentation uses a server-issued roster, so distant markers do not depend
// on the soldier being streamed to the client. Every action is rechecked by server.
class ACR_UnitMarker : SCR_MapMarkerBase
{
 RplId m_UnitId;
 string m_Label;
 int m_State;
 bool m_Original;
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
  m_MarkerWidgetComp.SetColor(color);
  if (m_GroupSymbol) m_MarkerWidgetComp.UpdateMilitarySymbol(m_GroupSymbol);
 }
 void Select()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc) return;
  if (m_Original) pc.ACR_Request(3);
  else pc.ACR_Request(2, m_UnitId);
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
 protected IEntity m_ACR_MenuControlBody;
 override protected void UpdateControls()
 {
  super.UpdateControls();
  if (!ACR_TestRange.s_Instance || this != GetGame().GetPlayerController()) return;
  IEntity body = GetControlledEntity();
  if (body == m_ACR_MenuControlBody) return;
  ACR_MapHandoff.SetMenuControls(m_ACR_MenuControlBody, false);
  ACR_MapHandoff.SetMenuControls(body, GetGame().GetMenuManager().IsAnyMenuOpen());
  m_ACR_MenuControlBody = body;
 }
 protected float m_ACR_LastCommand = -1000;
 void ACR_Request(int command, RplId targetId = RplId.Invalid(), vector point = vector.Zero)
 {
  if (!ACR_TestRange.s_Instance) return;
  if (command == 2 || command == 3) ACR_MapHandoff.Release(this, GetControlledEntity(), false);
  Rpc(ACR_CommandServer, command, targetId, point);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_CommandServer(int command, RplId targetId, vector point)
 {
  if (!ACR_TestRange.s_Instance) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  string message = ACR_TestRange.s_Instance.Command(this, command, targetId, point);
  Rpc(ACR_FeedbackOwner, message);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_FeedbackOwner(string message)
 {
  Print("[ACR] " + message);
  SCR_HintManagerComponent.ShowCustomHint(message, "Arma Commander test", 5, true);
 }

 protected ref array<RplId> m_ACR_Ids = {};
 protected ref array<vector> m_ACR_Positions = {};
 protected ref array<int> m_ACR_States = {};
 protected vector m_ACR_OriginalPos;
 protected ref array<ref ACR_UnitMarker> m_ACR_Markers = {};
 protected bool m_ACR_MapWasOpen;
 protected bool m_ACR_LoggedMarkers;

 void ACR_PossessAimed()
 {
  vector camera[4];
  GetGame().GetWorld().GetCurrentCamera(camera);
  IEntity hit = ACR_TraceSoldier(camera[3], camera[2]);
  if (!hit)
  {
   SCR_HintManagerComponent.ShowCustomHint("Aim directly at a friendly squad soldier within 250 metres", "Possess soldier", 4, true);
   return;
  }
  RplId targetId = SCR_EntityHelper.EntityToRplId(hit);
  int index = m_ACR_Ids.Find(targetId);
  if (index < 0)
  {
   SCR_HintManagerComponent.ShowCustomHint("The soldier under the centre of your view is not in your test squad", "No eligible target", 4, true);
   return;
  }
  PrintFormat("[ACR] AIM requested US %1 id=%2", index + 1, targetId);
  ACR_Request(2, targetId);
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
 void ACR_SendRoster(array<RplId> ids, array<vector> positions, array<int> states, vector original)
 {
  Rpc(ACR_RosterOwner, ids, positions, states, original);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RosterOwner(array<RplId> ids, array<vector> positions, array<int> states, vector original)
 {
  m_ACR_Ids.Copy(ids);
  m_ACR_Positions.Copy(positions);
  m_ACR_States.Copy(states);
  m_ACR_OriginalPos = original;
 }
 void ACR_ClearMarkers()
 {
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (manager)
  {
   foreach (ACR_UnitMarker marker : m_ACR_Markers) manager.RemoveStaticMarker(marker);
  }
  m_ACR_Markers.Clear();
  if (manager)
   foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) manager.RemoveStaticMarker(groupMarker);
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
   if (m_ACR_MapWasOpen) ACR_ClearMarkers();
   m_ACR_MapWasOpen = false;
   return;
  }
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  if (!m_ACR_MapWasOpen)
  {
   GetGame().GetCallqueue().CallLater(ACR_CenterMap, 100, false);
  }
  m_ACR_MapWasOpen = true;
  int count = m_ACR_Ids.Count();
  if (m_ACR_Markers.Count() != count + 1)
  {
   ACR_ClearMarkers();
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
   marker.m_Label = string.Format("US %1", i + 1);
   if (marker.m_State == 1) marker.m_Label += " [PLAYER]";
   if (marker.m_State == 2) marker.m_Label += " [DEAD]";
   vector pos = m_ACR_Positions[i];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(marker.m_UnitId);
   if (streamed) pos = streamed.GetOrigin();
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
  }
  ACR_UnitMarker original = m_ACR_Markers[count];
  original.m_Label = "ORIGINAL [F8]";
  original.SetWorldPos(m_ACR_OriginalPos[0], m_ACR_OriginalPos[2]);
  original.RefreshStyle();
  ACR_UpdateGroupMarkers();
  if (!m_ACR_LoggedMarkers && count == 4)
  {
   m_ACR_LoggedMarkers = true;
   PrintFormat("[ACR] MAP markers=%1 widget=%2", m_ACR_Markers.Count(), m_ACR_Markers[0].GetRootWidget() != null);
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
  if (selected) selected.Select();
  else SCR_HintManagerComponent.ShowCustomHint("Place the cursor over a unit marker, then press F7", "Possess soldier", 4, true);
 }
 void ACR_TestSelectUnit(RplId id)
 {
  foreach (ACR_UnitMarker marker : m_ACR_Markers)
   if (marker.m_UnitId == id) { marker.Select(); return; }
  Print("[ACR] MAP HANDOFF missing test marker");
 }
 void ACR_TestMapSelection()
 {
  if (m_ACR_Markers.Count() == 0)
  {
   Print("[ACR] MAP TEST failed: no markers");
   return;
  }
  RplId leaderId = SCR_EntityHelper.EntityToRplId(ACR_TestRange.s_Instance.m_CommandBridge.GetLeader());
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
   if (marker.m_UnitId == leaderId) marker.Select();
  InputManager input = GetGame().GetInputManager();
  PrintFormat("[ACR] MAP TEST possessed=%1 shortcut=%2 movement=%3", IsPossessing(), input.IsActionActive("ACR_Possess"), input.IsActionActive("CharacterForward"));
 }

 void ACR_TestLeaderMarker()
 {
  RplId leaderId = SCR_EntityHelper.EntityToRplId(ACR_TestRange.s_Instance.m_CommandBridge.GetLeader());
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
   if (marker.m_UnitId == leaderId)
    PrintFormat("[ACR] GROUP TEST label=%1 playerState=%2 widget=%3 authority=%4", marker.m_Label, marker.m_State, marker.GetRootWidget() != null, ACR_TestRange.s_Instance.m_CommandBridge.m_Commander == GetPlayerId());
 }
 bool ACR_CanCommandSquad()
 {
  if (m_ACR_LeaderIds.IsEmpty()) return false;
  return SCR_EntityHelper.EntityToRplId(GetControlledEntity()) == m_ACR_LeaderIds[0];
 }
 protected ref array<RplId> m_ACR_LeaderIds = {};
 protected ref array<vector> m_ACR_LeaderPositions = {};
 protected ref array<string> m_ACR_GroupNames = {};
 protected ref array<int> m_ACR_GroupSymbols = {};
 protected ref array<int> m_ACR_LeaderStates = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_GroupMarkers = {};
 protected bool m_ACR_GroupMarkersLogged;
 void ACR_SendGroupMarkers(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states)
 {
  Rpc(ACR_GroupMarkersOwner, leaders, positions, names, symbols, states);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_GroupMarkersOwner(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states)
 {
  m_ACR_LeaderIds.Copy(leaders);
  m_ACR_LeaderPositions.Copy(positions);
  m_ACR_GroupNames.Copy(names);
  m_ACR_GroupSymbols.Copy(symbols);
  m_ACR_LeaderStates.Copy(states);
 }
 void ACR_UpdateGroupMarkers()
 {
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (m_ACR_GroupMarkers.Count() != m_ACR_LeaderIds.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_GroupMarkers) manager.RemoveStaticMarker(old);
   m_ACR_GroupMarkers.Clear();
   for (int i = 0; i < m_ACR_LeaderIds.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    int icons = m_ACR_GroupSymbols[i * 3];
    int dimension = m_ACR_GroupSymbols[i * 3 + 1];
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
   marker.m_State = m_ACR_LeaderStates[i];
   if (marker.m_State == 1) marker.m_Label += " [PLAYER LEADER]";
   else marker.m_Label += " [AI LEADER]";
   vector pos = m_ACR_LeaderPositions[i];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(marker.m_UnitId);
   if (streamed) pos = streamed.GetOrigin();
   marker.SetWorldPos(pos[0], pos[2]);
   if (!marker.m_GroupSymbol) marker.m_GroupSymbol = new SCR_MilitarySymbol();
   marker.m_GroupSymbol.Set(EMilitarySymbolIdentity.BLUFOR, m_ACR_GroupSymbols[i * 3 + 1], m_ACR_GroupSymbols[i * 3], m_ACR_GroupSymbols[i * 3 + 2]);
   marker.RefreshStyle();
   if (!m_ACR_GroupMarkersLogged && marker.GetRootWidget())
    PrintFormat("[ACR] GROUP MARKER name=%1 icons=%2 amplifier=%3", marker.m_Label, m_ACR_GroupSymbols[i * 3], m_ACR_GroupSymbols[i * 3 + 2]);
  }
  if (m_ACR_GroupMarkers.Count() > 0) m_ACR_GroupMarkersLogged = true;
  foreach (ACR_UnitMarker unit : m_ACR_Markers)
  {
   if (unit.m_Original) continue;
   bool leader = m_ACR_LeaderIds.Contains(unit.m_UnitId);
   unit.SetServerDisabled(leader);
   unit.SetVisible(!leader);
  }
 }
}
