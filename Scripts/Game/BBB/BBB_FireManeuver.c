// Two logical fireteams inside an unchanged native group. Only ordinary attack
// combat moves are coordinated; native danger, weapons, firing and FF run first.
class BBB_FireTeamJob : Managed
{
 SCR_AIGroup m_Group;
 AIWaypoint m_Waypoint;
 vector m_WaypointPos, m_Threat, m_Center;
 ref array<SCR_AIUtilityComponent> m_Members = {};
 ref array<SCR_AIUtilityComponent> m_Reached = {};
 int m_iCoverTeam, m_iPhase, m_iArrived;
 float m_fStarted, m_fPhaseSince, m_fCoverFire, m_fLog, m_fFresh;
 bool m_bMoving;
}

class BBB_FireManeuver
{
 static bool s_bRead;
 static bool s_bOn;
 static int s_iStarted, s_iSwaps, s_iRequests, s_iArrived, s_iNoCover, s_iCoverShots;
 static int s_iTicks, s_iTickMs, s_iTickMax, s_iDecisionMs, s_iEventMs;
 protected static bool s_bInTick;
 protected static ref map<SCR_AIGroup, ref BBB_FireTeamJob> s_Jobs = new map<SCR_AIGroup, ref BBB_FireTeamJob>();
 protected static ref map<SCR_AIGroup, float> s_Next = new map<SCR_AIGroup, float>();

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbFireManeuver", value)) s_bOn = value == "1";
  PrintFormat("[BBB] FIRE MANEUVER enabled=%1", s_bOn);
 }

 // Resolve readiness from actual equipped native selection, not prefab names.
 static bool Ready(SCR_AIUtilityComponent utility)
 {
  if (!utility || !utility.m_CombatComponent || !utility.m_CombatMoveState || BBB_OrderIntent.Blocks(utility)) return false;
  SCR_AICombatComponent combat = utility.m_CombatComponent;
  SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
  if (!SCR_AIAttackBehavior.Cast(behavior) && !SCR_AISuppressBehavior.Cast(behavior)) return false;
  if (behavior.EvaluatePriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER || combat.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) return false;
  if (!utility.m_AIInfo || utility.m_AIInfo.HasUnitState(EUnitState.IN_VEHICLE) || utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS)) return false;
  BaseWeaponComponent weapon = combat.GetCurrentWeapon();
  if (!weapon || (weapon.GetWeaponType() != EWeaponType.WT_RIFLE && weapon.GetWeaponType() != EWeaponType.WT_MACHINEGUN) || !weapon.GetCurrentMagazine() || weapon.GetCurrentMagazine().GetAmmoCount() <= 0) return false;
  if (SCR_AIAttackBehavior.Cast(behavior) && combat.BBBSelectedWeapon() != weapon) return false;
  ChimeraCharacter body = ChimeraCharacter.Cast(utility.m_OwnerEntity);
  if (!body || !body.GetCharacterController() || !BBB.IsAlive(body) || body.GetCharacterController().IsPlayerControlled() || body.IsInVehicle()) return false;
  CharacterControllerComponent controller = body.GetCharacterController();
  if (controller.IsReloading() || controller.IsChangingItem() || controller.IsClimbing() || controller.IsSwimming() || controller.GetDisableWeaponControls()) return false;
  if (utility.m_ThreatSystem && utility.m_ThreatSystem.GetThreatMeasure() >= 0.66) return false;
  return !BBB_Drag.Busy(body);
 }

 static bool Fresh(SCR_AIUtilityComponent utility, out vector known)
 {
  if (!Ready(utility)) return false;
  SCR_AICombatComponent combat = utility.m_CombatComponent;
  array<BaseTarget> targets = {};
  if (combat.GetCurrentTarget()) targets.Insert(combat.GetCurrentTarget());
  if (SCR_AISuppressBehavior.Cast(utility.GetCurrentBehavior()) && utility.m_PerceptionComponent)
   utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
  foreach (BaseTarget target : targets)
  {
   if (!target || target.GetTargetCategory() != ETargetCategory.ENEMY || target.GetUnitType() != EAIUnitType.UnitType_Infantry || target.IsDisarmed()) continue;
   if (target.GetTimeSinceSeen() > 3 || target.GetDistance() < 45 || target.GetDistance() > 300 || !combat.IsTargetVisible(target) || target.GetTraceFraction() < 0.5) continue;
   known = target.GetLastSeenPosition();
   return true;
  }
  return false;
 }

 static bool Detected(SCR_AIUtilityComponent utility, out vector known)
 {
  if (!utility || !utility.m_PerceptionComponent || !utility.m_OwnerEntity) return false;
  array<BaseTarget> targets = {};
  utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
  foreach (BaseTarget target : targets)
  {
   if (!target || target.GetUnitType() != EAIUnitType.UnitType_Infantry || target.IsDisarmed() || target.GetTimeSinceDetected() < 0 || target.GetTimeSinceDetected() > 5) continue;
   vector report = target.GetLastDetectedPosition();
   float distance = vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), report);
   if (distance < 45 || distance > 300) continue;
   known = report;
   return true;
  }
  return false;
 }

 static bool Known(SCR_AIUtilityComponent utility, BBB_FireTeamJob job)
 {
  if (!Ready(utility)) return false;
  BaseTarget target = utility.m_CombatComponent.GetCurrentTarget();
  SCR_AISuppressBehavior suppression = SCR_AISuppressBehavior.Cast(utility.GetCurrentBehavior());
  if (suppression && suppression.m_SuppressionVolume.m_Value && GetGame().GetWorld().GetWorldTime() / 1000 - job.m_fFresh <= 6)
   return vector.DistanceXZ(suppression.m_SuppressionVolume.m_Value.GetCenterPosition(), job.m_Threat) <= 70;
  if (!target || target.GetTargetCategory() != ETargetCategory.ENEMY || target.GetUnitType() != EAIUnitType.UnitType_Infantry || target.IsDisarmed() || target.GetTimeSinceDetected() > 10 || target.GetDistance() < 45 || target.GetDistance() > 300) return false;
  // A moving rifleman may lose his own sight briefly; covering guns still need
  // personal sight or a fresh native suppression volume and actual fire.
  // Only native remembered positions are used.
  return vector.DistanceXZ(target.GetLastDetectedPosition(), job.m_Threat) <= 70;
 }

 protected static bool SuppressionCover(SCR_AIUtilityComponent utility, BBB_FireTeamJob job, float now)
 {
  if (!Ready(utility) || now - job.m_fFresh > 3 || utility.m_fBBBLastFire < now - 3) return false;
  SCR_AISuppressBehavior action = SCR_AISuppressBehavior.Cast(utility.GetCurrentBehavior());
  return action && action.m_SuppressionVolume.m_Value && vector.DistanceXZ(action.m_SuppressionVolume.m_Value.GetCenterPosition(), job.m_Threat) <= 70;
 }

 static bool GroupAllowed(SCR_AIGroup group)
 {
  if (!group || !group.GetGroupUtilityComponent() || !BBB.s_Hooks.AllowFireManeuver(group)) return false;
  SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
  if (groupUtility.GetCombatModeExternal() != EAIGroupCombatMode.FIRE_AT_WILL) return false;
  SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(group.GetCurrentWaypoint());
  if (waypoint)
  {
   if (waypoint.GetPriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return false;
   if (waypoint.GetPrefabData())
   {
    string path = waypoint.GetPrefabData().GetPrefabName();
    if (path.Contains("Defend") || path.Contains("Garrison") || path.Contains("Suppress") || path.Contains("GetIn") || path.Contains("GetOut") || path.Contains("Patrol") || path.Contains("Follow") || path.Contains("Artillery")) return false;
   }
  }
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!member) continue;
   if (!member.GetCharacterController() || member.GetCharacterController().IsPlayerControlled() || member.IsInVehicle()) return false;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (utility && utility.GetCurrentBehavior() && utility.GetCurrentBehavior().EvaluatePriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return false;
  }
  return true;
 }

 static BBB_FireTeamJob Job(SCR_AIGroup group) { return s_Jobs.Get(group); }

 static void Tick()
 {
  int started = System.GetTickCount();
  s_bInTick = true;
  TickGroups();
  s_bInTick = false;
  if (BBB_Perf.s_bOn)
  {
   int elapsed = System.GetTickCount() - started;
   s_iTicks++;
   s_iTickMs += elapsed;
   s_iTickMax = Math.Max(s_iTickMax, elapsed);
  }
 }
 protected static void TickGroups()
 {
  Read();
  if (!s_bOn) { Stop(); return; }
  array<SCR_AIGroup> groups = {};
  BBB.s_Hooks.GetGroups(groups);
  float now = GetGame().GetWorld().GetWorldTime() / 1000;
  for (int j = s_Jobs.Count() - 1; j >= 0; j--)
  {
   SCR_AIGroup existing = s_Jobs.GetKey(j);
   if (!existing)
   {
    BBB_FireTeamJob deadJob = s_Jobs.GetElement(j);
    if (deadJob) { StopMoves(deadJob); ClearMembers(deadJob); }
    s_Jobs.RemoveElement(j);
    continue;
   }
   if (!groups.Contains(existing) || !s_bOn || !GroupAllowed(existing)) End(existing, now);
  }
  for (int c = s_Next.Count() - 1; c >= 0; c--)
   if (!s_Next.GetKey(c) || !groups.Contains(s_Next.GetKey(c))) s_Next.RemoveElement(c);
  if (!s_bOn) return;
  foreach (SCR_AIGroup group : groups)
  {
   BBB_FireTeamJob job = s_Jobs.Get(group);
   if (!job)
   {
    if (now < s_Next.Get(group) || !GroupAllowed(group)) continue;
    job = Start(group, now);
   }
   if (job) Update(job, now);
  }
 }

 protected static BBB_FireTeamJob Start(SCR_AIGroup group, float now)
 {
  array<AIAgent> agents = {};
  BBB.s_Hooks.GetMembers(group, agents);
  ref BBB_FireTeamJob job = new BBB_FireTeamJob();
  vector seen;
  int fresh;
  SCR_AIUtilityComponent mg;
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility || !utility.IsMilitary() || !utility.m_OwnerEntity || !BBB.IsAlive(utility.m_OwnerEntity)) continue;
   ChimeraCharacter soldier = ChimeraCharacter.Cast(utility.m_OwnerEntity);
   if (!soldier || soldier.IsInVehicle() || soldier.GetCharacterController().IsUnconscious() || soldier.GetCharacterController().IsPlayerControlled()) continue;
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
   array<IEntity> carried = {};
   if (weapons) weapons.GetWeaponsList(carried);
   bool armed;
   bool hasMG;
   foreach (IEntity item : carried)
   {
    BaseWeaponComponent primary = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
    if (!primary) continue;
    if (primary.GetWeaponType() == EWeaponType.WT_RIFLE || primary.GetWeaponType() == EWeaponType.WT_MACHINEGUN) armed = true;
    if (primary.GetWeaponType() == EWeaponType.WT_MACHINEGUN) hasMG = true;
   }
   if (!armed) continue;
   vector known;
   if (Detected(utility, known)) { if (fresh == 0) seen = known; fresh++; }
   job.m_Members.Insert(utility);
   if (hasMG) mg = utility;
  }
  if (job.m_Members.Count() < 4 || fresh < 2) return null;
  // MG starts in the covering half; the same members retain team identity.
  if (mg) { job.m_Members.RemoveItem(mg); job.m_Members.InsertAt(mg, 0); }
  job.m_Group = group;
  job.m_Threat = seen;
  job.m_Waypoint = group.GetCurrentWaypoint();
  if (job.m_Waypoint) job.m_WaypointPos = job.m_Waypoint.GetOrigin();
  job.m_Center = BBB.AnchorOf(group).GetOrigin();
  job.m_fStarted = now;
  job.m_fFresh = now;
  job.m_fPhaseSince = now;
  foreach (int index, SCR_AIUtilityComponent member : job.m_Members)
  {
   member.m_BBBFireJob = job;
   member.m_CombatMoveState.m_BBBFireOwner = member;
   member.m_iBBBFireTeam = 1;
   if (index < job.m_Members.Count() / 2) member.m_iBBBFireTeam = 0;
   member.m_iBBBFirePhase = -1;
   member.m_iBBBFireArrivedPhase = -1;
   member.m_bBBBFireGeometry = false;
   member.m_bBBBGeometryTried = false;
   member.m_bBBBFlankRetried = false;
   member.m_iBBBFlankSide = 1;
   member.m_BBBFireFailureHandled = null;
   member.m_fBBBLastFire = 0;
   member.m_iBBBLastRounds = -1;
  }
  s_Jobs.Set(group, job);
  s_iStarted++;
  PrintFormat("[BBB] FIRE MANEUVER %1: %2 + %3, MG cover=%4", BBB.s_Hooks.NameOf(group), job.m_Members.Count() / 2, job.m_Members.Count() - job.m_Members.Count() / 2, mg != null);
  return job;
 }

 protected static void Update(BBB_FireTeamJob job, float now)
 {
  AIWaypoint waypoint = job.m_Group.GetCurrentWaypoint();
  if (waypoint != job.m_Waypoint || (waypoint && vector.DistanceXZ(waypoint.GetOrigin(), job.m_WaypointPos) > 1) || now - job.m_fStarted > 60) { End(job.m_Group, now); return; }
  int coverReady, moveReady, nextCoverReady;
  bool arrivedCoverFire;
  bool coverMG;
  bool nextCoverFire, nextCoverMG;
  bool fresh;
  int alive;
  array<AIAgent> roster = {};
  BBB.s_Hooks.GetMembers(job.m_Group, roster);
  foreach (SCR_AIUtilityComponent utility : job.m_Members)
  {
   if (!utility || !BBB.IsAlive(utility.m_OwnerEntity) || !roster.Contains(utility.GetAIAgent())) { if (System.IsCLIParam("acrBbbFireManeuverTest")) Print("[BBB] FIRE END roster or death"); End(job.m_Group, now); return; }
   alive++;
   if (utility.m_BBBFireMove && utility.m_BBBFireMove.m_eState == SCR_EAICombatMoveRequestState.FAIL)
    Failed(utility, utility.m_BBBFireMove, utility.m_BBBFireMove.m_eFailReason);
   // Native non-cover movement can remain executing after physically reaching
   // the goal, then be canceled when support pauses. Observe the actual arrival
   // before that cancellation instead of depending on the completion callback.
   if (utility.m_BBBFireMove && utility.m_bBBBFireDestination && utility.m_CombatMoveState.GetRequest() == utility.m_BBBFireMove)
    Arrived(utility, utility.m_BBBFireMove);
   SampleFire(utility, now);
   // A reload may legitimately hide the gun in prone. Once ready again, a
   // verified physical firing aperture needs its crouched stance restored.
   // Do not interrupt an executing native danger or FF request.
   if (!utility.m_CombatMoveState.IsExecutingRequest() && ExposeGeometry(utility))
   {
    ChimeraCharacter settledBody = ChimeraCharacter.Cast(utility.m_OwnerEntity);
    if (settledBody.GetCharacterController().GetStance() == ECharacterStance.PRONE)
     utility.m_CombatMoveState.ApplyRequestChangeStanceOutsideCover(ECharacterStance.CROUCH);
   }
   vector known;
   vector report;
   if (Detected(utility, report) && vector.DistanceXZ(report, job.m_Threat) <= 70) fresh = true;
   bool personallySeen = Fresh(utility, known);
   bool visible = personallySeen || SuppressionCover(utility, job, now);
   if (!Known(utility, job)) continue;
   if (utility.m_iBBBFireTeam == job.m_iCoverTeam)
   {
    if (visible && !utility.m_CombatMoveState.IsMoving() && utility.m_CombatMoveState.m_fTimerStopped_s >= 0.5)
    {
     coverReady++;
     if (utility.m_CombatComponent.GetCurrentWeapon().GetWeaponType() == EWeaponType.WT_MACHINEGUN) coverMG = true;
     if (utility.m_fBBBLastFire >= now - 3) job.m_fCoverFire = now;
    }
   }
   else if (vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), job.m_Center) < 60)
   {
    moveReady++;
    if (visible && !utility.m_CombatMoveState.IsMoving() && utility.m_CombatMoveState.m_fTimerStopped_s >= 0.5)
    {
     nextCoverReady++;
     if (utility.m_CombatComponent.GetCurrentWeapon().GetWeaponType() == EWeaponType.WT_MACHINEGUN) nextCoverMG = true;
     if (utility.m_fBBBLastFire >= now - 3)
     {
      nextCoverFire = true;
      if (job.m_Reached.Contains(utility) && vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), utility.m_vBBBFireDestination) <= 2.5) arrivedCoverFire = true;
     }
    }
   }
  }
  if (fresh) job.m_fFresh = now;
  if (now - job.m_fFresh > 6 || alive < 4) { End(job.m_Group, now); return; }
  if ((System.IsCLIParam("acrBbbFireManeuverTest") || System.IsCLIParam("acrBbbFireManeuverProbeTest")) && now >= job.m_fLog)
  {
   PrintFormat("[BBB] FIRE STATE cover=%1 ready=%2 movers=%3 arrivals=%4 fireAge=%5 moving=%6", job.m_iCoverTeam, coverReady, moveReady, job.m_iArrived, now - job.m_fCoverFire, job.m_bMoving);
   foreach (SCR_AIUtilityComponent observer : job.m_Members)
   {
    BaseTarget selected = observer.m_CombatComponent.GetCurrentTarget();
    if (selected) PrintFormat("[BBB] FIRE MEMBER team=%1 ready=%2 visible=%3 trace=%4 seen=%5 dist=%6 moving=%7 stopped=%8 threat=%9", observer.m_iBBBFireTeam, Ready(observer), observer.m_CombatComponent.IsTargetVisible(selected), selected.GetTraceFraction(), selected.GetTimeSinceSeen(), selected.GetDistance(), observer.m_CombatMoveState.IsMoving(), observer.m_CombatMoveState.m_fTimerStopped_s, observer.m_ThreatSystem.GetThreatMeasure());
   }
   foreach (SCR_AIUtilityComponent boundMember : job.m_Members)
   {
    if (!boundMember.m_BBBFireMove) continue;
    SCR_AICombatMoveRequestBase current = boundMember.m_CombatMoveState.GetRequest();
    string currentReason = "none";
    if (current) currentReason = typename.EnumToString(SCR_EAICombatMoveReason, current.m_eReason);
    PrintFormat("[BBB] FIRE REQUEST state=%1 failure=%2 current=%3 own=%4 action=%5", typename.EnumToString(SCR_EAICombatMoveRequestState, boundMember.m_BBBFireMove.m_eState), typename.EnumToString(SCR_EAICombatMoveRequestFailReason, boundMember.m_BBBFireMove.m_eFailReason), currentReason, current == boundMember.m_BBBFireMove, boundMember.GetCurrentBehavior().Type());
   }
   job.m_fLog = now + 5;
  }
  bool supported = (coverReady >= 1) && job.m_fCoverFire > 0 && now - job.m_fCoverFire <= 3;
  if (!job.m_bMoving && !supported && nextCoverReady >= 1 && nextCoverFire && (job.m_iArrived == 0 || arrivedCoverFire))
  {
   int reachedNext;
   foreach (SCR_AIUtilityComponent settled : job.m_Reached)
    if (settled && settled.m_iBBBFireTeam != job.m_iCoverTeam) reachedNext++;
   job.m_iCoverTeam = 1 - job.m_iCoverTeam;
   // This same readiness fallback also handles loss of the previous covering
   // guns after a completed bound. Count/clear that verified handoff once.
   if (reachedNext >= 1 && arrivedCoverFire)
   {
    s_iSwaps++;
    job.m_iPhase++;
    job.m_iArrived = 0;
    job.m_Reached.Clear();
    PrintFormat("[BBB] FIRE MANEUVER %1: verified support handoff, phase %2", BBB.s_Hooks.NameOf(job.m_Group), job.m_iPhase);
   }
   job.m_fCoverFire = now;
   coverReady = nextCoverReady;
   supported = true;
   moveReady = 0;
   foreach (SCR_AIUtilityComponent reserve : job.m_Members)
    if (reserve.m_iBBBFireTeam != job.m_iCoverTeam && Known(reserve, job)) moveReady++;
  }
  if (job.m_bMoving && !supported)
  {
   // Loss of the covering guns immediately returns movers to native safety.
   StopMoves(job);
   job.m_bMoving = false;
   job.m_iPhase++;
   job.m_fPhaseSince = now;
  }
  if (!job.m_bMoving && supported && moveReady >= 2 && now - job.m_fPhaseSince >= 3)
  {
   job.m_bMoving = true;
   job.m_fPhaseSince = now;
   PrintFormat("[BBB] FIRE MANEUVER %1: team %2 covers, team %3 bounds", BBB.s_Hooks.NameOf(job.m_Group), job.m_iCoverTeam, 1 - job.m_iCoverTeam);
  }
  if (job.m_bMoving && now - job.m_fPhaseSince >= 10)
  {
   // Handoff needs a gun firing from its verified arrival, rather than waiting
   // for a second soldier who may be reloading or reacting to danger.
   if (job.m_iArrived < 1 || nextCoverReady < 1 || !arrivedCoverFire)
   {
    if (now - job.m_fPhaseSince >= 30) End(job.m_Group, now);
    return;
   }
   job.m_iCoverTeam = 1 - job.m_iCoverTeam;
   job.m_iPhase++;
   job.m_iArrived = 0;
   job.m_Reached.Clear();
   job.m_bMoving = false;
   job.m_fCoverFire = 0;
   job.m_fPhaseSince = now;
   s_iSwaps++;
   PrintFormat("[BBB] FIRE MANEUVER %1: roles switch, phase %2", BBB.s_Hooks.NameOf(job.m_Group), job.m_iPhase);
  }
 }

 protected static void SampleFire(SCR_AIUtilityComponent utility, float now)
 {
  SCR_AICombatComponent combat = utility.m_CombatComponent;
  if (!combat) return;
  BaseWeaponComponent weapon = combat.GetCurrentWeapon();
  BaseMagazineComponent magazine;
  if (weapon) magazine = weapon.GetCurrentMagazine();
  if (!magazine) return;
  int rounds = magazine.GetAmmoCount();
  if (magazine == utility.m_BBBFireMagazine && utility.m_iBBBLastRounds > rounds)
  {
   utility.m_fBBBLastFire = now;
   if (utility.m_BBBFireJob && utility.m_iBBBFireTeam == utility.m_BBBFireJob.m_iCoverTeam) s_iCoverShots += utility.m_iBBBLastRounds - rounds;
  }
  utility.m_BBBFireMagazine = magazine;
  utility.m_iBBBLastRounds = rounds;
 }

 // Geometry cover has no native cover lock. Keep its verified crouched firing
 // aperture instead of letting ordinary outside-cover stance hide it in prone.
 // Native danger/FF conditions run before this stance branch; pins/reloads drop
 // readiness and restore normal stance choices immediately.
 static bool ExposeGeometry(SCR_AIUtilityComponent utility)
 {
  if (!s_bOn || !utility || !utility.m_BBBFireJob || !utility.m_bBBBFireGeometry) return false;
  BBB_FireTeamJob job = utility.m_BBBFireJob;
  if (!job.m_Reached.Contains(utility) || vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), utility.m_vBBBFireDestination) > 2.5) return false;
  // Full roster/host eligibility already runs once per group in TickGroups.
  // Keep immediate intent checks here, like the ordinary-movement decision.
  if (!job.m_Group || job.m_Group.GetCurrentWaypoint() != job.m_Waypoint || job.m_Group.GetGroupUtilityComponent().GetCombatModeExternal() != EAIGroupCombatMode.FIRE_AT_WILL) return false;
  SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(job.m_Waypoint);
  if (waypoint && waypoint.GetPriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return false;
  if (!Known(utility, job)) return false;
  return BBB_FiringCover.Protected(utility.m_OwnerEntity.GetOrigin(), job.m_Threat);
 }

 static int Decision(SCR_AIUtilityComponent utility)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  int result = Decide(utility);
  if (BBB_Perf.s_bOn)
  {
   int elapsed = System.GetTickCount() - started;
   s_iDecisionMs += elapsed;
   BBB_Perf.Add("ev.fireManeuver", elapsed);
  }
  return result;
 }
 protected static int Decide(SCR_AIUtilityComponent utility)
 {
  Read();
  if (!utility) return 0;
  BBB_FireTeamJob job = utility.m_BBBFireJob;
  if (!s_bOn || !job || !Ready(utility) || !job.m_Group) return 0;
  // Group roster/host intent is validated once per tick. Native order changes
  // and this soldier's direct command are checked immediately without N^2 scans.
  if (job.m_Group.GetCurrentWaypoint() != job.m_Waypoint || job.m_Group.GetGroupUtilityComponent().GetCombatModeExternal() != EAIGroupCombatMode.FIRE_AT_WILL) return 0;
  SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(job.m_Waypoint);
  if (waypoint && waypoint.GetPriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_PLAYER) return 0;
  if (utility.m_iBBBFireTeam == job.m_iCoverTeam)
  {
   float now = GetGame().GetWorld().GetWorldTime() / 1000;
   if (!job.m_bMoving && now - job.m_fPhaseSince > 6) return 0;
   vector visible;
   if ((Fresh(utility, visible) && vector.DistanceXZ(visible, job.m_Threat) <= 70) || SuppressionCover(utility, job, GetGame().GetWorld().GetWorldTime() / 1000)) return -1;
   return 0;
  }
  if (!Known(utility, job) || vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), job.m_Center) >= 60) return 0;
  // A rifleman who reached cover gets time to become the next covering gun,
  // rather than immediately taking another ordinary native combat move.
  if (job.m_Reached.Contains(utility))
  {
   if (ExposeGeometry(utility)) return -1;
   vector sight;
   if ((Fresh(utility, sight) && vector.DistanceXZ(sight, job.m_Threat) <= 70) || SuppressionCover(utility, job, GetGame().GetWorld().GetWorldTime() / 1000)) return -1;
  }
  if (!job.m_bMoving) return 0;
  if (utility.m_iBBBFirePhase != job.m_iPhase && !utility.m_CombatMoveState.IsExecutingRequest() && utility.m_CombatMoveState.m_fTimerStopped_s >= 2) return 1;
  return 0;
 }

 static void Configure(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequest_Move request, bool committed = false)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  DoConfigure(utility, request, committed);
  if (BBB_Perf.s_bOn && !s_bInTick)
  {
   int elapsed = System.GetTickCount() - started;
   s_iEventMs += elapsed;
   BBB_Perf.Add("ev.fireManeuverWork", elapsed);
  }
 }
 protected static void DoConfigure(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequest_Move request, bool committed)
 {
  if (!request || request.m_eReason != SCR_EAICombatMoveReason.STANDARD || !s_bOn || !utility.m_BBBFireJob) return;
  if (!committed && Decision(utility) != 1) return;
  BBB_FireTeamJob job = utility.m_BBBFireJob;
  vector at = utility.m_OwnerEntity.GetOrigin();
  vector forward = job.m_Threat - at;
  forward[1] = 0;
  forward.Normalize();
  vector side = Vector(-forward[2], 0, forward[0]);
  // Stable flank side for the whole group, including after the role switch.
  if (job.m_Center[0] > job.m_Threat[0]) side = -side;
  if (utility.m_iBBBFlankSide < 0) side = -side;
  request.m_vTargetPos = job.m_Threat + Vector(0, 1.4, 0);
  request.m_vMovePos = BBB.Ground(at + side * 18 + forward * 8);
  request.m_eDirection = SCR_EAICombatMoveDirection.CUSTOM_POS;
  request.m_fCoverSearchDistMin = 5;
  request.m_fCoverSearchDistMax = 25;
  request.m_fCoverSearchSectorHalfAngleRad = 0.35 * Math.PI;
  request.m_bUseCoverSearchDirectivity = true;
  request.m_bTryFindCover = true;
  request.m_bCheckCoverVisibility = true;
  request.m_bFailIfNoCover = true;
  request.m_eMovementType = EMovementType.RUN;
  request.m_eStanceMoving = ECharacterStance.CROUCH;
  request.m_eStanceEnd = ECharacterStance.CROUCH;
  request.m_bAimAtTarget = true;
  request.m_bAimAtTargetEnd = true;
  request.m_fMoveDuration_s = 15;
  if (utility.m_bBBBFireGeometry)
  {
   if (!BBB_FiringCover.Protected(utility.m_vBBBFireGeometry, job.m_Threat)) return;
   request.m_vMovePos = utility.m_vBBBFireGeometry;
   request.m_bTryFindCover = false;
   request.m_bFailIfNoCover = false;
  }
  utility.m_iBBBFirePhase = job.m_iPhase;
  utility.m_BBBFireMove = request;
  utility.m_bBBBFireDestination = false;
  request.GetOnMovementStarted().Insert(MoveStarted);
  request.GetOnCompleted().Insert(Arrived);
  request.GetOnFailed().Insert(Failed);
  s_iRequests++;
 }

 protected static void MoveStarted(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequest_Move request, vector position, bool cover)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  DoMoveStarted(utility, request, position, cover);
  if (BBB_Perf.s_bOn && !s_bInTick)
  {
   int elapsed = System.GetTickCount() - started;
   s_iEventMs += elapsed;
   BBB_Perf.Add("ev.fireManeuverWork", elapsed);
  }
 }
 protected static void DoMoveStarted(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequest_Move request, vector position, bool cover)
 {
  if (utility.m_BBBFireMove != request || !utility.m_BBBFireJob) return;
  bool physicalCover = cover || (utility.m_bBBBFireGeometry && BBB_FiringCover.Protected(position, utility.m_BBBFireJob.m_Threat));
  if (!physicalCover || vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), position) < 5 || BBB_Terrain.IsWaterAt(position) || BBB_Terrain.WaterAlong(utility.m_OwnerEntity.GetOrigin(), position, 5) > 0)
  {
   SCR_AICombatMoveRequest_Stop stop = new SCR_AICombatMoveRequest_Stop();
   stop.m_bAimAtTarget = true;
   stop.m_bAimAtTargetEnd = true;
   utility.m_CombatMoveState.ApplyNewRequest(stop);
   return;
  }
  utility.m_vBBBFireStart = utility.m_OwnerEntity.GetOrigin();
  utility.m_vBBBFireDestination = position;
  utility.m_bBBBFireDestination = true;
  if (physicalCover) BBB_Perf.Count("fireManeuver.coverMoves");
  PrintFormat("[BBB] FIRE BOUND team=%1 phase=%2 destination=%3 cover=%4", utility.m_iBBBFireTeam, utility.m_iBBBFirePhase, position, physicalCover);
 }
 protected static void Arrived(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequestBase request)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  DoArrived(utility, request);
  if (BBB_Perf.s_bOn && !s_bInTick)
  {
   int elapsed = System.GetTickCount() - started;
   s_iEventMs += elapsed;
   BBB_Perf.Add("ev.fireManeuverWork", elapsed);
  }
 }
 protected static void DoArrived(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequestBase request)
 {
  if (!utility.m_BBBFireJob || utility.m_BBBFireMove != request || !utility.m_bBBBFireDestination || vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), utility.m_vBBBFireDestination) > 2.5) return;
  if (vector.DistanceXZ(utility.m_OwnerEntity.GetOrigin(), utility.m_vBBBFireStart) < 3 || utility.m_BBBFireJob.m_Reached.Contains(utility)) return;
  if (utility.m_bBBBFireGeometry && !BBB_FiringCover.Protected(utility.m_OwnerEntity.GetOrigin(), utility.m_BBBFireJob.m_Threat)) return;
  utility.m_BBBFireJob.m_Reached.Insert(utility);
  utility.m_iBBBFireArrivedPhase = utility.m_iBBBFirePhase;
  utility.m_BBBFireJob.m_iArrived++;
  s_iArrived++;
 }
 protected static void Failed(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequestBase request, SCR_EAICombatMoveRequestFailReason reason)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  DoFailed(utility, request, reason);
  if (BBB_Perf.s_bOn && !s_bInTick)
  {
   int elapsed = System.GetTickCount() - started;
   s_iEventMs += elapsed;
   BBB_Perf.Add("ev.fireManeuverWork", elapsed);
  }
 }
 protected static void DoFailed(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequestBase request, SCR_EAICombatMoveRequestFailReason reason)
 {
  if (utility.m_BBBFireMove != request || utility.m_BBBFireFailureHandled == request) return;
  utility.m_BBBFireFailureHandled = request;
  if (reason == SCR_EAICombatMoveRequestFailReason.COVER_NOT_FOUND)
  {
   s_iNoCover++;
   if (!utility.m_bBBBFlankRetried)
   {
    utility.m_bBBBFlankRetried = true;
    utility.m_iBBBFlankSide = -1;
    utility.m_iBBBFirePhase = -1;
    return;
   }
   if (!utility.m_bBBBGeometryTried && utility.m_BBBFireJob)
   {
    utility.m_bBBBGeometryTried = true;
    vector sheltered;
    if (BBB_FiringCover.Find(utility, utility.m_BBBFireJob.m_Threat, sheltered))
    {
     utility.m_bBBBFireGeometry = true;
     utility.m_vBBBFireGeometry = sheltered;
     utility.m_iBBBFirePhase = -1;
     PrintFormat("[BBB] FIRE GEOMETRY validated destination=%1", sheltered);
    }
   }
  }
 }

 protected static void StopMoves(BBB_FireTeamJob job)
 {
  foreach (SCR_AIUtilityComponent utility : job.m_Members)
  {
   if (!utility || !utility.m_BBBFireMove || !utility.m_CombatMoveState || utility.m_CombatMoveState.GetRequest() != utility.m_BBBFireMove) continue;
   if (utility.m_CombatMoveState.IsExecutingRequest())
   {
    SCR_AICombatMoveRequest_Stop stop = new SCR_AICombatMoveRequest_Stop();
    stop.m_bAimAtTarget = true;
    stop.m_bAimAtTargetEnd = true;
    utility.m_CombatMoveState.ApplyNewRequest(stop);
   }
   else utility.m_CombatMoveState.CancelRequest();
   utility.m_BBBFireMove = null;
  }
 }
 protected static void End(SCR_AIGroup group, float now)
 {
  BBB_FireTeamJob job = s_Jobs.Get(group);
  if (!job) return;
  StopMoves(job);
  ClearMembers(job);
  s_Jobs.Remove(group);
  if (group) s_Next.Set(group, now + 15);
 }
 protected static void ClearMembers(BBB_FireTeamJob job)
 {
  foreach (SCR_AIUtilityComponent utility : job.m_Members)
  {
   if (!utility || utility.m_BBBFireJob != job) continue;
   if (utility.m_CombatMoveState) utility.m_CombatMoveState.m_BBBFireOwner = null;
   utility.m_BBBFireJob = null;
   utility.m_BBBFireMove = null;
  }
 }
 static void Stop()
 {
  // Entity keys may already be null on mission shutdown. Clean values directly
  // and clear the map once, instead of waiting on weak-key lookup/removal.
  foreach (SCR_AIGroup group, BBB_FireTeamJob job : s_Jobs)
  {
   if (!job) continue;
   StopMoves(job);
   ClearMembers(job);
  }
  s_Jobs.Clear();
  s_Next.Clear();
 }
}

// Native cover points may be absent on a lane. Accept physical defilade only
// when a low torso is shielded but a crouched firing view is clear. These rays
// use the group's observed contact position, never a live enemy entity position.
class BBB_FiringCover
{
 static bool Geometry(notnull IEntity entity) { return ChimeraCharacter.Cast(entity) == null; }
 static float Ray(vector from, vector to)
 {
  TraceParam trace = new TraceParam();
  trace.Start = from;
  trace.End = to;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return GetGame().GetWorld().TraceMove(trace, BBB_FiringCover.Geometry);
 }
 static bool Protected(vector position, vector observed)
 {
  vector threat = BBB.Ground(observed) + Vector(0, 0.8, 0);
  return Ray(threat, position + Vector(0, 0.35, 0)) < 0.95 && Ray(position + Vector(0, 1.2, 0), threat) >= 0.99;
 }
 static bool Find(SCR_AIUtilityComponent utility, vector observed, out vector position)
 {
  if (!utility || !utility.GetAIAgent() || !utility.m_OwnerEntity) return false;
  AIPathfindingComponent pathing = AIPathfindingComponent.Cast(utility.GetAIAgent().FindComponent(AIPathfindingComponent));
  if (!pathing) return false;
  vector start = utility.m_OwnerEntity.GetOrigin();
  vector forward = observed - start;
  forward[1] = 0;
  forward.Normalize();
  vector lateral = Vector(-forward[2], 0, forward[0]);
  array<float> sides = {1, -1};
  array<float> distances = {8, 12, 18, 24};
  foreach (float sign : sides)
   foreach (float distance : distances)
   {
    vector at = BBB.Ground(start + lateral * (distance * sign) + forward * 4);
    vector snapped;
    if (!pathing.GetClosestPositionOnNavmesh(at, Vector(1, 2, 1), snapped)) continue;
    if (vector.DistanceXZ(snapped, at) > 1 || BBB_Terrain.IsWaterAt(snapped) || BBB_Terrain.WaterAlong(start, snapped, 4) > 0) continue;
    if (Math.AbsFloat(snapped[1] - start[1]) > 3) continue;
    if (Ray(start + Vector(0, 1.2, 0), snapped + Vector(0, 1.2, 0)) < 0.99 || !Protected(snapped, observed)) continue;
    position = snapped;
    return true;
   }
  return false;
 }
}

modded class SCR_AIUtilityComponent
{
 BBB_FireTeamJob m_BBBFireJob;
 int m_iBBBFireTeam, m_iBBBFirePhase, m_iBBBLastRounds, m_iBBBFireArrivedPhase;
 bool m_bBBBFireGeometry, m_bBBBGeometryTried, m_bBBBFlankRetried;
 int m_iBBBFlankSide;
 SCR_AICombatMoveRequestBase m_BBBFireFailureHandled;
 vector m_vBBBFireGeometry, m_vBBBFireDestination, m_vBBBFireStart;
 bool m_bBBBFireDestination;
 float m_fBBBLastFire;
 BaseMagazineComponent m_BBBFireMagazine;
 ref SCR_AICombatMoveRequest_Move m_BBBFireMove;
}

modded class SCR_AICombatMoveLogic_Attack
{
 protected override bool MoveToNextPosCondition()
 {
  int decision = BBB_FireManeuver.Decision(m_Utility);
  if (decision < 0) return false;
  if (decision > 0) return true;
  return super.MoveToNextPosCondition();
 }
 protected override void PushRequestMove()
 {
  bool bound = BBB_FireManeuver.Decision(m_Utility) == 1;
  super.PushRequestMove();
  BBB_FireManeuver.Configure(m_Utility, SCR_AICombatMoveRequest_Move.Cast(m_State.GetRequest()), bound);
 }
}

modded class SCR_AICombatMoveLogic_Suppressive
{
 protected override bool MoveToNextPosCondition()
 {
  int decision = BBB_FireManeuver.Decision(m_Utility);
  if (decision < 0) return false;
  if (decision > 0) return true;
  return super.MoveToNextPosCondition();
 }
 protected override void PushRequestMove()
 {
  bool bound = BBB_FireManeuver.Decision(m_Utility) == 1;
  super.PushRequestMove();
  BBB_FireManeuver.Configure(m_Utility, SCR_AICombatMoveRequest_Move.Cast(m_State.GetRequest()), bound);
 }
}

modded class SCR_AICombatMoveState
{
 SCR_AIUtilityComponent m_BBBFireOwner;
 override void ApplyRequestChangeStanceOutsideCover(ECharacterStance stance)
 {
  int started;
  if (BBB_Perf.s_bOn) started = System.GetTickCount();
  if (stance == ECharacterStance.PRONE && BBB_FireManeuver.ExposeGeometry(m_BBBFireOwner)) stance = ECharacterStance.CROUCH;
  if (BBB_Perf.s_bOn)
  {
   int elapsed = System.GetTickCount() - started;
   BBB_FireManeuver.s_iEventMs += elapsed;
   BBB_Perf.Add("ev.fireManeuverStance", elapsed);
  }
  super.ApplyRequestChangeStanceOutsideCover(stance);
 }
}
