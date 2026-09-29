enum ACR_ERequest
{
 ADVANCE,          // test hotkey: selected squad moves 35 m east of its home
 RALLY,            // test hotkey: selected squad returns home
 POSSESS,          // take control of a soldier (targetId = soldier)
 RETURN,           // go back to your command unit
 MOVE_TO,          // squad (targetId = group, or selected) moves to point
 SWITCH_TO_GROUP,  // possess the group's leader if free, else another free soldier
 ORDER_DEFEND,     // defend point
 ORDER_GET_IN,     // board vehicle near point / nearest vehicle
 ORDER_GET_OUT,    // dismount
 ORDER_UNLOAD,     // passengers of this squad's vehicles dismount
 MODE_COMBAT,
 MODE_FAST,
 MODE_SILENT,
 TAKE_COMMAND,
 LEAVE_COMMAND
}

// Arma Commander core. One instance exists on every machine while the mode is
// running; the squad registry and all authority decisions live on the server.
// Clients only use the local-frame input handling.
class ACR_Manager : Managed
{
 protected static ref ACR_Manager s_Instance;

 ref array<ref ACR_Squad> m_Squads = {};
 protected ref map<int, ACR_Squad> m_LastSquad = new map<int, ACR_Squad>();
 protected int m_SquadNumber;

 // Command rules. One commander per faction unless shared command is on.
 bool m_SharedCommand;
 protected ref map<string, ref array<int>> m_Commanders = new map<string, ref array<int>>();
 protected ref array<int> m_LeftCommand = {};   // players who chose to leave command

 // local input state
 protected IEntity m_LastLocalBody;
 protected bool m_WasMovementActive;
 protected bool m_WasMapOpen;
 protected bool m_HintShown;

 static ACR_Manager GetInstance() { return s_Instance; }

 static ACR_Manager Start()
 {
  if (!s_Instance)
  {
   s_Instance = new ACR_Manager();
   if (Replication.IsServer()) GetGame().GetCallqueue().CallLater(s_Instance.Tick, 1000, true);
   Print("[ACR] Manager started");
  }
  return s_Instance;
 }

 static void Stop()
 {
  if (!s_Instance) return;
  if (GetGame()) GetGame().GetCallqueue().Remove(s_Instance.Tick);
  s_Instance = null;
 }

 static vector Ground(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 0.15;
  return point;
 }

 static IEntity SpawnPrefab(ResourceName prefab, vector point)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = Ground(point);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
 }

 // ================================================================ registry (server)

 void Tick()
 {
  ScanForSquads();
  EnsureCommanders();
  for (int i = m_Squads.Count() - 1; i >= 0; i--)
  {
   ACR_Squad squad = m_Squads[i];
   if (squad.IsAlive())
   {
    squad.Refresh();
    continue;
   }
   PrintFormat("[ACR] Squad removed: %1", squad.GetName());
   squad.Dispose();
   m_Squads.RemoveOrdered(i);
  }
  ACR_MapFeed.PublishAll(this);
 }

 // Any AI squad in the world, including ones spawned mid-game, is picked up
 // within a second. Player groups and our own command groups are skipped.
 void ScanForSquads()
 {
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(agent);
   if (!group) group = SCR_AIGroup.Cast(agent.GetParentGroup());
   if (group && CanRegister(group)) Register(group);
  }
 }

 bool CanRegister(SCR_AIGroup group)
 {
  if (group.IsPlayable() || group.IsSlave() || !group.GetFaction()) return false;
  if (group.GetAgentsCount() == 0) return false;
  return !FindSquadByGroup(group) && !FindSquadByCommandGroup(group);
 }

 ACR_Squad Register(SCR_AIGroup group)
 {
  m_SquadNumber++;
  ACR_Squad squad = new ACR_Squad(group, m_SquadNumber);
  m_Squads.Insert(squad);
  squad.Init();
  PrintFormat("[ACR] Squad registered: %1 faction=%2 soldiers=%3", squad.GetName(), group.GetFaction().GetFactionKey(), group.GetAgentsCount());
  return squad;
 }

 ACR_Squad FindSquadByGroup(SCR_AIGroup group)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_Group == group) return squad;
  return null;
 }

 ACR_Squad FindSquadByCommandGroup(SCR_AIGroup group)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_CommandGroup == group) return squad;
  return null;
 }

 ACR_Squad FindSquadBySoldier(IEntity soldier)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.IndexOf(soldier) >= 0) return squad;
  return null;
 }

 ACR_Squad FindSquadLedBy(SCR_PlayerController pc)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.IsLeader(pc)) return squad;
  return null;
 }

 void ReleaseAuthority(SCR_PlayerController pc)
 {
  foreach (ACR_Squad squad : m_Squads)
   if (squad.m_Commander == pc.GetPlayerId()) squad.Release();
 }

 // Squad that hotkey/map orders apply to until real map selection exists:
 // the squad you are in, else the squad you were last in, else your faction's first.
 ACR_Squad GetSelectedSquad(SCR_PlayerController pc)
 {
  ACR_Squad squad = FindSquadBySoldier(pc.GetControlledEntity());
  if (squad) return squad;
  ACR_Squad last;
  if (m_LastSquad.Find(pc.GetPlayerId(), last) && last && m_Squads.Contains(last)) return last;
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  foreach (ACR_Squad candidate : m_Squads)
   if (candidate.GetFaction() == faction) return candidate;
  return null;
 }

 // ================================================================ command rules (server)

 array<int> GetCommanders(Faction faction)
 {
  if (!faction) return null;
  array<int> list;
  if (!m_Commanders.Find(faction.GetFactionKey(), list))
  {
   list = {};
   m_Commanders.Insert(faction.GetFactionKey(), list);
  }
  return list;
 }

 bool IsCommander(int playerId)
 {
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(playerId));
  return list && list.Contains(playerId);
 }

 // Drop commanders who left or switched sides; if a faction has players but
 // no commander, the first one who hasn't opted out takes command.
 void EnsureCommanders()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (string key, array<int> list : m_Commanders)
  {
   for (int i = list.Count() - 1; i >= 0; i--)
   {
    Faction faction = SCR_FactionManager.SGetPlayerFaction(list[i]);
    if (!players.Contains(list[i]) || !faction || faction.GetFactionKey() != key) list.RemoveOrdered(i);
   }
  }
  for (int i = m_LeftCommand.Count() - 1; i >= 0; i--)
   if (!players.Contains(m_LeftCommand[i])) m_LeftCommand.RemoveOrdered(i);
  foreach (int id : players)
  {
   Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!playerFaction || m_LeftCommand.Contains(id)) continue;
   array<int> factionCommanders = GetCommanders(playerFaction);
   if (!factionCommanders.IsEmpty()) continue;
   factionCommanders.Insert(id);
   Notify(id, "You are in command");
   PrintFormat("[ACR] Commander: %1 takes command of %2", GetGame().GetPlayerManager().GetPlayerName(id), playerFaction.GetFactionKey());
  }
 }

 string CommandStatus(int playerId)
 {
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(playerId));
  if (!list || list.IsEmpty()) return "Command: AI";
  if (m_SharedCommand)
  {
   if (list.Contains(playerId)) return string.Format("Shared command (%1 commanders)", list.Count());
   return "Shared command";
  }
  if (list.Contains(playerId)) return "You are in command";
  return string.Format("Command: %1", GetGame().GetPlayerManager().GetPlayerName(list[0]));
 }

 string TakeCommand(SCR_PlayerController pc)
 {
  int id = pc.GetPlayerId();
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(id));
  if (!list) return "You have no faction";
  if (list.Contains(id)) return "You are already in command";
  if (!m_SharedCommand && !list.IsEmpty())
   return string.Format("%1 is in command", GetGame().GetPlayerManager().GetPlayerName(list[0]));
  m_LeftCommand.RemoveItem(id);
  list.Insert(id);
  return "You are in command";
 }

 string LeaveCommand(SCR_PlayerController pc)
 {
  int id = pc.GetPlayerId();
  array<int> list = GetCommanders(SCR_FactionManager.SGetPlayerFaction(id));
  if (!list || !list.Contains(id)) return "You are not in command";
  list.RemoveItem(id);
  if (!m_LeftCommand.Contains(id)) m_LeftCommand.Insert(id);
  EnsureCommanders();
  return "You left command";
 }

 void Notify(int playerId, string message)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (pc) pc.ACR_Notify(message);
 }

 // ================================================================ requests (server)

 string HandleRequest(SCR_PlayerController pc, int command, RplId targetId, vector point)
 {
  if (!Replication.IsServer() || !pc) return "Server unavailable";
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  switch (command)
  {
   case ACR_ERequest.RETURN: return ReturnToCommandUnit(pc, body);
   case ACR_ERequest.POSSESS: return Possess(pc, body, targetId);
   case ACR_ERequest.SWITCH_TO_GROUP: return SwitchToGroup(pc, body, targetId);
   case ACR_ERequest.TAKE_COMMAND: return TakeCommand(pc);
   case ACR_ERequest.LEAVE_COMMAND: return LeaveCommand(pc);
  }
  return Order(pc, command, targetId, point);
 }

 string ReturnToCommandUnit(SCR_PlayerController pc, ChimeraCharacter body)
 {
  if (!pc.IsPossessing()) return "You are already in your command unit";
  ACR_MapHandoff.Release(pc, body);
  ReleaseAuthority(pc);
  pc.SetPossessedEntity(null);
  ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
  PrintFormat("[ACR] RETURN player=%1", pc.GetPlayerId());
  return "Returned to your command unit";
 }

 string Possess(SCR_PlayerController pc, ChimeraCharacter body, RplId targetId)
 {
  ChimeraCharacter target = ChimeraCharacter.Cast(SCR_EntityHelper.RplIdToEntity(targetId));
  if (!target) return "Aim directly at a friendly soldier, or select its map marker";
  if (target == body) return "You already control this soldier";
  ACR_Squad squad = FindSquadBySoldier(target);
  if (!squad) return "That soldier is not in a commandable squad";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  if (target.GetCharacterController().IsDead()) return "That soldier is dead";
  if (ACR_Squad.FindController(target)) return "Another player controls that soldier";
  SCR_PossessingManagerComponent possessing = SCR_PossessingManagerComponent.GetInstance();
  if (possessing && possessing.GetIdFromMainEntity(target) != 0) return "That soldier is a player's command unit";

  // The engine supports chained possession and preserves the command unit.
  ACR_MapHandoff.Release(pc, body);
  ReleaseAuthority(pc);
  pc.SetPossessedEntity(target);
  squad.Refresh();
  ACR_MapHandoff.ClearCharacterInput(target);
  m_LastSquad.Set(pc.GetPlayerId(), squad);
  PrintFormat("[ACR] POSSESS player=%1 squad=%2 index=%3", pc.GetPlayerId(), squad.GetName(), squad.IndexOf(target));
  if (pc.GetControlledEntity() != target) return "Control transfer failed; try again";
  if (squad.IsLeader(pc))
   return string.Format("Now leading %1. Native squad commands are available. F7 swaps again; F8 returns.", squad.GetName());
  return string.Format("Now controlling %1-%2. F7 swaps again; F8 returns to your command unit.", squad.GetName(), squad.NumberOf(target));
 }

 // Arma Commander's Switch button: the leader if free, otherwise any free soldier.
 string SwitchToGroup(SCR_PlayerController pc, ChimeraCharacter body, RplId groupId)
 {
  ACR_Squad squad = FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) return "Select a friendly squad first";
  array<IEntity> candidates = {};
  IEntity leader = squad.GetLeader();
  if (leader) candidates.Insert(leader);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() && !candidates.Contains(agent.GetControlledEntity())) candidates.Insert(agent.GetControlledEntity());
  foreach (IEntity candidate : candidates)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(candidate);
   if (!soldier || soldier == body || soldier.GetCharacterController().IsDead() || ACR_Squad.FindController(soldier)) continue;
   return Possess(pc, body, SCR_EntityHelper.EntityToRplId(soldier));
  }
  return string.Format("%1 has no free soldier to take over", squad.GetName());
 }

 string Order(SCR_PlayerController pc, int command, RplId groupId, vector point)
 {
  ACR_Squad squad = FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad) squad = GetSelectedSquad(pc);
  if (!squad) return "No squad selected";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  if (!IsCommander(pc.GetPlayerId())) return "Only the commander can give map orders. Take command first.";
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return "That squad is not ready";
  bool usesPoint = command == ACR_ERequest.MOVE_TO || command == ACR_ERequest.ORDER_DEFEND || command == ACR_ERequest.ORDER_GET_IN;
  // Reject nonsense coordinates from a client.
  if (usesPoint && vector.Distance(point, anchor.GetOrigin()) > 20000) return "Order is too far away";
  if (usesPoint) point = Ground(point);
  m_LastSquad.Set(pc.GetPlayerId(), squad);
  string name = squad.GetName();
  switch (command)
  {
   case ACR_ERequest.ADVANCE:
    if (squad.MoveTo(Ground(squad.m_Home + Vector(35, 0, 0)))) return name + ": moving";
    break;
   case ACR_ERequest.RALLY:
    if (squad.MoveTo(Ground(squad.m_Home))) return name + ": rallying";
    break;
   case ACR_ERequest.MOVE_TO:
    if (squad.MoveTo(point))
    {
     PrintFormat("[ACR] MOVE player=%1 squad=%2 target=%3", pc.GetPlayerId(), name, point);
     return name + ": moving";
    }
    break;
   case ACR_ERequest.ORDER_DEFEND:
    if (squad.Defend(point)) return name + ": defending";
    break;
   case ACR_ERequest.ORDER_GET_IN:
    if (squad.GetIn(point)) return name + ": getting in";
    return name + ": no vehicle found";
   case ACR_ERequest.ORDER_GET_OUT:
    if (squad.GetOut()) return name + ": getting out";
    return name + " is not in a vehicle";
   case ACR_ERequest.ORDER_UNLOAD:
    return Unload(squad);
   case ACR_ERequest.MODE_COMBAT:
    squad.SetMode(ACR_EMoveMode.COMBAT);
    return name + ": combat";
   case ACR_ERequest.MODE_FAST:
    squad.SetMode(ACR_EMoveMode.FAST);
    return name + ": fast";
   case ACR_ERequest.MODE_SILENT:
    squad.SetMode(ACR_EMoveMode.SILENT);
    return name + ": silent";
   default:
    return "Unknown command";
  }
  return name + ": could not create that order";
 }

 // Every other squad riding in this squad's vehicles dismounts.
 string Unload(ACR_Squad transport)
 {
  array<IEntity> vehicles = {};
  transport.GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return transport.GetName() + " is not in a vehicle";
  int unloaded;
  array<IEntity> passengerVehicles = {};
  foreach (ACR_Squad squad : m_Squads)
  {
   if (squad == transport) continue;
   squad.GetVehicles(passengerVehicles);
   foreach (IEntity vehicle : passengerVehicles)
   {
    if (!vehicles.Contains(vehicle)) continue;
    if (squad.GetOut()) unloaded++;
    break;
   }
  }
  if (!unloaded) return transport.GetName() + " has no passengers";
  return string.Format("%1: unloading %2 squad(s)", transport.GetName(), unloaded);
 }

 // ================================================================ local input (every machine)

 void OnFrame()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !pc.GetControlledEntity()) return;
  InputManager input = GetGame().GetInputManager();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool mapOpen = mapEntity && mapEntity.IsOpen();
  // Activate independently of the character context: possession changes its update order.
  if (mapOpen) input.ActivateContext("ACR_MapContext");
  else input.ActivateContext("ACR_TestContext");
  bool movementActive = input.IsActionActive("CharacterForward") && !mapOpen;
  IEntity localBody = pc.GetControlledEntity();
  if (localBody != m_LastLocalBody || (m_WasMovementActive && !movementActive) || mapOpen != m_WasMapOpen)
  {
   input.ResetAction("CharacterForward");
   input.ResetAction("CharacterRight");
   ACR_MapHandoff.ClearCharacterInput(localBody);
  }
  if (localBody != m_LastLocalBody)
  {
   SCR_PlayerControllerCommandingComponent commanding = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
   if (commanding) commanding.ACR_CloseCommandMenu();
  }
  m_LastLocalBody = localBody;
  m_WasMovementActive = movementActive;
  m_WasMapOpen = mapOpen;
  pc.ACR_UpdateMap();
  if (!m_HintShown)
  {
   m_HintShown = true;
   SCR_HintManagerComponent.ShowCustomHint("Map: click a squad to select, right-click to order | F7: possess aimed soldier (or hovered marker) | F8: command unit | F9: map", "Arma Commander", 30, true);
  }
  if (input.GetActionTriggered("ACR_Move"))
  {
   if (mapOpen)
   {
    float x, z;
    mapEntity.GetMapCursorWorldPosition(x, z);
    pc.ACR_Request(ACR_ERequest.MOVE_TO, pc.ACR_GetSelectedGroup(), Vector(x, 0, z));
   }
   else pc.ACR_Request(ACR_ERequest.ADVANCE);
  }
  if (input.GetActionTriggered("ACR_Rally")) pc.ACR_Request(ACR_ERequest.RALLY);
  if (input.GetActionTriggered("ACR_Possess"))
  {
   if (mapOpen) pc.ACR_PossessMapCursor();
   else pc.ACR_PossessAimed();
  }
  if (input.GetActionTriggered("ACR_Return")) pc.ACR_Request(ACR_ERequest.RETURN);
  if (input.GetActionTriggered("ACR_Map"))
  {
   if (mapOpen) GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
   else GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  }
 }
}
