// Fixed native firefights: paired off/on rounds, factions swapped. No contact or
// cover position is injected into production decisions. Two damage-protected
// sentries keep the contact available; attacking squads take normal damage.
class BBB_FireManeuverTest : BBB_AimTest
{
 static ref BBB_FireManeuverTest s_TestFM;
 static const ResourceName RU_RIFLE = "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et";
 static const ResourceName US_MG = "{5B1996C05B1E51A4}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_AR.et";
 static const ResourceName RU_MG = "{23ADBBC31B6A3DC6}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_AR.et";
 protected ref BBB_AimLane m_Lane;
 protected float m_fEnd, m_fHook, m_fLog;
 protected int m_iFMRound, m_iFinished, m_iRequests, m_iArrivals, m_iSwaps;
 protected int m_iPass, m_iFail;
 protected bool m_bSaved, m_bGuards, m_bProbe;
 protected ref map<IEntity, vector> m_Positions = new map<IEntity, vector>();
 protected float m_fMoverTravel, m_fCoverTravel;
 protected int m_iCoverFire;
 protected ref array<int> m_Shots = {};
 protected ref array<int> m_Losses = {};

 static void MaybeStartFM()
 {
  if (s_TestFM || !Replication.IsServer() || (!System.IsCLIParam("acrBbbFireManeuverTest") && !System.IsCLIParam("acrBbbFireManeuverProbeTest") && !System.IsCLIParam("acrBbbFireManeuverEastTest"))) return;
  s_TestFM = new BBB_FireManeuverTest();
  s_TestFM.m_bProbe = System.IsCLIParam("acrBbbFireManeuverProbeTest") || System.IsCLIParam("acrBbbFireManeuverEastTest");
  if (System.IsCLIParam("acrBbbFireManeuverEastTest")) s_TestFM.m_iFMRound = 2;
  GetGame().GetCallqueue().CallLater(s_TestFM.BeginFM, 10000);
 }
 protected void CheckFM(string name, bool ok)
 {
  string result = "FAIL";
  if (ok) { result = "PASS"; m_iPass++; } else m_iFail++;
  PrintFormat("[BBB] FIRETEST %1 %2", result, name);
 }
 protected void BeginFM()
 {
  BBB_FireManeuver.Read();
  m_bSaved = BBB_FireManeuver.s_bOn;
  BBB_Perf.s_bOn = true;
  BBB_Perf.s_bOwnReport = true;
  // Farm approach: existing fences, trees and building cover, not a bare
  // accuracy lane. Pick a direction with a native crouched view to the enemy.
  array<vector> starts = {"1346 0 2776", "1370 0 2350", "1850 0 2500", "1335 0 2832", "1300 0 2530"};
  array<vector> directions = {Vector(0.087, 0, 0.996), Vector(0.996, 0, -0.087), Vector(-0.087, 0, -0.996), Vector(-0.996, 0, 0.087)};
  bool found;
  foreach (vector start : starts)
  {
   foreach (vector direction : directions)
   {
    if (BBB_FiringCover.Ray(BBB.Ground(start) + Vector(0, 1.4, 0), BBB.Ground(start + direction * 100) + Vector(0, 1.4, 0)) < 0.99) continue;
    m_aSiteFrom.Insert(BBB.Ground(start));
    m_aSiteDir.Insert(direction);
    found = true;
    break;
   }
   if (found) break;
  }
  if (!found) { FinishFM("no clear cover-area lane"); return; }
  LoadLane();
  PrintFormat("[BBB] FIRETEST lane=%1 dir=%2, 4 x 75 s off/on, factions swapped", m_aSiteFrom[0], m_aSiteDir[0]);
  StartRound();
  GetGame().GetCallqueue().CallLater(TickFM, 500, true);
 }
 protected SCR_AIGroup SpawnSquad(bool east, vector position, vector facing, BBB_AimLane lane, int size = 8)
 {
  ResourceName groupPrefab = TEAM_US;
  ResourceName rifle = RIFLE_US;
  ResourceName machinegun = US_MG;
  if (east) { groupPrefab = TEAM_USSR; rifle = RU_RIFLE; machinegun = RU_MG; }
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(Spawn(groupPrefab, position, facing));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return null;
  lane.m_aSpawned.Insert(group);
  group.m_aUnitPrefabSlots.Clear();
  vector side = Vector(-facing[2], 0, facing[0]);
  for (int i = 0; i < size; i++)
  {
   ResourceName unit = rifle;
   if (i == 0 || i == 4) unit = machinegun;
   group.m_aUnitPrefabSlots.Insert(unit);
   IEntity man = Spawn(unit, BBB.Ground(position + side * (i * 2.5 - 9)), facing);
   if (man) { group.AddAIEntityToGroup(man); lane.m_aSpawned.Insert(man); }
  }
  return group;
 }
 protected void StartRound()
 {
  BBB_FireManeuver.Stop();
  BBB_FireManeuver.s_bOn = m_bProbe || m_iFMRound % 2 == 1;
  m_Lane = new BBB_AimLane();
  m_Lane.m_vFrom = m_aSiteFrom[0];
  m_Lane.m_vDir = m_aSiteDir[0];
  m_Lane.m_vTo = BBB.Ground(m_Lane.m_vFrom + m_Lane.m_vDir * 100);
  m_Lane.m_A.m_Group = SpawnSquad(m_iFMRound >= 2, m_Lane.m_vFrom, m_Lane.m_vDir, m_Lane);
  m_Lane.m_B.m_Group = SpawnSquad(m_iFMRound < 2, m_Lane.m_vTo, m_Lane.m_vDir * -1, m_Lane, 2);
  if (!m_Lane.m_A.m_Group || !m_Lane.m_B.m_Group) { FinishFM("missing group"); return; }
  // Real physical cover in a controlled lane. Nothing is registered as a
  // fake native cover point; production must find native or ray-tested cover.
  vector lateral = Vector(-m_Lane.m_vDir[2], 0, m_Lane.m_vDir[0]);
  for (int row = 0; row < 4; row++)
   for (int col = -4; col <= 4; col++)
   {
    vector wallAt = BBB.Ground(m_Lane.m_vFrom + m_Lane.m_vDir * (6 + row * 12) + lateral * (col * 6));
    IEntity wall = Spawn("{5EF52FAE299A831A}Prefabs/Props/Military/Sandbags/Sandbag_01_short_base.et", wallAt, m_Lane.m_vDir);
    if (wall) m_Lane.m_aSpawned.Insert(wall);
   }
  m_iRequests = BBB_FireManeuver.s_iRequests;
  m_iArrivals = BBB_FireManeuver.s_iArrived;
  m_iSwaps = BBB_FireManeuver.s_iSwaps;
  m_iCoverFire = BBB_FireManeuver.s_iCoverShots;
  m_Positions.Clear();
  m_fMoverTravel = 0;
  m_fCoverTravel = 0;
  m_fHook = Now() + 3;
  m_fEnd = Now() + 75;
  m_fLog = Now() + 5;
 }
 protected void TickFM()
 {
  if (!m_Lane) return;
  if (Now() >= m_fHook && !m_Lane.m_bHooked)
  {
   m_Lane.m_bHooked = true;
   Hook(m_Lane.m_A, false);
   Hook(m_Lane.m_B, false);
   PlaceOnNativeCover(m_Lane.m_A.m_Group);
   m_Lane.m_B.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
   m_Lane.m_B.m_Group.GetGroupUtilityComponent().EvaluateCombatMode();
   foreach (IEntity sentry : m_Lane.m_B.m_aMen)
   {
    ChimeraCharacter target = ChimeraCharacter.Cast(sentry);
    if (target)
    {
     target.GetDamageManager().EnableDamageHandling(false);
     target.GetCharacterController().OverrideMaxSpeed(0);
    }
   }
   ProbeCover(m_Lane.m_A.m_Group);
   ProbeCover(m_Lane.m_B.m_Group);
  }
  // Keep the controlled sentries visible; their native aiming and firing
  // remain active. This is a drill fixture, not a claim about survival benefit.
  foreach (IEntity sentryBody : m_Lane.m_B.m_aMen)
  {
   ChimeraCharacter fixedTarget = ChimeraCharacter.Cast(sentryBody);
   if (!fixedTarget) continue;
   SCR_AIStanceHandling.SetStance(fixedTarget.GetCharacterController(), ECharacterStance.STAND);
   AIControlComponent sentryControl = AIControlComponent.Cast(sentryBody.FindComponent(AIControlComponent));
   if (!sentryControl || !sentryControl.GetAIAgent()) continue;
   SCR_AIUtilityComponent sentryUtility = SCR_AIUtilityComponent.Cast(sentryControl.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
   if (sentryUtility && sentryUtility.GetCurrentBehavior()) sentryUtility.GetCurrentBehavior().m_bUseCombatMove = false;
  }
  array<BBB_AimSide> sides = {m_Lane.m_A, m_Lane.m_B};
  foreach (BBB_AimSide team : sides)
  {
   team.SampleAmmo();
   foreach (IEntity man : team.m_aMen)
   {
    AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
    SCR_AIUtilityComponent utility;
    if (control && control.GetAIAgent()) utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
    if (!utility) continue;
    float travel;
    if (m_Positions.Contains(man)) travel = vector.DistanceXZ(man.GetOrigin(), m_Positions.Get(man));
    m_Positions.Set(man, man.GetOrigin());
    if (utility.m_BBBFireJob && utility.m_BBBFireJob.m_bMoving)
    {
     if (utility.m_iBBBFireTeam == utility.m_BBBFireJob.m_iCoverTeam) m_fCoverTravel += travel;
     else m_fMoverTravel += travel;
    }
    if (!m_bGuards && BBB_FireManeuver.s_iSwaps > m_iSwaps && BBB_FireManeuver.Decision(utility) != 0) Guards(utility);
   }
  }
  if (Now() >= m_fLog)
  {
   PrintFormat("[BBB] FIRETEST round=%1 enabled=%2 shots=%3/%4 down=%5/%6 requests=%7 arrived=%8 swaps=%9", m_iFMRound, BBB_FireManeuver.s_bOn, m_Lane.m_A.m_iShots, m_Lane.m_B.m_iShots, Downed(m_Lane.m_A), Downed(m_Lane.m_B), BBB_FireManeuver.s_iRequests - m_iRequests, BBB_FireManeuver.s_iArrived - m_iArrivals, BBB_FireManeuver.s_iSwaps - m_iSwaps);
   ProbeCover(m_Lane.m_A.m_Group);
   m_fLog = Now() + 10;
  }
  if (Now() < m_fEnd) return;
  int fired = m_Lane.m_A.m_iShots + m_Lane.m_B.m_iShots;
  m_Shots.Insert(fired);
  m_Losses.Insert(Downed(m_Lane.m_A) + Downed(m_Lane.m_B));
  CheckFM("native firefight produced shots", fired > 0);
  if (BBB_FireManeuver.s_bOn)
  {
   CheckFM("validated cover bounds completed", BBB_FireManeuver.s_iArrived > m_iArrivals);
   CheckFM("covering team fired while movers travelled", BBB_FireManeuver.s_iCoverShots > m_iCoverFire && m_fMoverTravel > 3);
   CheckFM("fireteams switched roles after reaching cover", BBB_FireManeuver.s_iSwaps > m_iSwaps);
   PrintFormat("[BBB] FIRETEST travel round=%1 movers=%2 cover=%3 (sum of 0.5 s native displacements)", m_iFMRound, m_fMoverTravel, m_fCoverTravel);
  }
  else CheckFM("off switch produces no coordinated requests", BBB_FireManeuver.s_iRequests == m_iRequests);
  BBB_FireManeuver.Stop();
  Clear(m_Lane);
  m_Lane = null;
  m_iFMRound++;
  m_iFinished++;
  if (m_iFMRound == 4 || m_bProbe) FinishFM("");
  else StartRound();
 }
 protected void PlaceOnNativeCover(SCR_AIGroup group)
 {
  ChimeraCoverManagerComponent covers = ChimeraCoverManagerComponent.Cast(GetGame().GetAIWorld().FindComponent(ChimeraCoverManagerComponent));
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  vector center = m_Lane.m_vFrom;
  int placed;
  foreach (AIAgent agent : agents)
  {
   AIPathfindingComponent pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!pathing || !utility || !covers) continue;
   CoverQueryProperties query = new CoverQueryProperties();
   query.m_vSectorPos = center;
   query.m_vThreatPos = m_Lane.m_vTo + Vector(0, 0.8, 0);
   query.m_fSectorDistMin = 0;
   query.m_fSectorDistMax = 60;
   query.m_iMaxCoversToCheck = 25;
   query.m_vNearestPolyHalfExtend = Vector(1, 2, 1);
   query.m_fNmAreaCostScale = 0.333;
   query.m_bCheckVisibility = true;
   query.m_fCoverToThreatAngleCosMin = 0.5;
   vector at, tall;
   int tileX, tileY, id;
   if (!covers.GetBestCover("Soldiers", pathing, query, at, tall, tileX, tileY, id)) continue;
   agent.GetControlledEntity().SetOrigin(at);
   utility.m_CombatMoveState.AssignCover(new SCR_AICoverLock(tileX, tileY, id, at, tall));
   utility.m_CombatMoveState.m_bInCover = true;
   utility.m_CombatMoveState.m_bExposedInCover = true;
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   SCR_AIStanceHandling.SetStance(character.GetCharacterController(), ECharacterStance.CROUCH);
   placed++;
  }
  PrintFormat("[BBB] FIRETEST initial real native cover posts=%1", placed);
 }

 protected void ProbeCover(SCR_AIGroup group)
 {
  AIAgent leader = group.GetLeaderAgent();
  IEntity body = leader.GetControlledEntity();
  AIPathfindingComponent pathing = AIPathfindingComponent.Cast(body.FindComponent(AIPathfindingComponent));
  bool bodyPath = pathing != null;
  if (!pathing) pathing = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
  ChimeraCoverManagerComponent covers = ChimeraCoverManagerComponent.Cast(GetGame().GetAIWorld().FindComponent(ChimeraCoverManagerComponent));
  if (!pathing || !covers) { PrintFormat("[BBB] FIRETEST cover components bodyPath=%1 path=%2 manager=%3", bodyPath, pathing != null, covers != null); return; }
  CoverQueryProperties query = new CoverQueryProperties();
  query.m_vSectorPos = body.GetOrigin();
  query.m_vThreatPos = body.GetOrigin() + m_Lane.m_vDir * 100 + Vector(0, 1, 0);
  query.m_fSectorDistMin = 5;
  query.m_fSectorDistMax = 35;
  query.m_iMaxCoversToCheck = 25;
  query.m_vNearestPolyHalfExtend = Vector(1, 2, 1);
  query.m_fNmAreaCostScale = 0.333;
  query.m_bCheckVisibility = false;
  vector coverPos, tall;
  int tileX, tileY, id;
  bool found = covers.GetBestCover("Soldiers", pathing, query, coverPos, tall, tileX, tileY, id);
  PrintFormat("[BBB] FIRETEST native cover found=%1 position=%2 bodyPath=%3 from=%4", found, coverPos, bodyPath, body.GetOrigin());
 }

 protected void Guards(SCR_AIUtilityComponent utility)
 {
  m_bGuards = true;
  BBB_FireManeuver.s_bOn = false;
  CheckFM("off switch releases role restrictions immediately", BBB_FireManeuver.Decision(utility) == 0);
  BBB_FireManeuver.s_bOn = true;
  SCR_AIGroup group = SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
  SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
  EAIGroupCombatMode oldMode = groupUtility.GetCombatModeExternal();
  groupUtility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  groupUtility.EvaluateCombatMode();
  CheckFM("hold fire releases coordination", BBB_FireManeuver.Decision(utility) == 0);
  groupUtility.SetCombatMode(oldMode);
  groupUtility.EvaluateCombatMode();
  SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
  float level = behavior.EvaluatePriorityLevel();
  behavior.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  CheckFM("direct behavior priority wins", BBB_FireManeuver.Decision(utility) == 0);
  behavior.SetPriorityLevel(level);
 }
 protected void FinishFM(string reason)
 {
  GetGame().GetCallqueue().Remove(TickFM);
  BBB_FireManeuver.Stop();
  if (m_Lane) Clear(m_Lane);
  BBB_FireManeuver.s_bOn = m_bSaved;
  string verdict = "PASS";
  int expected = 15;
  int rounds = 4;
  if (m_bProbe) { expected = 7; rounds = 1; }
  if (!reason.IsEmpty() || m_iFinished != rounds || m_iFail > 0 || m_iPass != expected) verdict = "FAIL";
  PrintFormat("[BBB] FIRETEST results shots=%1 losses=%2 (two pairs are coverage, not a survival benefit claim)", m_Shots, m_Losses);
  PrintFormat("[ACR] VERDICT %1 test=bbbfiremaneuver passed=%2 failed=%3 expected=%4 rounds=%5 reason=%6", verdict, m_iPass, m_iFail, expected, m_iFinished, reason);
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_FireManeuverTest.MaybeStartFM();
 }
}
