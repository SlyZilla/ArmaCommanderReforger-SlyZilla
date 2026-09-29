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

// A crewed gunship flown by script: straight gun run, or orbit (loiter) the target.
class ACR_HeliRun : ACR_SupportRun
{
 static const float SPEED_RUN = 55;
 static const float SPEED_LOITER = 38;
 static const float LOITER_RADIUS = 330;
 static const float LOITER_TIME_MS = 90000;

 IEntity m_Vehicle;
 string m_sFaction;
 bool m_bLoiter;
 vector m_Target;
 vector m_Dir;
 vector m_Pos;
 float m_fYaw;
 float m_fRoll;
 ResourceName m_Rocket;
 int m_iRockets;
 float m_fNextRocket;
 int m_iPhase; // 0 inbound, 1 orbit, 2 outbound
 float m_fOrbitEnd;
 vector m_ExitDir;
 float m_fStarted;
 bool m_bDestroyed;

 bool Start(ACR_Supports supports, ResourceName prefab, float now)
 {
  m_Pos = m_Target - m_Dir * 2200;
  m_Pos[1] = SafeAltitude(m_Pos, m_Dir) ;
  m_fYaw = Math.Atan2(m_Dir[0], m_Dir[2]) * Math.RAD2DEG;
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
 static float SafeAltitude(vector pos, vector forward)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = 0;
  for (int i = 0; i < 4; i++)
  {
   vector probe = pos + forward * (i * 90);
   ground = Math.Max(ground, world.GetSurfaceY(probe[0], probe[2]));
  }
  return ground + 70;
 }

 bool IsDestroyed()
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(m_Vehicle.FindComponent(DamageManagerComponent));
  return damage && damage.GetState() == EDamageState.DESTROYED;
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

  vector flat = m_Target - m_Pos;
  flat[1] = 0;
  float distance = flat.Length();
  vector toTarget = flat.Normalized();
  vector desired = m_Dir;
  float speed = SPEED_RUN;
  if (m_bLoiter)
  {
   speed = SPEED_LOITER;
   if (m_iPhase == 0 && distance < LOITER_RADIUS + 40)
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
    vector tangent = Vector(-toTarget[2], 0, toTarget[0]);
    desired = (tangent + toTarget * ((distance - LOITER_RADIUS) / LOITER_RADIUS * 1.5)).Normalized();
   }
   else desired = m_ExitDir;
  }

  // Turn toward the desired heading at a helicopter-like rate, banking into it.
  float desiredYaw = Math.Atan2(desired[0], desired[2]) * Math.RAD2DEG;
  float delta = desiredYaw - m_fYaw;
  while (delta > 180) delta -= 360;
  while (delta < -180) delta += 360;
  float maxTurn = 22 * dt;
  float turn = Math.Clamp(delta, -maxTurn, maxTurn);
  m_fYaw += turn;
  float yawRate = 0;
  if (dt > 0) yawRate = turn / dt;
  m_fRoll += (Math.Clamp(-yawRate * 1.2, -25, 25) - m_fRoll) * Math.Min(1, dt * 2);

  vector forward = Vector(Math.Sin(m_fYaw * Math.DEG2RAD), 0, Math.Cos(m_fYaw * Math.DEG2RAD));
  vector previous = m_Pos;
  m_Pos = m_Pos + forward * (speed * dt);
  float wantedY = SafeAltitude(m_Pos, forward);
  m_Pos[1] = previous[1] + Math.Clamp(wantedY - previous[1], -10 * dt, 12 * dt);
  float pitch = -4;

  // Rockets: gun run fires one pass on the way in; loiter fires salvos at enemies near the target.
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
   else if (m_iPhase == 1)
   {
    IEntity enemy = supports.FindEnemyNear(m_sFaction, m_Target, 220);
    vector enemyFlat;
    if (enemy) enemyFlat = enemy.GetOrigin() - m_Pos;
    enemyFlat[1] = 0;
    // Only when the nose points roughly at the enemy (salvo of 4, then a pause).
    shoot = enemy != null && vector.Dot(enemyFlat.Normalized(), forward) > 0.2 && enemyFlat.Length() < 700;
    if (enemy) aim = enemy.GetOrigin() + Vector(Math.RandomFloat(-8, 8), 0.5, Math.RandomFloat(-8, 8));
   }
   if (shoot)
   {
    aim[1] = Math.Max(aim[1], GetGame().GetWorld().GetSurfaceY(aim[0], aim[2]) + 0.5);
    vector right = Vector(forward[2], 0, -forward[0]);
    float side = 1.6;
    if (m_iRockets % 2 == 0) side = -1.6;
    supports.FireProjectile(m_Rocket, m_Pos + forward * 6 + right * side - Vector(0, 1.2, 0), aim, forward * speed);
    m_iRockets--;
    PrintFormat("[ACR] SUPPORT heli rocket, %1 left, pos %2 range %3", m_iRockets, m_Pos, vector.Distance(m_Pos, aim));
    m_fNextRocket = now + 280;
    if (m_bLoiter && m_iRockets % 4 == 0) m_fNextRocket = now + 8000;
   }
  }

  vector transform[4];
  Math3D.AnglesToMatrix(Vector(m_fYaw, pitch, m_fRoll), transform);
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
 static int DefaultDelay(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 12;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 15;
   case ACR_ESupportType.MORTAR_CREEPING: return 15;
   case ACR_ESupportType.MORTAR_SMOKE: return 10;
   case ACR_ESupportType.ILLUMINATION: return 6;
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
  string message = string.Format("%1 on %2, ETA %3 s.", entry.m_sName, where, delay);
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
 }
}

// The game mode entity is replicated, so it carries the "launch this projectile" broadcast.
modded class SCR_BaseGameMode
{
 void ACR_BroadcastLaunch(RplId id, vector dir, vector parentVelocity)
 {
  Rpc(ACR_RpcLaunch, id, dir, parentVelocity);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
 protected void ACR_RpcLaunch(RplId id, vector dir, vector parentVelocity)
 {
  ACR_Supports.LaunchLocal(SCR_EntityHelper.RplIdToEntity(id), dir, parentVelocity);
 }
}
