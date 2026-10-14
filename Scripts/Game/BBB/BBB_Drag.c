// AI drag or carry a downed buddy into cover before treating him (4 Oct 2026), using
// ACE Carrying (part of ACE All in One) when it is loaded. ACE is called by function name
// (ScriptModule.Call), so neither BBB nor its host needs ACE as a dependency: without ACE
// this module does nothing.
// When: the soldier vanilla sent to heal a casualty has reached him (ARRIVE_M), the
// casualty is unconscious, and either the healer is under fire (threat >= alerted) or the
// casualty lies in view of an enemy his group saw or heard in the last RECENT_S seconds.
// Where: the nearest spot (MIN_M..MAX_M away, turned away from the enemy, on the soldier
// navmesh, dry) that every such enemy has no line of sight to. No spot: treat in place.
// How: DRAG (crouched, walking) while enemies are within DRAG_ENEMY_M or the healer is
// under fire; else CARRY (standing, walking). The healer gets a move behaviour that
// outranks his heal (BBB_AIDragMove) plus stance/speed settings; at the spot (or on a
// timeout, or if he is hit) ACE puts the casualty down and the vanilla heal carries on.
// One drag per casualty per COOLDOWN_S. Host option: -bbbDrag 0 = off.
// Only what the group perceived is used (no map knowledge).
class BBB_DragJob : Managed
{
 IEntity m_Healer;
 IEntity m_Casualty;
 vector m_vSpot;
 bool m_bCarry;
 float m_fStartMs;
 float m_fDeadlineMs;
 bool m_bConfirmed;
 float m_fLogMs;
 ref BBB_AIDragMove m_Move;
 ref SCR_AICharacterStanceSetting m_Stance;
 ref SCR_AICharacterMovementSpeedSetting m_Speed;
}

class BBB_AIDragMove : SCR_AIMoveIndividuallyBehavior
{
 override float CustomEvaluate()
 {
  return BBB_Drag.MOVE_PRIORITY;
 }

 override int GetCause()
 {
  return SCR_EAIBehaviorCause.GROUP_GOAL;
 }
}

class BBB_Drag
{
 static bool s_bRead;
 static bool s_bOn = true;
 static int s_iAce = -1;

 static const float ARRIVE_M = 3.5;
 static const float RECENT_S = 30;
 static const float THREAT_M = 400;
 static const float DRAG_ENEMY_M = 150;
 static const float MIN_M = 5;
 static const float MAX_M = 25;
 static const float SPEED_MS = 0.8;
 static const float COOLDOWN_S = 60;
 static const float ALERTED = 0.33;
 // Above the heal behaviour (111): he moves the casualty before treating him.
 static const float MOVE_PRIORITY = 112;

 static ref array<ref BBB_DragJob> s_aJobs = {};
 // Casualty -> world time (ms) of his last drag, or of a search that found no spot.
 static ref map<IEntity, float> s_mLast = new map<IEntity, float>();
 static bool s_bTicking;
 // For the test.
 static int s_iStarted;
 static int s_iArrived;
 static int s_iNoSpot;

 static void Stop()
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(Tick);
  s_bTicking = false;
  foreach (BBB_DragJob job : s_aJobs)
   if (job) Finish(job, "BBB stopped");
  s_aJobs.Clear();
  s_mLast.Clear();
  s_mRefusedLog.Clear();
  s_iStarted = 0;
  s_iArrived = 0;
  s_iNoSpot = 0;
 }

 protected static void StartTicking()
 {
  if (s_bTicking || !Replication.IsServer()) return;
  s_bTicking = true;
  if (GetGame().GetCallqueue().GetRemainingTime(Tick) < 0)
   GetGame().GetCallqueue().CallLater(Tick, 250, true);
 }

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbDrag", value) && value == "0") s_bOn = false;
  PrintFormat("[BBB] DRAG on=%1 ace=%2", s_bOn, AceLoaded());
 }

 // ACE Carrying present? (Looked up by name: BBB doesn't depend on ACE.)
 static bool AceLoaded()
 {
  if (s_iAce < 0)
  {
   typename helper = String("ACE_Carrying_HelperCompartment").ToType();
   s_iAce = 0;
   if (helper) s_iAce = 1;
  }
  return s_iAce == 1;
 }

 static bool On()
 {
  Read();
  return s_bOn && AceLoaded();
 }

 static float NowMs()
 {
  return GetGame().GetWorld().GetWorldTime();
 }

 static SCR_CharacterControllerComponent Controller(IEntity soldier)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character) return null;
  return SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
 }

 static bool Unconscious(IEntity soldier)
 {
  SCR_CharacterControllerComponent controller = Controller(soldier);
  return controller && controller.GetLifeState() == ECharacterLifeState.INCAPACITATED;
 }

 static bool InVehicle(IEntity soldier)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  return character && character.IsInVehicle();
 }

 // ---------------------------------------------------------------- ACE by name

 static bool AceBool(IEntity soldier, string function)
 {
  SCR_CharacterControllerComponent controller = Controller(soldier);
  if (!controller) return false;
  bool result = false;
  GetGame().GetScriptModule().Call(controller, function, false, result);
  return result;
 }

 static IEntity AceCarried(IEntity carrier)
 {
  SCR_CharacterControllerComponent controller = Controller(carrier);
  if (!controller) return null;
  IEntity carried = null;
  GetGame().GetScriptModule().Call(controller, "ACE_GetCarried", false, carried);
  return carried;
 }

 static void AceStart(IEntity healer, IEntity casualty, bool carry)
 {
  SCR_CharacterControllerComponent controller = Controller(healer);
  if (!controller) return;
  string function = "ACE_Carrying_DragCasualty";
  if (carry) function = "ACE_Carrying_CarryCasualty";
  int unused;
  GetGame().GetScriptModule().Call(controller, function, false, unused, casualty);
 }

 static void AceRelease(IEntity healer)
 {
  SCR_CharacterControllerComponent controller = Controller(healer);
  if (!controller) return;
  IEntity carried = AceCarried(healer);
  if (!carried) return;
  int unused;
  GetGame().GetScriptModule().Call(controller, "ACE_ReleaseCarried", false, unused, carried);
 }

 // ---------------------------------------------------------------- decision

 static bool Busy(IEntity healer)
 {
  foreach (BBB_DragJob job : s_aJobs)
   if (job.m_Healer == healer) return true;
  return false;
 }

 static bool BeingMoved(IEntity casualty)
 {
  foreach (BBB_DragJob job : s_aJobs)
   if (job.m_Casualty == casualty) return true;
  return false;
 }

 // Called from the healer's heal behaviour. True = a drag started (the heal waits).
 static bool Consider(SCR_AIUtilityComponent utility, IEntity healer, IEntity casualty, float priorityLevel)
 {
  if (!On() || !healer || !casualty || !utility) return false;
  if (vector.Distance(healer.GetOrigin(), casualty.GetOrigin()) > ARRIVE_M) return false;
  if (s_mLast.Contains(casualty) && NowMs() - s_mLast.Get(casualty) < COOLDOWN_S * 1000) return false;
  string refused = "";
  if (BeingMoved(casualty)) refused = "already being moved";
  else if (!Unconscious(casualty)) refused = "casualty conscious";
  else if (InVehicle(casualty) || InVehicle(healer)) refused = "in a vehicle";
  else if (!BBB_Medic.Fit(healer)) refused = "healer hurt";
  else if (AceBool(healer, "ACE_IsCarrier") || AceBool(casualty, "ACE_IsCarried")) refused = "already carrying/carried";
  if (!refused.IsEmpty())
  {
   Refused(healer, casualty, refused);
   return false;
  }
  float threat = 0;
  if (utility.m_ThreatSystem) threat = utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor();
  array<vector> enemies = {};
  Threats(healer, enemies);
  bool exposed = false;
  foreach (vector enemy : enemies)
  {
   if (BBB_Medic.ClearLine(enemy, casualty.GetOrigin())) { exposed = true; break; }
  }
  if (!exposed && threat < ALERTED)
  {
   Refused(healer, casualty, string.Format("safe enough (%1 enemies known, threat %2)", enemies.Count(), threat));
   return false;
  }
  s_mLast.Set(casualty, NowMs());
  StartTicking();
  vector spot;
  if (enemies.IsEmpty() || !FindCover(healer, casualty, enemies, spot))
  {
   s_iNoSpot++;
   PrintFormat("[BBB] DRAG %1 -> %2: no cover spot (%3 enemies known), treating in place", BBB_Medic.Name(healer), BBB_Medic.Name(casualty), enemies.Count());
   return false;
  }
  bool carry = threat < ALERTED && Nearest(casualty.GetOrigin(), enemies) > DRAG_ENEMY_M;
  Start(utility, healer, casualty, spot, carry, priorityLevel);
  return true;
 }

 // Log why a drag was not started (at most every 10 s per casualty).
 static ref map<IEntity, float> s_mRefusedLog = new map<IEntity, float>();
 static void Refused(IEntity healer, IEntity casualty, string why)
 {
  float now = NowMs();
  if (s_mRefusedLog.Contains(casualty) && now - s_mRefusedLog.Get(casualty) < 10000) return;
  if (s_mRefusedLog.Count() > 64) s_mRefusedLog.Clear();
  s_mRefusedLog.Set(casualty, now);
  StartTicking();
  PrintFormat("[BBB] DRAG %1 -> %2: not dragging, %3", BBB_Medic.Name(healer), BBB_Medic.Name(casualty), why);
 }

 // Enemies the healer's group saw or heard lately (last known spots).
 static void Threats(IEntity healer, notnull array<vector> outPositions)
 {
  outPositions.Clear();
  SCR_AIGroup group = BBB_Medic.GroupOf(healer);
  if (!group) return;
  SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
  PerceptionManager perception = GetGame().GetPerceptionManager();
  if (!utility || !utility.m_Perception || !perception) return;
  float now = perception.GetTime();
  float range = THREAT_M * THREAT_M;
  foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
  {
   if (!target) continue;
   if (target.m_eCategory != EAITargetInfoCategory.DETECTED && target.m_eCategory != EAITargetInfoCategory.IDENTIFIED) continue;
   if (now - target.m_fTimestamp > RECENT_S) continue;
   if (vector.DistanceSqXZ(target.m_vWorldPos, healer.GetOrigin()) > range) continue;
   outPositions.Insert(target.m_vWorldPos);
   if (outPositions.Count() >= BBB_Medic.MAX_TRACES) return;
  }
 }

 static float Nearest(vector at, array<vector> positions)
 {
  float best = 100000;
  foreach (vector p : positions)
   best = Math.Min(best, vector.DistanceXZ(p, at));
  return best;
 }

 // The nearest spot no known enemy can see, turned away from them.
 static bool FindCover(IEntity healer, IEntity casualty, array<vector> enemies, out vector spot)
 {
  vector from = casualty.GetOrigin();
  vector centre = vector.Zero;
  foreach (vector enemy : enemies) centre = centre + enemy;
  float count = enemies.Count();
  centre = centre / count;
  vector away = from - centre;
  away[1] = 0;
  if (away.Length() < 0.1) away = Vector(1, 0, 0);
  away.Normalize();
  AIPathfindingComponent pathing = Pathing(healer);
  array<float> radii = {MIN_M, 8, 12, 16, 20, MAX_M};
  array<float> turns = {0, 30, -30, 60, -60, 90, -90, 120, -120};
  foreach (float radius : radii)
  {
   foreach (float turn : turns)
   {
    vector dir = Turn(away, turn);
    vector at = BBB.Ground(from + dir * radius);
    if (BBB_Terrain.IsWaterAt(at)) continue;
    if (pathing)
    {
     vector onMesh;
     if (!pathing.GetClosestPositionOnNavmesh(at, Vector(1.5, 2, 1.5), onMesh)) continue;
     if (vector.DistanceXZ(onMesh, at) > 1.5) continue;
     at = onMesh;
    }
    bool hidden = true;
    foreach (vector enemy : enemies)
    {
     if (BBB_Medic.ClearLine(enemy, at)) { hidden = false; break; }
    }
    if (!hidden) continue;
    spot = at;
    return true;
   }
  }
  return false;
 }

 static vector Turn(vector dir, float degrees)
 {
  float rad = degrees * Math.DEG2RAD;
  float c = Math.Cos(rad);
  float s = Math.Sin(rad);
  return Vector(dir[0] * c - dir[2] * s, 0, dir[0] * s + dir[2] * c);
 }

 // The AI settings live on the soldier's agent.
 static SCR_AICharacterSettingsComponent Settings(IEntity soldier)
 {
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  if (!control || !control.GetControlAIAgent()) return null;
  return SCR_AICharacterSettingsComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AICharacterSettingsComponent));
 }

 static AIPathfindingComponent Pathing(IEntity soldier)
 {
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  if (!control || !control.GetControlAIAgent()) return null;
  return AIPathfindingComponent.Cast(control.GetControlAIAgent().FindComponent(AIPathfindingComponent));
 }

 // ---------------------------------------------------------------- the job

 static void Start(SCR_AIUtilityComponent utility, IEntity healer, IEntity casualty, vector spot, bool carry, float priorityLevel)
 {
  BBB_DragJob job = new BBB_DragJob();
  job.m_Healer = healer;
  job.m_Casualty = casualty;
  job.m_vSpot = spot;
  job.m_bCarry = carry;
  job.m_fStartMs = NowMs();
  float distance = vector.Distance(healer.GetOrigin(), spot);
  job.m_fDeadlineMs = job.m_fStartMs + (distance / SPEED_MS + 10) * 1000;
  ECharacterStance stance = ECharacterStance.CROUCH;
  if (carry) stance = ECharacterStance.STAND;
  SCR_AICharacterSettingsComponent settings = Settings(healer);
  if (settings)
  {
   job.m_Stance = SCR_AICharacterStanceSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, stance);
   job.m_Speed = SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.BEHAVIOR, SCR_EAIBehaviorCause.ALWAYS, EMovementType.WALK);
   settings.AddCharacterSetting(job.m_Stance, false);
   settings.AddCharacterSetting(job.m_Speed, false);
  }
  SCR_CharacterControllerComponent controller = Controller(healer);
  if (controller) SCR_AIStanceHandling.SetStance(controller, stance);
  AceStart(healer, casualty, carry);
  job.m_Move = new BBB_AIDragMove(utility, null, spot, MOVE_PRIORITY, priorityLevel, null, 1.0);
  utility.AddAction(job.m_Move);
  s_aJobs.Insert(job);
  s_iStarted++;
  string how = "drags";
  if (carry) how = "carries";
  PrintFormat("[BBB] DRAG %1 %2 %3 %4 m to cover", BBB_Medic.Name(healer), how, BBB_Medic.Name(casualty), Math.Round(distance));
  StartTicking();
 }

 static void Tick()
 {
  // A repeat queued just before Stop() may not yet have been removable.
  if (!s_bTicking)
  {
   if (GetGame()) GetGame().GetCallqueue().Remove(Tick);
   return;
  }
  // Keep sweeping even after a refused search or the last completed job.
  BBB_Medic.Prune(s_mLast);
  BBB_Medic.Prune(s_mRefusedLog);
  float now = NowMs();
  for (int i = s_aJobs.Count() - 1; i >= 0; i--)
  {
   BBB_DragJob job = s_aJobs[i];
   string ended = Check(job, now);
   if (ended.IsEmpty()) continue;
   Finish(job, ended);
   s_aJobs.Remove(i);
  }
  if (s_aJobs.IsEmpty() && s_mLast.IsEmpty() && s_mRefusedLog.IsEmpty())
  {
   s_bTicking = false;
   GetGame().GetCallqueue().Remove(Tick);
  }
 }

 // Empty while the job goes on, else why it ended.
 static string Check(BBB_DragJob job, float now)
 {
  if (!job.m_Healer || !BBB_Medic.Fit(job.m_Healer)) return "healer hit";
  if (!job.m_Casualty || !BBB.IsAlive(job.m_Casualty)) return "casualty died";
  bool carrying = AceBool(job.m_Healer, "ACE_IsCarrier");
  if (!job.m_bConfirmed)
  {
   if (carrying) job.m_bConfirmed = true;
   else if (now - job.m_fStartMs > 2000) return "ACE didn't pick him up";
   return "";
  }
  if (now - job.m_fLogMs >= 1000)
  {
   job.m_fLogMs = now;
   PrintFormat("[BBB] DRAG %1: %2 m to go, doing %3, weapon %4, stance %5", BBB_Medic.Name(job.m_Healer), Math.Round(vector.DistanceXZ(job.m_Healer.GetOrigin(), job.m_vSpot)), Doing(job.m_Healer), WeaponOf(job.m_Healer), StanceOf(job.m_Healer));
  }
  if (!carrying) return string.Format("dropped (doing %1, weapon %2)", Doing(job.m_Healer), WeaponOf(job.m_Healer));
  if (vector.DistanceXZ(job.m_Healer.GetOrigin(), job.m_vSpot) <= 1.5)
  {
   s_iArrived++;
   return "in cover";
  }
  if (now > job.m_fDeadlineMs) return "timed out";
  return "";
 }

 static string Doing(IEntity soldier)
 {
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  if (!control || !control.GetControlAIAgent()) return "?";
  SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetControlAIAgent().FindComponent(SCR_AIUtilityComponent));
  if (!utility || !utility.GetCurrentBehavior()) return "nothing";
  return utility.GetCurrentBehavior().ClassName();
 }

 static string WeaponOf(IEntity soldier)
 {
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons || !weapons.GetCurrentWeapon()) return "none";
  return weapons.GetCurrentWeapon().GetOwner().GetPrefabData().GetPrefabName();
 }

 static string StanceOf(IEntity soldier)
 {
  SCR_CharacterControllerComponent controller = Controller(soldier);
  if (!controller) return "?";
  return typename.EnumToString(ECharacterStance, controller.GetStance());
 }

 static void Finish(BBB_DragJob job, string why)
 {
  if (job.m_Healer && AceLoaded() && AceBool(job.m_Healer, "ACE_IsCarrier")) AceRelease(job.m_Healer);
  if (job.m_Move && job.m_Move.GetActionState() != EAIActionState.COMPLETED && job.m_Move.GetActionState() != EAIActionState.FAILED) job.m_Move.Complete();
  if (job.m_Healer)
  {
   SCR_AICharacterSettingsComponent settings = Settings(job.m_Healer);
   if (settings)
   {
    if (job.m_Stance) settings.RemoveSetting(job.m_Stance);
    if (job.m_Speed) settings.RemoveSetting(job.m_Speed);
   }
  }
  float seconds;
  float distance;
  if (GetGame() && GetGame().GetWorld()) seconds = (NowMs() - job.m_fStartMs) / 1000;
  if (job.m_Healer) distance = vector.DistanceXZ(job.m_Healer.GetOrigin(), job.m_vSpot);
  PrintFormat("[BBB] DRAG %1 with %2: %3 after %4 s, %5 m from the spot", BBB_Medic.Name(job.m_Healer), BBB_Medic.Name(job.m_Casualty), why, Math.Round(seconds), Math.Round(distance));
 }
}

// While dragging or carrying, the AI mustn't take a weapon in hand: ACE Carrying drops the
// casualty as soon as the carrier selects a weapon (its player rule).
modded class SCR_AISwitchWeapon
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  if (owner && BBB_Drag.Busy(owner.GetControlledEntity())) return ENodeResult.FAIL;
  return super.EOnTaskSimulate(owner, dt);
 }
}

// A casualty in an ACE carry helper (a "vehicle" seat on the carrier) isn't in a real
// vehicle: vanilla groups logged "SCR_AIVehicleUsageComponent not found" on every pick-up and
// put-down. Skip the group's vehicle bookkeeping for those seats.
modded class SCR_AIGroupUtilityComponent
{
 protected bool BBBCarrySeat(IEntity targetEntity)
 {
  if (!targetEntity) return false;
  if (ChimeraCharacter.Cast(targetEntity)) return true;
  return targetEntity.ClassName().StartsWith("ACE_");
 }

 override protected void OnAgentCompartmentEntered(AIAgent agent, IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
 {
  if (BBBCarrySeat(targetEntity)) return;
  super.OnAgentCompartmentEntered(agent, targetEntity, manager, mgrID, slotID, move);
 }

 override protected void OnAgentCompartmentLeft(AIAgent agent, IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
 {
  if (BBBCarrySeat(targetEntity)) return;
  super.OnAgentCompartmentLeft(agent, targetEntity, manager, mgrID, slotID, move);
 }
}

// A soldier dragging or carrying has no weapon in hand: he doesn't attack (vanilla's attack
// behaviour, up to priority 1120 for close targets, kept him standing still; without a
// weapon it also hit a Math.RandomFloat error).
modded class SCR_AIAttackBehavior
{
 override float CustomEvaluate()
 {
  if (m_Utility && BBB_Drag.Busy(m_Utility.m_OwnerEntity)) return 0;
  return super.CustomEvaluate();
 }
}

// Vanilla AI edge cases that only log errors, first seen in the drag test with ACE loaded
// (4 Oct): a fire mode with (almost) no shot span (Math.RandomFloat min > max), a look
// target at the soldier's own position and a zero-length combat move
// (GenerateRandomPointInRadius 0..0). Same result, no error line. Always on.
modded class SCR_AIGetFireTimes
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  if (m_CombatComponent)
  {
   BaseWeaponComponent weapon = m_CombatComponent.GetCurrentWeapon();
   if (weapon && weapon.GetCurrentMuzzle() && weapon.GetCurrentMuzzle().GetCurrentFireMode())
   {
    if (weapon.GetCurrentMuzzle().GetCurrentFireMode().GetShotSpan() < 0.053) return ENodeResult.FAIL;
   }
  }
  return super.EOnTaskSimulate(owner, dt);
 }
}

modded class SCR_AIGetRandomLookPosition
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  IEntity me = owner.GetControlledEntity();
  vector inPos;
  if (me && GetVariableIn(PORT_IN_POS, inPos) && vector.Distance(me.GetOrigin(), inPos) * m_fTangent < 0.01)
  {
   SetVariableOut(PORT_OUT_POS, inPos);
   return ENodeResult.SUCCESS;
  }
  return super.EOnTaskSimulate(owner, dt);
 }
}

modded class SCR_AICalculateNextCombatMovePos
{
 override protected vector RandomizeDestinationPos(float distance, vector centerPos)
 {
  if (distance * 0.1 < 0.01) return centerPos;
  return super.RandomizeDestinationPos(distance, centerPos);
 }
}
