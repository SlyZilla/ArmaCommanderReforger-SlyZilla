// Aim and binocular fixes for the game's soldier AI (2 Oct 2026, first pass; see the
// "Quick fixes" table in the BBB Combat AI Levers doc). Each one is a small override of
// a vanilla method, written against the game version of 2 Oct 2026:
//  1. Launchers: vanilla gives rocket launchers no aim error at all (they never miss).
//  2. Binoculars: vanilla raises them for any threat over 200 m away, even while the
//     soldier is the one being shot at or has been hit.
//  3. Suppression: vanilla's effect of threat on aim was removed ("adding threat effect
//     later"); aim error can now grow with the shooter's threat level.
//  4. Long range: vanilla aim error stops growing at 215 m; it can now keep growing.
//  5. One accuracy setting for hosts (vanilla has a TODO for it).
// Fixes 2, 3 and 5 are on by default (2 is a plain bug fix, 5 does nothing at 1). Fix 3 runs
// at x1.3: the firefight test (3 Oct) showed x2.5 far too strong (tuned side downed 2 vs 16)
// and x1.3 a moderate effect (11 vs 17 downed, 111 vs 87 shots per enemy down). Fixes 1 and
// 4 stay at vanilla values until a test backs them (BBB rule: off until tested);
// -bbbAimTuning 1 turns them on with the proposed values.
// Host options (server command line):
//  -bbbAimFixes 0          everything here off (vanilla)
//  -bbbAimTuning 1         fixes 1 and 4 on too: launcher 1, long range 3
//  -bbbAimScale <x>        overall AI aim error, 1 = vanilla (fix 5)
//  -bbbLauncherAim <x>     launcher aim error factor (fix 1; 0 = vanilla, 1 = like a rifle)
//  -bbbSuppressedAim <x>   aim error when fully suppressed (fix 3; default 1.3; 1 = vanilla)
//  -bbbLongRangeAim <x>    aim error factor at 600 m (fix 4; 0 = vanilla, which holds 1.4)
//  -bbbBinocsUnderFire 1   vanilla binoculars (fix 2 off)
class BBB_Aim
{
 static bool s_bRead;
 static bool s_bOn = true;
 static float s_fScale = 1;
 static float s_fLauncher = 0;
 static float s_fSuppressed = 1.3;
 static float s_fLongRange = 0;
 // The proposed values for fixes 1, 3 and 4 (-bbbAimTuning 1, and the range test).
 static const float TUNED_LAUNCHER = 1;
 static const float TUNED_SUPPRESSED = 1.3;
 static const float TUNED_LONG_RANGE = 3;
 static bool s_bBinoculars = true;
 // Test only: soldiers that keep vanilla aim (the A/B lanes of BBB_AimTest).
 static ref array<IEntity> s_aVanilla = {};

 // Vanilla SCR_AIGetAimErrorOffset: error factor 1.4 from 215 m on.
 static const float RAMP_END_M = 215;
 static const float VANILLA_FAR_FACTOR = 1.4;
 static const float FAR_M = 600;
 static const float CAP_M = 1000;
 // Threat (0..1) from which suppression starts to spoil aim: vanilla's "alerted" line.
 static const float THREAT_FROM = 0.33;

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbAimFixes", value) && value == "0") s_bOn = false;
  if (System.GetCLIParam("bbbAimTuning", value) && value == "1") UseTuning();
  if (System.GetCLIParam("bbbAimScale", value)) s_fScale = Math.Max(0, value.ToFloat());
  if (System.GetCLIParam("bbbLauncherAim", value)) s_fLauncher = Math.Max(0, value.ToFloat());
  if (System.GetCLIParam("bbbSuppressedAim", value)) s_fSuppressed = Math.Max(1, value.ToFloat());
  if (System.GetCLIParam("bbbLongRangeAim", value)) s_fLongRange = value.ToFloat();
  if (System.GetCLIParam("bbbBinocsUnderFire", value) && value == "1") s_bBinoculars = false;
  PrintFormat("[BBB] AIM fixes on=%1 scale=%2 launcher=%3 suppressed=%4 longRange=%5 binoculars=%6", s_bOn, s_fScale, s_fLauncher, s_fSuppressed, s_fLongRange, s_bBinoculars);
 }

 static void UseTuning()
 {
  s_fLauncher = TUNED_LAUNCHER;
  s_fSuppressed = TUNED_SUPPRESSED;
  s_fLongRange = TUNED_LONG_RANGE;
 }

 static bool On(IEntity soldier)
 {
  Read();
  if (!s_bOn) return false;
  if (soldier && s_aVanilla.Contains(soldier)) return false;
  return true;
 }

 // Fix 4: the aim error factor past 215 m (vanilla holds it at 1.4).
 static float FarFactor(float distance)
 {
  if (s_fLongRange <= 0 || distance <= RAMP_END_M) return VANILLA_FAR_FACTOR;
  float d = Math.Min(distance, CAP_M);
  return VANILLA_FAR_FACTOR + (s_fLongRange - VANILLA_FAR_FACTOR) * (d - RAMP_END_M) / (FAR_M - RAMP_END_M);
 }

 // Fix 3: x1 up to the alerted line, rising to s_fSuppressed at full threat.
 static float ThreatFactor(float threat)
 {
  if (threat <= THREAT_FROM) return 1;
  float t = Math.Min(1, (threat - THREAT_FROM) / (1 - THREAT_FROM));
  return 1 + (s_fSuppressed - 1) * t;
 }

 // A direction with no exact zero in x or z (see the suppression box fix below).
 static vector OffAxis(vector direction)
 {
  if (direction[0] == 0) direction[0] = 0.0001;
  if (direction[2] == 0) direction[2] = 0.0001;
  return direction;
 }

 // Fix 2: no binoculars while the threat is aimed at him, has hit him, or he's pinned.
 static bool BinocularsAllowed(SCR_EAIThreatSectorFlags flags, EAIThreatState state)
 {
  if (!s_bBinoculars) return true;
  if (flags & SCR_EAIThreatSectorFlags.DIRECTED_AT_ME) return false;
  if (flags & SCR_EAIThreatSectorFlags.CAUSED_DAMAGE) return false;
  return state != EAIThreatState.THREATENED;
 }
}

// Fixes 1, 3, 4, 5. Aim error = offset factor x distance factor x skill x light; both
// overrides below are called once per aim update, so this adds no per-tick work.
modded class SCR_AIGetAimErrorOffset
{
 protected SCR_AIUtilityComponent m_BBBUtility;

 override void OnInit(AIAgent owner)
 {
  super.OnInit(owner);
  m_BBBUtility = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
 }

 protected IEntity BBBShooter()
 {
  if (!m_CombatComponent) return null;
  return m_CombatComponent.GetOwner();
 }

 override float GetDistanceFactor(float distance)
 {
  float factor = super.GetDistanceFactor(distance);
  if (distance <= BBB_Aim.RAMP_END_M || !BBB_Aim.On(BBBShooter())) return factor;
  return BBB_Aim.FarFactor(distance);
 }

 override float GetOffsetWeaponTypeFactor(EWeaponType weaponType)
 {
  float factor = super.GetOffsetWeaponTypeFactor(weaponType);
  if (!BBB_Aim.On(BBBShooter())) return factor;
  if (weaponType == EWeaponType.WT_ROCKETLAUNCHER) factor = BBB_Aim.s_fLauncher;
  float threat = 0;
  if (m_BBBUtility && m_BBBUtility.m_ThreatSystem) threat = m_BBBUtility.m_ThreatSystem.GetThreatMeasure();
  return factor * BBB_Aim.s_fScale * BBB_Aim.ThreatFactor(threat);
 }
}

// Fix 2: binoculars down (and weapon up) while being shot at, hit or pinned.
modded class SCR_AILookAtThreatSystemLogic
{
 protected SCR_AIUtilityComponent m_BBBUtility;

 override void OnInit(AIAgent owner)
 {
  super.OnInit(owner);
  m_BBBUtility = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
 }

 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  ENodeResult result = super.EOnTaskSimulate(owner, dt);
  if (!m_bEvaluateEquipmentUsage || m_iSectorMajor == -1 || !m_ThreatFilter || !m_BBBUtility || !m_BBBUtility.m_ThreatSystem) return result;
  if (!BBB_Aim.On(m_MyEntity)) return result;
  SCR_EAIThreatSectorFlags flags = m_ThreatFilter.GetSectorFlags(m_iSectorMajor);
  if (BBB_Aim.BinocularsAllowed(flags, m_BBBUtility.m_ThreatSystem.GetState())) return result;
  SetVariableOut(PORT_USE_OPTICS, false);
  SetVariableOut(PORT_RAISE_WEAPON, true);
  return result;
 }
}

// Bonus fix found while testing (2 Oct): vanilla's suppression box divides by zero when
// the fire direction lies exactly along a map axis (VM exception in GetOutsideEdgePos,
// which ends that suppression). Nudge such directions off the axis. Always on: it only
// replaces a script error.
modded class SCR_AISuppressionVolumeBox
{
 override protected vector GetOutsideEdgePos(vector direction)
 {
  return super.GetOutsideEdgePos(BBB_Aim.OffAxis(direction));
 }
}
