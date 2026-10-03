// Range test for BBB_Aim. Starts with -bbbAimTest 1 (the ACA test launcher's preset
// "bbbaim" passes -acrBbbAimTest 1, the only kind of flag it accepts). Run it on a world
// without AC: Actual, which would take the test groups over: put
// worlds/GameMaster/GM_Arland.ent in place of "{world}" in tools/testserver-args.txt for
// the run, then put it back. About 45 minutes (REPEATS 3).
// Unit checks of the BBB_Aim maths first, then two-sided firefights on one lane found on
// the map (clear at crouch height to 500 m), far from any player but with a temporary
// observer at the lane:
//  Rifles: US light fire team vs USSR light fire team (4 rifles each) at 100/300/500 m
//          for 90 s, both firing at will; one side has BBB's aim, the other vanilla aim,
//          then the sides swap. Per aim type: shots fired and enemies downed (dead or
//          unconscious).
//  AT:     USSR AT team vs an M2 Humvee with a US gunner (vanilla aim) at 150/300 m for
//          60 s, BBB then vanilla aim for the AT team. Rockets, rocket hits (vehicle health
//          drops 0.05+ at once), seconds until the Humvee is destroyed.
// Logs "[BBB] AIMTEST ..." lines and ends with "[ACR] TEST COMPLETE" (the launcher waits for it).
// Learned building it (2 Oct): a lone shooter saw his target once, moved to cover and never
// fired; deactivated or hold-fire targets were hardly engaged; OnProjectileShot never fired
// for AI (shots are counted from the loaded magazines); lanes are turned off the map axes
// (vanilla's suppression volume divided by zero along an axis; BBB_Aim fixes that).
class BBB_AimSide : Managed
{
 bool m_bVanilla;
 SCR_AIGroup m_Group;
 ref array<IEntity> m_aMen = {};
 ref array<BaseWeaponComponent> m_aWeapons = {};
 ref array<int> m_aLastAmmo = {};
 int m_iShots;

 void SampleAmmo()
 {
  foreach (int i, BaseWeaponComponent weapon : m_aWeapons)
  {
   if (!weapon) continue;
   int ammo = 0;
   if (weapon.GetCurrentMagazine()) ammo = weapon.GetCurrentMagazine().GetAmmoCount();
   if (ammo < m_aLastAmmo[i]) m_iShots += m_aLastAmmo[i] - ammo;
   m_aLastAmmo[i] = ammo;
  }
 }

 string Label()
 {
  if (m_bVanilla) return "vanilla";
  return "BBB";
 }
}

class BBB_AimLane : Managed
{
 bool m_bAT;
 vector m_vFrom;
 vector m_vTo;
 vector m_vDir;
 ref BBB_AimSide m_A = new BBB_AimSide();
 ref BBB_AimSide m_B = new BBB_AimSide();
 IEntity m_Vehicle;
 bool m_bHooked;
 float m_fHookAt;
 float m_fProbeAt;
 float m_fStart;
 float m_fDestroyedAfter = -1;
 int m_iHits;
 float m_fHealth = 1;
 ref array<IEntity> m_aSpawned = {};
}

class BBB_AimTest : Managed
{
 static ref BBB_AimTest s_Test;

 static const ResourceName TEAM_US = "{FCF7F5DC4F83955C}Prefabs/Groups/BLUFOR/Group_US_LightFireTeam.et";
 static const ResourceName TEAM_USSR = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
 static const ResourceName TEAM_USSR_AT = "{96BAB56E6558788E}Prefabs/Groups/OPFOR/Group_USSR_Team_AT.et";
 static const ResourceName RIFLE_US = "{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et";
 static const ResourceName GUN_TRUCK = "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et";
 static const ResourceName GROUP_US = "{EACD97CF4A702FAE}Prefabs/Groups/BLUFOR/Group_US_Base.et";

 static const float RIFLE_S = 90;
 static const float AT_S = 60;
 static const int REPEATS = 3;
 // Suppression-only run (-bbbSuppressTest 1, preset "bbbsuppress" = -acrBbbSuppressTest 1,
 // ~16 min): fix 3 alone at SUPPRESS_TRY, rifles at 100 m only, SUPPRESS_REPEATS pairs.
 static const float SUPPRESS_TRY = 1.3;
 static const int SUPPRESS_REPEATS = 5;
 static bool s_bSuppressOnly;
 static const float EYE_M = 0.8;
 static const int OBSERVER_KEY = 22101;

 ref array<ref BBB_AimLane> m_aLanes = {};
 ref array<float> m_aCheckDistances = {100, 150, 300, 500};
 ref array<vector> m_aSiteFrom = {};
 ref array<vector> m_aSiteDir = {};
 int m_iRound = -1;
 ref array<bool> m_aRoundAT = {};
 ref array<float> m_aRoundDistance = {};
 ref array<bool> m_aRoundSwap = {};
 float m_fRoundEnd;
 float m_fNextRound;
 // Totals per "kind distance aim": shots, enemies downed (rifles) / hits, destroyed (AT).
 ref map<string, int> m_mShots = new map<string, int>();
 ref map<string, int> m_mDowned = new map<string, int>();
 ref map<string, int> m_mHits = new map<string, int>();
 ref array<string> m_aKeys = {};

 static void MaybeStart()
 {
  if (s_Test || !Replication.IsServer()) return;
  s_bSuppressOnly = System.IsCLIParam("bbbSuppressTest") || System.IsCLIParam("acrBbbSuppressTest");
  if (!s_bSuppressOnly && !System.IsCLIParam("bbbAimTest") && !System.IsCLIParam("acrBbbAimTest")) return;
  s_Test = new BBB_AimTest();
  GetGame().GetCallqueue().CallLater(s_Test.Begin, 10000);
 }

 protected float Now()
 {
  return GetGame().GetWorld().GetWorldTime() / 1000;
 }

 // ---------------------------------------------------------------- unit checks

 protected void Check(string name, bool ok, string detail)
 {
  string verdict = "FAIL";
  if (ok) verdict = "PASS";
  PrintFormat("[BBB] AIMTEST CHECK %1 %2 (%3)", name, verdict, detail);
 }

 protected void UnitChecks()
 {
  BBB_Aim.Read();
  // The test compares the proposed values with vanilla aim.
  BBB_Aim.UseTuning();
  if (s_bSuppressOnly)
  {
   BBB_Aim.s_fLauncher = 0;
   BBB_Aim.s_fLongRange = 0;
   BBB_Aim.s_fSuppressed = SUPPRESS_TRY;
   PrintFormat("[BBB] AIMTEST suppression only: x%1 when fully suppressed", SUPPRESS_TRY);
  }
  float far215 = BBB_Aim.FarFactor(215);
  float far600 = BBB_Aim.FarFactor(600);
  float far2000 = BBB_Aim.FarFactor(2000);
  Check("longrange-215", Math.AbsFloat(far215 - 1.4) < 0.001, far215.ToString());
  Check("longrange-600", Math.AbsFloat(far600 - BBB_Aim.s_fLongRange) < 0.001, far600.ToString());
  Check("longrange-cap", far2000 <= BBB_Aim.FarFactor(1000) + 0.001, far2000.ToString());
  float calm = BBB_Aim.ThreatFactor(0.2);
  float pinned = BBB_Aim.ThreatFactor(1);
  float mid = BBB_Aim.ThreatFactor(0.66);
  Check("suppressed-calm", Math.AbsFloat(calm - 1) < 0.001, calm.ToString());
  Check("suppressed-full", Math.AbsFloat(pinned - BBB_Aim.s_fSuppressed) < 0.001, pinned.ToString());
  Check("suppressed-mid", mid > 1 && mid < pinned, mid.ToString());
  Check("binoculars-calm", BBB_Aim.BinocularsAllowed(0, EAIThreatState.ALERTED), "allowed");
  Check("binoculars-shot-at", !BBB_Aim.BinocularsAllowed(SCR_EAIThreatSectorFlags.DIRECTED_AT_ME, EAIThreatState.ALERTED), "refused");
  Check("binoculars-hit", !BBB_Aim.BinocularsAllowed(SCR_EAIThreatSectorFlags.CAUSED_DAMAGE, EAIThreatState.SAFE), "refused");
  Check("binoculars-pinned", !BBB_Aim.BinocularsAllowed(0, EAIThreatState.THREATENED), "refused");
  vector along = BBB_Aim.OffAxis(Vector(0, 0, 1));
  Check("suppression-axis", along[0] != 0 && along[2] != 0, along.ToString());
 }

 // ---------------------------------------------------------------- site

 protected bool Visible(vector from, vector to)
 {
  BaseWorld world = GetGame().GetWorld();
  if (BBB_Terrain.IsWaterAt(to)) return false;
  // Crouched height at both ends: soldiers in a fight crouch.
  vector eye = BBB.Ground(from) + Vector(0, EYE_M, 0);
  vector aim = BBB.Ground(to) + Vector(0, EYE_M, 0);
  for (int i = 1; i < 50; i++)
  {
   float t = i;
   t = t / 50;
   vector p = eye + (aim - eye) * t;
   if (world.GetSurfaceY(p[0], p[2]) > p[1] - 0.15) return false;
  }
  TraceParam trace = new TraceParam();
  trace.Start = eye;
  trace.End = aim;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = world.TraceMove(trace, null);
  return fraction >= 0.99;
 }

 protected bool LaneClear(vector from, vector dir)
 {
  if (BBB_Terrain.IsWaterAt(from)) return false;
  foreach (float distance : m_aCheckDistances)
  {
   if (!Visible(from, from + dir * distance)) return false;
  }
  return true;
 }

 // One lane (shooter spot + heading). Headings are turned 5 degrees off the map axes.
 protected bool FindSite()
 {
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  array<vector> dirs = {Vector(0.087, 0, 0.996), Vector(0.996, 0, -0.087), Vector(-0.087, 0, -0.996), Vector(-0.996, 0, 0.087)};
  int tested;
  for (float x = mins[0] + 600; x < maxs[0] - 600; x += 100)
  {
   for (float z = mins[2] + 600; z < maxs[2] - 600; z += 100)
   {
    vector from = Vector(x, 0, z);
    foreach (vector dir : dirs)
    {
     tested++;
     if (!LaneClear(from, dir)) continue;
     m_aSiteFrom.Insert(from);
     m_aSiteDir.Insert(dir);
     PrintFormat("[BBB] AIMTEST lane at %1 heading %2 (%3 tried)", from, dir, tested);
     return true;
    }
   }
  }
  PrintFormat("[BBB] AIMTEST no clear lane in %1 tries", tested);
  return false;
 }

 protected NavmeshWorldComponent Navmesh()
 {
  if (!GetGame().GetAIWorld()) return null;
  return GetGame().GetAIWorld().GetNavmeshWorldComponent("Soldiers");
 }

 // The soldier navmesh streams in tiles; ask for the lane's before the first round.
 protected void LoadLane()
 {
  NavmeshWorldComponent navmesh = Navmesh();
  if (!navmesh) return;
  for (int i = -1; i <= 11; i++)
  {
   vector p = m_aSiteFrom[0] + m_aSiteDir[0] * (i * 50);
   navmesh.LoadTileIn(p);
  }
 }

 // ---------------------------------------------------------------- spawning

 protected IEntity Spawn(ResourceName prefab, vector at, vector facing)
 {
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  vector angles = facing.VectorToAngles();
  angles[1] = 0;
  angles[2] = 0;
  Math3D.AnglesToMatrix(angles, parameters.Transform);
  parameters.Transform[3] = BBB.Ground(at);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
 }

 protected void PinDetail(AIAgent agent)
 {
  if (!agent) return;
  agent.SetPermanentLOD(0);
  agent.PreventMaxLOD();
 }

 // Dead or unconscious: most hits knock a soldier out rather than kill him.
 protected bool Down(IEntity man)
 {
  if (!man || !BBB.IsAlive(man)) return true;
  ChimeraCharacter character = ChimeraCharacter.Cast(man);
  return character && character.GetCharacterController() && character.GetCharacterController().IsUnconscious();
 }

 protected int Downed(BBB_AimSide side)
 {
  int count;
  foreach (IEntity man : side.m_aMen)
  {
   if (Down(man)) count++;
  }
  return count;
 }

 // A team's members (they come a few frames after the group), their weapons of one kind
 // (launchers or not) for shot counting, and the aim they use.
 protected void Hook(BBB_AimSide side, bool launchers)
 {
  side.m_aMen.Clear();
  if (!side.m_Group) return;
  PinDetail(side.m_Group);
  array<AIAgent> agents = {};
  side.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   PinDetail(agent);
   IEntity man = agent.GetControlledEntity();
   if (!man) continue;
   side.m_aMen.Insert(man);
   if (side.m_bVanilla) BBB_Aim.s_aVanilla.Insert(man);
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
   if (!weapons) continue;
   array<IEntity> carried = {};
   weapons.GetWeaponsList(carried);
   foreach (IEntity item : carried)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
    if (!weapon) continue;
    bool launcher = weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER;
    if (launchers != launcher) continue;
    int ammo = 0;
    if (weapon.GetCurrentMagazine()) ammo = weapon.GetCurrentMagazine().GetAmmoCount();
    side.m_aWeapons.Insert(weapon);
    side.m_aLastAmmo.Insert(ammo);
   }
  }
 }

 // The armed Humvee and its gunner (a group of one, vanilla aim).
 protected void SpawnGunTruck(BBB_AimLane lane)
 {
  lane.m_Vehicle = Spawn(GUN_TRUCK, lane.m_vTo, Vector(lane.m_vDir[2], 0, -lane.m_vDir[0]));
  if (!lane.m_Vehicle) return;
  lane.m_aSpawned.Insert(lane.m_Vehicle);
  lane.m_fHealth = BBB.Health(lane.m_Vehicle);
  vector at = lane.m_vTo + lane.m_vDir * 6;
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(Spawn(GROUP_US, at, lane.m_vDir));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return;
  lane.m_aSpawned.Insert(group);
  if (!group.m_aUnitPrefabSlots) group.m_aUnitPrefabSlots = {};
  group.m_aUnitPrefabSlots.Clear();
  group.m_aUnitPrefabSlots.Insert(RIFLE_US);
  IEntity gunner = Spawn(RIFLE_US, at, lane.m_vDir);
  if (!gunner) return;
  lane.m_aSpawned.Insert(gunner);
  group.AddAIEntityToGroup(gunner);
  lane.m_B.m_Group = group;
  ChimeraCharacter character = ChimeraCharacter.Cast(gunner);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(lane.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (character && compartments && character.GetCompartmentAccessComponent())
  {
   array<BaseCompartmentSlot> seats = {};
   compartments.GetCompartmentsOfType(seats, ECompartmentType.TURRET);
   if (!seats.IsEmpty()) character.GetCompartmentAccessComponent().GetInVehicle(lane.m_Vehicle, seats[0], true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
 }

 protected void Clear(BBB_AimLane lane)
 {
  array<BBB_AimSide> sides = {lane.m_A, lane.m_B};
  foreach (BBB_AimSide side : sides)
  {
   foreach (IEntity man : side.m_aMen)
   {
    BBB_Aim.s_aVanilla.RemoveItem(man);
    if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
   }
  }
  foreach (IEntity entity : lane.m_aSpawned)
  {
   if (entity) SCR_EntityHelper.DeleteEntityAndChildren(entity);
  }
  lane.m_aSpawned.Clear();
 }

 // ---------------------------------------------------------------- rounds

 protected void Begin()
 {
  UnitChecks();
  if (!FindSite())
  {
   Print("[BBB] AIMTEST range part skipped");
   Print("[ACR] TEST COMPLETE");
   return;
  }
  LoadLane();
  // A temporary observer at the lane, as if a player stood there: the fight then gets the
  // same simulation detail as one next to a player.
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers)
  {
   vector mid = m_aSiteFrom[0] + m_aSiteDir[0] * 250;
   observers.InsertObserverSP(OBSERVER_KEY, mid[0], mid[2], null);
  }
  PlanRounds();
  m_fNextRound = Now() + 5;
  GetGame().GetCallqueue().CallLater(Tick, 500, true);
 }

 // Each repeat: rifles at 100/300/500 m (US side BBB, then USSR side BBB), then AT at
 // 150/300 m (AT team BBB, then vanilla).
 protected void PlanRounds()
 {
  array<float> rifle = {100, 300, 500};
  array<float> antitank = {150, 300};
  int repeats = REPEATS;
  if (s_bSuppressOnly)
  {
   rifle.Clear();
   rifle.Insert(100);
   antitank.Clear();
   repeats = SUPPRESS_REPEATS;
  }
  for (int r = 0; r < repeats; r++)
  {
   foreach (float d : rifle)
   {
    m_aRoundAT.Insert(false); m_aRoundDistance.Insert(d); m_aRoundSwap.Insert(false);
    m_aRoundAT.Insert(false); m_aRoundDistance.Insert(d); m_aRoundSwap.Insert(true);
   }
   foreach (float e : antitank)
   {
    m_aRoundAT.Insert(true); m_aRoundDistance.Insert(e); m_aRoundSwap.Insert(false);
    m_aRoundAT.Insert(true); m_aRoundDistance.Insert(e); m_aRoundSwap.Insert(true);
   }
  }
 }

 protected bool StartRound(int round)
 {
  if (round >= m_aRoundAT.Count()) return false;
  bool at = m_aRoundAT[round];
  bool swap = m_aRoundSwap[round];
  float distance = m_aRoundDistance[round];
  m_aLanes.Clear();
  BBB_AimLane lane = new BBB_AimLane();
  lane.m_bAT = at;
  lane.m_vFrom = m_aSiteFrom[0];
  lane.m_vDir = m_aSiteDir[0];
  lane.m_vTo = lane.m_vFrom + lane.m_vDir * distance;
  lane.m_A.m_bVanilla = swap;
  lane.m_B.m_bVanilla = !swap || at;
  ResourceName team = TEAM_US;
  if (at) team = TEAM_USSR_AT;
  lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(team, lane.m_vFrom, lane.m_vDir));
  if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
  if (at) SpawnGunTruck(lane);
  else
  {
   lane.m_B.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_USSR, lane.m_vTo, lane.m_vDir * -1));
   if (lane.m_B.m_Group) lane.m_aSpawned.Insert(lane.m_B.m_Group);
  }
  lane.m_fStart = Now();
  lane.m_fHookAt = Now() + 4;
  lane.m_fProbeAt = Now() + 12;
  m_aLanes.Insert(lane);
  float length = RIFLE_S;
  if (at) length = AT_S;
  m_fRoundEnd = Now() + length;
  if (at) PrintFormat("[BBB] AIMTEST round %1/%2: AT team (%3 aim) vs M2 Humvee at %4 m", round + 1, m_aRoundAT.Count(), lane.m_A.Label(), distance);
  else PrintFormat("[BBB] AIMTEST round %1/%2: rifles at %3 m, US %4 aim vs USSR %5 aim", round + 1, m_aRoundAT.Count(), distance, lane.m_A.Label(), lane.m_B.Label());
  return true;
 }

 protected void Tick()
 {
  float now = Now();
  if (m_aLanes.IsEmpty())
  {
   if (now < m_fNextRound) return;
   m_iRound++;
   if (!StartRound(m_iRound)) Finish();
   return;
  }
  foreach (BBB_AimLane lane : m_aLanes)
  {
   if (!lane.m_bHooked && now >= lane.m_fHookAt)
   {
    lane.m_bHooked = true;
    Hook(lane.m_A, lane.m_bAT);
    Hook(lane.m_B, false);
   }
   if (lane.m_fProbeAt > 0 && now >= lane.m_fProbeAt)
   {
    lane.m_fProbeAt = 0;
    Probe(lane);
   }
   lane.m_A.SampleAmmo();
   lane.m_B.SampleAmmo();
   WatchVehicle(lane, now);
  }
  if (now >= m_fRoundEnd) EndRound();
 }

 protected void Probe(BBB_AimLane lane)
 {
  PrintFormat("[BBB] AIMTEST probe at 12 s: side A %1 men %2 shots, side B %3 men %4 shots", lane.m_A.m_aMen.Count(), lane.m_A.m_iShots, lane.m_B.m_aMen.Count(), lane.m_B.m_iShots);
 }

 protected void WatchVehicle(BBB_AimLane lane, float now)
 {
  if (!lane.m_bAT || !lane.m_Vehicle || lane.m_fDestroyedAfter >= 0) return;
  float health = BBB.Health(lane.m_Vehicle);
  if (lane.m_fHealth - health >= 0.05) lane.m_iHits++;
  lane.m_fHealth = health;
  if (health <= 0) lane.m_fDestroyedAfter = now - lane.m_fStart;
 }

 protected void Count(string key, int shots, int downed, int hits)
 {
  if (!m_aKeys.Contains(key)) m_aKeys.Insert(key);
  m_mShots.Set(key, m_mShots.Get(key) + shots);
  m_mDowned.Set(key, m_mDowned.Get(key) + downed);
  m_mHits.Set(key, m_mHits.Get(key) + hits);
 }

 protected void EndRound()
 {
  foreach (BBB_AimLane lane : m_aLanes)
  {
   int distance = Math.Round(vector.DistanceXZ(lane.m_vFrom, lane.m_vTo));
   if (lane.m_bAT)
   {
    int destroyed = 0;
    if (lane.m_fDestroyedAfter >= 0) destroyed = 1;
    PrintFormat("[BBB] AIMTEST AT %1 m %2: rockets %3, hits %4, destroyed after %5 s, health left %6 (AT team down %7 of %8)", distance, lane.m_A.Label(), lane.m_A.m_iShots, lane.m_iHits, lane.m_fDestroyedAfter, BBB.Health(lane.m_Vehicle), Downed(lane.m_A), lane.m_A.m_aMen.Count());
    Count(string.Format("AT %1 m %2", distance, lane.m_A.Label()), lane.m_A.m_iShots, destroyed, lane.m_iHits);
   }
   else
   {
    int downA = Downed(lane.m_A);
    int downB = Downed(lane.m_B);
    PrintFormat("[BBB] AIMTEST rifles %1 m: US (%2) %3 shots, downed %4 of %5 USSR; USSR (%6) %7 shots, downed %8 of %9 US", distance, lane.m_A.Label(), lane.m_A.m_iShots, downB, lane.m_B.m_aMen.Count(), lane.m_B.Label(), lane.m_B.m_iShots, downA, lane.m_A.m_aMen.Count());
    Count(string.Format("rifle %1 m %2", distance, lane.m_A.Label()), lane.m_A.m_iShots, downB, 0);
    Count(string.Format("rifle %1 m %2", distance, lane.m_B.Label()), lane.m_B.m_iShots, downA, 0);
   }
   Clear(lane);
  }
  m_aLanes.Clear();
  m_fNextRound = Now() + 4;
 }

 protected void Finish()
 {
  GetGame().GetCallqueue().Remove(Tick);
  foreach (string key : m_aKeys)
  {
   int shots = m_mShots.Get(key);
   int downed = m_mDowned.Get(key);
   if (key.StartsWith("AT")) PrintFormat("[BBB] AIMTEST TOTAL %1: rockets %2, hits %3, destroyed %4", key, shots, m_mHits.Get(key), downed);
   else
   {
    float perDown = -1;
    if (downed > 0)
    {
     perDown = shots;
     perDown = perDown / downed;
    }
    PrintFormat("[BBB] AIMTEST TOTAL %1: shots %2, enemies downed %3, shots per enemy downed %4", key, shots, downed, perDown);
   }
  }
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.RemoveObserverSP(OBSERVER_KEY);
  Print("[BBB] AIMTEST done");
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_AimTest.MaybeStart();
 }
}
