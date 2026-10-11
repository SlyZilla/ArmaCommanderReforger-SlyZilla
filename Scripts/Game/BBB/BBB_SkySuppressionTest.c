// Controlled report-path reproduction. A real helicopter passes a fire team;
// gunshot reports are injected at the receiving group API, not claimed as live
// helicopter weapon fire. Compares vanilla and guarded source classification.
class BBB_SkySuppressionTest : BBB_AimTest
{
 static ref BBB_SkySuppressionTest s_Sky;
 static const ResourceName HELI = "{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et";
 protected IEntity m_Helicopter;
 protected IEntity m_Crew;
 protected IEntity m_GroundShooter;
 protected ref SCR_AIGroupPerception m_Raw;
 protected ref SCR_AIGroupPerception m_Guarded;
 protected vector m_From;
 protected vector m_Dir;
 protected float m_Start;
 protected int m_Passed;
 protected int m_Failed;
 protected bool m_Original;
 protected bool m_Reproduced;
 protected bool m_GuardHeld = true;
 protected bool m_Ready;

 static void MaybeStartSky()
 {
  if (s_Sky || !Replication.IsServer() || !System.IsCLIParam("acrBbbSkyTest")) return;
  s_Sky = new BBB_SkySuppressionTest();
  GetGame().GetCallqueue().CallLater(s_Sky.BeginSky, 10000);
 }

 void BBB_SkySuppressionTest() { m_aCheckDistances = {100}; }

 protected void AssertSky(string name, bool ok)
 {
  string result = "FAIL";
  if (ok) { result = "PASS"; m_Passed++; }
  else m_Failed++;
  PrintFormat("[BBB] SKYTEST %1 %2", result, name);
 }

 protected void BeginSky()
 {
  BBB_SuppressionSafety.Read();
  m_Original = BBB_SuppressionSafety.s_bOn;
  if (!FindSite()) { FinishSky("no clear lane"); return; }
  LoadLane();
  m_From = m_aSiteFrom[0];
  m_Dir = m_aSiteDir[0];
  BBB_AimLane lane = new BBB_AimLane();
  lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_USSR, m_From, m_Dir));
  if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
  m_aLanes.Insert(lane);
  m_Helicopter = Spawn(HELI, m_From + m_Dir * 100, m_Dir);
  m_GroundShooter = Spawn(RIFLE_US, m_From + m_Dir * 100, m_Dir);
  if (!lane.m_A.m_Group || !m_Helicopter || !m_GroundShooter) { FinishSky("missing entities"); return; }
  Physics physics = m_Helicopter.GetPhysics();
  if (physics) physics.EnableGravity(false);
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(m_Helicopter.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<ECompartmentType> crewTypes = {ECompartmentType.PILOT, ECompartmentType.TURRET};
  if (seats) seats.SpawnDefaultOccupants(crewTypes);
  SCR_AIGroupUtilityComponent utility = lane.m_A.m_Group.GetGroupUtilityComponent();
  if (!utility) { FinishSky("no group utility"); return; }
  m_Raw = new SCR_AIGroupPerception(utility, lane.m_A.m_Group);
  m_Guarded = new SCR_AIGroupPerception(utility, lane.m_A.m_Group);
  m_Start = Now();
  GetGame().GetCallqueue().CallLater(TickSky, 1000, true);
 }

 protected void TickSky()
 {
  float elapsed = Now() - m_Start;
  vector at = BBB.Ground(m_From + m_Dir * (100 + elapsed * 3)) + Vector(0, 40, 0);
  m_Helicopter.SetOrigin(at);
  if (m_Helicopter.GetPhysics()) m_Helicopter.GetPhysics().SetVelocity(vector.Zero);
  if (elapsed < 4) return;
  if (!m_Ready)
  {
   Hook(m_aLanes[0].m_A, false);
   SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Helicopter.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> turrets = {};
   if (compartments) compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
   foreach (BaseCompartmentSlot seat : turrets) if (seat.GetOccupant()) { m_Crew = seat.GetOccupant(); break; }
   if (!m_Crew) { FinishSky("no seated gunner"); return; }
   m_Ready = true;
  }
  float timestamp = GetGame().GetPerceptionManager().GetTime();
  Faction faction = GetGame().GetFactionManager().GetFactionByKey("US");
  BBB_SuppressionSafety.s_bOn = false;
  m_Raw.AddOrUpdateGunshot(m_Crew, at, faction, timestamp, true);
  if (!m_Raw.m_aTargets.IsEmpty())
  {
   SCR_AIGroupTargetCluster cluster = new SCR_AIGroupTargetCluster();
   cluster.AddTarget(m_Raw.m_aTargets[0], 0, 100);
   SCR_AITargetClusterState state = new SCR_AITargetClusterState(cluster);
   state.ProcessTargets();
   SCR_AISuppressionVolumeClusterBox volume = SCR_AISuppressionVolumeClusterBox.GetVolumeFromClusterState(state);
   vector center = volume.GetCenterPosition();
   float height = center[1] - GetGame().GetWorld().GetSurfaceY(center[0], center[2]);
   if (height > 20) m_Reproduced = true;
   PrintFormat("[BBB] SKYTEST raw crew report -> suppression center %1 m above terrain at %2", height, center);
  }
  BBB_SuppressionSafety.s_bOn = true;
  m_Guarded.AddOrUpdateGunshot(m_Crew, at, faction, timestamp, true);
  m_Guarded.AddOrUpdateGunshot(m_Helicopter, at, faction, timestamp, true);
  if (!m_Guarded.m_aTargets.IsEmpty()) m_GuardHeld = false;
  if (elapsed < 20) return;
  AssertSky("airborne gunner report reproduces a sky suppression volume with guard off", m_Reproduced);
  AssertSky("passing helicopter and seated gunner reports stay excluded with guard on", m_GuardHeld);
  AssertSky("gunner drill refuses airborne heard sectors", !BBB_SuppressionSafety.GroundSector(at));
  vector ground = BBB.Ground(m_GroundShooter.GetOrigin()) + Vector(0, 1.5, 0);
  m_Guarded.AddOrUpdateGunshot(m_GroundShooter, ground, faction, timestamp, true);
  AssertSky("ground infantry reports and ground sectors remain usable", m_Guarded.m_aTargets.Count() == 1 && BBB_SuppressionSafety.GroundSector(ground));
  FinishSky("");
 }

 protected void FinishSky(string reason)
 {
  GetGame().GetCallqueue().Remove(TickSky);
  BBB_SuppressionSafety.s_bOn = m_Original;
  string verdict = "PASS";
  if (reason != "" || m_Failed > 0 || m_Passed != 4) verdict = "FAIL";
  PrintFormat("[ACR] VERDICT %1 test=bbbsky passed=%2 failed=%3 expected=4 reason=%4", verdict, m_Passed, m_Failed, reason);
  // Leave fixture entities alive until process shutdown; their contacts can still
  // be queued by native perception and BBB radio for the following frame.
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_SkySuppressionTest.MaybeStartSky();
 }
}
