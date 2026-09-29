enum ACR_ERequest
{
 ADVANCE,   // test hotkey: selected squad moves 35 m east of its home
 RALLY,     // test hotkey: selected squad returns home
 POSSESS,   // take control of a soldier (targetId)
 RETURN,    // go back to your command unit
 MOVE_TO    // selected squad moves to a map point
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

 // ================================================================ requests (server)

 string HandleRequest(SCR_PlayerController pc, int command, RplId targetId, vector point)
 {
  if (!Replication.IsServer() || !pc) return "Server unavailable";
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  if (!body || body.GetCharacterController().IsDead()) return "Wait until you have spawned";
  if (command == ACR_ERequest.RETURN) return ReturnToCommandUnit(pc, body);
  if (command == ACR_ERequest.POSSESS) return Possess(pc, body, targetId);
  if (command == ACR_ERequest.ADVANCE || command == ACR_ERequest.RALLY || command == ACR_ERequest.MOVE_TO)
   return Order(pc, command, point);
  return "Unknown command";
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

 string Order(SCR_PlayerController pc, int command, vector point)
 {
  ACR_Squad squad = GetSelectedSquad(pc);
  if (!squad) return "No squad selected. Possess a squad soldier first.";
  if (squad.GetFaction() != SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId())) return "That squad belongs to another faction";
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return "That squad is not ready";
  vector destination = squad.m_Home + Vector(35, 0, 0);
  if (command == ACR_ERequest.RALLY) destination = squad.m_Home;
  if (command == ACR_ERequest.MOVE_TO)
  {
   // Reject nonsense coordinates from a client.
   if (vector.Distance(point, anchor.GetOrigin()) > 20000) return "Order is too far away";
   destination = point;
  }
  destination = Ground(destination);
  if (!squad.MoveTo(destination)) return "Could not create a movement order";
  m_LastSquad.Set(pc.GetPlayerId(), squad);
  PrintFormat("[ACR] MOVE player=%1 squad=%2 target=%3 command=%4", pc.GetPlayerId(), squad.GetName(), destination, command);
  return string.Format("%1: movement order accepted", squad.GetName());
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
   SCR_HintManagerComponent.ShowCustomHint("F5: squad advance / move to map cursor | F6: squad rally | F7: possess aimed soldier (or map marker) | F8: return | F9: command map", "Arma Commander", 30, true);
  }
  if (input.GetActionTriggered("ACR_Move"))
  {
   if (mapOpen)
   {
    float x, z;
    mapEntity.GetMapCursorWorldPosition(x, z);
    pc.ACR_Request(ACR_ERequest.MOVE_TO, RplId.Invalid(), Vector(x, 0, z));
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
