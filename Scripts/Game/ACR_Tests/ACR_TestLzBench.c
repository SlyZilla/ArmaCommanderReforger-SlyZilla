// srv:lzbench (Kolguyev, ~40 s): times ACR_LandingZone.Find, the air lift pickup/drop search,
// around every generated base and four points 150 m out from it. Prints per-search times and
// the spots found so before/after changes can be compared. Jacob 18 Oct: air lift planning froze.
class ACR_TestLzBench
{
 static void Run(int second)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second != 8) return;
  array<vector> points = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   vector o = base.GetOrigin();
   points.Insert(o);
   points.Insert(o + Vector(150, 0, 0));
   points.Insert(o + Vector(-150, 0, 0));
   points.Insert(o + Vector(0, 0, 150));
   points.Insert(o + Vector(0, 0, -150));
  }
  int total, worst, found, none, slow;
  int checksTotal;
  array<vector> spots = {};
  foreach (int i, vector p : points)
  {
   if (!ACR_Manager.OnMap(p) || BBB_Terrain.IsWater(p[0], p[2])) continue;
   ACR_LandingZone.s_iChecks = 0;
   ACR_LandingZone.s_iRejTree = 0; ACR_LandingZone.s_iRejRay = 0; ACR_LandingZone.s_iRejGround = 0; ACR_LandingZone.s_iRejBox = 0;
   int t = System.GetTickCount();
   int n = ACR_LandingZone.Find(p, ACR_AirLift.PICKUP_SEARCH, 2, spots);
   int ms = System.GetTickCount() - t;
   total += ms;
   checksTotal += ACR_LandingZone.s_iChecks;
   worst = Math.Max(worst, ms);
   if (ms > 100) slow++;
   if (n > 0) found++; else none++;
   string where = "";
   foreach (vector s : spots) where += string.Format(" <%1 %2>", (int)s[0], (int)s[2]);
   PrintFormat("[ACR] LZBENCH %1 at <%2 %3>: %4 ms, %5 checks, %6 spot(s)%7 | refused tree %8 ray %9", i, (int)p[0], (int)p[2], ms, ACR_LandingZone.s_iChecks, n, where, ACR_LandingZone.s_iRejTree, ACR_LandingZone.s_iRejRay);
   PrintFormat("[ACR] LZBENCH   %1 refused ground %2 box %3", i, ACR_LandingZone.s_iRejGround, ACR_LandingZone.s_iRejBox);
  }
  int searches = found + none;
  PrintFormat("[ACR] LZBENCH summary: %1 searches, total %2 ms, avg %3 ms, max %4 ms, %5 over 100 ms, %6 checks, %7 found / %8 none", searches, total, total / Math.Max(1, searches), worst, slow, checksTotal, found, none);
  ACR_TestRegress.Check("benchmark ran searches", searches > 20, searches.ToString());
  ACR_TestRegress.Check("slowest landing zone search under 200 ms", worst < 200, worst.ToString() + " ms");
  ACR_TestRegress.Complete("lzbench", 2);
 }
}
