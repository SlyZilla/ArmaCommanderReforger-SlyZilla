// Firefight test for BBB_Medic. Starts with -bbbMedicTest 1 (test launcher preset
// "bbbmedic" passes -acrBbbMedicTest 1). Like the aim test it needs a world without
// AC: Actual: put worlds/GameMaster/GM_Arland.ent in place of "{world}" in
// tools/testserver-args.txt for the run, then put it back. About 25 minutes (REPEATS 2).
// Unit checks of BBB_Medic.Decide first. Then rounds on a lane found on the map (reuses
// BBB_AimTest's lane finder, spawning and observer): a US light fire team plus a medic
// 20 m behind it vs the same for the USSR at 100 m (the aim test's lane). One side heals the BBB way, the other
// the vanilla way, then they swap. FIGHT_S seconds of fighting, then one side is removed
// and the other gets LULL_S seconds of quiet to treat its wounded. WOUND_AT_S seconds in,
// each side's man nearest the enemy is knocked out and bleeds (a planted casualty), so
// every round has a buddy down under fire.
// Per healing type: heal behaviours started and completed, healers hit on their way
// (BBB counts them on both sides), own men down at the end of the fight, own men dead at
// the end of the round, wounded left untreated after the lull, enemies downed.
// Road mode (-bbbMedicRoadTest 1, preset "bbbmedicroad", ~12 min), after Jacob's live case
// (4 Oct: healers shot by one AI ~50 m down a road): the healing side (fire team + medic)
// faces a 2-man sentry team at 50 m that can't be hurt (stands in for a shooter the team
// can't get at; the first road run with normal sentries saw them shot within seconds and no
// healer hit); only the healing side gets a planted casualty and is scored. Rounds: US heal BBB / vanilla, then USSR heal BBB / vanilla, repeated.
// Logs "[BBB] MEDICTEST ..." lines, ends with "[ACR] TEST COMPLETE".
class BBB_MedicSide : Managed
{
 bool m_bVanilla;
 SCR_AIGroup m_Group;
 IEntity m_Medic;
 IEntity m_Casualty;
 ref array<IEntity> m_aMen = {};

 string Label()
 {
  if (m_bVanilla) return "vanilla";
  return "BBB";
 }
}

class BBB_MedicTest : BBB_AimTest
{
 static ref BBB_MedicTest s_MedicTest;

 static const ResourceName MEDIC_US = "{C9E4FEAF5AAC8D8C}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Medic.et";
 static const ResourceName MEDIC_USSR = "{AB9726163EC1BD81}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Medic.et";
 static const float DISTANCE_M = 100;
 static const float ROAD_M = 50;
 static const ResourceName SENTRY_US = "{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et";
 static const ResourceName SENTRY_USSR = "{CB58D90EA14430AD}Prefabs/Groups/OPFOR/Group_USSR_SentryTeam.et";
 static bool s_bRoad;
 // Drag mode (-bbbDragTest 1, preset "bbbdrag", needs ACE Core + Carrying loaded, ~18 min):
 // road layout; rounds 1-2 "still enemy" (sentries can't be hurt and hold fire: does the
 // drag itself work?), then fights with normal sentries, BBB_Drag on ("BBB") vs off
 // ("vanilla"), US and USSR healers. BBB_Medic's holds stay off.
 static bool s_bDrag;
 ref array<bool> m_aStill = {};
 bool m_bStill;
 int m_iDragsAtStart;
 int m_iArrivedAtStart;
 static const float WOUND_AT_S = 25;
 static const float FIGHT_S = 120;
 static const float LULL_S = 50;
 static const int MEDIC_REPEATS = 2;

 ref BBB_MedicSide m_SideA;
 ref BBB_MedicSide m_SideB;
 ref array<IEntity> m_aRoundSpawned = {};
 int m_iMRound = -1;
 ref array<bool> m_aSwap = {};
 ref array<bool> m_aKeepA = {};
 float m_fHookAt;
 float m_fWoundAt;
 bool m_bWounded;
 float m_fFightEnd;
 float m_fLullEnd;
 float m_fNext;
 bool m_bActive;
 bool m_bHooked;
 bool m_bLull;
 BBB_MedicSide m_Kept;
 // Per round snapshots at the end of the fight.
 int m_iDownA;
 int m_iDownB;
 // Totals per label.
 ref map<string, int> m_mTotals = new map<string, int>();
 ref array<string> m_aTotalKeys = {};

 static void MaybeStartMedic()
 {
  if (s_MedicTest || !Replication.IsServer()) return;
  s_bDrag = System.IsCLIParam("bbbDragTest") || System.IsCLIParam("acrBbbDragTest");
  s_bRoad = s_bDrag || System.IsCLIParam("bbbMedicRoadTest") || System.IsCLIParam("acrBbbMedicRoadTest");
  if (!s_bRoad && !System.IsCLIParam("bbbMedicTest") && !System.IsCLIParam("acrBbbMedicTest")) return;
  s_MedicTest = new BBB_MedicTest();
  GetGame().GetCallqueue().CallLater(s_MedicTest.MedicBegin, 10000);
 }

 protected void MCheck(string name, bool ok, string detail)
 {
  string verdict = "FAIL";
  if (ok) verdict = "PASS";
  PrintFormat("[BBB] MEDICTEST CHECK %1 %2 (%3)", name, verdict, detail);
 }

 protected void MedicUnitChecks()
 {
  BBB_Medic.Read();
  // The test compares BBB healing with vanilla, so it's on here whatever the default.
  BBB_Medic.s_bOn = true;
  MCheck("calm-go", BBB_Medic.Decide(40, 0.01, false, false, false) == BBB_Medic.GO, "40 m, calm");
  MCheck("arrived-go", BBB_Medic.Decide(2, 0.9, true, true, true) == BBB_Medic.GO, "2 m, everything bad");
  MCheck("suppressed-hold", BBB_Medic.Decide(40, 0.8, false, false, false) == BBB_Medic.HOLD_SUPPRESSED, "threat 0.8");
  MCheck("bleeding-hold", BBB_Medic.Decide(40, 0.01, true, false, false) == BBB_Medic.HOLD_BLEEDING, "healer bleeding");
  MCheck("enemy-hold", BBB_Medic.Decide(40, 0.01, false, true, false) == BBB_Medic.HOLD_ENEMY, "casualty in view");
  MCheck("alerted-go", BBB_Medic.Decide(40, 0.5, false, false, false) == BBB_Medic.GO, "threat 0.5, not pinned");
  MCheck("dash-go", BBB_Medic.Decide(8, 0.1, false, true, false) == BBB_Medic.GO, "8 m dash, casualty in view");
  MCheck("hot-hold", BBB_Medic.Decide(40, 0.01, false, false, true) == BBB_Medic.HOLD_HOT, "healer hit there");
  MCheck("smoking-hold", BBB_Medic.Decide(40, 0.01, false, false, false, true) == BBB_Medic.HOLD_SMOKING, "waiting for smoke bloom");
 }

 protected void MedicBegin()
 {
  MedicUnitChecks();
  if (s_bDrag)
  {
   BBB_Medic.s_bOn = false;
   BBB_Drag.Read();
   MCheck("ace-carrying-loaded", BBB_Drag.AceLoaded(), "ACE_Carrying_HelperCompartment found by name");
   vector turned = BBB_Drag.Turn(Vector(1, 0, 0), 90);
   MCheck("turn-90", Math.AbsFloat(turned[2] - 1) < 0.01, turned.ToString());
  }
  if (!FindSite())
  {
   Print("[BBB] MEDICTEST range part skipped");
   Print("[ACR] TEST COMPLETE");
   return;
  }
  LoadLane();
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers)
  {
   vector mid = m_aSiteFrom[0] + m_aSiteDir[0] * (DISTANCE_M / 2);
   observers.InsertObserverSP(OBSERVER_KEY, mid[0], mid[2], null);
  }
  // Each repeat: (US BBB, USSR vanilla) and swapped, keeping the US after the fight,
  // then the same keeping the USSR.
  int repeats = MEDIC_REPEATS;
  if (s_bRoad) repeats = 1;
  if (s_bDrag)
  {
   // m_aSwap = drag off; m_aKeepA = US healers; m_aStill = the enemy can't be hurt and holds fire.
   m_aSwap.Insert(false); m_aKeepA.Insert(true); m_aStill.Insert(true);
   m_aSwap.Insert(false); m_aKeepA.Insert(false); m_aStill.Insert(true);
   m_aSwap.Insert(false); m_aKeepA.Insert(true); m_aStill.Insert(false);
   m_aSwap.Insert(true); m_aKeepA.Insert(true); m_aStill.Insert(false);
   m_aSwap.Insert(false); m_aKeepA.Insert(false); m_aStill.Insert(false);
   m_aSwap.Insert(true); m_aKeepA.Insert(false); m_aStill.Insert(false);
   repeats = 0;
  }
  for (int r = 0; r < repeats; r++)
  {
   if (s_bRoad)
   {
    // m_aKeepA = the healers are US; m_aSwap = they heal the vanilla way.
    m_aSwap.Insert(false); m_aKeepA.Insert(true);
    m_aSwap.Insert(true); m_aKeepA.Insert(true);
    m_aSwap.Insert(false); m_aKeepA.Insert(false);
    m_aSwap.Insert(true); m_aKeepA.Insert(false);
    continue;
   }
   m_aSwap.Insert(false); m_aKeepA.Insert(true);
   m_aSwap.Insert(true); m_aKeepA.Insert(true);
   m_aSwap.Insert(false); m_aKeepA.Insert(false);
   m_aSwap.Insert(true); m_aKeepA.Insert(false);
  }
  m_fNext = Now() + 5;
  GetGame().GetCallqueue().CallLater(MedicTick, 500, true);
 }

 protected SCR_AIGroup SpawnTeam(ResourceName team, ResourceName medic, vector at, vector facing, BBB_MedicSide side)
 {
  side.m_Group = SCR_AIGroup.Cast(Spawn(team, at, facing));
  if (side.m_Group) m_aRoundSpawned.Insert(side.m_Group);
  if (medic.IsEmpty()) return side.m_Group;
  side.m_Medic = Spawn(medic, at - facing * 20, facing);
  if (side.m_Medic) m_aRoundSpawned.Insert(side.m_Medic);
  return side.m_Group;
 }

 protected void StartMedicRound(int round)
 {
  m_SideA = new BBB_MedicSide();
  m_SideB = new BBB_MedicSide();
  m_SideA.m_bVanilla = m_aSwap[round];
  m_bStill = s_bDrag && m_aStill[round];
  if (s_bDrag) BBB_Drag.s_bOn = !m_aSwap[round];
  m_iDragsAtStart = BBB_Drag.s_iStarted;
  m_iArrivedAtStart = BBB_Drag.s_iArrived;
  m_SideB.m_bVanilla = !m_aSwap[round];
  vector from = m_aSiteFrom[0];
  vector dir = m_aSiteDir[0];
  if (s_bRoad)
  {
   m_SideB.m_bVanilla = true;
   if (m_aKeepA[round])
   {
    SpawnTeam(TEAM_US, MEDIC_US, from, dir, m_SideA);
    SpawnTeam(SENTRY_USSR, "", from + dir * Distance(), dir * -1, m_SideB);
   }
   else
   {
    SpawnTeam(TEAM_USSR, MEDIC_USSR, from, dir, m_SideA);
    SpawnTeam(SENTRY_US, "", from + dir * Distance(), dir * -1, m_SideB);
   }
  }
  else
  {
   SpawnTeam(TEAM_US, MEDIC_US, from, dir, m_SideA);
   SpawnTeam(TEAM_USSR, MEDIC_USSR, from + dir * DISTANCE_M, dir * -1, m_SideB);
  }
  m_bActive = true;
  m_bHooked = false;
  m_bLull = false;
  m_fHookAt = Now() + 4;
  m_fWoundAt = Now() + WOUND_AT_S;
  m_bWounded = false;
  m_fFightEnd = Now() + FIGHT_S;
  if (s_bRoad)
  {
   string healers = "USSR";
   if (m_aKeepA[round]) healers = "US";
   string extra = "";
   if (s_bDrag) extra = string.Format(", drag on %1, still enemy %2", BBB_Drag.s_bOn, m_bStill);
   PrintFormat("[BBB] MEDICTEST road round %1/%2: %3 fire team + medic (%4 healing) vs 2-man sentry team at %5 m%6", round + 1, m_aSwap.Count(), healers, m_SideA.Label(), Distance(), extra);
  }
  else PrintFormat("[BBB] MEDICTEST round %1/%2: US (%3 healing) vs USSR (%4 healing) at %5 m", round + 1, m_aSwap.Count(), m_SideA.Label(), m_SideB.Label(), DISTANCE_M);
 }

 protected void HookSide(BBB_MedicSide side)
 {
  side.m_aMen.Clear();
  if (!side.m_Group) return;
  if (side.m_Medic) side.m_Group.AddAIEntityToGroup(side.m_Medic);
  PinDetail(side.m_Group);
  array<AIAgent> agents = {};
  side.m_Group.GetAgents(agents);
  string items;
  foreach (AIAgent agent : agents)
  {
   PinDetail(agent);
   IEntity man = agent.GetControlledEntity();
   if (!man) continue;
   side.m_aMen.Insert(man);
   if (side.m_bVanilla) BBB_Medic.s_aVanilla.Insert(man);
   items += " " + BBB_Medic.HealingItems(man).ToString();
  }
  PrintFormat("[BBB] MEDICTEST %1 side: %2 men, healing items per man:%3", side.Label(), side.m_aMen.Count(), items);
 }

 // One casualty per side, planted mid-fight: the man nearest the enemy (not the medic)
 // is knocked out and bleeds from the chest. Does a healer go out to him under fire?
 protected float Distance()
 {
  if (s_bRoad) return ROAD_M;
  return DISTANCE_M;
 }

 protected void WoundOne(BBB_MedicSide side, BBB_MedicSide enemy)
 {
  vector toward = m_aSiteFrom[0] + m_aSiteDir[0] * Distance();
  if (side == m_SideB) toward = m_aSiteFrom[0];
  IEntity pick = null;
  float best = 1000000;
  foreach (IEntity man : side.m_aMen)
  {
   if (!man || man == side.m_Medic || Down(man)) continue;
   float d = vector.DistanceXZ(man.GetOrigin(), toward);
   if (d < best) { best = d; pick = man; }
  }
  side.m_Casualty = pick;
  ChimeraCharacter character = ChimeraCharacter.Cast(pick);
  if (!character) return;
  SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.GetDamageManager());
  if (!damage) return;
  damage.AddParticularBleeding("Chest");
  damage.ForceUnconsciousness(0);
  PrintFormat("[BBB] MEDICTEST %1 side: planted casualty %2, %3 m from the enemy start, unconscious %4", side.Label(), BBB_Medic.Name(pick), Math.Round(best), character.GetCharacterController().IsUnconscious());
  GetGame().GetCallqueue().CallLater(ReportCasualty, 2000, false, pick);
 }

 protected void ReportCasualty(IEntity man)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(man);
  if (!character) return;
  SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
  PrintFormat("[BBB] MEDICTEST planted casualty 2 s later: life state %1, bleeding %2", typename.EnumToString(ECharacterLifeState, controller.GetLifeState()), BBB_Medic.Bleeding(man));
 }

 protected bool Treated(IEntity man)
 {
  return man && BBB.IsAlive(man) && !BBB_Medic.Bleeding(man);
 }

 protected void Unhurtable(BBB_MedicSide side)
 {
  foreach (IEntity man : side.m_aMen)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(man);
   if (character && character.GetDamageManager()) character.GetDamageManager().EnableDamageHandling(false);
  }
 }

 protected int SumFor(map<IEntity, int> counts, BBB_MedicSide side)
 {
  int sum;
  foreach (IEntity man : side.m_aMen)
  {
   if (man && counts.Contains(man)) sum += counts.Get(man);
  }
  return sum;
 }

 protected int Dead(BBB_MedicSide side)
 {
  int count;
  foreach (IEntity man : side.m_aMen)
  {
   if (!man || !BBB.IsAlive(man)) count++;
  }
  return count;
 }

 protected int DownCount(BBB_MedicSide side)
 {
  int count;
  foreach (IEntity man : side.m_aMen)
  {
   if (Down(man)) count++;
  }
  return count;
 }

 // Living men still bleeding or unconscious.
 protected int Untreated(BBB_MedicSide side)
 {
  int count;
  foreach (IEntity man : side.m_aMen)
  {
   if (!man || !BBB.IsAlive(man)) continue;
   if (Down(man) || BBB_Medic.Bleeding(man)) count++;
  }
  return count;
 }

 protected void Add(string label, string what, int value)
 {
  string key = label + " " + what;
  if (!m_aTotalKeys.Contains(key)) m_aTotalKeys.Insert(key);
  m_mTotals.Set(key, m_mTotals.Get(key) + value);
 }

 protected void MedicTick()
 {
  float now = Now();
  if (!m_bActive)
  {
   if (now < m_fNext) return;
   m_iMRound++;
   if (m_iMRound >= m_aSwap.Count())
   {
    MedicFinish();
    return;
   }
   StartMedicRound(m_iMRound);
   return;
  }
  if (!m_bHooked && now >= m_fHookAt)
  {
   m_bHooked = true;
   HookSide(m_SideA);
   HookSide(m_SideB);
   if (s_bRoad && (!s_bDrag || m_bStill)) Unhurtable(m_SideB);
   if (m_bStill && m_SideB.m_Group && m_SideB.m_Group.GetGroupUtilityComponent()) m_SideB.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  }
  if (m_bHooked && !m_bWounded && now >= m_fWoundAt)
  {
   m_bWounded = true;
   WoundOne(m_SideA, m_SideB);
   if (!s_bRoad) WoundOne(m_SideB, m_SideA);
  }
  if (!m_bLull && now >= m_fFightEnd) EndFight();
  if (m_bLull && now >= m_fLullEnd) EndMedicRound();
 }

 protected void EndFight()
 {
  m_bLull = true;
  m_fLullEnd = Now() + LULL_S;
  m_iDownA = DownCount(m_SideA);
  m_iDownB = DownCount(m_SideB);
  BBB_MedicSide gone = m_SideB;
  m_Kept = m_SideA;
  if (!s_bRoad && !m_aKeepA[m_iMRound])
  {
   gone = m_SideA;
   m_Kept = m_SideB;
  }
  PrintFormat("[BBB] MEDICTEST fight over: side A down %1 of %2, side B down %3 of %4; %5 side removed, %6 side gets %7 s of quiet", m_iDownA, m_SideA.m_aMen.Count(), m_iDownB, m_SideB.m_aMen.Count(), gone.Label(), m_Kept.Label(), LULL_S);
  // Record the removed side's numbers now (its men are deleted). In road mode only the
  // healing side is scored.
  if (!s_bRoad) Record(gone, false);
  foreach (IEntity man : gone.m_aMen)
  {
   BBB_Medic.s_aVanilla.RemoveItem(man);
   if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
  gone.m_aMen.Clear();
 }

 protected void Record(BBB_MedicSide side, bool kept)
 {
  BBB_MedicSide enemy = m_SideA;
  int ownDown = m_iDownB;
  int enemyDown = m_iDownA;
  if (side == m_SideA)
  {
   enemy = m_SideB;
   ownDown = m_iDownA;
   enemyDown = m_iDownB;
  }
  int started = SumFor(BBB_Medic.s_mStartedBy, side);
  int done = SumFor(BBB_Medic.s_mDoneBy, side);
  int hit = SumFor(BBB_Medic.s_mHitBy, side);
  int dead = Dead(side);
  string label = side.Label();
  if (m_bStill) label = label + " (still enemy)";
  if (s_bDrag)
  {
   int drags = BBB_Drag.s_iStarted - m_iDragsAtStart;
   int inCover = BBB_Drag.s_iArrived - m_iArrivedAtStart;
   PrintFormat("[BBB] MEDICTEST %1: drags started %2, reached cover %3", label, drags, inCover);
   Add(label, "drags started", drags);
   Add(label, "drags reached cover", inCover);
  }
  PrintFormat("[BBB] MEDICTEST %1 side (%2): heals started %3, done %4, healers hit on the way %5, own down at fight end %6, dead %7, enemies downed %8", label, kept, started, done, hit, ownDown, dead, enemyDown);
  if (!kept)
  {
   // Removed at the fight's end: was the planted casualty treated during the fight?
   int treatedInFight = 0;
   if (Treated(side.m_Casualty)) treatedInFight = 1;
   PrintFormat("[BBB] MEDICTEST %1 side: planted casualty treated during the fight %2", label, treatedInFight);
   Add(label, "removed: planted casualty treated in fight", treatedInFight);
   Add(label, "removed: rounds", 1);
  }
  Add(label, "heals started", started);
  Add(label, "heals done", done);
  Add(label, "healers hit on the way", hit);
  Add(label, "own down at fight end", ownDown);
  Add(label, "enemies downed", enemyDown);
  Add(label, "men fielded", side.m_aMen.Count());
  if (!kept) return;
  int untreated = Untreated(side);
  int treated = 0;
  if (Treated(side.m_Casualty)) treated = 1;
  PrintFormat("[BBB] MEDICTEST %1 side after %2 s of quiet: dead %3, still untreated %4, planted casualty treated %5", label, LULL_S, dead, untreated, treated);
  Add(label, "kept: planted casualty treated", treated);
  Add(label, "kept: dead after lull", dead);
  Add(label, "kept: untreated after lull", untreated);
  Add(label, "kept: rounds", 1);
 }

 protected void EndMedicRound()
 {
  Record(m_Kept, true);
  foreach (IEntity man : m_Kept.m_aMen)
  {
   BBB_Medic.s_aVanilla.RemoveItem(man);
   if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
  foreach (IEntity entity : m_aRoundSpawned)
  {
   if (entity) SCR_EntityHelper.DeleteEntityAndChildren(entity);
  }
  m_aRoundSpawned.Clear();
  m_bActive = false;
  m_fNext = Now() + 4;
 }

 protected void MedicFinish()
 {
  GetGame().GetCallqueue().Remove(MedicTick);
  foreach (string key : m_aTotalKeys)
   PrintFormat("[BBB] MEDICTEST TOTAL %1: %2", key, m_mTotals.Get(key));
  ObserversSystem observers = ObserversSystem.Cast(GetGame().GetWorld().FindSystem(ObserversSystem));
  if (observers) observers.RemoveObserverSP(OBSERVER_KEY);
  Print("[BBB] MEDICTEST done");
  Print("[ACR] TEST COMPLETE");
 }
}

modded class SCR_BaseGameMode
{
 protected override void OnGameStart()
 {
  super.OnGameStart();
  BBB_MedicTest.MaybeStartMedic();
 }
}
