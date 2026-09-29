enum ACR_EAITask
{
 FREE,      // available for orders
 DEFEND,    // holding a threatened base
 ATTACK,    // part of a base attack
 POSTED     // idle, moved up to a frontline base
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
 static const float ATTACK_RATIO = 1.8;       // attackers want 1.8x the defence
 static const float DEFENSE_RATIO = 1.1;      // defenders want 1.1x the threat
 static const float CONCEDE_RATIO = 4;        // give a base up against 4x the defence
 static const int PLAYER_STRENGTH = 5;
 static const float STAGING_TIMEOUT = 300;    // seconds

 ACR_Manager m_Manager;
 string m_sFaction;
 bool m_bActive;
 protected int m_iSeconds;
 protected ref array<ref ACR_AIAttack> m_Attacks = {};
 protected ref map<ACR_Base, int> m_Defence = new map<ACR_Base, int>();
 protected ref map<ACR_Base, int> m_Threat = new map<ACR_Base, int>();
 protected ref map<ACR_Base, string> m_LastOwner = new map<ACR_Base, string>();
 protected ACR_Base m_CounterTarget;
 protected float m_fCounterUntil;
 static const float RETREAT_RATIO = 0.4;   // a squad below 40% in contact falls back
 static const float ABORT_RATIO = 0.35;    // an attack below 35% of its strength is called off
 static const int SUPPORT_RESERVE = 15;    // RP the AI keeps back when calling supports
 static const int MAX_GRABS = 3;           // empty bases it goes for at the same time

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
  m_iSeconds++;
  if (m_iSeconds % TICK_SECONDS != 1) return;
  Assess();
  UpdateAttacks();
  AssignDefenders();
  AssignAttacks();
  PostIdle();
  Requisition();
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
   if (freeOnly && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
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
  float best = float.MAX;
  foreach (ACR_Base base : mine)
  {
   float nearest = float.MAX;
   foreach (ACR_Base other : others) nearest = Math.Min(nearest, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (nearest < best - 1) { best = nearest; result.Clear(); }
   if (nearest <= best + 150) result.Insert(base);
  }
 }

 // ---------------------------------------------------------------- supports

 protected int SupportIndex(ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 protected bool SupportReady(ACR_ESupportType type)
 {
  int index = SupportIndex(type);
  if (index < 0 || !m_Manager.m_Supports) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  ACR_SupportEntry entry = data.m_aSupports[index];
  return GetGame().GetWorld().GetWorldTime() >= entry.m_fReadyAt && data.m_iRP >= entry.m_iCost + SUPPORT_RESERVE;
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
  if (FriendliesNear(point, ACR_Supports.ZoneRadius(type) + 40)) return false;
  string result = m_Manager.m_Supports.Call(m_sFaction, SupportIndex(type), point, true);
  PrintFormat("[ACR] AI %1 support (%2): %3", m_sFaction, why, result);
  return true;
 }

 // Enemy squad nearest to a point (for defensive fires).
 protected ACR_Squad EnemyNear(vector point, float radius)
 {
  ACR_Squad best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction || !squad.GetAnchor()) continue;
   float distance = vector.DistanceXZ(PositionOf(squad), point);
   if (distance < bestDistance) { bestDistance = distance; best = squad; }
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
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || !squad.GetAnchor()) continue;
    float distance = vector.DistanceXZ(PositionOf(squad), center);
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
    float d = vector.DistanceXZ(body.GetOrigin(), center);
    if (faction.GetFactionKey() == base.m_sOwner && d < DEFENSE_DISTANCE) defence += PLAYER_STRENGTH;
    if (faction.GetFactionKey() != base.m_sOwner && d < THREAT_DISTANCE + base.m_fRadius) threat += PLAYER_STRENGTH;
   }
   if (!base.m_sOwner.IsEmpty()) defence += base.m_iGarrisonLeft;
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
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad.m_iAITask != ACR_EAITask.DEFEND) continue;
   ACR_Base held = squad.m_AITarget;
   if (!held || held.m_sOwner != m_sFaction || ThreatTo(held) == 0) squad.m_iAITask = ACR_EAITask.FREE;
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
    squad.Defend(base.GetOrigin());
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
   ACR_Squad attacker = EnemyNear(held.GetOrigin(), THREAT_DISTANCE + held.m_fRadius);
   if (!attacker) continue;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, PositionOf(attacker), "defensive fire at " + held.m_sName)) break;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, PositionOf(attacker), "defensive fire at " + held.m_sName)) break;
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
  // Keep a reserve at home once the force is big enough.
  array<ACR_Squad> all = {};
  MySquads(all, false);
  if (all.Count() >= 5 && free.Count() >= 2)
  {
   ACR_Base home = HomeBase();
   if (home)
   {
    SortByDistance(free, home.GetOrigin());
    ACR_Squad reserve = free[0];
    free.RemoveOrdered(0);
    if (reserve.m_iAITask != ACR_EAITask.POSTED || vector.DistanceXZ(PositionOf(reserve), home.GetOrigin()) > 150)
    {
     reserve.m_iAITask = ACR_EAITask.POSTED;
     reserve.SetMode(ACR_EMoveMode.COMBAT);
     reserve.Defend(home.GetOrigin());
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
  while (grabs < MAX_GRABS && !free.IsEmpty())
  {
   ACR_Base empty;
   ACR_Squad runner;
   float bestTime = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!candidate.m_sOwner.IsEmpty() || !Visible(candidate) || IsAttacking(candidate)) continue;
    foreach (ACR_Squad squad : free)
    {
     if (squad.Strength() < 2) continue; // not a lone driver
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
   grab.m_Squads.Insert(runner);
   grab.m_aStaging.Insert(empty.GetOrigin());
   grab.m_iStartStrength = runner.Strength();
   runner.m_iAITask = ACR_EAITask.ATTACK;
   runner.m_AITarget = empty;
   runner.SetMode(ACR_EMoveMode.FAST);
   runner.MoveTo(empty.GetOrigin());
   free.RemoveItem(runner);
   m_Attacks.Insert(grab);
   grabs++;
   PrintFormat("[ACR] AI %1: %2 grabs empty %3 (about %4 s away)", m_sFaction, runner.GetName(), empty.m_sName, (int)bestTime);
  }
  if (free.IsEmpty()) return;
  if (free.Count() >= 4) maxAttacks++; // plenty of idle troops: open a second front
  if (attacks >= maxAttacks && !counterDue) return;

  // Target: a counterattack on a base just lost, otherwise the enemy base with
  // the lowest (defence x ratio) scaled by distance.
  ACR_Base target;
  vector from = PositionOf(free[0]);
  if (counterDue) target = m_CounterTarget;
  else
  {
   float bestScore = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base) || IsAttacking(base)) continue;
    float cost = Math.Max(10, DefenceOf(base) * ATTACK_RATIO);
    float score = cost * (vector.DistanceXZ(base.GetOrigin(), from) / 1000 + 0.1);
    if (score < bestScore) { bestScore = score; target = base; }
   }
  }
  if (!target) return;

  float required = 1;
  if (!target.m_sOwner.IsEmpty()) required = Math.Max(10, DefenceOf(target) * ATTACK_RATIO * 0.65) + target.m_iGarrisonLeft;
  if (counterDue) required = required * 0.7; // strike before they dig in
  SortByDistance(free, target.GetOrigin());
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  attack.m_bCounter = counterDue;
  int strength;
  foreach (ACR_Squad squad : free)
  {
   attack.m_Squads.Insert(squad);
   strength += squad.Strength();
   if (strength >= required) break;
  }
  if (strength < required)
  {
   PrintFormat("[ACR] AI %1: not strong enough for %2 (%3/%4)", m_sFaction, target.m_sName, strength, required);
   return; // Requisition() will add troops
  }
  attack.m_iStartStrength = strength;
  // Staging: the main approach from the attackers' side; with several squads
  // they spread out and come in from different angles (pincers).
  vector dir = from - target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  float ring = target.m_fRadius + STAGING_DISTANCE;
  attack.m_Staging = ACR_Manager.Ground(target.GetOrigin() + dir * ring);
  array<float> angles = {0, 55, -55, 90, -90};
  foreach (int i, ACR_Squad member : attack.m_Squads)
  {
   float angle = angles[i % angles.Count()] * Math.DEG2RAD;
   vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
   attack.m_aStaging.Insert(ACR_Manager.Ground(target.GetOrigin() + rotated * ring));
  }
  bool empty = target.m_sOwner.IsEmpty();
  // With two or more squads on a held base, the one on the direct approach
  // becomes the base of fire while the others manoeuvre.
  if (!empty && attack.m_Squads.Count() >= 2) attack.m_Suppressor = attack.m_Squads[0];
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + STAGING_TIMEOUT * 1000;
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.m_iAITask = ACR_EAITask.ATTACK;
   squad.m_AITarget = target;
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) squad.MoveTo(target.GetOrigin());
   else squad.MoveTo(attack.StagingOf(squad));
  }
  attack.m_bAssaulting = empty;
  m_Attacks.Insert(attack);
  string kind = "attacking";
  if (counterDue) { kind = "COUNTERATTACKING"; m_CounterTarget = null; }
  PrintFormat("[ACR] AI %1: %2 %3 with %4 squad(s), strength %5/%6", m_sFaction, kind, target.m_sName, attack.m_Squads.Count(), strength, required);
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

 protected ACR_Base NearestOwnBase(vector point)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 // A beaten squad breaks contact and falls back to the nearest own base.
 protected void FallBack(ACR_Squad squad, string why)
 {
  ACR_Base refuge = NearestOwnBase(PositionOf(squad));
  squad.m_iAITask = ACR_EAITask.FREE;
  squad.SetMode(ACR_EMoveMode.FAST);
  if (refuge) squad.Defend(refuge.GetOrigin());
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

 protected void UpdateAttacks()
 {
  for (int i = m_Attacks.Count() - 1; i >= 0; i--)
  {
   ACR_AIAttack attack = m_Attacks[i];
   for (int s = attack.m_Squads.Count() - 1; s >= 0; s--)
   {
    ACR_Squad squad = attack.m_Squads[s];
    if (!squad || !m_Manager.m_Squads.Contains(squad) || squad.HasPlayer()) { attack.Remove(s); continue; }
    // Badly hurt and still under fire: pull out instead of dying in place.
    if (attack.m_bAssaulting && squad.StrengthRatio() < RETREAT_RATIO && squad.KnownEnemies(300) > 0)
    {
     FallBack(squad, "heavy casualties");
     attack.Remove(s);
    }
   }
   ACR_Base target = attack.m_Target;
   bool won = target && target.m_sOwner == m_sFaction;
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
   if (!target || won || attack.m_Squads.IsEmpty() || broken)
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (broken) outcome = "called off (too many losses)";
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
   bool gathered = true;
   foreach (ACR_Squad squad : attack.m_Squads)
    if (vector.DistanceXZ(PositionOf(squad), attack.StagingOf(squad)) > 120) gathered = false;
   if (gathered || GetGame().GetWorld().GetWorldTime() > attack.m_fStageDeadline)
   {
    attack.m_bAssaulting = true;
    // Preparation: shell the objective, screen the approach with smoke.
    if (!CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, target.GetOrigin(), "preparation fire on " + target.m_sName))
     CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, target.GetOrigin(), "preparation fire on " + target.m_sName);
    vector screen = attack.m_Staging + (target.GetOrigin() - attack.m_Staging) * 0.55;
    CallSupport(ACR_ESupportType.MORTAR_SMOKE, screen, "smoke screen for " + target.m_sName);
    if (attack.m_bCounter) CallSupport(ACR_ESupportType.HELI_LOITER, target.GetOrigin(), "gunship for the counterattack on " + target.m_sName);
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
   ACR_Base nearest;
   float best = float.MAX;
   foreach (ACR_Base base : front)
   {
    float d = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
    if (d < best) { best = d; nearest = base; }
   }
   squad.m_iAITask = ACR_EAITask.POSTED;
   if (!nearest || best < 150) continue;
   squad.SetMode(ACR_EMoveMode.FAST);
   squad.Defend(nearest.GetOrigin());
   PrintFormat("[ACR] AI %1: %2 posted to %3", m_sFaction, squad.GetName(), nearest.m_sName);
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
  if (mine.Count() >= cap) return;
  // Deploy at the own spawn base furthest from the fighting.
  ACR_Base spawn;
  float best = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !base.m_bAllowSpawn || base.m_bContested) continue;
   // distance to the nearest non-owned base = how far behind the lines it is
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (ThreatTo(base) > 0) nearestFront = 0;
   if (nearestFront > best) { best = nearestFront; spawn = base; }
  }
  if (!spawn) return;
  // Buy up to two squads per tick from what's affordable, favouring infantry.
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  for (int n = 0; n < 2 && mine.Count() + n < cap; n++)
  {
   array<int> options = {};
   foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
   {
    if (entry.m_iRemaining <= 0 || entry.m_iCost > data.m_iRP) continue;
    // AI can't fly: a bought helicopter would just sit at base. Gunships come as supports instead.
    if (entry.m_bAircraft || entry.m_sVehicle.Contains("/Helicopters/") || entry.m_sVehicle.Contains("/Planes/")) continue;
    // Keep infantry the backbone: three chances in four for infantry-only entries.
    if (!entry.m_sVehicle.IsEmpty() && Math.RandomFloat(0, 1) < 0.75) continue;
    options.Insert(i);
   }
   if (options.IsEmpty()) return;
   string result = m_Manager.PurchaseAt(m_sFaction, options.GetRandomElement(), spawn);
   PrintFormat("[ACR] AI %1 requisition: %2", m_sFaction, result);
  }
 }

 // ---------------------------------------------------------------- hand-over

 // A player took command: leave every squad as it is, but forget AI tasks.
 protected void ReleaseAll()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction) squad.m_iAITask = ACR_EAITask.FREE;
  m_Attacks.Clear();
 }
}
