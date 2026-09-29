// Test-range scenario entity. Placed in the world layer: starts Arma Commander,
// spawns one US fireteam ("Alpha 1") and drives the optional automated tests.
// Everything generic lives in ACR_Manager / ACR_Squad; this file is scenario only.
class ACR_TestRangeClass : GenericEntityClass {}
class ACR_TestRange : GenericEntity
{
 static ACR_TestRange s_Instance;
 SCR_AIGroup m_Group;
 vector m_Rally;
 vector m_Target;
 protected ref ACR_TestHarness m_Harness;

 void ACR_TestRange(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_Instance = this;
  ACR_Manager.Start();
  if (Replication.IsServer())
  {
   IEntity spawn = GetGame().GetWorld().FindEntityByName("ACR_PlayerSpawn");
   if (spawn) spawn.SetOrigin(ACR_Manager.Ground("2400 0 2400"));
   GetGame().GetCallqueue().CallLater(Setup, 1500, false);
   m_Harness = ACR_TestHarness.CreateFromCommandLine(this);
   if (m_Harness) GetGame().GetCallqueue().CallLater(m_Harness.Tick, 1000, true);
  }
  Print("[ACR] Range initialized");
 }

 void Setup()
 {
  m_Rally = ACR_Manager.Ground("2415 0 2400");
  m_Target = ACR_Manager.Ground("2450 0 2400");
  m_Group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab("{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et", m_Rally));
  // The manager registers it automatically once its soldiers exist.
  if (m_Group) m_Group.SetCustomName("Alpha 1", 0);
  PrintFormat("[ACR] Squad spawn: %1 at %2", m_Group, m_Rally);
 }

 override void EOnFrame(IEntity owner, float timeSlice)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (s_Instance && manager) manager.OnFrame();
 }

 void ~ACR_TestRange()
 {
  if (GetGame())
  {
   GetGame().GetCallqueue().Remove(Setup);
   if (m_Harness) GetGame().GetCallqueue().Remove(m_Harness.Tick);
  }
  if (s_Instance == this)
  {
   s_Instance = null;
   ACR_Manager.Stop();
  }
 }
}
