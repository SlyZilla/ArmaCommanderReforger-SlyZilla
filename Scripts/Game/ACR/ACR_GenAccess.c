// Startup-only access screening: sampled slopes, dry terrain and authored road/bridge links.
// Cached per survey. This is a conservative reachability estimate, not a navmesh guarantee;
// buildings, fences and the AI driver's ability to negotiate a route still need playtesting.
class ACR_GenAccess
{
 static const float CELL = 100;
 static const float HQ_CLEARANCE = 350;
 protected float m_fX, m_fZ, m_fCell;
 protected int m_iW, m_iH;
 protected ref array<vector> m_RoadPoints = {};
 protected ref map<int, ref array<int>> m_Extra = new map<int, ref array<int>>();
 static const float SAMPLE = 10;
 static const float MAX_GRADE = 0.7; // ~35 degrees: infantry approaches, not vehicle access
 static const float ROAD_GRADE = 0.35;
 static const int MAX_ROAD_NODES = 60000;
 protected ref array<int> m_Land = {};
 protected ref array<int> m_Links = {};
 protected ref array<int> m_DX = {1, -1, 0, 0, 1, 1, -1, -1};
 protected ref array<int> m_DZ = {0, 0, 1, -1, 1, -1, 1, -1};

 void Build(array<ref ACR_GenSite> sites)
 {
  int started = System.GetTickCount();
  m_Land.Clear(); m_Links.Clear(); m_RoadPoints.Clear(); m_Extra.Clear();
  m_iW = 0; m_iH = 0;
  if (sites.IsEmpty()) return;
  float minX = float.MAX, minZ = float.MAX, maxX = -float.MAX, maxZ = -float.MAX;
  foreach (ACR_GenSite site : sites)
  {
   minX = Math.Min(minX, site.m_vPos[0]); minZ = Math.Min(minZ, site.m_vPos[2]);
   maxX = Math.Max(maxX, site.m_vPos[0]); maxZ = Math.Max(maxZ, site.m_vPos[2]);
  }
  vector boundsMin, boundsMax;
  GetGame().GetWorld().GetBoundBox(boundsMin, boundsMax);
  m_fX = Math.Max(boundsMin[0], minX - 1500);
  m_fZ = Math.Max(boundsMin[2], minZ - 1500);
  float spanX = Math.Min(boundsMax[0], maxX + 1500) - m_fX;
  float spanZ = Math.Min(boundsMax[2], maxZ + 1500) - m_fZ;
  m_fCell = Math.Max(CELL, Math.Sqrt(spanX * spanZ / 50000));
  m_iW = Math.Max(1, (int)(spanX / m_fCell) + 1);
  m_iH = Math.Max(1, (int)(spanZ / m_fCell) + 1);
  int count = m_iW * m_iH;
  m_Land.Resize(count); m_Links.Resize(count);
  for (int i = 0; i < count; i++)
  {
   vector at = Position(i);
   if (!BBB_Terrain.IsWaterAt(at)) m_Land[i] = 1;
   m_Links[i] = -1;
  }
  BuildRoads();
  PrintFormat("[ACR] GEN ACCESS terrain=%1 road=%2 build=%3 ms", m_Land.Count(), m_RoadPoints.Count(), System.GetTickCount() - started);
 }

 protected vector Position(int index)
 {
  if (index >= m_iW * m_iH) return m_RoadPoints[index - m_iW * m_iH];
  int x = index % m_iW;
  int z = index / m_iW;
  return Vector(m_fX + m_fCell * x, 0, m_fZ + m_fCell * z);
 }

 // Every connector includes its endpoints; equal endpoint heights must not hide a cliff.
 protected bool DryLeg(vector a, vector b)
 {
  return PassableLeg(a, b, false);
 }

 protected bool Surface(vector p, bool road, out float height)
 {
  height = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
  bool wet = height < BBB_Terrain.SEA_LEVEL;
  if (!road) return !wet;
  if (!wet && p[1] <= height + 1) return true;
  // A road polyline alone cannot certify a bridge. Require a physical deck above water
  // or above terrain where the authored road is elevated (ravines and overpasses).
  TraceParam trace = new TraceParam();
  float top = p[1] + 5;
  if (wet) top = Math.Max(top, 30);
  trace.Start = Vector(p[0], top, p[2]);
  float bottom = Math.Max(height, BBB_Terrain.SEA_LEVEL);
  trace.End = Vector(p[0], bottom, p[2]);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  if (fraction >= 1) return false;
  height = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
  return height > bottom + 0.01;
 }

 protected bool PassableLeg(vector a, vector b, bool road)
 {
  float distance = vector.DistanceXZ(a, b);
  int steps = Math.Max(1, Math.Ceil(distance / SAMPLE));
  float grade = MAX_GRADE;
  if (road) grade = ROAD_GRADE;
  float previous;
  if (!Surface(a, road, previous)) return false;
  for (int s = 1; s <= steps; s++)
  {
   vector p = a + (b - a) * (s / (float)steps);
   float height;
   if (!Surface(p, road, height)) return false;
   if (Math.AbsFloat(height - previous) > grade * distance / steps + 0.01) return false;
   previous = height;
  }
  return true;
 }

 protected void AddEdge(int a, int b)
 {
  array<int> edges;
  if (!m_Extra.Find(a, edges)) { edges = {}; m_Extra.Insert(a, edges); }
  if (!edges.Contains(b)) edges.Insert(b);
 }

 // Keep the road's actual bends and bridge approaches, instead of snapping a narrow bridge
 // onto a 100 m terrain cell. Junctions join only within 12 m, with the same surface checks.
 protected void BuildRoads()
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(Vector(m_fX, -1000, m_fZ), Vector(m_fX + m_iW * m_fCell, 3000, m_fZ + m_iH * m_fCell), roads);
  map<int, ref array<int>> buckets = new map<int, ref array<int>>();
  foreach (BaseRoad road : roads)
  {
   array<vector> points = {};
   road.GetPoints(points);
   int last = -1;
   for (int p = 1; p < points.Count(); p++)
   {
    vector a = points[p - 1], b = points[p];
    int steps = Math.Max(1, Math.Ceil(vector.DistanceXZ(a, b) / 25));
    int first = 1;
    if (p == 1) first = 0;
    for (int s = first; s <= steps; s++)
    {
     if (m_RoadPoints.Count() >= MAX_ROAD_NODES) return; // conservative truncation
     vector at = a + (b - a) * (s / (float)steps);
     float height;
     if (!Surface(at, true, height)) { last = -1; continue; }
     at[1] = height;
     int id = m_iW * m_iH + m_RoadPoints.Count();
     m_RoadPoints.Insert(at);
     if (last >= 0 && PassableLeg(Position(last), at, true)) { AddEdge(last, id); AddEdge(id, last); }
     last = id;
     // Elevated road decks only connect to terrain at their approaches, never by dropping
     // vertically to the ground beneath an overpass.
     float terrainHeight;
     if (Surface(at, false, terrainHeight) && Math.AbsFloat(at[1] - terrainHeight) <= 1)
     {
      int ground = Nearby(at);
      if (ground >= 0) { AddEdge(ground, id); AddEdge(id, ground); }
     }
     int cx = Math.Floor(at[0] / 12), cz = Math.Floor(at[2] / 12);
     for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++)
     {
      array<int> near;
      if (!buckets.Find((cx + dx) * 100003 + cz + dz, near)) continue;
      foreach (int other : near)
       if (vector.DistanceXZ(at, Position(other)) <= 12 && Math.AbsFloat(at[1] - Position(other)[1]) <= 0.5 + ROAD_GRADE * vector.DistanceXZ(at, Position(other)) && PassableLeg(at, Position(other), true)) { AddEdge(id, other); AddEdge(other, id); }
     }
     int key = cx * 100003 + cz;
     array<int> bucket;
     if (!buckets.Find(key, bucket)) { bucket = {}; buckets.Insert(key, bucket); }
     bucket.Insert(id);
    }
   }
  }
 }

 protected int Nearby(vector at, array<int> reached = null, vector opposingHQ = vector.Zero, float clearance = -1)
 {
  if (m_iW == 0 || m_iH == 0) return -1;
  int x = Math.Round((at[0] - m_fX) / m_fCell);
  int z = Math.Round((at[2] - m_fZ) / m_fCell);
  int best = -1;
  float distance = float.MAX;
  for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++)
  {
   int nx = x + dx, nz = z + dz;
   if (nx < 0 || nz < 0 || nx >= m_iW || nz >= m_iH) continue;
   int index = nz * m_iW + nx;
   if (!m_Land[index] || (reached != null && !reached[index])) continue;
   vector point = Position(index);
   if (clearance >= 0 && !OutsideHQ(at, point, opposingHQ, clearance)) continue;
   float d = vector.DistanceXZ(at, point);
   if (d < distance && DryLeg(at, point)) { distance = d; best = index; }
  }
  return best;
 }

 protected int Links(int index)
 {
  if (index >= m_Land.Count()) return 0;
  if (m_Links[index] >= 0) return m_Links[index];
  int bits;
  int x = index % m_iW, z = index / m_iW;
  for (int n = 0; n < 8; n++)
  {
   int nx = x + m_DX[n], nz = z + m_DZ[n];
   if (nx < 0 || nz < 0 || nx >= m_iW || nz >= m_iH) continue;
   int next = nz * m_iW + nx;
   if (m_Land[next] && DryLeg(Position(index), Position(next))) bits = bits | (1 << n);
  }
  m_Links[index] = bits;
  return bits;
 }

 static bool OutsideHQ(vector a, vector b, vector hq, float clearance = HQ_CLEARANCE)
 {
  a[1] = 0; b[1] = 0; hq[1] = 0;
  vector leg = b - a;
  float lengthSq = leg.LengthSq();
  float t;
  if (lengthSq > 0) t = Math.Clamp(vector.Dot(hq - a, leg) / lengthSq, 0, 1);
  return vector.DistanceXZ(a + leg * t, hq) >= clearance;
 }

 // Reachable land while the opponent's HQ area is forbidden, even along diagonal edges.
 void Reach(vector from, vector opposingHQ, notnull array<int> reached, float clearance = HQ_CLEARANCE)
 {
  reached.Clear(); reached.Resize(m_Land.Count() + m_RoadPoints.Count());
  if (m_Land.IsEmpty()) return;
  int start = Nearby(from, null, opposingHQ, clearance);
  if (start < 0 || !OutsideHQ(from, Position(start), opposingHQ, clearance)) return;
  ref array<int> queue = {start};
  reached[start] = 1;
  for (int head = 0; head < queue.Count(); head++)
  {
   int current = queue[head];
   int x = current % m_iW, z = current / m_iW;
   array<int> extra;
   if (m_Extra.Find(current, extra)) foreach (int linked : extra)
   {
    if (reached[linked] || !OutsideHQ(Position(current), Position(linked), opposingHQ, clearance)) continue;
    reached[linked] = 1;
    queue.Insert(linked);
   }
   int bits = Links(current);
   for (int n = 0; n < 8; n++)
   {
    if ((bits & (1 << n)) == 0) continue;
    int next = (z + m_DZ[n]) * m_iW + x + m_DX[n];
    if (reached[next] || !OutsideHQ(Position(current), Position(next), opposingHQ, clearance)) continue;
    reached[next] = 1;
    queue.Insert(next);
   }
  }
 }

 bool CanReach(vector at, array<int> reached, vector opposingHQ, float clearance = HQ_CLEARANCE)
 {
  int index = Nearby(at, reached, opposingHQ, clearance);
  return index >= 0 && OutsideHQ(at, Position(index), opposingHQ, clearance);
 }
}
