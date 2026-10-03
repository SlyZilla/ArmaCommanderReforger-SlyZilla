enum ACR_EDebug
{
 RP_OWN,          // +RP_STEP to your side
 RP_ENEMY,        // +RP_STEP to the other side
 PAUSE_ENEMY,     // toggle: the enemy AI commander stops deciding (squads keep their orders)
 ATTACK_NOW,      // the enemy AI commander attacks now (the selected base, else its pick)
 CAPTURE_OWN,     // the selected base becomes yours
 CAPTURE_ENEMY,   // ... the other side's
 REVEAL,          // toggle: your side's intel sees every enemy soldier
 RESET_SUPPORTS   // every support ready again (both sides)
}

// Debug menu (map panel, host and admins only): shortcuts for testing. Server side;
// ACR_PlayerController.ACR_DebugServer checks who asks, ACR_MapFeed sends the status.
class ACR_Debug : Managed
{
 static const int RP_STEP = 500;
 protected ref array<string> m_Paused = {};
 protected ref array<string> m_Reveal = {};

 bool IsPaused(string factionKey) { return m_Paused.Contains(factionKey); }
 bool Reveals(string factionKey) { return m_Reveal.Contains(factionKey); }

 static string EnemyOf(string factionKey)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
   if (settings.m_sFaction != factionKey) return settings.m_sFaction;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_sOwner.IsEmpty() && base.m_sOwner != factionKey) return base.m_sOwner;
  return "";
 }

 static ACR_Base FindBase(string baseName)
 {
  if (baseName.IsEmpty()) return null;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.GetLabel() == baseName || base.m_sName == baseName) return base; // the map sends labels
  return null;
 }

 // One line for the panel, e.g. "USSR AI: PAUSED   Reveal: off".
 string Status(ACR_Manager manager, string ownKey)
 {
  string enemy = EnemyOf(ownKey);
  string ai = "running";
  if (IsPaused(enemy)) ai = "PAUSED";
  else if (!manager.IsAIInCommand(enemy)) ai = "not in command";
  string reveal = "off";
  if (Reveals(ownKey)) reveal = "ON";
  return string.Format("%1 AI: %2   Reveal: %3", enemy, ai, reveal);
 }

 string Run(ACR_Manager manager, int action, string ownKey, string baseName)
 {
  string enemy = EnemyOf(ownKey);
  if (action == ACR_EDebug.RP_OWN) return GiveRP(manager, ownKey);
  if (action == ACR_EDebug.RP_ENEMY) return GiveRP(manager, enemy);
  if (action == ACR_EDebug.PAUSE_ENEMY) return TogglePause(enemy);
  if (action == ACR_EDebug.ATTACK_NOW) return AttackNow(manager, enemy, FindBase(baseName));
  if (action == ACR_EDebug.CAPTURE_OWN) return CaptureFor(manager, FindBase(baseName), ownKey);
  if (action == ACR_EDebug.CAPTURE_ENEMY) return CaptureFor(manager, FindBase(baseName), enemy);
  if (action == ACR_EDebug.REVEAL) return ToggleReveal(ownKey);
  if (action == ACR_EDebug.RESET_SUPPORTS) return ResetSupports(manager);
  return "Unknown debug action";
 }

 // Capped like income (ACR_BaseSystem.POINTS_CAP), or the next payout would cut it back.
 protected string GiveRP(ACR_Manager manager, string factionKey)
 {
  if (factionKey.IsEmpty()) return "No such side";
  ACR_FactionData data = manager.m_Bases.GetFaction(factionKey);
  data.m_iRP = Math.Min(data.m_iRP + RP_STEP, ACR_BaseSystem.POINTS_CAP);
  return string.Format("Debug: %1 now has %2 RP (cap %3)", factionKey, data.m_iRP, ACR_BaseSystem.POINTS_CAP);
 }

 protected string TogglePause(string factionKey)
 {
  if (factionKey.IsEmpty()) return "No enemy side";
  if (m_Paused.Contains(factionKey))
  {
   m_Paused.RemoveItem(factionKey);
   return string.Format("Debug: %1 AI commander resumed", factionKey);
  }
  m_Paused.Insert(factionKey);
  return string.Format("Debug: %1 AI commander paused (squads keep their current orders)", factionKey);
 }

 protected string AttackNow(ACR_Manager manager, string factionKey, ACR_Base target)
 {
  ACR_AICommander ai = manager.AICommanderFor(factionKey);
  if (!ai) return string.Format("%1 has no AI commander", factionKey);
  string result = "Debug: " + ai.ForceAttack(target);
  if (IsPaused(factionKey)) result += " (paused: it moves once you resume)";
  return result;
 }

 protected string CaptureFor(ACR_Manager manager, ACR_Base base, string factionKey)
 {
  if (!base) return "Debug: select a base on the map first";
  if (factionKey.IsEmpty()) return "No such side";
  if (base.m_sOwner == factionKey) return string.Format("%1 already holds %2", factionKey, base.m_sName);
  manager.m_Bases.DebugCapture(manager, base, factionKey);
  return string.Format("Debug: %1 now belongs to %2", base.m_sName, factionKey);
 }

 protected string ToggleReveal(string factionKey)
 {
  if (m_Reveal.Contains(factionKey))
  {
   m_Reveal.RemoveItem(factionKey);
   return "Debug: reveal off (sightings fade as usual)";
  }
  m_Reveal.Insert(factionKey);
  return string.Format("Debug: %1 intel now sees every enemy soldier", factionKey);
 }

 protected string ResetSupports(ACR_Manager manager)
 {
  int count;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   foreach (ACR_SupportEntry entry : manager.m_Bases.GetFaction(settings.m_sFaction).m_aSupports)
   {
    entry.m_fReadyAt = 0;
    count++;
   }
  }
  return string.Format("Debug: %1 support(s) ready again (both sides)", count);
 }
}
