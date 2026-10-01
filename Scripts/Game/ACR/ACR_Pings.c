// Pings (server): Look here / Attack this / Defend here / Support here, placed
// on the map or in the world by anyone on a side and shown to that side only.
// Each player keeps at most MAX_PER_PLAYER; the oldest goes first. A support
// call that lands on a Support ping answers it.
enum ACR_EPing
{
 LOOK,
 ATTACK,
 DEFEND,
 SUPPORT
}

class ACR_Ping : Managed
{
 int m_iId;
 int m_iKind;
 vector m_Pos;
 int m_iOwner;
 string m_sFaction;
 string m_sTag;
 float m_fPlaced;
 float m_fUntil;
 string m_sAnswer;   // "Answered by Jaco: Mortar HE" once a support call landed on it
}

class ACR_Pings : Managed
{
 static const int MAX_PER_PLAYER = 3;
 static const float LOOK_MS = 15000;
 static const float ORDER_MS = 60000;      // attack, defend, support
 static const float ANSWERED_MS = 8000;    // an answered support ping stays this long
 static const float MIN_GAP_MS = 400;      // per player, against spam
 static const float ANSWER_RADIUS = 200;   // m: a support call this close answers a support ping
 static const float WORLD_REACH = 1500;    // m: farthest a ping from the 3D view may be

 protected ACR_Manager m_Manager;
 ref array<ref ACR_Ping> m_Pings = {};
 protected int m_iNextId = 1;
 int m_iSerial;   // bumped on every change (the map feed resends then)
 protected ref map<int, float> m_LastPlaced = new map<int, float>();

 void ACR_Pings(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 static string KindName(int kind)
 {
  if (kind == ACR_EPing.ATTACK) return "Attack here";
  if (kind == ACR_EPing.DEFEND) return "Defend here";
  if (kind == ACR_EPing.SUPPORT) return "Support here";
  return "Look here";
 }

 static float Lifetime(int kind)
 {
  if (kind == ACR_EPing.LOOK) return LOOK_MS;
  return ORDER_MS;
 }

 // Place a ping. fromWorld: aimed in the 3D view (checked against reach).
 string Place(int playerId, int kind, vector pos, bool fromWorld, string tag)
 {
  if (kind < ACR_EPing.LOOK || kind > ACR_EPing.SUPPORT) return "Unknown ping";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
  if (!faction) return "You have no side";
  IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
  if (fromWorld && body && vector.Distance(body.GetOrigin(), pos) > WORLD_REACH + 50) return "Too far to ping";
  if (!ACR_Manager.OnMap(pos)) return "Not on the map";
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (m_LastPlaced.Find(playerId, last) && now - last < MIN_GAP_MS) return "";
  m_LastPlaced.Set(playerId, now);
  Add(faction.GetFactionKey(), playerId, kind, pos, tag);
  return "";
 }

 // Adds a ping (checks done). Public for the tests.
 ACR_Ping Add(string factionKey, int playerId, int kind, vector pos, string tag)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // At most MAX_PER_PLAYER: the oldest of this player's goes.
  array<int> mine = {};
  foreach (int i, ACR_Ping existing : m_Pings) if (existing.m_iOwner == playerId) mine.Insert(i);
  while (mine.Count() >= MAX_PER_PLAYER)
  {
   m_Pings.RemoveOrdered(mine[0]);
   mine.Clear();
   foreach (int j, ACR_Ping again : m_Pings) if (again.m_iOwner == playerId) mine.Insert(j);
  }
  ACR_Ping ping = new ACR_Ping();
  ping.m_iId = m_iNextId++;
  ping.m_iKind = kind;
  pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  ping.m_Pos = pos;
  ping.m_iOwner = playerId;
  ping.m_sFaction = factionKey;
  ping.m_sTag = tag;
  ping.m_fPlaced = now;
  ping.m_fUntil = now + Lifetime(kind);
  m_Pings.Insert(ping);
  m_iSerial++;
  ACR_Intel intel = m_Manager.IntelFor(ping.m_sFaction);
  string place = "";
  if (intel) place = " (" + intel.Place(pos) + ")";
  if (intel) intel.AddFeed(now, string.Format("%1: %2%3", tag, KindName(kind), place));
  PrintFormat("[ACR] PING %1 %2 by %3 at %4", ping.m_sFaction, KindName(kind), tag, pos);
  return ping;
 }

 // A player clears one of his own pings.
 string Remove(int playerId, int pingId)
 {
  foreach (int i, ACR_Ping ping : m_Pings)
  {
   if (ping.m_iId != pingId) continue;
   if (ping.m_iOwner != playerId) return "That is someone else's ping";
   m_Pings.RemoveOrdered(i);
   m_iSerial++;
   return "";
  }
  return "";
 }

 // A support call landed: the nearest open Support ping of that side is answered.
 void OnSupportCalled(string factionKey, vector target, string by, string support)
 {
  ACR_Ping best;
  float bestDistance = ANSWER_RADIUS;
  foreach (ACR_Ping ping : m_Pings)
  {
   if (ping.m_sFaction != factionKey || ping.m_iKind != ACR_EPing.SUPPORT || !ping.m_sAnswer.IsEmpty()) continue;
   float d = vector.DistanceXZ(ping.m_Pos, target);
   if (d < bestDistance) { bestDistance = d; best = ping; }
  }
  if (!best) return;
  float now = GetGame().GetWorld().GetWorldTime();
  best.m_sAnswer = string.Format("Answered by %1: %2", by, support);
  best.m_fUntil = now + ANSWERED_MS;
  m_iSerial++;
  ACR_Intel intel = m_Manager.IntelFor(factionKey);
  if (intel) intel.AddFeed(now, string.Format("%1's support ping: %2", best.m_sTag, best.m_sAnswer));
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_Pings.Count() - 1; i >= 0; i--)
  {
   if (now < m_Pings[i].m_fUntil) continue;
   m_Pings.RemoveOrdered(i);
   m_iSerial++;
  }
 }

 void Of(string factionKey, notnull array<ACR_Ping> result)
 {
  result.Clear();
  foreach (ACR_Ping ping : m_Pings) if (ping.m_sFaction == factionKey) result.Insert(ping);
 }
}
