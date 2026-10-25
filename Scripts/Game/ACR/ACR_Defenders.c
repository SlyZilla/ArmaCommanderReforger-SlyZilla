// Defenders for built positions (Fortify tier II and III, ACR_Fortify.Defenders): soldiers from
// the side's garrison pool held in reserve at the position, like a base's garrison
// (ACR_BaseSystem). They deploy when an enemy comes within SPAWN_RANGE and defend the position,
// pack up (survivors back to the reserve) once no enemy is within DESPAWN_RANGE, and are used up
// as they die. A position that is gone keeps the defenders who are out until they can pack up;
// its reserve goes with it. Their group is never a squad.
class ACR_DefenderPost : Managed
{
 string m_sFaction;
 vector m_vPos;
 string m_sName;
 float m_fRadius;      // their Defend round the position
 int m_iReserve;       // soldiers not out (used up as they die)
 SCR_AIGroup m_Group;  // out now
 bool m_bGone;         // its position is gone
}

class ACR_Defenders
{
 static ref array<ref ACR_DefenderPost> s_Posts = {};
 static const float SPAWN_RANGE = 400;   // as ACR_BaseSystem's garrisons
 static const float DESPAWN_RANGE = 450;

 static ACR_DefenderPost Add(string faction, vector pos, string name, float radius)
 {
  ACR_DefenderPost post = new ACR_DefenderPost();
  post.m_sFaction = faction;
  post.m_vPos = pos;
  post.m_sName = name;
  post.m_fRadius = radius;
  s_Posts.Insert(post);
  return post;
 }

 // Its position is gone: no more from the reserve.
 static void Remove(ACR_DefenderPost post)
 {
  if (!post) return;
  post.m_bGone = true;
  post.m_iReserve = 0;
 }

 // From ACR_Fortify.Tick every 5 s.
 static void Tick()
 {
  if (s_Posts.IsEmpty()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  for (int i = s_Posts.Count() - 1; i >= 0; i--)
  {
   ACR_DefenderPost post = s_Posts[i];
   if (post.m_Group)
   {
    ACR_Squad.KeepSimulated(post.m_Group);
    if (manager.m_Bases.GuardsAlive(post.m_Group) > 0 && EnemyWithin(manager, post.m_sFaction, post.m_vPos, DESPAWN_RANGE)) continue;
    int back = manager.m_Bases.PackUpGuards(post.m_Group);
    post.m_Group = null;
    if (!post.m_bGone) post.m_iReserve += back;
    PrintFormat("[ACR] DEFENDERS %1 packed up: %2 came back, %3 in reserve", post.m_sName, back, post.m_iReserve);
   }
   if (post.m_bGone)
   {
    s_Posts.RemoveOrdered(i);
    continue;
   }
   if (post.m_iReserve <= 0 || !EnemyWithin(manager, post.m_sFaction, post.m_vPos, SPAWN_RANGE)) continue;
   post.m_Group = manager.m_Bases.SpawnGuards(post.m_sFaction, post.m_vPos, post.m_iReserve, post.m_sName + " defenders", post.m_fRadius);
   if (!post.m_Group) continue;
   PrintFormat("[ACR] DEFENDERS %1 deployed: %2 soldiers", post.m_sName, post.m_iReserve);
   post.m_iReserve = 0;
  }
 }

 // A living enemy soldier (a squad's or a player) within range of pos.
 static bool EnemyWithin(ACR_Manager manager, string faction, vector pos, float range)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == faction) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), pos) > range + 300) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body && ACR_Respawn.IsAlive(body) && vector.DistanceXZ(body.GetOrigin(), pos) <= range) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (!player || !ACR_Respawn.IsAlive(player)) continue;
   string key = ACR_Intel.FactionKeyOf(player);
   if (!key.IsEmpty() && key != faction && vector.DistanceXZ(player.GetOrigin(), pos) <= range) return true;
  }
  return false;
 }

 static void Reset()
 {
  s_Posts.Clear();
 }
}
