// Test for small arms vs armour (BBB_Armour). -bbbArmourTest 1 (preset "bbbarmour" passes
// -acrBbbArmourTest 1); a world without AC: Actual (GM_Arland). About 9 minutes.
// A USSR light fire team (AK rifles, an RPK) on the aim lane; 150 m away an armoured M1025
// Humvee with a US driver and passenger, and two US riflemen in the open 12 m beside it.
// Round 1 (Humvee alone, 120 s) lets the team learn what their rounds do to it; then pairs
// of rounds with BBB_Armour on and off (learning kept): "solo" (the Humvee is the only
// target: are rounds wasted on it?) and "mixed" (two US riflemen in the open 12 m beside
// it: do they get shot instead?). Per round: Humvee hits, team shots, US men down (crew /
// in the open), team men down, holds and steers. Logs "[BBB] ARMOURTEST ...".
class BBB_ArmourTest : BBB_AimTest
{
 static ref BBB_ArmourTest s_Armour;
 static const ResourceName HUMVEE = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 static const int SOLO_PAIRS = 3;
 static const int MIXED_PAIRS = 3;
 static const float ROUND_S = 60;
 static const float RANGE_M = 150;

 protected ref array<bool> m_aOn = {};
 protected ref array<bool> m_aMixed = {};
 protected bool m_bMixed;
 protected bool m_bOnRound;
 protected IEntity m_Humvee;
 protected ref array<IEntity> m_aCrew = {};
 protected ref array<IEntity> m_aOpen = {};
 protected int m_iHitsAtStart;
 protected int m_iHeldAtStart;
 protected int m_iSteeredAtStart;
 protected string m_sKind;
 protected ref map<string, int> m_mT = new map<string, int>();
 protected ref array<string> m_aTKeys = {};

 static void MaybeStartArmour()
 {
  if (s_Armour || !Replication.IsServer()) return;
  if (!System.IsCLIParam("bbbArmourTest") && !System.IsCLIParam("acrBbbArmourTest")) return;
  s_Armour = new BBB_ArmourTest();
  GetGame().GetCallqueue().CallLater(s_Armour.Begin, 10000);
 }

 override protected void Begin()
 {
  BBB_Armour.Read();
  BBB_Armour.s_bDiag = true;
  Check("key", BBB_Armour.Key("{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et") == "prefabs/vehicles/wheeled/m998/m1025.et", BBB_Armour.Key(HUMVEE));
  m_sKind = BBB_Armour.Key(HUMVEE);
  string round = BBB_Armour.Round("prefabs/weapons/ammo/ammo_545x39_tracer_7t3.et");
  Check("calibre", round == "cal 545x39", round);
  string plain = BBB_Armour.Round("prefabs/weapons/ammo/ammo_rocket_pg7vm.et");
  Check("no-calibre", plain == "prefabs/weapons/ammo/ammo_rocket_pg7vm.et", plain);
  if (!FindSite())
  {
   Print("[BBB] ARMOURTEST no lane");
   Print("[ACR] TEST COMPLETE");
   return;
  }
  LoadLane();
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers)
  {
   vector mid = m_aSiteFrom[0] + m_aSiteDir[0] * 75;
   observers.InsertObserverSP(OBSERVER_KEY, mid[0], mid[2], null);
  }
  m_aOn.Insert(true); m_aMixed.Insert(false); // learning round
  for (int r = 0; r < SOLO_PAIRS + MIXED_PAIRS; r++)
  {
   bool first = (r % 2) == 0;
   bool mixed = r >= SOLO_PAIRS;
   m_aOn.Insert(first); m_aMixed.Insert(mixed);
   m_aOn.Insert(!first); m_aMixed.Insert(mixed);
  }
  m_fNextRound = Now() + 5;
  GetGame().GetCallqueue().CallLater(Tick, 500, true);
 }

 protected IEntity Seat(SCR_AIGroup group, vector at, vector dir, ECompartmentType type, BBB_AimLane lane)
 {
  IEntity man = Spawn(RIFLE_US, at, dir);
  if (!man) return null;
  lane.m_aSpawned.Insert(man);
  group.AddAIEntityToGroup(man);
  ChimeraCharacter character = ChimeraCharacter.Cast(man);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(m_Humvee.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (character && compartments && character.GetCompartmentAccessComponent())
  {
   array<BaseCompartmentSlot> seats = {};
   compartments.GetCompartmentsOfType(seats, type);
   foreach (BaseCompartmentSlot seat : seats)
   {
    if (seat.GetOccupant()) continue;
    character.GetCompartmentAccessComponent().GetInVehicle(m_Humvee, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
    break;
   }
  }
  return man;
 }

 protected SCR_AIGroup EmptyGroup(vector at, vector dir, int slots, BBB_AimLane lane)
 {
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(Spawn(GROUP_US, at, dir));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return null;
  lane.m_aSpawned.Insert(group);
  if (!group.m_aUnitPrefabSlots) group.m_aUnitPrefabSlots = {};
  group.m_aUnitPrefabSlots.Clear();
  for (int i = 0; i < slots; i++) group.m_aUnitPrefabSlots.Insert(RIFLE_US);
  return group;
 }

 override protected bool StartRound(int round)
 {
  if (round >= m_aOn.Count()) return false;
  m_bOnRound = m_aOn[round];
  m_bMixed = m_aMixed[round];
  BBB_Armour.s_bOn = m_bOnRound;
  m_aLanes.Clear();
  m_aCrew.Clear();
  m_aOpen.Clear();
  BBB_AimLane lane = new BBB_AimLane();
  lane.m_vFrom = m_aSiteFrom[0];
  lane.m_vDir = m_aSiteDir[0];
  lane.m_vTo = lane.m_vFrom + lane.m_vDir * RANGE_M;
  lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(TEAM_USSR, lane.m_vFrom, lane.m_vDir));
  if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
  vector side = Vector(lane.m_vDir[2], 0, -lane.m_vDir[0]);
  m_Humvee = Spawn(HUMVEE, lane.m_vTo, side);
  if (m_Humvee) lane.m_aSpawned.Insert(m_Humvee);
  vector behind = lane.m_vTo + lane.m_vDir * 5;
  SCR_AIGroup crew = EmptyGroup(behind, lane.m_vDir, 2, lane);
  if (crew && m_Humvee)
  {
   IEntity driver = Seat(crew, behind, lane.m_vDir, ECompartmentType.PILOT, lane);
   IEntity passenger = Seat(crew, behind + side * 2, lane.m_vDir, ECompartmentType.CARGO, lane);
   if (driver) m_aCrew.Insert(driver);
   if (passenger) m_aCrew.Insert(passenger);
  }
  vector open = lane.m_vTo + side * 12;
  SCR_AIGroup pair;
  if (m_bMixed) pair = EmptyGroup(open, lane.m_vDir * -1, 2, lane);
  if (pair)
  {
   for (int i = 0; i < 2; i++)
   {
    IEntity man = Spawn(RIFLE_US, open + side * (i * 3), lane.m_vDir * -1);
    if (!man) continue;
    lane.m_aSpawned.Insert(man);
    pair.AddAIEntityToGroup(man);
    m_aOpen.Insert(man);
   }
  }
  lane.m_fStart = Now();
  lane.m_fHookAt = Now() + 4;
  m_aLanes.Insert(lane);
  m_fRoundEnd = Now() + ROUND_S;
  if (round == 0) m_fRoundEnd = Now() + 2 * ROUND_S;
  m_iHitsAtStart = BBB_Armour.HitsOn(m_sKind);
  m_iHeldAtStart = BBB_Armour.s_iHeld;
  m_iSteeredAtStart = BBB_Armour.s_iSteered;
  PrintFormat("[BBB] ARMOURTEST round %1/%2: fire team vs M1025 at %3 m, armour logic %4, men in the open %5", round + 1, m_aOn.Count(), RANGE_M, m_bOnRound, m_bMixed);
  return true;
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
    foreach (IEntity m : m_aCrew) PinMan(m);
    foreach (IEntity o : m_aOpen) PinMan(o);
   }
   lane.m_A.SampleAmmo();
  }
  if (now >= m_fRoundEnd) EndRound();
 }

 protected void PinMan(IEntity man)
 {
  if (!man) return;
  AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
  if (control) PinDetail(control.GetAIAgent());
 }

 protected int DownOf(array<IEntity> men)
 {
  int n;
  foreach (IEntity man : men) if (Down(man)) n++;
  return n;
 }

 protected void Add(string key, int v)
 {
  if (!m_aTKeys.Contains(key)) m_aTKeys.Insert(key);
  m_mT.Set(key, m_mT.Get(key) + v);
 }

 override protected void EndRound()
 {
  foreach (BBB_AimLane lane : m_aLanes)
  {
   string mode = "off";
   if (m_bOnRound) mode = "on";
   if (m_bMixed) mode = "mixed " + mode;
   else mode = "solo " + mode;
   if (m_iRound == 0) mode = "learn";
   int hits = BBB_Armour.HitsOn(m_sKind) - m_iHitsAtStart;
   int held = BBB_Armour.s_iHeld - m_iHeldAtStart;
   int steered = BBB_Armour.s_iSteered - m_iSteeredAtStart;
   int crewDown = DownOf(m_aCrew);
   int openDown = DownOf(m_aOpen);
   int teamDown = Downed(lane.m_A);
   string downs = string.Format("crew down %1 of %2, open men down %3 of %4, team down %5 of %6", crewDown, m_aCrew.Count(), openDown, m_aOpen.Count(), teamDown, lane.m_A.m_aMen.Count());
   PrintFormat("[BBB] ARMOURTEST %1: Humvee hits %2, team shots %3, %4, holds %5, steers %6", mode, hits, lane.m_A.m_iShots, downs, held, steered);
   if (m_iRound > 0)
   {
    Add(mode + " rounds", 1);
    Add(mode + " hits", hits);
    Add(mode + " shots", lane.m_A.m_iShots);
    Add(mode + " opendown", openDown);
    Add(mode + " crewdown", crewDown);
    Add(mode + " teamdown", teamDown);
   }
   Clear(lane);
  }
  m_aLanes.Clear();
  m_fNextRound = Now() + 4;
 }

 override protected void Finish()
 {
  GetGame().GetCallqueue().Remove(Tick);
  BBB_Armour.s_bOn = true;
  BBB_Armour.Dump();
  foreach (string key : m_aTKeys) PrintFormat("[BBB] ARMOURTEST TOTAL %1: %2", key, m_mT.Get(key));
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.RemoveObserverSP(OBSERVER_KEY);
  Print("[BBB] ARMOURTEST done");
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_ArmourTest.MaybeStartArmour();
 }
}
