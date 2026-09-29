// A real native playable group supplies the command menu's player leadership and
// commanded-AI association. The possessed leader temporarily joins the player
// command group so the remaining AI can navigate; the logical roster stays intact.
class ACR_CommandBridge
{
 SCR_AIGroup m_Squad;
 IEntity m_Leader;
 AIAgent m_DetachedLeader;
 SCR_AIGroup m_CommandGroup;
 int m_Commander;
 int m_PreviousGroup = -1;
 bool m_PreviousDeleteIfEmpty;

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
 void Init(SCR_AIGroup squad)
 {
  m_Squad = squad;
  if (!squad) return;
  squad.SetCustomName("Alpha 1", 0);
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  m_CommandGroup = manager.CreateNewPlayableGroup(squad.GetFaction(), SCR_EGroupRole.ASSAULT);
  if (!m_CommandGroup) return;
  m_CommandGroup.SetDeleteWhenEmpty(false);
  m_CommandGroup.SetCanDeleteIfNoPlayer(false);
  m_CommandGroup.SetPrivate(true);
  m_CommandGroup.SetPrivacyChangeable(false);
  m_CommandGroup.SetCustomName("Alpha 1", 0);
  // CreateNewPlayableGroup links an empty subgroup on a delayed RPC. Link our
  // spawned squad after that initialization has completed.
  GetGame().GetCallqueue().CallLater(LinkSquad, 3000, false);
 }
 void LinkSquad()
 {
  if (!m_CommandGroup || !m_Squad) return;
  SCR_GroupsManagerComponent manager = SCR_GroupsManagerComponent.GetInstance();
  manager.RequestSetGroupSlave(SCR_EntityHelper.EntityToRplId(m_CommandGroup), SCR_EntityHelper.EntityToRplId(m_Squad));
 }
 // Keep the designated leader across possession: the engine may temporarily
 // choose another active AI agent as its internal formation leader.
 IEntity GetLeader()
 {
  ChimeraCharacter leader = ChimeraCharacter.Cast(m_Leader);
  if (!leader || leader.GetCharacterController().IsDead())
  {
   if (m_Squad) m_Leader = m_Squad.GetLeaderEntity();
  }
  return m_Leader;
 }
 bool IsLeader(SCR_PlayerController pc)
 {
  if (!pc || !m_Squad) return false;
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
   if (!previous) previous = manager.CreateNewPlayableGroup(m_Squad.GetFaction(), SCR_EGroupRole.ASSAULT);
   if (previous) component.RPC_AskJoinGroup(previous.GetGroupID());
  }
  if (m_DetachedLeader)
  {
   m_CommandGroup.RemoveAgent(m_DetachedLeader);
   m_Squad.AddAgent(m_DetachedLeader);
   m_DetachedLeader = null;
  }
  if (previous) previous.SetCanDeleteIfNoPlayer(m_PreviousDeleteIfEmpty);
  PrintFormat("[ACR] NATIVE COMMAND released player=%1", m_Commander);
  m_Commander = 0;
  m_PreviousGroup = -1;
 }
 void UpdateFormationLeader()
 {
  // A player-held AI leader cannot navigate the group's formation to a waypoint.
  // Keep the RTS commander's identity, but let an AI lead formation movement.
  array<AIAgent> agents = {};
  m_Squad.GetAgents(agents);
  AIAgent next;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || body.GetCharacterController().IsDead() || FindController(body)) continue;
   if (!next || body == GetLeader()) next = agent;
  }
  if (next && next != m_Squad.GetLeaderAgent()) m_Squad.SetNewLeader(next);
 }
 void Refresh()
 {
  if (!m_Squad || !m_CommandGroup) return;
  GetLeader();
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
    m_Squad.RemoveAgent(m_DetachedLeader);
    m_CommandGroup.AddAgent(m_DetachedLeader);
   }
  }
  m_CommandGroup.SetGroupLeader(leaderPlayer);
  m_CommandGroup.SetCustomName("Alpha 1", 0);
  m_Commander = leaderPlayer;
  PrintFormat("[ACR] NATIVE COMMAND granted player=%1 group=%2 linked=%3", leaderPlayer, component.GetGroupID(), m_CommandGroup.GetSlave() == m_Squad);
 }
 void ~ACR_CommandBridge()
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(LinkSquad);
 }
}

// Validate again at execution: a delayed menu request must not command a squad
// after its leader has been released, or from another player's controller.
modded class SCR_CommandingManagerComponent
{
 override void RequestCommandExecution(int commandIndex, RplId cursorTargetID, RplId groupRplID, vector targetPosition, int playerID, float seed)
 {
  ACR_TestRange range = ACR_TestRange.s_Instance;
  SCR_BaseRadialCommand command = FindCommand(FindCommandNameFromIndex(commandIndex));
  if (range && command && command.Type().IsInherited(SCR_BaseGroupCommand))
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerID));
   if (!range.m_CommandBridge || !range.m_CommandBridge.IsLeader(pc))
   {
    Print("[ACR] NATIVE COMMAND rejected: player is not controlling the squad leader");
    return;
   }
   groupRplID = SCR_EntityHelper.EntityToRplId(range.m_Group);
   PrintFormat("[ACR] NATIVE COMMAND executed: %1", FindCommandNameFromIndex(commandIndex));
  }
  super.RequestCommandExecution(commandIndex, cursorTargetID, groupRplID, targetPosition, playerID, seed);
 }
}

// Vanilla checks the player's persistent group rank, which survives body swaps.
// The test range instead grants command access only in the designated leader body.
modded class SCR_PlayerControllerCommandingComponent
{
 bool ACR_HasCommandAuthority()
 {
  if (!ACR_TestRange.s_Instance) return true;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetOwner());
  return pc && pc.ACR_CanCommandSquad();
 }
 void ACR_CloseCommandMenu()
 {
  if (m_RadialMenuController && m_RadialMenuController.IsMenuOpen()) m_RadialMenuController.CloseMenu();
 }
 override protected void OpenCommandingMenu()
 {
  if (!ACR_HasCommandAuthority())
  {
   ACR_CloseCommandMenu();
   SCR_HintManagerComponent.ShowCustomHint("Possess Alpha 1's leader to issue squad commands", "Squad command", 3, true);
   return;
  }
  super.OpenCommandingMenu();
 }
 override void PrepareExecuteCommand(string commandName, vector targetPosition = vector.Zero)
 {
  if (!ACR_HasCommandAuthority()) return;
  super.PrepareExecuteCommand(commandName, targetPosition);
 }
}
modded class SCR_BaseGroupCommand
{
 override bool CanRoleShow()
 {
  if (ACR_TestRange.s_Instance)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
   if (!pc || !pc.ACR_CanCommandSquad()) return false;
  }
  return super.CanRoleShow();
 }
}
