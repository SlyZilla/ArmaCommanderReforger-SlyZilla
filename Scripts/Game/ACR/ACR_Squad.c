// What a squad's order looks like on the map (order line colour).
enum ACR_EOrderKind
{
 MOVE,
 DEFEND,
 GET_IN,
 GET_OUT,
 HUNT
}

// What a squad is doing, for the logic (m_Order is only the label shown on the
// map). NONE: a label that carries no state of its own (or one written
// directly to m_Order, which nothing does any more).
enum ACR_EOrderState
{
 NONE,
 HOLD,       // halted, dismounting, waiting...
 MOVE,       // "Moving"
 DEFEND,     // defending / holding position
 ASSAULT,
 SUPPRESS,
 GET_IN,
 GET_OUT,
 HUNT,
 REGROUP,
 RIDE,       // waiting for, boarding or riding a transport (ACR_Rides)
 RETURN,     // a transport going home after a ride
 DISMOUNT,   // everyone getting out now
 COMMANDED,  // a player commands it through the game's own command menu
 DRIVEN      // its vehicle is driven by a player in the gunner seat
}

enum ACR_EMoveMode
{
 COMBAT,
 FAST,
 SILENT
}

// A squad's order, saved to be put back later (native command set-aside,
// regroup, resume after a dismount, land/road routes).
class ACR_OrderSnapshot : Managed
{
 ResourceName m_sPrefab;
 vector m_Target;
 float m_fRadius = -1;
 string m_sLabel;
 ACR_EOrderState m_eState;
 int m_iKind;

 static ACR_OrderSnapshot Make(ResourceName prefab, vector target, float radius, string label, ACR_EOrderState state, int kind)
 {
  ACR_OrderSnapshot snapshot = new ACR_OrderSnapshot();
  snapshot.m_sPrefab = prefab;
  snapshot.m_Target = target;
  snapshot.m_fRadius = radius;
  snapshot.m_sLabel = label;
  snapshot.m_eState = state;
  snapshot.m_iKind = kind;
  return snapshot;
 }
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

 protected ref ACR_OrderSnapshot m_Aside; // map order set aside while a player commands the squad

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
  // Back to the map order it had before, unless the player left it busy with one of his own.
  if (m_Aside && m_Group)
  {
   array<AIWaypoint> current = {};
   m_Group.GetWaypoints(current);
   if (current.IsEmpty())
   {
    RestoreOrder(m_Aside);
    PrintFormat("[ACR] %1: resumes its map order (%2)", m_Name, m_Order);
   }
  }
  m_Aside = null;
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

 protected bool m_bHadPlayer;

 // A player just left one of our soldiers (F8, F7 to someone else, death): make sure the
 // crew's AI and the vehicle's own AI are back on. Live report 3 Oct: after dropping control
 // the LAV sat still and ignored orders (no log of it; this logs what it finds).
 protected void WatchPlayerLeft()
 {
  bool has = HasPlayer();
  if (m_bHadPlayer && !has) GetGame().GetCallqueue().CallLater(WakeCrew, 2000, false, "player left");
  m_bHadPlayer = has;
 }

 void WakeCrew(string why)
 {
  if (HasPlayer()) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (vehicle && manager && manager.IsGunnerDriven(vehicle)) return;
  int woke;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || FindController(body) || !ACR_Respawn.IsAlive(body)) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   if (control && !control.IsAIActivated()) { control.ActivateAI(); woke++; }
  }
  bool vehicleAIWasOff, pilotDetached;
  if (vehicle)
  {
   AIControlComponent vehicleAI = AIControlComponent.Cast(vehicle.FindComponent(ChimeraAIVehicleControlComponent));
   if (vehicleAI && !vehicleAI.IsAIActivated()) { vehicleAI.ActivateAI(); vehicleAIWasOff = true; }
   CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
   pilotDetached = car && !car.GetPilotCompartmentSlot();
  }
  array<AIWaypoint> waypoints = {};
  if (m_Group) m_Group.GetWaypoints(waypoints);
  PrintFormat("[ACR] %1: %2: woke %3 crew AI, vehicle %4 (vehicle AI was off %5, pilot seat unhooked %6), waypoints %7, order '%8'", m_Name, why, woke, vehicle != null, vehicleAIWasOff, pilotDetached, waypoints.Count(), m_Order);
  // A helicopter crew must not keep a waypoint once the player is out of it (they'd get out).
  if (Aircraft())
  {
   if (!waypoints.IsEmpty() || OrderState() != ACR_EOrderState.HOLD) Halt("Holding");
   return;
  }
  // Left "Following you" with nothing to do: plain Holding, so the panel and AI read it right.
  if (OrderState() == ACR_EOrderState.COMMANDED && waypoints.IsEmpty()) SetOrder("Holding", ACR_EOrderState.HOLD);
 }

 void Refresh()
 {
  WatchPlayerLeft();
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
  // Set our standing map order aside, or the squad keeps doing it and the
  // player's command-wheel orders queue up behind it. It resumes when he leaves.
  m_Aside = null;
  if (m_Waypoint != null && !m_sOrderPrefab.IsEmpty()) m_Aside = SnapshotOrder();
  Halt("Following you", ACR_EOrderState.COMMANDED);
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
 static const ResourceName WP_GARRISON = "{AC230926A00100A2}Prefabs/ACR/ACR_Waypoint_Garrison.et"; // see ACR_Garrison

 vector m_MoveTarget;   // where the current order points
 bool m_bForced;        // current order outranks combat (Fast)
 ResourceName m_sOrderPrefab;
 int m_iOrderSerial;     // bumped by every new order from a player or the AI commander (not by internal re-issues)
 // Who gave the current order (shared command): a player id, or 0 for the AI
 // commander and the squad's own decisions. Set by the manager after a player's order.
 int m_iOrderedBy;
 float m_fOrderedAt;
 float m_fOrderRadius = -1;
 float m_fContactLogged;

 ACR_EMoveMode m_Mode = ACR_EMoveMode.COMBAT;
 string m_Order = "Holding";   // label for the map and panel; logic uses OrderState()
 protected ACR_EOrderState m_eOrderState = ACR_EOrderState.HOLD;
 protected string m_sStateLabel = "Holding"; // the label m_eOrderState belongs to

 // Set the order label and what it means together.
 void SetOrder(string label, ACR_EOrderState state)
 {
  m_Order = label;
  m_eOrderState = state;
  m_sStateLabel = label;
 }

 // The current order, to put back later with RestoreOrder.
 ACR_OrderSnapshot SnapshotOrder()
 {
  return ACR_OrderSnapshot.Make(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius, m_Order, OrderState(), m_OrderKind);
 }

 // Issue a saved order again (at its own target, or at another point, e.g. a route leg).
 void RestoreOrder(ACR_OrderSnapshot snapshot)
 {
  RestoreOrderAt(snapshot, snapshot.m_sPrefab, snapshot.m_Target, snapshot.m_fRadius);
 }
 protected void RestoreOrderAt(ACR_OrderSnapshot snapshot, ResourceName prefab, vector target, float radius)
 {
  // A garrison put back (after a dismount, a set-aside...) needs its posts again.
  if (prefab == WP_GARRISON && m_Group && !ACR_Garrison.HasPosts(m_Group))
  {
   vector center;
   if (ACR_Garrison.Assign(m_Group, m_Group.GetLeaderAgent(), target, Math.Max(1, CountAlive()), center, radius, false) <= 0) prefab = WP_DEFEND;
  }
  IssueWaypoint(prefab, target, radius);
  SetOrder(snapshot.m_sLabel, snapshot.m_eState);
  m_OrderKind = snapshot.m_iKind;
 }

 // What the squad is doing. NONE when m_Order no longer matches the label the
 // state was set with (every order goes through SetOrder/Halt now).
 ACR_EOrderState OrderState()
 {
  if (m_Order != m_sStateLabel) return ACR_EOrderState.NONE;
  return m_eOrderState;
 }

 // For the order labels that callers pass in as text (RouteTo and the dismount
 // hand-overs, used by the AI commander and rides): the state a label stands for.
 static ACR_EOrderState StateOfLabel(string label)
 {
  if (label == "Moving") return ACR_EOrderState.MOVE;
  if (label == "Defending" || label == "Holding position" || label == "Garrisoning") return ACR_EOrderState.DEFEND;
  if (label == "Assaulting") return ACR_EOrderState.ASSAULT;
  if (label == "Suppressing") return ACR_EOrderState.SUPPRESS;
  if (label == "Getting in") return ACR_EOrderState.GET_IN;
  if (label == "Getting out") return ACR_EOrderState.GET_OUT;
  return ACR_EOrderState.NONE;
 }

 // Test runs: the state must say what the old label tests said (any mismatch
 // is printed, and counted in ACR_Perf.s_iInvariantErrors).
 void CheckOrderState()
 {
  ACR_EOrderState state = OrderState();
  string label = m_Order;
  bool oldMoving = label == "Moving";
  bool oldFinished = oldMoving || label == "Assaulting" || label == "Suppressing" || label == "Getting in" || label == "Getting out";
  if ((state == ACR_EOrderState.MOVE) == oldMoving && FinishesOnArrival(state) == oldFinished) return;
  ACR_Perf.s_iInvariantErrors++;
  PrintFormat("[ACR] INVARIANT order state %1 doesn't match label '%2' (%3)", typename.EnumToString(ACR_EOrderState, state), label, m_Name);
 }

 // An order the squad finishes by arriving (shown as "Holding" afterwards).
 static bool FinishesOnArrival(ACR_EOrderState state)
 {
  return state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.SUPPRESS || state == ACR_EOrderState.GET_IN || state == ACR_EOrderState.GET_OUT;
 }
 // AI commander bookkeeping
 int m_iAITask;          // ACR_EAITask
 IEntity m_OwnVehicle;   // the vehicle this squad came with (crews), even when dismounted
 ACR_Base m_AITarget;
 bool m_bMortar;    // a mortar team (ACR_Mortars runs its set-up, packing and fire missions)
 int m_OrderKind;   // ACR_EOrderKind, for map symbology

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
 static bool s_bGameDefend; // test (srv:crewold): crews get the game's Defend waypoint as before
 AIWaypoint IssueWaypoint(ResourceName prefab, vector position, float completionRadius = -1, bool forced = false)
 {
  if (!m_Group) return null;
  if (prefab != WP_GET_OUT && Aircraft())
  {
   PrintFormat("[ACR] %1: no waypoint for a helicopter crew (AI can't fly; they would get out)", m_Name);
   return null;
  }
  // Vehicle crews never get the game's Defend waypoint: it orders everyone in a turret out,
  // when it starts and again when it is replaced by the next order (LAV crew bailing out for
  // no reason, 2 Oct live). A plain move there holds the vehicle just the same; the order
  // keeps WP_DEFEND as its kind (m_sOrderPrefab) and its "Defending" label.
  ResourceName spawned = prefab;
  if (prefab == WP_DEFEND && !s_bGameDefend && ACR_Transport.CrewedVehicle(this)) spawned = WP_MOVE;
  Resource resource = Resource.Load(spawned);
  if (!resource.IsValid()) return null;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = position;
  AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, params));
  if (!waypoint) return null;
  return AdoptWaypoint(waypoint, prefab, position, completionRadius, forced);
 }

 // Replace every queued order with a waypoint the caller spawned (and configured) itself.
 AIWaypoint AdoptWaypoint(AIWaypoint waypoint, ResourceName prefab, vector position, float completionRadius = -1, bool forced = false)
 {
  if (!m_Group || !waypoint) return null;
  SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(waypoint);
  m_bForced = forced || m_Mode == ACR_EMoveMode.FAST;
  m_MoveTarget = position;
  m_sOrderPrefab = prefab;
  m_fOrderRadius = completionRadius;
  if (scripted && m_bForced) scripted.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  if (completionRadius >= 0) waypoint.SetCompletionRadius(completionRadius);
  if (prefab != WP_GARRISON) ACR_Garrison.Release(m_Group);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  m_Waypoint = waypoint;
  m_Group.AddWaypoint(waypoint);
  return waypoint;
 }

 // Every new order: a fresh serial, and nobody (yet) to credit it to.
 protected void NewOrder()
 {
  m_bGarrisonPending = false;
  m_iGarrisonTries = 0;
  m_iGarrisonReissues = 0;
  m_iOrderSerial++;
  m_iOrderedBy = 0;
  m_fOrderedAt = GetGame().GetWorld().GetWorldTime();
 }

 // Drop every queued order and stand still (gunner drives, the Halt button).
 void Halt(string label, ACR_EOrderState state = ACR_EOrderState.HOLD)
 {
  if (!m_Group) return;
  ClearOrders();
  SetOrder(label, state);
  NewOrder();
  ClearPlans();
 }

 protected void ClearOrders()
 {
  if (!m_Group) return;
  ACR_Garrison.Release(m_Group);
  array<AIWaypoint> previousOrders = {};
  m_Group.GetWaypoints(previousOrders);
  foreach (AIWaypoint previousOrder : previousOrders) m_Group.RemoveWaypoint(previousOrder);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  m_Waypoint = null;
 }

 bool MoveTo(vector destination)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_MOVE, destination)) return false;
  SetOrder("Moving", ACR_EOrderState.MOVE);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 bool Defend(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_DEFEND, position, 15)) return false;
  SetOrder("Defending", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Move into the building at (or next to) the point and hold its windows and doorways,
 // spilling into neighbours (ACR_Garrison). Vehicle crews and a point with no usable
 // building get a plain Defend; returns false then too, so callers can say so.
 bool Garrison(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  return PlaceGarrison(position);
 }

 // The soldier navmesh streams in tiles (about a second): a building whose tile isn't
 // loaded yet is surveyed again next second while the squad heads there; after a few
 // tries it is surveyed without the navmesh check.
 protected bool m_bGarrisonPending;
 protected vector m_GarrisonPending;
 protected int m_iGarrisonTries;
 protected int m_iGarrisonReissues;

 protected bool PlaceGarrison(vector position)
 {
  vector center;
  float radius;
  int placed;
  if (m_Group && !ACR_Transport.CrewedVehicle(this))
   placed = ACR_Garrison.Assign(m_Group, m_Group.GetLeaderAgent(), position, Math.Max(1, CountAlive()), center, radius, m_iGarrisonTries < 4);
  if (placed < 0)
  {
   m_iGarrisonTries++;
   if (!m_bGarrisonPending)
   {
    IssueWaypoint(WP_MOVE, position, 20);
    SetOrder("Garrisoning", ACR_EOrderState.DEFEND);
    m_OrderKind = ACR_EOrderKind.DEFEND;
   }
   m_bGarrisonPending = true;
   m_GarrisonPending = position;
   return true;
  }
  m_bGarrisonPending = false;
  m_iGarrisonTries = 0;
  if (placed == 0)
  {
   if (IssueWaypoint(WP_DEFEND, position, 15)) SetOrder("Defending", ACR_EOrderState.DEFEND);
   m_OrderKind = ACR_EOrderKind.DEFEND;
   return false;
  }
  if (!IssueWaypoint(WP_GARRISON, center, radius)) return false;
  SetOrder("Garrisoning", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
  return true;
 }

 // Clear an objective: move in and fight through it (search and destroy).
 bool Assault(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SEARCH_DESTROY, position, 30)) return false;
  SetOrder("Assaulting", ACR_EOrderState.ASSAULT);
  m_OrderKind = ACR_EOrderKind.MOVE;
  return true;
 }

 // Put fire on an area from where the squad stands (support by fire).
 bool Suppress(vector position)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (!IssueWaypoint(WP_SUPPRESS, position)) return false;
  SetOrder("Suppressing", ACR_EOrderState.SUPPRESS);
  m_OrderKind = ACR_EOrderKind.MOVE;
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

 // Is anyone of this squad aboard its own vehicle?
 bool InOwnVehicle()
 {
  if (!m_OwnVehicle) return false;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body) && ACR_Transport.VehicleOf(body) == m_OwnVehicle) return true;
  }
  return false;
 }

 // The vehicle that defines this squad's map symbol: its own (if still
 // intact), else whatever it is crewing.
 IEntity MarkVehicle()
 {
  if (m_OwnVehicle && ACR_Logistics.Health(m_OwnVehicle) > 0) return m_OwnVehicle;
  return ACR_Transport.CrewedVehicle(this);
 }

 // ---------------------------------------------------------------- hunt
 // Doggedly pursue one enemy group: follow its latest known position (our
 // side's intel) until it is destroyed, the trail goes cold, or we are no
 // longer fit to fight.
 protected ACR_Squad m_HuntSquad;
 protected IEntity m_HuntBody;
 protected bool m_bHunting;
 protected int m_iHuntSerial;
 protected string m_sHuntWhat;
 protected vector m_HuntPos;
 protected float m_fHuntSeen;       // world time of the latest sighting we are following
 protected float m_fNextHuntCheck;
 static const float HUNT_CHECK_MS = 8000;
 static const float HUNT_COLD_MS = 180000;   // no new sighting this long after reaching the last one: lost
 static const float HUNT_GIVE_UP_MS = 420000; // no new sighting at all this long: lost

 void Hunt(ACR_Squad target, IEntity body, vector position, string what)
 {
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  m_HuntSquad = target;
  m_HuntBody = body;
  m_sHuntWhat = what;
  m_HuntPos = position;
  m_fHuntSeen = GetGame().GetWorld().GetWorldTime();
  m_fNextHuntCheck = 0;
  m_bHunting = true;
  IssueWaypoint(WP_SEARCH_DESTROY, position, 30);
  SetOrder("Hunting " + what, ACR_EOrderState.HUNT);
  m_OrderKind = ACR_EOrderKind.HUNT;
  m_iHuntSerial = m_iOrderSerial;
 }

 bool IsHunting() { return m_bHunting; }

 protected void EndHunt(string reason)
 {
  m_bHunting = false;
  m_HuntSquad = null;
  m_HuntBody = null;
  PrintFormat("[ACR] HUNT %1: %2", m_Name, reason);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && GetFaction()) manager.NotifyFaction(GetFaction().GetFactionKey(), m_Name + ": " + reason);
  IEntity anchor = GetAnchor();
  if (anchor) Defend(anchor.GetOrigin());
 }

 protected void HuntTick(float now)
 {
  if (!m_bHunting) return;
  // Any other order ends the hunt quietly.
  if (m_iOrderSerial != m_iHuntSerial) { m_bHunting = false; m_HuntSquad = null; m_HuntBody = null; return; }
  if (now < m_fNextHuntCheck) return;
  m_fNextHuntCheck = now + HUNT_CHECK_MS;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !GetFaction()) return;
  bool gone;
  if (m_HuntSquad) gone = !manager.m_Squads.Contains(m_HuntSquad) || m_HuntSquad.CountAlive() == 0;
  else gone = !m_HuntBody || !ACR_Respawn.IsAlive(m_HuntBody);
  if (gone) { EndHunt("target destroyed, holding here"); return; }
  if (StrengthRatio() < 0.35 || !IsCombatUnit() || (m_iAmmoPct >= 0 && m_iAmmoPct < 15))
  {
   EndHunt("breaking off the hunt, no longer fit to fight");
   return;
  }
  ACR_Intel intel = manager.IntelFor(GetFaction().GetFactionKey());
  vector known;
  float seen;
  if (intel && intel.LastKnown(m_HuntSquad, m_HuntBody, known, seen) && seen > m_fHuntSeen)
  {
   m_fHuntSeen = seen;
   // Follow it when it has moved on.
   if (vector.DistanceXZ(known, m_HuntPos) > 40)
   {
    m_HuntPos = known;
    IssueWaypoint(WP_SEARCH_DESTROY, known, 30);
    SetOrder("Hunting " + m_sHuntWhat, ACR_EOrderState.HUNT);
    m_OrderKind = ACR_EOrderKind.HUNT;
   }
   return;
  }
  IEntity anchor = GetAnchor();
  bool atLastKnown = anchor && vector.DistanceXZ(anchor.GetOrigin(), m_HuntPos) < 60;
  if ((atLastKnown && now - m_fHuntSeen > HUNT_COLD_MS) || now - m_fHuntSeen > HUNT_GIVE_UP_MS)
   EndHunt("lost track of the " + m_sHuntWhat);
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
  WatchCrew();
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
  HuntTick(now);
  if (m_bGarrisonPending && m_Order == "Garrisoning") PlaceGarrison(m_GarrisonPending);
  else if (m_sOrderPrefab == WP_GARRISON && m_Order == "Garrisoning" && m_Group.GetCurrentWaypoint() == null && m_iGarrisonReissues < 2)
  {
   // The game completed the garrison waypoint (a failed group move does that): again.
   m_iGarrisonReissues++;
   PrintFormat("[ACR] GARRISON %1: the game dropped the order, issuing it again", m_Name);
   if (!ACR_Garrison.HasPosts(m_Group)) PlaceGarrison(m_MoveTarget);
   else IssueWaypoint(WP_GARRISON, m_MoveTarget, m_fOrderRadius);
  }
  bool contact = KnownEnemies(350) > 0;
  FollowRoute();
  Cohesion(now, contact);
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
  // (not after a Halt: m_Waypoint is gone then, and the old order must not come back)
  if (m_bForced && movingOrder && contact && m_Waypoint && !IsMounted() && !m_bRegrouping)
  {
   ACR_EMoveMode mode = m_Mode;
   m_Mode = ACR_EMoveMode.COMBAT;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
   m_Mode = mode;
   PrintFormat("[ACR] TACTICS %1: contact, fighting its way forward", m_Name);
  }
  if (OrderState() == ACR_EOrderState.MOVE && m_OrderKind == ACR_EOrderKind.MOVE && !IsMounted())
  {
   array<AIWaypoint> waypoints = {};
   m_Group.GetWaypoints(waypoints);
   IEntity anchor = GetAnchor();
   if (waypoints.IsEmpty() && anchor && vector.DistanceXZ(anchor.GetOrigin(), m_MoveTarget) < 60)
   {
    IssueWaypoint(WP_DEFEND, m_MoveTarget, 20);
    SetOrder("Holding position", ACR_EOrderState.DEFEND);
    m_OrderKind = ACR_EOrderKind.DEFEND;
    PrintFormat("[ACR] TACTICS %1: arrived, taking positions", m_Name);
   }
  }
  DriveWatch(now, contact);
 }

 // Crewmen leaving a driver or gunner seat when no order of ours asked for it are logged
 // (2 Oct live: a LAV crew got out twice "for no reason"; not reproduced on the server).
 protected ref map<IEntity, int> m_CrewSeats = new map<IEntity, int>();
 protected void WatchCrew()
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   IEntity vehicle = ACR_Transport.VehicleOf(body);
   int seat = -1;
   if (vehicle) seat = ACR_Transport.SeatOf(body);
   int before;
   bool known = m_CrewSeats.Find(body, before);
   m_CrewSeats.Set(body, seat);
   if (!known || before < 0 || before == ECompartmentType.CARGO || seat >= 0) continue;
   ACR_EOrderState state = OrderState();
   if (state == ACR_EOrderState.GET_OUT || state == ACR_EOrderState.DISMOUNT) continue;
   string where = "";
   if (m_OwnVehicle) where = string.Format(", vehicle health %1, driver %2", ACR_Logistics.Health(m_OwnVehicle), DriverOf(m_OwnVehicle) != null);
   PrintFormat("[ACR] CREW LEFT %1: %2 got out of the %3 seat on his own (order '%4', alive %5, enemies near %6, player %7%8)", m_Name, NumberOf(body), typename.EnumToString(ECompartmentType, before), m_Order, ACR_Respawn.IsAlive(body), KnownEnemies(350), FindController(body), where);
  }
 }

 // ---------------------------------------------------------------- land routes
 // An order whose straight line crosses open water is walked leg by leg along a
 // land route instead (see BBB_Terrain), so squads go round a bay, not across it.
 protected ref array<vector> m_aRoute = {};
 protected ref ACR_OrderSnapshot m_Route; // the order at the end of the route (target unused)

 bool RouteTo(vector destination, ResourceName prefab, float radius, string order, int kind)
 {
  // Vehicle crews: aim for the nearest road point, which AI drivers can reach.
  vector road;
  IEntity driven = ACR_Transport.CrewedVehicle(this);
  if (driven && BBB_Terrain.RoadPointNear(destination, 250, road)) destination = road;
  IEntity anchor = GetAnchor();
  array<vector> legs = {};
  // ...and on longer trips, along the roads in short hops.
  if (driven && anchor && vector.DistanceXZ(anchor.GetOrigin(), destination) > 400)
  {
   int nodes, millis;
   if (BBB_Roads.Route(driven.GetOrigin(), destination, legs, nodes, millis) && legs.Count() > 1)
   {
    PrintFormat("[ACR] ROAD %1: %2 hops over %3 road points (%4 ms)", m_Name, legs.Count(), nodes, millis);
    if (System.IsCLIParam("acrRideTest")) foreach (vector hop : legs) PrintFormat("[ACR] ROAD   hop %1", hop);
    StartRoute(legs, prefab, radius, order, kind);
    return true;
   }
   legs.Clear();
  }
  if (!anchor || IsMounted() || !BBB_Terrain.LandRoute(anchor.GetOrigin(), destination, legs))
  {
   if (prefab == WP_GARRISON) { Garrison(destination); return true; }
   if (prefab == WP_DEFEND) return Defend(destination);
   if (prefab == WP_SEARCH_DESTROY) return Assault(destination);
   return MoveTo(destination);
  }
  PrintFormat("[ACR] ROUTE %1: straight line crosses water, going round in %2 legs", m_Name, legs.Count());
  StartRoute(legs, prefab, radius, order, kind);
  return true;
 }

 protected void StartRoute(array<vector> legs, ResourceName prefab, float radius, string order, int kind)
 {
  NewOrder();
  m_bRegrouping = false;
  ClearPlans();
  foreach (vector leg : legs) m_aRoute.Insert(leg);
  m_Route = ACR_OrderSnapshot.Make(prefab, vector.Zero, radius, order, StateOfLabel(order), kind);
  NextLeg();
 }

 protected void NextLeg()
 {
  vector leg = m_aRoute[0];
  m_aRoute.RemoveOrdered(0);
  if (m_aRoute.IsEmpty()) RestoreOrderAt(m_Route, m_Route.m_sPrefab, leg, m_Route.m_fRadius);
  else RestoreOrderAt(m_Route, WP_MOVE, leg, 25);
 }

 // ---- queued orders (Shift + right-click on the map)
 static const int MAX_QUEUED = 8;
 protected ref array<vector> m_aQueuePos = {};
 protected ref array<int> m_aQueueKind = {};   // 0 move, 1 defend, 2 garrison

 void ClearPlans()
 {
  m_aRoute.Clear();
  m_aQueuePos.Clear();
  m_aQueueKind.Clear();
 }

 // Add an order after the current one (or carry it out now if there's nothing to do).
 bool Queue(vector position, bool defend, bool garrison = false)
 {
  if (!m_Group) return false;
  bool busy = m_Group.GetCurrentWaypoint() != null || !m_aRoute.IsEmpty() || !m_aQueuePos.IsEmpty();
  if (!busy)
  {
   if (garrison) { Garrison(position); return true; }
   if (defend) return Defend(position);
   return MoveTo(position);
  }
  if (m_aQueuePos.Count() >= MAX_QUEUED) return false;
  m_aQueuePos.Insert(position);
  int kind = 0;
  if (defend) kind = 1;
  if (garrison) kind = 2;
  m_aQueueKind.Insert(kind);
  return true;
 }

 // Every point still ahead of the squad, in order: route legs, then queued orders.
 void GetPlannedPoints(notnull array<vector> points, notnull array<int> kinds)
 {
  int routeKind;
  if (m_Route) routeKind = m_Route.m_iKind;
  foreach (vector leg : m_aRoute)
  {
   points.Insert(leg);
   kinds.Insert(routeKind);
  }
  foreach (int i, vector queued : m_aQueuePos)
  {
   points.Insert(queued);
   int queuedKind = m_aQueueKind[i];
   if (queuedKind == 2) queuedKind = ACR_EOrderKind.DEFEND;
   kinds.Insert(queuedKind);
  }
 }

 protected void FollowQueue()
 {
  if (m_aQueuePos.IsEmpty() || !m_aRoute.IsEmpty() || m_bRegrouping || !m_Group) return;
  if (m_Group.GetCurrentWaypoint()) return;
  vector next = m_aQueuePos[0];
  int kind = m_aQueueKind[0];
  m_aQueuePos.RemoveOrdered(0);
  m_aQueueKind.RemoveOrdered(0);
  if (kind == 2)
  {
   PlaceGarrison(next);
   kind = ACR_EOrderKind.DEFEND;
  }
  else if (kind == 1)
  {
   IssueWaypoint(WP_DEFEND, next, 15);
   SetOrder("Defending", ACR_EOrderState.DEFEND);
  }
  else
  {
   IssueWaypoint(WP_MOVE, next);
   SetOrder("Moving", ACR_EOrderState.MOVE);
  }
  m_OrderKind = kind;
 }

 protected void FollowRoute()
 {
  FollowQueue();
  if (m_aRoute.IsEmpty() || m_bRegrouping || !m_Group) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetLeader();
  // Vehicles get the next road hop well before reaching this one: the AI driver
  // brakes as it nears the end of a waypoint, so handing over at 30 m made it stop
  // for a second at every hop (1 Oct live test).
  float handover = 30;
  if (IsMounted() && ACR_Transport.CrewedVehicle(this)) handover = 70;
  if (waypoints.IsEmpty() || (leader && vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < handover)) NextLeg();
 }

 // ---------------------------------------------------------------- cohesion
 // On a long move the leader keeps his pace while the rest string out behind
 // him. When a third of the squad is more than 60 m off, the squad stops (no
 // orders: the game's formation pulls everyone back to the leader) until they
 // are within 25 m, then carries on. Not in contact, and at most once a minute.
 static const float STRAGGLER_DISTANCE = 60;
 static const float REGROUPED_DISTANCE = 25;
 static const float REGROUP_TIMEOUT_MS = 60000;
 static const float REGROUP_COOLDOWN_MS = 60000;
 static const float REGROUP_FAILED_COOLDOWN_MS = 150000; // didn't close up: don't stop-start every minute

 protected bool m_bRegrouping;
 protected float m_fRegroupSince;
 protected float m_fNextRegroup;
 protected ref ACR_OrderSnapshot m_Regroup; // the order resumed once regrouped

 // Middle of the living members on foot (where a strung-out squad meets up).
 bool Centroid(out vector center)
 {
  array<AIAgent> agents = {};
  GetAgents(agents);
  vector sum;
  int n;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body)) continue;
   sum = sum + body.GetOrigin();
   n++;
  }
  if (n == 0) return false;
  center = ACR_Manager.Ground(sum * (1.0 / n));
  return true;
 }

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
   if (!closed) m_fNextRegroup = now + REGROUP_FAILED_COOLDOWN_MS;
   RestoreOrder(m_Regroup);
   PrintFormat("[ACR] TACTICS %1: regrouped (%2 s), moving on", m_Name, (int)((now - m_fRegroupSince) / 1000));
   return;
  }
  if (contact || now < m_fNextRegroup || !m_Group || HasPlayer() || IsMounted()) return;
  bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
  if (!movingOrder) return;
  array<AIWaypoint> waypoints = {};
  m_Group.GetWaypoints(waypoints);
  IEntity leader = GetLeader();
  if (waypoints.IsEmpty() || !leader || vector.DistanceXZ(leader.GetOrigin(), m_MoveTarget) < 200) return;
  int alive = CountAlive();
  if (alive < 3) return;
  int behind = Stragglers(STRAGGLER_DISTANCE);
  if (behind * 3 < alive) return;
  m_Regroup = SnapshotOrder();
  ClearOrders();
  // Everyone heads for the middle of the squad: the front waits and walks
  // back a little, the stragglers catch up, instead of the leader standing
  // still while the tail wanders.
  vector gather;
  if (Centroid(gather)) IssueWaypoint(WP_MOVE, gather, 10);
  SetOrder("Regrouping", ACR_EOrderState.REGROUP);
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

 protected IEntity m_StallVehicle;
 protected vector m_StallPos;
 protected float m_fStallSince;
 protected int m_iUnsticks;
 protected bool m_bUnsticking;
 protected ref ACR_SquadUnstick m_UnstickListener; // told by BBB_Unstick when backing out is over
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

 // The helicopter this squad crews (pilot/gunner seats), if any. AI can't fly in Reforger:
 // any waypoint makes a helicopter crew get out and walk (4 Oct MP: a player flying a
 // Game Master helicopter gave it a map move, the co-pilot bailed; after he left the seat the
 // pilot bailed too, walking to that move). Such a crew only ever holds or gets out.
 IEntity Aircraft()
 {
  IEntity vehicle = ACR_Transport.CrewedVehicle(this);
  if (!vehicle)
  {
   IEntity leader = GetLeader();
   if (leader && ACR_Transport.SeatOf(leader) != ECompartmentType.CARGO) vehicle = ACR_Transport.VehicleOf(leader);
  }
  if (vehicle && vehicle.FindComponent(VehicleHelicopterSimulation)) return vehicle;
  return null;
 }

 // Can fight: infantry of two or more, or a vehicle crew with a manned gun.
 // A lone driver or an unarmed truck crew is not sent to take or hold ground.
 bool IsCombatUnit()
 {
  if (m_bMortar) return false; // fire support, kept out of the fighting (ACR_Mortars)
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
   bool movingOrder = m_sOrderPrefab == WP_MOVE || m_sOrderPrefab == WP_DEFEND || m_sOrderPrefab == WP_SEARCH_DESTROY || m_sOrderPrefab == WP_GARRISON;
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
  float steer = 0.8;
  if (m_iUnsticks % 2 == 0) steer = -0.8;
  if (!m_UnstickListener) m_UnstickListener = new ACR_SquadUnstick(this);
  // BBB_Unstick parks the driver's AI and reverses (inputs applied every frame).
  BBB_Unstick.Begin(vehicle, driver, steer, m_UnstickListener);
  PrintFormat("[ACR] DRIVE %1: stuck, backing up (try %2)", m_Name, m_iUnsticks);
 }

 // Backing out is over (BBB_Unstick has given the driver his AI back).
 void EndUnstick()
 {
  m_bUnsticking = false;
  if (m_StallVehicle) m_StallPos = m_StallVehicle.GetOrigin();
  m_fStallSince = GetGame().GetWorld().GetWorldTime();
  // Fresh order, so the driver plans a new route from here.
  if (m_Group && !m_sOrderPrefab.IsEmpty())
  {
   // Label and state stay as they are.
   int kind = m_OrderKind;
   IssueWaypoint(m_sOrderPrefab, m_MoveTarget, m_fOrderRadius);
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
  m_Resume = ACR_OrderSnapshot.Make(prefab, target, radius, order, StateOfLabel(order), kind);
 }
 protected ref ACR_OrderSnapshot m_Resume; // carried on on foot after a dismount

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
  Halt("Dismounting", ACR_EOrderState.DISMOUNT);
  m_iDismountSweeps = 0;
  // One follow-up per squad: a second dismount (bail-out during a ride, F6 twice)
  // used to leave two queued, and the later one replaced the resumed order with
  // "Holding position".
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().CallLater(HoldAfterDismount, 3000, false);
  return dropped;
 }

 // Put members near the vehicle into its free passenger seats. The game's AI
 // won't board a vehicle another group is driving, so this is done directly:
 // climbing in normally when close, or placed in once they've had time to walk up.
 // forceAll: also seat soldiers who are walking up but stuck (the last one or two of a
 // squad often never made it in, 1 Oct live test).
 int BoardCargo(IEntity vehicle, float reach, bool placeFar, bool forceAll = false)
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
   if (!access || (access.IsGettingIn() && !forceAll)) continue;
   bool teleport = distance > 4 || forceAll;
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
   RestoreOrder(m_Resume);
   return;
  }
  IssueWaypoint(WP_DEFEND, anchor.GetOrigin(), 20);
  SetOrder("Holding position", ACR_EOrderState.DEFEND);
  m_OrderKind = ACR_EOrderKind.DEFEND;
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

 // Soldiers the engine took away for its active-AI limit (group dormant); they still count
 // toward the soldier cap so the AI doesn't keep buying empty squads.
 int CountDormant()
 {
  if (!m_Group || !m_Group.IsDormant()) return 0;
  return Math.Max(0, m_Group.GetDormantAliveCount());
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
  NewOrder();
  ClearPlans();
  m_bRegrouping = false;
  if (vehicle)
  {
   SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(IssueWaypoint(WP_GET_IN, vehicle.GetOrigin(), -1, true));
   if (!boarding) return false;
   boarding.SetEntity(vehicle);
  }
  else if (!IssueWaypoint(WP_GET_IN_NEAREST, position, 20, true)) return false;
  SetOrder("Getting in", ACR_EOrderState.GET_IN);
  m_OrderKind = ACR_EOrderKind.GET_IN;
  return true;
 }

 bool GetOut()
 {
  NewOrder();
  ClearPlans();
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  if (vehicles.IsEmpty()) return false;
  if (!IssueWaypoint(WP_GET_OUT, vehicles[0].GetOrigin(), -1, true)) return false;
  SetOrder("Getting out", ACR_EOrderState.GET_OUT);
  m_OrderKind = ACR_EOrderKind.GET_OUT;
  m_iGetOutChecks = 0;
  m_iGetOutSerial = m_iOrderSerial;
  GetGame().GetCallqueue().Remove(EnsureGetOut);
  if (!s_bNoGetOutFallback) GetGame().GetCallqueue().CallLater(EnsureGetOut, 4000, false);
  return true;
 }

 // The game's Get out only asks each passenger to leave. Passengers in a vehicle
 // another squad crews (a requisitioned Humvee's driver) stayed aboard (1 Oct MP),
 // so whoever is still inside once the vehicle has stopped is taken out directly.
 static bool s_bNoGetOutFallback; // tests: the game's waypoint alone
 protected int m_iGetOutChecks;
 protected int m_iGetOutSerial;
 void EnsureGetOut()
 {
  if (!m_Group || m_iGetOutSerial != m_iOrderSerial || OrderState() != ACR_EOrderState.GET_OUT || !IsMounted()) return;
  m_iGetOutChecks++;
  array<IEntity> vehicles = {};
  GetVehicles(vehicles);
  ForgetVehicles(vehicles);
  int stillIn, moved;
  array<AIAgent> agents = {};
  GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.IsInVehicle() || !ACR_Respawn.IsAlive(character)) continue;
   CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
   if (!access) continue;
   stillIn++;
   IEntity vehicle = access.GetVehicleIn(character);
   float speed;
   Physics physics;
   if (vehicle) physics = vehicle.GetPhysics();
   if (physics) speed = physics.GetVelocity().Length();
   if (speed > 1.5) continue; // the driver is someone else's: wait until it stops
   if (access.IsGettingOut()) continue;
   bool ok = access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   if (!ok && m_iGetOutChecks >= 3) ok = access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   if (ok) moved++;
  }
  PrintFormat("[ACR] GET OUT %1: check %2, %3 still aboard, %4 sent out", m_Name, m_iGetOutChecks, stillIn, moved);
  if (m_iGetOutChecks < 10) GetGame().GetCallqueue().CallLater(EnsureGetOut, 3000, false);
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
  if (m_Group && !m_Group.GetCurrentWaypoint() && FinishesOnArrival(OrderState())) order = "Holding";
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
  if (m_UnstickListener) BBB_Unstick.Cancel(m_UnstickListener);
  m_Aside = null; // being removed: nothing to resume
  Release();
  GetGame().GetCallqueue().Remove(LinkSquad);
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().Remove(EnsureGetOut);
  if (m_Waypoint) SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  if (manager && m_CommandGroup) manager.DeleteAndUnregisterGroup(m_CommandGroup);
  if (m_Group && m_Group.GetAgentsCount() == 0) SCR_EntityHelper.DeleteEntityAndChildren(m_Group);
  m_CommandGroup = null;
 }

 void ~ACR_Squad()
 {
  if (!GetGame()) return;
  GetGame().GetCallqueue().Remove(LinkSquad);
  GetGame().GetCallqueue().Remove(HoldAfterDismount);
  GetGame().GetCallqueue().Remove(EnsureGetOut);
 }
}
