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

 // Server tick: anyone not on the chosen side is moved to it (only while not
 // alive in the field, so nobody is yanked mid-fight).
 static void Enforce()
 {
  string key = Key();
  if (key == "") return;
  Faction target = GetGame().GetFactionManager().GetFactionByKey(key);
  if (!target) return;
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  foreach (int id : ids)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (!pc) continue;
   SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
   if (!affiliation || affiliation.GetAffiliatedFaction() == target) continue;
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
