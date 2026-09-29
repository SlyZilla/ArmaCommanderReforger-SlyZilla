// Native squad command menu, restricted to whoever controls a registered
// squad's designated leader. Vanilla checks the player's persistent group
// rank, which survives body swaps, so we check the controlled body instead.

// Server: validate again at execution. A delayed request must not command a
// squad after its leader was released, and orders are routed to the squad the
// player actually leads.
modded class SCR_CommandingManagerComponent
{
 override void RequestCommandExecution(int commandIndex, RplId cursorTargetID, RplId groupRplID, vector targetPosition, int playerID, float seed)
 {
  ACR_Manager acr = ACR_Manager.GetInstance();
  SCR_BaseRadialCommand command = FindCommand(FindCommandNameFromIndex(commandIndex));
  if (acr && command && command.Type().IsInherited(SCR_BaseGroupCommand))
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerID));
   ACR_Squad squad = acr.FindSquadLedBy(pc);
   if (!squad || !squad.m_Group)
   {
    Print("[ACR] Native command rejected: player is not controlling a squad leader");
    return;
   }
   groupRplID = SCR_EntityHelper.EntityToRplId(squad.m_Group);
   PrintFormat("[ACR] Native command executed: %1 -> %2", FindCommandNameFromIndex(commandIndex), squad.GetName());
  }
  super.RequestCommandExecution(commandIndex, cursorTargetID, groupRplID, targetPosition, playerID, seed);
 }
}

modded class SCR_PlayerControllerCommandingComponent
{
 bool ACR_HasCommandAuthority()
 {
  if (!ACR_Manager.GetInstance()) return true;
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
   SCR_HintManagerComponent.ShowCustomHint("Possess a squad leader to issue squad commands", "Squad command", 3, true);
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
  if (ACR_Manager.GetInstance())
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
   if (!pc || !pc.ACR_CanCommandSquad()) return false;
  }
  return super.CanRoleShow();
 }
}
