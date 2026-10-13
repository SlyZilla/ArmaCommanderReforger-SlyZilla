// How AC: Actual plugs into Better Battle Buddies (Scripts/Game/BBB): BBB looks
// after our squads' groups, names them as we do, knows their own vehicles, leaves
// gunner-driven vehicles alone and hands dropped launchers to our cleanup.
class ACR_BBBHooks : BBB_Hooks
{
 protected ACR_Manager m_Manager;

 void ACR_BBBHooks(ACR_Manager manager) { m_Manager = manager; }

 protected ACR_Squad SquadOf(SCR_AIGroup group) { return m_Manager.FindSquadByGroup(group); }

 override void GetGroups(notnull array<SCR_AIGroup> groups)
 {
  groups.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.m_Group) groups.Insert(squad.m_Group);
 }

 // Our roster includes a leader a player commands through (he sits in the command group).
 override void GetMembers(SCR_AIGroup group, notnull array<AIAgent> agents)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) squad.GetAgents(agents);
  else super.GetMembers(group, agents);
 }

 override string NameOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.GetName();
  return super.NameOf(group);
 }

 override IEntity AnchorOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.GetAnchor();
  return super.AnchorOf(group);
 }

 override IEntity OwnVehicleOf(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (squad) return squad.m_OwnVehicle;
  return null;
 }

 override bool AllowFireManeuver(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_Commander != 0 || squad.m_CombatFlank.Active()) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.HUNT;
 }

 override bool LeaveAlone(IEntity vehicle) { return m_Manager.IsGunnerDriven(vehicle); }

 override void OnLitter(IEntity item)
 {
  if (m_Manager.m_Cleanup) m_Manager.m_Cleanup.AddLitter(item);
 }
}

// A squad's end-of-unstick callback (BBB_Unstick tells it when backing out is over).
class ACR_SquadUnstick : BBB_UnstickListener
{
 protected ACR_Squad m_Squad;
 void ACR_SquadUnstick(ACR_Squad squad) { m_Squad = squad; }
 override void OnUnstickEnded(IEntity vehicle) { if (m_Squad) m_Squad.EndUnstick(); }
}
