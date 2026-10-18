enum ACR_EAITask
{
 FREE,      // available for orders
 DEFEND,    // holding a threatened base
 ATTACK,    // part of a base attack
 POSTED,    // idle, moved up to a frontline base
 RESUPPLY,  // on its way to an ammo / repair / fuel / medical vehicle
 BUILD_FOB  // establishing a forward base
}

enum ACR_ESapperDemoState
{
 NONE,
 APPROACHING,
 PLANTING,
 FALLBACK,
 DETONATED
}

// Combat AI sapper demolition assault on an enemy FOB HQ composition.
class ACR_SapperDemo : Managed
{
 static const float APPROACH_RADIUS = 30.0;
 static const float PLANT_RADIUS = 7.0;
 static const float PLANT_DURATION_MS = 4000.0;
 static const float FUSE_DURATION_MS = 12000.0;
 static const float RETREAT_DISTANCE = 55.0;

 static const ResourceName CHARGE_WEST = "{1CC0CBB56B9F7ADB}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et";
 static const ResourceName CHARGE_EAST = "{A50106D0A7A6A73F}Prefabs/Weapons/Explosives/DemoBlock_TSh400g/DemoBlock_TSh400g.et";

 ACR_Manager m_Manager;
 ACR_AICommander m_Commander;
 ACR_AIAttack m_Attack;
 ACR_ESapperDemoState m_State = ACR_ESapperDemoState.NONE;
 float m_fPlantStart;
 float m_fFuseDeadline;
 IEntity m_DemoCharge;
 ACR_Squad m_SapperSquad;
 vector m_vChargePos;
 vector m_vFallbackPos;
 bool m_bDetonated;

 void ACR_SapperDemo(ACR_Manager manager, ACR_AICommander commander, ACR_AIAttack attack)
 {
  m_Manager = manager;
  m_Commander = commander;
  m_Attack = attack;
  m_State = ACR_ESapperDemoState.NONE;
 }

 ACR_ESapperDemoState State() { return m_State; }
 void SetStateForTest(ACR_ESapperDemoState state) { m_State = state; }
 IEntity DemoCharge() { return m_DemoCharge; }
 bool IsDetonated() { return m_bDetonated; }
 ACR_Squad SapperSquad() { return m_SapperSquad; }
 vector FallbackPos() { return m_vFallbackPos; }
 void SetPlantStartForTest(float time) { m_fPlantStart = time; }
 void SetFuseDeadlineForTest(float time) { m_fFuseDeadline = time; }

 static vector PositionOf(ACR_Squad squad)
 {
  if (!squad) return vector.Zero;
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 static bool IsSapperSquad(ACR_Squad squad)
 {
  if (!squad || squad.CountAlive() <= 0) return false;
  string name = squad.GetName();
  name.ToLower();
  if (name.Contains("sapper") || name.Contains("engineer") || name.Contains("demolition") || name.Contains("demo") || name.Contains("pioneer"))
   return true;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity entity = agent.GetControlledEntity();
   if (!entity) continue;
   if (IsSapperEntity(entity)) return true;
  }
  return false;
 }

 static bool IsSapperEntity(IEntity entity)
 {
  if (!entity) return false;
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (character && character.GetCharacterController() && character.GetCharacterController().IsDead()) return false;
  EntityPrefabData data = entity.GetPrefabData();
  if (data)
  {
   string pName = data.GetPrefabName();
   pName.ToLower();
   if (pName.Contains("sapper") || pName.Contains("engineer") || pName.Contains("demolition") || pName.Contains("demo") || pName.Contains("pioneer") || pName.Contains("miner"))
    return true;
  }
  return false;
 }

 ACR_Squad FindSapperSquad()
 {
  if (!m_Attack || m_Attack.m_Squads.IsEmpty()) return null;
  foreach (ACR_Squad squad : m_Attack.m_Squads)
  {
   if (squad && squad.CountAlive() > 0 && IsSapperSquad(squad)) return squad;
  }
  foreach (ACR_Squad squad : m_Attack.m_Squads)
  {
   if (squad && squad.CountAlive() > 0 && squad.IsCombatUnit()) return squad;
  }
  if (m_Attack.m_Squads.Count() > 0) return m_Attack.m_Squads[0];
  return null;
 }

 IEntity TargetHqEntity()
 {
  if (!m_Attack || !m_Attack.m_Target || !m_Manager || !m_Manager.m_Structures) return null;
  return m_Manager.m_Structures.HqEntityOf(m_Attack.m_Target);
 }

 vector TargetHqPosition()
 {
  IEntity hq = TargetHqEntity();
  if (hq) return hq.GetOrigin();
  if (m_Attack && m_Attack.m_Target) return m_Attack.m_Target.GetOrigin();
  return vector.Zero;
 }

 void Tick(float now)
 {
  if (m_State == ACR_ESapperDemoState.DETONATED) return;
  if (!m_Attack || !m_Attack.m_Target || m_Attack.m_Target.m_bRetired) return;

  vector hqPos = TargetHqPosition();
  if (hqPos == vector.Zero) return;

  if (!m_SapperSquad || m_SapperSquad.CountAlive() <= 0)
  {
   m_SapperSquad = FindSapperSquad();
   if (!m_SapperSquad) return;
  }

  vector sapperPos = PositionOf(m_SapperSquad);
  float distToHq = vector.DistanceXZ(sapperPos, hqPos);

  if (m_State == ACR_ESapperDemoState.NONE)
  {
   m_State = ACR_ESapperDemoState.APPROACHING;
   m_SapperSquad.SetMode(ACR_EMoveMode.COMBAT);
   m_SapperSquad.Assault(hqPos);
   PrintFormat("[ACR] SAPPER %1: advancing to demolish enemy FOB %2 HQ at %3", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, hqPos);
  }

  if (m_State == ACR_ESapperDemoState.APPROACHING)
  {
   if (distToHq <= APPROACH_RADIUS)
   {
    m_SapperSquad.MoveTo(hqPos);
   }
   if (distToHq <= PLANT_RADIUS)
   {
    m_State = ACR_ESapperDemoState.PLANTING;
    m_fPlantStart = now;
    m_vChargePos = sapperPos;
    m_SapperSquad.SetOrder("Planting demolition charges", ACR_EOrderState.HOLD);
    PrintFormat("[ACR] SAPPER %1: planting demolition charges on enemy FOB %2 HQ (dist=%3m)", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, distToHq);
   }
   return;
  }

  if (m_State == ACR_ESapperDemoState.PLANTING)
  {
   if (distToHq > PLANT_RADIUS + 5.0)
   {
    m_SapperSquad.MoveTo(hqPos);
    return;
   }
   if (now - m_fPlantStart >= PLANT_DURATION_MS)
   {
    ResourceName chargePrefab = CHARGE_WEST;
    string fac = "";
    if (m_Commander) fac = m_Commander.Faction();
    if (!fac.IsEmpty() && !ACR_Supports.IsWestern(fac)) chargePrefab = CHARGE_EAST;

    m_DemoCharge = ACR_Manager.SpawnPrefab(chargePrefab, m_vChargePos);
    m_State = ACR_ESapperDemoState.FALLBACK;
    m_fFuseDeadline = now + FUSE_DURATION_MS;

    vector dir = sapperPos - hqPos;
    dir[1] = 0;
    if (dir.LengthSq() < 0.1) dir = Vector(1, 0, 0);
    else dir.Normalize();

    m_vFallbackPos = ACR_Manager.Ground(hqPos + dir * RETREAT_DISTANCE);

    foreach (ACR_Squad squad : m_Attack.m_Squads)
    {
     if (!squad || squad.CountAlive() <= 0) continue;
     squad.SetOrder("Clearing blast radius", ACR_EOrderState.MOVE);
     squad.SetMode(ACR_EMoveMode.FAST);
     squad.MoveTo(m_vFallbackPos);
    }
    PrintFormat("[ACR] SAPPER %1: charges primed (12s fuse)! Falling back %2m from %3", m_SapperSquad.GetName(), RETREAT_DISTANCE, hqPos);
   }
   return;
  }

  if (m_State == ACR_ESapperDemoState.FALLBACK)
  {
   if (now >= m_fFuseDeadline)
   {
    Detonate();
   }
   return;
  }
 }

 void Detonate()
 {
  if (m_bDetonated) return;
  m_bDetonated = true;
  m_State = ACR_ESapperDemoState.DETONATED;

  vector hqPos = TargetHqPosition();
  IEntity hq = TargetHqEntity();

  PrintFormat("[ACR] SAPPER: DEMOLITION CHARGE DETONATED at %1! Enemy FOB HQ destroyed.", hqPos);

  if (m_Manager && m_Manager.m_Supports)
  {
   m_Manager.m_Supports.ExplodeAt(hqPos);
  }

  if (m_DemoCharge)
  {
   SCR_EntityHelper.DeleteEntityAndChildren(m_DemoCharge);
   m_DemoCharge = null;
  }

  if (hq)
  {
   SCR_EntityHelper.DeleteEntityAndChildren(hq);
  }

  if (m_Attack && m_Attack.m_Target && !m_Attack.m_Target.m_bRetired)
  {
   m_Attack.m_Target.Retire();
  }

  if (m_Attack)
  {
   foreach (ACR_Squad squad : m_Attack.m_Squads)
   {
    if (squad) squad.m_iAITask = ACR_EAITask.FREE;
   }
  }
 }
}

// One base attack: groups gather at a staging point, then assault together.
class ACR_AIAttack
{
 ACR_Base m_Target;
 ref array<ACR_Squad> m_Squads = {};
 ref array<vector> m_aStaging = {};  // one staging point per squad (pincers)
 vector m_Staging;                   // main approach, for smoke
 bool m_bAssaulting;
 float m_fStageDeadline;
 ACR_Squad m_Suppressor;             // support-by-fire squad
 int m_iStartStrength;
 bool m_bCounter;
 bool m_bGrab;                       // quick grab of an empty base (doesn't count against the attack limit)
 float m_fAssaultStart;              // world time the assault (or grab) began
 float m_fNextPush;                  // next time idle attackers are sent in again
 float m_fGrabSeconds;               // a grab's own time limit: the trip there plus slack (big maps)
 bool m_bFobRaid;                    // demolition raid against an enemy forward operating base
 ref ACR_SapperDemo m_SapperDemo;

 vector StagingOf(ACR_Squad squad)
 {
  int i = m_Squads.Find(squad);
  if (i >= 0 && i < m_aStaging.Count()) return m_aStaging[i];
  return m_Staging;
 }
 void Remove(int index)
 {
  if (m_Suppressor == m_Squads[index]) m_Suppressor = null;
  m_Squads.RemoveOrdered(index);
  if (index < m_aStaging.Count()) m_aStaging.RemoveOrdered(index);
 }
}

// Strategic AI for one faction, adapted from the Arma 3 mod's ai_strategic.sqf
// and ai_attack.sqf. Every 15 seconds it values each base, defends the ones
// under threat, attacks the weakest enemy or empty border base, posts idle
// squads to the front, and spends RP on reinforcements. It stands down while a
// player of its faction is in command.
class ACR_AICommander : Managed
{
 static const int TICK_SECONDS = 15;          // STRATEGY_TICK
 static const float DEFENSE_DISTANCE = 450;   // squads this close count as defending a base
 static const float THREAT_DISTANCE = 400;    // enemies this far past the ring threaten it
 static const float STAGING_DISTANCE = 275;   // staging point outside the target ring
 static const float STAGING_CLEARANCE = 350;  // m kept between a staging point and any other enemy base ring
 static const float ATTACK_RATIO = 1.8;       // attackers want 1.8x the defence
 static const float DEFENSE_RATIO = 1.1;      // defenders want 1.1x the threat
 static const float CONCEDE_RATIO = 4;        // give a base up against 4x the defence
 static const int PLAYER_STRENGTH = 5;
 static float STAGING_TIMEOUT = 300;          // seconds
 // An assault that hasn't taken the base by now is called off (it would
 // otherwise hold the attack slot forever). A foothold inside the base ring
 // buys it more time. Grabs of empty bases get less.
 static float ASSAULT_TIMEOUT = 1200;         // s, nobody of ours inside the ring
 static float ASSAULT_HARD_TIMEOUT = 1800;    // s, even with a foothold
 static bool s_bGrabByDistance = true;          // off in tests that shorten GRAB_TIMEOUT
 static float GRAB_TIMEOUT = 480;             // s  (the test harness shortens these)
 static const float STALLED_COOLDOWN = 300;   // s before the same base is tried again
 static const float PUSH_INTERVAL = 120;      // s between re-issuing assault orders
 // Initiative: with no attack launched for a while, the commander accepts
 // worse odds (down to 65% of the usual strength) and commits its reserve,
 // so two AI sides don't sit facing each other forever.
 static const float QUIET_MINUTES = 4;
 static const float PRESSURE_PER_MINUTE = 0.05;
 static const float MIN_PRESSURE = 0.65;
 protected float m_fLastOffensive = -1;
 protected float m_fNextWeakLog;

 protected float Pressure()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fLastOffensive < 0) m_fLastOffensive = now;
  float quiet = (now - m_fLastOffensive) / 60000 - QUIET_MINUTES;
  if (quiet <= 0) return 1;
  return Math.Max(MIN_PRESSURE, 1 - quiet * PRESSURE_PER_MINUTE);
 }

 ACR_Manager m_Manager;
 string m_sFaction;
 string Faction() { return m_sFaction; }
 bool m_bActive;
 protected int m_iSeconds;
 protected ref array<ref ACR_AIAttack> m_Attacks = {};
 protected ref map<ACR_Base, int> m_Defence = new map<ACR_Base, int>();
 protected ref map<ACR_Base, int> m_Threat = new map<ACR_Base, int>();
 protected ref map<ACR_Base, string> m_LastOwner = new map<ACR_Base, string>();
 protected ACR_Base m_CounterTarget;
 protected ref map<ACR_Base, float> m_StalledUntil = new map<ACR_Base, float>();
 protected float m_fNextStatus;
 protected float m_fCounterUntil;
 static const float RETREAT_RATIO = 0.4;   // a squad below 40% in contact falls back
 static const float ABORT_RATIO = 0.35;    // an attack below 35% of its strength is called off
 static const int SUPPORT_RESERVE = 15;    // RP the AI keeps back when calling supports
 static const int SUPPORT_BUDGET_MAX = 60;  // most RP it holds back from buying for supports
 static const int MAX_GRABS = 3;           // empty bases it goes for at the same time (at least)
 static const int MAX_GRABS_BIG = 8;       // ... at most, on maps with many empty bases

 void ACR_AICommander(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
 }

 // Called every second by the manager.
 void Tick(bool noPlayerCommander)
 {
  if (m_bActive != noPlayerCommander)
  {
   m_bActive = noPlayerCommander;
   PrintFormat("[ACR] AI commander %1: %2", m_sFaction, m_bActive);
   if (!m_bActive) ReleaseAll();
  }
  if (!m_bActive) return;
  GatherIntel();
  m_iSeconds++;
  if (m_iSeconds % TICK_SECONDS != 1) return;
  int t = System.GetTickCount();
  Assess();
  t = ACR_Perf.Lap("ai.assess", t);
  UpdateAttacks();
  t = ACR_Perf.Lap("ai.attacks", t);
  AssignDefenders();
  t = ACR_Perf.Lap("ai.defend", t);
  AssignAttacks();
  t = ACR_Perf.Lap("ai.plan", t);
  PostIdle();
  PositionMortars();
  Recrew();
  t = ACR_Perf.Lap("ai.post", t);
  ManageLogistics();
  ManageFobBuild();
  ManageFobUpgrades();
  t = ACR_Perf.Lap("ai.logistics", t);
  Requisition();
  t = ACR_Perf.Lap("ai.buy", t);
  LogStatus();
 }

 protected ACR_FactionSettings Settings() { return ACR_FactionSettings.Find(m_sFaction); }

 // ---------------------------------------------------------------- helpers

 protected void MySquads(array<ACR_Squad> result, bool freeOnly)
 {
  result.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.HasPlayer() || !squad.GetAnchor()) continue; // players lead themselves
   if (squad.m_bMortar) continue; // mortar teams: PositionMortars
   if (squad.Aircraft()) continue; // helicopter crews: AI can't fly
   if (ACR_AirLift.Busy(squad)) continue; // flown somewhere: leave it until it has landed
   if (freeOnly && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   if (freeOnly && !squad.IsCombatUnit()) continue; // lone drivers, unarmed crews: kept out of the fighting
   result.Insert(squad);
  }
 }

 protected vector PositionOf(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 protected void SortByDistance(array<ACR_Squad> squads, vector point)
 {
  // simple insertion sort; squad counts are small
  for (int i = 1; i < squads.Count(); i++)
  {
   ACR_Squad current = squads[i];
   float d = vector.DistanceXZ(PositionOf(current), point);
   int j = i - 1;
   while (j >= 0 && vector.DistanceXZ(PositionOf(squads[j]), point) > d)
   {
    squads[j + 1] = squads[j];
    j--;
   }
   squads[j + 1] = current;
  }
 }

 protected bool CoolingDown(ACR_Base base)
 {
  float until;
  return m_StalledUntil.Find(base, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 protected bool Visible(ACR_Base base)
 {
  return base.IsVisibleTo(m_sFaction);
 }

 // Own bases nearest to anything not ours: where fights happen.
 protected void FrontlineBases(array<ACR_Base> result)
 {
  result.Clear();
  array<ACR_Base> mine = {};
  array<ACR_Base> others = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction) mine.Insert(base);
   else if (Visible(base)) others.Insert(base);
  }
  if (others.IsEmpty()) { result.Copy(mine); return; }
  array<float> nearest = {};
  foreach (ACR_Base base : mine)
  {
   float gap = float.MAX;
   foreach (ACR_Base other : others) gap = Math.Min(gap, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   nearest.Insert(gap);
  }
  array<int> picked = {};
  FrontlinePick(nearest, picked);
  foreach (int i : picked) result.Insert(mine[i]);
 }

 static const float FRONTLINE_SLACK = 150; // m: bases this much farther than the closest still count

 // The indexes of every base within FRONTLINE_SLACK of the smallest gap. Two passes, so
 // the answer doesn't depend on the order the bases come in (R07, 5 Oct review: the old
 // single pass dropped bases seen before a closer one).
 static void FrontlinePick(notnull array<float> nearest, notnull array<int> result)
 {
  result.Clear();
  float best = float.MAX;
  foreach (float gap : nearest) best = Math.Min(best, gap);
  foreach (int i, float gap : nearest) if (gap <= best + FRONTLINE_SLACK) result.Insert(i);
 }

 // ---------------------------------------------------------------- supports

 protected int SupportIndex(ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 // RP kept back from unit purchases so a fire mission is always affordable:
 // the dearest mortar mission plus the usual reserve (capped).
 protected int SupportBudget()
 {
  int dearest;
  foreach (ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
  {
   if (entry.m_eType != ACR_ESupportType.MORTAR_HE_SMALL && entry.m_eType != ACR_ESupportType.MORTAR_HE_LARGE && entry.m_eType != ACR_ESupportType.MORTAR_SMOKE) continue;
   dearest = Math.Max(dearest, entry.m_iCost);
  }
  if (dearest == 0) return 0;
  return Math.Min(SUPPORT_BUDGET_MAX, dearest + SUPPORT_RESERVE);
 }

 // Needing troops beats keeping fire support money: well under strength, or a
 // base threatened by more than it has defending it.
 protected bool UrgentTroops(int combatUnits, int cap)
 {
  if (combatUnits < cap / 2) return true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == m_sFaction && ThreatTo(base) > DefenceOf(base)) return true;
  return false;
 }

 bool SupportReady(ACR_ESupportType type)
 {
  int index = SupportIndex(type);
  if (index < 0 || !m_Manager.m_Supports) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  ACR_SupportEntry entry = data.m_aSupports[index];
  return GetGame().GetWorld().GetWorldTime() >= entry.m_fReadyAt && data.m_iRP >= entry.CostFor(true) + SUPPORT_RESERVE;
 }

 // Own soldiers (squads and players) within radius: never call fire on them.
 protected bool FriendliesNear(vector point, float radius)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (soldier && vector.DistanceXZ(soldier.GetOrigin(), point) < radius) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (faction && body && faction.GetFactionKey() == m_sFaction && vector.DistanceXZ(body.GetOrigin(), point) < radius) return true;
  }
  return false;
 }

 protected bool CallSupport(ACR_ESupportType type, vector point, string why)
 {
  if (!SupportReady(type)) return false;
  if (ACR_Mortars.IsMortarType(type) && !m_Manager.Mortars().CanFire(m_sFaction, type, point)) return false;
  if (ACR_HeliStation.IsHeliType(type) && ACR_HeliStation.UsesPads(m_sFaction) && (!m_Manager.m_Supports || !m_Manager.m_Supports.Station().ReadyFor(m_sFaction, ACR_HeliStation.NeedsGunship(type), point))) return false;
  if (FriendliesNear(point, ACR_Supports.ZoneRadius(type) + 40)) return false;
  string result = m_Manager.m_Supports.Call(m_sFaction, SupportIndex(type), point, true);
  PrintFormat("[ACR] AI %1 support (%2): %3", m_sFaction, why, result);
  return true;
 }

 // ---------------------------------------------------------------- 0. intelligence
 // No map hacks: the commander only knows about enemies its own soldiers have
 // actually spotted (their perception), or that walk into one of its bases
 // (sentries). Each sighting is remembered at the last seen position for
 // INTEL_MEMORY_MS, then forgotten. Threat estimates, defensive fires and
 // attack planning all work from this picture, not from true positions.
 static const float INTEL_MEMORY_MS = 90000;
 static const float SENTRY_RANGE = 60;
 static const int UNKNOWN_DEFENCE = 4;        // assumed extra defenders at an enemy base we can't see into
 protected ref map<IEntity, vector> m_SeenAt = new map<IEntity, vector>();
 protected ref map<IEntity, float> m_SeenTime = new map<IEntity, float>();

 void Note(IEntity enemy, float now)
 {
  if (!enemy || !ACR_Respawn.IsAlive(enemy)) return;
  m_SeenAt.Set(enemy, enemy.GetOrigin());
  m_SeenTime.Set(enemy, now);
 }

 protected string FactionKeyOf(IEntity body)
 {
  if (!body) return "";
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return "";
  return affiliation.GetAffiliatedFaction().GetFactionKey();
 }

 // Every second: the side's shared picture (ACR_Intel, gathered by the manager
 // from our squads, sentries and players).
 protected void GatherIntel()
 {
  ACR_Intel intel = m_Manager.IntelFor(m_sFaction);
  if (!intel) return;
  m_SeenAt = intel.m_SeenAt;
  m_SeenTime = intel.m_SeenTime;
 }

 // Where we last saw any member of this enemy squad (false: we haven't).
 protected bool KnownPosition(ACR_Squad squad, out vector position)
 {
  float newest = -1;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   float when;
   if (!body || !m_SeenTime.Find(body, when) || when <= newest) continue;
   newest = when;
   position = m_SeenAt.Get(body);
  }
  return newest >= 0;
 }

 // Known enemy squad nearest to a point (for defensive fires): aimed at where it was last seen.
 protected ACR_Squad EnemyNear(vector point, float radius, out vector seenAt)
 {
  ACR_Squad best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   float distance = vector.DistanceXZ(known, point);
   if (distance < bestDistance) { bestDistance = distance; best = squad; seenAt = known; }
  }
  return best;
 }

 // ---------------------------------------------------------------- 1. assessment

 protected void Assess()
 {
  m_Defence.Clear();
  m_Threat.Clear();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   int defence, threat;
   vector center = base.GetOrigin();
   // Our own squads we know exactly; enemy squads only where last seen.
   bool ours = base.m_sOwner == m_sFaction;
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || !squad.GetAnchor()) continue;
    bool mine = squad.GetFaction().GetFactionKey() == m_sFaction;
    vector where = PositionOf(squad);
    if (!mine && !KnownPosition(squad, where)) continue;
    float distance = vector.DistanceXZ(where, center);
    bool friendly = squad.GetFaction().GetFactionKey() == base.m_sOwner;
    if (friendly && distance < DEFENSE_DISTANCE) defence += squad.Strength();
    if (!friendly && distance < THREAT_DISTANCE + base.m_fRadius) threat += squad.Strength();
   }
   // Player bodies not in any squad (command units) count too.
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
    if (!body || !faction || m_Manager.FindSquadBySoldier(body)) continue;
    vector bodyAt = body.GetOrigin();
    if (faction.GetFactionKey() != m_sFaction)
    {
     if (!m_SeenAt.Contains(body)) continue;
     bodyAt = m_SeenAt.Get(body);
    }
    float d = vector.DistanceXZ(bodyAt, center);
    if (faction.GetFactionKey() == base.m_sOwner && d < DEFENSE_DISTANCE) defence += PLAYER_STRENGTH;
    if (faction.GetFactionKey() != base.m_sOwner && d < THREAT_DISTANCE + base.m_fRadius) threat += PLAYER_STRENGTH;
   }
   if (!base.m_sOwner.IsEmpty()) defence += base.m_iGarrisonLeft;
   if (!ours && !base.m_sOwner.IsEmpty()) defence += UNKNOWN_DEFENCE; // fog of war: expect more than we've seen
   m_Defence.Set(base, defence);
   m_Threat.Set(base, threat);
   string before;
   if (m_LastOwner.Find(base, before) && before == m_sFaction && base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty())
   {
    // Hit back while the enemy is still thin on the ground.
    m_CounterTarget = base;
    m_fCounterUntil = GetGame().GetWorld().GetWorldTime() + 300000;
    PrintFormat("[ACR] AI %1: lost %2, planning a counterattack", m_sFaction, base.m_sName);
   }
   m_LastOwner.Set(base, base.m_sOwner);
  }
 }

 protected int DefenceOf(ACR_Base base) { int v; m_Defence.Find(base, v); return v; }
 protected int ThreatTo(ACR_Base base) { int v; m_Threat.Find(base, v); return v; }

 // ---------------------------------------------------------------- 2. defence

 protected void AssignDefenders()
 {
  // Release defenders of bases that are no longer threatened (or no longer ours).
  // (only our own: with both sides on AI, the other commander's defenders
  // were set free here every tick, since their base isn't ours)
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad.m_iAITask != ACR_EAITask.DEFEND || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   ACR_Base held = squad.m_AITarget;
   if (!held || held.m_sOwner != m_sFaction || ThreatTo(held) == 0) { squad.m_iAITask = ACR_EAITask.FREE; continue; }
   // A defence that is being overrun: the badly hurt get out rather than die in place.
   bool overrun = ThreatTo(held) > DefenceOf(held) * 2;
   if (overrun && squad.StrengthRatio() < RETREAT_RATIO && squad.KnownEnemies(300) > 0)
    FallBack(squad, "defence of " + held.m_sName + " overrun", held);
   else if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < OUT_OF_AMMO && squad.KnownEnemies(300) > 0)
    FallBack(squad, "out of ammo", held);
  }
  array<ACR_Squad> free = {};
  MySquads(free, true);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   int threat = ThreatTo(base);
   if (threat == 0) continue;
   int defence = DefenceOf(base);
   if (threat > defence * CONCEDE_RATIO && defence > 0) continue; // lost cause
   float needed = threat * DEFENSE_RATIO - defence;
   if (needed <= 0) continue;
   SortByDistance(free, base.GetOrigin());
   int k = 0;
   while (k < free.Count() && needed > 0)
   {
    ACR_Squad squad = free[k];
    if (vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) > DEFENSE_DISTANCE * 3) { k++; continue; }
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, base.GetOrigin(), true, 0);
    squad.m_iAITask = ACR_EAITask.DEFEND;
    squad.m_AITarget = base;
    needed -= squad.Strength();
    free.RemoveOrdered(k);
    PrintFormat("[ACR] AI %1: %2 defends %3 (threat %4)", m_sFaction, squad.GetName(), base.m_sName, threat);
   }
  }
  // Defensive fires: shell the attackers of a base that is losing the fight.
  foreach (ACR_Base held : ACR_Base.s_All)
  {
   if (held.m_sOwner != m_sFaction || ThreatTo(held) <= DefenceOf(held)) continue;
   vector seenAt;
   ACR_Squad attacker = EnemyNear(held.GetOrigin(), THREAT_DISTANCE + held.m_fRadius, seenAt);
   if (!attacker) continue;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, seenAt, "defensive fire at " + held.m_sName)) break;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, seenAt, "defensive fire at " + held.m_sName)) break;
   // No mortar in reach: the howitzer battery (slow and dear, but it reaches anywhere).
   if (CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, seenAt, "battery fire at " + held.m_sName)) break;
  }
 }

 // ---------------------------------------------------------------- 3. attacks

 protected void AssignAttacks()
 {
  ACR_FactionSettings settings = Settings();
  int maxAttacks = 1;
  if (settings) maxAttacks = settings.m_iAIAttacks;
  bool counterDue = m_CounterTarget && m_CounterTarget.m_sOwner != m_sFaction && GetGame().GetWorld().GetWorldTime() < m_fCounterUntil && !IsAttacking(m_CounterTarget);
  array<ACR_Squad> free = {};
  MySquads(free, true);
  // Keep a reserve once the force is big enough, at a secure base behind the front (not the
  // HQ unless nothing else fits: Jacob 14 Oct, reserves at the HQ took too long to respond).
  array<ACR_Squad> all = {};
  MySquads(all, false);
  if (all.Count() >= 5 && free.Count() >= 2 && Pressure() >= 0.9)
  {
   ACR_Base home = ReserveBase();
   if (home)
   {
    SortByDistance(free, home.GetOrigin());
    ACR_Squad reserve = free[0];
    free.RemoveOrdered(0);
    if (reserve.m_iAITask != ACR_EAITask.POSTED || vector.DistanceXZ(PositionOf(reserve), home.GetOrigin()) > 150)
    {
     reserve.m_iAITask = ACR_EAITask.POSTED;
     reserve.SetMode(ACR_EMoveMode.COMBAT);
     Travel(reserve, home.GetOrigin(), true, 0);
     PrintFormat("[ACR] AI %1: %2 held in reserve at %3", m_sFaction, reserve.GetName(), home.m_sName);
    }
   }
  }
  if (free.IsEmpty()) return;

  // Empty bases: several quick grabs at once, each by whichever free squad gets
  // there first (vehicles count as three times faster than walking).
  int grabs = 0;
  int attacks = 0;
  foreach (ACR_AIAttack running : m_Attacks)
  {
   if (running.m_bGrab) grabs++;
   else attacks++;
  }
  // Big maps (Everon soak 5 Oct: 3 grabs at once left each side with 7 of 56 bases after
  // 18 min): one grab per 6 empty bases in sight, 3 to 8.
  int unclaimed;
  foreach (ACR_Base open : ACR_Base.s_All) if (open.m_sOwner.IsEmpty() && Visible(open)) unclaimed++;
  int maxGrabs = Math.Clamp(unclaimed / 6, MAX_GRABS, MAX_GRABS_BIG);
  while (grabs < maxGrabs && !free.IsEmpty())
  {
   ACR_Base empty;
   ACR_Squad runner;
   float bestTime = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!candidate.m_sOwner.IsEmpty() || !Visible(candidate) || IsAttacking(candidate) || CoolingDown(candidate)) continue;
    foreach (ACR_Squad squad : free)
    {
     float speed = 3;
     if (squad.IsMounted()) speed = 9;
     float time = vector.DistanceXZ(PositionOf(squad), candidate.GetOrigin()) / speed;
     if (time < bestTime) { bestTime = time; empty = candidate; runner = squad; }
    }
   }
   if (!empty) break;
   ACR_AIAttack grab = new ACR_AIAttack();
   grab.m_Target = empty;
   grab.m_bGrab = true;
   grab.m_bAssaulting = true;
   grab.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
   grab.m_Squads.Insert(runner);
   grab.m_aStaging.Insert(empty.GetOrigin());
   grab.m_iStartStrength = runner.Strength();
   // Everon soak 5 Oct: most grabs of bases 2-3 km away timed out on the way at 480 s.
   // Allow the walk (or drive) there at about 1.2 m/s, plus 3 minutes.
   grab.m_fGrabSeconds = vector.DistanceXZ(PositionOf(runner), empty.GetOrigin()) / 1.2 + 180;
   runner.m_iAITask = ACR_EAITask.ATTACK;
   runner.m_AITarget = empty;
   runner.SetMode(ACR_EMoveMode.FAST);
   Travel(runner, empty.GetOrigin(), false, 100);
   free.RemoveItem(runner);
   m_Attacks.Insert(grab);
   grabs++;
   PrintFormat("[ACR] AI %1: %2 grabs empty %3 (about %4 s away)", m_sFaction, runner.GetName(), empty.m_sName, (int)bestTime);
  }
  if (free.IsEmpty()) return;
  // Plenty of idle troops: open more fronts (one more per 4 free squads, up to 3 more).
  maxAttacks += Math.Min(3, free.Count() / 4);
  if (attacks >= maxAttacks && !counterDue) return;

  // Target: a counterattack on a base just lost, otherwise the enemy base with
  // the lowest (defence x ratio) scaled by distance -- the best one we can
  // actually muster enough for, rather than waiting forever on one we can't.
  float pressure = Pressure();
  int available;
  foreach (ACR_Squad counted : free) available += counted.Strength();
  ACR_Base target;
  ACR_Base wanted;
  float required = 1;
  float wantedRequired;
  vector from = PositionOf(free[0]);
  array<ACR_Base> candidates = {};
  array<float> scores = {};
  if (counterDue) candidates.Insert(m_CounterTarget);
  else
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base) || IsAttacking(base) || CoolingDown(base)) continue;
    float cost = Math.Max(10, DefenceOf(base) * ATTACK_RATIO);
    float score = cost * (vector.DistanceXZ(base.GetOrigin(), from) / 1000 + 0.1);
    if (base.m_bBuilt)
    {
     score *= 0.35;
     float frontDist = float.MAX;
     foreach (ACR_Base ownBase : ACR_Base.s_All)
     {
      if (ownBase && !ownBase.m_bRetired && ownBase.m_sOwner == m_sFaction)
      {
       float d = vector.DistanceXZ(base.GetOrigin(), ownBase.GetOrigin());
       if (d < frontDist) frontDist = d;
      }
     }
     if (frontDist < 1200) score *= 0.6;
    }
    int at = 0;
    while (at < scores.Count() && scores[at] <= score) at++;
    candidates.InsertAt(base, at);
    scores.InsertAt(score, at);
   }
  }
  foreach (ACR_Base candidate : candidates)
  {
   float need = 1;
   if (!candidate.m_sOwner.IsEmpty()) need = Math.Max(10, DefenceOf(candidate) * ATTACK_RATIO * 0.65) + candidate.m_iGarrisonLeft;
   if (candidate.m_bBuilt) need = Math.Max(8, DefenceOf(candidate) * 0.9 + candidate.m_iGarrisonLeft * 0.7);
   if (counterDue) need = need * 0.7; // strike before they dig in
   need = need * pressure;
   if (ACR_Manager.ThreatOption() == 2) need = need * 0.85; // High: launch a viable force sooner
   if (!wanted) { wanted = candidate; wantedRequired = need; }
   if (available >= need) { target = candidate; required = need; break; }
  }
  if (!target)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (wanted && now >= m_fNextWeakLog)
   {
    m_fNextWeakLog = now + 60000;
    PrintFormat("[ACR] AI %1: not strong enough for any target (best: %2, %3/%4, pressure x%5)", m_sFaction, wanted.m_sName, available, (int)wantedRequired, pressure);
   }
   return; // Requisition() will add troops
  }
  SortByDistance(free, target.GetOrigin());
  if (target.m_bBuilt)
  {
   for (int s = free.Count() - 1; s >= 0; s--)
   {
    if (ACR_SapperDemo.IsSapperSquad(free[s]))
    {
     ACR_Squad sapperSquad = free[s];
     free.RemoveOrdered(s);
     free.InsertAt(sapperSquad, 0);
     break;
    }
   }
  }
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  attack.m_bCounter = counterDue;
  if (target.m_bBuilt)
  {
   attack.m_bFobRaid = true;
   attack.m_SapperDemo = new ACR_SapperDemo(m_Manager, this, attack);
   PrintFormat("[ACR] AI %1: targeted enemy forward base %2 for sapper demolition", m_sFaction, target.m_sName);
  }
  int strength;
  // With troops to spare, send a margin above the bare minimum.
  float margin = 1;
  if (free.Count() > 4) margin = 1 + Math.Min(0.5, (free.Count() - 4) * 0.05);
  foreach (ACR_Squad squad : free)
  {
   attack.m_Squads.Insert(squad);
   strength += squad.Strength();
   if (strength >= required * margin) break;
  }
  if (strength < required)
  {
   PrintFormat("[ACR] AI %1: not strong enough for %2 (%3/%4)", m_sFaction, target.m_sName, strength, required);
   return; // Requisition() will add troops
  }
  LaunchAttack(attack, from, strength, required, pressure);
 }

 // Sends the chosen squads off: staging points (pincers), base of fire, orders.
 protected void LaunchAttack(ACR_AIAttack attack, vector from, int strength, float required, float pressure)
 {
  ACR_Base target = attack.m_Target;
  attack.m_iStartStrength = strength;
  // Staging: the main approach from the attackers' side; with several squads
  // they spread out and come in from different angles (pincers).
  vector dir = from - target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  float ring = target.m_fRadius + STAGING_DISTANCE;
  attack.m_Staging = ACR_Manager.Ground(target.GetOrigin() + dir * ring);
  // Staging points on dry land with a dry run in (no forming up in a bay).
  vector dryMain;
  if (StagingPoint(target, dir, ring, dryMain)) attack.m_Staging = dryMain;
  array<float> angles = {0, 55, -55, 90, -90};
  foreach (int i, ACR_Squad member : attack.m_Squads)
  {
   float angle = angles[i % angles.Count()] * Math.DEG2RAD;
   vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
   vector stage;
   if (!StagingPoint(target, rotated, ring, stage)) stage = attack.m_Staging;
   attack.m_aStaging.Insert(stage);
  }
  bool empty = target.m_sOwner.IsEmpty();
  // With two or more squads on a held base, one becomes the base of fire
  // while the others manoeuvre.
  // Picked by firepower (a gun vehicle or the biggest rifle squad), never a support team.
  if (!empty && attack.m_Squads.Count() >= 2)
  {
   int bestFire = 2;
   foreach (ACR_Squad fireCandidate : attack.m_Squads)
   {
    int firepower = fireCandidate.FirePower();
    if (firepower > bestFire) { bestFire = firepower; attack.m_Suppressor = fireCandidate; }
   }
  }
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + STAGING_TIMEOUT * 1000;
  // Motorized assault: an armed carrier (APC) in the attack takes an infantry squad to the
  // staging point and stays there with it as base of fire.
  array<ACR_Squad> carried = {};
  if (!empty && m_Manager.m_Rides) PairCarriers(attack, carried);
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.m_iAITask = ACR_EAITask.ATTACK;
   squad.m_AITarget = target;
   if (carried.Contains(squad)) continue; // the ride has its orders
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) Travel(squad, target.GetOrigin(), false, 100);
   else Travel(squad, attack.StagingOf(squad), false, 0);
  }
  attack.m_bAssaulting = empty;
  m_Attacks.Insert(attack);
  m_fLastOffensive = GetGame().GetWorld().GetWorldTime();
  if (pressure < 1) PrintFormat("[ACR] AI %1: taking the initiative against %2 (accepting x%3 odds after a quiet spell)", m_sFaction, target.m_sName, pressure);
  string kind = "attacking";
  if (attack.m_bCounter) { kind = "COUNTERATTACKING"; m_CounterTarget = null; }
  PrintFormat("[ACR] AI %1: %2 %3 with %4 squad(s), strength %5/%6", m_sFaction, kind, target.m_sName, attack.m_Squads.Count(), strength, required);
 }

 // Each armed carrier with room (gun manned, 4+ free seats) picks the nearest infantry squad of the
 // attack that fits and is within 400 m, and rides it to that squad's staging point (5 Oct;
 // Jacob: APCs for motorized assaults). The carrier then forms up there instead of its own point.
 static float CARRIER_PICKUP = 400;
 static int s_iCarrierTries; // tests
 protected void PairCarriers(ACR_AIAttack attack, array<ACR_Squad> carried)
 {
  foreach (int c, ACR_Squad carrier : attack.m_Squads)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(carrier);
   if (!vehicle || carried.Contains(carrier) || !carrier.HasGunner(vehicle)) continue;
   int seats = ACR_Rides.FreeSeats(vehicle);
   if (seats < 4) continue;
   ACR_Squad best;
   float bestDistance = CARRIER_PICKUP;
   foreach (ACR_Squad rider : attack.m_Squads)
   {
    if (rider == carrier || carried.Contains(rider) || rider.IsMounted() || ACR_Transport.CrewedVehicle(rider)) continue;
    if (rider.CountAlive() > seats) continue;
    float d = vector.DistanceXZ(PositionOf(rider), vehicle.GetOrigin());
    if (d < bestDistance) { bestDistance = d; best = rider; }
   }
   if (!best) continue;
   vector stage = attack.StagingOf(best);
   s_iCarrierTries++;
   string why;
   if (!m_Manager.m_Rides.RequestCarrier(best, carrier, stage, ACR_Squad.WP_MOVE, -1, "Moving", 0, why))
   {
    PrintFormat("[ACR] AI %1: %2 can't carry %3 (%4)", m_sFaction, carrier.GetName(), best.GetName(), why);
    continue;
   }
   if (c < attack.m_aStaging.Count()) attack.m_aStaging[c] = stage;
   carried.Insert(best);
   carried.Insert(carrier);
   PrintFormat("[ACR] AI %1: %2 carries %3 to the staging point for %4", m_sFaction, carrier.GetName(), best.GetName(), attack.m_Target.m_sName);
  }
 }

 // Debug menu: attack now with every free squad, ignoring odds, attack limits and
 // cooldowns. target null = the nearest enemy-held base (an empty one if none).
 string ForceAttack(ACR_Base target)
 {
  if (!m_bActive) return string.Format("%1 AI commander isn't in command (a player commands it, or it's held or off)", m_sFaction);
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (free.IsEmpty()) return string.Format("%1 AI has no free squads to attack with", m_sFaction);
  vector from = PositionOf(free[0]);
  if (!target)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), from);
    if (base.m_sOwner.IsEmpty()) d += 1000000; // enemy bases first
    if (d < best) { best = d; target = base; }
   }
  }
  if (!target) return "No base to attack";
  if (target.m_sOwner == m_sFaction) return string.Format("%1 already holds %2", m_sFaction, target.m_sName);
  m_StalledUntil.Remove(target);
  SortByDistance(free, target.GetOrigin());
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  int strength;
  foreach (ACR_Squad squad : free)
  {
   attack.m_Squads.Insert(squad);
   strength += squad.Strength();
  }
  PrintFormat("[ACR] AI %1: forced attack (debug menu)", m_sFaction);
  LaunchAttack(attack, from, strength, strength, 1);
  return string.Format("%1 AI attacking %2 with %3 squad(s)", m_sFaction, target.m_sName, free.Count());
 }

 // Where the reserve waits: an own base that is safe (not contested, no known threat, at
 // least RESERVE_MIN_FRONT from every enemy base we know of), bigger first (value, radius),
 // not much farther back than RESERVE_IDEAL_FRONT, and not an HQ (IsHQ). The current
 // reserve base gets a bonus so the reserve doesn't shuttle between two near-equal bases.
 // Falls back to HomeBase (the HQ) when nothing else qualifies.
 static const float RESERVE_MIN_FRONT = 600;
 static const float RESERVE_IDEAL_FRONT = 1200;
 protected ACR_Base m_ReserveBase;

 bool ReserveFits(ACR_Base base)
 {
  if (!base || base.m_sOwner != m_sFaction || IsHQ(base) || base.m_bContested || ThreatTo(base) > 0) return false;
  return FrontDistance(base) >= RESERVE_MIN_FRONT;
 }

 // Distance to the nearest enemy-held base we know of (any other base if none is known).
 float FrontDistance(ACR_Base base)
 {
  float enemy = float.MAX;
  float other = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner == m_sFaction || !Visible(candidate)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), candidate.GetOrigin());
   if (!candidate.m_sOwner.IsEmpty()) enemy = Math.Min(enemy, d);
   other = Math.Min(other, d);
  }
  if (enemy < float.MAX) return enemy;
  return other;
 }

 // An HQ = a base with one of this side's player spawn points (most bases can deploy bought
 // units, so m_bAllowSpawn doesn't tell; the skirmish layout moves the spawn points to its HQs).
 bool IsHQ(ACR_Base base)
 {
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && point.GetFactionKey() == m_sFaction && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  return false;
 }

 ACR_Base ReserveBase()
 {
  ACR_Base best;
  float bestScore = -float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ReserveFits(base)) continue;
   float front = FrontDistance(base);
   float score = base.m_iValue * 150 + base.m_fRadius - Math.Max(0, front - RESERVE_IDEAL_FRONT) * 0.25;
   if (base == m_ReserveBase) score += 100;
   if (score > bestScore) { bestScore = score; best = base; }
  }
  if (!best) best = HomeBase();
  m_ReserveBase = best;
  return best;
 }

 // The own spawn base furthest from the fighting.
 protected ACR_Base HomeBase()
 {
  ACR_Base home;
  float best = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !base.m_bAllowSpawn) continue;
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (nearestFront > best) { best = nearestFront; home = base; }
  }
  return home;
 }

 protected ACR_Base NearestOwnBase(vector point, ACR_Base except = null)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base == except) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 static bool IsServiceVehicle(ACR_RequisitionEntry entry)
 {
  if (entry.m_sVehicle.IsEmpty()) return false;
  string text = entry.m_sName + " " + entry.m_sVehicle;
  text.ToLower();
  array<string> words = {"fuel", "repair", "ammo", "supply", "medical", "ambulance", "command", "radio", "tanker"};
  foreach (string word : words)
   if (text.Contains(word)) return true;
  return false;
 }

 // Long trips on foot become rides or air lifts:
 // Air lift (aca-f22 part 2): long moves (~2 km+ or long road detours) when the flight path is clear of known enemies.
 static const float RIDE_DISTANCE = 900;
 static const float AIRLIFT_DISTANCE = 2000;
 static const float AIRLIFT_ENEMY_CLEARANCE = 350;

 static float DistanceToSegment(vector p, vector a, vector b)
 {
  vector ab = b - a;
  ab[1] = 0;
  float l2 = ab[0] * ab[0] + ab[2] * ab[2];
  if (l2 < 1.0) return vector.DistanceXZ(p, a);
  vector ap = p - a;
  ap[1] = 0;
  float t = Math.Clamp((ap[0] * ab[0] + ap[2] * ab[2]) / l2, 0.0, 1.0);
  vector closest = Vector(a[0] + ab[0] * t, 0, a[2] + ab[2] * t);
  return vector.DistanceXZ(p, closest);
 }

 bool FlightPathClear(vector from, vector to)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (IEntity enemy, vector seenPos : m_SeenAt)
  {
   if (!enemy || !ACR_Respawn.IsAlive(enemy)) continue;
   float seenTime;
   if (!m_SeenTime.Find(enemy, seenTime) || (now - seenTime) > INTEL_MEMORY_MS) continue;
   if (DistanceToSegment(seenPos, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   if (DistanceToSegment(known, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base)) continue;
   float dDrop = vector.DistanceXZ(to, base.GetOrigin());
   if (dDrop < base.m_fRadius + 150) return false;
   float dClosest = DistanceToSegment(base.GetOrigin(), from, to);
   if (dClosest < base.m_fRadius + AIRLIFT_ENEMY_CLEARANCE && dClosest < dDrop - 20) return false;
  }
  return true;
 }

 bool TryAirLift(ACR_Squad squad, vector destination, ResourceName prefab, float radius, string order, int kind, float dropShort = 0)
 {
  if (!m_Manager || !m_Manager.m_Supports) return false;
  if (!squad || !squad.m_Group || squad.HasPlayer()) return false;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) return false;
  if (ACR_AirLift.Busy(squad)) return false;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  vector from = anchor.GetOrigin();
  if (!ACR_Manager.OnMap(destination)) return false;

  float distance = vector.DistanceXZ(from, destination);
  if (distance < ACR_AirLift.MIN_DISTANCE) return false;

  bool isLong = distance >= AIRLIFT_DISTANCE;
  bool isDetour = false;
  if (!isLong && distance >= RIDE_DISTANCE)
  {
   if (BBB_Terrain.WaterAlong(from, destination) > BBB_Terrain.WET_ALLOWED)
   {
    isDetour = true;
   }
   else
   {
    array<vector> roadLegs = {};
    int roadNodes, roadMillis;
    if (!BBB_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis) || BBB_Roads.s_bLastDetour)
    {
     isDetour = true;
    }
    else if (BBB_Roads.s_fLastLength > distance * 1.6)
    {
     isDetour = true;
    }
   }
  }
  if (!isLong && !isDetour) return false;

  if (squad.KnownEnemies(300) > 0) return false;

  int supportIdx = SupportIndex(ACR_ESupportType.AIR_LIFT);
  if (supportIdx < 0) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return false;
  ACR_SupportEntry entry = data.m_aSupports[supportIdx];
  if (!entry || !entry.m_bEnabled) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < entry.m_fReadyAt) return false;
  int cost = entry.CostFor(true);
  if (data.m_iRP < cost + SUPPORT_RESERVE) return false;

  vector targetLZ = destination;
  if (dropShort > 0)
  {
   vector dir = destination - from;
   dir[1] = 0;
   if (dir.Length() > 1)
   {
    dir.Normalize();
    targetLZ = destination - dir * Math.Min(dropShort, distance * 0.5);
   }
  }

  if (!FlightPathClear(from, targetLZ)) return false;

  string result = m_Manager.m_Supports.Call(m_sFaction, supportIdx, targetLZ, true, -1, squad, prefab, destination, radius, order, kind);
  if (!ACR_AirLift.Busy(squad)) return false;
  PrintFormat("[ACR] AI %1: air lift for %2 to %3 (%4 m): %5", m_sFaction, squad.GetName(), ACR_Manager.Grid(destination), (int)distance, result);
  return true;
 }

 protected void Travel(ACR_Squad squad, vector destination, bool defend, float dropShort)
 {
  // Defending a base: infantry garrison its buildings (the one nearest the centre,
  // spilling into its neighbours); vehicle crews, and bases without buildings, defend.
  bool garrison;
  vector building;
  if (defend && !ACR_Transport.CrewedVehicle(squad) && ACR_Garrison.AnchorNear(destination, 80, building))
  {
   garrison = true;
   destination = building;
  }
  ResourceName prefab = ACR_Squad.WP_MOVE;
  float radius = -1;
  string order = "Moving";
  int kind = 0;
  if (defend)
  {
   prefab = ACR_Squad.WP_DEFEND;
   radius = 15;
   order = "Defending";
   kind = 1;
  }
  if (garrison)
  {
   prefab = ACR_Squad.WP_GARRISON;
   order = "Garrisoning";
  }
  if (!squad.IsMounted())
  {
   if (TryAirLift(squad, destination, prefab, radius, order, kind, dropShort)) return;
   if (m_Manager.m_Rides && vector.DistanceXZ(PositionOf(squad), destination) > RIDE_DISTANCE)
   {
    string why;
    if (m_Manager.m_Rides.Request(squad, destination, dropShort, prefab, radius, order, kind, why)) return;
   }
  }
  // On foot (or its own vehicle): round any water rather than across it.
  if (garrison) squad.RouteTo(destination, ACR_Squad.WP_GARRISON, 15, "Garrisoning", 1);
  else if (defend) squad.RouteTo(destination, ACR_Squad.WP_DEFEND, 15, "Defending", 1);
  else squad.RouteTo(destination, ACR_Squad.WP_MOVE, -1, "Moving", 0);
 }

 // A beaten squad breaks contact and falls back to the nearest own base.
 protected void FallBack(ACR_Squad squad, string why, ACR_Base except = null)
 {
  ACR_Base refuge = NearestOwnBase(PositionOf(squad), except);
  squad.m_iAITask = ACR_EAITask.FREE;
  squad.SetMode(ACR_EMoveMode.FAST);
  // On foot, round any water; no waiting for a truck under fire.
  if (refuge) squad.RouteTo(ACR_Manager.Ground(refuge.GetOrigin()), ACR_Squad.WP_DEFEND, 15, "Falling back", 1);
  string where = "its position";
  if (refuge) where = refuge.m_sName;
  PrintFormat("[ACR] AI %1: %2 falls back to %3 (%4)", m_sFaction, squad.GetName(), where, why);
 }

 protected bool IsAttacking(ACR_Base base)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_Target == base) return true;
  return false;
 }

 // True when an assault or grab has gone on too long without taking the base.
 protected bool Stalled(ACR_AIAttack attack)
 {
  if (!attack.m_bAssaulting || attack.m_fAssaultStart <= 0) return false;
  float seconds = (GetGame().GetWorld().GetWorldTime() - attack.m_fAssaultStart) / 1000;
  if (attack.m_bGrab && !s_bGrabByDistance) return seconds > GRAB_TIMEOUT;
  if (attack.m_bGrab) return seconds > Math.Max(GRAB_TIMEOUT, attack.m_fGrabSeconds);
  if (seconds > ASSAULT_HARD_TIMEOUT) return true;
  if (seconds <= ASSAULT_TIMEOUT) return false;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (vector.DistanceXZ(PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius) return false;
  return true;
 }

 // Every couple of minutes, send attackers that stopped short of the base in again.
 protected void PushAttackers(ACR_AIAttack attack)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (attack.m_bGrab || now < attack.m_fNextPush) return;
  if (attack.m_fNextPush > 0)
  {
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (squad == attack.m_Suppressor || squad.m_CombatFlank.Active() || ACR_AirLift.Busy(squad)) continue;
    if (vector.DistanceXZ(PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius * 0.6) continue;
    squad.SetMode(ACR_EMoveMode.COMBAT);
    squad.Assault(attack.m_Target.GetOrigin());
   }
  }
  attack.m_fNextPush = now + PUSH_INTERVAL * 1000;
 }

 // A line in the server log every 5 minutes: what this commander is doing.
 protected void LogStatus()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextStatus) return;
  m_fNextStatus = now + 300000;
  array<ACR_Squad> all = {};
  MySquads(all, false);
  int nFree = 0;
  int nDefend = 0;
  int nAttack = 0;
  int nOther = 0;
  foreach (ACR_Squad squad : all)
  {
   if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED) nFree++;
   else if (squad.m_iAITask == ACR_EAITask.DEFEND) nDefend++;
   else if (squad.m_iAITask == ACR_EAITask.ATTACK) nAttack++;
   else nOther++;
  }
  string attacks;
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   string phase = "staging";
   if (attack.m_bGrab) phase = "grab";
   else if (attack.m_bAssaulting) phase = "assault";
   int minutes = 0;
   if (attack.m_fAssaultStart > 0) minutes = (now - attack.m_fAssaultStart) / 60000;
   attacks += string.Format(" %1(%2 %3m)", attack.m_Target.m_sName, phase, minutes);
  }
  array<ACR_Squad> ready = {};
  MySquads(ready, true);
  attacks += string.Format(" | combat-ready idle %1", ready.Count());
  map<string, int> idleAt = new map<string, int>();
  foreach (ACR_Squad idle : ready)
  {
   ACR_Base near = NearestOwnBase(PositionOf(idle));
   string where = "field";
   if (near && vector.DistanceXZ(PositionOf(idle), near.GetOrigin()) < near.m_fRadius + 150) where = near.m_sName;
   int had;
   idleAt.Find(where, had);
   idleAt.Set(where, had + 1);
  }
  foreach (string place, int n : idleAt) attacks += string.Format(" %1:%2", place, n);
  int visible;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty() && Visible(base)) visible++;
  attacks += string.Format(" | support budget %1", SupportBudget());
  PrintFormat("[ACR] AI %1 status: %2 squads (free %3, defending %4, attacking %5, other %6), attacks:%7, enemy bases known %8, RP %9",
   m_sFaction, all.Count(), nFree, nDefend, nAttack, nOther, attacks, visible, m_Manager.m_Bases.GetRP(m_sFaction));
 }

 protected void UpdateAttacks()
 {
  for (int i = m_Attacks.Count() - 1; i >= 0; i--)
  {
   ACR_AIAttack attack = m_Attacks[i];
   for (int s = attack.m_Squads.Count() - 1; s >= 0; s--)
   {
    ACR_Squad squad = attack.m_Squads[s];
    if (!squad || !m_Manager.m_Squads.Contains(squad) || squad.HasPlayer()) { attack.Remove(s); continue; }
    // Lost its gunner (or down to one man): no use in an attack.
    if (!squad.IsCombatUnit())
    {
     FallBack(squad, "can no longer fight");
     attack.Remove(s);
     continue;
    }
    // Badly hurt and still under fire: pull out instead of dying in place.
    if (attack.m_bAssaulting && squad.StrengthRatio() < RETREAT_RATIO && squad.KnownEnemies(300) > 0)
    {
     FallBack(squad, "heavy casualties");
     attack.Remove(s);
     continue;
    }
    // Shot dry: pull back to rearm instead of charging with empty rifles.
    if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < OUT_OF_AMMO)
    {
     FallBack(squad, "out of ammo");
     attack.Remove(s);
    }
   }
   ACR_Base target = attack.m_Target;
   bool won = target && (target.m_sOwner == m_sFaction || target.m_bRetired);
   if (attack.m_bGrab && target && !target.m_sOwner.IsEmpty() && !won)
   {
    PrintFormat("[ACR] AI %1: %2 was taken first, grab cancelled", m_sFaction, target.m_sName);
    foreach (ACR_Squad late : attack.m_Squads) late.m_iAITask = ACR_EAITask.FREE;
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.Strength();
   bool broken = attack.m_iStartStrength > 0 && strength < attack.m_iStartStrength * ABORT_RATIO;
   bool stalled = !won && target && Stalled(attack);
   if (!target || won || attack.m_Squads.IsEmpty() || broken || stalled)
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (broken) outcome = "called off (too many losses)";
    if (stalled)
    {
     outcome = "called off (stalled)";
     m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + STALLED_COOLDOWN * 1000);
    }
    if (target) PrintFormat("[ACR] AI %1: attack on %2 %3", m_sFaction, target.m_sName, outcome);
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     if (broken) FallBack(squad, "attack called off");
     else squad.m_iAITask = ACR_EAITask.FREE;
    }
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   if (attack.m_bAssaulting)
   {
    if (attack.m_bFobRaid && attack.m_SapperDemo)
    {
     float now = GetGame().GetWorld().GetWorldTime();
     attack.m_SapperDemo.Tick(now);
    }
    PushAttackers(attack);
    // Base of fire keeps suppressing until the assault squads are on the
    // objective, then lifts fire and joins in.
    if (attack.m_Suppressor)
    {
     bool closeIn = false;
     foreach (ACR_Squad assaulter : attack.m_Squads)
      if (assaulter != attack.m_Suppressor && vector.DistanceXZ(PositionOf(assaulter), target.GetOrigin()) < target.m_fRadius + 60) closeIn = true;
     if (closeIn)
     {
      attack.m_Suppressor.SetMode(ACR_EMoveMode.COMBAT);
      attack.m_Suppressor.Assault(target.GetOrigin());
      PrintFormat("[ACR] AI %1: %2 lifts fire and joins the assault on %3", m_sFaction, attack.m_Suppressor.GetName(), target.m_sName);
      attack.m_Suppressor = null;
     }
     else attack.m_Suppressor.Suppress(target.GetOrigin());
    }
    continue;
   }
   bool anyAirLift = false;
   bool gathered = true;
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (ACR_AirLift.Busy(squad)) { anyAirLift = true; gathered = false; continue; }
    if (vector.DistanceXZ(PositionOf(squad), attack.StagingOf(squad)) > 120) gathered = false;
   }
   if (gathered || (!anyAirLift && GetGame().GetWorld().GetWorldTime() > attack.m_fStageDeadline))
   {
    attack.m_bAssaulting = true;
    attack.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
    // Preparation: shell the objective, screen the approach with smoke.
    if (!CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, target.GetOrigin(), "preparation fire on " + target.m_sName))
     if (!CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, target.GetOrigin(), "preparation fire on " + target.m_sName))
      CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, target.GetOrigin(), "battery preparation fire on " + target.m_sName);
    vector screen = attack.m_Staging + (target.GetOrigin() - attack.m_Staging) * 0.55;
    CallSupport(ACR_ESupportType.MORTAR_SMOKE, screen, "smoke screen for " + target.m_sName);
    // Reserve gunship support for an assault, not only the rare counterattack.
    // CallSupport still checks cooldown, range/pad readiness and friendly distance.
    CallSupport(ACR_ESupportType.HELI_LOITER, target.GetOrigin(), "gunship for the assault on " + target.m_sName);
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     squad.SetMode(ACR_EMoveMode.COMBAT);
     if (squad == attack.m_Suppressor) squad.Suppress(target.GetOrigin());
     else squad.Assault(target.GetOrigin());
    }
    string suppressor = "none";
    if (attack.m_Suppressor) suppressor = attack.m_Suppressor.GetName();
    PrintFormat("[ACR] AI %1: assaulting %2 from %3 directions, base of fire: %4", m_sFaction, target.m_sName, attack.m_Squads.Count(), suppressor);
   }
  }
 }

 // ---------------------------------------------------------------- 4. idle squads

 protected void PostIdle()
 {
  array<ACR_Base> front = {};
  FrontlineBases(front);
  if (front.IsEmpty()) return;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  foreach (ACR_Squad squad : free)
  {
   if (squad.m_iAITask == ACR_EAITask.POSTED) continue;
   // Nearest frontline base, but spread out: each squad already posted
   // there counts as 400 m farther, so idle troops don't pile up in one place.
   ACR_Base nearest;
   float best = float.MAX;
   float bestScore = float.MAX;
   foreach (ACR_Base base : front)
   {
    float d = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
    float score = d + PostedAt(base) * 400;
    if (score < bestScore) { bestScore = score; best = d; nearest = base; }
   }
   squad.m_iAITask = ACR_EAITask.POSTED;
   if (!nearest || best < 150) continue;
   squad.SetMode(ACR_EMoveMode.FAST);
   Travel(squad, nearest.GetOrigin(), true, 0);
   PrintFormat("[ACR] AI %1: %2 posted to %3", m_sFaction, squad.GetName(), nearest.m_sName);
  }
 }

 // A staging point around the target, as close to the wanted direction as
 // possible, that is dry, has a dry run in, and stays clear of every other
 // enemy base we know of (forming up next to one drags the force into a fight
 // it didn't plan).
 protected bool StagingPoint(ACR_Base target, vector dir, float ring, out vector point)
 {
  array<float> turns = {0, 30, -30, 60, -60, 90, -90, 120, -120, 150, -150, 180};
  foreach (float turn : turns)
  {
   float angle = turn * Math.DEG2RAD;
   vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
   vector candidate;
   if (!BBB_Terrain.DryApproach(target.GetOrigin(), rotated, ring, candidate)) continue;
   if (NearOtherEnemyBase(candidate, target)) continue;
   point = candidate;
   return true;
  }
  return BBB_Terrain.DryApproach(target.GetOrigin(), dir, ring, point);
 }

 protected bool NearOtherEnemyBase(vector point, ACR_Base except)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base)) continue;
   if (vector.DistanceXZ(point, base.GetOrigin()) < base.m_fRadius + STAGING_CLEARANCE) return true;
  }
  return false;
 }

 // Crews that ended up on foot next to their own intact, empty vehicle get
 // back in (the game's AI sometimes bails out and leaves a BTR standing).
 protected ref map<ACR_Squad, float> m_NextRecrew = new map<ACR_Squad, float>();
 protected void Recrew()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   IEntity vehicle = squad.m_OwnVehicle;
   if (!vehicle || squad.IsMounted() || squad.m_iAITask == ACR_EAITask.RESUPPLY) continue;
   if (ACR_Logistics.Health(vehicle) < 0.25 || !ACR_Vehicles.IsEmpty(vehicle)) continue;
   if (vector.DistanceXZ(PositionOf(squad), vehicle.GetOrigin()) > 400 || squad.KnownEnemies(120) > 0) continue;
   float next;
   if (m_NextRecrew.Find(squad, next) && now < next) continue;
   // Not back into a vehicle that just got stuck (the crew bailed, got back in, got stuck again).
   if (now - squad.m_fStuckAt < 600000) continue;
   m_NextRecrew.Set(squad, now + 90000);
   if (squad.GetIn(vehicle.GetOrigin())) PrintFormat("[ACR] AI %1: %2 gets back into its vehicle", m_sFaction, squad.GetName());
  }
 }

 protected int PostedAt(ACR_Base base)
 {
  int count;
  array<ACR_Squad> mine = {};
  MySquads(mine, true);
  foreach (ACR_Squad squad : mine)
   if (squad.m_iAITask == ACR_EAITask.POSTED && vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) < base.m_fRadius + 150) count++;
  return count;
 }

 // ---------------------------------------------------------------- mortar teams
 // One working team at a time, bought once troops aren't short. While no base that isn't
 // ours is in reach it goes to the own base nearest the front (setting up by itself on
 // arrival), so it follows the front as it moves.
 static const float MORTAR_MOVE_MS = 120000;
 static const float MORTAR_MIN_STANDOFF = 400; // don't set up closer than this to an enemy base
 protected float m_fNextMortarMove;

 protected bool BuyMortarTeam(ACR_Base spawn, ACR_FactionData data)
 {
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams) if (!team.m_bLost) return false;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!entry.m_bMortarTeam || entry.m_iRemaining <= 0) continue;
   if (data.m_iRP < entry.m_iCost + SUPPORT_RESERVE) return false;
   PrintFormat("[ACR] AI %1 requisition (mortar team): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, spawn));
   return true;
  }
  return false;
 }

 // One support gunship on a helipad (worlds with pads), bought again when lost.
 protected bool BuyHelicopter(ACR_FactionData data)
 {
  if (!ACR_HeliStation.UsesPads(m_sFaction) || !m_Manager.m_Supports) return false;
  ACR_HeliStation station = m_Manager.m_Supports.Station();
  if (station.CountOf(m_sFaction, true) > 0) return false;
  ACR_Helipad pad = station.FreePad(m_sFaction, null);
  if (!pad) return false;
  ACR_Base base = pad.Base();
  if (!ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) return false;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iSupportHeli != 1 || entry.m_iRemaining <= 0) continue;
   if (data.m_iRP < entry.m_iCost + SUPPORT_RESERVE) return false;
   PrintFormat("[ACR] AI %1 requisition (support gunship): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, base));
   return true;
  }
  return false;
 }

 // The own, safe base closest to the enemy (but not right on top of it).
 protected ACR_Base MortarBase()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float front = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) front = Math.Min(front, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (front < MORTAR_MIN_STANDOFF || front >= bestFront) continue;
   bestFront = front;
   best = base;
  }
  return best;
 }

 protected void PositionMortars()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextMortarMove) return;
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   ACR_Squad squad = team.m_Squad;
   if (!squad || squad.HasPlayer() || team.m_bLost || team.m_bFiring) continue;
   if (team.m_eState == ACR_EMortarState.PACKING || team.m_eState == ACR_EMortarState.DEPLOYING) continue;
   vector here = PositionOf(squad);
   if (team.m_eState == ACR_EMortarState.DEPLOYED) here = team.m_Spot;
   else if (ACR_Mortars.IsMoving(squad, here)) continue; // on its way
   // Stays while it can reach a base that isn't ours (with some margin).
   float reach = ACR_Mortars.MaxRange(team.m_sTube) * 0.85;
   bool inReach = false;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction && vector.DistanceXZ(here, other.GetOrigin()) < reach) inReach = true;
   if (inReach) continue;
   ACR_Base front = MortarBase();
   if (!front || vector.DistanceXZ(here, front.GetOrigin()) < front.m_fRadius + 80) continue;
   vector spot = ACR_Manager.Ground(front.GetOrigin() + Vector(Math.RandomFloat(-20, 20), 0, Math.RandomFloat(-20, 20)));
   squad.MoveTo(spot);
   m_fNextMortarMove = now + MORTAR_MOVE_MS;
   PrintFormat("[ACR] AI %1 mortar: %2 moving to %3", m_sFaction, squad.GetName(), front.m_sName);
  }
 }

 // ---------------------------------------------------------------- 5. requisition

 protected void Requisition()
 {
  ACR_FactionSettings settings = Settings();
  int cap = 8;
  if (settings) cap = settings.m_iAIUnitCap;
  // Money piling up: field a few more squads rather than sit on it.
  int banked = m_Manager.m_Bases.GetRP(m_sFaction);
  if (banked > 150) cap += Math.Min(4, (banked - 100) / 50);
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  // Only fighting units count against the cap (trucks and lone drivers don't).
  for (int m = mine.Count() - 1; m >= 0; m--)
   if (!mine[m].IsCombatUnit()) mine.RemoveOrdered(m);
  if (mine.Count() >= cap) return;
  // Deploy at the own spawn base furthest from the fighting.
  ACR_Base spawn;
  float best = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) continue;
   // distance to the nearest non-owned base = how far behind the lines it is
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (ThreatTo(base) > 0) nearestFront = 0;
   if (nearestFront > best) { best = nearestFront; spawn = base; }
  }
  if (!spawn) return;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  // A service truck for a need nobody can meet (squads low on ammo, a wrecked
  // BRDM...), one of each kind at a time.
  if (m_Manager.m_Logistics)
  {
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((m_iNeeds & service) == 0 || m_Manager.m_Logistics.CountStations(m_sFaction, service) > 0) continue;
    int pick = -1;
    foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
    {
     if (offer.m_iRemaining <= 0 || offer.m_iCost > data.m_iRP || offer.m_sVehicle.IsEmpty()) continue;
     if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & service) == 0) continue;
     if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
    }
    if (pick < 0) continue;
    PrintFormat("[ACR] AI %1 requisition (%2 support): %3", m_sFaction, ACR_Logistics.ServiceName(service), m_Manager.PurchaseAt(m_sFaction, pick, spawn));
    return;
   }
  }
  // Squads keep having to walk far with no truck to fetch them: buy a transport first.
  if (m_Manager.m_Rides && m_Manager.m_Rides.ShortOfTransport(m_sFaction) && m_Manager.m_Rides.CountTransports(m_sFaction) < 3)
  {
   int truck = -1;
   foreach (int t, ACR_RequisitionEntry candidate : data.m_aRequisitions)
    if (candidate.m_bTransport && !IsServiceVehicle(candidate) && candidate.m_iRemaining > 0 && candidate.m_iCost <= data.m_iRP && (truck < 0 || candidate.m_iCost < data.m_aRequisitions[truck].m_iCost)) truck = t;
   if (truck >= 0)
   {
    PrintFormat("[ACR] AI %1 requisition (transport): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, truck, spawn));
    m_Manager.m_Rides.m_NoTransportAt.Remove(m_sFaction);
    return;
   }
  }
  // Buy up to two units per tick. The pick is weighted by price, so real fighting
  // power (rifle squads, MG teams, armed vehicles) wins over a swarm of cheap
  // two-man teams; specialist teams (engineers, sappers, medics, logistics,
  // recon) are rare and never doubled up.
  // Keep a fire-support budget unless troops are needed more.
  int keep = 0;
  bool highThreat = ACR_Manager.ThreatOption() == 2;
  if (highThreat)
  {
   // High intensity should field its fire support even while the infantry pool
   // is still building. These checks remain stock/RP/pad gated and buy at most
   // one support unit per strategy tick.
   keep = SupportBudget();
   if (BuyMortarTeam(spawn, data)) return;
   if (BuyHelicopter(data)) return;
  }
  else if (!UrgentTroops(mine.Count(), cap))
  {
   keep = SupportBudget();
   if (BuyMortarTeam(spawn, data)) return;
   if (BuyHelicopter(data)) return;
  }
  for (int n = 0; n < 2 && mine.Count() + n < cap; n++)
  {
   array<int> options = {};
   array<float> weights = {};
   float total;
   foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
   {
    if (entry.m_iRemaining <= 0 || entry.m_iCost > data.m_iRP - keep) continue;
    if (entry.m_bMortarTeam) continue; // bought on its own (BuyMortarTeam)
    // AI can't fly: a bought helicopter would just sit at base. Gunships come as supports instead.
    if (entry.m_bAircraft || entry.m_sVehicle.Contains("/Helicopters/") || entry.m_sVehicle.Contains("/Planes/")) continue;
    // Nor service vehicles (bought on need) or unarmed cars (nothing to fight with).
    if (IsServiceVehicle(entry)) continue;
    if (!entry.m_sVehicle.IsEmpty() && !entry.m_bArmed) continue;
    // Weigh by the base price: a unit whose stepped price has climbed isn't worth more.
    float weight = entry.m_iCost;
    if (entry.m_iBaseCost > 0) weight = entry.m_iBaseCost;
    if (highThreat && !entry.m_sVehicle.IsEmpty()) weight *= 1.5;
    if (highThreat && m_sFaction == "RHS_AFRF" && IsRUArmorEntry(entry)) weight *= 2;
    bool isSapper = IsSapperEntry(entry);
    if (isSapper && HasEnemyFob())
    {
     if (!OwnsLike(entry)) weight *= 3.0;
    }
    else if (IsSpecialistTeam(entry))
    {
     if (OwnsLike(entry)) continue;
     weight *= 0.3;
    }
    // Keep infantry the backbone.
    if (!entry.m_sVehicle.IsEmpty()) weight *= 0.5;
    // A low-threat war is mostly fought on foot.
    if (!entry.m_sVehicle.IsEmpty() && ACR_Manager.ThreatOption() == 0) weight *= 0.4;
    options.Insert(i);
    weights.Insert(weight);
    total += weight;
   }
   if (options.IsEmpty()) return;
   float roll = Math.RandomFloat(0, total);
   int pick = options[options.Count() - 1];
   foreach (int k, float w : weights)
   {
    roll -= w;
    if (roll <= 0) { pick = options[k]; break; }
   }
   string result = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
   PrintFormat("[ACR] AI %1 requisition: %2", m_sFaction, result);
  }
 }

 protected bool IsRUArmorEntry(ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty()) return false;
  string vehicle = FilePath.StripExtension(FilePath.StripPath(entry.m_sVehicle));
  vehicle.ToLower();
  return vehicle.Contains("btr70") || vehicle.Contains("brdm2");
 }

 static bool IsSapperEntry(ACR_RequisitionEntry entry)
 {
  if (!entry) return false;
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("sapper") || name.Contains("engineer")) return true;
  string grp = entry.m_sGroup;
  grp.ToLower();
  if (grp.Contains("sapper") || grp.Contains("engineer")) return true;
  return false;
 }

 bool HasEnemyFob()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty()) continue;
   if (base.m_bBuilt && Visible(base)) return true;
  }
  return false;
 }

 static bool IsSpecialistTeam(ACR_RequisitionEntry entry)
 {
  string name = entry.m_sName;
  name.ToLower();
  array<string> words = {"engineer", "sapper", "medic", "logistic", "supply", "recon", "sniper", "hq", "radio"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 // Already have a unit bought from this entry (names are numbered per purchase).
 protected bool OwnsLike(ACR_RequisitionEntry entry)
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
   if (squad.GetName().StartsWith(entry.m_sName)) return true;
  return false;
 }

 // ---------------------------------------------------------------- 6. logistics
 // Squads low on ammo, hurt, or with a damaged / thirsty vehicle get the matching
 // service vehicle: it drives out to squads holding a position, while free
 // squads go back to it. Idle service vehicles wait at the safest base nearest
 // the front, and go home to restock when they run low. Lose the truck and the
 // front runs dry.
 static const int LOW_AMMO = 45;
 static const int OUT_OF_AMMO = 15;           // below this a squad in a fight pulls back
 static const int LOW_HEALTH = 60;
 static const int LOW_FUEL = 25;
 static const float SERVICE_REACH = 2000;     // farthest a service vehicle is sent or sought
 protected int m_iNeeds;                      // services wanted this tick (for requisition)
 protected ref map<ACR_Squad, ACR_Squad> m_ServiceJobs = new map<ACR_Squad, ACR_Squad>(); // service crew -> squad it is serving

 protected ACR_Squad CrewOf(IEntity vehicle)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (ACR_Transport.CrewedVehicle(squad) == vehicle) return squad;
  return null;
 }

 protected bool InAssault(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_bAssaulting && !attack.m_bGrab && attack.m_Squads.Contains(squad)) return true;
  return false;
 }

 protected int NeedsOf(ACR_Squad squad)
 {
  int need;
  if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < LOW_AMMO) need |= ACR_EService.AMMO;
  if (squad.m_iAvgHpPct < LOW_HEALTH) need |= ACR_EService.MEDICAL;
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (vehicle)
  {
   if (ACR_Logistics.Health(vehicle) * 100 < LOW_HEALTH) need |= ACR_EService.REPAIR;
   float fuel = ACR_Logistics.Fuel(vehicle);
   if (fuel >= 0 && fuel * 100 < LOW_FUEL) need |= ACR_EService.FUEL;
  }
  return need;
 }

 protected void ManageLogistics()
 {
  ACR_Logistics logistics = m_Manager.m_Logistics;
  m_iNeeds = 0;
  if (!logistics) return;
  // Finished jobs: the served squad no longer needs it, or either side is gone.
  array<ACR_Squad> done = {};
  foreach (ACR_Squad jobCrew, ACR_Squad jobClient : m_ServiceJobs)
   if (!jobCrew || !jobClient || !m_Manager.m_Squads.Contains(jobCrew) || !m_Manager.m_Squads.Contains(jobClient) || NeedsOf(jobClient) == 0) done.Insert(jobCrew);
  foreach (ACR_Squad finished : done) m_ServiceJobs.Remove(finished);
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   int need = NeedsOf(squad);
   if (squad.m_iAITask == ACR_EAITask.RESUPPLY && need == 0)
   {
    squad.m_iAITask = ACR_EAITask.FREE;
    PrintFormat("[ACR] AI %1: %2 resupplied", m_sFaction, squad.GetName());
   }
   if (need == 0 || InAssault(squad) || ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(squad)) != 0) continue;
   m_iNeeds |= need;
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((need & service) == 0) continue;
    array<IEntity> stations = {};
    logistics.Stations(m_sFaction, service, stations);
    IEntity station;
    float best = SERVICE_REACH;
    foreach (IEntity candidate : stations)
    {
     float d = vector.DistanceXZ(candidate.GetOrigin(), PositionOf(squad));
     if (d < best) { best = d; station = candidate; }
    }
    if (!station || best < ACR_Logistics.SERVICE_RANGE) continue; // none, or already being served
    ACR_Squad crew = CrewOf(station);
    bool holding = squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.POSTED || !squad.m_Group.GetCurrentWaypoint();
    bool crewFree = crew && !crew.HasPlayer() && !m_ServiceJobs.Contains(crew);
    string what = ACR_Logistics.ServiceName(service);
    if (crewFree && (holding || squad.IsMounted() == false && best > 800))
    {
     // Bring the truck to them.
     crew.SetMode(ACR_EMoveMode.FAST);
     crew.RouteTo(ACR_Manager.Ground(PositionOf(squad)), ACR_Squad.WP_MOVE, -1, "Resupplying " + squad.GetName(), 0);
     m_ServiceJobs.Set(crew, squad);
     PrintFormat("[ACR] AI %1: %2 drives out to %3 (%4)", m_sFaction, crew.GetName(), squad.GetName(), what);
    }
    else if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED)
    {
     // Go back to the truck.
     squad.m_iAITask = ACR_EAITask.RESUPPLY;
     squad.SetMode(ACR_EMoveMode.FAST);
     Travel(squad, ACR_Manager.Ground(station.GetOrigin()), false, 0);
     PrintFormat("[ACR] AI %1: %2 falls back to the %3 truck", m_sFaction, squad.GetName(), what);
    }
    break;
   }
  }
  // Idle service vehicles: restock if low, otherwise wait at the supply point.
  ACR_Base supplyPoint = SupplyPoint();
  foreach (ACR_Squad idleCrew : mine)
  {
   IEntity serviceVehicle = ACR_Transport.CrewedVehicle(idleCrew);
   if (!serviceVehicle || ACR_Logistics.ServiceOf(serviceVehicle) == 0 || idleCrew.HasPlayer() || m_ServiceJobs.Contains(idleCrew)) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.BUILD_FOB || idleCrew.m_iAITask == ACR_EAITask.BUILD_FOB) continue;
   ACR_Base goal = supplyPoint;
   if (logistics.StockPercent(serviceVehicle) < 25) goal = HomeBase();
   if (!goal || vector.DistanceXZ(serviceVehicle.GetOrigin(), goal.GetOrigin()) < goal.m_fRadius) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(idleCrew.m_MoveTarget, goal.GetOrigin()) < goal.m_fRadius) continue;
   idleCrew.RouteTo(ACR_Manager.Ground(goal.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   PrintFormat("[ACR] AI %1: %2 to %3 (%4)", m_sFaction, idleCrew.GetName(), goal.m_sName, ACR_Logistics.ServiceName(ACR_Logistics.ServiceOf(serviceVehicle)));
  }
 }

 // Our base nearest the front that isn't threatened: where service vehicles wait.
 protected ACR_Base SupplyPoint()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float front = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction && Visible(other)) front = Math.Min(front, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (front < 500) continue; // too close to the enemy to park trucks
   if (front < bestFront) { bestFront = front; best = base; }
  }
  if (!best) best = HomeBase();
  return best;
 }

 // ---------------------------------------------------------------- FOB establishment (Stage 3)
 static const float FOB_FRONT_DISTANCE = 1500;       // 1.5 km front gap requirement
 static const float FOB_EXISTING_PROXIMITY = 1000;   // don't build within 1 km of existing friendly FOB
 static const int FOB_RP_COMBAT_RESERVE = 30;        // RP reserve required when buying construction truck
 static const float FOB_EVAL_INTERVAL_MS = 30000;    // evaluate every 30 seconds

 protected ACR_Squad m_FobBuilder;
 protected ACR_Squad m_FobEscort;
 protected vector m_vFobTargetSite;
 protected float m_fNextFobEval;

 bool HasActiveFobProject()
 {
  if (m_FobBuilder)
  {
   if (m_FobBuilder.CountAlive() > 0 && m_FobBuilder.OrderState() == ACR_EOrderState.BUILD_FOB) return true;
   m_FobBuilder = null;
   m_FobEscort = null;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_iAITask == ACR_EAITask.BUILD_FOB) return true;
  }
  return false;
 }

 ACR_Base NearestDeployBase(ACR_Base fromBase)
 {
  if (!fromBase) return null;
  ACR_Base best;
  float bestDist = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
   float d = vector.DistanceXZ(fromBase.GetOrigin(), candidate.GetOrigin());
   if (d < bestDist)
   {
    bestDist = d;
    best = candidate;
   }
  }
  return best;
 }

 ACR_Base NeedsFob()
 {
  array<ACR_Base> frontBases = {};
  FrontlineBases(frontBases);
  if (frontBases.IsEmpty()) return null;

  foreach (ACR_Base front : frontBases)
  {
   if (!front) continue;
   float nearestDeploy = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
    float d = vector.DistanceXZ(front.GetOrigin(), candidate.GetOrigin());
    if (d < nearestDeploy) nearestDeploy = d;
   }

   if (nearestDeploy >= FOB_FRONT_DISTANCE)
   {
    bool hasCloseFob = false;
    foreach (ACR_Base existingFob : ACR_Base.s_All)
    {
     if (existingFob.m_sOwner != m_sFaction || !existingFob.m_bBuilt || existingFob.m_bRetired) continue;
     if (vector.DistanceXZ(front.GetOrigin(), existingFob.GetOrigin()) < FOB_EXISTING_PROXIMITY)
     {
      hasCloseFob = true;
      break;
     }
    }
    if (!hasCloseFob) return front;
   }
  }
  return null;
 }

 bool FindFobSite(ACR_Base front, out vector site)
 {
  site = Vector(0, 0, 0);
  if (!front) return false;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir;
  if (deployBase)
  {
   dir = deployBase.GetOrigin() - front.GetOrigin();
   dir[1] = 0;
   if (dir.LengthSq() > 0.001) dir.Normalize();
   else dir = Vector(0, 0, -1);
  }
  else
  {
   dir = Vector(0, 0, -1);
  }

  array<float> dists = {450.0, 550.0, 350.0, 650.0};
  array<float> angles = {0.0, 25.0, -25.0, 50.0, -50.0};

  foreach (float d : dists)
  {
   foreach (float a : angles)
   {
    float rad = a * Math.DEG2RAD;
    float cosA = Math.Cos(rad);
    float sinA = Math.Sin(rad);
    vector rotatedDir = Vector(dir[0] * cosA - dir[2] * sinA, 0, dir[0] * sinA + dir[2] * cosA);
    vector candidate = front.GetOrigin() + rotatedDir * d;
    candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);

    vector validSpot;
    string refusal;
    if (!ACR_FobBuild.ValidateSite(candidate, validSpot, refusal)) continue;

    bool safe = true;
    foreach (ACR_Base b : ACR_Base.s_All)
    {
     if (b.m_sOwner != m_sFaction && !b.m_sOwner.IsEmpty() && !b.m_bRetired)
     {
      if (vector.DistanceXZ(b.GetOrigin(), validSpot) < 600.0)
      {
       safe = false;
       break;
      }
     }
    }
    if (!safe || ACR_FobBuild.IsContested(validSpot, m_sFaction)) continue;

    site = validSpot;
    return true;
   }
  }
  return false;
 }

 ACR_Squad FindConstructionSquad()
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0) continue;
   if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.m_iAITask == ACR_EAITask.DEFEND) continue;
   IEntity truck = squad.ConstructionTruck();
   if (!truck || ACR_Logistics.Health(truck) <= 0) continue;
   if (m_Manager.m_Logistics && m_Manager.m_Logistics.StockOf(truck) >= ACR_FobBuild.LOADS_RESERVED)
    return squad;
  }
  return null;
 }

 bool BuyConstructionTruck(ACR_Base spawn)
 {
  if (!spawn) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data || !data.m_aRequisitions) return false;

  int pick = -1;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (offer.m_iRemaining <= 0 || offer.m_iCost > data.m_iRP || offer.m_sVehicle.IsEmpty()) continue;
   if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
  }
  if (pick < 0) return false;

  int cost = data.m_aRequisitions[pick].m_iCost;
  if (data.m_iRP < cost + FOB_RP_COMBAT_RESERVE) return false;

  string bought = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
  PrintFormat("[ACR] AI %1 requisition (FOB construction truck): %2", m_sFaction, bought);
  return !bought.IsEmpty();
 }

 ACR_Squad FindEscortSquad(ACR_Squad builder)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  foreach (ACR_Squad squad : free)
  {
   if (!squad || squad == builder || squad.HasPlayer() || squad.CountAlive() <= 0) continue;
   if (squad.IsCombatUnit()) return squad;
  }
  return null;
 }

 void ManageFobBuild()
 {
  if (HasActiveFobProject()) return;

  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobEval) return;
  m_fNextFobEval = now + FOB_EVAL_INTERVAL_MS;

  ACR_Base front = NeedsFob();
  if (!front) return;

  vector site;
  if (!FindFobSite(front, site)) return;

  ACR_Squad builder = FindConstructionSquad();
  if (!builder)
  {
   ACR_Base spawn = NearestDeployBase(front);
   if (!spawn) spawn = HomeBase();
   BuyConstructionTruck(spawn);
   return;
  }

  builder.m_iAITask = ACR_EAITask.BUILD_FOB;
  if (!builder.EstablishFob(site))
  {
   builder.m_iAITask = ACR_EAITask.FREE;
   return;
  }

  m_FobBuilder = builder;
  m_vFobTargetSite = site;

  ACR_Squad escort = FindEscortSquad(builder);
  if (escort)
  {
   m_FobEscort = escort;
   escort.m_iAITask = ACR_EAITask.DEFEND;
   escort.SetMode(ACR_EMoveMode.FAST);
   escort.RouteTo(site, ACR_Squad.WP_DEFEND, -1, "Escorting FOB construction", 0);
   PrintFormat("[ACR] AI %1: %2 assigned as escort for FOB build at %3", m_sFaction, escort.GetName(), site);
  }

  PrintFormat("[ACR] AI %1: dispatched %2 to establish forward FOB near %3 at %4", m_sFaction, builder.GetName(), front.m_sName, site);
 }

 static const float FOB_UPGRADE_EVAL_INTERVAL_MS = 15000;
 protected float m_fNextFobUpgradeEval;

 void SetFobUpgradeEvalTimeForTest(float time) { m_fNextFobUpgradeEval = time; }

 void ManageFobUpgrades()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobUpgradeEval) return;
  m_fNextFobUpgradeEval = now + FOB_UPGRADE_EVAL_INTERVAL_MS;

  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return;

  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bBuilt || base.m_sOwner != m_sFaction || !base.m_FobSupply) continue;
   if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) continue;

   // 1. Supply replenishment check
   OrderFobSupplies(base, data);

   // 2. Upgrade check & engineer acceleration
   if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);
   bool isBuilding = false;
   foreach (ACR_FobSlot slot : base.m_aFobSlots)
   {
    if (slot && slot.m_eState == ACR_EFobSlotState.BUILDING)
    {
     isBuilding = true;
     break;
    }
   }

   if (isBuilding)
   {
    AssistFobConstruction(base);
   }
   else
   {
    int stock = ACR_FobSupply.Stock(base.m_FobSupply);
    OrderFobUpgrade(base, stock);
   }
  }
 }

 bool OrderFobSupplies(ACR_Base base, ACR_FactionData data)
 {
  if (!base || !base.m_FobSupply || !data) return false;
  if (base.m_iSupplyDelivery > 0) return false;
  if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) return false;

  int stock = ACR_FobSupply.Stock(base.m_FobSupply);
  int capacity = ACR_FobSupply.CapacityOf(base);
  if (capacity - stock < ACR_FobSupply.DELIVERY_AMOUNT) return false;

  if (stock >= 500) return false;

  if (data.m_iRP < ACR_FobSupply.DELIVERY_COST + FOB_RP_COMBAT_RESERVE) return false;

  string result = ACR_FobSupply.Order(m_Manager, m_sFaction, base);
  PrintFormat("[ACR] AI %1 ordered supplies for %2: %3", m_sFaction, base.m_sName, result);
  return base.m_iSupplyDelivery > 0;
 }

 int BestUpgradeSlotFor(ACR_Base base, out int targetTier)
 {
  targetTier = 0;
  if (!base) return -1;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  array<int> priorities = {
   ACR_FobSlots.SLOT_RELAY,
   ACR_FobSlots.SLOT_BARRACKS,
   ACR_FobSlots.SLOT_REPAIR,
   ACR_FobSlots.SLOT_FUEL,
   ACR_FobSlots.SLOT_HOSPITAL,
   ACR_FobSlots.SLOT_DEPOT,
   ACR_FobSlots.SLOT_AMMO
  };

  foreach (int slotIdx : priorities)
  {
   if (slotIdx < 0 || slotIdx >= base.m_aFobSlots.Count()) continue;
   ACR_FobSlot slot = base.m_aFobSlots[slotIdx];
   if (!slot) continue;
   if (slot.m_eState == ACR_EFobSlotState.EMPTY)
   {
    targetTier = 0;
    return slotIdx;
   }
  }

  // Tier 1 upgrades after base tiers are built
  ACR_FobSlot barracksSlot = base.m_aFobSlots[ACR_FobSlots.SLOT_BARRACKS];
  if (barracksSlot && barracksSlot.m_eState == ACR_EFobSlotState.BUILT && barracksSlot.m_iTier < 1)
  {
   targetTier = 1;
   return ACR_FobSlots.SLOT_BARRACKS;
  }

  ACR_FobSlot repairSlot = base.m_aFobSlots[ACR_FobSlots.SLOT_REPAIR];
  if (repairSlot && repairSlot.m_eState == ACR_EFobSlotState.BUILT && repairSlot.m_iTier < 1)
  {
   targetTier = 1;
   return ACR_FobSlots.SLOT_REPAIR;
  }

  ACR_FobSlot heliSlot = base.m_aFobSlots[ACR_FobSlots.SLOT_HELIPAD];
  if (heliSlot && heliSlot.m_eState == ACR_EFobSlotState.EMPTY)
  {
   targetTier = 0;
   return ACR_FobSlots.SLOT_HELIPAD;
  }

  return -1;
 }

 bool OrderFobUpgrade(ACR_Base base, int stock)
 {
  if (!base) return false;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  int targetTier = 0;
  int pickSlot = BestUpgradeSlotFor(base, targetTier);
  if (pickSlot < 0) return false;

  int cost = ACR_FobSlots.SlotCost(pickSlot, targetTier);
  if (stock < cost) return false;

  string refusal;
  string outcome = ACR_FobSlots.OrderUpgradeBase(m_Manager, m_sFaction, base, pickSlot, refusal);
  if (!outcome.IsEmpty())
  {
   PrintFormat("[ACR] AI %1: ordered FOB %2 upgrade %3 (tier %4)", m_sFaction, base.m_sName, ACR_FobSlots.SlotName(pickSlot, targetTier), targetTier);
   return true;
  }
  return false;
 }

 void AssistFobConstruction(ACR_Base base)
 {
  if (!base || ACR_FobSlots.HasEngineer(base)) return;

  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0) continue;
   if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.BUILD_FOB) continue;
   IEntity truck = squad.ConstructionTruck();
   if (!truck || ACR_Logistics.Health(truck) <= 0) continue;

   float dist = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
   if (dist <= 500.0)
   {
    squad.SetMode(ACR_EMoveMode.FAST);
    squad.RouteTo(base.GetOrigin(), ACR_Squad.WP_MOVE, -1, "Assisting FOB upgrade construction", 0);
    PrintFormat("[ACR] AI %1: dispatched %2 to assist upgrade construction at %3", m_sFaction, squad.GetName(), base.m_sName);
    return;
   }
  }
 }

 void SetFobEvalTimeForTest(float time) { m_fNextFobEval = time; }
 ACR_Squad FobBuilder() { return m_FobBuilder; }
 ACR_Squad FobEscort() { return m_FobEscort; }
 vector FobTargetSite() { return m_vFobTargetSite; }
 void SetFobBuilderForTest(ACR_Squad builder) { m_FobBuilder = builder; }
 ACR_Base GetHomeBase() { return HomeBase(); }

 // ---------------------------------------------------------------- hand-over

 // A player took command: leave every squad as it is, but forget AI tasks.
 protected void ReleaseAll()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction) squad.m_iAITask = ACR_EAITask.FREE;
  m_Attacks.Clear();
  m_FobBuilder = null;
  m_FobEscort = null;
 }
}
