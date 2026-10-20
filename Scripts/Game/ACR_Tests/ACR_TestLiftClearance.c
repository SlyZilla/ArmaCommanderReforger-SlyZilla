// srv:liftclearance: real UH-1H and Mi-8 airframes over the steepest surveyed
// 1 km climb on Kolguyev, plus native obstacle-volume and landing checks. About 2.5 minutes.
class ACR_ClearanceTestHeli : ACR_LiftHeli
{
 void BeginCruise(vector from, vector direction, float now)
 {
  m_Pos = from;
  m_Dir = direction;
  m_Start = from + direction * 3500;
  m_fYaw = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
  m_fBodyYaw = m_fYaw;
  m_fSpeed = SPEED_RUN;
  m_fClimb = 0;
  m_fNextTerrain = 0;
  m_fLeftAt = now;
  Place(direction * m_fSpeed, 0, 0);
 }

 void Cruise(float dt, float now) { FlyAway(dt, now); }
 void Hold() { Spool(); Place(vector.Zero, 0, 0); }
 void BeginApproach(vector zone)
 {
  m_Pos = zone + Vector(5, 12, 0);
  m_Dir = Vector(-1, 0, 0);
  m_fYaw = -90;
  m_fBodyYaw = -90;
  m_fSpeed = 2;
  m_fClimb = 0;
  m_fNextTerrain = 0;
  Place(vector.Zero, 0, 0);
 }
 bool Approach(ACR_Supports supports, vector zone, float dt, float now) { return FlyToSpot(supports, zone, dt, now); }
 bool CanMoveAt(vector from, vector to, bool vertical = false)
 {
  m_fBodyYaw = -90;
  IgnoreFlightOccupants();
  return RotorMoveClear(from, to, vertical);
 }
}

class ACR_TestLiftClearance
{
 static ref array<ref ACR_ClearanceTestHeli> s_Helis = {};
 static ref array<vector> s_Starts = {};
 static float s_fLast;
 static float s_fMinClearance = 10000;
 static float s_fMaxDamage;
 static float s_fDistance;
 static vector s_From, s_Dir, s_Zone;
 static IEntity s_Obstacle;
 static IEntity s_Soldier;
 static bool s_bFlying;
 static bool s_bFinished;
 static bool s_bLanding, s_bLanded;

 static void Run(int second)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
   foreach (ACR_Squad peaceful : manager.m_Squads)
    if (peaceful.m_Group && peaceful.m_Group.GetGroupUtilityComponent()) peaceful.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  if (second == 5) Prepare();
  if (second == 7 && s_Obstacle)
  {
   ACR_TestRegress.Check("fixture overhang stays at the requested height", Math.AbsFloat(s_Obstacle.GetOrigin()[1] - s_Zone[1] - 20) < 0.01, s_Obstacle.GetOrigin().ToString());
   ACR_TestRegress.Check("landing volume rejects an overhang outside the old 12 m disk", !ACR_LandingZone.Clear(s_Zone));
   vector blockedFrom = s_Zone + Vector(0, 15, 0);
   ACR_TestRegress.Check("tail movement guard rejects the overhang", !s_Helis.IsEmpty() && !s_Helis[0].CanMoveAt(blockedFrom, blockedFrom + Vector(1, 0, 0)));
   SCR_EntityHelper.DeleteEntityAndChildren(s_Obstacle);
  }
  if (second == 9)
  {
   ACR_TestRegress.Check("landing volume clears after obstacle removal", ACR_LandingZone.Clear(s_Zone));
   vector openFrom = s_Zone + Vector(0, 15, 0);
   ACR_TestRegress.Check("tail movement guard clears after obstacle removal", !s_Helis.IsEmpty() && s_Helis[0].CanMoveAt(openFrom, openFrom + Vector(1, 0, 0)));
   // Jacob 19 Oct: a lift hovered 1 m up because its squad stood under the tail. Nose -X,
   // so the tail box is centred 11 m toward +X; the soldier stands there.
   s_Soldier = ACR_Manager.SpawnPrefab(ACR_TestFobDesign.US_RIFLEMAN, ACR_Manager.Ground(s_Zone + Vector(11, 0, 0)));
   if (s_Soldier)
   {
    vector mins, maxs;
    s_Soldier.GetWorldBounds(mins, maxs);
    // Heli origin 0.3 m above the soldier's feet: the tail box (from 1 m above the origin)
    // then reaches down to 1.3 m, well into a standing soldier (about 1.8 m).
    vector touchFrom = Vector(s_Zone[0], s_Soldier.GetOrigin()[1] + 0.3, s_Zone[2]);
    ACR_TestRegress.Check("fixture soldier reaches into the tail box", maxs[1] > touchFrom[1] + 1, string.Format("soldier top %1, tail box bottom %2", maxs[1], touchFrom[1] + 1));
    ACR_TestRegress.Check("soldier under the tail does not stop the touchdown", !s_Helis.IsEmpty() && s_Helis[0].CanMoveAt(touchFrom, touchFrom - Vector(0, 0.5, 0), true));
    SCR_EntityHelper.DeleteEntityAndChildren(s_Soldier);
   }
   else ACR_TestRegress.Check("fixture soldier spawned", false);
  }
  if (second == 30 && s_Helis.Count() == 2)
  {
   s_fLast = GetGame().GetWorld().GetWorldTime();
   foreach (int i, ACR_ClearanceTestHeli heli : s_Helis)
   {
    vector start = s_From + Vector(s_Dir[2], 0, -s_Dir[0]) * (i * 90);
    start[1] = GetGame().GetWorld().GetSurfaceY(start[0], start[2]) + 70;
    heli.BeginCruise(start, s_Dir, s_fLast);
    s_Starts.Insert(start);
   }
   s_bFlying = true;
  }
  if (second == 115)
  {
   s_bFlying = false;
   ACR_TestRegress.Check("both airframes traverse the mountain route", s_Helis.Count() == 2 && s_fDistance > 1000, string.Format("minimum final displacement %1 m", s_fDistance));
   ACR_TestRegress.Check("mountain flight keeps terrain clearance", s_fMinClearance >= 19.5, s_fMinClearance.ToString());
   ACR_TestRegress.Check("mountain flight leaves rotors and drivetrain undamaged", s_fMaxDamage < 0.01, s_fMaxDamage.ToString());
   if (!s_Helis.IsEmpty()) s_Helis[0].BeginApproach(s_Zone);
   s_bLanding = true;
  }
  if (second == 140)
  {
   s_bFinished = true;
   GetGame().GetCallqueue().Remove(Frame);
   ACR_TestRegress.Check("final braking reaches the landing center and touches down", s_bLanded);
   ACR_TestRegress.Check("vertical landing leaves rotors and drivetrain undamaged", s_fMaxDamage < 0.01, s_fMaxDamage.ToString());
   foreach (ACR_ClearanceTestHeli cleanup : s_Helis) cleanup.Finish();
   ACR_TestRegress.Complete("liftclearance", 13);
  }
 }

 static void Prepare()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  world.GetBoundBox(mins, maxs);
  float bestRise;
  for (float x = mins[0] + 1500; x < maxs[0] - 1500; x += 500)
  {
   for (float z = mins[2] + 1500; z < maxs[2] - 1500; z += 500)
   {
    float ground = world.GetSurfaceY(x, z);
    if (ground < 5) continue;
    for (int bearing = 0; bearing < 8; bearing++)
    {
     vector dir = ACR_HeliRun.Heading(bearing * 45);
     vector end = Vector(x, 0, z) + dir * 1000;
     float rise = world.GetSurfaceY(end[0], end[2]) - ground;
     if (rise <= bestRise) continue;
     bestRise = rise;
     s_From = Vector(x, ground + 70, z);
     s_Dir = dir;
    }
   }
  }
  ACR_TestRegress.Check("fixture finds a dramatic 1 km elevation rise", bestRise > 150, string.Format("rise %1, from %2 toward %3", bestRise, s_From, s_Dir));
  if (bestRise <= 150) { ACR_TestRegress.Abort("liftclearance", "no mountain route"); return; }
  array<vector> zones = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   ACR_LandingZone.Find(base.GetOrigin(), 250, 1, zones);
   if (!zones.IsEmpty()) break;
  }
  if (zones.IsEmpty()) { ACR_TestRegress.Abort("liftclearance", "no landing fixture"); return; }
  s_Zone = zones[0];
  s_Obstacle = ACR_Manager.SpawnPrefab(ACR_TestStructures.HUMVEE, s_Zone + Vector(16, 20, 2));
  if (!s_Obstacle) { ACR_TestRegress.Abort("liftclearance", "no obstacle fixture"); return; }
  if (s_Obstacle.GetPhysics()) s_Obstacle.GetPhysics().ChangeSimulationState(SimulationState.COLLISION);
  // SpawnPrefab intentionally snaps purchases to the ground. Move this frozen
  // fixture afterward so the clearance tests exercise an actual overhang.
  s_Obstacle.SetOrigin(s_Zone + Vector(16, 20, 2));
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  array<ResourceName> prefabs = {ACR_Supports.HELI_TRANSPORT_WEST, ACR_Supports.HELI_TRANSPORT_EAST};
  foreach (int i, ResourceName prefab : prefabs)
  {
   ACR_ClearanceTestHeli heli = new ACR_ClearanceTestHeli();
   heli.m_sFaction = "US";
   heli.m_Target = s_From + Vector(i * 100, 0, 0);
   heli.m_Dir = s_Dir;
   if (heli.Start(manager.m_Supports, prefab, world.GetWorldTime()))
   {
    manager.m_Supports.Station().ApplyCrewPrefab(heli.m_Vehicle, manager.m_Supports.CrewPrefab("US", false));
    // This is a collision drill: leave the cockpit crew, remove door gunners so the
    // two national airframes cannot contaminate the damage check by firing at each other.
    SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(heli.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
    array<BaseCompartmentSlot> turrets = {};
    if (compartments) compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
    foreach (BaseCompartmentSlot turret : turrets)
     if (turret.GetOccupant()) SCR_EntityHelper.DeleteEntityAndChildren(turret.GetOccupant());
    s_Helis.Insert(heli);
   }
  }
  GetGame().GetCallqueue().CallLater(Frame, 50, true);
 }

 static void Frame()
 {
  if (s_bFinished) return;
  if (s_bLanding)
  {
   float landNow = GetGame().GetWorld().GetWorldTime();
   float landDt = Math.Min(0.1, (landNow - s_fLast) * 0.001);
   s_fLast = landNow;
   if (!s_Helis.IsEmpty())
   {
    ACR_ClearanceTestHeli landing = s_Helis[0];
    s_bLanded = landing.Approach(ACR_Manager.GetInstance().m_Supports, s_Zone, landDt, landNow);
    s_fMaxDamage = Math.Max(s_fMaxDamage, landing.MovementDamage());
   }
   return;
  }
  if (!s_bFlying)
  {
   foreach (ACR_ClearanceTestHeli parked : s_Helis) parked.Hold();
   return;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  float dt = Math.Min(0.1, (now - s_fLast) * 0.001);
  s_fLast = now;
  s_fDistance = float.MAX;
  foreach (int i, ACR_ClearanceTestHeli heli : s_Helis)
  {
   if (!heli.m_Vehicle) { s_fMaxDamage = 1; continue; }
   heli.Cruise(dt, now);
   float surface = GetGame().GetWorld().GetSurfaceY(heli.m_Pos[0], heli.m_Pos[2]);
   s_fMinClearance = Math.Min(s_fMinClearance, heli.m_Pos[1] - surface);
   float damage = heli.MovementDamage();
   if (damage > s_fMaxDamage) PrintFormat("[ACR] LIFTCLEARANCE damage airframe %1: %2 at %3 (%4 m AGL)", i, damage, heli.m_Pos, heli.m_Pos[1] - surface);
   s_fMaxDamage = Math.Max(s_fMaxDamage, damage);
   s_fDistance = Math.Min(s_fDistance, vector.DistanceXZ(heli.m_Pos, s_Starts[i]));
  }
 }
}
