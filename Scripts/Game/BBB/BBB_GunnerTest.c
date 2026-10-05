// Firefight test for the gunner contact drill (BBB_Gunner). -bbbGunnerTest 1 (preset
// "bbbgunner" passes -acrBbbGunnerTest 1); a world without AC: Actual, like the aim test
// (GM_Arland via tools/testserver-args.txt). About 27 minutes.
// An M2 Humvee with a lone US gunner against a USSR light fire team (4 rifles) on the aim
// test's lane, at 350 and 600 m, 90 s a round, drill on and off in turn (REPEATS pairs per
// distance). Per round: seconds until the gunner's first burst, rounds he fired, drills
// started, fire team men down, gunner down. Logs "[BBB] GUNNERTEST ...".
class BBB_GunnerTest : BBB_AimTest
{
 static ref BBB_GunnerTest s_Gunner;
 static const int GUNNER_REPEATS = 4;
 static const float ROUND_S = 90;

 protected ref array<bool> m_aDrill = {};
 protected int m_iTurretLast = -1;
 protected int m_iTurretShots;
 protected float m_fFirstShot = -1;
 protected int m_iDrillsAtStart;
 protected bool m_bDrillRound;
 // Totals per "distance on|off".
 protected ref map<string, float> m_mFirst = new map<string, float>();
 protected ref map<string, int> m_mFirstN = new map<string, int>();
 protected ref map<string, int> m_mRounds = new map<string, int>();
 protected ref map<string, int> m_mFired = new map<string, int>();
 protected ref map<string, int> m_mDown = new map<string, int>();
 protected ref map<string, int> m_mGunnerDown = new map<string, int>();
 protected ref map<string, int> m_mDrills = new map<string, int>();
 protected ref array<string> m_aGKeys = {};

 void BBB_GunnerTest()
 {
  m_aCheckDistances = {100, 300, 450, 600};
 }

 static void MaybeStartGunner()
 {
  if (s_Gunner || !Replication.IsServer()) return;
  if (!System.IsCLIParam("bbbGunnerTest") && !System.IsCLIParam("acrBbbGunnerTest")) return;
  s_Gunner = new BBB_GunnerTest();
  GetGame().GetCallqueue().CallLater(s_Gunner.Begin, 10000);
 }

 override protected void Begin()
 {
  BBB_Gunner.Read();
  float far = BBB_Gunner.Guess(Vector(0, 0, 0), Vector(0, 0, 500), 500).Length();
  Check("guess-range", far >= 370 && far <= 655, far.ToString());
  if (!FindSite())
  {
   Print("[BBB] GUNNERTEST no lane");
   Print("[ACR] TEST COMPLETE");
   return;
  }
  LoadLane();
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers)
  {
   vector mid = m_aSiteFrom[0] + m_aSiteDir[0] * 300;
   observers.InsertObserverSP(OBSERVER_KEY, mid[0], mid[2], null);
  }
  for (int r = 0; r < GUNNER_REPEATS; r++)
  {
   array<float> distances = {350, 600};
   foreach (float d : distances)
   {
    // Alternate which goes first so drift over the run doesn't favour one.
    bool first = (r % 2) == 0;
    m_aRoundAT.Insert(true); m_aRoundDistance.Insert(d); m_aDrill.Insert(first);
    m_aRoundAT.Insert(true); m_aRoundDistance.Insert(d); m_aDrill.Insert(!first);
   }
  }
  m_fNextRound = Now() + 5;
  GetGame().GetCallqueue().CallLater(Tick, 500, true);
 }

 override protected bool StartRound(int round)
 {
  if (round >= m_aRoundAT.Count()) return false;
  float distance = m_aRoundDistance[round];
  m_bDrillRound = m_aDrill[round];
  BBB_Gunner.s_bDrill = m_bDrillRound;
  m_aLanes.Clear();
  BBB_AimLane lane = new BBB_AimLane();
  lane.m_bAT = false;
  lane.m_vFrom = m_aSiteFrom[0];
  lane.m_vDir = m_aSiteDir[0];
  lane.m_vTo = lane.m_vFrom + lane.m_vDir * distance;
  lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_USSR, lane.m_vFrom, lane.m_vDir));
  if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
  SpawnGunTruck(lane);
  lane.m_fStart = Now();
  lane.m_fHookAt = Now() + 4;
  m_aLanes.Insert(lane);
  m_fRoundEnd = Now() + ROUND_S;
  m_iTurretLast = -1;
  m_iTurretShots = 0;
  m_fFirstShot = -1;
  m_iDrillsAtStart = BBB_Gunner.s_iDrills;
  PrintFormat("[BBB] GUNNERTEST round %1/%2: M2 Humvee vs fire team at %3 m, drill %4", round + 1, m_aRoundAT.Count(), distance, m_bDrillRound);
  return true;
 }

 protected int TurretAmmo(IEntity vehicle)
 {
  if (!vehicle) return -1;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return -1;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartmentsOfType(seats, ECompartmentType.TURRET);
  if (seats.IsEmpty()) return -1;
  TurretControllerComponent controller = TurretControllerComponent.Cast(seats[0].GetController());
  if (!controller || !controller.GetWeaponManager()) return -1;
  BaseWeaponComponent weapon = controller.GetWeaponManager().GetCurrentWeapon();
  if (!weapon || !weapon.GetCurrentMagazine()) return -1;
  return weapon.GetCurrentMagazine().GetAmmoCount();
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
    Hook(lane.m_B, false);
   }
   int ammo = TurretAmmo(lane.m_Vehicle);
   if (ammo >= 0)
   {
    if (m_iTurretLast >= 0 && ammo < m_iTurretLast)
    {
     m_iTurretShots += m_iTurretLast - ammo;
     if (m_fFirstShot < 0) m_fFirstShot = now - lane.m_fStart;
    }
    m_iTurretLast = ammo;
   }
  }
  if (now >= m_fRoundEnd) EndRound();
 }

 override protected void EndRound()
 {
  foreach (BBB_AimLane lane : m_aLanes)
  {
   int distance = Math.Round(vector.DistanceXZ(lane.m_vFrom, lane.m_vTo));
   string mode = "off";
   if (m_bDrillRound) mode = "on";
   string key = string.Format("%1 m drill %2", distance, mode);
   int down = Downed(lane.m_A);
   int gunnerDown = Downed(lane.m_B);
   int drills = BBB_Gunner.s_iDrills - m_iDrillsAtStart;
   PrintFormat("[BBB] GUNNERTEST %1: first burst after %2 s, rounds fired %3, drills %4, fire team down %5 of %6, gunner down %7", key, m_fFirstShot, m_iTurretShots, drills, down, lane.m_A.m_aMen.Count(), gunnerDown);
   if (!m_aGKeys.Contains(key)) m_aGKeys.Insert(key);
   m_mRounds.Set(key, m_mRounds.Get(key) + 1);
   if (m_fFirstShot >= 0) { m_mFirst.Set(key, m_mFirst.Get(key) + m_fFirstShot); m_mFirstN.Set(key, m_mFirstN.Get(key) + 1); }
   m_mFired.Set(key, m_mFired.Get(key) + m_iTurretShots);
   m_mDown.Set(key, m_mDown.Get(key) + down);
   m_mGunnerDown.Set(key, m_mGunnerDown.Get(key) + gunnerDown);
   m_mDrills.Set(key, m_mDrills.Get(key) + drills);
   Clear(lane);
  }
  m_aLanes.Clear();
  m_fNextRound = Now() + 4;
 }

 override protected void Finish()
 {
  GetGame().GetCallqueue().Remove(Tick);
  BBB_Gunner.s_bDrill = true;
  foreach (string key : m_aGKeys)
  {
   float first = -1;
   if (m_mFirstN.Get(key) > 0) first = m_mFirst.Get(key) / m_mFirstN.Get(key);
   PrintFormat("[BBB] GUNNERTEST TOTAL %1: rounds %2, fired in %3, mean first burst %4 s, rounds fired %5, drills %6, enemies down %7, gunner down %8", key, m_mRounds.Get(key), m_mFirstN.Get(key), first, m_mFired.Get(key), m_mDrills.Get(key), m_mDown.Get(key), m_mGunnerDown.Get(key));
  }
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.RemoveObserverSP(OBSERVER_KEY);
  Print("[BBB] GUNNERTEST done");
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_GunnerTest.MaybeStartGunner();
 }
}
