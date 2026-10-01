// Place one of these in any world to run Arma Commander there. Starts the
// mode, spawns ACR_StartingUnit entities, and drives the local input.
class ACR_ModeClass : GenericEntityClass {}
class ACR_Mode : GenericEntity
{
 [Attribute("1", UIWidgets.CheckBox, "Game Master for the host and server admins")]
 bool m_bHostGameMaster;
 [Attribute("0", UIWidgets.CheckBox, "Shared command: every player of a faction may command")]
 bool m_bSharedCommand;

 [Attribute("70", UIWidgets.ComboBox, "Skill of all Arma Commander AI soldiers", "", ParamEnumArray.FromEnum(EAISkill))]
 int m_iAISkill;
 [Attribute("1.6", UIWidgets.EditBox, "AI reaction speed: how fast AI spot and identify targets (game default 1)")]
 float m_fAIPerception;
 [Attribute("15", UIWidgets.EditBox, "Seconds before a killed command unit respawns at base")]
 int m_iRespawnSeconds;

 static ACR_Mode s_Instance;
 protected bool m_bSummary;
 protected bool m_bSoak;
 protected int m_iSeconds;

 void ACR_Mode(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT | EntityEvent.FRAME | EntityEvent.POSTFRAME);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_Instance = this;
  ACR_Manager manager = ACR_Manager.Start();
  manager.m_bHostGameMaster = m_bHostGameMaster;
  manager.m_SharedCommand = m_bSharedCommand;
  manager.m_iRespawnSeconds = m_iRespawnSeconds;
  if (m_iAISkill > 0) manager.m_iAISkill = m_iAISkill;
  if (m_fAIPerception > 0) manager.m_fAIPerception = m_fAIPerception;
  string respawnFlag;
  if (System.GetCLIParam("acrRespawnSeconds", respawnFlag)) manager.m_iRespawnSeconds = respawnFlag.ToInt();
  if (Replication.IsServer())
  {
   GetGame().GetCallqueue().CallLater(SpawnStartingUnits, 2000, false);
   string flag;
   m_bSummary = System.GetCLIParam("acrScenarioTest", flag);
  m_bSoak = System.GetCLIParam("acrSoakTest", flag);
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
  if (m_bSoak) { Soak(); return; }
  if (!m_bSummary || m_iSeconds % 30 != 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  PrintFormat("[ACR] SCENARIO t=%1 squads=%2 US rp=%3 USSR rp=%4", m_iSeconds, manager.m_Squads.Count(), manager.m_Bases.GetRP("US"), manager.m_Bases.GetRP("USSR"));
  foreach (ACR_Base base : ACR_Base.s_All)
   PrintFormat("[ACR] SCENARIO   %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  if (m_iSeconds >= 180) Print("[ACR] TEST COMPLETE");
 }

 // -acrSoakTest: AI against AI for 20 minutes (the player hands command to the
 // AI), with a summary every minute for reading the whole battle afterwards.
 protected void Soak()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (m_iSeconds == 5)
  {
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
    if (pc) PrintFormat("[ACR] SOAK player %1: %2", id, manager.LeaveCommand(pc));
   }
  }
  if (m_iSeconds % 60 == 0)
  {
   int us, ussr, usMen, ussrMen;
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction()) continue;
    if (squad.GetFaction().GetFactionKey() == "US") { us++; usMen += squad.CountAlive(); }
    else { ussr++; ussrMen += squad.CountAlive(); }
   }
   PrintFormat("[ACR] SOAK t=%1 | US %2 squads/%3 men rp %4 | USSR %5 squads/%6 men rp %7", m_iSeconds, us, usMen, manager.m_Bases.GetRP("US"), ussr, ussrMen, manager.m_Bases.GetRP("USSR"));
   foreach (ACR_Base base : ACR_Base.s_All)
    PrintFormat("[ACR] SOAK   %1: %2", base.m_sName, manager.m_Bases.Describe(base));
  }
  if (m_iSeconds >= 1200) Print("[ACR] TEST COMPLETE");
 }

 // After the frame's other updates (gunner drives re-assert their inputs).
 override void EOnPostFrame(IEntity owner, float timeSlice)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance == this && manager) manager.SimulateStep();
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
