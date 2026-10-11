// Gunner contact drill (5 Oct 2026; Suggestions 1 + 3 in the BBB Combat AI Levers doc).
// Vanilla: a vehicle gunner taking fire from a shooter he hasn't recognised only looks
// toward the noise (observe behaviour, priority 69); Attack and Suppress need a known
// enemy, so an M2 gunner could sit pinned and "look around like a dork" without firing.
// BBB: when the threat filter says rounds are coming AT him (fly-bys, or he or his vehicle
// was hit) and he has no target, the gunner turns to where he heard it come from and fires
// short bursts into that area (reconnaissance by fire, vanilla suppressive fire behaviour)
// for about 12 s, or until he recognises a target (Attack outranks it). No cheating: the
// aim point is what he could hear and see (the threat sector's averaged origin, the way
// vanilla aims his look) with bearing and range errors added; a contact his perception has
// DETECTED (not yet recognised) near that bearing refines it. Vanilla's friendly fire check
// stays on while firing. One drill per gunner per 20 s.
// Moving scan: vanilla gunners in a moving vehicle look only 15 deg either side of the
// nose; BBB lets them scan 75 deg either side (stopped, vanilla already uses the whole arc).
// Host options: -bbbGunnerDrill 0 (drill off), -bbbGunnerScan 0 (moving scan vanilla).
class BBB_Gunner
{
 static bool s_bRead;
 static bool s_bDrill = true;
 static bool s_bScan = true;
 static int s_iDrills;          // drills started (tests read it)
 static ref array<IEntity> s_aVanilla = {}; // test only: gunners kept vanilla

 static const float DRILL_S = 12;
 static const float COOLDOWN_S = 20;
 static const float MIN_M = 25;
 static const float MAX_M = 900;
 static const float BEARING_SD_DEG = 7;   // hearing a shooter's direction under fire
 static const float BEARING_MAX_DEG = 20;
 static const float DETECTED_CONE_DEG = 25;
 static const float MOVING_SCAN_DEG = 75;
 static const float PRIORITY = 69.5;     // above observe-threats (69), below any attack (70+)

 static void Read()
 {
  if (s_bRead) return;
  s_bRead = true;
  string value;
  if (System.GetCLIParam("bbbGunnerDrill", value) && value == "0") s_bDrill = false;
  if (System.GetCLIParam("bbbGunnerScan", value) && value == "0") s_bScan = false;
  PrintFormat("[BBB] GUNNER drill=%1 movingScan=%2", s_bDrill, s_bScan);
 }

 static bool DrillOn(IEntity gunner)
 {
  Read();
  return s_bDrill && !(gunner && s_aVanilla.Contains(gunner));
 }

 // Gaussian-ish error: the sum of three uniforms, clamped.
 static float BearingError()
 {
  float e = (Math.RandomFloat(-1, 1) + Math.RandomFloat(-1, 1) + Math.RandomFloat(-1, 1)) * BEARING_SD_DEG;
  return Math.Clamp(e, -BEARING_MAX_DEG, BEARING_MAX_DEG);
 }

 // A sector of the threat filter changed: start the drill if it fits.
 static void OnSector(SCR_AIUtilityComponent utility, int sectorId)
 {
  if (!utility || !BBB_Combat.ValidSector(sectorId) || !utility.m_SectorThreatFilter || !utility.m_AIInfo) return;
  if (!utility.m_AIInfo.HasUnitState(EUnitState.IN_TURRET)) return;
  IEntity gunner = utility.m_OwnerEntity;
  if (!gunner || !DrillOn(gunner)) return;
  int t = System.GetTickCount();
  Consider(utility, gunner, sectorId);
  BBB_Perf.Add("ev.gunner", System.GetTickCount() - t);
 }

 protected static void Consider(SCR_AIUtilityComponent utility, IEntity gunner, int sectorId)
 {
  SCR_EAIThreatSectorFlags flags = utility.m_SectorThreatFilter.GetSectorFlags(sectorId);
  if (!(flags & SCR_EAIThreatSectorFlags.DIRECTED_AT_ME) && !(flags & SCR_EAIThreatSectorFlags.CAUSED_DAMAGE)) return;
  float now = GetGame().GetWorld().GetWorldTime() / 1000;
  if (now < utility.m_fBBBDrillReady) return;
  if (utility.m_AIInfo.HasUnitState(EUnitState.PILOT) || utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS)) return;
  if (utility.m_CombatComponent && utility.m_CombatComponent.GetCurrentTarget()) return;
  if (utility.m_CombatComponent && utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) return;
  if (BBB.PlayerOf(gunner)) return;

  vector me = gunner.GetOrigin();
  vector heard = utility.m_SectorThreatFilter.GetSectorPos(sectorId);
  if (!BBB_SuppressionSafety.GroundSector(heard)) return;
  float distance = vector.DistanceXZ(me, heard);
  if (distance < MIN_M || distance > MAX_M) return;

  string source = "heard";
  vector aim = Guess(me, heard, distance);
  vector seen;
  if (DetectedNear(utility, me, heard, seen))
  {
   aim = seen;
   source = "detected";
  }
  aim = BBB.Ground(aim);

  float half = Math.Clamp(distance * 0.06, 4, 25);
  vector bbMin = Vector(aim[0] - half, aim[1] - 1, aim[2] - half);
  vector bbMax = Vector(aim[0] + half, aim[1] + 2.5, aim[2] + half);
  SCR_AISuppressionVolumeBox volume = new SCR_AISuppressionVolumeBox(bbMin, bbMax);
  BBB_AIReconByFireBehavior drill = new BBB_AIReconByFireBehavior(utility, null, volume, DRILL_S, 1.2);
  utility.AddAction(drill);
  utility.m_fBBBDrillReady = now + DRILL_S + COOLDOWN_S;
  s_iDrills++;
  BBB_Perf.Count("gunner.drill");
  PrintFormat("[BBB] GUNNER drill: fire %1 m away, bursts into %2 (%3, %4 m off)", Math.Round(distance), aim, source, Math.Round(vector.DistanceXZ(aim, heard)));
 }

 // Where he thinks it came from: the heard origin turned by a bearing error, at a range
 // guessed 25% short to 30% long.
 static vector Guess(vector me, vector heard, float distance)
 {
  vector dir = heard - me;
  dir[1] = 0;
  dir.Normalize();
  float turn = BearingError() * Math.DEG2RAD;
  float c = Math.Cos(turn);
  float s = Math.Sin(turn);
  vector turned = Vector(dir[0] * c - dir[2] * s, 0, dir[0] * s + dir[2] * c);
  return me + turned * (distance * Math.RandomFloat(0.75, 1.3));
 }

 // A contact his perception has detected but not recognised, near that bearing.
 protected static bool DetectedNear(SCR_AIUtilityComponent utility, vector me, vector heard, out vector seen)
 {
  if (!utility.m_PerceptionComponent) return false;
  array<BaseTarget> targets = {};
  utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.DETECTED);
  vector dir = heard - me;
  dir[1] = 0;
  dir.Normalize();
  float cone = Math.Cos(DETECTED_CONE_DEG * Math.DEG2RAD);
  float best = -1;
  foreach (BaseTarget target : targets)
  {
   if (!target || target.GetTimeSinceDetected() > 10) continue;
   vector at = target.GetLastDetectedPosition();
   if (BBB_SuppressionSafety.AircraftSource(target.GetTargetEntity()) || !BBB_SuppressionSafety.GroundSector(at)) continue;
   vector to = at - me;
   to[1] = 0;
   float range = to.Length();
   if (range < MIN_M || range > MAX_M) continue;
   to.Normalize();
   float dot = vector.Dot(dir, to);
   if (dot < cone || dot <= best) continue;
   best = dot;
   seen = at;
  }
  return best > 0;
 }
}

// Reconnaissance by fire: vanilla suppressive fire into a box, for a few seconds, given
// up as soon as he has a real target.
class BBB_AIReconByFireBehavior : SCR_AISuppressBehavior
{
 protected float m_fEndMs;

 // Same leading parameters as vanilla's (Enforce passes them to the parent constructor).
 void BBB_AIReconByFireBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, SCR_AISuppressionVolumeBase suppressionVolume, float suppressionDuration, float fireRate, float priorityLevel = PRIORITY_LEVEL_NORMAL)
 {
  m_fEndMs = GetGame().GetWorld().GetWorldTime() + suppressionDuration * 1000;
  SetIsUniqueInActionQueue(true);
 }

 override float CustomEvaluate()
 {
  if (GetGame().GetWorld().GetWorldTime() > m_fEndMs) { Complete(); return 0; }
  if (m_Utility && m_Utility.m_CombatComponent && m_Utility.m_CombatComponent.GetCurrentTarget()) { Complete(); return 0; }
  if (m_Utility && m_Utility.m_AIInfo && !m_Utility.m_AIInfo.HasUnitState(EUnitState.IN_TURRET)) { Complete(); return 0; }
  return BBB_Gunner.PRIORITY;
 }
}

modded class SCR_AIUtilityComponent
{
 float m_fBBBDrillReady; // world time (s) from which a new gunner drill may start
}

modded class SCR_AIObserveThreatSystemBehavior
{
 override void OnThreatSectorEscalation(SCR_AISectorThreatFilter ts, int sectorId, float dangerValue)
 {
  super.OnThreatSectorEscalation(ts, sectorId, dangerValue);
  BBB_Gunner.OnSector(m_Utility, sectorId);
  BBB_Combat.OnThreatSector(m_Utility, sectorId);
 }

 override void OnMajorSectorChanged(SCR_AISectorThreatFilter ts, int newSectorId, int oldSectorId, float dangerValue)
 {
  super.OnMajorSectorChanged(ts, newSectorId, oldSectorId, dangerValue);
  BBB_Gunner.OnSector(m_Utility, newSectorId);
  BBB_Combat.OnThreatSector(m_Utility, newSectorId);
 }

 override void OnDamageTaken(SCR_AISectorThreatFilter ts, int sectorId)
 {
  super.OnDamageTaken(ts, sectorId);
  BBB_Gunner.OnSector(m_Utility, sectorId);
  BBB_Combat.OnThreatSector(m_Utility, sectorId);
 }
}

// Moving scan: a gunner in a moving vehicle watches 75 deg either side, not 15.
modded class SCR_AIGetAllowedLookRange
{
 override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
 {
  ENodeResult result = super.EOnTaskSimulate(owner, dt);
  if (result != ENodeResult.SUCCESS) return result;
  BBB_Gunner.Read();
  if (!BBB_Gunner.s_bScan) return result;
  SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner.GetControlledEntity());
  if (!character || !character.GetCompartmentAccessComponent()) return result;
  TurretCompartmentSlot turret = TurretCompartmentSlot.Cast(character.GetCompartmentAccessComponent().GetCompartment());
  if (!turret) return result;
  IEntity vehicle = turret.GetVehicle();
  if (!vehicle || !vehicle.GetPhysics() || !vehicle.GetPhysics().IsActive()) return result;
  if (vehicle.GetPhysics().GetVelocity().LengthSq() <= MIN_SPEED_MPS_SQ) return result;
  TurretControllerComponent controller = TurretControllerComponent.Cast(turret.GetController());
  if (!controller || !controller.GetTurretComponent()) return result;
  vector horizontal, vertical;
  controller.GetTurretComponent().GetAimingLimits(horizontal, vertical);
  float limit = horizontal[1];
  if (controller.GetTurretComponent().HasMoveableBase()) limit = 180;
  SetVariableOut(RANGE_PORT, Math.Min(limit, BBB_Gunner.MOVING_SCAN_DEG));
  return result;
 }
}
