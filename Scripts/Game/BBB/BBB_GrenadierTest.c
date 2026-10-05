// Do AI grenadiers use their 40 mm on light armour, and does it work? (Suggestion 4.)
// -bbbGrenadierTest 1 (preset "bbbgrenadier" passes -acrBbbGrenadierTest 1); a world
// without AC: Actual (GM_Arland swap). About 20 minutes.
// Two grenadiers (US: M203 + M433 HEDP; USSR: GP-25 + VOG-25 frag) on the aim lane against
// an APC with only an enemy driver aboard (no gunner, so no return fire) at 100/200/300 m,
// 60 s a round: US vs BRDM-2, US vs BTR-70, USSR vs LAV-25, REPEATS times. Per round:
// 40 mm fired, rifle rounds fired, what their weapon picker chose (sampled), every hit on the
// vehicle by damage type and source with effective damage, health left. "[BBB] GLTEST ...".
class BBB_GrenadierTest : BBB_AimTest
{
 static ref BBB_GrenadierTest s_GL;
 static bool s_bNear; // preset bbbglnear: US only, 75/100/150 m, to see why they hold fire up close
 static const ResourceName GL_US = "{84029128FA6F6BB9}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_GL.et";
 static const ResourceName GL_USSR = "{8E0FE664CE7D1CA9}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_GL.et";
 static const ResourceName BRDM = "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et";
 static const ResourceName BTR = "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et";
 static const ResourceName LAV = "{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et";
 static const ResourceName GROUP_USSR = "{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et";
 static const ResourceName RIFLE_USSR = "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et";
 static const int GL_REPEATS = 2;
 static const float ROUND_S = 60;

 protected ref array<int> m_aKind = {};      // 0 US v BRDM, 1 US v BTR, 2 USSR v LAV
 protected ref array<float> m_aRange = {};
 protected int m_iKind;
 protected IEntity m_Vehicle;
 protected ref array<IEntity> m_aGL = {};
 protected int m_iGLStart;
 protected int m_iSamplesGL;
 protected int m_iSamplesRifle;
 protected int m_iSamplesNone;
 protected float m_fNextSample;
 protected int m_iMaxEnemy;
 protected int m_iMaxDetected;
 protected int m_iMaxCantAttack;
 protected IEntity m_Driver;
 protected int m_iDriverOut;
 protected int m_iDriverSamples;
 protected string m_sTargets;
 protected ref map<string, int> m_mTotal = new map<string, int>();
 protected ref map<string, float> m_mTotalF = new map<string, float>();
 protected ref array<string> m_aTKeys = {};

 static void MaybeStartGL()
 {
  if (s_GL || !Replication.IsServer()) return;
  s_bNear = System.IsCLIParam("acrBbbGrenadierNearTest");
  if (!s_bNear && !System.IsCLIParam("bbbGrenadierTest") && !System.IsCLIParam("acrBbbGrenadierTest")) return;
  s_GL = new BBB_GrenadierTest();
  GetGame().GetCallqueue().CallLater(s_GL.Begin, 10000);
 }

 void BBB_GrenadierTest()
 {
  m_aCheckDistances = {100, 200, 300};
 }

 override protected void Begin()
 {
  if (!FindSite())
  {
   Print("[BBB] GLTEST no lane");
   Print("[ACR] TEST COMPLETE");
   return;
  }
  LoadLane();
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers)
  {
   vector mid = m_aSiteFrom[0] + m_aSiteDir[0] * 150;
   observers.InsertObserverSP(OBSERVER_KEY, mid[0], mid[2], null);
  }
  array<float> ranges = {100, 200, 300};
  int kinds = 3;
  int repeats = GL_REPEATS;
  if (s_bNear)
  {
   // Why do US grenadiers not even see a BRDM/BTR at 100 m? Swap who is in what.
   array<int> near = {0, 3, 4, 2, 0};
   foreach (int k : near) { m_aKind.Insert(k); m_aRange.Insert(100); }
  }
  else
  {
   for (int r = 0; r < repeats; r++)
    for (int k = 0; k < kinds; k++)
     foreach (float d : ranges) { m_aKind.Insert(k); m_aRange.Insert(d); }
  }
  m_fNextRound = Now() + 5;
  GetGame().GetCallqueue().CallLater(Tick, 500, true);
 }

 protected SCR_AIGroup EmptyGroup(ResourceName groupPrefab, ResourceName slot, int slots, vector at, vector dir, BBB_AimLane lane)
 {
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(Spawn(groupPrefab, at, dir));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return null;
  lane.m_aSpawned.Insert(group);
  if (!group.m_aUnitPrefabSlots) group.m_aUnitPrefabSlots = {};
  group.m_aUnitPrefabSlots.Clear();
  for (int i = 0; i < slots; i++) group.m_aUnitPrefabSlots.Insert(slot);
  return group;
 }

 protected int FortyMil(IEntity man)
 {
  if (!man) return 0;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
  int n;
  if (inventory)
  {
   n += inventory.GetDepositItemCountByResource(man, "{1663496AE5B9F10B}Prefabs/Weapons/Ammo/Ammo_Grenade_HEDP_M433.et");
   n += inventory.GetDepositItemCountByResource(man, "{262F0D09C4130826}Prefabs/Weapons/Ammo/Ammo_Grenade_HE_VOG25.et");
  }
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
  if (weapons)
  {
   array<IEntity> list = {};
   weapons.GetWeaponsList(list);
   foreach (IEntity item : list)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
    if (!weapon) continue;
    array<BaseMuzzleComponent> muzzles = {};
    weapon.GetMuzzlesList(muzzles);
    foreach (BaseMuzzleComponent muzzle : muzzles)
     if (muzzle.GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) n += muzzle.GetAmmoCount();
   }
  }
  return n;
 }

 protected int FortyMilAll()
 {
  int n;
  foreach (IEntity man : m_aGL) n += FortyMil(man);
  return n;
 }

 override protected bool StartRound(int round)
 {
  if (round >= m_aKind.Count()) return false;
  m_iKind = m_aKind[round];
  float range = m_aRange[round];
  m_aLanes.Clear();
  m_aGL.Clear();
  BBB_AimLane lane = new BBB_AimLane();
  lane.m_vFrom = m_aSiteFrom[0];
  lane.m_vDir = m_aSiteDir[0];
  lane.m_vTo = lane.m_vFrom + lane.m_vDir * range;
  // Kinds: 0 US GL v BRDM, 1 US GL v BTR, 2 USSR GL v LAV, 3 USSR GL v BRDM (US driver),
  // 4 US GL v LAV (USSR driver).
  bool us = m_iKind == 0 || m_iKind == 1 || m_iKind == 4;
  ResourceName glMan = GL_USSR;
  ResourceName glGroup = GROUP_USSR;
  ResourceName driverMan = RIFLE_US;
  ResourceName driverGroup = GROUP_US;
  if (us) { glMan = GL_US; glGroup = GROUP_US; driverMan = RIFLE_USSR; driverGroup = GROUP_USSR; }
  ResourceName vehicle = BRDM;
  if (m_iKind == 1) vehicle = BTR;
  if (m_iKind == 2 || m_iKind == 4) vehicle = LAV;
  SCR_AIGroup team = EmptyGroup(glGroup, glMan, 2, lane.m_vFrom, lane.m_vDir, lane);
  vector side = Vector(lane.m_vDir[2], 0, -lane.m_vDir[0]);
  for (int i = 0; i < 2 && team; i++)
  {
   IEntity man = Spawn(glMan, lane.m_vFrom + side * (i * 4), lane.m_vDir);
   if (!man) continue;
   lane.m_aSpawned.Insert(man);
   team.AddAIEntityToGroup(man);
   m_aGL.Insert(man);
  }
  lane.m_A.m_Group = team;
  m_Vehicle = Spawn(vehicle, lane.m_vTo, side);
  if (m_Vehicle) lane.m_aSpawned.Insert(m_Vehicle);
  BBB_Armour.s_Watch = m_Vehicle;
  BBB_Armour.s_mWatchHits.Clear();
  BBB_Armour.s_mWatchDamage.Clear();
  SCR_AIGroup crew = EmptyGroup(driverGroup, driverMan, 1, lane.m_vTo + lane.m_vDir * 5, lane.m_vDir, lane);
  if (crew && m_Vehicle)
  {
   IEntity driver = Spawn(driverMan, lane.m_vTo + lane.m_vDir * 5, lane.m_vDir);
   m_Driver = driver;
   if (driver)
   {
    lane.m_aSpawned.Insert(driver);
    crew.AddAIEntityToGroup(driver);
    ChimeraCharacter character = ChimeraCharacter.Cast(driver);
    SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
    if (character && compartments && character.GetCompartmentAccessComponent())
    {
     array<BaseCompartmentSlot> seats = {};
     compartments.GetCompartmentsOfType(seats, ECompartmentType.PILOT);
     if (!seats.IsEmpty()) character.GetCompartmentAccessComponent().GetInVehicle(m_Vehicle, seats[0], true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
    }
   }
   SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(crew.FindComponent(SCR_AIGroupUtilityComponent));
   if (utility) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  }
  lane.m_fStart = Now();
  lane.m_fHookAt = Now() + 4;
  m_aLanes.Insert(lane);
  m_fRoundEnd = Now() + ROUND_S;
  m_iGLStart = -1;
  m_iSamplesGL = 0;
  m_iSamplesRifle = 0;
  m_iSamplesNone = 0;
  m_iMaxEnemy = 0;
  m_iMaxDetected = 0;
  m_iMaxCantAttack = 0;
  m_iDriverOut = 0;
  m_iDriverSamples = 0;
  m_sTargets = "";
  m_fNextSample = Now() + 6;
  PrintFormat("[BBB] GLTEST round %1/%2: %3 at %4 m", round + 1, m_aKind.Count(), KindName(m_iKind), range);
  return true;
 }

 protected string KindName(int kind)
 {
  if (kind == 0) return "US HEDP v BRDM-2";
  if (kind == 1) return "US HEDP v BTR-70";
  if (kind == 3) return "USSR VOG-25 v BRDM-2 (US driver)";
  if (kind == 4) return "US HEDP v LAV-25 (USSR driver)";
  return "USSR VOG-25 v LAV-25";
 }

 protected void Sample()
 {
  ChimeraCharacter driver = ChimeraCharacter.Cast(m_Driver);
  if (driver && !Down(driver)) { m_iDriverSamples++; if (!driver.IsInVehicle()) m_iDriverOut++; }
  foreach (IEntity man : m_aGL)
  {
   if (!man || Down(man)) continue;
   AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
   if (!control || !control.GetAIAgent()) continue;
   SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(man.FindComponent(SCR_AICombatComponent));
   PerceptionComponent perception = PerceptionComponent.Cast(man.FindComponent(PerceptionComponent));
   if (perception)
   {
    m_iMaxEnemy = Math.Max(m_iMaxEnemy, perception.GetTargetCount(ETargetCategory.ENEMY));
    m_iMaxDetected = Math.Max(m_iMaxDetected, perception.GetTargetCount(ETargetCategory.DETECTED));
   }
   if (combat && combat.GetRetreatTarget()) m_iMaxCantAttack++;
   if (!combat || !combat.GetCurrentTarget()) { m_iSamplesNone++; continue; }
   IEntity what = combat.GetCurrentTarget().GetTargetEntity();
   string kind = "other";
   if (what == m_Vehicle) kind = "vehicle";
   else if (what == m_Driver && driver && driver.IsInVehicle()) kind = "driver inside";
   else if (what == m_Driver) kind = "driver outside";
   if (!m_sTargets.Contains(kind)) m_sTargets += " " + kind;
   bool gl = false;
   BaseWeaponComponent weapon = combat.BBBSelectedWeapon();
   if (weapon)
   {
    array<BaseMuzzleComponent> muzzles = {};
    weapon.GetMuzzlesList(muzzles);
    int m = combat.BBBSelectedMuzzle();
    if (m >= 0 && m < muzzles.Count() && muzzles[m].GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) gl = true;
   }
   if (gl) m_iSamplesGL++;
   else m_iSamplesRifle++;
  }
 }

 override protected void Tick()
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
    Hook(lane.m_A, false);
    m_iGLStart = FortyMilAll();
   }
   lane.m_A.SampleAmmo();
  }
  if (now >= m_fNextSample) { m_fNextSample = now + 2; Sample(); }
  if (now >= m_fRoundEnd) EndRound();
 }

 protected void Add(string key, int v, float f)
 {
  if (!m_aTKeys.Contains(key)) m_aTKeys.Insert(key);
  m_mTotal.Set(key, m_mTotal.Get(key) + v);
  m_mTotalF.Set(key, m_mTotalF.Get(key) + f);
 }

 override protected void EndRound()
 {
  foreach (BBB_AimLane lane : m_aLanes)
  {
   int range = Math.Round(vector.DistanceXZ(lane.m_vFrom, lane.m_vTo));
   int fired = 0;
   if (m_iGLStart >= 0) fired = m_iGLStart - FortyMilAll();
   string hits = "";
   int explosive;
   float explosiveDamage;
   foreach (string key, int n : BBB_Armour.s_mWatchHits)
   {
    float dmg = BBB_Armour.s_mWatchDamage.Get(key);
    hits += string.Format(" [%1: %2 hits, %3 dmg]", key, n, Math.Round(dmg));
    if (!key.StartsWith("KINETIC")) { explosive += n; explosiveDamage += dmg; }
   }
   string picks = string.Format("picks 40mm %1 / rifle %2 / no target %3 (max enemies known %4, detected %5, can't-attack samples %6)", m_iSamplesGL, m_iSamplesRifle, m_iSamplesNone, m_iMaxEnemy, m_iMaxDetected, m_iMaxCantAttack);
   PrintFormat("[BBB] GLTEST %1 %2 m: driver out of the vehicle in %3 of %4 samples; targets:%5", KindName(m_iKind), range, m_iDriverOut, m_iDriverSamples, m_sTargets);
   PrintFormat("[BBB] GLTEST %1 %2 m: 40mm fired %3, rifle rounds %4, %5, health left %6, hits:%7", KindName(m_iKind), range, fired, lane.m_A.m_iShots, picks, BBB.Health(m_Vehicle), hits);
   string key2 = string.Format("%1 %2 m", KindName(m_iKind), range);
   int killed = 0;
   if (BBB.Health(m_Vehicle) <= 0) killed = 1;
   Add(key2 + " 40mm fired", fired, 0);
   Add(key2 + " rifle rounds", lane.m_A.m_iShots, 0);
   Add(key2 + " picks 40mm", m_iSamplesGL, 0);
   Add(key2 + " picks rifle", m_iSamplesRifle, 0);
   Add(key2 + " non-kinetic hits / dmg", explosive, explosiveDamage);
   Add(key2 + " destroyed", killed, 0);
   BBB_Armour.s_Watch = null;
   Clear(lane);
  }
  m_aLanes.Clear();
  m_fNextRound = Now() + 4;
 }

 override protected void Finish()
 {
  GetGame().GetCallqueue().Remove(Tick);
  foreach (string key : m_aTKeys) PrintFormat("[BBB] GLTEST TOTAL %1: %2 (%3)", key, m_mTotal.Get(key), Math.Round(m_mTotalF.Get(key)));
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.RemoveObserverSP(OBSERVER_KEY);
  Print("[BBB] GLTEST done");
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_GrenadierTest.MaybeStartGL();
 }
}
