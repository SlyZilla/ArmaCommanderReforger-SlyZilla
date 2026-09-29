// Place one of these in any world to run Arma Commander there. Starts the
// mode, spawns ACR_StartingUnit entities, and drives the local input.
class ACR_ModeClass : GenericEntityClass {}
class ACR_Mode : GenericEntity
{
 [Attribute("1", UIWidgets.CheckBox, "Game Master for the host and server admins")]
 bool m_bHostGameMaster;
 [Attribute("0", UIWidgets.CheckBox, "Shared command: every player of a faction may command")]
 bool m_bSharedCommand;

 static ACR_Mode s_Instance;
 protected bool m_bSummary;
 protected int m_iSeconds;

 void ACR_Mode(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_Instance = this;
  ACR_Manager manager = ACR_Manager.Start();
  manager.m_bHostGameMaster = m_bHostGameMaster;
  manager.m_SharedCommand = m_bSharedCommand;
  if (Replication.IsServer())
  {
   GetGame().GetCallqueue().CallLater(SpawnStartingUnits, 2000, false);
   string flag;
   m_bSummary = System.GetCLIParam("acrScenarioTest", flag);
   GetGame().GetCallqueue().CallLater(Second, 1000, true);
  }
  Print("[ACR] Scenario started");
 }

 protected void SpawnStartingUnits()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   vector pos = unit.GetOrigin();
   manager.SpawnUnit(unit.m_sName, unit.m_sGroup, unit.m_sVehicle, unit.m_bCrewed, unit.m_sPassengers, ACR_Manager.Ground(pos), ACR_Manager.Ground(pos + Vector(6, 0, 6)));
   PrintFormat("[ACR] Starting unit: %1", unit.m_sName);
  }
 }

 // Optional log summary for automated runs (-acrScenarioTest 1).
 protected void Second()
 {
  m_iSeconds++;
  if (!m_bSummary || m_iSeconds % 30 != 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  PrintFormat("[ACR] SCENARIO t=%1 squads=%2 US rp=%3 USSR rp=%4", m_iSeconds, manager.m_Squads.Count(), manager.m_Bases.GetRP("US"), manager.m_Bases.GetRP("USSR"));
  foreach (ACR_Base base : ACR_Base.s_All)
   PrintFormat("[ACR] SCENARIO   %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  if (m_iSeconds >= 180) Print("[ACR] TEST COMPLETE");
 }

 override void EOnFrame(IEntity owner, float timeSlice)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance == this && manager)
  {
   manager.ServerFrame(timeSlice);
   manager.OnFrame();
  }
 }

 void ~ACR_Mode()
 {
  if (GetGame())
  {
   GetGame().GetCallqueue().Remove(SpawnStartingUnits);
   GetGame().GetCallqueue().Remove(Second);
  }
  if (s_Instance == this)
  {
   s_Instance = null;
   ACR_Manager.Stop();
  }
 }
}

// A unit that exists at the start of the match, placed where it should appear.
// Same fields as a requisition entry; the name is used as-is (no number).
class ACR_StartingUnitClass : GenericEntityClass {}
class ACR_StartingUnit : GenericEntity
{
 [Attribute("", UIWidgets.EditBox, "Name shown on the map, e.g. 1st Squad")]
 string m_sName;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Infantry group", "et")]
 ResourceName m_sGroup;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle (optional)", "et")]
 ResourceName m_sVehicle;
 [Attribute("1", UIWidgets.CheckBox, "Vehicle comes with its driver and gunners")]
 bool m_bCrewed;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Group riding in the vehicle (optional)", "et")]
 ResourceName m_sPassengers;

 static ref array<ACR_StartingUnit> s_All = {};

 void ACR_StartingUnit(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }
 override void EOnInit(IEntity owner)
 {
  if (GetGame().InPlayMode()) s_All.Insert(this);
 }
 void ~ACR_StartingUnit()
 {
  if (s_All) s_All.RemoveItem(this);
 }
}
