// -acrMortarSpotTest 1 (server-capable, ~20 s): a mortar's set-up spot keeps out of the way of
// vehicles (#33). A spot on a road near the test range, and one at a US base's centre, are
// refused; searching from each finds a spot clear of the road's edge by CLEAR_OF_ROAD and of the
// base's centre by CLEAR_OF_BASE; and on open ground away from both, the search still finds level
// ground as before.
class ACR_TestMortarSpot
{
 static const int CHECKS = 3;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   ACR_TestRegress.Abort("mortarspot", "no Alpha squad");
   return;
  }
  vector here = alpha.GetAnchor().GetOrigin();

  // On a road.
  string roadName = "mortarspot: a spot on a road is refused, and the search finds one clear of the road's edge";
  vector onRoad = RoadPoint(here);
  if (onRoad == vector.Zero) ACR_TestRegress.Skip(roadName, "no road point 60 m from every base within 1.6 km");
  else
  {
   vector roadSpot;
   bool roadRefused = !ACR_Mortars.CheckSpot(onRoad, roadSpot);
   vector roadFound;
   bool roadGot = ACR_Mortars.FindSpot(onRoad, roadFound);
   string why = "";
   if (roadGot) why = ACR_Mortars.InTheWay(roadFound);
   ACR_TestRegress.Check(roadName, roadRefused && roadGot && why == "", string.Format("on the road at %1 ('%2'): refused %3; found %4 at %5, %6 m off ('%7')", onRoad, ACR_Mortars.InTheWay(onRoad), roadRefused, roadGot, roadFound, (int)vector.DistanceXZ(roadFound, onRoad), why));
  }

  // At a base's centre, where bought vehicles appear round it.
  string baseName = "mortarspot: a spot at a base's centre is refused, and the search finds one outside its vehicle ring";
  ACR_Base base;
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (b && !b.m_bRetired && b.m_sOwner == "US") { base = b; break; }
  }
  if (!base) ACR_TestRegress.Skip(baseName, "no US base");
  else
  {
   vector centre = ACR_Manager.Ground(base.GetOrigin());
   vector baseSpot;
   bool baseRefused = !ACR_Mortars.CheckSpot(centre, baseSpot);
   vector baseFound;
   bool baseGot = ACR_Mortars.FindSpot(centre, baseFound);
   float off = vector.DistanceXZ(baseFound, base.GetOrigin());
   ACR_TestRegress.Check(baseName, baseRefused && baseGot && off >= ACR_Mortars.CLEAR_OF_BASE, string.Format("%1's centre: refused %2; found %3 %4 m from it", base.m_sName, baseRefused, baseGot, (int)off));
  }

  // Open ground away from both: as before.
  string openName = "mortarspot: on open ground away from roads and bases the search finds level ground as before";
  vector open = OpenGround(here);
  if (open == vector.Zero) ACR_TestRegress.Skip(openName, "no open ground found");
  else
  {
   vector openFound;
   bool openGot = ACR_Mortars.FindSpot(open, openFound);
   ACR_TestRegress.Check(openName, openGot && vector.DistanceXZ(openFound, open) <= 12 && ACR_Mortars.SlopeAt(openFound) <= ACR_Mortars.MAX_SLOPE_DEG + 0.5, string.Format("from %1: found %2, %3 m off, slope %4 deg", open, openGot, (int)vector.DistanceXZ(openFound, open), ACR_Mortars.SlopeAt(openFound)));
  }
  ACR_TestRegress.Complete("mortarspot", CHECKS);
 }

 // A point on a road's centreline 60 m or more from every base (so a refusal there is the road's):
 // the road nearest Alpha, nearest each base, or nearest points in rings round Alpha out to 1.6 km
 // (vector.Zero: none).
 protected static vector RoadPoint(vector near)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return vector.Zero;
  array<vector> from = {near};
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (b) from.Insert(b.GetOrigin());
  }
  for (float r = 200; r <= 1600; r += 200)
  {
   for (int k = 0; k < 8; k++)
   {
    float angle = k * 45 * Math.DEG2RAD;
    from.Insert(near + Vector(Math.Sin(angle) * r, 0, Math.Cos(angle) * r));
   }
  }
  foreach (vector origin : from)
  {
   BaseRoad road;
   float distance;
   aiWorld.GetRoadNetworkManager().GetClosestRoad(origin, road, distance);
   if (!road) continue;
   array<vector> points = {};
   road.GetPoints(points);
   for (int i = 1; i < points.Count(); i++)
   {
    vector mid = ACR_Manager.Ground((points[i - 1] + points[i]) * 0.5);
    bool clear = true;
    foreach (ACR_Base base : ACR_Base.s_All)
    {
     if (base && vector.DistanceXZ(base.GetOrigin(), mid) < 60) clear = false;
    }
    if (clear && ACR_Mortars.InTheWay(mid) == "by a road") return mid;
   }
  }
  return vector.Zero;
 }

 // Level, dry ground 60 m or more from any road and base, within 600 m.
 protected static vector OpenGround(vector near)
 {
  for (float r = 50; r <= 600; r += 50)
  {
   for (int k = 0; k < 12; k++)
   {
    float angle = k * 30 * Math.DEG2RAD;
    vector p = ACR_Manager.Ground(near + Vector(Math.Sin(angle) * r, 0, Math.Cos(angle) * r));
    if (BBB_Terrain.IsWaterAt(p) || ACR_Mortars.SlopeAt(p) > ACR_Mortars.MAX_SLOPE_DEG * 0.5) continue;
    if (ACR_Mortars.InTheWay(p) != "") continue;
    bool farOff = true;
    foreach (ACR_Base b : ACR_Base.s_All)
    {
     if (b && vector.DistanceXZ(b.GetOrigin(), p) < 60) farOff = false;
    }
    if (farOff) return p;
   }
  }
  return vector.Zero;
 }
}
