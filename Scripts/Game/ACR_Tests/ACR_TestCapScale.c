// -acrCapScaleTest 1 (server-capable, ~20 s): squad / soldier caps scaled to the map
// (-acrCapScale). Off by default (x1, caps as set); the factor from bases and spread (x1 up to
// 12 bases and 5 km, then more, at most x2); applied at x1.5: squad caps and the soldier cap
// grow, the active-AI limit follows; the soldier cap stops at 250 and a host's higher cap stays.
class ACR_TestCapScale
{
 static const int CHECKS = 6;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || tick != 8) return;
  ACR_FactionSettings us = ACR_FactionSettings.Find("US");
  if (!us)
  {
   ACR_TestRegress.Abort("capscale", "no US settings");
   return;
  }
  ACR_TestRegress.Check("capscale: off by default (x1)", !ACR_Options.Flag("acrCapScale") && manager.m_fCapScale == 1, string.Format("factor %1", manager.m_fCapScale));

  bool math = ACR_Manager.CapScaleFor(10, 3000) == 1 && ACR_Manager.CapScaleFor(18, 3000) == 1.5 && Math.AbsFloat(ACR_Manager.CapScaleFor(6, 9000) - 1.8) < 0.001 && ACR_Manager.CapScaleFor(40, 12000) == 2;
  ACR_TestRegress.Check("capscale: x1 up to 12 bases and 5 km, then by whichever says more, at most x2", math, string.Format("%1 %2 %3 %4", ACR_Manager.CapScaleFor(10, 3000), ACR_Manager.CapScaleFor(18, 3000), ACR_Manager.CapScaleFor(6, 9000), ACR_Manager.CapScaleFor(40, 12000)));

  // Applied at x1.5.
  int unitWas = us.m_iAIUnitCap;
  int soldiersWas = manager.m_iSoldierCap;
  us.m_iAIUnitCap = 8;
  manager.m_iSoldierCap = 100;
  manager.ApplyCapScale(1.5, "test");
  int limit = GetGame().GetAIWorld().GetLimitOfActiveAIs();
  ACR_TestRegress.Check("capscale: at x1.5 squad caps 8 -> 12 and the soldier cap 100 -> 150, the active-AI limit follows", us.m_iAIUnitCap == 12 && manager.m_iSoldierCap == 150 && limit >= 2 * 150 + ACR_Manager.AI_LIMIT_HEADROOM, string.Format("squad cap %1, soldier cap %2, limit %3", us.m_iAIUnitCap, manager.m_iSoldierCap, limit));

  // The soldier cap stops at 250; a host's higher one stays.
  manager.m_iSoldierCap = 200;
  manager.ApplyCapScale(1.5, "test");
  int stopped = manager.m_iSoldierCap;
  manager.m_iSoldierCap = 300;
  manager.ApplyCapScale(1.5, "test");
  ACR_TestRegress.Check("capscale: the soldier cap stops at 250, a host's higher cap stays", stopped == 250 && manager.m_iSoldierCap == 300, string.Format("200 -> %1, 300 -> %2", stopped, manager.m_iSoldierCap));

  // Resolved once: a second pass at the first tick changes nothing.
  us.m_iAIUnitCap = 8;
  manager.ScaleCapsToMap();
  ACR_TestRegress.Check("capscale: with the option off a pass changes nothing", us.m_iAIUnitCap == 8, string.Format("squad cap %1", us.m_iAIUnitCap));
  // The setup screen's row: off by default, on when set.
  string offRow = ACR_Setup.Rows();
  ACR_Options.Set("acrCapScale", "1");
  string onRow = ACR_Setup.Rows();
  ACR_Options.Unset("acrCapScale");
  ACR_TestRegress.Check("capscale: a setup row, off by default", offRow.Contains("capscale\tSquad caps scale with map: off") && onRow.Contains("capscale\tSquad caps scale with map: on") && ACR_Setup.Option("capscale") == "acrCapScale", "");
  us.m_iAIUnitCap = unitWas;
  manager.m_iSoldierCap = soldiersWas;
  manager.m_fCapScale = 1;
  ACR_TestRegress.Complete("capscale", CHECKS);
 }
}
