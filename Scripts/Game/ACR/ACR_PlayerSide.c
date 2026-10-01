// Host option: all players on one side (-acrPlayers US / USSR; "any" or absent
// = free choice as before). The server puts every joining player on that side
// and refuses requests to switch to another, so the deploy screen goes straight
// to spawning. Server only; clients never see the option.
class ACR_PlayerSide
{
 protected static bool s_bRead;
 protected static string s_sKey;

 static void Force(string key)
 {
  s_bRead = true;
  s_sKey = key;
 }

 // The forced faction key, or "" for free choice.
 static string Key()
 {
  if (s_bRead) return s_sKey;
  s_bRead = true;
  string value;
  if (!System.GetCLIParam("acrPlayers", value)) return s_sKey;
  value.Trim();
  string lower = value;
  lower.ToLower();
  if (lower == "" || lower == "any" || lower == "free") return s_sKey;
  FactionManager factions = GetGame().GetFactionManager();
  if (!factions) { s_bRead = false; return ""; }
  // Match the key regardless of case.
  array<Faction> all = {};
  factions.GetFactionsList(all);
  foreach (Faction faction : all)
  {
   string key = faction.GetFactionKey();
   string keyLower = key;
   keyLower.ToLower();
   if (keyLower == lower) s_sKey = key;
  }
  if (s_sKey == "") PrintFormat("[ACR] Host option -acrPlayers %1: no such faction, players choose freely", value);
  else PrintFormat("[ACR] Host option: all players join %1", s_sKey);
  return s_sKey;
 }

 // The scenario may only have a spawn point for one side (Arland: US at the airfield).
 // With players forced onto a side that has none, the game's auto-spawn retried every
 // second forever and the game never finished loading (1 Oct, -acrPlayers USSR).
 // Place one at that side's own base farthest from the other side's spawn points.
 static const ResourceName SPAWN_POINT = "{5A2220DD0C16958B}Prefabs/MP/Spawning/SpawnPoint_US.et";
 static bool s_bSpawnChecked; // (regress test resets it)
 static void EnsureSpawnPoint()
 {
  if (s_bSpawnChecked) return;
  string key = Key();
  if (key == "") { s_bSpawnChecked = true; return; }
  if (ACR_Base.s_All.IsEmpty()) return; // bases not registered yet
  s_bSpawnChecked = true;
  if (SCR_SpawnPoint.GetSpawnPointCountForFaction(key) > 0) return;
  ACR_Base best;
  float bestDistance = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != key || !base.m_bAllowSpawn) continue;
   float nearestEnemy = 1000000;
   foreach (SCR_SpawnPoint other : SCR_SpawnPoint.GetSpawnPoints())
    if (other && other.GetFactionKey() != key) nearestEnemy = Math.Min(nearestEnemy, vector.Distance(other.GetOrigin(), base.GetOrigin()));
   if (nearestEnemy > bestDistance) { bestDistance = nearestEnemy; best = base; }
  }
  if (!best) { PrintFormat("[ACR] No base of %1 to put a player spawn point at", key); return; }
  SCR_SpawnPoint point = SCR_SpawnPoint.Cast(ACR_Manager.SpawnPrefab(SPAWN_POINT, ACR_Manager.Ground(best.GetOrigin() + Vector(6, 0, 6))));
  if (point) point.SetFactionKey(key);
  PrintFormat("[ACR] Player spawn point for %1 placed at %2: %3", key, best.m_sName, point != null);
 }

 // Server tick: anyone not on the chosen side is moved to it (only while not
 // alive in the field, so nobody is yanked mid-fight).
 static void Enforce()
 {
  string key = Key();
  if (key == "") return;
  EnsureSpawnPoint();
  Faction target = GetGame().GetFactionManager().GetFactionByKey(key);
  if (!target) return;
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  foreach (int id : ids)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (!pc) continue;
   SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
   if (!affiliation) continue;
   // The faction manager's record is what everything else reads (command, map, RP).
   // Set straight after connecting, it was wiped when the player finished registering,
   // while the player's own component kept USSR: he spawned, but as nobody's
   // commander (1 Oct, hosting as USSR). Re-sync the record when they differ.
   if (affiliation.GetAffiliatedFaction() == target)
   {
    if (SCR_FactionManager.SGetPlayerFaction(id) != target)
    {
     SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
     if (factionManager) factionManager.UpdatePlayerFaction_S(affiliation);
     PrintFormat("[ACR] %1: faction record re-synced to %2 (now %3)", GetGame().GetPlayerManager().GetPlayerName(id), key, SCR_FactionManager.SGetPlayerFaction(id) == target);
    }
    continue;
   }
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (body && ACR_Respawn.IsAlive(body)) continue;
   if (affiliation.SetFaction_S(target)) PrintFormat("[ACR] %1 assigned to %2", GetGame().GetPlayerManager().GetPlayerName(id), key);
  }
 }
}

modded class SCR_PlayerFactionAffiliationComponent
{
 // For the test harness.
 bool ACR_WouldAllow(Faction faction)
 {
  return CanRequestFaction_S(faction);
 }

 override protected bool CanRequestFaction_S(Faction faction)
 {
  string key = ACR_PlayerSide.Key();
  if (key != "" && faction && faction.GetFactionKey() != key) return false;
  return super.CanRequestFaction_S(faction);
 }
}

// The scenario's auto-spawn names a fixed side (US on Arland): with a forced players'
// side, spawn players on that side instead (the faction request for US was refused).
// (The container attribute must be repeated on a modded config class, or the world's
// respawn component loads without its spawn logic.)
[BaseContainerProps(category: "Respawn")]
modded class SCR_AutoSpawnLogic
{
 override protected bool GetForcedFaction(out Faction faction)
 {
  string key = ACR_PlayerSide.Key();
  if (key != "")
  {
   faction = GetGame().GetFactionManager().GetFactionByKey(key);
   if (faction) return true;
  }
  return super.GetForcedFaction(faction);
 }

 // Players who left keep a queued auto-spawn retry; the game's own retry then threw a
 // null exception every second (no player controller any more).
 override protected void DoSpawn_S(int playerId)
 {
  if (!GetGame().GetPlayerManager().GetPlayerController(playerId)) return;
  super.DoSpawn_S(playerId);
 }

 // For the regression test.
 Faction ACR_ForcedFaction()
 {
  Faction faction;
  GetForcedFaction(faction);
  return faction;
 }
}
