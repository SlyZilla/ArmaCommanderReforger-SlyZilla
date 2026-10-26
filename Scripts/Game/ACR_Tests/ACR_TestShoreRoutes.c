// srv:shoreroutes (Everon, ~40 s): BBB land routes round water with and without the shore
// rule (BBB #4: a leg ended on a beach at 0.9 m and the squad was stuck there 20 min). Pairs of
// land points on a 1 km grid, 1.5-3.5 km apart, whose straight line crosses water. Each pair is
// routed the old way and the new way; counts legs ending below BBB_Terrain.SHORE_LEVEL, length,
// water on each leg and time.
class ACR_TestShoreRoutes
{
 static void Run(int second)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second != 8) return;
  BBB_Terrain.ShoreRoutes();
  bool saved = BBB_Terrain.s_bShore;
  array<vector> land = {};
  for (int x = 500; x < 13000; x += 1000)
  {
   for (int z = 500; z < 13000; z += 1000)
   {
    vector p = Vector(x, 0, z);
    if (!ACR_Manager.OnMap(p) || BBB_Terrain.IsWater(x, z)) continue;
    land.Insert(ACR_Manager.Ground(p));
   }
  }
  int pairs, routedOld, routedNew, shoreOld, shoreNew, worstMs, wetLegs;
  float lengthOld, lengthNew;
  for (int i = 0; i < land.Count() && pairs < 30; i++)
  {
   for (int j = i + 1; j < land.Count() && pairs < 30; j++)
   {
    float apart = vector.DistanceXZ(land[i], land[j]);
    if (apart < 1500 || apart > 3500 || BBB_Terrain.WaterAlong(land[i], land[j]) <= BBB_Terrain.WET_ALLOWED) continue;
    pairs++;
    array<vector> legs = {};
    BBB_Terrain.s_bShore = false;
    bool okOld = BBB_Terrain.LandRoute(land[i], land[j], legs);
    float lenOld = Length(land[i], legs);
    int lowOld = ShoreCorners(legs);
    BBB_Terrain.s_bShore = true;
    int started = System.GetTickCount();
    bool okNew = BBB_Terrain.LandRoute(land[i], land[j], legs);
    int took = System.GetTickCount() - started;
    float lenNew = Length(land[i], legs);
    int lowNew = ShoreCorners(legs);
    float worstWet;
    vector prev = land[i];
    foreach (vector leg : legs)
    {
     worstWet = Math.Max(worstWet, BBB_Terrain.WaterAlong(prev, leg));
     prev = leg;
    }
    if (worstWet > BBB_Terrain.WET_ALLOWED / 2) wetLegs++;
    worstMs = Math.Max(worstMs, took);
    if (okOld && okNew)
    {
     routedOld++;
     routedNew++;
     shoreOld += lowOld;
     shoreNew += lowNew;
     lengthOld += lenOld;
     lengthNew += lenNew;
    }
    PrintFormat("[ACR] SHOREROUTE <%1 %2> -> <%3 %4>: old routed=%5 shore corners %6 length %7 m | new routed=%8 shore corners %9", (int)land[i][0], (int)land[i][2], (int)land[j][0], (int)land[j][2], okOld, lowOld, (int)lenOld, okNew, lowNew);
    PrintFormat("[ACR] SHOREROUTE   new length %1 m, worst leg water %2 m, %3 ms", (int)lenNew, (int)worstWet, took);
   }
  }
  BBB_Terrain.s_bShore = saved;
  float ratio = lengthNew / Math.Max(1, lengthOld);
  PrintFormat("[ACR] SHOREROUTE summary: %1 pairs, %2 routed both ways, shore corners old %3 new %4, length new/old %5, slowest %6 ms, wet legs %7", pairs, routedOld, shoreOld, shoreNew, ratio, worstMs, wetLegs);
  ACR_TestRegress.Check("enough water-crossing pairs routed", routedOld >= 10, routedOld.ToString());
  ACR_TestRegress.Check("old routes had shore corners to fix", shoreOld > 0, shoreOld.ToString());
  ACR_TestRegress.Check("new routes end no legs on the shore", shoreNew == 0, shoreNew.ToString());
  ACR_TestRegress.Check("new routes at most 25% longer in total", ratio < 1.25, ratio.ToString());
  ACR_TestRegress.Check("every new leg still dry", wetLegs == 0, wetLegs.ToString());
  ACR_TestRegress.Check("slowest new route under 200 ms", worstMs < 200, worstMs.ToString() + " ms");
  ACR_TestRegress.Complete("shoreroutes", 6);
 }

 // Legs that end below the shore level, not counting the destination.
 protected static int ShoreCorners(array<vector> legs)
 {
  int low;
  for (int k = 0; k + 1 < legs.Count(); k++)
  {
   if (BBB_Terrain.IsShore(legs[k])) low++;
  }
  return low;
 }

 protected static float Length(vector from, array<vector> legs)
 {
  if (legs.IsEmpty()) return 0;
  float total;
  vector prev = from;
  foreach (vector leg : legs)
  {
   total += vector.DistanceXZ(prev, leg);
   prev = leg;
  }
  return total;
 }
}
