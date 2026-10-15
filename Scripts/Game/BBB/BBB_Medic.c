// Safer healing (3 Oct 2026). Vanilla sends the nearest group member who carries any
// healing item (a bandage is enough) to a downed buddy. His heal behaviour (priority 111)
// beats attacking (70/90) and suppressing (63), and the tree walks him straight to the
// casualty: no threat check (vanilla declares a threat limit but never uses it), no cover.
// Soldiers died one after another running out to the same man.
// What BBB changes (each heal decision is one check, only for soldiers with a heal queued):
//  1. Don't walk into the fire: the healer holds the heal (keeps fighting or stays in
//     cover) while he is pinned down or bleeding himself, and while the casualty lies in
//     the open in view of an enemy his group saw or heard in the last RECENT_S seconds
//     (within DANGER_M; one line-of-sight trace per known enemy). That last hold lasts at
//     most EXPOSED_HOLD_S: casualties bleed out, so then he takes the risk. A healer already
//     at the casualty (ARRIVED_M) carries on; one a short dash away (DASH_M) ignores the
//     exposure. The first firefight test (3 Oct) held for any enemy near the casualty for
//     the whole fight: no casualty was treated during a fight and all bled out.
//  2. Don't run the gauntlet twice: when a healer is hit or killed on his way, nobody else
//     goes for that casualty for HOT_S seconds.
//  3. Prefer the real medic: a soldier with MEDIC_ITEMS+ healing items is picked over the
//     nearest rifleman with a bandage if he is at most PREFER_M farther away.
// While a heal is held, vanilla's 5 minute heal timeout is paused (up to MAX_HOLD_S), so
// the casualty is still treated once the fight goes quiet.
// The healer only uses what he or his group perceived (no map knowledge).
// OFF by default (BBB rule: off until a test backs it). Host option (server command line):
// -bbbMedic 1 turns it on.
// Test results (BBB_MedicTest, 5 v 5 at 100 m on an open lane, AI vs AI, 3 Oct): vanilla
// healers were never hit on their way (0 of ~30 heals over two runs), and every version of
// the hold cost the BBB side men: holding from "alerted" 15 vs 13 own men down, from
// "pinned" 15 vs 7 (casualties left lying bleed out or stay out of the fight). The live
// problem (healers shot one after another) didn't show up against AI shooters.
class BBB_Medic
{
 static bool s_bRead;
 static bool s_bOn = false;
 // Test only: soldiers that keep vanilla healing (the A/B sides of BBB_MedicTest).
 static ref array<IEntity> s_aVanilla = {};

 static const float RECENT_S = 15;
 static const float EXPOSED_HOLD_S = 30;
 static const int MAX_TRACES = 6;
 static const float DANGER_M = 300;
 static const float ARRIVED_M = 4;
 static const float DASH_M = 12;
 static const float HOT_S = 45;
 static const float MAX_HOLD_S = 900;
 static const int MEDIC_ITEMS = 4;
 static const float PREFER_M = 30;
 // Healer threat (without his own injury) from which he holds: vanilla's "threatened"
 // (pinned) line. The second test (3 Oct) held from "alerted" (0.33): a healer 100 m from
 // the enemy stayed above it the whole fight and never went.
 static const float THREAT_HOLD = 0.66;

 // Why a heal is held (0 = go).
 static const int GO = 0;
 static const int HOLD_SUPPRESSED = 1;
 static const int HOLD_BLEEDING = 2;
 static const int HOLD_ENEMY = 3;
 static const int HOLD_HOT = 4;
 static const int HOLD_SMOKING = 5;

 static bool s_bSmokeOn = true;
 static const float SMOKE_BLOOM_MS = 4000;
 static const float SMOKE_DURATION_MS = 30000;
 static const float SMOKE_COOLDOWN_MS = 45000;
 static const float SMOKE_RADIUS = 9.0;
 static const float SMOKE_THROW_MAX_M = 38.0;

 static ref map<IEntity, float> s_mSmokeTime = new map<IEntity, float>();
 static ref map<IEntity, vector> s_mSmokePos = new map<IEntity, vector>();

 // Casualty -> world time (ms) a heal for him was last held / he became a hot spot.
 static ref map<IEntity, float> s_mHeld = new map<IEntity, float>();
 static ref map<IEntity, float> s_mHot = new map<IEntity, float>();
 // Healer -> world time (ms) he was last let go to a casualty (on his way), and to whom.
 static ref map<IEntity, float> s_mGoing = new map<IEntity, float>();
 static ref map<IEntity, IEntity> s_mGoingTo = new map<IEntity, IEntity>();
 static bool s_bWatching;

 // Counters for the test (heal behaviour selected / completed, healers hit on the way).
 static int s_iStarted;
 static int s_iDone;
 static int s_iHealersHit;
 static ref map<IEntity, int> s_mStartedBy = new map<IEntity, int>();
 static ref map<IEntity, int> s_mDoneBy = new map<IEntity, int>();
 static ref map<IEntity, int> s_mHitBy = new map<IEntity, int>();

 static void Stop()
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(WatchGoing);
  s_bWatching = false;
  s_mHeld.Clear();
  s_mHot.Clear();
  s_mGoing.Clear();
  s_mGoingTo.Clear();
  s_mSmokeTime.Clear();
  s_mSmokePos.Clear();
  s_aVanilla.Clear();
  s_mStartedBy.Clear();
  s_mDoneBy.Clear();
  s_mHitBy.Clear();
  s_iStarted = 0;
  s_iDone = 0;
  s_iHealersHit = 0;
 }

 protected static void StartWatching()
 {
  if (s_bWatching || !Replication.IsServer()) return;
  s_bWatching = true;
  // A callback added and removed during the same queue dispatch may still be
  // pending. Reuse it on restart rather than adding a duplicate watch.
  if (GetGame().GetCallqueue().GetRemainingTime(WatchGoing) < 0)
   GetGame().GetCallqueue().CallLater(WatchGoing, 500, true);
 }

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbMedic", value)) s_bOn = value == "1";
  if (System.GetCLIParam("bbbMedicSmoke", value)) s_bSmokeOn = value != "0";
  PrintFormat("[BBB] MEDIC safer healing on=%1 smoke=%2", s_bOn, s_bSmokeOn);
 }

 static bool SmokeOn()
 {
  Read();
  return s_bSmokeOn;
 }

 static float SmokeAge(IEntity casualty)
 {
  if (!casualty || !s_mSmokeTime.Contains(casualty)) return -1;
  return NowMs() - s_mSmokeTime.Get(casualty);
 }

 static bool On(IEntity soldier)
 {
  Read();
  if (!s_bOn) return false;
  if (soldier && s_aVanilla.Contains(soldier)) return false;
  return true;
 }

 static float NowMs()
 {
  return GetGame().GetWorld().GetWorldTime();
 }

 // The decision itself, from plain facts (unit-checked by BBB_MedicTest).
 // exposed: an enemy can see the casualty and the hold for that hasn't run out yet.
 static int Decide(float distance, float threat, bool bleeding, bool exposed, bool hot, bool smoking = false)
 {
  if (distance <= ARRIVED_M) return GO;
  if (smoking) return HOLD_SMOKING;
  if (bleeding) return HOLD_BLEEDING;
  if (threat >= THREAT_HOLD) return HOLD_SUPPRESSED;
  if (hot) return HOLD_HOT;
  if (exposed && distance > DASH_M) return HOLD_ENEMY;
  return GO;
 }

 static string Reason(int decision)
 {
  switch (decision)
  {
   case HOLD_SUPPRESSED: return "pinned down";
   case HOLD_BLEEDING: return "bleeding himself";
   case HOLD_ENEMY: return "casualty in view of the enemy";
   case HOLD_HOT: return "a healer was hit going there";
   case HOLD_SMOKING: return "waiting for smoke screen to bloom";
  }
  return "go";
 }

 static bool Bleeding(IEntity soldier)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character) return false;
  SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
  return damage && damage.IsBleeding();
 }

 // Alive, conscious and not bleeding.
 static bool Fit(IEntity soldier)
 {
  if (!BBB.IsAlive(soldier)) return false;
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (character.GetCharacterController().IsUnconscious()) return false;
  return !Bleeding(soldier);
 }

 static bool HasSmokeGrenade(IEntity soldier)
 {
  if (!soldier) return false;
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character) return false;
  SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(character.FindComponent(SCR_AICombatComponent));
  if (!combat) return false;
  return combat.FindWeaponOfType(EWeaponType.WT_SMOKEGRENADE) != null;
 }

 static bool LineIntersectsSphere(vector from, vector to, vector center, float radius)
 {
  vector dir = to - from;
  float lenSq = dir.LengthSq();
  if (lenSq < 0.001) return vector.DistanceSq(from, center) <= radius * radius;
  vector fromToCenter = center - from;
  float t = vector.Dot(fromToCenter, dir) / lenSq;
  t = Math.Clamp(t, 0.0, 1.0);
  vector closest = from + dir * t;
  return vector.DistanceSq(closest, center) <= radius * radius;
 }

 static bool IsOccludedBySmoke(vector from, vector to)
 {
  if (!s_bSmokeOn) return false;
  float now = NowMs();
  for (int i = s_mSmokeTime.Count() - 1; i >= 0; i--)
  {
   IEntity cas = s_mSmokeTime.GetKey(i);
   if (!cas) continue;
   float elapsed = now - s_mSmokeTime.GetElement(i);
   if (elapsed >= SMOKE_BLOOM_MS && elapsed <= SMOKE_DURATION_MS)
   {
    vector smokePos = s_mSmokePos.Get(cas);
    if (LineIntersectsSphere(from, to, smokePos, SMOKE_RADIUS)) return true;
   }
  }
  return false;
 }

 // Does an enemy the healer's group saw or heard lately have a clear line to the casualty?
 static bool Exposed(IEntity healer, IEntity casualty)
 {
  SCR_AIGroup group = GroupOf(healer);
  if (!group || !casualty) return false;
  SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
  PerceptionManager perception = GetGame().GetPerceptionManager();
  if (!utility || !utility.m_Perception || !perception) return false;
  float now = perception.GetTime();
  vector at = casualty.GetOrigin();
  float danger = DANGER_M * DANGER_M;
  int traces;
  foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
  {
   if (!target) continue;
   if (target.m_eCategory != EAITargetInfoCategory.DETECTED && target.m_eCategory != EAITargetInfoCategory.IDENTIFIED) continue;
   if (now - target.m_fTimestamp > RECENT_S) continue;
   if (vector.DistanceSqXZ(target.m_vWorldPos, at) > danger) continue;
   if (traces >= MAX_TRACES) return true;
   traces++;
   if (ClearLine(target.m_vWorldPos, at)) return true;
  }
  return false;
 }

 // Eye height at the enemy's last known spot to a man lying on the ground; the ends are
 // pulled in a metre so the enemy and the casualty themselves don't block it.
 static bool ClearLine(vector from, vector to)
 {
  if (IsOccludedBySmoke(from, to)) return false;
  vector eye = from + Vector(0, 1.5, 0);
  vector body = to + Vector(0, 0.4, 0);
  vector dir = body - eye;
  float length = dir.Length();
  if (length < 3) return true;
  dir = dir / length;
  TraceParam trace = new TraceParam();
  trace.Start = eye + dir;
  trace.End = body - dir;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  return fraction >= 0.99;
 }

 static bool TryDeploySmoke(IEntity healer, IEntity casualty)
 {
  Read();
  if (!s_bSmokeOn || !healer || !casualty) return false;
  float now = NowMs();
  if (s_mSmokeTime.Contains(casualty) && now - s_mSmokeTime.Get(casualty) < SMOKE_COOLDOWN_MS) return false;

  SCR_AIGroup group = GroupOf(healer);
  if (!group) return false;

  vector casPos = casualty.GetOrigin();
  vector enemyPos = vector.Zero;
  SCR_AIGroupUtilityComponent utility = group.GetGroupUtilityComponent();
  if (utility && utility.m_Perception)
  {
   float bestDistSq = 600 * 600;
   foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
   {
    if (!target) continue;
    if (target.m_eCategory != EAITargetInfoCategory.DETECTED && target.m_eCategory != EAITargetInfoCategory.IDENTIFIED) continue;
    float distSq = vector.DistanceSqXZ(target.m_vWorldPos, casPos);
    if (distSq < bestDistSq)
    {
     bestDistSq = distSq;
     enemyPos = target.m_vWorldPos;
    }
   }
  }

  if (enemyPos == vector.Zero)
  {
   if (s_mHot.Contains(casualty))
   {
    vector toCas = casPos - healer.GetOrigin();
    toCas[1] = 0;
    if (toCas.LengthSq() > 1) enemyPos = casPos + toCas.Normalized() * 20;
   }
   if (enemyPos == vector.Zero)
   {
    vector fwd = healer.GetTransformAxis(2);
    enemyPos = casPos + fwd * 25;
   }
  }

  vector toEnemy = enemyPos - casPos;
  toEnemy[1] = 0;
  float enemyDist = toEnemy.Length();
  if (enemyDist < 1.0) toEnemy = "0 0 1";
  else toEnemy = toEnemy / enemyDist;

  float throwDistance = Math.Clamp(enemyDist * 0.35, 8.0, 16.0);
  vector smokePos = casPos + toEnemy * throwDistance;
  smokePos[1] = GetGame().GetWorld().GetSurfaceY(smokePos[0], smokePos[2]);

  IEntity thrower = null;
  ChimeraCharacter chHealer = ChimeraCharacter.Cast(healer);
  if (chHealer && Fit(healer) && !chHealer.IsInVehicle() && HasSmokeGrenade(healer))
  {
   if (vector.DistanceXZ(healer.GetOrigin(), smokePos) <= SMOKE_THROW_MAX_M) thrower = healer;
  }

  if (!thrower)
  {
   array<AIAgent> agents = {};
   BBB.s_Hooks.GetMembers(group, agents);
   float bestThrowerDist = SMOKE_THROW_MAX_M;
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter member = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!member || member == casualty || member == healer || !Fit(member) || member.IsInVehicle()) continue;
    if (!HasSmokeGrenade(member)) continue;
    float d = vector.DistanceXZ(member.GetOrigin(), smokePos);
    if (d <= bestThrowerDist)
    {
     bestThrowerDist = d;
     thrower = member;
    }
   }
  }

  if (!thrower) return false;

  SCR_AIUtilityComponent throwerUtil = SCR_AIUtilityComponent.Cast(thrower.FindComponent(SCR_AIUtilityComponent));
  if (!throwerUtil) return false;

  SCR_AIThrowGrenadeToBehavior behavior = new SCR_AIThrowGrenadeToBehavior(
   throwerUtil, null, smokePos, EWeaponType.WT_SMOKEGRENADE, 0.1, SCR_AIActionBase.PRIORITY_BEHAVIOR_THROW_GRENADE
  );
  throwerUtil.AddAction(behavior);

  s_mSmokeTime.Set(casualty, now);
  s_mSmokePos.Set(casualty, smokePos);
  StartWatching();
  PrintFormat("[BBB] MEDIC SMOKE %1 deploying smoke to %2 for casualty %3", Name(thrower), smokePos, Name(casualty));
  return true;
 }

 static SCR_AIGroup GroupOf(IEntity soldier)
 {
  if (!soldier) return null;
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  if (!control || !control.GetControlAIAgent()) return null;
  return SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup());
 }

 static bool Hot(IEntity casualty)
 {
  if (!casualty || !s_mHot.Contains(casualty)) return false;
  return NowMs() - s_mHot.Get(casualty) < HOT_S * 1000;
 }

 // A healer was hit on his way to the casualty; with BBB on, that spot is hot for a while.
 static void HealerHit(IEntity casualty, IEntity healer, bool on)
 {
  if (!casualty) return;
  s_iHealersHit++;
  Count(s_mHitBy, healer);
  if (!on)
  {
   PrintFormat("[BBB] MEDIC healer %1 hit on his way to %2 (vanilla healing)", Name(healer), Name(casualty));
   return;
  }
  Prune(s_mHot);
  s_mHot.Set(casualty, NowMs());
  StartWatching();
  PrintFormat("[BBB] MEDIC healer %1 hit on his way to %2: nobody goes there for %3 s", Name(healer), Name(casualty), HOT_S);
 }

 static void MarkHeld(IEntity casualty)
 {
  if (!casualty) return;
  Prune(s_mHeld);
  s_mHeld.Set(casualty, NowMs());
  StartWatching();
 }

 static void MarkGoing(IEntity healer, IEntity casualty)
 {
  if (!healer) return;
  // Already hit (bleeding or down): he isn't counted again while he limps on.
  if (!casualty || !Fit(healer))
  {
   s_mGoing.Remove(healer);
   s_mGoingTo.Remove(healer);
   return;
  }
  s_mGoing.Set(healer, NowMs());
  s_mGoingTo.Set(healer, casualty);
  StartWatching();
 }

 // Twice a second: a healer on his way who is down or bleeding was hit on his way.
 static void WatchGoing()
 {
  // Stop() can run before a newly queued repeat becomes removable. Such a
  // callback must cancel itself without touching the cleared mission state.
  if (!s_bWatching)
  {
   if (GetGame()) GetGame().GetCallqueue().Remove(WatchGoing);
   return;
  }
  Prune(s_mHeld);
  Prune(s_mHot);
  Prune(s_mSmokeTime);
  for (int s = s_mSmokePos.Count() - 1; s >= 0; s--)
  {
   IEntity key = s_mSmokePos.GetKey(s);
   if (!key || !s_mSmokeTime.Contains(key)) s_mSmokePos.RemoveElement(s);
  }
  PruneCounts(s_mStartedBy);
  PruneCounts(s_mDoneBy);
  PruneCounts(s_mHitBy);
  for (int v = s_aVanilla.Count() - 1; v >= 0; v--)
   if (!s_aVanilla[v]) s_aVanilla.Remove(v);
  float now = NowMs();
  for (int i = s_mGoing.Count() - 1; i >= 0; i--)
  {
   IEntity healer = s_mGoing.GetKey(i);
   if (!healer || !s_mGoingTo.Get(healer) || now - s_mGoing.GetElement(i) > 4000)
   {
    if (healer) s_mGoingTo.Remove(healer);
    s_mGoing.RemoveElement(i);
    continue;
   }
   if (Fit(healer)) continue;
   IEntity casualty = s_mGoingTo.Get(healer);
   s_mGoingTo.Remove(healer);
   s_mGoing.RemoveElement(i);
   if (casualty && vector.Distance(healer.GetOrigin(), casualty.GetOrigin()) > ARRIVED_M) HealerHit(casualty, healer, On(casualty));
  }
  // A decayed key cannot be found through the null pointer's hash bucket.
  for (int target = s_mGoingTo.Count() - 1; target >= 0; target--)
  {
   IEntity key = s_mGoingTo.GetKey(target);
   if (!key || !s_mGoingTo.GetElement(target) || !s_mGoing.Contains(key)) s_mGoingTo.RemoveElement(target);
  }
 }

 // Was he let go to a casualty in the last few seconds (not holding back)?
 static bool Going(IEntity healer)
 {
  if (!healer || !s_mGoing.Contains(healer)) return false;
  return NowMs() - s_mGoing.Get(healer) < 4000;
 }

 static bool HeldLately(IEntity casualty)
 {
  if (!casualty || !s_mHeld.Contains(casualty)) return false;
  return NowMs() - s_mHeld.Get(casualty) < 5000;
 }

 // Drop entries for deleted soldiers and old ones (keeps the maps small).
 static void Prune(map<IEntity, float> times)
 {
  float now = NowMs();
  for (int i = times.Count() - 1; i >= 0; i--)
  {
   if (!times.GetKey(i) || now - times.GetElement(i) > 120000) times.RemoveElement(i);
  }
 }

 protected static void PruneCounts(map<IEntity, int> counts)
 {
  for (int i = counts.Count() - 1; i >= 0; i--)
   if (!counts.GetKey(i)) counts.RemoveElement(i);
 }

 static int HealingItems(IEntity soldier)
 {
  if (!soldier) return 0;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return 0;
  return inventory.GetHealthComponentCount();
 }

 // Short name for log lines: prefab name and position.
 static string Name(IEntity soldier)
 {
  if (!soldier) return "nobody";
  string prefab = "soldier";
  if (soldier.GetPrefabData()) prefab = soldier.GetPrefabData().GetPrefabName();
  int slash = prefab.LastIndexOf("/");
  if (slash >= 0) prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1);
  prefab.Replace(".et", "");
  prefab.Replace("Character_", "");
  vector at = soldier.GetOrigin();
  return string.Format("%1@%2,%3", prefab, Math.Round(at[0]), Math.Round(at[2]));
 }

 static void Count(map<IEntity, int> perHealer, IEntity healer)
 {
  if (!healer) return;
  perHealer.Set(healer, perHealer.Get(healer) + 1);
  StartWatching();
 }
}

// Fix 1: hold the heal while it isn't safe to go.
modded class SCR_AIMedicHealBehavior
{
 protected int m_iBBBDecision = -1;
 protected float m_fBBBCheckedAt;
 protected float m_fBBBExposedSince;

 override float CustomEvaluate()
 {
  float priority = super.CustomEvaluate();
  if (priority <= 0 || !m_Utility) return priority;
  IEntity healer = m_Utility.m_OwnerEntity;
  IEntity casualty = m_EntityToHeal.m_Value;
  if (!healer || !casualty) return priority;
  // BBB_Drag: move the casualty into cover first (ACE Carrying, when loaded).
  if (BBB_Drag.Busy(healer)) return 0;
  if (BBB_Drag.Consider(m_Utility, healer, casualty, EvaluatePriorityLevel())) return 0;
  if (!BBB_Medic.On(healer))
  {
   // Vanilla healing (off, or the test's vanilla side): on his way while this runs.
   if (m_Utility.GetCurrentBehavior() == this) BBB_Medic.MarkGoing(healer, casualty);
   return priority;
  }
  // A Game Master's explicit order isn't second-guessed.
  if (EvaluatePriorityLevel() >= PRIORITY_LEVEL_GAMEMASTER) return priority;
  float now = BBB_Medic.NowMs();
  if (m_iBBBDecision < 0 || now - m_fBBBCheckedAt >= 1000)
  {
   m_fBBBCheckedAt = now;
   float distance = vector.Distance(healer.GetOrigin(), casualty.GetOrigin());
   float threat = 0;
   if (m_Utility.m_ThreatSystem) threat = m_Utility.m_ThreatSystem.GetThreatMeasureWithoutInjuryFactor();
   bool exposed = false;
   if (distance > BBB_Medic.DASH_M) exposed = BBB_Medic.Exposed(healer, casualty);
   if (!exposed) m_fBBBExposedSince = 0;
   else if (m_fBBBExposedSince <= 0) m_fBBBExposedSince = now;
   else if (now - m_fBBBExposedSince > BBB_Medic.EXPOSED_HOLD_S * 1000) exposed = false;
   bool hot = BBB_Medic.Hot(casualty);
   bool smoking = false;
   if (BBB_Medic.SmokeOn() && distance > BBB_Medic.ARRIVED_M)
   {
    if (exposed || hot || threat >= 0.33)
     BBB_Medic.TryDeploySmoke(healer, casualty);
    float smokeAge = BBB_Medic.SmokeAge(casualty);
    if (smokeAge >= 0 && smokeAge < BBB_Medic.SMOKE_BLOOM_MS)
     smoking = true;
   }
   int decision = BBB_Medic.Decide(distance, threat, BBB_Medic.Bleeding(healer), exposed, hot, smoking);
   if (decision != m_iBBBDecision) PrintFormat("[BBB] MEDIC %1 -> %2 at %3 m: %4", BBB_Medic.Name(healer), BBB_Medic.Name(casualty), Math.Round(distance), BBB_Medic.Reason(decision));
   m_iBBBDecision = decision;
  }
  if (m_iBBBDecision == BBB_Medic.GO)
  {
   if (m_Utility.GetCurrentBehavior() == this) BBB_Medic.MarkGoing(healer, casualty);
   return priority;
  }
  BBB_Medic.MarkGoing(healer, null);
  BBB_Medic.MarkHeld(casualty);
  return 0;
 }

 override void OnActionSelected()
 {
  super.OnActionSelected();
  BBB_Medic.s_iStarted++;
  if (!m_Utility) return;
  BBB_Medic.Count(BBB_Medic.s_mStartedBy, m_Utility.m_OwnerEntity);
  BBB_Medic.MarkGoing(m_Utility.m_OwnerEntity, m_EntityToHeal.m_Value);
 }

 override void OnActionCompleted()
 {
  super.OnActionCompleted();
  BBB_Medic.s_iDone++;
  if (!m_Utility) return;
  BBB_Medic.Count(BBB_Medic.s_mDoneBy, m_Utility.m_OwnerEntity);
  BBB_Medic.MarkGoing(m_Utility.m_OwnerEntity, null);
 }

 override void OnActionFailed()
 {
  super.OnActionFailed();
  if (m_Utility) BBB_Medic.MarkGoing(m_Utility.m_OwnerEntity, null);
 }
}

// The paused timeout while a heal is held (fix 2, the healer watch, is BBB_Medic.WatchGoing).
modded class SCR_AIHealActivity
{
 protected float m_fBBBLastMs;
 protected float m_fBBBBornMs;

 override float CustomEvaluate()
 {
  IEntity casualty = m_EntityToHeal.m_Value;
  if (casualty)
  {
   bool on = BBB_Medic.On(casualty);
   float now = BBB_Medic.NowMs();
   if (m_fBBBBornMs <= 0) m_fBBBBornMs = now;
   if (on && m_fBBBLastMs > 0 && BBB_Medic.HeldLately(casualty) && now - m_fBBBBornMs < BBB_Medic.MAX_HOLD_S * 1000)
    m_fTimeCreated_world += now - m_fBBBLastMs;
   m_fBBBLastMs = now;
  }
  return super.CustomEvaluate();
 }
}

// Fix 3: the real medic over the nearest man with a bandage, if he isn't much farther.
modded class SCR_AIFindMedic
{
 override protected AIAgent FindAgentMedic(IEntity entToHeal, notnull array<AIAgent> agents, notnull array<AIAgent> agentsToExclude, bool excludeDrivers = false)
 {
  AIAgent nearest = super.FindAgentMedic(entToHeal, agents, agentsToExclude, excludeDrivers);
  if (!nearest || !entToHeal || !BBB_Medic.On(entToHeal) || !nearest.GetControlledEntity()) return nearest;
  if (BBB_Medic.HealingItems(nearest.GetControlledEntity()) >= BBB_Medic.MEDIC_ITEMS) return nearest;
  float limit = vector.Distance(nearest.GetControlledEntity().GetOrigin(), entToHeal.GetOrigin()) + BBB_Medic.PREFER_M;
  AIAgent best = null;
  float bestDistance = limit;
  foreach (AIAgent agent : agents)
  {
   SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
   if (!chimera || !chimera.m_InfoComponent) continue;
   IEntity man = agent.GetControlledEntity();
   if (!man || man == entToHeal || agentsToExclude.Contains(agent)) continue;
   SCR_AIInfoComponent info = chimera.m_InfoComponent;
   if (info.HasUnitState(EUnitState.UNCONSCIOUS)) continue;
   if (excludeDrivers && info.HasUnitState(EUnitState.PILOT)) continue;
   if (info.GetAIState() != EUnitAIState.AVAILABLE) continue;
   if (BBB_Medic.HealingItems(man) < BBB_Medic.MEDIC_ITEMS) continue;
   float distance = vector.Distance(man.GetOrigin(), entToHeal.GetOrigin());
   if (distance > bestDistance) continue;
   best = agent;
   bestDistance = distance;
  }
  if (!best) return nearest;
  PrintFormat("[BBB] MEDIC %1 (%2 healing items) sent to %3 instead of %4", BBB_Medic.Name(best.GetControlledEntity()), BBB_Medic.HealingItems(best.GetControlledEntity()), BBB_Medic.Name(entToHeal), BBB_Medic.Name(nearest.GetControlledEntity()));
  return best;
 }
}
