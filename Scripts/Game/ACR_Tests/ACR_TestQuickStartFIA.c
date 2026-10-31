// srv:quickstartfia (Everon whole map, about 1 min): FIA on a Quick start with many cells. Handing
// out most of the map used to count every base as a starting base, which left FIA no room (playtest
// pn16: 8 cells of 14, 2 houses). Checks: only the sides' own bases are starting bases; the cells are
// there, houses among them; they keep off those bases and out of every base.
class ACR_TestQuickStartFIA
{
 static void Run(int second)
 {
  if (second == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (second == 10) Placed();
 }

 protected static void Placed()
 {
  int given = ACR_QuickStart.s_Assigned.Count();
  int starts = ACR_Guerrillas.s_aStarts.Count();
  ACR_TestRegress.Check("quickstartfia: the handed-out bases aren't starting bases", given >= 10 && starts >= 2 && starts < 10, string.Format("%1 handed out, %2 starting bases", given, starts));
  int roads, houses;
  bool clear = true;
  bool outside = true;
  string detail = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell) continue;
   if (post.m_Cell.m_bHouse) houses++;
   else roads++;
   vector site = post.m_Cell.m_vKillZone;
   foreach (vector start : ACR_Guerrillas.s_aStarts) if (vector.DistanceXZ(site, start) < ACR_Guerrillas.AMBUSH_HQ_CLEAR) clear = false;
   foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && !base.m_bHidden && vector.DistanceXZ(site, base.GetOrigin()) < base.m_fRadius) outside = false;
   detail += string.Format(" %1", post.m_sName);
  }
  ACR_TestRegress.Check("quickstartfia: most of the wanted cells placed, houses among them", roads + houses >= 10 && houses >= 1, string.Format("%1 roads, %2 houses (14 wanted);%3", roads, houses, detail));
  ACR_TestRegress.Check("quickstartfia: all off the starting bases", clear, "");
  ACR_TestRegress.Check("quickstartfia: all outside every base", outside, "");
  ACR_TestRegress.Complete("quickstartfia", 4);
 }
}
