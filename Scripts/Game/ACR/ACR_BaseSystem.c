// Server-side bases, capture, garrisons, hidden-base discovery and requisition
// income. Numbers follow the Arma 3 mod (defines/commonDefines.inc, economy.sqf).
class ACR_FactionData
{
 int m_iRP;
 ref array<ref ACR_RequisitionEntry> m_aRequisitions = {};
 ref array<ResourceName> m_aGarrison = {};
 ref array<ref ACR_SupportEntry> m_aSupports = {};
}

class ACR_BaseSystem : Managed
{
 static const float CAPTURE_RATE = 0.4;     // score per second per soldier of difference
 static const float CAPTURE_MAX_STEP = 6;   // cap per second (original mod: 2; raised so more troops cap faster)
 static const float SPAWN_RANGE = 400;      // enemies this close deploy the garrison
 static const float DESPAWN_RANGE = 450;    // no enemies this close: garrison packs up
 static const int POINTS_CAP = 1000;

 int m_iIncomeInterval = 60;                // seconds
 int m_iBasicIncome = 5;
 int m_iStartingRP = 50;
 protected float m_fNextIncome = -1;

 static const ResourceName GROUP_BASE = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
 protected ref map<string, ref ACR_FactionData> m_Factions = new map<string, ref ACR_FactionData>();
 ref array<SCR_AIGroup> m_GarrisonGroups = {};

 // per-tick census
 protected ref array<IEntity> m_Bodies = {};
 protected ref array<string> m_BodyFactions = {};

 // Buy list, garrison soldiers and RP for a faction, built from its
 // ACR_FactionSettings entity (or catalog defaults) the first time it's needed.
 ACR_FactionData GetFaction(string factionKey)
 {
  ACR_FactionData data;
  if (m_Factions.Find(factionKey, data)) return data;
  data = new ACR_FactionData();
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  data.m_iRP = m_iStartingRP;
  if (settings) data.m_iRP = settings.m_iStartingRP;
  if (ACR_Manager.StartRPOption() > 0) data.m_iRP = ACR_Manager.StartRPOption();
  ACR_FactionCatalog.BuildRequisitions(factionKey, settings, data.m_aRequisitions);
  ACR_FactionCatalog.BuildGarrisonPool(factionKey, settings, data.m_aGarrison);
  if (!settings || settings.m_bSupports)
  {
   if (settings && settings.m_aSupports && !settings.m_aSupports.IsEmpty())
   {
    foreach (ACR_SupportEntry configured : settings.m_aSupports)
    {
     configured.Resolve();
     data.m_aSupports.Insert(configured);
    }
   }
   else ACR_Supports.BuildDefaults(data.m_aSupports);
  }
  m_Factions.Insert(factionKey, data);
  return data;
 }

 bool Spend(string factionKey, int amount)
 {
  ACR_FactionData data = GetFaction(factionKey);
  if (data.m_iRP < amount) return false;
  data.m_iRP -= amount;
  return true;
 }

 bool IsGarrison(SCR_AIGroup group)
 {
  return m_GarrisonGroups.Contains(group);
 }

 // ================================================================ economy

 int GetRP(string factionKey)
 {
  return GetFaction(factionKey).m_iRP;
 }

 int GetIncome(string factionKey)
 {
  int income = m_iBasicIncome;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == factionKey && base.m_bIncome) income += base.m_iValue;
  return income;
 }

 int SecondsToIncome()
 {
  if (m_fNextIncome < 0) return m_iIncomeInterval;
  return Math.Max(0, Math.Ceil((m_fNextIncome - GetGame().GetWorld().GetWorldTime()) / 1000));
 }

 protected void TickIncome(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fNextIncome < 0) m_fNextIncome = now + m_iIncomeInterval * 1000;
  if (now < m_fNextIncome) return;
  m_fNextIncome = now + m_iIncomeInterval * 1000;
  array<Faction> factions = {};
  GetGame().GetFactionManager().GetFactionsList(factions);
  foreach (Faction faction : factions)
  {
   string key = faction.GetFactionKey();
   int added = Math.Round(GetIncome(key) * ACR_Manager.IncomeOption());
   ACR_FactionData data = GetFaction(key);
   data.m_iRP = Math.Min(data.m_iRP + added, POINTS_CAP);
  }
  PrintFormat("[ACR] Income paid");
 }

 // ================================================================ bases

 void Tick(ACR_Manager manager)
 {
  CollectBodies();
  foreach (ACR_Base base : ACR_Base.s_All)
   UpdateBase(manager, base);
  TickIncome(manager);
 }

 // Every living character with a faction: AI agents plus player bodies.
 protected void CollectBodies()
 {
  m_Bodies.Clear();
  m_BodyFactions.Clear();
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents)
   AddBody(agent.GetControlledEntity());
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
   AddBody(GetGame().GetPlayerManager().GetPlayerControlledEntity(id));
 }

 protected void AddBody(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (!character || m_Bodies.Contains(character) || character.GetCharacterController().IsDead()) return;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return;
  m_Bodies.Insert(character);
  m_BodyFactions.Insert(affiliation.GetAffiliatedFaction().GetFactionKey());
 }

 protected void UpdateBase(ACR_Manager manager, ACR_Base base)
 {
  vector center = base.GetOrigin();
  map<string, int> inside = new map<string, int>();
  bool enemyNear, enemyInRange, enemyApproaching;
  foreach (int i, IEntity body : m_Bodies)
  {
   if (!body) continue; // deleted this tick (e.g. a garrison packed up after a capture)
   string key = m_BodyFactions[i];
   float distance = vector.DistanceXZ(body.GetOrigin(), center);
   if (distance <= base.m_fRadius)
   {
    int count;
    inside.Find(key, count);
    inside.Set(key, count + 1);
    if (!base.m_aDetectedBy.Contains(key))
    {
     base.m_aDetectedBy.Insert(key);
     if (base.m_bHidden && key != base.m_sOwner) manager.NotifyFaction(key, string.Format("Enemy base discovered: %1", base.m_sName));
    }
   }
   if (!base.m_sOwner.IsEmpty() && key != base.m_sOwner)
   {
    if (distance <= SPAWN_RANGE) enemyNear = true;
    if (distance <= SPAWN_RANGE + 300) enemyApproaching = true;
    if (distance <= DESPAWN_RANGE) enemyInRange = true;
   }
  }
  UpdateCapture(manager, base, inside);
  if (enemyApproaching && !base.m_GarrisonGroup && base.m_iGarrisonLeft > 0) ACR_Garrison.Prepare(center, GarrisonArea(base));
  UpdateGarrison(manager, base, enemyNear, enemyInRange);
 }

 // Buildings this far from a base's centre are garrisoned by its own soldiers.
 static float GarrisonArea(ACR_Base base)
 {
  return Math.Min(base.m_fRadius, 80);
 }

 // Strongest faction inside other than 'exclude'.
 protected string Strongest(map<string, int> inside, string exclude, out int best)
 {
  string winner;
  best = 0;
  foreach (string key, int count : inside)
  {
   if (key == exclude || count <= best) continue;
   best = count;
   winner = key;
  }
  return winner;
 }

 protected void UpdateCapture(ACR_Manager manager, ACR_Base base, map<string, int> inside)
 {
  int attackers;
  if (!base.m_sOwner.IsEmpty())
  {
   int defenders;
   inside.Find(base.m_sOwner, defenders);
   string attacker = Strongest(inside, base.m_sOwner, attackers);
   bool wasContested = base.m_bContested;
   base.m_bContested = attackers > 0;
   if (base.m_bContested && !wasContested)
    manager.NotifyFaction(base.m_sOwner, string.Format("%1 is under attack", base.m_sName));
   float change = 1; // secure again over time when no attackers are inside
   if (attackers > 0) change = Math.Clamp((defenders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
   base.m_fScore = Math.Clamp(base.m_fScore + change, -100, 100);
   if (base.m_fScore <= -100) Capture(manager, base, attacker);
   return;
  }
  // Neutral base: whoever holds it longest takes it.
  int holders;
  if (!base.m_sCapturer.IsEmpty()) inside.Find(base.m_sCapturer, holders);
  string challenger = Strongest(inside, base.m_sCapturer, attackers);
  if (holders == 0 && attackers > 0 && base.m_fScore <= 0)
  {
   base.m_sCapturer = challenger;
   holders = attackers;
   Strongest(inside, challenger, attackers);
  }
  base.m_bContested = holders > 0 && attackers > 0;
  float step = -1;
  if (holders > 0 || attackers > 0) step = Math.Clamp((holders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
  base.m_fScore = Math.Clamp(base.m_fScore + step, 0, 100);
  if (base.m_fScore <= 0) base.m_sCapturer = "";
  if (base.m_fScore >= 100 && !base.m_sCapturer.IsEmpty()) Capture(manager, base, base.m_sCapturer);
 }

 // Debug menu: hand a base over at once.
 void DebugCapture(ACR_Manager manager, ACR_Base base, string factionKey) { Capture(manager, base, factionKey); }

 protected void Capture(ACR_Manager manager, ACR_Base base, string factionKey)
 {
  string previous = base.m_sOwner;
  DespawnGarrison(base, false);
  base.m_sOwner = factionKey;
  base.m_sCapturer = "";
  base.m_fScore = 100;
  base.m_bContested = false;
  // Like the original: the new owner gets half the original garrison.
  base.m_iGarrisonLeft = Math.Ceil(base.m_iGarrison * 0.5);
  if (!base.m_aDetectedBy.Contains(factionKey)) base.m_aDetectedBy.Insert(factionKey);
  PrintFormat("[ACR] Base captured: %1 by %2 (was %3)", base.m_sName, factionKey, previous);
  manager.NotifyFaction(factionKey, string.Format("%1 captured", base.m_sName));
  if (!previous.IsEmpty()) manager.NotifyFaction(previous, string.Format("%1 lost", base.m_sName));
 }

 // ================================================================ garrisons

 protected void UpdateGarrison(ACR_Manager manager, ACR_Base base, bool enemyNear, bool enemyInRange)
 {
  if (base.m_GarrisonGroup)
  {
   ACR_Squad.KeepSimulated(base.m_GarrisonGroup);
   base.m_iGarrisonLeft = CountAlive(base.m_GarrisonGroup);
   if (!enemyInRange && !base.m_bContested) DespawnGarrison(base, true);
   return;
  }
  if (enemyNear && base.m_iGarrisonLeft > 0 && !base.m_sOwner.IsEmpty()) SpawnGarrison(base);
 }

 protected int CountAlive(SCR_AIGroup group)
 {
  int alive;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  return alive;
 }

 protected void SpawnGarrison(ACR_Base base)
 {
  array<ResourceName> pool = GetFaction(base.m_sOwner).m_aGarrison;
  if (pool.IsEmpty())
  {
   PrintFormat("[ACR] No garrison soldiers for faction %1", base.m_sOwner);
   base.m_iGarrisonLeft = 0;
   return;
  }
  vector center = base.GetOrigin();
  // The faction group prefab would otherwise spawn its own six default members on top of ours.
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(GROUP_BASE, center));
  SCR_AIGroup.IgnoreSpawning(false);
  if (group) group.SetMaxUnitsToSpawn(0);
  if (!group) return;
  m_GarrisonGroups.Insert(group); // before any soldier joins, so it is never registered as a squad
  group.SetDeleteWhenEmpty(false); // we delete it ourselves; otherwise it vanishes mid-fight and respawns
  group.SetCustomName(base.m_sName + " garrison", 0);
  float spread = Math.Min(base.m_fRadius * 0.6, 25);
  for (int i = 0; i < base.m_iGarrisonLeft; i++)
  {
   vector offset = Vector(Math.RandomFloat(-spread, spread), 0, Math.RandomFloat(-spread, spread));
   vector spot = ACR_Manager.Ground(center + offset);
   vector free;
   if (SCR_WorldTools.FindEmptyTerrainPosition(free, spot, 5, 0.6, 2)) spot = ACR_Manager.Ground(free);
   IEntity soldier = ACR_Manager.SpawnPrefab(pool.GetRandomElement(), spot);
   if (!soldier) continue;
   if (!group.AddAIEntityToGroup(soldier)) PrintFormat("[ACR] Garrison soldier %1 could not join its group", i);
   FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(soldier.FindComponent(FactionAffiliationComponent));
   if (affiliation) affiliation.SetAffiliatedFactionByKey(base.m_sOwner);
  }
  ACR_Squad.KeepSimulated(group);
  base.m_GarrisonGroup = group;
  // The base's buildings get posts at their windows; the soldiers start on them (the
  // game's fast init) and the rest hold the base round its centre as before.
  IEntity navAgent = ACR_Garrison.AnyAgent();
  int posts = ACR_Garrison.AssignArea(group, navAgent, center, GarrisonArea(base), base.m_iGarrisonLeft);
  ResourceName order = ACR_Squad.WP_DEFEND;
  if (posts > 0) order = ACR_Squad.WP_GARRISON;
  AIWaypoint defend = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(order), null, SpawnParams(center)));
  SCR_DefendWaypoint garrison = SCR_DefendWaypoint.Cast(defend);
  if (garrison && posts > 0) garrison.SetFastInit(true);
  if (defend)
  {
   defend.SetCompletionRadius(base.m_fRadius);
   group.AddWaypoint(defend);
  }
  PrintFormat("[ACR] Garrison deployed at %1: %2 soldiers, %3 in group", base.m_sName, base.m_iGarrisonLeft, group.GetAgentsCount());
 }

 protected EntitySpawnParams SpawnParams(vector position)
 {
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = position;
  return params;
 }

 // keepSurvivors: pack up (survivors return to the reserve) vs wiped on capture.
 protected void DespawnGarrison(ACR_Base base, bool keepSurvivors)
 {
  SCR_AIGroup group = base.m_GarrisonGroup;
  if (!group) return;
  if (keepSurvivors) base.m_iGarrisonLeft = CountAlive(group);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   // Never delete a soldier a player is controlling.
   if (soldier && !ACR_Squad.FindController(soldier)) SCR_EntityHelper.DeleteEntityAndChildren(soldier);
  }
  m_GarrisonGroups.RemoveItem(group);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
  base.m_GarrisonGroup = null;
  PrintFormat("[ACR] Garrison packed up at %1: %2 in reserve", base.m_sName, base.m_iGarrisonLeft);
 }

 // One line for the command panel.
 string Describe(ACR_Base base)
 {
  string owner = base.m_sOwner;
  if (owner.IsEmpty()) owner = "Unclaimed";
  string text = string.Format("%1 | garrison %2", owner, base.m_iGarrisonLeft);
  if (base.m_bIncome) text += string.Format(" | +%1 RP", base.m_iValue);
  else text += " | no income";
  if (base.m_bContested) text += " | CONTESTED";
  if (base.m_sOwner.IsEmpty() && !base.m_sCapturer.IsEmpty())
   text += string.Format(" | %1 capturing %2/100", base.m_sCapturer, Math.Round(base.m_fScore));
  else if (!base.m_sOwner.IsEmpty() && base.m_fScore < 100)
   text += string.Format(" | held %1/100", Math.Round((base.m_fScore + 100) / 2));
  return text;
 }
}
