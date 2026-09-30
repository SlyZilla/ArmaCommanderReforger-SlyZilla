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
   PublishZones(manager, pc, key);
   PublishOrders(manager, pc, faction);
   PublishCapture(pc, key);
  }
 }

 // Capture bar for a player standing in a base ring (empty label = hide).
 //  tone: 0 ours / neutral progress, 1 we are taking it, 2 the enemy is taking it
 static void PublishCapture(SCR_PlayerController pc, string key)
 {
  IEntity body = pc.GetControlledEntity();
  string label;
  float progress;
  int tone;
  if (body)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) > base.m_fRadius || !base.IsVisibleTo(key)) continue;
    if (base.m_sOwner == key)
    {
     if (!base.m_bContested && base.m_fScore >= 100) break;
     label = base.m_sName + ": holding";
     if (base.m_bContested) label = base.m_sName + ": under attack";
     progress = (base.m_fScore + 100) / 200;
     tone = 2;
     if (!base.m_bContested) tone = 0;
    }
    else if (!base.m_sOwner.IsEmpty())
    {
     label = "Capturing " + base.m_sName;
     progress = (100 - base.m_fScore) / 200;
     tone = 1;
    }
    else if (base.m_sCapturer == key)
    {
     label = "Capturing " + base.m_sName;
     progress = base.m_fScore / 100;
     tone = 1;
    }
    else if (!base.m_sCapturer.IsEmpty())
    {
     label = base.m_sName + ": enemy capturing";
     progress = base.m_fScore / 100;
     tone = 2;
    }
    else
    {
     label = base.m_sName + ": neutral";
     progress = 0;
     tone = 0;
    }
    if (base.m_bContested && tone == 1) label += " (contested)";
    break;
   }
  }
  pc.ACR_SendCapture(label, progress, tone);
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
  // Status bars: 4 ints per group (health %, ammo % or -1, fuel % or -1, service stock % or -1).
  array<int> status = {};
  foreach (RplId statusGroup : groupIds)
  {
   ACR_Squad known = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(statusGroup)));
   if (!known)
   {
    status.Insert(-1);
    status.Insert(-1);
    status.Insert(-1);
    status.Insert(-1);
    continue;
   }
   status.Insert(known.m_iHealthPct);
   status.Insert(known.m_iAmmoPct);
   status.Insert(known.m_iFuelPct);
   status.Insert(known.m_iStockPct);
  }
  pc.ACR_SendGroupStatus(groupIds, status);
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
   RplId groupId = SCR_EntityHelper.EntityToRplId(squad.m_Group);
   groupIds.Insert(groupId);
   targets.Insert(target);
   kinds.Insert(kind);
   // Then the rest of the plan (route legs, queued waypoints) as a chain.
   array<vector> planned = {};
   array<int> plannedKinds = {};
   squad.GetPlannedPoints(planned, plannedKinds);
   foreach (int p, vector point : planned)
   {
    groupIds.Insert(groupId);
    targets.Insert(point);
    kinds.Insert(plannedKinds[p]);
   }
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
 static void PublishZones(ACR_Manager manager, SCR_PlayerController pc, string factionKey)
 {
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  if (manager.m_Supports)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   foreach (ACR_SupportZone zone : manager.m_Supports.m_Zones)
   {
    if (zone.m_sFaction != factionKey) continue;
    positions.Insert(zone.m_Pos);
    radii.Insert(zone.m_fRadius);
    int left = Math.Ceil((zone.m_fImpactAt - now) / 1000);
    if (left > 0) labels.Insert(string.Format("%1 - %2 s", zone.m_sName, left));
    else labels.Insert(zone.m_sName + " - ACTIVE");
   }
  }
  pc.ACR_SendZones(positions, radii, labels);
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
