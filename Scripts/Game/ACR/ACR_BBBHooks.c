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
  if (!squad || ACR_AirLift.Busy(squad) || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_Commander != 0 || squad.m_CombatFlank.Active()) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.HUNT;
 }

 override bool LeaveAlone(IEntity vehicle) { return m_Manager.IsGunnerDriven(vehicle); }

 override bool AllowRelaxedMarch(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  return squad && !ACR_AirLift.Busy(squad) && squad.OrderState() == ACR_EOrderState.MOVE && !squad.m_CombatFlank.Active();
 }

 // BBB may react locally to ordinary autonomous foot orders. Player orders,
 // quiet/forced movement, road columns and transport remain host-owned.
 override bool IsBinding(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return false;
  if (ACR_AirLift.Busy(squad)) return true;
  if (squad.m_Commander != 0 || squad.m_iOrderedBy != 0 || squad.m_bForced || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_CombatFlank.Active()) return true;
  ACR_EOrderState state = squad.OrderState();
  // Riders and crews in any order stay host-owned: ACA's stall/unstick drives its vehicles,
  // so BBB armour escapes must not back a defending or assaulting tank out on their own.
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad)) return true;
  return state != ACR_EOrderState.MOVE && state != ACR_EOrderState.ASSAULT && state != ACR_EOrderState.HUNT && state != ACR_EOrderState.DEFEND && state != ACR_EOrderState.HOLD && state != ACR_EOrderState.SUPPRESS;
 }

 override bool ManagesVehicleDriving(SCR_AIGroup group)
 {
  return SquadOf(group) != null;
 }

 // Illumination is allowed for autonomous combat squads, never player command,
 // quiet travel, transport, regroup or a host-owned flanking maneuver.
 override bool AllowFlares(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return true;
  if (ACR_AirLift.Busy(squad)) return false;
  if (squad.m_Commander != 0 || squad.m_Mode != ACR_EMoveMode.COMBAT || squad.m_CombatFlank.Active()) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.DEFEND || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.SUPPRESS || state == ACR_EOrderState.HUNT;
 }
 // ACA's native artillery, roadblock and fortified-position defence own these static-weapon
 // jobs. BBB's opportunistic quiet-time release must not take their gunners away.
 override bool AllowStaticWeapons(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  return !squad || (!ACR_AirLift.Busy(squad) && !squad.m_bMortar && !squad.m_ActiveRoadblock && !squad.m_Fortification);
 }

 // Gunners stay on the static guns at the spot a squad holds (ACR_GunHold; BBB_HeldGuns keeps
 // them there and mans an empty one again).
 override void GetHeldGuns(SCR_AIGroup group, notnull array<IEntity> guns)
 {
  guns.Clear();
  ACR_Squad squad = SquadOf(group);
  if (squad) ACR_GunHold.HeldGuns(squad, guns);
  // A position's gun crew from its reserve (its squad away on another job).
  else ACR_Defenders.CrewGunsOf(group, guns);
 }

 // Radio contact reports (BBB_ContactRelay) can make vanilla send fireteams to look. Only
 // squads waiting in place take them: free, posted or defending, on foot, not under a player's
 // own command. Movers, riders, attackers, builders and squads walking back to refit keep
 // ACA's plan.
 override bool AllowContactRelay(SCR_AIGroup group)
 {
  ACR_Squad squad = SquadOf(group);
  if (!squad) return true;
  if (ACR_AirLift.Busy(squad) || squad.m_Commander != 0 || squad.m_bForced || squad.m_Mode != ACR_EMoveMode.COMBAT) return false;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_CombatFlank.Active()) return false;
  int task = squad.m_iAITask;
  if (task != ACR_EAITask.FREE && task != ACR_EAITask.POSTED && task != ACR_EAITask.DEFEND) return false;
  ACR_EOrderState state = squad.OrderState();
  return state == ACR_EOrderState.NONE || state == ACR_EOrderState.HOLD || state == ACR_EOrderState.DEFEND;
 }

 // BBB's committed squad tactic (contact pause, assault, hold, reorganise) only for the same
 // waiting squads as contact relay: its HOLD stops advances, so ACA's movers, attackers and
 // flankers keep their plan and vanilla/BBB reactions as before.
 override bool AllowSquadTactic(SCR_AIGroup group)
 {
  return AllowContactRelay(group);
 }

 override bool AllowMorale(SCR_AIGroup group)
 {
  return SquadOf(group) == null;
 }

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
