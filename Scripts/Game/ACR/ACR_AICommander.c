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
 vector m_Staging;
 bool m_bAssaulting;
 float m_fStageDeadline;
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
 }

 // ---------------------------------------------------------------- 3. attacks

 protected void AssignAttacks()
 {
  ACR_FactionSettings settings = Settings();
  int maxAttacks = 1;
  if (settings) maxAttacks = settings.m_iAIAttacks;
  if (m_Attacks.Count() >= maxAttacks) return;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (free.IsEmpty()) return;

  // Pick the cheapest target: empty bases first, then enemy bases by
  // (defence x ratio + garrison) scaled by distance, like the original.
  ACR_Base target;
  float bestScore = float.MAX;
  vector from = PositionOf(free[0]);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || !Visible(base) || IsAttacking(base)) continue;
   float cost = Math.Max(10, DefenceOf(base) * ATTACK_RATIO);
   if (base.m_sOwner.IsEmpty()) cost = 1;
   float score = cost * (vector.DistanceXZ(base.GetOrigin(), from) / 1000 + 0.1);
   if (score < bestScore) { bestScore = score; target = base; }
  }
  if (!target) return;

  float required = 1;
  if (!target.m_sOwner.IsEmpty()) required = Math.Max(10, DefenceOf(target) * ATTACK_RATIO * 0.65) + target.m_iGarrisonLeft;
  SortByDistance(free, target.GetOrigin());
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
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
  // Staging point on the attackers' side of the target.
  vector dir = from - target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  attack.m_Staging = ACR_Manager.Ground(target.GetOrigin() + dir * (target.m_fRadius + STAGING_DISTANCE));
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + STAGING_TIMEOUT * 1000;
  bool empty = target.m_sOwner.IsEmpty();
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.m_iAITask = ACR_EAITask.ATTACK;
   squad.m_AITarget = target;
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) squad.MoveTo(target.GetOrigin());
   else squad.MoveTo(attack.m_Staging);
  }
  attack.m_bAssaulting = empty;
  m_Attacks.Insert(attack);
  PrintFormat("[ACR] AI %1: attacking %2 with %3 squad(s), strength %4/%5", m_sFaction, target.m_sName, attack.m_Squads.Count(), strength, required);
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
    if (!squad || !m_Manager.m_Squads.Contains(squad) || squad.HasPlayer()) attack.m_Squads.RemoveOrdered(s);
   }
   ACR_Base target = attack.m_Target;
   bool won = target && target.m_sOwner == m_sFaction;
   if (!target || won || attack.m_Squads.IsEmpty())
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (target) PrintFormat("[ACR] AI %1: attack on %2 %3", m_sFaction, target.m_sName, outcome);
    foreach (ACR_Squad squad : attack.m_Squads) squad.m_iAITask = ACR_EAITask.FREE;
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   if (attack.m_bAssaulting) continue;
   bool gathered = true;
   foreach (ACR_Squad squad : attack.m_Squads)
    if (vector.DistanceXZ(PositionOf(squad), attack.m_Staging) > 120) gathered = false;
   if (gathered || GetGame().GetWorld().GetWorldTime() > attack.m_fStageDeadline)
   {
    attack.m_bAssaulting = true;
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     squad.SetMode(ACR_EMoveMode.COMBAT);
     squad.MoveTo(target.GetOrigin());
    }
    PrintFormat("[ACR] AI %1: assaulting %2", m_sFaction, target.m_sName);
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
