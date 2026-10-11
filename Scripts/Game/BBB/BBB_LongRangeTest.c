// Fix 4 alone: 180-second rifle fights at 300/500 m, tuning sides swapped.
// Suppressed aim and global scale are held at vanilla to avoid measuring several
// aim changes together. This is measurement; it never enables production tuning.
class BBB_LongRangeTest : BBB_AimTest
{
 static ref BBB_LongRangeTest s_Long;
 static ref array<IEntity> s_StaticShooters = {};
 protected float m_SavedLong;
 protected float m_SavedSuppressed;
 protected float m_SavedLauncher;
 protected float m_SavedScale;
 protected bool m_SavedOn;
 protected int m_Completed;
 protected bool m_Missing;
 protected ref map<IEntity, float> m_LastHealth = new map<IEntity, float>();
 protected int m_DamagedA;
 protected int m_DamagedB;
 protected int m_TotalDamaged;
 protected int m_LastShots;
 protected float m_MinFiringSeparation;
 protected float m_MaxFiringSeparation;
 protected int m_CloseFireRounds;
 protected bool m_SavedResponse;
 protected bool m_SavedRadio;

 static void MaybeStartLong()
 {
  if (s_Long || !Replication.IsServer() || !System.IsCLIParam("acrBbbLongRangeTest")) return;
  s_Long = new BBB_LongRangeTest();
  GetGame().GetCallqueue().CallLater(s_Long.Begin, 10000);
 }

 void BBB_LongRangeTest() { m_aCheckDistances = {300, 500}; }

 protected override void UnitChecks()
 {
  BBB_Aim.Read();
  BBB_Combat.Read();
  m_SavedLong = BBB_Aim.s_fLongRange;
  m_SavedSuppressed = BBB_Aim.s_fSuppressed;
  m_SavedLauncher = BBB_Aim.s_fLauncher;
  m_SavedScale = BBB_Aim.s_fScale;
  m_SavedOn = BBB_Aim.s_bOn;
  m_SavedResponse = BBB_Combat.s_bResponse;
  m_SavedRadio = BBB_Combat.s_bRadio;
  BBB_Combat.s_bResponse = false;
  BBB_Combat.s_bRadio = false;
  BBB_Aim.s_bOn = true;
  BBB_Aim.s_fLongRange = BBB_Aim.TUNED_LONG_RANGE;
  BBB_Aim.s_fSuppressed = 1;
  BBB_Aim.s_fLauncher = 0;
  BBB_Aim.s_fScale = 1;
  Print("[BBB] LONGTEST stationary accuracy control: long range 3; scale/suppression vanilla; return response/radio off; 4 x 180 s, swapped sides");
 }

 protected override void PlanRounds()
 {
  array<float> ranges = {300, 500};
  foreach (float distance : ranges)
  {
   m_aRoundAT.Insert(false); m_aRoundDistance.Insert(distance); m_aRoundSwap.Insert(false);
   m_aRoundAT.Insert(false); m_aRoundDistance.Insert(distance); m_aRoundSwap.Insert(true);
  }
 }

 protected override bool StartRound(int round)
 {
  if (!super.StartRound(round)) return false;
  m_fRoundEnd = Now() + 180;
  m_LastHealth.Clear();
  m_DamagedA = 0;
  m_DamagedB = 0;
  m_LastShots = 0;
  m_MinFiringSeparation = float.MAX;
  m_MaxFiringSeparation = 0;
  return true;
 }

 static bool StaticShooter(IEntity entity)
 {
  return s_Long != null && entity != null && s_StaticShooters.Contains(entity);
 }

 protected override void Hook(BBB_AimSide side, bool launchers)
 {
  super.Hook(side, launchers);
  foreach (IEntity man : side.m_aMen)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(man);
   if (!character || !character.GetCharacterController()) continue;
   s_StaticShooters.Insert(man);
   character.GetCharacterController().OverrideMaxSpeed(0);
  }
 }

 protected override void Clear(BBB_AimLane lane)
 {
  array<BBB_AimSide> sides = {lane.m_A, lane.m_B};
  foreach (BBB_AimSide side : sides)
  {
   foreach (IEntity man : side.m_aMen)
   {
    s_StaticShooters.RemoveItem(man);
    ChimeraCharacter character = ChimeraCharacter.Cast(man);
    if (character && character.GetCharacterController()) character.GetCharacterController().OverrideMaxSpeed(-1);
   }
  }
  super.Clear(lane);
 }

 protected override void WatchVehicle(BBB_AimLane lane, float now)
 {
  int shots = lane.m_A.m_iShots + lane.m_B.m_iShots;
  if (shots > m_LastShots)
  {
   float nearest = float.MAX;
   foreach (IEntity a : lane.m_A.m_aMen)
   {
    if (Down(a)) continue;
    foreach (IEntity b : lane.m_B.m_aMen)
    {
     if (Down(b)) continue;
     nearest = Math.Min(nearest, vector.DistanceXZ(a.GetOrigin(), b.GetOrigin()));
    }
   }
   if (nearest != float.MAX)
   {
    m_MinFiringSeparation = Math.Min(m_MinFiringSeparation, nearest);
    m_MaxFiringSeparation = Math.Max(m_MaxFiringSeparation, nearest);
   }
  }
  m_LastShots = shots;
  array<BBB_AimSide> sides = {lane.m_A, lane.m_B};
  foreach (int sideIndex, BBB_AimSide side : sides)
  {
   foreach (IEntity man : side.m_aMen)
   {
    if (!man) continue;
    float health = BBB.Health(man);
    float last = 1;
    if (m_LastHealth.Contains(man)) last = m_LastHealth.Get(man);
    if (health < last - 0.001)
    {
     if (sideIndex == 0) m_DamagedA++;
     else m_DamagedB++;
    }
    m_LastHealth.Set(man, health);
   }
  }
 }

 protected override void EndRound()
 {
  BBB_AimLane lane = m_aLanes[0];
  if (lane.m_A.m_aMen.IsEmpty() || lane.m_B.m_aMen.IsEmpty()) m_Missing = true;
  m_Completed++;
  m_TotalDamaged += m_DamagedA + m_DamagedB;
  if (m_MinFiringSeparation <= BBB_Aim.RAMP_END_M) m_CloseFireRounds++;
  PrintFormat("[BBB] LONGTEST minimum opposing-team separation during shot samples: %1-%2 m", m_MinFiringSeparation, m_MaxFiringSeparation);
  PrintFormat("[BBB] LONGTEST round %1 range %2: health-drop samples US %3 / USSR %4, deaths/incapacitation %5/%6", m_Completed, m_aRoundDistance[m_iRound], m_DamagedA, m_DamagedB, Downed(lane.m_A), Downed(lane.m_B));
  super.EndRound();
 }

 protected override void Finish()
 {
  BBB_Aim.s_fLongRange = m_SavedLong;
  BBB_Aim.s_fSuppressed = m_SavedSuppressed;
  BBB_Aim.s_fLauncher = m_SavedLauncher;
  BBB_Aim.s_fScale = m_SavedScale;
  BBB_Aim.s_bOn = m_SavedOn;
  BBB_Combat.s_bResponse = m_SavedResponse;
  BBB_Combat.s_bRadio = m_SavedRadio;
  string verdict = "PASS";
  if (m_Completed != 4 || m_Missing || m_TotalDamaged == 0 || m_CloseFireRounds > 0) verdict = "FAIL";
  PrintFormat("[ACR] VERDICT %1 test=bbblongrange rounds=%2 expected=4 missingTeams=%3 damageSamples=%4 closeFireRounds=%5", verdict, m_Completed, m_Missing, m_TotalDamaged, m_CloseFireRounds);
  if (m_CloseFireRounds > 0) Print("[BBB] LONGTEST inconclusive: teams closed into vanilla aim range; production aim unchanged");
  if (m_TotalDamaged == 0) Print("[BBB] LONGTEST inconclusive: no health damage; production aim unchanged");
  super.Finish();
 }
}

// Only opt-in test participants hold their firing positions. Disabling attack
// combat movement lets native attack/suppression aim and fire instead of waiting
// forever to reach a cover point while their maximum speed is zero.
modded class SCR_AIAttackBehavior
{
 override float CustomEvaluate()
 {
  float priority = super.CustomEvaluate();
  if (m_Utility && BBB_LongRangeTest.StaticShooter(m_Utility.m_OwnerEntity)) m_bUseCombatMove = false;
  return priority;
 }
}

modded class SCR_AISuppressGroupClusterBehavior
{
 override float CustomEvaluate()
 {
  float priority = super.CustomEvaluate();
  if (m_Utility && BBB_LongRangeTest.StaticShooter(m_Utility.m_OwnerEntity)) m_bUseCombatMove = false;
  return priority;
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_LongRangeTest.MaybeStartLong();
 }
}
