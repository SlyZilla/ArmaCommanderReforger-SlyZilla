// Off-map supports the commander buys with RP and calls on the map (server side).
//  - Mortar barrages (HE, creeping, smoke) and illumination use the game's own
//    Game Master effect modules: spawned at the target, they fire and clean up.
//  - Rocket strike and bomb run are "virtual aircraft": rockets launched from a
//    moving point in the sky / large explosions along a line. No aircraft model
//    is needed, so a modded jet can replace this later.
//  - Helicopter gun run / loiter use a real crewed gunship that the script flies
//    along a path (the AI cannot fly). Its door gunners fight on their own; it
//    fires rocket salvos itself. It can be shot down.
enum ACR_ESupportType
{
 MORTAR_HE_SMALL,
 MORTAR_HE_LARGE,
 MORTAR_CREEPING,
 MORTAR_SMOKE,
 ILLUMINATION,
 ROCKET_STRIKE,
 BOMB_RUN,
 HELI_GUN_RUN,
 HELI_LOITER
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_SupportEntry
{
 [Attribute("0", UIWidgets.ComboBox, "Kind of support", "", ParamEnumArray.FromEnum(ACR_ESupportType))]
 ACR_ESupportType m_eType;
 [Attribute("", UIWidgets.EditBox, "Name in the command panel (empty = default)")]
 string m_sName;
 [Attribute("-1", UIWidgets.EditBox, "Cost in RP (-1 = default)")]
 int m_iCost;
 [Attribute("-1", UIWidgets.EditBox, "Seconds before it can be called again (-1 = default)")]
 int m_iCooldown;

 // runtime
 float m_fReadyAt;

 void Resolve()
 {
  if (m_sName.IsEmpty()) m_sName = ACR_Supports.DefaultName(m_eType);
  if (m_iCost < 0) m_iCost = ACR_Supports.DefaultCost(m_eType);
  if (m_iCooldown < 0) m_iCooldown = ACR_Supports.DefaultCooldown(m_eType);
 }
}

// A script-driven flight or firing run, updated every server frame.
class ACR_SupportRun : Managed
{
 bool m_bDone;
 bool Frame(ACR_Supports supports, float dt, float now) { return false; }
 void Finish() {}
}

// Rockets fired from a point moving through the sky toward the target.
class ACR_RocketStrikeRun : ACR_SupportRun
{
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 ResourceName m_Rocket;
 int m_iLeft = 16;
 float m_fNext;

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  m_Pos = m_Pos + m_Dir * (110 * dt);
  float along = vector.Dot(m_Target - m_Pos, m_Dir);
  if (along < 950 && along > 420 && m_iLeft > 0 && now >= m_fNext)
  {
   vector right = Vector(m_Dir[2], 0, -m_Dir[0]);
   vector aim = m_Target + m_Dir * Math.RandomFloat(-40, 60) + right * Math.RandomFloat(-18, 18);
   aim[1] = GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 1;
   supports.FireProjectile(m_Rocket, m_Pos + right * Math.RandomFloat(-4, 4), aim);
   m_iLeft--;
   if (m_iLeft % 4 == 0) PrintFormat("[ACR] SUPPORT strike rockets left %1 from %2", m_iLeft, m_Pos);
   m_fNext = now + 110;
  }
  return along > 250;
 }
}

// One door/nose gun of a support helicopter. The script aims and fires it
// (the AI gunners barely engage from a moving helicopter): it picks an enemy the
// gun can reach and see, leads for the helicopter's own speed, and fires bursts.
class ACR_HeliGun : Managed
{
 BaseCompartmentSlot m_Slot;
 IEntity m_Turret;
 TurretComponent m_TurretComponent;
 TurretControllerComponent m_Controller;
 string m_sName;
 ref array<bool> m_aBins = {};  // 36 bins of 10 deg: can it reach a target at that bearing from the nose
 int m_iLastAmmo = -1;
 float m_fYawCmd;
 float m_fPitchCmd;
 float m_fYawSign = 1;
 float m_fPitchSign = 1;
 float m_fLastYawErr = 999;
 int m_iWorse;
 IEntity m_Target;
 bool m_bFiring;
 float m_fBurstEnd;
 float m_fPauseEnd;
 float m_fNextPick;
 vector m_Wander;        // slowly drifting aim error (metres at the target)
 vector m_WanderGoal;
 float m_fNextWander;
 float m_fSettleUntil;   // time to get on a new target before the first burst
 static const float WANDER_METRES = 2.5;

 static int Bin(float bearing)
 {
  int bin = Math.Round((bearing + 180) / 10);
  while (bin < 0) bin += 36;
  while (bin >= 36) bin -= 36;
  return bin;
 }
 bool CoversBearing(float bearing)
 {
  if (m_aBins.Count() != 36) return false;
  return m_aBins[Bin(bearing)];
 }
 string Describe()
 {
  string text = "";
  for (int i = 0; i < 36; i++) if (m_aBins[i]) text += (i * 10 - 180).ToString() + " ";
  return text;
 }

 int Ammo()
 {
  if (!m_Controller || !m_Controller.GetWeaponManager()) return -1;
  BaseWeaponComponent weapon = m_Controller.GetWeaponManager().GetCurrentWeapon();
  if (!weapon || !weapon.GetCurrentMuzzle()) return -1;
  return weapon.GetCurrentMuzzle().GetAmmoCount();
 }

 bool Covers(vector point)
 {
  if (!m_TurretComponent) return false;
  vector excess = m_TurretComponent.GetAimingAngleExcess(point);
  return Math.AbsFloat(excess[0]) < 0.5 && Math.AbsFloat(excess[1]) < 0.5;
 }

 IEntity Occupant()
 {
  if (!m_Slot) return null;
  return m_Slot.GetOccupant();
 }

 bool Visible(IEntity heli, vector point)
 {
  TraceParam trace = new TraceParam();
  vector from = m_Turret.GetOrigin();
  trace.Start = from + (point - from).Normalized() * 2;
  trace.End = point;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  trace.Exclude = heli;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  if (fraction >= 0.97) return true;
  IEntity hit = trace.TraceEnt;
  return hit && (hit == m_Target || hit.GetRootParent() == m_Target);
 }

 // Where to point: chest height, led for the helicopter's own motion (bullets
 // inherit it) and a little bullet drop.
 vector LeadPoint(vector heliVelocity, IEntity target)
 {
  vector point = target.GetOrigin() + Vector(0, 1.1, 0);
  float flight = vector.Distance(point, m_Turret.GetOrigin()) / 820;
  point = point - heliVelocity * flight;
  point[1] = point[1] + 0.02 * flight * flight * 9.8 * 25;
  return point;
 }

 void StopFire()
 {
  if (m_bFiring && m_Controller) m_Controller.SetFireWeaponWanted(false);
  m_bFiring = false;
 }

 void Update(IEntity heli, vector heliVelocity, vector center, array<IEntity> enemies, float now)
 {
  IEntity gunner = Occupant();
  if (!m_Controller || !m_TurretComponent || !ACR_Respawn.IsAlive(gunner)) { StopFire(); return; }
  // The script is the gunner's brain: keep the AI from fighting over the gun.
  AIControlComponent ai = AIControlComponent.Cast(gunner.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();

  if (m_Target && (!ACR_Respawn.IsAlive(m_Target) || !Covers(LeadPoint(heliVelocity, m_Target)))) m_Target = null;
  if (!m_Target && now >= m_fNextPick)
  {
   m_fNextPick = now + 400;
   float best = float.MAX;
   foreach (IEntity enemy : enemies)
   {
    vector point = LeadPoint(heliVelocity, enemy);
    if (!Covers(point)) continue;
    float score = vector.Distance(enemy.GetOrigin(), center);
    if (score >= best) continue;
    if (!Visible(heli, point)) continue;
    best = score;
    m_Target = enemy;
   }
   m_iWorse = 0;
   m_fLastYawErr = 999;
   if (m_Target) m_fSettleUntil = now + Math.RandomFloat(900, 1600);
  }
  if (!m_Target) { StopFire(); return; }

  // A human gunner in a moving helicopter: the aim drifts around the target and
  // each new target takes a moment to get on. The gun's own dispersion adds on top.
  if (now >= m_fNextWander)
  {
   m_fNextWander = now + Math.RandomFloat(500, 1100);
   m_WanderGoal = Vector(Math.RandomFloat(-1, 1), Math.RandomFloat(-0.5, 0.7), Math.RandomFloat(-1, 1)) * WANDER_METRES;
  }
  m_Wander = m_Wander + (m_WanderGoal - m_Wander) * 0.05;
  vector aim = LeadPoint(heliVelocity, m_Target) + m_Wander;
  vector desired = (aim - m_Turret.GetOrigin()).Normalized();
  vector current = m_TurretComponent.GetAimingDirectionWorld();
  float errYaw = ACR_HeliRun.Wrap((Math.Atan2(desired[0], desired[2]) - Math.Atan2(current[0], current[2])) * Math.RAD2DEG);
  float errPitch = (Math.Asin(Math.Clamp(desired[1], -1, 1)) - Math.Asin(Math.Clamp(current[1], -1, 1))) * Math.RAD2DEG;
  // Closed loop on the gun's real aim; if the error keeps growing the axis sign is flipped.
  if (Math.AbsFloat(errYaw) > m_fLastYawErr + 0.2 && Math.AbsFloat(errYaw) > 5) m_iWorse++;
  else m_iWorse = 0;
  if (m_iWorse > 12)
  {
   m_fYawSign = -m_fYawSign;
   m_iWorse = 0;
   PrintFormat("[ACR] SUPPORT gun %1: flipped yaw sign", m_sName);
  }
  m_fLastYawErr = Math.AbsFloat(errYaw);
  m_fYawCmd = Math.Clamp(m_fYawCmd + m_fYawSign * errYaw * Math.DEG2RAD * 0.5, -3, 3);
  m_fPitchCmd = Math.Clamp(m_fPitchCmd + m_fPitchSign * errPitch * Math.DEG2RAD * 0.5, -1.5, 1.5);
  m_Controller.SetAimingAngles(m_fYawCmd, m_fPitchCmd);

  bool onTarget = Math.AbsFloat(errYaw) < 1.5 && Math.AbsFloat(errPitch) < 1.5;
  if (m_bFiring && now > m_fBurstEnd)
  {
   StopFire();
   m_fPauseEnd = now + Math.RandomFloat(400, 800);
  }
  else if (!m_bFiring && onTarget && now > m_fPauseEnd && now > m_fSettleUntil)
  {
   m_Controller.SetFireWeaponWanted(true);
   m_bFiring = true;
   m_fBurstEnd = now + Math.RandomFloat(700, 1300);
  }
  else if (m_bFiring && !onTarget && Math.AbsFloat(errYaw) > 6) StopFire();
 }
}

// A crewed gunship flown by script (the AI cannot fly).
//  Gun run: one straight pass with a rocket salvo.
//  Loiter: the helicopter's guns are measured when it arrives, and it flies the
//  pattern that keeps the most guns on the target:
//   - side guns (UH-1H doors): a pylon-turn orbit with the gun side toward the target;
//   - nose/tail guns (Mi-8 bow and ramp PKTs): repeated attack passes, nose gun and
//     rockets on the way in, ramp gun covering the target on the way out.
class ACR_HeliRun : ACR_SupportRun
{
 static const float SPEED_RUN = 55;
 static const float SPEED_LOITER = 36;
 static const float LOITER_RADIUS = 280;
 static const float LOITER_TIME_MS = 90000;
 static const float ALT_TRANSIT = 70;
 static const float ALT_LOITER = 55;

 IEntity m_Vehicle;
 string m_sFaction;
 bool m_bLoiter;
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 float m_fYaw;       // direction of travel
 float m_fBodyYaw;   // where the nose points (differs in a pylon turn)
 float m_fRoll;
 ResourceName m_Rocket;
 int m_iRockets;
 float m_fNextRocket;
 int m_iSalvoLeft;          // loiter: rockets still to fire in the current salvo
 vector m_SalvoAim;
 int m_iPhase;       // 0 inbound, 1 on station, 2 outbound
 float m_fOrbitEnd;
 vector m_ExitDir;
 float m_fStarted;
 bool m_bDestroyed;
 ref array<ref ACR_HeliGun> m_Guns = {};
 bool m_bAnalyzed;
 bool m_bPasses;     // attack passes instead of an orbit
 float m_fSideBearing = -90; // orbit: target bearing relative to the nose
 bool m_bPassOutbound;
 float m_fNextGunLog;
 float m_fNextEnemyScan;
 float m_fNextSideCheck;

 bool SideCanFire(float bearing)
 {
  foreach (ACR_HeliGun gun : m_Guns)
   if (gun.CoversBearing(bearing) && ACR_Respawn.IsAlive(gun.Occupant()) && gun.Ammo() != 0) return true;
  return false;
 }
 ref array<IEntity> m_aEnemies = {};

 bool Start(ACR_Supports supports, ResourceName prefab, float now)
 {
  float startDistance = 2200;
  if (m_bLoiter) startDistance = 1500;
  m_Pos = m_Target - m_Dir * startDistance;
  m_Pos[1] = SafeAltitude(m_Pos, m_Dir, ALT_TRANSIT);
  m_fYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
  m_fBodyYaw = m_fYaw;
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return false;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(m_fYaw, 0, 0), parameters.Transform);
  parameters.Transform[3] = m_Pos;
  m_Vehicle = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!m_Vehicle) return false;
  supports.m_Vehicles.Insert(m_Vehicle);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) physics.EnableGravity(false);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
  if (compartments) compartments.SpawnDefaultOccupants(seats);
  BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(m_Vehicle.FindComponent(BaseVehicleControllerComponent));
  if (controller) controller.ForceStartEngine();
  m_fStarted = now;
  PrintFormat("[ACR] SUPPORT helicopter %1 inbound from %2", prefab, m_Pos);
  m_iRockets = 14;
  if (m_bLoiter) m_iRockets = 16;
  return true;
 }

 // Terrain-following height: clear the ground (and sea) ahead.
 static float SafeAltitude(vector pos, vector forward, float above)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = 0;
  for (int i = 0; i < 4; i++)
  {
   vector probe = pos + forward * (i * 90);
   ground = Math.Max(ground, world.GetSurfaceY(probe[0], probe[2]));
  }
  return ground + above;
 }

 bool IsDestroyed()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  return damage && damage.GetState() == EDamageState.DESTROYED;
 }

 static vector Heading(float yaw)
 {
  return Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
 }

 static float Wrap(float angle)
 {
  while (angle > 180) angle -= 360;
  while (angle < -180) angle += 360;
  return angle;
 }

 // Find every crewed gun and measure which target bearings (relative to the
 // nose, target 60 m below at 280 m) it can reach, then pick the flight pattern.
 void AnalyzeGuns(ACR_Supports supports)
 {
  m_bAnalyzed = true;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  // Some helicopters (the Mi-8) have no default crew for their guns: seat soldiers there.
  AIGroup crewGroup;
  foreach (BaseCompartmentSlot seat : slots)
  {
   if (seat.GetOccupant())
   {
    AIControlComponent seated = AIControlComponent.Cast(seat.GetOccupant().FindComponent(AIControlComponent));
    if (seated && seated.GetControlAIAgent()) crewGroup = seated.GetControlAIAgent().GetParentGroup();
    if (crewGroup) break;
   }
  }
  ResourceName crewman = supports.CrewPrefab(m_sFaction);
  foreach (BaseCompartmentSlot empty : slots)
  {
   if (empty.GetOccupant() || empty.GetType() != ECompartmentType.TURRET) continue;
   IEntity seatedCrew = empty.SpawnCharacterInCompartment(crewman, crewGroup);
   PrintFormat("[ACR] SUPPORT seated %1 in %2: %3", crewman, empty.GetCompartmentName(), seatedCrew != null);
  }
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetType() != ECompartmentType.TURRET) continue;
   IEntity turret = slot.GetOwner();
   ACR_HeliGun gun = new ACR_HeliGun();
   gun.m_Slot = slot;
   gun.m_Turret = turret;
   gun.m_TurretComponent = TurretComponent.Cast(turret.FindComponent(TurretComponent));
   gun.m_Controller = TurretControllerComponent.Cast(turret.FindComponent(TurretControllerComponent));
   gun.m_sName = slot.GetCompartmentName();
   if (!gun.m_TurretComponent || !gun.m_Controller) continue;
   // Rocket pods and fixed guns are pilot weapons (no turret slot); this is crewed guns only.
   for (int bin = 0; bin < 36; bin++)
   {
    vector point = m_Pos + Heading(m_fBodyYaw + bin * 10 - 180) * LOITER_RADIUS;
    point[1] = m_Pos[1] - 60;
    gun.m_aBins.Insert(gun.Covers(point));
   }
   m_Guns.Insert(gun);
   PrintFormat("[ACR] SUPPORT gun %1 (occupant=%2 ammo=%3) reaches bearings: %4", gun.m_sName, slot.GetOccupant() != null, gun.Ammo(), gun.Describe());
  }
  // Best side bearing for an orbit: most guns covering +-(60..120).
  int bestCount = 0;
  float bestBearing = -90;
  array<int> signs = {-1, 1};
  array<int> sideBearings = {90, 80, 100, 70, 110, 60, 120};
  foreach (int b : sideBearings)
  {
   foreach (int sign : signs)
   {
    int count = 0;
    foreach (ACR_HeliGun g : m_Guns) if (g.CoversBearing(sign * b)) count++;
    if (count > bestCount) { bestCount = count; bestBearing = sign * b; }
   }
  }
  int noseTail = 0;
  foreach (ACR_HeliGun g2 : m_Guns)
  {
   if (g2.CoversBearing(0) || g2.CoversBearing(10) || g2.CoversBearing(-10)) noseTail++;
   else if (g2.CoversBearing(180) || g2.CoversBearing(170) || g2.CoversBearing(-170)) noseTail++;
  }
  m_fSideBearing = bestBearing;
  m_bPasses = bestCount == 0 || noseTail > bestCount;
  PrintFormat("[ACR] SUPPORT gunship pattern: %1 (side guns %2 at %3 deg, nose/tail guns %4)", m_bPasses, bestCount, bestBearing, noseTail);
 }

 override bool Frame(ACR_Supports supports, float dt, float now)
 {
  if (!m_Vehicle) return false;
  if (IsDestroyed())
  {
   m_bDestroyed = true;
   Physics wreck = m_Vehicle.GetPhysics();
   if (wreck) wreck.EnableGravity(true);
   supports.m_Manager.NotifyFaction(m_sFaction, "Support helicopter is going down!");
   Print("[ACR] SUPPORT helicopter destroyed");
   return false;
  }
  if (now - m_fStarted > 300000) return false;
  if (!m_bAnalyzed && now - m_fStarted > 500) AnalyzeGuns(supports);

  vector flat = m_Target - m_Pos;
  flat[1] = 0;
  float distance = flat.Length();
  vector toTarget = flat.Normalized();
  float yawToTarget = Math.Atan2(toTarget[0], toTarget[2]) * Math.RAD2DEG;
  vector desired = m_Dir;
  float bodyOffset = 0; // nose = track + offset
  float speed = SPEED_RUN;
  float altitude = ALT_TRANSIT;
  if (m_bLoiter)
  {
   speed = SPEED_LOITER;
   if (m_iPhase == 0 && distance < LOITER_RADIUS + 60)
   {
    m_iPhase = 1;
    m_fOrbitEnd = now + LOITER_TIME_MS;
    supports.m_Manager.NotifyFaction(m_sFaction, "Gunship on station.");
    PrintFormat("[ACR] SUPPORT gunship on station at %1", m_Pos);
   }
   if (m_iPhase == 1 && now > m_fOrbitEnd)
   {
    m_iPhase = 2;
    m_ExitDir = -toTarget;
    supports.m_Manager.NotifyFaction(m_sFaction, "Gunship leaving station.");
   }
   if (m_iPhase == 0) desired = toTarget;
   else if (m_iPhase == 1)
   {
    altitude = ALT_LOITER;
    // Gunner on the working side down or out of ammo: turn the other side in.
    if (!m_bPasses && now >= m_fNextSideCheck)
    {
     m_fNextSideCheck = now + 2000;
     if (!SideCanFire(m_fSideBearing) && SideCanFire(-m_fSideBearing))
     {
      m_fSideBearing = -m_fSideBearing;
      PrintFormat("[ACR] SUPPORT gunship switching to its other side (bearing %1)", m_fSideBearing);
     }
    }
    if (m_bPasses)
    {
     // Attack passes: run in, overfly, extend 650 m, come round again.
     if (!m_bPassOutbound && distance < 90) m_bPassOutbound = true;
     if (m_bPassOutbound && distance > 650) m_bPassOutbound = false;
     if (m_bPassOutbound) desired = Heading(m_fYaw);
     else desired = toTarget;
    }
    else
    {
     // Pylon turn: circle so the target stays on the gun side.
     // Target on the left (negative bearing) = circle with the left side inward.
     float side = -1;
     if (m_fSideBearing < 0) side = 1;
     vector tangent = Vector(toTarget[2], 0, -toTarget[0]) * side;
     desired = (tangent + toTarget * ((distance - LOITER_RADIUS) / LOITER_RADIUS * 1.5)).Normalized();
     // Point the nose so the target sits exactly at the guns' best bearing.
     bodyOffset = Wrap(yawToTarget - m_fSideBearing - Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG);
     bodyOffset = Math.Clamp(bodyOffset, -35, 35);
    }
   }
   else desired = m_ExitDir;
  }

  // Track: turn toward the desired heading at a helicopter-like rate, banking into it.
  float desiredYaw = Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG;
  float delta = Wrap(desiredYaw - m_fYaw);
  float maxTurn = 24 * dt;
  float turn = Math.Clamp(delta, -maxTurn, maxTurn);
  m_fYaw = Wrap(m_fYaw + turn);
  float yawRate = 0;
  if (dt > 0) yawRate = turn / dt;
  m_fRoll += (Math.Clamp(-yawRate * 1.2, -25, 25) - m_fRoll) * Math.Min(1, dt * 2);
  // Body: the nose follows the track plus the pylon-turn offset.
  float bodyDelta = Wrap(m_fYaw + bodyOffset - m_fBodyYaw);
  m_fBodyYaw = Wrap(m_fBodyYaw + Math.Clamp(bodyDelta, -40 * dt, 40 * dt));

  vector forward = Heading(m_fYaw);
  vector nose = Heading(m_fBodyYaw);
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (speed * dt);
  float wantedY = SafeAltitude(m_Pos, forward, altitude);
  m_Pos[1] = previous[1] + Math.Clamp(wantedY - previous[1], -10 * dt, 12 * dt);
  float pitch = -4;

  // Rockets: gun run fires one pass on the way in; loiter fires salvos at
  // enemies near the target whenever the nose points at them.
  if (m_iRockets > 0 && now >= m_fNextRocket)
  {
   vector aim;
   bool shoot;
   if (!m_bLoiter)
   {
    float along = vector.Dot(m_Target - m_Pos, m_Dir);
    shoot = along < 650 && along > 280;
    aim = m_Target + Vector(Math.RandomFloat(-15, 15), 3, Math.RandomFloat(-15, 15));
    if (shoot) pitch = -8;
   }
   else if (m_iSalvoLeft > 0)
   {
    // A salvo, once started, is fired complete at the same spot (no odd pair of rockets).
    shoot = true;
    aim = m_SalvoAim + Vector(Math.RandomFloat(-8, 8), 0, Math.RandomFloat(-8, 8));
    pitch = -8;
   }
   else if (m_iPhase == 1)
   {
    IEntity enemy = supports.FindEnemyNear(m_sFaction, m_Target, 220);
    vector enemyFlat;
    if (enemy) enemyFlat = enemy.GetOrigin() - m_Pos;
    enemyFlat[1] = 0;
    shoot = enemy != null && vector.Dot(enemyFlat.Normalized(), nose) > 0.9 && enemyFlat.Length() < 650 && enemyFlat.Length() > 150;
    if (enemy) aim = enemy.GetOrigin() + Vector(Math.RandomFloat(-6, 6), 2, Math.RandomFloat(-6, 6));
    if (shoot)
    {
     pitch = -8;
     m_iSalvoLeft = Math.Min(4, m_iRockets);
     m_SalvoAim = enemy.GetOrigin() + Vector(0, 2, 0);
    }
   }
   if (shoot)
   {
    aim[1] = Math.Max(aim[1], GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 0.5);
    vector right = Vector(nose[2], 0, -nose[0]);
    float sideOffset = 1.6;
    if (m_iRockets % 2 == 0) sideOffset = -1.6;
    supports.FireProjectile(m_Rocket, m_Pos + nose * 6 + right * sideOffset - Vector(0, 1.2, 0), aim, forward * speed);
    m_iRockets--;
    PrintFormat("[ACR] SUPPORT heli rocket, %1 left, range %2", m_iRockets, vector.Distance(m_Pos, aim));
    m_fNextRocket = now + 280;
    if (m_bLoiter)
    {
     m_iSalvoLeft--;
     if (m_iSalvoLeft <= 0) m_fNextRocket = now + 8000;
    }
   }
  }

  // Gunnery: engage enemies near the target once within reach.
  vector heliVelocity = forward * speed;
  if (m_bLoiter && m_iPhase != 2 && distance < 700)
  {
   if (now >= m_fNextEnemyScan)
   {
    m_fNextEnemyScan = now + 500;
    m_aEnemies.Clear();
    supports.FindEnemies(m_sFaction, m_Target, 320, m_aEnemies);
   }
   foreach (ACR_HeliGun shooter : m_Guns) shooter.Update(m_Vehicle, heliVelocity, m_Target, m_aEnemies, now);
  }
  else foreach (ACR_HeliGun idle : m_Guns) idle.StopFire();

  // Gun report (loiter): which guns can reach the nearest enemy, and rounds fired.
  if (m_bLoiter && m_iPhase == 1 && now >= m_fNextGunLog)
  {
   m_fNextGunLog = now + 5000;
   IEntity nearest = supports.FindEnemyNear(m_sFaction, m_Target, 250);
   string report = "";
   foreach (ACR_HeliGun gun : m_Guns)
   {
    int ammo = gun.Ammo();
    int fired = 0;
    if (gun.m_iLastAmmo >= 0 && ammo >= 0) fired = gun.m_iLastAmmo - ammo;
    gun.m_iLastAmmo = ammo;
    bool reach = nearest != null && gun.Covers(nearest.GetOrigin());
    report += string.Format(" | %1 manned=%2 target=%3 inArc=%4 fired=%5", gun.m_sName, ACR_Respawn.IsAlive(gun.Occupant()), gun.m_Target != null, reach, fired);
   }
   PrintFormat("[ACR] SUPPORT gunship guns%1", report);
  }

  vector transform[4];
  Math3D.AnglesToMatrix(Vector(m_fBodyYaw, pitch, m_fRoll), transform);
  transform[3] = m_Pos;
  m_Vehicle.SetTransform(transform);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics)
  {
   vector velocity = forward * speed;
   if (dt > 0) velocity[1] = (m_Pos[1] - previous[1]) / dt;
   physics.SetVelocity(velocity);
   physics.SetAngularVelocity(vector.Zero);
  }

  if (!m_bLoiter) return vector.Dot(m_Target - m_Pos, m_Dir) > -1800;
  if (m_iPhase == 2) return distance < 2000;
  return true;
 }

 override void Finish()
 {
  if (!m_Vehicle || m_bDestroyed) return;
  array<IEntity> crew = {};
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots) if (slot.GetOccupant()) crew.Insert(slot.GetOccupant());
  }
  foreach (IEntity member : crew) SCR_EntityHelper.DeleteEntityAndChildren(member);
  SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
  Print("[ACR] SUPPORT helicopter left the area");
 }
}

// A support's danger area, shown on the map to that side until it is over.
class ACR_SupportZone : Managed
{
 string m_sFaction;
 vector m_Pos;
 float m_fRadius;
 string m_sName;
 float m_fImpactAt;
 float m_fEndAt;
}

class ACR_PendingLaunch : Managed
{
 IEntity m_Entity;
 vector m_Dir;
 vector m_Velocity;
 int m_iFrame;
}

class ACR_Supports : Managed
{
 static const ResourceName MORTAR_SMALL = "{5D48E2F7DB0C3714}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Small.et";
 static const ResourceName MORTAR_LARGE = "{1DDF7187608968F2}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Large.et";
 static const ResourceName MORTAR_CREEPING = "{F5FAFFDBC271088C}PrefabsEditable/EffectsModules/Mortar/EffectModule_Zoned_MortarBarrage_Creeping.et";
 static const ResourceName MORTAR_SMOKE = "{2FDF3089DF6EC422}PrefabsEditable/EffectsModules/Mortar/Smoke/EffectModule_Zoned_MortarBarrage_Smoke_Medium.et";
 static const ResourceName FLARE = "{AA7F04F403FD2E00}PrefabsEditable/EffectsModules/Flares/EffectModule_Mortar_Flare_Illumination.et";
 static const ResourceName BOMB = "{72BEEF40AF179763}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Large.et";
 static const ResourceName HELI_WEST = "{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et";
 static const ResourceName HELI_EAST = "{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et";
 static const ResourceName ROCKET_WEST = "{072A755D5CB85D47}Prefabs/Weapons/Ammo/Ammo_Rocket_Hydra70_HE_M229.et";
 static const ResourceName ROCKET_EAST = "{EF17BED6DCEE4DE4}Prefabs/Weapons/Ammo/Ammo_Rocket_S5_FRAG_S5MO.et";

 ACR_Manager m_Manager;
 ref array<IEntity> m_Vehicles = {};
 protected ref array<ref ACR_SupportRun> m_Runs = {};
 protected ref array<ref ACR_PendingLaunch> m_Launches = {};
 protected int m_iFrame;
 ref array<ref ACR_SupportZone> m_Zones = {};

 void AddZone(string factionKey, vector pos, float radius, string name, int delaySeconds, int durationSeconds)
 {
  ACR_SupportZone zone = new ACR_SupportZone();
  float now = GetGame().GetWorld().GetWorldTime();
  zone.m_sFaction = factionKey;
  zone.m_Pos = pos;
  zone.m_fRadius = radius;
  zone.m_sName = name;
  zone.m_fImpactAt = now + delaySeconds * 1000;
  zone.m_fEndAt = zone.m_fImpactAt + durationSeconds * 1000;
  m_Zones.Insert(zone);
 }

 static float ZoneRadius(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 30;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 60;
   case ACR_ESupportType.MORTAR_CREEPING: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 40;
   case ACR_ESupportType.ILLUMINATION: return 90;
   case ACR_ESupportType.ROCKET_STRIKE: return 70;
   case ACR_ESupportType.BOMB_RUN: return 70;
   case ACR_ESupportType.HELI_GUN_RUN: return 40;
   case ACR_ESupportType.HELI_LOITER: return ACR_HeliRun.LOITER_RADIUS;
  }
  return 50;
 }
 static int ZoneDuration(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.ILLUMINATION: return 80;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.HELI_LOITER: return 100;
   case ACR_ESupportType.MORTAR_CREEPING: return 30;
  }
  return 15;
 }

 void ACR_Supports(ACR_Manager manager) { m_Manager = manager; }

 static string DefaultName(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return "Mortars: HE (small)";
   case ACR_ESupportType.MORTAR_HE_LARGE: return "Mortars: HE (large)";
   case ACR_ESupportType.MORTAR_CREEPING: return "Mortars: creeping barrage";
   case ACR_ESupportType.MORTAR_SMOKE: return "Mortars: smoke screen";
   case ACR_ESupportType.ILLUMINATION: return "Illumination flares";
   case ACR_ESupportType.ROCKET_STRIKE: return "Air: rocket strike";
   case ACR_ESupportType.BOMB_RUN: return "Air: bomb run";
   case ACR_ESupportType.HELI_GUN_RUN: return "Helicopter: gun run";
   case ACR_ESupportType.HELI_LOITER: return "Helicopter: loiter (90 s)";
  }
  return "Support";
 }
 static int DefaultCost(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 10;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 25;
   case ACR_ESupportType.MORTAR_CREEPING: return 25;
   case ACR_ESupportType.MORTAR_SMOKE: return 6;
   case ACR_ESupportType.ILLUMINATION: return 4;
   case ACR_ESupportType.ROCKET_STRIKE: return 25;
   case ACR_ESupportType.BOMB_RUN: return 35;
   case ACR_ESupportType.HELI_GUN_RUN: return 30;
   case ACR_ESupportType.HELI_LOITER: return 45;
  }
  return 20;
 }
 static int DefaultCooldown(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 90;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 180;
   case ACR_ESupportType.MORTAR_CREEPING: return 180;
   case ACR_ESupportType.MORTAR_SMOKE: return 60;
   case ACR_ESupportType.ILLUMINATION: return 45;
   case ACR_ESupportType.ROCKET_STRIKE: return 180;
   case ACR_ESupportType.BOMB_RUN: return 240;
   case ACR_ESupportType.HELI_GUN_RUN: return 180;
   case ACR_ESupportType.HELI_LOITER: return 300;
  }
  return 120;
 }
 // Seconds from the call until rounds land / aircraft arrive (helicopters: flight time).
 // Mortar missions take a realistic, somewhat random time: the call goes up the
 // chain, the section lays the tubes, then the rounds' time of flight.
 static int DefaultDelay(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return Math.RandomIntInclusive(45, 90);
   case ACR_ESupportType.MORTAR_HE_LARGE: return Math.RandomIntInclusive(50, 95);
   case ACR_ESupportType.MORTAR_CREEPING: return Math.RandomIntInclusive(60, 110);
   case ACR_ESupportType.MORTAR_SMOKE: return Math.RandomIntInclusive(40, 75);
   case ACR_ESupportType.ILLUMINATION: return Math.RandomIntInclusive(30, 60);
   case ACR_ESupportType.ROCKET_STRIKE: return 15;
   case ACR_ESupportType.BOMB_RUN: return 20;
  }
  return 0;
 }

 // Every support type, used when a faction's settings list none.
 static void BuildDefaults(array<ref ACR_SupportEntry> entries)
 {
  for (int type = ACR_ESupportType.MORTAR_HE_SMALL; type <= ACR_ESupportType.HELI_LOITER; type++)
  {
   ACR_SupportEntry entry = new ACR_SupportEntry();
   entry.m_eType = type;
   entry.m_iCost = -1;
   entry.m_iCooldown = -1;
   entry.Resolve();
   entries.Insert(entry);
  }
 }

 static bool IsWestern(string factionKey)
 {
  return factionKey == "US" || factionKey.Contains("US") || factionKey == "NATO" || factionKey == "BLUFOR";
 }

 bool IsSupportGroup(SCR_AIGroup group)
 {
  if (m_Vehicles.IsEmpty()) return false;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!character || !character.GetCompartmentAccessComponent()) continue;
   BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
   if (slot && m_Vehicles.Contains(slot.GetVehicle())) return true;
  }
  return false;
 }

 // Called by the commander (or AI). Returns the feedback message.
 string Call(string factionKey, int index, vector target, bool byAI = false)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  if (index < 0 || index >= data.m_aSupports.Count()) return "Unknown support";
  ACR_SupportEntry entry = data.m_aSupports[index];
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < entry.m_fReadyAt) return string.Format("%1 is not ready (%2 s)", entry.m_sName, Math.Ceil((entry.m_fReadyAt - now) / 1000));
  if (!m_Manager.m_Bases.Spend(factionKey, entry.m_iCost)) return string.Format("Not enough RP for %1 (%2 needed)", entry.m_sName, entry.m_iCost);
  entry.m_fReadyAt = now + entry.m_iCooldown * 1000;
  target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
  vector dir = ApproachDirection(factionKey, target);
  string where = ACR_Manager.Grid(target);
  int delay = DefaultDelay(entry.m_eType);
  PrintFormat("[ACR] SUPPORT %1 %2 at %3 (delay %4 s)", factionKey, entry.m_sName, target, delay);
  switch (entry.m_eType)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: ScheduleModule(MORTAR_SMALL, target, dir, delay); break;
   case ACR_ESupportType.MORTAR_HE_LARGE: ScheduleModule(MORTAR_LARGE, target, dir, delay); break;
   case ACR_ESupportType.MORTAR_CREEPING: ScheduleModule(MORTAR_CREEPING, target, dir, delay); break;
   case ACR_ESupportType.MORTAR_SMOKE: ScheduleModule(MORTAR_SMOKE, target, dir, delay); break;
   case ACR_ESupportType.ILLUMINATION:
    for (int flare = 0; flare < 4; flare++)
    {
     vector spot = target + Vector(Math.RandomFloat(-60, 60), 0, Math.RandomFloat(-60, 60));
     ScheduleModule(FLARE, spot, dir, delay + flare * 20);
    }
    break;
   case ACR_ESupportType.ROCKET_STRIKE:
   {
    ACR_RocketStrikeRun strike = new ACR_RocketStrikeRun();
    strike.m_Target = target;
    strike.m_Dir = dir;
    strike.m_Pos = target - dir * (950 + 110 * delay);
    strike.m_Pos[1] = target[1] + 170;
    strike.m_Rocket = RocketFor(factionKey);
    m_Runs.Insert(strike);
    break;
   }
   case ACR_ESupportType.BOMB_RUN:
    for (int bomb = 0; bomb < 4; bomb++)
    {
     vector impact = target + dir * ((bomb - 1.5) * 28) + Vector(Math.RandomFloat(-5, 5), 0, Math.RandomFloat(-5, 5));
     GetGame().GetCallqueue().CallLater(Explode, delay * 1000 + bomb * 350, false, impact);
    }
    break;
   case ACR_ESupportType.HELI_GUN_RUN:
   case ACR_ESupportType.HELI_LOITER:
   {
    ACR_HeliRun heli = new ACR_HeliRun();
    heli.m_sFaction = factionKey;
    heli.m_bLoiter = entry.m_eType == ACR_ESupportType.HELI_LOITER;
    heli.m_Target = target;
    heli.m_Dir = dir;
    heli.m_Rocket = RocketFor(factionKey);
    if (!heli.Start(this, HelicopterFor(factionKey), now)) return "No helicopter available";
    m_Runs.Insert(heli);
    delay = (int)(2200 / ACR_HeliRun.SPEED_RUN);
    break;
   }
  }
  AddZone(factionKey, target, ZoneRadius(entry.m_eType), entry.m_sName, delay, ZoneDuration(entry.m_eType));
  // Long waits are only announced roughly (to the nearest 15 s).
  string eta = string.Format("%1 s", delay);
  if (delay >= 30) eta = string.Format("about %1 s", (int)Math.Round(delay / 15.0) * 15);
  string message = string.Format("%1 on %2, ETA %3.", entry.m_sName, where, eta);
  if (!byAI) m_Manager.NotifyFaction(factionKey, message);
  return message;
 }

 ResourceName HelicopterFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && !settings.m_sSupportHelicopter.IsEmpty()) return settings.m_sSupportHelicopter;
  if (IsWestern(factionKey)) return HELI_WEST;
  return HELI_EAST;
 }

 ResourceName RocketFor(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && !settings.m_sSupportRocket.IsEmpty()) return settings.m_sSupportRocket;
  if (IsWestern(factionKey)) return ROCKET_WEST;
  return ROCKET_EAST;
 }

 // Aircraft and creeping barrages come from the side's nearest base.
 vector ApproachDirection(string factionKey, vector target)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), target);
   if (distance > 60 && distance < bestDistance) { bestDistance = distance; best = base; }
  }
  vector dir = Vector(0, 0, 1);
  if (best) dir = target - best.GetOrigin();
  dir[1] = 0;
  if (dir.Length() < 1) dir = Vector(0, 0, 1);
  return dir.Normalized();
 }

 protected void ScheduleModule(ResourceName prefab, vector target, vector dir, int delaySeconds)
 {
  GetGame().GetCallqueue().CallLater(SpawnModule, Math.Max(0, delaySeconds - 3) * 1000, false, prefab, target, dir);
 }

 // Effect modules fire their first round about 3 s after spawning.
 protected void SpawnModule(ResourceName prefab, vector target, vector dir)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) { PrintFormat("[ACR] SUPPORT missing prefab %1", prefab); return; }
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG, 0, 0), parameters.Transform);
  parameters.Transform[3] = target;
  IEntity module = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  PrintFormat("[ACR] SUPPORT module %1 spawned=%2", prefab, module != null);
 }

 protected void Explode(vector impact)
 {
  impact[1] = GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]) + 0.3;
  IEntity bomb = ACR_Manager.SpawnPrefab(BOMB, impact);
  PrintFormat("[ACR] SUPPORT bomb at %1 spawned=%2", impact, bomb != null);
  // The explosion's flash and sound only play where it is created (the server,
  // which on a dedicated host nobody hears): replay it on every client.
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (mode) mode.ACR_BroadcastExplosion(impact);
 }

 // Spawns a rocket/shell pointing at the target; it is launched next frame on the
 // server and every client (projectiles are simulated locally everywhere).
 void FireProjectile(ResourceName prefab, vector from, vector to, vector parentVelocity = "0 0 0")
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return;
  vector dir = (to - from).Normalized();
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixFromForwardVec(dir, parameters.Transform);
  parameters.Transform[3] = from;
  IEntity projectile = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!projectile) return;
  ACR_PendingLaunch launch = new ACR_PendingLaunch();
  launch.m_Entity = projectile;
  launch.m_Dir = dir;
  launch.m_Velocity = parentVelocity;
  launch.m_iFrame = m_iFrame;
  m_Launches.Insert(launch);
 }

 static void LaunchLocal(IEntity projectile, vector dir, vector parentVelocity)
 {
  if (!projectile) return;
  ProjectileMoveComponent move = ProjectileMoveComponent.Cast(projectile.FindComponent(ProjectileMoveComponent));
  if (move) move.Launch(dir, parentVelocity, 1, projectile, null, null, null, null);
 }

 ResourceName CrewPrefab(string factionKey)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(factionKey);
  if (!data.m_aGarrison.IsEmpty()) return data.m_aGarrison[0];
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  SCR_BasePlayerLoadout loadout;
  if (faction) loadout = GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction);
  if (loadout) return loadout.GetLoadoutResource();
  return "";
 }

 // Living enemy soldiers (squads, garrisons and players) within radius.
 void FindEnemies(string factionKey, vector center, float radius, array<IEntity> result)
 {
  array<AIAgent> agents = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == factionKey) continue;
   agents.Clear();
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) AddIfNear(agent.GetControlledEntity(), center, radius, result);
  }
  foreach (SCR_AIGroup garrison : m_Manager.m_Bases.m_GarrisonGroups)
  {
   if (!garrison || !garrison.GetFaction() || garrison.GetFaction().GetFactionKey() == factionKey) continue;
   agents.Clear();
   garrison.GetAgents(agents);
   foreach (AIAgent guard : agents) AddIfNear(guard.GetControlledEntity(), center, radius, result);
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction || faction.GetFactionKey() == factionKey) continue;
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (pc) AddIfNear(pc.GetControlledEntity(), center, radius, result);
  }
 }

 protected void AddIfNear(IEntity soldier, vector center, float radius, array<IEntity> result)
 {
  if (!ACR_Respawn.IsAlive(soldier) || result.Contains(soldier)) return;
  if (vector.DistanceXZ(soldier.GetOrigin(), center) <= radius) result.Insert(soldier);
 }

 IEntity FindEnemyNear(string factionKey, vector center, float radius)
 {
  IEntity best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == factionKey) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!ACR_Respawn.IsAlive(soldier)) continue;
    float distance = vector.DistanceXZ(soldier.GetOrigin(), center);
    if (distance < bestDistance) { bestDistance = distance; best = soldier; }
   }
  }
  return best;
 }

 // Every server frame.
 void Frame(float dt)
 {
  m_iFrame++;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int l = m_Launches.Count() - 1; l >= 0; l--)
  {
   ACR_PendingLaunch launch = m_Launches[l];
   if (launch.m_iFrame >= m_iFrame) continue;
   m_Launches.Remove(l);
   if (!launch.m_Entity) continue;
   LaunchLocal(launch.m_Entity, launch.m_Dir, launch.m_Velocity);
   SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
   if (gameMode) gameMode.ACR_BroadcastLaunch(SCR_EntityHelper.EntityToRplId(launch.m_Entity), launch.m_Dir, launch.m_Velocity);
  }
  for (int r = m_Runs.Count() - 1; r >= 0; r--)
  {
   ACR_SupportRun run = m_Runs[r];
   if (run.Frame(this, dt, now)) continue;
   run.Finish();
   m_Runs.Remove(r);
  }
  for (int v = m_Vehicles.Count() - 1; v >= 0; v--) if (!m_Vehicles[v]) m_Vehicles.Remove(v);
  for (int z = m_Zones.Count() - 1; z >= 0; z--) if (now > m_Zones[z].m_fEndAt) m_Zones.Remove(z);
 }
}

// The game mode entity is replicated, so it carries the "launch this projectile" broadcast.
modded class SCR_BaseGameMode
{
 void ACR_BroadcastLaunch(RplId id, vector dir, vector parentVelocity)
 {
  Rpc(ACR_RpcLaunch, id, dir, parentVelocity);
 }
 void ACR_BroadcastExplosion(vector at)
 {
  Rpc(ACR_RpcExplosion, at);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcExplosion(vector at)
 {
  if (Replication.IsServer()) return;
  Resource resource = Resource.Load(ACR_Supports.BOMB);
  if (!resource.IsValid()) return;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = at;
  GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcLaunch(RplId id, vector dir, vector parentVelocity)
 {
  ACR_Supports.LaunchLocal(SCR_EntityHelper.RplIdToEntity(id), dir, parentVelocity);
 }
}
