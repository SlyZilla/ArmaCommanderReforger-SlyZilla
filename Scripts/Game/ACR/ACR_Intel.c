// What one side knows about the enemy. Every second the server gathers the
// enemies its people can actually see: its AI squads' perception, sentries at
// its bases, and its players (anything within PLAYER_RANGE with a clear line
// of sight). Each sighting is kept at the last seen position for MEMORY_MS.
// The AI commander plans from this picture, and the side's players see it on
// the map as hostile markers plus a short text feed. No map hacks either way.
class ACR_Contact : Managed
{
 vector m_Pos;
 int m_iKind;          // ACR_Intel.KIND_*
 int m_iCount;         // soldiers seen
 float m_fSeen;        // world time of the newest sighting
 string m_sKey;        // squad or player, for the feed
 string m_sSpotter;
}

class ACR_Intel : Managed
{
 static const float MEMORY_MS = 90000;
 static const float SENTRY_RANGE = 60;
 static const float PLAYER_RANGE = 300;
 static const int FEED_LINES = 6;
 static const float REPORT_AGAIN_MS = 120000; // same enemy group reported again only after this
 static const int KIND_INFANTRY = 0;
 static const int KIND_VEHICLE = 1;
 static const int KIND_ARMED = 2;
 static const int KIND_ARMOUR = 3;
 static const int KIND_AIR = 4;

 string m_sFaction;
 protected ACR_Manager m_Manager;
 ref map<IEntity, vector> m_SeenAt = new map<IEntity, vector>();
 ref map<IEntity, float> m_SeenTime = new map<IEntity, float>();
 protected ref map<IEntity, string> m_SeenBy = new map<IEntity, string>();
 protected ref map<string, float> m_Reported = new map<string, float>();
 ref array<string> m_Feed = {};
 ref array<float> m_FeedTime = {};
 ref array<ref ACR_Contact> m_Contacts = {};
 protected int m_iTicks;

 void ACR_Intel(ACR_Manager manager, string faction)
 {
  m_Manager = manager;
  m_sFaction = faction;
 }

 static string FactionKeyOf(IEntity body)
 {
  if (!body) return "";
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return "";
  return affiliation.GetAffiliatedFaction().GetFactionKey();
 }

 protected void Note(IEntity enemy, float now, string spotter)
 {
  if (!enemy || !ACR_Respawn.IsAlive(enemy)) return;
  m_SeenAt.Set(enemy, enemy.GetOrigin());
  m_SeenTime.Set(enemy, now);
  m_SeenBy.Set(enemy, spotter);
 }

 // Called every second by the manager.
 void Tick()
 {
  m_iTicks++;
  float now = GetGame().GetWorld().GetWorldTime();
  // 1. Our AI squads' own perception.
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction || !squad.m_Group) continue;
   SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(squad.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
   if (!utility || !utility.m_Perception) continue;
   foreach (IEntity seen : utility.m_Perception.m_aTargetEntities)
   {
    string key = FactionKeyOf(seen);
    if (!key.IsEmpty() && key != m_sFaction) Note(seen, now, squad.GetName());
   }
  }
  // 2. Sentries: anyone inside (or right at) one of our bases.
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float reach = base.m_fRadius + SENTRY_RANGE;
   string sentries = base.m_sName + " sentries";
   foreach (ACR_Squad intruders : m_Manager.m_Squads)
   {
    if (!intruders.GetFaction() || intruders.GetFaction().GetFactionKey() == m_sFaction) continue;
    array<AIAgent> agents = {};
    intruders.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (body && vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) < reach) Note(body, now, sentries);
    }
   }
   foreach (int id : players)
   {
    IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    string playerSide = FactionKeyOf(player);
    if (player && !playerSide.IsEmpty() && playerSide != m_sFaction && vector.DistanceXZ(player.GetOrigin(), base.GetOrigin()) < reach) Note(player, now, sentries);
   }
  }
  // 3. Our players' eyes (every 2 s): enemies in range with a clear line of sight.
  if (m_iTicks % 2 == 0) PlayerSightings(players, now);
  // Forget the stale and the dead.
  array<IEntity> stale = {};
  foreach (IEntity enemy, float when : m_SeenTime)
   if (!enemy || now - when > MEMORY_MS || !ACR_Respawn.IsAlive(enemy)) stale.Insert(enemy);
  foreach (IEntity gone : stale)
  {
   m_SeenTime.Remove(gone);
   m_SeenAt.Remove(gone);
   m_SeenBy.Remove(gone);
  }
  BuildContacts(now);
  Report(now);
 }

 protected void PlayerSightings(array<int> players, float now)
 {
  array<IEntity> eyes = {};
  array<string> names = {};
  foreach (int id : players)
  {
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (!body || !ACR_Respawn.IsAlive(body) || FactionKeyOf(body) != m_sFaction) continue;
   eyes.Insert(body);
   names.Insert(GetGame().GetPlayerManager().GetPlayerName(id));
  }
  if (eyes.IsEmpty()) return;
  array<IEntity> enemies = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
    if (agent.GetControlledEntity()) enemies.Insert(agent.GetControlledEntity());
  }
  foreach (int id : players)
  {
   IEntity other = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   string otherSide = FactionKeyOf(other);
   if (other && !otherSide.IsEmpty() && otherSide != m_sFaction && !enemies.Contains(other)) enemies.Insert(other);
  }
  foreach (IEntity enemy : enemies)
  {
   if (!ACR_Respawn.IsAlive(enemy)) continue;
   foreach (int e, IEntity eye : eyes)
   {
    if (vector.Distance(eye.GetOrigin(), enemy.GetOrigin()) > PLAYER_RANGE) continue;
    if (!LineOfSight(eye, enemy)) continue;
    Note(enemy, now, names[e]);
    break;
   }
  }
 }

 protected static IEntity s_TraceIgnore;
 protected static IEntity s_TraceIgnoreVehicle;
 protected static IEntity s_TraceTarget;
 // Skip the looker, his vehicle and every other soldier (people don't block a view).
 protected static bool IgnoreOwn(notnull IEntity e)
 {
  IEntity root = e.GetRootParent();
  if (e == s_TraceIgnore || root == s_TraceIgnore) return false;
  if (s_TraceIgnoreVehicle && root == s_TraceIgnoreVehicle) return false;
  if (ChimeraCharacter.Cast(e) && e != s_TraceTarget) return false;
  return true;
 }

 // Eye to chest, stopped by terrain, buildings and vegetation.
 static bool LineOfSight(IEntity eye, IEntity target)
 {
  TraceParam trace = new TraceParam();
  trace.Start = eye.GetOrigin() + Vector(0, 1.6, 0);
  vector aim = target.GetOrigin() + Vector(0, 1.1, 0);
  trace.End = aim;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  s_TraceIgnore = eye;
  s_TraceIgnoreVehicle = ACR_Transport.VehicleOf(eye);
  s_TraceTarget = target;
  float fraction = GetGame().GetWorld().TraceMove(trace, IgnoreOwn);
  s_TraceIgnore = null;
  s_TraceIgnoreVehicle = null;
  s_TraceTarget = null;
  if (fraction >= 0.98) return true;
  IEntity hit = trace.TraceEnt;
  if (!hit) return false;
  IEntity root = hit.GetRootParent();
  return hit == target || root == target || (ACR_Transport.VehicleOf(target) && root == ACR_Transport.VehicleOf(target));
 }

 // One contact per enemy squad (newest sighting of any member), plus lone
 // enemy players who aren't in a squad.
 protected void BuildContacts(float now)
 {
  m_Contacts.Clear();
  array<IEntity> covered = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   ACR_Contact contact;
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    float when;
    if (!body || !m_SeenTime.Find(body, when)) continue;
    covered.Insert(body);
    if (!contact)
    {
     contact = new ACR_Contact();
     contact.m_sKey = "S" + squad.GetName();
     contact.m_fSeen = -1;
    }
    contact.m_iCount++;
    int kind = KindOf(body);
    if (kind > contact.m_iKind) contact.m_iKind = kind;
    if (when > contact.m_fSeen)
    {
     contact.m_fSeen = when;
     contact.m_Pos = m_SeenAt.Get(body);
     contact.m_sSpotter = m_SeenBy.Get(body);
    }
   }
   if (contact) m_Contacts.Insert(contact);
  }
  // Everyone else (garrisons, players): clustered by their AI group.
  map<AIGroup, ACR_Contact> byGroup = new map<AIGroup, ACR_Contact>();
  foreach (IEntity body, float when : m_SeenTime)
  {
   if (!body || covered.Contains(body)) continue;
   AIGroup group = GroupOf(body);
   ACR_Contact other;
   if (group) byGroup.Find(group, other);
   if (!other)
   {
    other = new ACR_Contact();
    other.m_sKey = "E" + body.GetID();
    if (group) other.m_sKey = "G" + group.GetID();
    other.m_fSeen = -1;
    m_Contacts.Insert(other);
    if (group) byGroup.Insert(group, other);
   }
   other.m_iCount++;
   int otherKind = KindOf(body);
   if (otherKind > other.m_iKind) other.m_iKind = otherKind;
   if (when > other.m_fSeen)
   {
    other.m_fSeen = when;
    other.m_Pos = m_SeenAt.Get(body);
    other.m_sSpotter = m_SeenBy.Get(body);
   }
  }
 }

 protected static AIGroup GroupOf(IEntity body)
 {
  AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
  if (!control || !control.GetControlAIAgent()) return null;
  return control.GetControlAIAgent().GetParentGroup();
 }

 static int KindOf(IEntity body)
 {
  IEntity vehicle = ACR_Transport.VehicleOf(body);
  if (!vehicle) return KIND_INFANTRY;
  string name = "";
  if (vehicle.GetPrefabData()) name = vehicle.GetPrefabData().GetPrefabName();
  name.ToUpper();
  if (name.Contains("HELICOPTER") || name.Contains("MI8") || name.Contains("UH1")) return KIND_AIR;
  if (name.Contains("BTR") || name.Contains("BMP") || name.Contains("BRDM") || name.Contains("LAV") || name.Contains("T72") || name.Contains("M1A") || name.Contains("M2A") || name.Contains("TRACKED")) return KIND_ARMOUR;
  if (ACR_Squad.HasTurret(vehicle)) return KIND_ARMED;
  return KIND_VEHICLE;
 }

 static string Describe(int kind, int count)
 {
  if (kind == KIND_AIR) return "helicopter";
  if (kind == KIND_ARMOUR) return "armour";
  if (kind == KIND_ARMED) return "armed vehicle";
  if (kind == KIND_VEHICLE) return "vehicle";
  if (count == 1) return "1 soldier";
  return count.ToString() + " infantry";
 }

 // Where, in words: the nearest base if close, otherwise a map grid.
 string Place(vector pos)
 {
  ACR_Base nearest;
  float best = 900;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d < best) { best = d; nearest = base; }
  }
  if (nearest && nearest.IsVisibleTo(m_sFaction)) return "near " + nearest.m_sName;
  int gx = pos[0] / 100;
  int gz = pos[2] / 100;
  return "at grid " + gx.ToString(3) + "-" + gz.ToString(3);
 }

 // New sightings go into the feed (each enemy group at most once per REPORT_AGAIN_MS).
 protected void Report(float now)
 {
  foreach (ACR_Contact contact : m_Contacts)
  {
   if (now - contact.m_fSeen > 1500) continue;   // only fresh sightings
   float last;
   if (m_Reported.Find(contact.m_sKey, last) && now - last < REPORT_AGAIN_MS) continue;
   m_Reported.Set(contact.m_sKey, now);
   string spotter = contact.m_sSpotter;
   if (spotter.IsEmpty()) spotter = "Contact";
   AddFeed(now, string.Format("%1: %2 %3", spotter, Describe(contact.m_iKind, contact.m_iCount), Place(contact.m_Pos)));
  }
 }

 void AddFeed(float now, string line)
 {
  m_Feed.InsertAt(line, 0);
  m_FeedTime.InsertAt(now, 0);
  while (m_Feed.Count() > FEED_LINES)
  {
   m_Feed.RemoveOrdered(m_Feed.Count() - 1);
   m_FeedTime.RemoveOrdered(m_FeedTime.Count() - 1);
  }
  PrintFormat("[ACR] INTEL %1: %2", m_sFaction, line);
 }
}
