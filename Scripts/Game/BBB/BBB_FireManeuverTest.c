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
 protected bool m_bSaved, m_bGuards, m_bProbe, m_bDeletion, m_bLifecycle;
 protected ref map<IEntity, vector> m_Positions = new map<IEntity, vector>();
 protected float m_fMoverTravel, m_fCoverTravel;
 protected int m_iCoverFire;
 protected ref BBB_Hooks m_RestartHooks;
 protected IEntity m_RestartMedic;
 protected bool m_bRestartStarted;
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
   if (!m_bLifecycle) LifecycleChecks(m_Lane.m_A.m_aMen[0]);
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
  // Delete a second tracked group while a real native combat job is active.
  // Retain the job value across entity deletion to exercise the decayed key.
  BBB_FireTeamJob activeJob = BBB_FireManeuver.Job(m_Lane.m_A.m_Group);
  if (!m_bDeletion && activeJob)
  {
   m_bDeletion = true;
   SCR_AIGroup doomed = SpawnSquad(false, m_Lane.m_vFrom, m_Lane.m_vDir, m_Lane, 0);
   ref BBB_FireTeamJob deadJob = new BBB_FireTeamJob();
   deadJob.m_Group = doomed;
   ref BBB_Combat pruning = new BBB_Combat();
   pruning.SeedPruneCheck(doomed, m_Lane.m_A.m_Group, Now() + 120);
   float deadline = Now() + 120;
   BBB_FireManeuver.SeedDeletionCheck(doomed, deadJob, m_Lane.m_A.m_Group, deadline);
   if (doomed) SCR_EntityHelper.DeleteEntityAndChildren(doomed);
   CheckFM("deleted group key decays", !doomed && !deadJob.m_Group);
   BBB_FireManeuver.Tick();
   CheckFM("deleted job pruned while native survivor stays active", BBB_FireManeuver.Job(m_Lane.m_A.m_Group) == activeJob && BBB_FireManeuver.DeletionJobCount() == 1);
   CheckFM("unrelated group cooldown survives deletion", BBB_FireManeuver.DeletionDeadline(m_Lane.m_A.m_Group) == deadline);
   CheckFM("deleted cooldown key removed by index", BBB_FireManeuver.DeletionCooldownCount() == 1);
   array<SCR_AIGroup> keptGroups = {m_Lane.m_A.m_Group};
   pruning.RunPruneCheck(keptGroups, Now());
   CheckFM("combat wake map drops deleted key and retains survivor", pruning.PruneWakeCount() == 1 && pruning.PruneWakeUntil(m_Lane.m_A.m_Group) == deadline);
   CheckFM("combat report map drops deleted key and retains survivor", pruning.PruneReportCount() == 1 && pruning.PruneHasReport(m_Lane.m_A.m_Group));
   keptGroups.Clear();
   pruning.RunPruneCheck(keptGroups, Now());
   CheckFM("combat maps drop live groups omitted by host", pruning.PruneWakeCount() == 0 && pruning.PruneReportCount() == 0);
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
 protected void LifecycleChecks(IEntity survivor)
 {
  m_bLifecycle = true;
  BBB_Medic.Stop();
  BBB_Drag.Stop();
  IEntity doomed = Spawn(RIFLE_US, m_Lane.m_vFrom, m_Lane.m_vDir);
  IEntity orphan = Spawn(RIFLE_US, m_Lane.m_vFrom, m_Lane.m_vDir);
  SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.FindOnControlledEntity(survivor);
  AIControlComponent control = AIControlComponent.Cast(survivor.FindComponent(AIControlComponent));
  SCR_AIUtilityComponent utility;
  if (control && control.GetAIAgent()) utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
  CheckFM("lifecycle fixture has native soldiers and settings", doomed != null && orphan != null && settings != null && utility != null);
  if (!doomed || !orphan || !settings || !utility) return;
  float nowMs = BBB_Medic.NowMs();
  array<IEntity> keys = {doomed, survivor};
  foreach (IEntity key : keys)
  {
   BBB_Medic.s_mHeld.Set(key, nowMs);
   BBB_Medic.s_mHot.Set(key, nowMs);
   BBB_Medic.s_mStartedBy.Set(key, 7);
   BBB_Medic.s_mDoneBy.Set(key, 8);
   BBB_Medic.s_mHitBy.Set(key, 9);
   BBB_Medic.s_aVanilla.Insert(key);
   BBB_Medic.MarkGoing(key, survivor);
   BBB_Drag.s_mLast.Set(key, nowMs);
   BBB_Drag.s_mRefusedLog.Set(key, nowMs);
  }
  BBB_Medic.MarkGoing(orphan, doomed);
  // A refused attempt starts pruning even without a drag job.
  BBB_Drag.s_mRefusedLog.Remove(survivor);
  BBB_Drag.Refused(survivor, survivor, "lifecycle fixture");
  ref BBB_DragJob lostJob = new BBB_DragJob();
  lostJob.m_Healer = doomed;
  lostJob.m_Casualty = survivor;
  lostJob.m_fStartMs = nowMs;
  BBB_Drag.s_aJobs.Insert(lostJob);
  SCR_EntityHelper.DeleteEntityAndChildren(doomed);
  CheckFM("deleted soldier keys and job pointers decay", !doomed && !lostJob.m_Healer);
  BBB_Medic.WatchGoing();
  BBB_Drag.Tick();
  CheckFM("small medic caches prune deleted keys and retain live values", BBB_Medic.s_mHeld.Count() == 1 && BBB_Medic.s_mHeld.Get(survivor) == nowMs && BBB_Medic.s_mHot.Count() == 1 && BBB_Medic.s_mHot.Get(survivor) == nowMs);
  CheckFM("healer tracking prunes deleted keys and deleted destinations", BBB_Medic.s_mGoing.Count() == 1 && BBB_Medic.s_mGoingTo.Count() == 1 && BBB_Medic.s_mGoingTo.Get(survivor) == survivor);
  CheckFM("medic counters and vanilla array prune deleted soldiers", BBB_Medic.s_mStartedBy.Count() == 1 && BBB_Medic.s_mDoneBy.Count() == 1 && BBB_Medic.s_mHitBy.Count() == 1 && BBB_Medic.s_aVanilla.Count() == 1 && BBB_Medic.s_mHitBy.Get(survivor) == 9);
  CheckFM("drag caches retain live keys and release deleted healer job", BBB_Drag.s_mLast.Count() == 1 && BBB_Drag.s_mLast.Get(survivor) == nowMs && BBB_Drag.s_mRefusedLog.Count() == 1 && BBB_Drag.s_aJobs.IsEmpty());
  CheckFM("pruning timers remain active without a drag job", GetGame().GetCallqueue().GetRemainingTime(BBB_Medic.WatchGoing) >= 0 && GetGame().GetCallqueue().GetRemainingTime(BBB_Drag.Tick) >= 0);
  // Expired entries must also disappear below the former 32-entry threshold.
  BBB_Medic.s_mHeld.Set(survivor, nowMs - 120001);
  BBB_Drag.s_mLast.Set(survivor, nowMs - 120001);
  BBB_Drag.s_mRefusedLog.Set(survivor, nowMs - 120001);
  BBB_Medic.WatchGoing();
  BBB_Drag.Tick();
  CheckFM("expired small caches are pruned and idle drag watch stops", BBB_Medic.s_mHeld.IsEmpty() && BBB_Drag.s_mLast.IsEmpty() && BBB_Drag.s_mRefusedLog.IsEmpty() && !BBB_Drag.s_bTicking);
  // Exercise shutdown with real temporary settings and a native move action.
  ref BBB_DragJob activeDrag = new BBB_DragJob();
  activeDrag.m_Healer = survivor;
  activeDrag.m_Casualty = orphan;
  activeDrag.m_fStartMs = nowMs;
  activeDrag.m_Stance = SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, ECharacterStance.CROUCH);
  activeDrag.m_Speed = SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, EMovementType.WALK);
  settings.AddCharacterSetting(activeDrag.m_Stance, false);
  settings.AddCharacterSetting(activeDrag.m_Speed, false);
  activeDrag.m_Move = new BBB_AIDragMove(utility, null, survivor.GetOrigin(), BBB_Drag.MOVE_PRIORITY, 0, null, 1.0);
  utility.AddAction(activeDrag.m_Move);
  BBB_Drag.s_aJobs.Insert(activeDrag);
  BBB_Drag.Refused(survivor, survivor, "shutdown fixture");
  m_RestartHooks = BBB.s_Hooks;
  m_RestartMedic = survivor;
  m_bRestartStarted = BBB.IsStarted();
  BBB.Stop();
  array<SCR_AISettingBase> remaining = {};
  settings.GetAllSettings(remaining);
  CheckFM("BBB stop completes drag action and restores temporary settings", activeDrag.m_Move.GetActionState() == EAIActionState.COMPLETED && !remaining.Contains(activeDrag.m_Stance) && !remaining.Contains(activeDrag.m_Speed));
  CheckFM("BBB stop clears all medic and drag registries", BBB_Medic.s_mHeld.IsEmpty() && BBB_Medic.s_mHot.IsEmpty() && BBB_Medic.s_mGoing.IsEmpty() && BBB_Medic.s_mGoingTo.IsEmpty() && BBB_Medic.s_mStartedBy.IsEmpty() && BBB_Medic.s_mDoneBy.IsEmpty() && BBB_Medic.s_mHitBy.IsEmpty() && BBB_Medic.s_aVanilla.IsEmpty() && BBB_Drag.s_mLast.IsEmpty() && BBB_Drag.s_mRefusedLog.IsEmpty() && BBB_Drag.s_aJobs.IsEmpty());
  BBB.Stop();
  // A repeat added and stopped in this same dispatch may still be pending.
  // Allow both callback deadlines to pass and verify they cancel without work.
  GetGame().GetCallqueue().CallLater(FinishLifecycleTimers, 750, false);
  SCR_EntityHelper.DeleteEntityAndChildren(orphan);
 }

 protected void FinishLifecycleTimers()
 {
  int medicTimer = GetGame().GetCallqueue().GetRemainingTime(BBB_Medic.WatchGoing);
  int dragTimer = GetGame().GetCallqueue().GetRemainingTime(BBB_Drag.Tick);
  PrintFormat("[BBB] FIRETEST stopped timers medic=%1 drag=%2 watching=%3 ticking=%4", medicTimer, dragTimer, BBB_Medic.s_bWatching, BBB_Drag.s_bTicking);
  CheckFM("repeated BBB stop removes both scheduled callbacks", !BBB_Medic.s_bWatching && !BBB_Drag.s_bTicking && medicTimer < 0 && dragTimer < 0);
  BBB.Start(m_RestartHooks);
  BBB_Medic.MarkGoing(m_RestartMedic, m_RestartMedic);
  CheckFM("BBB restart can schedule medic tracking again", (!m_bRestartStarted || BBB.IsStarted()) && BBB_Medic.s_bWatching && GetGame().GetCallqueue().GetRemainingTime(BBB_Medic.WatchGoing) >= 0);
  BBB_Medic.Stop();
  BBB_Drag.Stop();
  m_RestartHooks = null;
  m_RestartMedic = null;
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
  GetGame().GetCallqueue().Remove(FinishLifecycleTimers);
  BBB_FireManeuver.Stop();
  if (m_Lane) Clear(m_Lane);
  BBB_FireManeuver.s_bOn = m_bSaved;
  string verdict = "PASS";
  int expected = 34;
  int rounds = 4;
  if (m_bProbe) { expected = 26; rounds = 1; }
  if (!reason.IsEmpty() || m_iFinished != rounds || m_iFail > 0 || m_iPass != expected) verdict = "FAIL";
  PrintFormat("[BBB] FIRETEST results shots=%1 losses=%2 (two pairs are coverage, not a survival benefit claim)", m_Shots, m_Losses);
  PrintFormat("[ACR] VERDICT %1 test=bbbfiremaneuver passed=%2 failed=%3 expected=%4 rounds=%5 reason=%6", verdict, m_iPass, m_iFail, expected, m_iFinished, reason);
  Print("[ACR] TEST COMPLETE");
 }
}

// Test-only access to protected registries; no production seeding API.
modded class BBB_FireManeuver
{
 static void SeedDeletionCheck(SCR_AIGroup doomed, BBB_FireTeamJob job, SCR_AIGroup survivor, float deadline)
 {
  s_Jobs.Set(doomed, job);
  s_Next.Set(doomed, deadline);
  s_Next.Set(survivor, deadline);
 }
 static int DeletionJobCount() { return s_Jobs.Count(); }
 static int DeletionCooldownCount() { return s_Next.Count(); }
 static float DeletionDeadline(SCR_AIGroup group) { return s_Next.Get(group); }
}

modded class BBB_Combat
{
 void SeedPruneCheck(SCR_AIGroup doomed, SCR_AIGroup survivor, float deadline)
 {
  m_ActiveUntil.Set(doomed, deadline);
  m_ActiveUntil.Set(survivor, deadline);
  ref array<ref BBB_CombatContact> deadReports = {};
  ref array<ref BBB_CombatContact> liveReports = {};
  BBB_CombatContact report = new BBB_CombatContact();
  report.m_fReportedAt = GetGame().GetWorld().GetWorldTime() / 1000;
  deadReports.Insert(report);
  liveReports.Insert(report);
  m_Reports.Set(doomed, deadReports);
  m_Reports.Set(survivor, liveReports);
 }
 void RunPruneCheck(array<SCR_AIGroup> groups, float now) { DetailTick(groups, now); PruneReports(groups, now); }
 int PruneWakeCount() { return m_ActiveUntil.Count(); }
 float PruneWakeUntil(SCR_AIGroup group) { return m_ActiveUntil.Get(group); }
 int PruneReportCount() { return m_Reports.Count(); }
 bool PruneHasReport(SCR_AIGroup group) { return m_Reports.Contains(group); }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_FireManeuverTest.MaybeStartFM();
 }
}
