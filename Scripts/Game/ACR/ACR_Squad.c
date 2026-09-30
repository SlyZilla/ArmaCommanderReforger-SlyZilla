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

 void Rename(string name)
 {
  m_Name = name;
  if (m_Group) m_Group.SetCustomName(name, 0);
  if (m_CommandGroup) m_CommandGroup.SetCustomName(name, 0);
 }

 // Highest number of living soldiers seen per prefab: what Reinforce refills to.
 protected ref map<ResourceName, int> m_FullStrength = new map<ResourceName, int>();

 protected void CountByPrefab(map<ResourceName, int> counts)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!soldier || soldier.GetCharacterController().IsDead() || !soldier.GetPrefabData()) continue;
   ResourceName prefab = soldier.GetPrefabData().GetPrefabName();
   int count;
   counts.Find(prefab, count);
   counts.Set(prefab, count + 1);
  }
 }

 void TrackStrength()
 {
  map<ResourceName, int> now = new map<ResourceName, int>();
  CountByPrefab(now);
  foreach (ResourceName prefab, int count : now)
  {
   int best;
   m_FullStrength.Find(prefab, best);
   if (count > best) m_FullStrength.Set(prefab, count);
  }
 }

 // Prefabs of fallen soldiers, one entry per missing man.
 void GetMissing(array<ResourceName> missing)
 {
  missing.Clear();
  map<ResourceName, int> now = new map<ResourceName, int>();
  CountByPrefab(now);
  foreach (ResourceName prefab, int full : m_FullStrength)
  {
   int alive;
   now.Find(prefab, alive);
   for (int i = alive; i < full; i++) missing.Insert(prefab);
  }
 }

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

 // Reforger lowers AI detail with distance from players, and far AI stop
 // perceiving and fighting. Pin commanded AI and garrisons at full detail.
 static void WakeUp(AIAgent agent)
 {
  if (agent.GetPermanentLOD() != 0) agent.SetPermanentLOD(0);
  agent.PreventMaxLOD();
 }

 void Refresh()
 {
  KeepSimulated(m_Group);
  TrackStrength();
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
  // Drop our standing map order, or the squad keeps doing it and the player's
  // command-wheel orders queue up behind it.
  Halt("Following you");
  PrintFormat("[ACR] %1: native command granted player=%2 group=%3 linked=%4", m_Name, leaderPlayer, component.GetGroupID(), m_CommandGroup.GetSlave() == m_Group);
 }

 // ---------------------------------------------------------------- orders

 static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
 static const ResourceName WP_DEFEND = "{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et";
 static const ResourceName WP_GET_IN_NEAREST = "{B049D4C74FBC0C4D}Prefabs/AI/Waypoints/AIWaypoint_GetInNearest.et";
 static const ResourceName WP_GET_IN = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";
 static const ResourceName WP_GET_OUT = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
 static const ResourceName WP_SEARCH_DESTROY = "{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et";
 static const ResourceName WP_SUPPRESS = "{ED8277F35B46B4AA}Prefabs/AI/Waypoints/AIWaypoint_Suppress.et";

 vector m_MoveTarget;   // where the current order points
 bool m_bForced;        // current order outranks combat (Fast)
 ResourceName m_sOrderPrefab;
 int m_iOrderSerial;     // bumped by every new order from a player or the AI commander (not by internal re-issues)
 float m_fOrderRadius = -1;
 float m_fContactLogged;

 ACR_EMoveMode m_Mode = ACR_EMoveMode.COMBAT;
 string m_Order = "Holding";
 // AI commander bookkeeping
 int m_iAITask;          // ACR_EAITask
 ACR_Base m_AITarget;
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
  m_bForced = forced || m_Mode == ACR_EMoveMode.FAST;
  m_MoveTarget = position;
  m_sOrderPrefab = prefab;
  m_fOrderRadius = completionRadius;
  if (scripted && m_bForced) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  if (completionRadius >= 0) waypoint.SetCompletionRadius(completionRadius);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  m_Waypoint = waypoint;
  m_Group.AddWaypoint(waypoint);
  return waypoint;
 }

 // Drop every queued order and stand still (used while a gunner drives).
 void Halt(string label)
 {
  if (!m_Group) return;
  ClearOrders();
  m_Order = label;
  m_iOrderSerial++;
  m_aRoute.Clear();
 }

 protected void ClearOrders()
 {
  if (!m_Group) return;
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  m_Waypoint = null;
 }

 bool MoveTo(vector destination)
 {
  m_iOrderSerial++;
  m_aRoute.Clear();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_MOVE, destination)) return false;
  m_Order = "Moving";
  m_OrderKind = 0;
  return true;
 }

 bool Defend(vector position)
 {
  m_iOrderSerial++;
  m_aRoute.Clear();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_DEFEND, position, 15)) return false;
  m_Order = "Defending";
  m_OrderKind = 1;
  return true;
 }

 // Clear an objective: move in and fight through it (search and destroy).
 bool Assault(vector position)
 {
  m_iOrderSerial++;
  m_aRoute.Clear();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SEARCH_DESTROY, position, 30)) return false;
  m_Order = "Assaulting";
  m_OrderKind = 0;
  return true;
 }

 // Put fire on an area from where the squad stands (support by fire).
 bool Suppress(vector position)
 {
  m_iOrderSerial++;
  m_aRoute.Clear();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SUPPRESS, position)) return false;
  m_Order = "Suppressing";
  m_OrderKind = 0;
  return true;
 }

 // Living enemies this squad knows about within radius of it.
 int KnownEnemies(float radius)
 {
  if (!m_Group) return 0;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  IEntity anchor = GetAnchor();
  if (!utility || !utility.m_Perception || !anchor) return 0;
  int count;
  foreach (IEntity enemy : utility.m_Perception.m_aTargetEntities)
  {
   if (!ACR_Respawn.IsAlive(enemy)) continue;
   if (vector.DistanceXZ(enemy.GetOrigin(), anchor.GetOrigin()) <= radius) count++;
  }
  return count;
 }

 bool IsMounted()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle()) return true;
  }
  return false;
 }

 float StrengthRatio()
 {
  int full;
  foreach (ResourceName prefab, int count : m_FullStrength) full += count;
  if (full <= 0) return 1;
  return CountAlive() / (float)full;
 }

 // Small-unit sense, every second for squads no player is leading:
 //  - a Fast (forced) move stops ignoring the enemy once the squad makes contact;
 //  - a squad that reaches its destination takes up positions instead of standing around;
 //  - soldiers get the mission's AI skill.
 void Tactics(int skill, float now)
 {
  if (!m_Group || m_Commander != 0) return;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(body.FindComponent(SCR_AICombatComponent));
   if (combat && combat.GetAISkill() != skill) combat.SetAISkill(skill);
   // Faster target identification: less standing and staring at a player
   // while deciding whether he's an enemy.
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (combat && manager && combat.GetPerceptionFactor() != manager.m_fAIPerception) combat.SetPerceptionFactor(manager.m_fAIPerception);
  }
  // A player is leading (or riding with) this squad: they give the orders, e.g.
  // with the command wheel. Re-issuing our last map order would wipe theirs.
  if (HasPlayer()) return;
  bool contact = KnownEnemies(350) > 0;
  FollowRoute();
  Cohesion(now, contact);
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY;
  if (m_bForced && movingOrder && contact && !IsMounted() && !m_bRegrouping)
  {
   string order = m_Order;
   ACR_EMoveMode mode = m_Mode;
   m_Mode = ACR_EMoveMode.COMBAT;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_Mode = mode;
   m_Order = order;
   PrintFormat("[ACR] TACTICS %1: contact, fighting its way forward", m_Name);
  }
  if (m_Order == "Moving" && m_OrderKind == 0 && !IsMounted())
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   IEntity anchor = GetAnchor();
   if (waypoints.IsEmpty() && anchor && vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget) < 60)
   {
    IssueWaypoint(WP_DEFEND, m_MoveTarget, 20);
    m_Order = "Holding position";
    m_OrderKind = 1;
    PrintFormat("[ACR] TACTICS %1: arrived, taking positions", m_Name);
   }
  }
  DriveWatch(now, contact);
 }

 // ---------------------------------------------------------------- land routes
 // An order whose straight line crosses open water is walked leg by leg along a
 // land route instead (see ACR_Terrain), so squads go round a bay, not across it.
 protected ref array<vector> m_aRoute = {};
 protected ResourceName m_sRoutePrefab;
 protected float m_fRouteRadius;
 protected string m_sRouteOrder;
 protected int m_iRouteKind;

 bool RouteTo(vector destination, ResourceName prefab, float radius, string order, int kind)
 {
  IEntity anchor = GetAnchor();
  array<vector> legs = {};
  if (!anchor || IsMounted() || !ACR_Terrain.LandRoute(anchor.GetOrigin(), destination, legs))
  {
   if (prefab == WP_DEFEND) return Defend(destination);
   if (prefab == WP_SEARCH_DESTROY) return Assault(destination);
   return MoveTo(destination);
  }
  m_iOrderSerial++;
  m_bRegrouping = false;
  m_aRoute.Clear();
  foreach (vector leg : legs) m_aRoute.Insert(leg);
  m_sRoutePrefab = prefab;
  m_fRouteRadius = radius;
  m_sRouteOrder = order;
  m_iRouteKind = kind;
  PrintFormat("[ACR] ROUTE %1: straight line crosses water, going round in %2 legs", m_Name, legs.Count());
  NextLeg();
  return true;
 }

 protected void NextLeg()
 {
  vector leg = m_aRoute[0];
  m_aRoute.RemoveOrdered(0);
  if (m_aRoute.IsEmpty()) IssueWaypoint(m_sRoutePrefab, leg, m_fRouteRadius);
  else IssueWaypoint(WP_MOVE, leg, 25);
  m_Order = m_sRouteOrder;
  m_OrderKind = m_iRouteKind;
 }

 protected void FollowRoute()
 {
  if (m_aRoute.IsEmpty() || m_bRegrouping || !m_Group) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetLeader();
  if (waypoints.IsEmpty() || (leader && vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < 30)) NextLeg();
 }

 // ---------------------------------------------------------------- cohesion
 // On a long move the leader keeps his pace while the rest string out behind
 // him. When a third of the squad is more than 60 m off, the squad stops (no
 // orders: the game's formation pulls everyone back to the leader) until they
 // are within 25 m, then carries on. Not in contact, and at most once a minute.
 static const float STRAGGLER_DISTANCE = 60;
 static const float REGROUPED_DISTANCE = 25;
 static const float REGROUP_TIMEOUT_MS = 45000;
 static const float REGROUP_COOLDOWN_MS = 60000;

 protected bool m_bRegrouping;
 protected float m_fRegroupSince;
 protected float m_fNextRegroup;
 protected ResourceName m_sRegroupPrefab;
 protected vector m_RegroupTarget;
 protected float m_fRegroupRadius;
 protected string m_sRegroupOrder;
 protected int m_iRegroupKind;

 // Living members on foot farther than this from the leader.
 int Stragglers(float radius)
 {
  IEntity leader = GetLeader();
  if (!leader) return 0;
  int count;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || body == leader || !ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), leader.GetOrigin()) > radius) count++;
  }
  return count;
 }

 protected void Cohesion(float now, bool contact)
 {
  if (m_bRegrouping)
  {
   bool closed = Stragglers(REGROUPED_DISTANCE) == 0;
   if (!closed && !contact && now - m_fRegroupSince < REGROUP_TIMEOUT_MS) return;
   m_bRegrouping = false;
   m_fNextRegroup = now + REGROUP_COOLDOWN_MS;
   IssueWaypoint(m_sRegroupPrefab, m_RegroupTarget, m_fRegroupRadius);
   m_Order = m_sRegroupOrder;
   m_OrderKind = m_iRegroupKind;
   PrintFormat("[ACR] TACTICS %1: regrouped (%2 s), moving on", m_Name, (int)((now - m_fRegroupSince) / 1000));
   return;
  }
  if (contact || now < m_fNextRegroup || !m_Group || HasPlayer() || IsMounted()) return;
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY;
  if (!movingOrder) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetLeader();
  if (waypoints.IsEmpty() || !leader || vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < 200) return;
  int alive = CountAlive();
  if (alive < 3) return;
  int behind = Stragglers(STRAGGLER_DISTANCE);
  if (behind * 3 < alive) return;
  m_sRegroupPrefab = m_sOrderPrefab;
  m_RegroupTarget = m_MoveTarget;
  m_fRegroupRadius = m_fOrderRadius;
  m_sRegroupOrder = m_Order;
  m_iRegroupKind = m_OrderKind;
  ClearOrders();
  m_Order = "Regrouping";
  m_bRegrouping = true;
  m_fRegroupSince = now;
  PrintFormat("[ACR] TACTICS %1: %2 of %3 strung out, regrouping", m_Name, behind, alive);
 }

 // ---------------------------------------------------------------- stalled vehicles
 // The game's AI drivers get stuck on obstacles (and sit there honking), or stop
 // dead under fire. A driven vehicle with somewhere to go that hasn't moved for a
 // while is backed up and turned (scripted, like "gunner drives") and sent on its
 // way again; after two tries, or at once when it is under fire with no working
 // gun, the crew bails out and carries on (or takes cover) on foot.
 static const float STALL_SECONDS = 15;
 static const float STALL_SECONDS_FIGHTING = 40; // armed and in contact: the gunner may be busy, give it longer
 static const float STALL_SECONDS_HELPLESS = 6;  // under fire with no gunner
 static const int UNSTICK_TRIES = 2;
 static const float UNSTICK_REVERSE_MS = 3500;    // reversing, then a short brake
 static const float UNSTICK_TOTAL_MS = 4300;

 protected IEntity m_StallVehicle;
 protected vector m_StallPos;
 protected float m_fStallSince;
 protected int m_iUnsticks;
 protected bool m_bUnsticking;
 protected float m_fUnstickStart;
 protected float m_fUnstickSteer;
 protected ChimeraCharacter m_UnstickDriver;
 protected bool m_bResumeOnFoot;
 int UnstickTries() { return m_iUnsticks; }
 bool IsUnsticking() { return m_bUnsticking; }

 static ChimeraCharacter DriverOf(IEntity vehicle)
 {
  if (!vehicle) return null;
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetPilotCompartmentSlot()) return null;
  return ChimeraCharacter.Cast(car.GetPilotCompartmentSlot().GetOccupant());
 }

 protected bool IsMember(IEntity body)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() == body) return true;
  return false;
 }

 // A living member of this squad is manning one of the vehicle's guns.
 bool HasGunner(IEntity vehicle)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   if (ACR_Transport.VehicleOf(body) == vehicle && ACR_Transport.SeatOf(body) == ECompartmentType.TURRET) return true;
  }
  return false;
 }

 static bool HasTurret(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return false;
  array<BaseCompartmentSlot> turrets = {};
  compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  return !turrets.IsEmpty();
 }

 // Can fight: infantry of two or more, or a vehicle crew with a manned gun.
 // A lone driver or an unarmed truck crew is not sent to take or hold ground.
 bool IsCombatUnit()
 {
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle) return HasGunner(vehicle);
  return CountAlive() >= 2;
 }

 // Suitability as a base of fire (support-role teams are not).
 int FirePower()
 {
  string name = m_Name;
  name.ToLower();
  if (name.Contains("logistic") || name.Contains("medic") || name.Contains("engineer") || name.Contains("supply") || name.Contains("sniper"))
   return 0;
  int value = CountAlive();
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (vehicle && HasGunner(vehicle)) value += 6;
  return value;
 }

 protected void DriveWatch(float now, bool contact)
 {
  if (m_bUnsticking || !m_Group) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  ChimeraCharacter driver = DriverOf(vehicle);
  ACR_Manager manager = ACR_Manager.GetInstance();
  bool eligible = driver != null && ACR_Respawn.IsAlive(driver) && IsMember(driver) && FindController(driver) == 0;
  if (eligible && manager && manager.IsGunnerDriven(vehicle)) eligible = false;
  bool wantsToMove;
  if (eligible)
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY;
   wantsToMove = !waypoints.IsEmpty() && movingOrder && vector.DistanceXZ(vehicle.GetOrigin(), m_MoveTarget) > 40;
  }
  if (!wantsToMove || vehicle != m_StallVehicle || vector.DistanceXZ(vehicle.GetOrigin(), m_StallPos) > 5)
  {
   if (vehicle != m_StallVehicle || !wantsToMove) m_iUnsticks = 0;
   m_StallVehicle = vehicle;
   if (vehicle) m_StallPos = vehicle.GetOrigin();
   m_fStallSince = now;
   return;
  }
  float stalled = (now - m_fStallSince) / 1000;
  bool armed = HasGunner(vehicle);
  // Fighting vehicles (anything with a turret: LAV, BTR, BRDM, gun trucks) keep
  // their crew aboard; only soft, unarmed vehicles are abandoned.
  bool fightingVehicle = HasTurret(vehicle);
  bool underFire = contact && KnownEnemies(300) > 0;
  if (underFire && !fightingVehicle && stalled >= STALL_SECONDS_HELPLESS) { Bail(vehicle, "stopped under fire, unarmed", false); return; }
  float limit = STALL_SECONDS;
  if (underFire || armed) limit = STALL_SECONDS_FIGHTING;
  if (stalled < limit) return;
  if (m_iUnsticks >= UNSTICK_TRIES)
  {
   if (fightingVehicle)
   {
    // Hold where it is and fight from there instead of walking away from the vehicle.
    Halt("Stuck, holding");
    m_iUnsticks = 0;
    PrintFormat("[ACR] DRIVE %1: stuck, holding position (armed vehicle)", m_Name);
    return;
   }
   Bail(vehicle, "stuck", !underFire);
   return;
  }
  m_iUnsticks++;
  BeginUnstick(vehicle, driver);
 }

 protected void BeginUnstick(IEntity vehicle, ChimeraCharacter driver)
 {
  m_bUnsticking = true;
  m_UnstickDriver = driver;
  m_fUnstickStart = GetGame().GetWorld().GetWorldTime();
  m_fUnstickSteer = 0.8;
  if (m_iUnsticks % 2 == 0) m_fUnstickSteer = -0.8;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
  PrintFormat("[ACR] DRIVE %1: stuck, backing up (try %2)", m_Name, m_iUnsticks);
  // Applied every frame (like gunner drive): the controller drops inputs set less often.
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && !manager.m_Unsticking.Contains(this)) manager.m_Unsticking.Insert(this);
 }

 // Returns false when finished.
 bool UnstickFrame()
 {
  if (!m_bUnsticking) return false;
  float elapsed = GetGame().GetWorld().GetWorldTime() - m_fUnstickStart;
  IEntity vehicle = m_StallVehicle;
  bool alive = vehicle != null && m_UnstickDriver != null && ACR_Respawn.IsAlive(m_UnstickDriver) && ACR_Transport.VehicleOf(m_UnstickDriver) == vehicle;
  CarControllerComponent car;
  if (alive) car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetSimulation() || elapsed > UNSTICK_TOTAL_MS)
  {
   EndUnstick(car);
   return false;
  }
  VehicleWheeledSimulation sim = car.GetSimulation();
  Physics physics = vehicle.GetPhysics();
  if (physics && !physics.IsActive()) physics.SetActive(ActiveState.ACTIVE);
  if (!car.IsEngineOn()) car.ForceStartEngine();
  car.SetPersistentHandBrake(false);
  int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
  sim.SetClutch(1);
  if (elapsed <= UNSTICK_REVERSE_MS)
  {
   if (sim.GetGear() >= neutral) sim.SetGear(neutral - 1);
   sim.SetBreak(0, false);
   sim.SetThrottle(0.7);
   sim.SetSteering(m_fUnstickSteer);
  }
  else
  {
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(1, false);
  }
  return true;
 }

 protected void EndUnstick(CarControllerComponent car)
 {
  if (car && car.GetSimulation())
  {
   VehicleWheeledSimulation sim = car.GetSimulation();
   sim.SetThrottle(0);
   sim.SetSteering(0);
   sim.SetBreak(0, false);
   int neutral = sim.GearboxGearsCount() - sim.GearboxForwardGearsCount() - 1;
   sim.SetGear(neutral + 1);
  }
  if (m_UnstickDriver && ACR_Respawn.IsAlive(m_UnstickDriver) && !FindController(m_UnstickDriver))
  {
   AIControlComponent ai = AIControlComponent.Cast(m_UnstickDriver.FindComponent(AIControlComponent));
   if (ai && !ai.IsAIActivated()) ai.ActivateAI();
  }
  m_bUnsticking = false;
  if (m_StallVehicle) m_StallPos = m_StallVehicle.GetOrigin();
  m_fStallSince = GetGame().GetWorld().GetWorldTime();
  // Fresh order, so the driver plans a new route from here.
  if (m_Group && !m_sOrderPrefab.IsEmpty())
  {
   string order = m_Order;
   int kind = m_OrderKind;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_Order = order;
   m_OrderKind = kind;
  }
 }

 // Everyone out of the vehicle, passengers included. With resume the crew then
 // carries on to its destination on foot; otherwise it takes cover where it is.
 protected void Bail(IEntity vehicle, string why, bool resume)
 {
  PrintFormat("[ACR] DRIVE %1: %2, crew bails out", m_Name, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   array<ACR_Squad> riders = {};
   ACR_Transport.PassengerSquads(manager, vehicle, riders);
   foreach (ACR_Squad rider : riders) if (rider != this && !rider.HasPlayer()) rider.DismountNow();
  }
  // Anyone whose AI was parked (backing up, gunner drive) must be able to walk away.
  array<AIAgent> crew = {};
  GetAgents(crew);
  foreach (AIAgent member : crew)
  {
   IEntity body = member.GetControlledEntity();
   if (!body || FindController(body) != 0) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) control.ActivateAI();
  }
  ResourceName prefab = m_sOrderPrefab;
  if (!resume) prefab = "";
  DismountAndContinue(prefab, m_MoveTarget, m_fOrderRadius, m_Order, m_OrderKind);
  m_StallVehicle = null;
  m_iUnsticks = 0;
 }

 // Everyone out now; then carry out this order on foot (or, with no prefab,
 // take positions where they got out).
 void DismountAndContinue(ResourceName prefab, vector target, float radius, string order, int kind)
 {
  DismountNow();
  ContinueAfterDismount(prefab, target, radius, order, kind);
 }

 // For a squad already getting out: what to do once everyone is on the ground.
 void ContinueAfterDismount(ResourceName prefab, vector target, float radius, string order, int kind)
 {
  m_bResumeOnFoot = !prefab.IsEmpty();
  m_sResumePrefab = prefab;
  m_ResumeTarget = target;
  m_fResumeRadius = radius;
  m_sResumeOrder = order;
  m_iResumeKind = kind;
 }
 protected ResourceName m_sResumePrefab;
 protected vector m_ResumeTarget;
 protected float m_fResumeRadius;
 protected string m_sResumeOrder;
 protected int m_iResumeKind;

 // ---------------------------------------------------------------- transport
 vector m_LZ;
 bool m_bHasLZ;
 float m_fLZSetAt;

 // Everyone out now, however the vehicle is sitting (hover, roof, water):
 // the normal dismount first, a teleport-out if that is refused.
 int DismountNow()
 {
  int dropped;
  array<IEntity> leaving = {};
  GetVehicles(leaving);
  ForgetVehicles(leaving);
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !ACR_Respawn.IsAlive(character)) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access) continue;
   // Still climbing in: stop, so nobody is left behind on board.
   if (!character.IsInVehicle())
   {
    if (access.IsGettingIn()) access.InterruptVehicleActionQueue(true, true, true);
    continue;
   }
   if (!access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
    access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   dropped++;
  }
  m_bHasLZ = false;
  Halt("Dismounting");
  m_iDismountSweeps = 0;
  GetGame().GetCallqueue().CallLater(HoldAfterDismount, 3000, false);
  return dropped;
 }

 // Put members near the vehicle into its free passenger seats. The game's AI
 // won't board a vehicle another group is driving, so this is done directly:
 // climbing in normally when close, or placed in once they've had time to walk up.
 int BoardCargo(IEntity vehicle, float reach, bool placeFar)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.CARGO);
  int started;
  array<BaseCompartmentSlot> taken = {};
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !ACR_Respawn.IsAlive(character) || character.IsInVehicle()) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   float distance = vector.DistanceXZ(character.GetOrigin(), vehicle.GetOrigin());
   if (!access || access.IsGettingIn()) continue;
   bool teleport = distance > 4;
   if (distance > reach || (teleport && !placeFar)) continue;
   foreach (BaseCompartmentSlot seat : seats)
   {
    if (seat.GetOccupant() || taken.Contains(seat) || (seat.IsReserved() && !seat.IsReservedBy(character))) continue;
    taken.Insert(seat);
    bool ok = access.GetInVehicle(vehicle, seat, teleport, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    if (System.IsCLIParam("acrRideTest")) PrintFormat("[ACR] BOARD %1 m, seat %2, placed %3 -> %4", (int)distance, seat.GetCompartmentName(), teleport, ok);
    if (ok)
    {
     started++;
     break;
    }
   }
  }
  return started;
 }

 // The game's AI treats vehicles it has used as its own and climbs back in;
 // once we've ordered a squad out, it should stay out.
 void ForgetVehicles(array<IEntity> vehicles)
 {
  if (!m_Group || vehicles.IsEmpty()) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return;
  foreach (IEntity vehicle : vehicles)
  {
   SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(vehicle.FindComponent(SCR_AIVehicleUsageComponent));
   if (usage && utility.IsUsableVehicle(usage)) utility.RemoveUsableVehicle(usage);
  }
 }

 protected int m_iDismountSweeps;

 // Once out, take positions where they landed (sweeping out any late boarder first).
 void HoldAfterDismount()
 {
  if (IsMounted() && m_iDismountSweeps < 3)
  {
   m_iDismountSweeps++;
   array<AIAgent> agents = {};
   GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!character || !character.IsInVehicle() || !character.GetCompartmentAccessComponent()) continue;
    CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
    if (!access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
     access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   }
   GetGame().GetCallqueue().CallLater(HoldAfterDismount, 3000, false);
   return;
  }
  IEntity anchor = GetAnchor();
  if (!m_Group || !anchor) return;
  if (m_bResumeOnFoot)
  {
   m_bResumeOnFoot = false;
   IssueWaypoint(m_sResumePrefab, m_ResumeTarget, m_fResumeRadius);
   m_Order = m_sResumeOrder;
   m_OrderKind = m_iResumeKind;
   return;
  }
  IssueWaypoint(WP_DEFEND, anchor.GetOrigin(), 20);
  m_Order = "Holding position";
  m_OrderKind = 1;
 }

 int CountAlive()
 {
  int alive;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  return alive;
 }

 // Rough fighting value: soldiers, plus 5 per armed (manned gun) vehicle (Arma Commander's default vehicle value).
 int Strength()
 {
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  int armed;
  foreach (IEntity vehicle : vehicles) if (HasGunner(vehicle)) armed++;
  return CountAlive() + armed * 5;
 }

 bool HasPlayer()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (FindController(agent.GetControlledEntity())) return true;
  return false;
 }

 // Board a specific vehicle if one is within 30 m of the point, otherwise the nearest free one.
 bool GetIn(vector position)
 {
  return BoardVehicle(FindVehicleNear(position, 30), position);
 }

 // Board this vehicle (or, without one, the nearest free vehicle to the point).
 bool BoardVehicle(Vehicle vehicle, vector position)
 {
  m_iOrderSerial++;
  m_aRoute.Clear();
  m_bRegrouping = false;
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
  m_iOrderSerial++;
  m_aRoute.Clear();
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
  bool finished = order == "Moving" || order == "Assaulting" || order == "Suppressing" || order == "Getting in" || order == "Getting out";
  if (m_Group && !m_Group.GetCurrentWaypoint() && finished) order = "Holding";
  string text = string.Format("%1 alive | %2 | %3", alive, ModeName(m_Mode), order);
  text += " | HP " + m_iHealthPct.ToString() + "%";
  if (m_iAmmoPct >= 0) text += " | ammo " + m_iAmmoPct.ToString() + "%";
  if (m_iFuelPct >= 0 && m_iFuelPct < 30) text += " | fuel " + m_iFuelPct.ToString() + "%";
  if (m_iStockPct >= 0) text += " | " + m_sServiceName + " stock " + m_iStockPct.ToString() + "%";
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (!vehicles.IsEmpty()) text += " | mounted";
  array<ResourceName> missing = {};
  GetMissing(missing);
  if (!missing.IsEmpty()) text += string.Format(" | %1 missing", missing.Count());
  return text;
 }

 // ---------------------------------------------------------------- status
 // Refreshed every few seconds on the server for the map markers and panel:
 // health (wounds and losses against full strength, and the vehicle's state),
 // ammo (magazines against what each soldier started with; guns for crews),
 // fuel, and for service vehicles their remaining stock.
 int m_iHealthPct = 100;
 int m_iAmmoPct = -1;
 int m_iFuelPct = -1;
 int m_iStockPct = -1;
 int m_iAvgHpPct = 100;       // average health of the living (for medics)
 string m_sServiceName;
 protected ref map<IEntity, float> m_MagazineBaseline = new map<IEntity, float>();

 int MagazineTarget(IEntity soldier)
 {
  float baseline;
  if (m_MagazineBaseline.Find(soldier, baseline)) return (int)Math.Round(baseline);
  return 6;
 }

 void UpdateStatus(ACR_Logistics logistics)
 {
  int full;
  foreach (ResourceName prefab, int count : m_FullStrength) full += count;
  array<AIAgent> agents = {};
  GetAgents(agents);
  float health;
  float have;
  float want;
  int alive;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   alive++;
   health += ACR_Logistics.Health(body);
   float magazines = ACR_Logistics.SoldierMagazines(body);
   float baseline;
   if (!m_MagazineBaseline.Find(body, baseline))
   {
    baseline = Math.Max(magazines, 1);
    m_MagazineBaseline.Set(body, baseline);
   }
   have += Math.Min(magazines, baseline);
   want += baseline;
  }
  int slots = Math.Max(full, alive);
  m_iHealthPct = 0;
  if (slots > 0) m_iHealthPct = (int)(100 * health / slots);
  m_iAvgHpPct = 100;
  if (alive > 0) m_iAvgHpPct = (int)(100 * health / alive);
  m_iAmmoPct = -1;
  if (want > 0) m_iAmmoPct = (int)(100 * have / want);
  m_iFuelPct = -1;
  m_iStockPct = -1;
  m_sServiceName = "";
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle) return;
  m_iHealthPct = (int)Math.Min(m_iHealthPct, 100 * ACR_Logistics.Health(vehicle));
  float guns = ACR_Logistics.VehicleAmmo(vehicle);
  if (guns >= 0 && HasGunner(vehicle))
  {
   if (m_iAmmoPct < 0) m_iAmmoPct = (int)(100 * guns);
   else m_iAmmoPct = (int)Math.Min(m_iAmmoPct, 100 * guns);
  }
  float fuel = ACR_Logistics.Fuel(vehicle);
  if (fuel >= 0) m_iFuelPct = (int)(100 * fuel);
  if (logistics && ACR_Logistics.ServiceOf(vehicle) != 0)
  {
   m_iStockPct = logistics.StockPercent(vehicle);
   m_sServiceName = ACR_Logistics.ServiceName(ACR_Logistics.ServiceOf(vehicle));
  }
 }

 // ---------------------------------------------------------------- teardown

 void Dispose()
 {
  m_bUnsticking = false;
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
