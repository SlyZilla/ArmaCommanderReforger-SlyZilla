// -acrSkirmishTest 1 (server, Everon, ~40 s): skirmish layouts (ACR_Skirmish). Every layout in
// the world must name real bases and an HQ per side. The test plays the Saint-Philippe
// flashpoint (US keeps the Airbase and its pads, USSR moves to the Power Plant, which has no
// pad); -acrSkirmishSouthTest 1 plays the South half (USSR keeps the Harbor, US moves to
// Lamentin). Checks: bases in play, HQ owners, everything else neutral, starting units and
// spawn points at the new HQ, helipads out of play, and that the side without a pad has no
// support helicopters to buy and gets the off-map helicopter support instead.
class ACR_TestSkirmish
{
 static string s_sProblems = "";
 static int s_iLayouts;

 // Called by ACR_Skirmish.Choose in test runs, before the layout is applied.
 static string Layout()
 {
  s_iLayouts = 0;
  s_sProblems = "";
  foreach (ACR_Skirmish layout : ACR_Skirmish.s_All)
  {
   s_iLayouts++;
   array<string> names = {};
   layout.m_sBases.Split(",", names, true);
   foreach (string name : names) if (!ACR_Skirmish.FindBase(name)) s_sProblems += layout.m_sName + ": " + name + "; ";
   array<string> pairs = {};
   layout.m_sHQs.Split(";", pairs, true);
   int hqs;
   foreach (string pair : pairs)
   {
    array<string> parts = {};
    pair.Split("=", parts, false);
    if (parts.Count() == 2 && ACR_Skirmish.FindBase(parts[1])) hqs++;
    else s_sProblems += layout.m_sName + " HQ: " + pair + "; ";
   }
   if (hqs < 2) s_sProblems += layout.m_sName + ": " + hqs.ToString() + " HQs; ";
  }
  if (System.IsCLIParam("acrSkirmishSouthTest")) return "South half";
  if (System.IsCLIParam("acrSkirmishPlayTest") || System.IsCLIParam("acrPadsMontignacTest")) return "Montignac";
  if (System.IsCLIParam("acrPadsLevieTest")) return "Levie";
  return "Saint-Philippe";
 }

 static bool South() { return System.IsCLIParam("acrSkirmishSouthTest"); }

 // Test runs that pick a layout (the pad tests run ACR_TestPads on a layout's HQs).
 static bool Wanted()
 {
  return System.IsCLIParam("acrSkirmishTest") || System.IsCLIParam("acrSkirmishSouthTest") || System.IsCLIParam("acrSkirmishPlayTest")
   || System.IsCLIParam("acrPadsLevieTest") || System.IsCLIParam("acrPadsMontignacTest") || System.IsCLIParam("acrPadsPhilippeTest");
 }

 static void Run(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (tick != 12 && tick != 30) return;
  string moved = "USSR";
  string kept = "US";
  string movedHQ = "Power Plant";
  string keptHQ = "Airbase";
  int expected = 8;
  if (South()) { moved = "US"; kept = "USSR"; movedHQ = "Lamentin"; keptHQ = "Harbor"; expected = 30; }
  if (tick == 12)
  {
   ACR_TestRegress.Check("skirmish: every layout names real bases and two HQs", s_iLayouts >= 5 && s_sProblems == "", string.Format("%1 layouts | %2", s_iLayouts, s_sProblems));
   ACR_Skirmish activeLayout = ACR_Skirmish.s_Active;
   string activeName = "none";
   if (activeLayout) activeName = activeLayout.m_sName;
   ACR_TestRegress.Check("skirmish: layout applied", activeLayout != null, activeName);
   ACR_TestRegress.Check("skirmish: only the layout's bases are in play", ACR_Base.s_All.Count() == expected, string.Format("%1 bases, expected %2", ACR_Base.s_All.Count(), expected));
   ACR_Base movedBase = ACR_Skirmish.FindBase(movedHQ);
   ACR_Base keptBase = ACR_Skirmish.FindBase(keptHQ);
   int ownedCount;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner != "") ownedCount++;
   bool owners = movedBase && keptBase && movedBase.m_sOwner == moved && keptBase.m_sOwner == kept && ownedCount == 2 && movedBase.m_iGarrisonLeft > 0;
   ACR_TestRegress.Check("skirmish: HQs owned by their sides, the rest neutral", owners, string.Format("%1 owned bases", ownedCount));
   // Starting units at their HQ.
   int atHome, away;
   string awayNames = "";
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction() || !squad.GetAnchor()) continue;
    string key = squad.GetFaction().GetFactionKey();
    ACR_Base home = keptBase;
    if (key == moved) home = movedBase;
    if (!home) continue;
    float distance = vector.DistanceXZ(squad.GetAnchor().GetOrigin(), home.GetOrigin());
    if (distance < 150) atHome++;
    else { away++; awayNames += string.Format("%1 %2 m; ", squad.GetName(), (int)distance); }
   }
   ACR_TestRegress.Check("skirmish: starting units at their side's HQ", atHome >= 8 && away == 0, string.Format("%1 at home, %2 away: %3", atHome, away, awayNames));
   bool pointMoved;
   foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPointsForFaction(moved))
    if (point && movedBase && vector.DistanceXZ(point.GetOrigin(), movedBase.GetOrigin()) < 60) pointMoved = true;
   ACR_TestRegress.Check("skirmish: player spawn point moved to the new HQ", pointMoved, moved);
   // Flashpoint HQs have two pads; the South HQ now has two at Lamentin. Retired bases are dropped.
   int gone, live;
   foreach (ACR_Helipad pad : ACR_Helipad.s_All) { if (pad.m_bGone) gone++; else live++; }
   bool padsMatch = live == 4 && gone == ACR_Helipad.s_All.Count() - 4;
   int lamentinPads;
   if (South())
   {
    foreach (ACR_Helipad pad2 : ACR_Helipad.s_All)
    {
     if (!pad2 || pad2.m_bGone || !pad2.Base() || pad2.Base().m_sName != "Lamentin") continue;
     lamentinPads++;
    }
    padsMatch = live >= 4 && ACR_HeliStation.UsesPads(kept) && ACR_HeliStation.UsesPads(moved) && lamentinPads == 2;
   }
   ACR_TestRegress.Check("skirmish: HQ pad access and Lamentin's two pads", padsMatch, string.Format("%1 gone, %2 in play; Lamentin pads %3", gone, live, lamentinPads));
   int movedHelis, keptHelis;
   foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction(moved).m_aRequisitions) if (entry.m_iSupportHeli > 0) movedHelis++;
   foreach (ACR_RequisitionEntry entry2 : manager.m_Bases.GetFaction(kept).m_aRequisitions) if (entry2.m_iSupportHeli > 0) keptHelis++;
   bool heliAccess = ACR_HeliStation.UsesPads(kept) && ACR_HeliStation.UsesPads(moved) && movedHelis == 2 && keptHelis == 2;
   ACR_TestRegress.Check("skirmish: support helicopter access follows each side's pads", heliAccess, string.Format("%1 / %2", movedHelis, keptHelis));
   // A layout that leaves a side without a pad: that side calls the off-map gunship.
   foreach (ACR_Helipad drop : ACR_Helipad.s_All) if (drop.Faction() == moved) drop.Drop();
   ACR_Skirmish.RecountPads();
   ACR_TestRegress.Check("skirmish: a side without pads uses off-map helicopters", ACR_HeliStation.UsesPads(kept) && !ACR_HeliStation.UsesPads(moved), "");
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   ACR_FactionData data = manager.m_Bases.GetFaction(moved);
   data.m_iRP = 500;
   int index = -1;
   foreach (int i, ACR_SupportEntry sup : data.m_aSupports) if (sup.m_eType == ACR_ESupportType.HELI_LOITER) index = i;
   string result = "no gunship support";
   vector target = ACR_Manager.Ground(movedBase.GetOrigin() + Vector(300, 0, 300));
   if (index >= 0) result = manager.m_Supports.Call(moved, index, target, true);
   ACR_TestRegress.Check("skirmish: side without a pad gets the off-map gunship", result.Contains("ETA"), result);
  }
  if (tick == 30)
  {
   array<ACR_HeliRun> flying = {};
   manager.m_Supports.Helicopters(flying);
   int up;
   foreach (ACR_HeliRun run : flying) if (run.m_Vehicle && run.m_sFaction == moved) up++;
   ACR_TestRegress.Check("skirmish: the off-map gunship is flying", up > 0, string.Format("%1 helicopters", up));
   ACR_TestRegress.Complete("skirmish", 11);
  }
 }
}
