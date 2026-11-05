// Control groups on the command map (playtest 30 Oct, the user: as in RTS games). Ctrl + 1-9 keeps
// the selected squads as group N; 1-9 selects group N again (Shift + N adds it to the selection);
// N twice within ACR_GROUP_TWICE_MS centres the map on the group (ACR_FocusMap). A squad in a group
// shows its number after its name on the map ("2nd Squad [1]"). The groups are this player's, kept
// on this machine for the match; a squad that is gone drops out. The keys are the squad list's and
// the ping menu's 1-9, and do nothing for groups while one of those is open (they pick there).
// (Named to load after ACR_PlayerMapDeclutter.c, whose methods it uses.)
modded class SCR_PlayerController
{
 static const int ACR_GROUPS = 9;
 static const int ACR_GROUP_TWICE_MS = 400;
 protected ref array<ref array<RplId>> m_ACR_ControlGroups;
 protected int m_ACR_LastGroupKey = -1;
 protected int m_ACR_LastGroupAt;

 // Group n (0-8), made on first use.
 protected array<RplId> ACR_ControlGroup(int n)
 {
  if (!m_ACR_ControlGroups)
  {
   m_ACR_ControlGroups = {};
   for (int i = 0; i < ACR_GROUPS; i++) m_ACR_ControlGroups.Insert(new array<RplId>());
  }
  if (n < 0 || n >= ACR_GROUPS) return null;
  return m_ACR_ControlGroups[n];
 }

 override protected void ACR_UpdatePicker()
 {
  super.ACR_UpdatePicker();
  ACR_UpdateControlGroups();
 }

 protected void ACR_UpdateControlGroups()
 {
  if (m_ACR_PickerOpen || ACR_SquadPickerOpen() || ACR_EditorOpen() || !m_ACR_Data) return;
  InputManager input = GetGame().GetInputManager();
  if (!input) return;
  array<string> keys = {"ACR_Ping1", "ACR_Ping2", "ACR_Ping3", "ACR_Ping4", "ACR_Pick5", "ACR_Pick6", "ACR_Pick7", "ACR_Pick8", "ACR_Pick9"};
  foreach (int k, string action : keys)
  {
   if (!input.GetActionTriggered(action)) continue;
   if (ACR_ModifierHeld("ACR_MapCtrl")) ACR_KeepControlGroup(k);
   else ACR_RecallControlGroup(k, ACR_ModifierHeld("ACR_MapShift"));
   return;
  }
 }

 // Ctrl + N: the selected squads become group N (none selected: the group is emptied).
 void ACR_KeepControlGroup(int n)
 {
  array<RplId> group = ACR_ControlGroup(n);
  if (!group) return;
  group.Copy(m_ACR_Data.m_Selected);
  m_ACR_LastGroupKey = -1;
  PrintFormat("[ACR] MAP group %1 kept: %2 squads", n + 1, group.Count());
 }

 // N: group N selected (Shift: added to the selection); N twice quickly: the map on it.
 void ACR_RecallControlGroup(int n, bool add)
 {
  array<RplId> group = ACR_ControlGroup(n);
  if (!group) return;
  // A squad that is gone drops out.
  for (int i = group.Count() - 1; i >= 0; i--)
  {
   if (!m_ACR_Data.m_GroupIds.Contains(group[i])) group.Remove(i);
  }
  if (group.IsEmpty()) return;
  int now = System.GetTickCount();
  bool twice = n == m_ACR_LastGroupKey && now - m_ACR_LastGroupAt < ACR_GROUP_TWICE_MS;
  m_ACR_LastGroupKey = n;
  m_ACR_LastGroupAt = now;
  array<RplId> next = {};
  if (add) next.Copy(m_ACR_Data.m_Selected);
  foreach (RplId id : group) if (!next.Contains(id)) next.Insert(id);
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  ACR_SetSelection(next);
  if (!twice) return;
  m_ACR_LastGroupKey = -1;
  vector sum = vector.Zero;
  int count;
  foreach (RplId member : group)
  {
   int g = m_ACR_Data.m_GroupIds.Find(member);
   if (g < 0) continue;
   sum += m_ACR_Data.LeaderPos(g);
   count++;
  }
  if (count > 0) ACR_FocusMap(sum / count);
 }

 // The groups a squad is in, as its label shows them: " [1]", " [1·3]".
 protected string ACR_ControlGroupTag(RplId groupId)
 {
  if (!m_ACR_ControlGroups || !groupId.IsValid()) return "";
  string tag = "";
  foreach (int n, array<RplId> group : m_ACR_ControlGroups)
  {
   if (!group.Contains(groupId)) continue;
   if (!tag.IsEmpty()) tag += "·";
   tag += (n + 1).ToString();
  }
  if (tag.IsEmpty()) return "";
  return " [" + tag + "]";
 }

 override protected string ACR_GroupLabel(int index, string label)
 {
  label = super.ACR_GroupLabel(index, label);
  if (!m_ACR_Data || index < 0 || index >= m_ACR_Data.m_GroupIds.Count()) return label;
  return label + ACR_ControlGroupTag(m_ACR_Data.m_GroupIds[index]);
 }

 // Tests: squads a and b selected and kept as group n (as Ctrl + n+1 would); their centre.
 vector ACR_KeepGroupForTest(int n, ACR_Squad a, ACR_Squad b)
 {
  if (!a || !b || !m_ACR_Data) return vector.Zero;
  array<RplId> pair = {SCR_EntityHelper.EntityToRplId(a.m_Group), SCR_EntityHelper.EntityToRplId(b.m_Group)};
  ACR_SetSelection(pair);
  ACR_KeepControlGroup(n);
  vector sum = vector.Zero;
  int count;
  foreach (RplId id : pair)
  {
   int g = m_ACR_Data.m_GroupIds.Find(id);
   if (g < 0) continue;
   sum += m_ACR_Data.LeaderPos(g);
   count++;
  }
  if (count == 0) return vector.Zero;
  return sum / count;
 }

 string ACR_GroupMarkerLabelForTest(RplId groupId)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (marker && marker.m_GroupId == groupId) return marker.m_Label;
  }
  return "";
 }

 int ACR_ControlGroupSizeForTest(int n)
 {
  array<RplId> group = ACR_ControlGroup(n);
  if (!group) return -1;
  return group.Count();
 }
}
