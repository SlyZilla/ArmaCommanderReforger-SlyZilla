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
 bool m_bByPlayer;     // seen by a player (not radioed: they saw it themselves)
 ACR_Squad m_Squad;    // the enemy squad (or null)
 IEntity m_Body;       // ...or one soldier of a group we don't manage
}

class ACR_Intel : Managed
{
 static const float MEMORY_MS = 90000;
 static const float SENTRY_RANGE = 60;
 static const float PLAYER_RANGE = 300;
 static const int FEED_LINES = 8;   // sightings, and in shared command orders, spending and pings
 static const float REPORT_AGAIN_MS = 120000; // same enemy group reported again only after this
 static const int KIND_INFANTRY = 0;
 static const int KIND_VEHICLE = 1;
 static const int KIND_ARMED = 2;
 static const int KIND_ARMOUR = 3;
 static const int KIND_AIR = 4;

 // Intentional gameplay abstraction (Jacob, 5 Oct): ownership of a base lets
 // its side report intruders without a living sentry or a line-of-sight check.
 static bool InsideBaseProximityReport(float distance, float baseRadius)
 {
  return distance < baseRadius + SENTRY_RANGE;
 }

 string m_sFaction;
 protected ACR_Manager m_Manager;
 ref map<IEntity, vector> m_SeenAt = new map<IEntity, vector>();
 ref map<IEntity, float> m_SeenTime = new map<IEntity, float>();
 protected ref map<IEntity, string> m_SeenBy = new map<IEntity, string>();
 protected ref map<IEntity, bool> m_SeenByPlayer = new map<IEntity, bool>();
 protected ref array<string> m_Radio = {};
 protected float m_fNextRadio;
 static const float RADIO_GAP_MS = 15000;   // at most one radio report per side this often
 static const int RADIO_QUEUE = 3;          // older unsent reports are dropped
 protected ref map<string, float> m_Reported = new map<string, float>();
 ref array<string> m_Feed = {};
 ref array<float> m_FeedTime = {};
 ref array<ref ACR_Contact> m_Contacts = {};
 protected int m_iTicks;
 // Our own empty vehicles that someone of ours can see (or saw), for the map.
 static const float VEHICLE_SIGHT = 400;          // m from any of our soldiers
 static const float VEHICLE_MEMORY_MS = 600000;   // last known spot kept 10 min
 ref map<IEntity, vector> m_VehicleAt = new map<IEntity, vector>();
 ref map<IEntity, float> m_VehicleSeen = new map<IEntity, float>();

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

 protected void Note(IEntity enemy, float now, string spotter, bool byPlayer = false)
 {
  if (!enemy || !ACR_Respawn.IsAlive(enemy)) return;
  m_SeenAt.Set(enemy, enemy.GetOrigin());
  m_SeenTime.Set(enemy, now);
  m_SeenBy.Set(enemy, spotter);
  m_SeenByPlayer.Set(enemy, byPlayer);
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
   string sentries = base.m_sName + " sentries";
   foreach (ACR_Squad intruders : m_Manager.m_Squads)
   {
    if (!intruders.GetFaction() || intruders.GetFaction().GetFactionKey() == m_sFaction) continue;
    array<AIAgent> agents = {};
    intruders.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (body && InsideBaseProximityReport(vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()), base.m_fRadius)) Note(body, now, sentries);
    }
   }
   foreach (int id : players)
   {
    IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    string playerSide = FactionKeyOf(player);
    if (player && !playerSide.IsEmpty() && playerSide != m_sFaction && InsideBaseProximityReport(vector.DistanceXZ(player.GetOrigin(), base.GetOrigin()), base.m_fRadius)) Note(player, now, sentries);
   }
  }
  // Debug menu: every enemy soldier counts as seen.
  if (m_Manager.m_Debug && m_Manager.m_Debug.Reveals(m_sFaction)) RevealAll(players, now);
  // 3. Our players' eyes (every 2 s): enemies in range with a clear line of sight.
  if (m_iTicks % 2 == 0) PlayerSightings(players, now);
  else TrackVehicles(players, now);
  // Forget the stale and the dead.
  array<IEntity> stale = {};
  foreach (IEntity enemy, float when : m_SeenTime)
   if (!enemy || now - when > MEMORY_MS || !ACR_Respawn.IsAlive(enemy)) stale.Insert(enemy);
  foreach (IEntity gone : stale)
  {
   m_SeenTime.Remove(gone);
   m_SeenAt.Remove(gone);
   m_SeenBy.Remove(gone);
   m_SeenByPlayer.Remove(gone);
  }
  BuildContacts(now);
  Report(now);
  if (m_iTicks % 60 == 0)
  {
   array<string> old = {};
   foreach (string key, float when : m_Reported) if (now - when > REPORT_AGAIN_MS) old.Insert(key);
   foreach (string done : old) m_Reported.Remove(done);
  }
 }

 protected void RevealAll(array<int> players, float now)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) Note(agent.GetControlledEntity(), now, "Debug");
  }
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   string side = FactionKeyOf(player);
   if (player && !side.IsEmpty() && side != m_sFaction) Note(player, now, "Debug");
  }
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
    Note(enemy, now, names[e], true);
    break;
   }
  }
 }

 // Our empty vehicles: known where one of our soldiers (or a base of ours) is
 // near enough to keep an eye on them; otherwise the last seen spot, for a while.
 protected void TrackVehicles(array<int> players, float now)
 {
  array<vector> eyes = {};
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body && ACR_Respawn.IsAlive(body)) eyes.Insert(body.GetOrigin());
   }
  }
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (player && ACR_Respawn.IsAlive(player) && FactionKeyOf(player) == m_sFaction) eyes.Insert(player.GetOrigin());
  }
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  foreach (IEntity vehicle : vehicles)
  {
   bool ours = ACR_Vehicles.FactionOf(vehicle) == m_sFaction && ACR_Vehicles.IsEmpty(vehicle) && ACR_Logistics.Health(vehicle) > 0;
   if (!ours)
   {
    m_VehicleAt.Remove(vehicle);
    m_VehicleSeen.Remove(vehicle);
    continue;
   }
   vector at = vehicle.GetOrigin();
   bool seen = false;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == m_sFaction && vector.DistanceXZ(at, base.GetOrigin()) < base.m_fRadius + 50) { seen = true; break; }
   if (!seen)
    foreach (vector eye : eyes)
     if (vector.DistanceXZ(at, eye) < VEHICLE_SIGHT) { seen = true; break; }
   if (seen)
   {
    m_VehicleAt.Set(vehicle, at);
    m_VehicleSeen.Set(vehicle, now);
   }
  }
  array<IEntity> stale = {};
  foreach (IEntity known, float when : m_VehicleSeen)
   if (!known || now - when > VEHICLE_MEMORY_MS) stale.Insert(known);
  foreach (IEntity gone : stale)
  {
   m_VehicleAt.Remove(gone);
   m_VehicleSeen.Remove(gone);
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
     contact.m_Squad = squad;
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
     contact.m_bByPlayer = m_SeenByPlayer.Get(body);
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
    other.m_Body = body;
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
    other.m_bByPlayer = m_SeenByPlayer.Get(body);
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
  return VehicleKind(vehicle);
 }

 static int VehicleKind(IEntity vehicle)
 {
  string name = "";
  if (vehicle.GetPrefabData()) name = vehicle.GetPrefabData().GetPrefabName();
  name.ToUpper();
  if (name.Contains("HELICOPTER") || name.Contains("MI8") || name.Contains("UH1")) return KIND_AIR;
  if (name.Contains("BTR") || name.Contains("BMP") || name.Contains("BRDM") || name.Contains("LAV") || name.Contains("T72") || name.Contains("M1A") || name.Contains("M2A") || name.Contains("TRACKED")) return KIND_ARMOUR;
  if (ACR_Squad.HasTurret(vehicle)) return KIND_ARMED;
  return KIND_VEHICLE;
 }

 // Map symbol for a kind (shared by enemy contacts and our vehicle crews).
 static int IconOf(int kind)
 {
  if (kind == KIND_AIR) return EMilitarySymbolIcon.ROTARY_WING;
  if (kind == KIND_ARMOUR) return EMilitarySymbolIcon.ARMOR;
  if (kind == KIND_ARMED) return EMilitarySymbolIcon.MOTORIZED | EMilitarySymbolIcon.MACHINEGUN;
  if (kind == KIND_VEHICLE) return EMilitarySymbolIcon.MOTORIZED;
  return EMilitarySymbolIcon.INFANTRY;
 }

 static int DimensionOf(int kind)
 {
  if (kind == KIND_AIR) return EMilitarySymbolDimension.AIR;
  return EMilitarySymbolDimension.LAND;
 }

 // Freshest sighting of an enemy squad (or a lone soldier).
 bool LastKnown(ACR_Squad squad, IEntity body, out vector position, out float when)
 {
  when = -1;
  if (squad)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity member = agent.GetControlledEntity();
    float seen;
    if (!member || !m_SeenTime.Find(member, seen) || seen <= when) continue;
    when = seen;
    position = m_SeenAt.Get(member);
   }
  }
  else if (body && m_SeenTime.Find(body, when)) position = m_SeenAt.Get(body);
  return when >= 0;
 }

 // The known enemy group nearest to a point (within radius).
 ACR_Contact ContactNear(vector point, float radius)
 {
  ACR_Contact best;
  float bestDistance = radius;
  foreach (ACR_Contact contact : m_Contacts)
  {
   float d = vector.DistanceXZ(contact.m_Pos, point);
   if (d < bestDistance) { bestDistance = d; best = contact; }
  }
  return best;
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
   if (!contact.m_bByPlayer) QueueRadio(string.Format("%1: contact, %2, %3.", spotter, Describe(contact.m_iKind, contact.m_iCount), Relative(contact.m_Pos)));
  }
 }

 // Radio-style position: "200 m E of Barracks", "at Barracks", or a grid.
 string Relative(vector pos)
 {
  ACR_Base nearest;
  float best = 2000;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.IsVisibleTo(m_sFaction)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), pos);
   if (d < best) { best = d; nearest = base; }
  }
  if (!nearest) return Place(pos);
  if (best < nearest.m_fRadius + 40) return "at " + nearest.m_sName;
  int metres = Math.Round(best / 50) * 50;
  vector dir = pos - nearest.GetOrigin();
  float angle = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG; // 0 = north, 90 = east
  if (angle < 0) angle += 360;
  int sector = Math.Round(angle / 45);
  if (sector >= 8) sector = 0;
  array<string> compass = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return string.Format("%1 m %2 of %3", metres, compass[sector], nearest.m_sName);
 }

 protected void QueueRadio(string line)
 {
  m_Radio.Insert(line);
  while (m_Radio.Count() > RADIO_QUEUE) m_Radio.RemoveOrdered(0);
 }

 // Next radio report to send now, or "" (rate-limited so chat never floods).
 string PopRadio(float now)
 {
  if (m_Radio.IsEmpty() || now < m_fNextRadio) return "";
  string line = m_Radio[0];
  m_Radio.RemoveOrdered(0);
  m_fNextRadio = now + RADIO_GAP_MS;
  return line;
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
