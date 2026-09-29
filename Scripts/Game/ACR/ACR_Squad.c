enum ACR_EMoveMode
{
 COMBAT,
 FAST,
 SILENT
}

// One commandable AI squad. Server-side only.
//
// Native squad commands need a player-led playable group, so each squad owns a
// hidden playable "command group" linked to it. When a player possesses the
// squad's designated leader, they join that command group and gain the native
// command menu. The possessed leader's AI agent temporarily moves into the
// command group, because a player-held agent inside the AI group stops the rest
// of the formation from following waypoints. The logical roster (GetAgents)
// always includes that detached leader.
class ACR_Squad : Managed
{
 SCR_AIGroup m_Group;
 SCR_AIGroup m_CommandGroup;
 IEntity m_Leader;            // designated leader; survives possession
 AIAgent m_DetachedLeader;    // leader agent parked in m_CommandGroup while possessed
 int m_Commander;             // player id holding native command authority, 0 = none
 int m_PreviousGroup = -1;    // commander's own playable group before taking command
 bool m_PreviousDeleteIfEmpty;
 string m_Name;
 vector m_Home;               // position at registration; used by the test RALLY order
 AIWaypoint m_Waypoint;       // last waypoint we spawned, deleted when replaced
 ref array<IEntity> m_Numbering = {}; // soldiers in first-seen order; gives stable "Alpha 1-2" labels

 void ACR_Squad(SCR_AIGroup group, int number)
 {
  m_Group = group;
  m_Name = group.GetCustomName();
  if (m_Name.IsEmpty()) m_Name = SCR_GroupHelperUI.GetTranslatedGroupName(group);
  if (m_Name.IsEmpty()) m_Name = string.Format("Squad %1", number);
  IEntity leader = group.GetLeaderEntity();
  if (leader) m_Home = leader.GetOrigin();
 }

 void Init()
 {
  if (!m_Group) return;
  // We clean up empty squads ourselves, so a squad whose only survivor is the
  // detached (possessed) leader is not deleted out from under us.
  m_Group.SetDeleteWhenEmpty(false);
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  if (!manager) return;
  m_CommandGroup = manager.CreateNewPlayableGroup(m_Group.GetFaction(), SCR_EGroupRole.ASSAULT);
  if (!m_CommandGroup) return;
  m_CommandGroup.SetDeleteWhenEmpty(false);
  m_CommandGroup.SetCanDeleteIfNoPlayer(false);
  m_CommandGroup.SetPrivate(true);
  m_CommandGroup.SetPrivacyChangeable(false);
  m_CommandGroup.SetCustomName(m_Name, 0);
  // CreateNewPlayableGroup links an empty subgroup on a delayed RPC. Link our
  // squad after that initialization has completed.
  GetGame().GetCallqueue().CallLater(LinkSquad, 3000, false);
 }

 void LinkSquad()
 {
  if (!m_CommandGroup || !m_Group) return;
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(m_CommandGroup), SCR_EntityHelper.EntityToRplId(m_Group));
 }

 // ---------------------------------------------------------------- roster

 string GetName() { return m_Name; }
 Faction GetFaction()
 {
  if (!m_Group) return null;
  return m_Group.GetFaction();
 }

 void GetAgents(array<AIAgent> agents)
 {
  agents.Clear();
  if (m_Group) m_Group.GetAgents(agents);
  if (m_DetachedLeader && !agents.Contains(m_DetachedLeader)) agents.Insert(m_DetachedLeader);
 }

 // 0-based position of a soldier in the roster, -1 if not a member.
 int IndexOf(IEntity soldier)
 {
  if (!soldier) return -1;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
   if (agent.GetControlledEntity() == soldier) return i;
  return -1;
 }

 // Stable 1-based number for a member. Roster order changes when the leader is
 // detached for possession, so labels must not come from roster position.
 int NumberOf(IEntity soldier)
 {
  if (!soldier) return 0;
  int index = m_Numbering.Find(soldier);
  if (index < 0) index = m_Numbering.Insert(soldier);
  return index + 1;
 }

 bool IsAlive()
 {
  if (m_DetachedLeader) return true;
  return m_Group && m_Group.GetAgentsCount() > 0;
 }

 IEntity GetAnchor()
 {
  IEntity leader = GetLeader();
  if (leader) return leader;
  if (!m_Group) return null;
  return m_Group.GetLeaderEntity();
 }

 // Resolve occupation from controllers' current bodies, including possession.
 static int FindController(IEntity entity)
 {
  if (!entity) return 0;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc && pc.GetControlledEntity() == entity) return id;
  }
  return 0;
 }

 // ---------------------------------------------------------------- leadership

 // Keep the designated leader across possession: the engine may temporarily
 // choose another active AI agent as its internal formation leader.
 IEntity GetLeader()
 {
  ChimeraCharacter leader = ChimeraCharacter.Cast(m_Leader);
  if (!leader || leader.GetCharacterController().IsDead())
  {
   if (m_Group) m_Leader = m_Group.GetLeaderEntity();
  }
  return m_Leader;
 }

 bool IsLeader(SCR_PlayerController pc)
 {
  if (!pc || !m_Group) return false;
  ChimeraCharacter body = ChimeraCharacter.Cast(pc.GetControlledEntity());
  return body && !body.GetCharacterController().IsDead() && body == GetLeader();
 }

 void Release()
 {
  if (!m_Commander) return;
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  SCR_PlayerControllerGroupComponent component = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(m_Commander);
  SCR_AIGroup previous = manager.FindGroup(m_PreviousGroup);
  if (component && m_CommandGroup && component.GetGroupID() == m_CommandGroup.GetGroupID())
  {
   if (!previous && GetFaction()) previous = manager.CreateNewPlayableGroup(GetFaction(), SCR_EGroupRole.ASSAULT);
   if (previous) component.RPC_AskJoinGroup(previous.GetGroupID());
  }
  if (m_DetachedLeader)
  {
   if (m_CommandGroup) m_CommandGroup.RemoveAgent(m_DetachedLeader);
   if (m_Group) m_Group.AddAgent(m_DetachedLeader);
   m_DetachedLeader = null;
  }
  if (previous) previous.SetCanDeleteIfNoPlayer(m_PreviousDeleteIfEmpty);
  PrintFormat("[ACR] %1: native command released player=%2", m_Name, m_Commander);
  m_Commander = 0;
  m_PreviousGroup = -1;
 }

 void UpdateFormationLeader()
 {
  // A player-held AI leader cannot navigate the group's formation to a waypoint.
  // Keep the RTS commander's identity, but let an AI lead formation movement.
  if (!m_Group) return;
  array<AIAgent> agents = {};
  m_Group.GetAgents(agents);
  AIAgent next;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || body.GetCharacterController().IsDead() || FindController(body)) continue;
   if (!next || body == GetLeader()) next = agent;
  }
  if (next && next != m_Group.GetLeaderAgent()) m_Group.SetNewLeader(next);
 }

 // Grant or revoke native command authority to match who controls the leader.
 // Reforger disables AI far from every player ("max LOD"). Arma Commander
 // battles happen anywhere on the map, so commanded squads stay fully simulated.
 static void KeepSimulated(AIGroup group)
 {
  if (!group) return;
  WakeUp(group);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) WakeUp(agent);
 }

 // PreventMaxLOD doesn't pull an agent that is already disabled back out, so
 // step it down one LOD first.
 static void WakeUp(AIAgent agent)
 {
  int maxLOD = AIAgent.GetMaxLOD();
  if (agent.GetLOD() >= maxLOD) agent.SetLOD(maxLOD - 1);
  agent.PreventMaxLOD();
 }

 void Refresh()
 {
  KeepSimulated(m_Group);
  if (!m_Group || !m_CommandGroup) return;
  UpdateFormationLeader();
  int leaderPlayer;
  IEntity leader = GetLeader();
  if (leader) leaderPlayer = FindController(leader);
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(leaderPlayer));
  if (!IsLeader(pc)) leaderPlayer = 0;
  if (m_Commander == leaderPlayer) return;
  Release();
  if (!leaderPlayer) return;
  SCR_PlayerControllerGroupComponent component = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(leaderPlayer);
  if (!component) return;
  m_PreviousGroup = component.GetGroupID();
  SCR_AIGroup previous = SCR_GroupsManagerComponent.GetInstance().FindGroup(m_PreviousGroup);
  if (previous)
  {
   m_PreviousDeleteIfEmpty = previous.GetDeleteIfNoPlayer();
   previous.SetCanDeleteIfNoPlayer(false);
  }
  component.RPC_AskJoinGroup(m_CommandGroup.GetGroupID());
  if (component.GetGroupID() != m_CommandGroup.GetGroupID())
  {
   if (previous) previous.SetCanDeleteIfNoPlayer(m_PreviousDeleteIfEmpty);
   return;
  }
  AIControlComponent aiControl = AIControlComponent.Cast(leader.FindComponent(AIControlComponent));
  if (aiControl)
  {
   m_DetachedLeader = aiControl.GetAIAgent();
   if (m_DetachedLeader)
   {
    m_Group.RemoveAgent(m_DetachedLeader);
    m_CommandGroup.AddAgent(m_DetachedLeader);
   }
  }
  m_CommandGroup.SetGroupLeader(leaderPlayer);
  m_CommandGroup.SetCustomName(m_Name, 0);
  m_Commander = leaderPlayer;
  PrintFormat("[ACR] %1: native command granted player=%2 group=%3 linked=%4", m_Name, leaderPlayer, component.GetGroupID(), m_CommandGroup.GetSlave() == m_Group);
 }

 // ---------------------------------------------------------------- orders

 static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
 static const ResourceName WP_DEFEND = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
 static const ResourceName WP_GET_IN_NEAREST = "{B049D4C74FBC0C4D}Prefabs/AI/Waypoints/AIWaypoint_GetInNearest.et";
 static const ResourceName WP_GET_IN = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";
 static const ResourceName WP_GET_OUT = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";

 ACR_EMoveMode m_Mode = ACR_EMoveMode.COMBAT;
 string m_Order = "Holding";
 int m_OrderKind;   // for map symbology: 0 move, 1 defend, 2 get in, 3 get out

 static string ModeName(ACR_EMoveMode mode)
 {
  if (mode == ACR_EMoveMode.FAST) return "Fast";
  if (mode == ACR_EMoveMode.SILENT) return "Silent";
  return "Combat";
 }

 // Movement behaviour, like Arma Commander's Combat / Transport / Silent:
 //  Combat: normal pace, engage at will, orders can be interrupted by contact.
 //  Fast:   run, orders take priority over contact (like a forced move).
 //  Silent: walk, only return fire.
 void SetMode(ACR_EMoveMode mode)
 {
  m_Mode = mode;
  if (!m_Group) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility)
  {
   if (mode == ACR_EMoveMode.SILENT) utility.SetCombatMode(EAIGroupCombatMode.RETURN_FIRE);
   else utility.SetCombatMode(EAIGroupCombatMode.FIRE_AT_WILL);
  }
  SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(m_Group.FindComponent(SCR_AIGroupSettingsComponent));
  if (settings)
  {
   settings.RemoveSettingsOfTypeAndOrigin(SCR_AIGroupCharactersMovementSpeedSettingBase, SCR_EAISettingOrigin.COMMANDING);
   if (mode == ACR_EMoveMode.FAST)
    settings.AddSetting(SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, EMovementType.RUN), false, true);
   if (mode == ACR_EMoveMode.SILENT)
    settings.AddSetting(SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.COMMANDING, EMovementType.WALK), false, true);
  }
 }

 // Replace every queued order (ours or native) with a single waypoint.
 AIWaypoint IssueWaypoint(ResourceName prefab, vector position, float completionRadius = -1, bool forced = false)
 {
  if (!m_Group) return null;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = position;
  AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, params));
  if (!waypoint) return null;
  SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(waypoint);
  if (scripted && (forced || m_Mode == ACR_EMoveMode.FAST)) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  if (completionRadius >= 0) waypoint.SetCompletionRadius(completionRadius);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  m_Waypoint = waypoint;
  m_Group.AddWaypoint(waypoint);
  return waypoint;
 }

 bool MoveTo(vector destination)
 {
  if (!IssueWaypoint(WP_MOVE, destination)) return false;
  m_Order = "Moving";
  m_OrderKind = 0;
  return true;
 }

 bool Defend(vector position)
 {
  if (!IssueWaypoint(WP_DEFEND, position, 15, true)) return false;
  m_Order = "Defending";
  m_OrderKind = 1;
  return true;
 }

 // Board a specific vehicle if one is within 30 m of the point, otherwise the nearest free one.
 bool GetIn(vector position)
 {
  Vehicle vehicle = FindVehicleNear(position, 30);
  if (vehicle)
  {
   SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(IssueWaypoint(WP_GET_IN, vehicle.GetOrigin(), -1, true));
   if (!boarding) return false;
   boarding.SetEntity(vehicle);
  }
  else if (!IssueWaypoint(WP_GET_IN_NEAREST, position, 20, true)) return false;
  m_Order = "Getting in";
  m_OrderKind = 2;
  return true;
 }

 bool GetOut()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return false;
  if (!IssueWaypoint(WP_GET_OUT, vehicles[0].GetOrigin(), -1, true)) return false;
  m_Order = "Getting out";
  m_OrderKind = 3;
  return true;
 }

 // Vehicles any member currently sits in.
 void GetVehicles(array<IEntity> vehicles)
 {
  vehicles.Clear();
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle()) continue;
   CompartmentAccessComponent compartment = character.GetCompartmentAccessComponent();
   if (!compartment) continue;
   IEntity vehicle = compartment.GetVehicleIn(character);
   if (vehicle && !vehicles.Contains(vehicle)) vehicles.Insert(vehicle);
  }
 }

 protected Vehicle m_FoundVehicle;
 protected bool CollectVehicle(IEntity entity)
 {
  Vehicle vehicle = Vehicle.Cast(entity);
  if (!vehicle) return true;
  m_FoundVehicle = vehicle;
  return false; // stop at the first hit
 }
 Vehicle FindVehicleNear(vector position, float radius)
 {
  m_FoundVehicle = null;
  GetGame().GetWorld().QueryEntitiesBySphere(position, radius, CollectVehicle);
  return m_FoundVehicle;
 }

 // Where the squad is currently headed (any waypoint, including native orders).
 bool GetOrderTarget(out vector target, out int kind)
 {
  if (!m_Group) return false;
  AIWaypoint current = m_Group.GetCurrentWaypoint();
  if (!current) return false;
  target = current.GetOrigin();
  kind = 0;
  if (current == m_Waypoint) kind = m_OrderKind;
  return true;
 }

 // One line for the command panel, e.g. "4/4 alive | Combat | Moving".
 string Describe()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  int alive;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  string order = m_Order;
  if (m_Group && !m_Group.GetCurrentWaypoint() && order != "Defending") order = "Holding";
  string text = string.Format("%1 alive | %2 | %3", alive, ModeName(m_Mode), order);
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (!vehicles.IsEmpty()) text += " | mounted";
  return text;
 }

 // ---------------------------------------------------------------- teardown

 void Dispose()
 {
  Release();
  GetGame().GetCallqueue().Remove(LinkSquad);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  if (manager && m_CommandGroup) manager.DeleteAndUnregisterGroup(m_CommandGroup);
  if (m_Group && m_Group.GetAgentsCount() == 0) SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
  m_CommandGroup = null;
 }

 void ~ACR_Squad()
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(LinkSquad);
 }
}
