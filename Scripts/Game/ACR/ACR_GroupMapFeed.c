class ACR_GroupMapFeed
{
 static void Publish(ACR_TestRange range, SCR_PlayerController pc)
 {
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!manager || !faction) return;
  array<SCR_AIGroup> groups = {};
  groups.Insert(range.m_Group);
  array<AIAgent> allAgents = {};
  GetGame().GetAIWorld().GetAIAgents(allAgents);
  foreach (AIAgent agent : allAgents)
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(agent);
   if (!group) group = SCR_AIGroup.Cast(agent.GetParentGroup());
   if (group && !groups.Contains(group)) groups.Insert(group);
  }
  array<SCR_AIGroup> playable = manager.GetPlayableGroupsByFaction(faction);
  foreach (SCR_AIGroup group : playable)
   if (group && !groups.Contains(group)) groups.Insert(group);
  array<RplId> leaders = {};
  array<vector> positions = {};
  array<string> names = {};
  array<int> symbols = {};
  array<int> states = {};
  foreach (SCR_AIGroup group : groups)
  {
   if (!group || group.GetFaction() != faction) continue;
   if (range.m_CommandBridge && group == range.m_CommandBridge.m_CommandGroup) continue;
   IEntity leader = group.GetLeaderEntity();
   if (group == range.m_Group && range.m_CommandBridge) leader = range.m_CommandBridge.GetLeader();
   if (group.GetLeaderID() > 0) leader = GetGame().GetPlayerManager().GetPlayerControlledEntity(group.GetLeaderID());
   ChimeraCharacter character = ChimeraCharacter.Cast(leader);
   if (!character || character.GetCharacterController().IsDead()) continue;
   RplId id = SCR_EntityHelper.EntityToRplId(leader);
   if (leaders.Contains(id)) continue;
   SCR_EditableGroupComponent editable = SCR_EditableGroupComponent.Cast(group.FindComponent(SCR_EditableGroupComponent));
   SCR_EditableGroupUIInfo info;
   if (editable) info = SCR_EditableGroupUIInfo.Cast(editable.GetInfo());
   SCR_MilitarySymbol symbol;
   if (info) symbol = info.GetMilitarySymbol();
   string name = group.GetCustomName();
   if (name.IsEmpty()) name = SCR_GroupHelperUI.GetTranslatedGroupName(group);
   if (name.IsEmpty() && info) name = info.GetName();
   if (name.IsEmpty()) name = "Friendly group";
   int icons = EMilitarySymbolIcon.INFANTRY;
   int dimension = EMilitarySymbolDimension.LAND;
   int amplifier = EMilitarySymbolAmplifier.SQUAD;
   if (symbol)
   {
    icons = symbol.GetIcons();
    dimension = symbol.GetDimension();
    amplifier = symbol.GetAmplifier();
   }
   leaders.Insert(id);
   positions.Insert(leader.GetOrigin());
   names.Insert(name);
   symbols.Insert(icons);
   symbols.Insert(dimension);
   symbols.Insert(amplifier);
   int state = 0;
   if (ACR_CommandBridge.FindController(leader) > 0) state = 1;
   states.Insert(state);
  }
  pc.ACR_SendGroupMarkers(leaders, positions, names, symbols, states);
 }
}
