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

// Selects the two playable sides before the skirmish layout is applied. The world
// is authored with US/USSR owners; these map to the selected factions at runtime.
class ACR_FactionSelection
{
 protected static bool s_bApplied;
 protected static bool s_bWorldMapped;
 protected static bool s_bUAChecked;
 protected static bool s_bUAAvailable;
 protected static string s_sWest = "US";
 protected static string s_sEast = "USSR";

 // This unique flag prefab is shipped by Ukraine Armed Forces RHS and does not
 // exist in vanilla or RHS Status Quo. Its presence detects the US -> UA override.
 protected static const ResourceName UA_PROBE = "{E50B55ED23B3EB27}Prefabs/Props/Fabric/Flags/Flag_UA_FA.et";

 static bool UkraineModLoaded()
 {
  if (!s_bUAChecked)
  {
   s_bUAChecked = true;
   Resource probe = Resource.Load(UA_PROBE);
   s_bUAAvailable = probe && probe.IsValid();
  }
  return s_bUAAvailable;
 }

 static string West() { Apply(); return s_sWest; }
 static string East() { Apply(); return s_sEast; }
 static bool IsSelected(string key) { Apply(); return key == s_sWest || key == s_sEast; }
 static string DisplayName(string key)
 {
  Apply();
  if (key == "RHS_AFRF") return "RHS Russia";
  if (key == "RHS_USAF") return "RHS US";
  if (key == "US" && UkraineModLoaded())
  {
   return "UA";
  }
  return key;
 }

 static bool HasFaction(string key)
 {
  return GetGame().GetFactionManager().GetFactionByKey(key) != null;
 }

 static void Apply()
 {
  if (!Replication.IsServer()) return;
  FactionManager manager = GetGame().GetFactionManager();
  if (!manager) return;
  string pair = "vanilla";
  if (!s_bApplied)
  {
   s_bApplied = true;
   System.GetCLIParam("acrFactions", pair);
   pair.ToLower();
   if (pair == "ru_us") { s_sWest = "RHS_USAF"; s_sEast = "RHS_AFRF"; }
   else if (pair == "ru_ua") { s_sWest = "US"; s_sEast = "RHS_AFRF"; }
   else if (pair == "us_ua") { s_sWest = "RHS_USAF"; s_sEast = "US"; }
   if ((pair == "ru_ua" || pair == "us_ua") && !UkraineModLoaded())
   {
    PrintFormat("[ACR] Faction pair %1 unavailable: Ukraine Armed Forces RHS not loaded", pair);
    s_sWest = "US";
    s_sEast = "USSR";
   }
   // Optional mods can be absent: keep the original playable sides in that case.
   if (!HasFaction(s_sWest) || !HasFaction(s_sEast))
   {
    PrintFormat("[ACR] Faction pair %1 unavailable; using vanilla US / USSR", pair);
    s_sWest = "US";
    s_sEast = "USSR";
   }
   array<Faction> all = {};
   manager.GetFactionsList(all);
   foreach (Faction baseFaction : all)
   {
    SCR_Faction faction = SCR_Faction.Cast(baseFaction);
    if (faction) faction.SetIsPlayable(faction.GetFactionKey() == s_sWest || faction.GetFactionKey() == s_sEast);
   }
   PrintFormat("[ACR] Playable faction pair: %1 / %2", s_sWest, s_sEast);
  }
  if (s_bWorldMapped || ACR_Base.s_All.IsEmpty()) return;
  s_bWorldMapped = true;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == "US") base.m_sOwner = s_sWest;
   else if (base.m_sOwner == "USSR") base.m_sOwner = s_sEast;
  }
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   if (point.GetFactionKey() == "US") point.SetFactionKey(s_sWest);
   else if (point.GetFactionKey() == "USSR") point.SetFactionKey(s_sEast);
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
