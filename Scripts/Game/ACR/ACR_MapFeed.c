// Server -> owner snapshots for the command map. Sent once a second so markers
// do not depend on distant soldiers being streamed to the client. Every action a
// client takes from these markers is re-validated by the server.
class ACR_MapFeed
{
 static void PublishAll(ACR_Manager manager)
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc || !pc.GetControlledEntity()) continue;
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction) continue;
   PublishRoster(manager, pc, faction);
   PublishGroups(manager, pc, faction);
   string key = faction.GetFactionKey();
   pc.ACR_SendCommandStatus(manager.IsCommander(id), manager.CommandStatus(id), manager.m_SharedCommand, manager.m_Bases.GetRP(key), manager.m_Bases.GetIncome(key), manager.m_Bases.SecondsToIncome());
   PublishBases(manager, pc, key);
   PublishRequisitions(manager, pc, key);
   PublishSupports(manager, pc, key);
   PublishOrders(manager, pc, faction);
  }
 }

 // Every soldier of every friendly squad, plus the player's command unit.
 static void PublishRoster(ACR_Manager manager, SCR_PlayerController pc, Faction faction)
 {
  array<RplId> ids = {};
  array<vector> positions = {};
  array<int> states = {};
  array<string> labels = {};
  array<RplId> groupIds = {};
  array<AIAgent> agents = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetFaction() != faction) continue;
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!soldier || soldier.GetCharacterController().IsDead()) continue;
    ids.Insert(SCR_EntityHelper.EntityToRplId(soldier));
    positions.Insert(soldier.GetOrigin());
    labels.Insert(string.Format("%1-%2", squad.GetName(), squad.NumberOf(soldier)));
    groupIds.Insert(SCR_EntityHelper.EntityToRplId(squad.m_Group));
    int state = 0;
    if (ACR_Squad.FindController(soldier) != 0) state = 1;
    states.Insert(state);
   }
  }
  IEntity original = pc.GetMainEntity();
  vector originalPos;
  if (original) originalPos = original.GetOrigin();
  pc.ACR_SendRoster(ids, positions, states, labels, groupIds, originalPos);
 }

 // A named, role-symbol marker at the leader of every friendly group: our
 // squads (commandable) and any other group such as player groups.
 static void PublishGroups(ACR_Manager manager, SCR_PlayerController pc, Faction faction)
 {
  array<SCR_AIGroup> groups = {};
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.m_Group && squad.GetFaction() == faction) groups.Insert(squad.m_Group);
  SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
  if (groupsManager)
  {
   array<SCR_AIGroup> playable = groupsManager.GetPlayableGroupsByFaction(faction);
   if (playable)
    foreach (SCR_AIGroup group : playable)
     if (group && !groups.Contains(group)) groups.Insert(group);
  }
  array<RplId> leaders = {};
  array<vector> positions = {};
  array<string> names = {};
  array<int> symbols = {};
  array<int> states = {};
  array<int> commandable = {};
  array<RplId> groupIds = {};
  array<string> details = {};
  foreach (SCR_AIGroup group : groups)
  {
   if (!group || group.GetFaction() != faction) continue;
   if (manager.FindSquadByCommandGroup(group)) continue;
   ACR_Squad squad = manager.FindSquadByGroup(group);
   IEntity leader = group.GetLeaderEntity();
   if (squad) leader = squad.GetLeader();
   else if (group.GetLeaderID() > 0) leader = GetGame().GetPlayerManager().GetPlayerControlledEntity(group.GetLeaderID());
   ChimeraCharacter character = ChimeraCharacter.Cast(leader);
   if (!character || character.GetCharacterController().IsDead()) continue;
   RplId id = SCR_EntityHelper.EntityToRplId(leader);
   if (leaders.Contains(id)) continue;
   SCR_EditableGroupComponent editable = SCR_EditableGroupComponent.Cast(group.FindComponent(SCR_EditableGroupComponent));
   SCR_EditableGroupUIInfo info;
   if (editable) info = SCR_EditableGroupUIInfo.Cast(editable.GetInfo());
   SCR_MilitarySymbol symbol;
   if (info) symbol = info.GetMilitarySymbol();
   string name;
   if (squad) name = squad.GetName();
   if (name.IsEmpty()) name = group.GetCustomName();
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
   int mode = 0;
   if (squad) mode = squad.m_Mode;
   symbols.Insert(mode); // 4 ints per group: icons, dimension, amplifier, move mode
   int state = 0;
   if (ACR_Squad.FindController(leader) > 0) state = 1;
   states.Insert(state);
   int canCommand = 0;
   if (squad) canCommand = 1;
   commandable.Insert(canCommand);
   groupIds.Insert(SCR_EntityHelper.EntityToRplId(group));
   string detail;
   if (squad) detail = squad.Describe();
   details.Insert(detail);

  }
  pc.ACR_SendGroupMarkers(leaders, positions, names, symbols, states, commandable, groupIds, details);
 }
 // Current destination of every friendly squad, for the order lines on the map.
 static void PublishOrders(ACR_Manager manager, SCR_PlayerController pc, Faction faction)
 {
  array<RplId> groupIds = {};
  array<vector> targets = {};
  array<int> kinds = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetFaction() != faction) continue;
   vector target;
   int kind;
   if (!squad.GetOrderTarget(target, kind)) continue;
   groupIds.Insert(SCR_EntityHelper.EntityToRplId(squad.m_Group));
   targets.Insert(target);
   kinds.Insert(kind);
  }
  pc.ACR_SendOrders(groupIds, targets, kinds);
 }
 // Bases this faction can see (hidden enemy bases only once discovered).
 static void PublishBases(ACR_Manager manager, SCR_PlayerController pc, string factionKey)
 {
  array<vector> positions = {};
  array<string> names = {};
  array<string> owners = {};
  array<string> details = {};
  array<float> radii = {};
  array<int> contested = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.IsVisibleTo(factionKey)) continue;
   positions.Insert(base.GetOrigin());
   names.Insert(base.GetLabel());
   owners.Insert(base.m_sOwner);
   details.Insert(manager.m_Bases.Describe(base));
   radii.Insert(base.m_fRadius);
   int flag = 0;
   if (base.m_bContested) flag = 1;
   contested.Insert(flag);
  }
  pc.ACR_SendBases(positions, names, owners, details, radii, contested);
 }
 static void PublishSupports(ACR_Manager manager, SCR_PlayerController pc, string factionKey)
 {
  array<string> names = {};
  array<int> costs = {};
  array<int> cooldowns = {};
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_SupportEntry entry : manager.m_Bases.GetFaction(factionKey).m_aSupports)
  {
   names.Insert(entry.m_sName);
   costs.Insert(entry.m_iCost);
   int left = Math.Ceil((entry.m_fReadyAt - now) / 1000);
   if (left < 0) left = 0;
   cooldowns.Insert(left);
  }
  pc.ACR_SendSupports(names, costs, cooldowns);
 }
 static void PublishRequisitions(ACR_Manager manager, SCR_PlayerController pc, string factionKey)
 {
  array<string> names = {};
  array<int> costs = {};
  array<int> remaining = {};
  foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(factionKey).m_aRequisitions)
  {
   names.Insert(entry.m_sName);
   costs.Insert(entry.m_iCost);
   remaining.Insert(entry.m_iRemaining);
  }
  pc.ACR_SendRequisitions(names, costs, remaining);
 }
}
