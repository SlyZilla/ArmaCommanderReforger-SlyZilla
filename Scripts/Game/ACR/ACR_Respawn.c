// Command-unit respawn (server).
//  - Killed in your command unit: the normal (auto) respawn happens after a delay, at
//    the owned base closest to your side's placed spawn point (your HQ while you
//    hold it), otherwise at the spawn point itself.
//  - Command unit killed while you control another soldier: you stay in that
//    soldier; after the delay a new command unit appears at that base and F8
//    takes you there.
//  - A possessed soldier dies: you go back to your command unit (or wait for the
//    new one), never into the Game Master editor.
class ACR_Respawn
{
 protected ACR_Manager m_Manager;
 protected ref map<int, float> m_DueAt = new map<int, float>();         // player -> world time the new unit is ready
 protected ref map<int, bool> m_ReturnWhenReady = new map<int, bool>();
 protected ref map<IEntity, vector> m_SpawnPointOrigins = new map<IEntity, vector>();

 void ACR_Respawn(ACR_Manager manager) { m_Manager = manager; }

 int DelayMs()
 {
  int seconds = m_Manager.m_iRespawnSeconds;
  if (seconds < 0) seconds = 0;
  return seconds * 1000;
 }

 // Where this faction's placed spawn point was at mission start (its HQ).
 protected ref map<string, vector> m_Home = new map<string, vector>();
 protected vector HomeOf(string factionKey, out bool found)
 {
  vector home;
  found = m_Home.Find(factionKey, home);
  if (found) return home;
  array<SCR_SpawnPoint> points = SCR_SpawnPoint.GetSpawnPointsForFaction(factionKey);
  if (points.IsEmpty()) return vector.Zero;
  home = points[0].GetOrigin();
  m_Home.Set(factionKey, home);
  found = true;
  return home;
 }

 // Spawn points withheld during a respawn delay: the native auto-spawn finds none,
 // retries every second, and succeeds once they are given back at the new base.
 protected ref map<SCR_SpawnPoint, string> m_Withheld = new map<SCR_SpawnPoint, string>();
 protected ref map<string, float> m_ReleaseAt = new map<string, float>();

 // Called just before the native kill handling of a player's command unit.
 void OnCommandUnitKilled(int playerId)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(playerId);
  if (!faction) return;
  string key = faction.GetFactionKey();
  bool found;
  HomeOf(key, found);
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(key))
  {
   m_Withheld.Set(point, key);
   point.SetFactionKey("");
  }
  m_ReleaseAt.Set(key, GetGame().GetWorld().GetWorldTime() + DelayMs());
  m_Manager.Notify(playerId, string.Format("Command unit killed. Respawning at %1 in %2 s.", BaseName(key), m_Manager.m_iRespawnSeconds));
  PrintFormat("[ACR] COMMAND UNIT KILLED player=%1, respawn in %2 s", playerId, m_Manager.m_iRespawnSeconds);
 }

 protected void ReleaseSpawnPoints(float now)
 {
  array<string> released = {};
  foreach (string key, float at : m_ReleaseAt)
  {
   if (now < at) continue;
   released.Insert(key);
   vector position = RespawnPosition(key);
   foreach (SCR_SpawnPoint point, string owner : m_Withheld)
   {
    if (owner != key || !point) continue;
    point.SetOrigin(position);
    point.SetFactionKey(key);
   }
   PrintFormat("[ACR] RESPAWN %1 at %2 %3", key, BaseName(key), position);
  }
  foreach (string key : released)
  {
   m_ReleaseAt.Remove(key);
   array<SCR_SpawnPoint> done = {};
   foreach (SCR_SpawnPoint point, string owner : m_Withheld) if (owner == key) done.Insert(point);
   foreach (SCR_SpawnPoint point : done) m_Withheld.Remove(point);
  }
 }

 ACR_Base ChooseBase(string factionKey)
 {
  bool found;
  vector home = HomeOf(factionKey, found);
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey || !base.m_bAllowSpawn || base.m_bContested) continue;
   float distance = 0;
   if (found) distance = vector.Distance(base.GetOrigin(), home);
   if (distance < bestDistance) { bestDistance = distance; best = base; }
  }
  return best;
 }

 string BaseName(string factionKey)
 {
  ACR_Base base = ChooseBase(factionKey);
  if (base) return base.m_sName;
  return "your start point";
 }

 protected vector RespawnPosition(string factionKey)
 {
  ACR_Base base = ChooseBase(factionKey);
  if (base) return m_Manager.ClearSpotNear(base, 4, 12, 1);
  bool found;
  vector home = HomeOf(factionKey, found);
  return ACR_Manager.Ground(home);
 }

 // A soldier the player controlled died.
 void OnPossessedKilled(SCR_PlayerController pc)
 {
  int id = pc.GetPlayerId();
  if (IsAlive(pc.GetMainEntity()))
  {
   m_Manager.ReleaseAuthority(pc);
   pc.SetPossessedEntity(null);
   m_Manager.Notify(id, "Soldier killed. Back in your command unit.");
   PrintFormat("[ACR] POSSESSED KILLED player=%1 -> command unit", id);
   return;
  }
  Schedule(id);
  m_ReturnWhenReady.Set(id, true);
  m_Manager.Notify(id, string.Format("Soldier killed. Your new command unit is ready in %1 s.", SecondsLeft(id)));
  PrintFormat("[ACR] POSSESSED KILLED player=%1 -> waiting for new command unit", id);
 }

 // F8 while the command unit is dead. Returns the message, or "" if not applicable.
 string OnReturnRequested(SCR_PlayerController pc)
 {
  if (!pc.IsPossessing() || IsAlive(pc.GetMainEntity())) return "";
  int id = pc.GetPlayerId();
  Schedule(id);
  m_ReturnWhenReady.Set(id, true);
  return string.Format("Your command unit is dead. You will switch to the new one in %1 s.", SecondsLeft(id));
 }

 static bool IsAlive(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  return character && character.GetCharacterController() && !character.GetCharacterController().IsDead();
 }

 protected void Schedule(int playerId)
 {
  if (m_DueAt.Contains(playerId)) return;
  m_DueAt.Set(playerId, GetGame().GetWorld().GetWorldTime() + DelayMs());
 }

 protected int SecondsLeft(int playerId)
 {
  float due;
  if (!m_DueAt.Find(playerId, due)) return 0;
  int left = Math.Ceil((due - GetGame().GetWorld().GetWorldTime()) / 1000);
  if (left < 0) left = 0;
  return left;
 }

 // Every second: command units that died while their player was elsewhere.
 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  ReleaseSpawnPoints(now);
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc || !pc.IsPossessing() || IsAlive(pc.GetMainEntity()))
   {
    m_DueAt.Remove(id);
    m_ReturnWhenReady.Remove(id);
    continue;
   }
   if (!m_DueAt.Contains(id))
   {
    Schedule(id);
    Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
    string where = "your start point";
    if (faction) where = BaseName(faction.GetFactionKey());
    m_Manager.Notify(id, string.Format("Your command unit was killed. A new one arrives at %1 in %2 s (F8 to switch when ready).", where, SecondsLeft(id)));
    PrintFormat("[ACR] COMMAND UNIT LOST player=%1 (possessing)", id);
    continue;
   }
   if (now < m_DueAt.Get(id)) continue;
   IEntity unit = CreateCommandUnit(pc);
   if (!unit) continue; // retry next second
   m_DueAt.Remove(id);
   bool goNow = m_ReturnWhenReady.Contains(id) || !IsAlive(pc.GetControlledEntity());
   m_ReturnWhenReady.Remove(id);
   if (goNow)
   {
    m_Manager.ReleaseAuthority(pc);
    pc.SetPossessedEntity(null);
    ACR_MapHandoff.ClearCharacterInput(pc.GetControlledEntity());
    m_Manager.Notify(id, "You are in your new command unit.");
   }
   else m_Manager.Notify(id, "New command unit ready. F8 to switch to it.");
  }
 }

 // A fresh command unit (the player's loadout) at the respawn base, set as the
 // player's main entity while they keep controlling their current soldier.
 protected IEntity CreateCommandUnit(SCR_PlayerController pc)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return null;
  ResourceName prefab;
  SCR_PlayerLoadoutComponent loadoutComponent = SCR_PlayerLoadoutComponent.Cast(pc.FindComponent(SCR_PlayerLoadoutComponent));
  if (loadoutComponent && loadoutComponent.GetLoadout()) prefab = loadoutComponent.GetLoadout().GetLoadoutResource();
  if (prefab.IsEmpty())
  {
   SCR_BasePlayerLoadout loadout = GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction);
   if (loadout) prefab = loadout.GetLoadoutResource();
  }
  if (prefab.IsEmpty()) return null;
  vector position = RespawnPosition(faction.GetFactionKey());
  IEntity unit = ACR_Manager.SpawnPrefab(prefab, position);
  if (!unit) return null;
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(unit.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFaction(faction);
  pc.ACR_SetMainEntity(unit);
  PrintFormat("[ACR] NEW COMMAND UNIT player=%1 at %2 %3", pc.GetPlayerId(), BaseName(faction.GetFactionKey()), position);
  return unit;
 }
}

// A possessed soldier dying returns the player to their command unit instead of
// opening the Game Master editor.
modded class SCR_PossessingManagerComponent
{
 override bool HandlePlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator instigator)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Respawn || playerId <= 0)
   return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  if (playerEntity == GetMainEntity(playerId) || !IsPossessing(playerId))
  {
   manager.m_Respawn.OnCommandUnitKilled(playerId);
   return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  }
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (!pc) return super.HandlePlayerKilled(playerId, playerEntity, killerEntity, instigator);
  manager.m_Respawn.OnPossessedKilled(pc);
  return false;
 }
}
