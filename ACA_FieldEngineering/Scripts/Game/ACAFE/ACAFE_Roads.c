// Road sense for the engineering orders: the nearest point on the nearest road, its heading
// and width (roadblocks and checkpoints are laid across it), and how far a point is from it.
class ACAFE_Roads
{
 // The road last snapped to (its points), for OffRoadDistance during one planning pass.
 protected static ref array<vector> s_Points = {};
 protected static float s_fWidth;

 // point: on the road centre line nearest to 'near'; dir: the road's heading there (unit, flat).
 static bool Snap(vector near, float maxDistance, out vector point, out vector dir, out float width)
 {
  s_Points.Clear();
  s_fWidth = 0;
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld) return false;
  RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
  if (!roads) return false;
  BaseRoad road;
  float roadDistance;
  roads.GetClosestRoad(near, road, roadDistance, true);
  if (!road) return false;
  array<vector> points = {};
  road.GetPoints(points);
  if (points.Count() < 2) return false;
  float best = float.MAX;
  vector bestPoint;
  vector bestDir;
  for (int i = 0; i + 1 < points.Count(); i++)
  {
   vector a = points[i];
   vector b = points[i + 1];
   vector closest;
   float t = Project(near, a, b, closest);
   if (t < 0) continue;
   float d = vector.DistanceXZ(closest, near);
   if (d >= best) continue;
   best = d;
   bestPoint = closest;
   bestDir = b - a;
  }
  if (best > maxDistance) return false;
  bestDir[1] = 0;
  if (bestDir.Length() < 0.01) return false;
  bestDir.Normalize();
  point = bestPoint;
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
  dir = bestDir;
  width = road.GetWidth();
  if (width <= 0) width = 6;
  s_Points.Copy(points);
  s_fWidth = width;
  return true;
 }

 // Closest point to p on the segment a-b (flat). Returns the segment parameter 0..1, or -1
 // for a degenerate segment.
 static float Project(vector p, vector a, vector b, out vector closest)
 {
  float abx = b[0] - a[0];
  float abz = b[2] - a[2];
  float lengthSq = abx * abx + abz * abz;
  if (lengthSq < 0.0001) return -1;
  float t = ((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lengthSq;
  t = Math.Clamp(t, 0, 1);
  closest = Vector(a[0] + abx * t, a[1] + (b[1] - a[1]) * t, a[2] + abz * t);
  return t;
 }

 // Metres from p to the edge of the road last snapped to (negative: on it). float.MAX when
 // nothing was snapped.
 static float OffRoadDistance(vector p)
 {
  if (s_Points.Count() < 2) return float.MAX;
  float best = float.MAX;
  for (int i = 0; i + 1 < s_Points.Count(); i++)
  {
   vector closest;
   if (Project(p, s_Points[i], s_Points[i + 1], closest) < 0) continue;
   best = Math.Min(best, vector.DistanceXZ(closest, p));
  }
  return best - s_fWidth * 0.5;
 }

 // Compass yaw (degrees) whose +Z points along dir, like ACA's FOB defences.
 static float Yaw(vector dir)
 {
  float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  while (yaw > 180) yaw -= 360;
  while (yaw <= -180) yaw += 360;
  return yaw;
 }

 // Local (x across, z along) to world, for a frame facing 'forward' at 'origin'.
 static vector ToWorld(vector origin, vector forward, vector local)
 {
  vector right = Vector(forward[2], 0, -forward[0]);
  vector world = origin + right * local[0] + forward * local[2];
  world[1] = GetGame().GetWorld().GetSurfaceY(world[0], world[2]);
  return world;
 }
}
