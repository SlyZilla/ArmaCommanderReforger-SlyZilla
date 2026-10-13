// Bookkeeping budget at 100 active, armed native combatants. Damage disabled to
// keep the load constant; this measures cost, not casualties or tactical benefit.
class BBB_FireManeuverPerfTest : BBB_FireManeuverTest
{
 static ref BBB_FireManeuverPerfTest s_PerfFM;
 protected ref array<ref BBB_AimLane> m_LanesFM = {};
 protected float m_fBenchStart;
 protected int m_iTicksBefore, m_iMsBefore, m_iDecisionBefore, m_iEventsBefore, m_iContacts;
 protected bool m_bPinned, m_bMeasured, m_bOriginalFM;
 static void MaybeStartPerfFM()
 {
  if (s_PerfFM || !Replication.IsServer() || !System.IsCLIParam("acrBbbFireManeuverPerfTest")) return;
  s_PerfFM = new BBB_FireManeuverPerfTest();
  GetGame().GetCallqueue().CallLater(s_PerfFM.BeginPerfFM, 10000);
 }
 protected void BeginPerfFM()
 {
  BBB_FireManeuver.Read();
  m_bOriginalFM = BBB_FireManeuver.s_bOn;
  BBB_FireManeuver.s_bOn = true;
  BBB_Perf.s_bOn = true;
  BBB_Perf.s_bOwnReport = true;
  for (int i = 0; i < 6; i++)
  {
   BBB_AimLane lane = new BBB_AimLane();
   lane.m_vFrom = BBB.Ground(Vector(1230 + i * 34, 0, 2520));
   lane.m_vDir = Vector(0.087, 0, 0.996);
   lane.m_vTo = BBB.Ground(lane.m_vFrom + lane.m_vDir * 100);
   int size = 8;
   if (i < 2) size = 9;
   lane.m_A.m_Group = SpawnSquad(false, lane.m_vFrom, lane.m_vDir, lane, size);
   lane.m_B.m_Group = SpawnSquad(true, lane.m_vTo, lane.m_vDir * -1, lane, size);
   m_LanesFM.Insert(lane);
  }
  m_fBenchStart = Now();
  GetGame().GetCallqueue().CallLater(TickPerfFM, 1000, true);
 }
 protected int Combatants()
 {
  int count;
  foreach (BBB_AimLane lane : m_LanesFM)
  {
   array<BBB_AimSide> sides = {lane.m_A, lane.m_B};
   foreach (BBB_AimSide side : sides)
   {
    array<AIAgent> agents = {};
    if (side.m_Group) side.m_Group.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     if (!BBB.IsAlive(agent.GetControlledEntity()) || agent.GetLOD() != 0) continue;
     count++;
     SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
     if (utility && utility.m_CombatComponent && utility.m_CombatComponent.GetCurrentTarget()) m_iContacts++;
    }
   }
  }
  return count;
 }
 protected void TickPerfFM()
 {
  float elapsed = Now() - m_fBenchStart;
  if (!m_bPinned && elapsed > 3)
  {
   m_bPinned = true;
   foreach (BBB_AimLane lane : m_LanesFM)
   {
    Hook(lane.m_A, false);
    Hook(lane.m_B, false);
    array<BBB_AimSide> sides = {lane.m_A, lane.m_B};
    foreach (BBB_AimSide side : sides)
     foreach (IEntity man : side.m_aMen)
     {
      ChimeraCharacter character = ChimeraCharacter.Cast(man);
      if (character) character.GetDamageManager().EnableDamageHandling(false);
     }
   }
  }
  if (!m_bMeasured && elapsed >= 30)
  {
   m_bMeasured = true;
   m_iTicksBefore = BBB_FireManeuver.s_iTicks;
   m_iMsBefore = BBB_FireManeuver.s_iTickMs;
   m_iDecisionBefore = BBB_FireManeuver.s_iDecisionMs;
   m_iEventsBefore = BBB_FireManeuver.s_iEventMs;
  }
  int active = Combatants();
  if (elapsed < 90) return;
  int ticks = BBB_FireManeuver.s_iTicks - m_iTicksBefore;
  float cost = BBB_FireManeuver.s_iTickMs - m_iMsBefore + BBB_FireManeuver.s_iDecisionMs - m_iDecisionBefore + BBB_FireManeuver.s_iEventMs - m_iEventsBefore;
  float average = cost / Math.Max(1, ticks);
  string verdict = "PASS";
  if (active != 100 || m_iContacts < 100 || ticks < 50 || average > 1.0) verdict = "FAIL";
  PrintFormat("[BBB] FIREPERF active=%1 contactSamples=%2 ticks=%3 bookkeepingAvgMs=%4 tickMaxMs=%5 (native cover/path/BT cost remains in frame time)", active, m_iContacts, ticks, average, BBB_FireManeuver.s_iTickMax);
  PrintFormat("[ACR] VERDICT %1 test=bbbfiremaneuverperf active=%2 expected=100 contacts=%3 averageMs=%4 budgetMs=1", verdict, active, m_iContacts, average);
  GetGame().GetCallqueue().Remove(TickPerfFM);
  BBB_FireManeuver.Stop();
  foreach (BBB_AimLane cleanup : m_LanesFM) Clear(cleanup);
  m_LanesFM.Clear();
  BBB_FireManeuver.s_bOn = m_bOriginalFM;
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_FireManeuverPerfTest.MaybeStartPerfFM();
 }
}
