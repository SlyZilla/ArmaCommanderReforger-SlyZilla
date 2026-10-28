// Hand a FOB to the AI (FOB panel, "AI manages this FOB"): while a player commands the side,
// the AI commander's FOB logic (ACR_AICommander.ManageFobUpgrades: supply orders paid with the
// side's RP, keeping its combat reserve, and upgrades in its build order, defences included)
// runs for the FOBs handed to it, and only for them. It never takes the side's squads to speed
// the building up. Off for every FOB until a commander turns it on; with the AI in command of
// the side it runs every FOB as before.
class ACR_FobAI
{
 // The FOB panel's toggle (server).
 static string Toggle(SCR_PlayerController pc, string label)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!Replication.IsServer() || !manager || !pc) return "Server unavailable";
  if (!manager.IsCommander(pc.GetPlayerId())) return "Take command to hand a FOB to the AI";
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  ACR_Base base = manager.m_Requisition.FindSpawnBase(faction.GetFactionKey(), label);
  if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply) return "Select one of your built FOBs";
  Set(base, !Handed(base), pc.GetPlayerId());
  if (Handed(base)) return base.m_sName + ": the AI manages it now (supplies, upgrades and defences)";
  return base.m_sName + ": back in your hands";
 }

 static void Set(ACR_Base base, bool on, int playerId = 0)
 {
  if (!base || Handed(base) == on) return;
  base.m_bAIManaged = on;
  base.m_sAIManagedFor = base.m_sOwner;
  PrintFormat("[ACR] FOB AI %1 (%2): %3 (player %4)", base.m_sName, base.m_sOwner, on, playerId);
 }

 // Handed to the AI by the side that holds it now (a captured FOB isn't the captor's).
 static bool Handed(ACR_Base base)
 {
  return base && base.m_bAIManaged && base.m_sAIManagedFor == base.m_sOwner && base.m_bBuilt && !base.m_bRetired;
 }

 // A FOB of this side is handed to the AI.
 static bool AnyFor(string faction)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (Handed(base) && base.m_sOwner == faction) return true;
  return false;
 }

 // The map feed's base flag: +2 handed to the AI (+1 contested).
 static int FeedFlag(ACR_Base base)
 {
  if (Handed(base)) return 2;
  return 0;
 }
}
