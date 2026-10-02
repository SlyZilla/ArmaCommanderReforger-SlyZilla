// Road routes for AI drivers. The game's AI driver is at its worst on long
// trips: it cuts across country, gets stuck, gives up and the crew walks.
// Instead, a vehicle's trip is planned once along the road network (A* over
// the road points in the area) and handed to the driver as a chain of short
// hops of about LEG metres, each on the road. The game still does the actual
// driving, so there is no per-frame cost: only this one search per order.
class BBB_Roads
{
 static const float MARGIN = 700;     // search area around the two ends
 static const float SPACING = 12;     // keep road points about this far apart
 static const float LINK = 14;        // points of different roads this close are connected (junctions)
 static const float LEG = 160;        // distance between the hops given to the driver
 static const float MAX_OFFROAD = 300;// ends farther than this from any road: no road route
 static const int MAX_NODES = 40000;
 static const float DEAD_END_LINK = 30; // a road's end this close to another road joins it
 static const float MAX_DETOUR = 1.8;  // road trip longer than this x the straight line (+250 m): drive direct
 static float s_fLastLength;          // length of the last route found (m)
 static bool s_bLastDetour;           // the last route was refused as a long way round

 // Legs along the roads from a to b (last one the road point nearest b).
 // Returns false when there is no sensible road route (then drive direct).
 static bool Route(vector a, vector b, notnull array<vector> legs, out int nodeCount, out int millis)
 {
  legs.Clear();
  nodeCount = 0;
  s_bLastDetour = false;
  s_fLastLength = 0;
  int started = System.GetTickCount();
  millis = 0;
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld) return false;
  RoadNetworkManager network = aiWorld.GetRoadNetworkManager();
  if (!network) return false;
  vector min = Vector(Math.Min(a[0], b[0]) - MARGIN, -1000, Math.Min(a[2], b[2]) - MARGIN);
  vector max = Vector(Math.Max(a[0], b[0]) + MARGIN, 3000, Math.Max(a[2], b[2]) + MARGIN);
  array<BaseRoad> roads = {};
  network.GetRoadsInAABB(min, max, roads);
  if (roads.IsEmpty()) return false;

  // Nodes: road points thinned to SPACING, linked to their neighbours along the road.
  array<vector> pos = {};
  array<int> prev = {};
  array<int> next = {};
  array<int> roadOf = {};
  foreach (int r, BaseRoad road : roads)
  {
   array<vector> points = {};
   road.GetPoints(points);
   int last = -1;
   vector lastPos;
   // Raw road points can be 50+ m apart on straight stretches; fill in points every
   // SPACING so a side road joining mid-stretch finds a point to link to (missing
   // junctions sent trucks on 2 km detours for 600 m trips).
   array<vector> dense = {};
   foreach (int p, vector point : points)
   {
    if (p > 0)
    {
     vector from = points[p - 1];
     float gap = vector.DistanceXZ(from, point);
     int steps = Math.Floor(gap / SPACING);
     for (int f = 1; f < steps; f++) dense.Insert(from + (point - from) * (f / (float)steps));
    }
    dense.Insert(point);
   }
   foreach (int q, vector densePoint : dense)
   {
    bool isEnd = q == 0 || q == dense.Count() - 1;
    if (last >= 0 && !isEnd && vector.DistanceXZ(densePoint, lastPos) < SPACING * 0.5) continue;
    int id = pos.Count();
    pos.Insert(densePoint);
    prev.Insert(last);
    next.Insert(-1);
    roadOf.Insert(r);
    if (last >= 0) next[last] = id;
    last = id;
    lastPos = densePoint;
    if (pos.Count() >= MAX_NODES) break;
   }
   if (pos.Count() >= MAX_NODES) break;
  }
  int count = pos.Count();
  nodeCount = count;
  if (count < 2) return false;

  // Junctions: points of different roads close together (a spatial grid keeps it fast).
  map<int, ref array<int>> grid = new map<int, ref array<int>>();
  for (int i = 0; i < count; i++)
  {
   int cell = CellKey(pos[i]);
   array<int> bucket;
   if (!grid.Find(cell, bucket))
   {
    bucket = {};
    grid.Insert(cell, bucket);
   }
   bucket.Insert(i);
  }
  map<int, ref array<int>> links = new map<int, ref array<int>>();
  for (int j = 0; j < count; j++)
  {
   int cx = Math.Floor(pos[j][0] / LINK);
   int cz = Math.Floor(pos[j][2] / LINK);
   for (int dx = -1; dx <= 1; dx++)
   {
    for (int dz = -1; dz <= 1; dz++)
    {
     array<int> near;
     if (!grid.Find((cx + dx) * 100003 + (cz + dz), near)) continue;
     foreach (int k : near)
     {
      if (k <= j || roadOf[k] == roadOf[j]) continue;
      if (vector.DistanceXZ(pos[j], pos[k]) > LINK) continue;
      AddLink(links, j, k);
      AddLink(links, k, j);
     }
    }
   }
  }

  // Road ends (dead ends, gaps in the data) join the nearest point of another road nearby.
  for (int e = 0; e < count; e++)
  {
   if (prev[e] >= 0 && next[e] >= 0) continue;
   int nearest = -1;
   float nearestDistance = DEAD_END_LINK;
   int ex = Math.Floor(pos[e][0] / LINK);
   int ez = Math.Floor(pos[e][2] / LINK);
   for (int gx = -3; gx <= 3; gx++)
   {
    for (int gz = -3; gz <= 3; gz++)
    {
     array<int> cellNodes;
     if (!grid.Find((ex + gx) * 100003 + (ez + gz), cellNodes)) continue;
     foreach (int o : cellNodes)
     {
      if (roadOf[o] == roadOf[e]) continue;
      float dd = vector.DistanceXZ(pos[e], pos[o]);
      if (dd < nearestDistance) { nearestDistance = dd; nearest = o; }
     }
    }
   }
   if (nearest >= 0)
   {
    AddLink(links, e, nearest);
    AddLink(links, nearest, e);
   }
  }

  int start = Nearest(pos, a);
  int goal = Nearest(pos, b);
  if (vector.DistanceXZ(pos[start], a) > MAX_OFFROAD || vector.DistanceXZ(pos[goal], b) > MAX_OFFROAD) return false;

  // A* along the roads.
  array<float> cost = {};
  array<float> key = {};
  array<int> parent = {};
  array<bool> closed = {};
  cost.Resize(count);
  key.Resize(count);
  parent.Resize(count);
  closed.Resize(count);
  for (int c = 0; c < count; c++)
  {
   cost[c] = float.MAX;
   parent[c] = -1;
   closed[c] = false;
  }
  array<int> heap = {};
  cost[start] = 0;
  key[start] = vector.DistanceXZ(pos[start], pos[goal]);
  BBB_Terrain.HeapPush(heap, key, start);
  bool found;
  array<int> around = {};
  while (!heap.IsEmpty())
  {
   int current = BBB_Terrain.HeapPop(heap, key);
   if (closed[current]) continue;
   closed[current] = true;
   if (current == goal) { found = true; break; }
   around.Clear();
   if (prev[current] >= 0) around.Insert(prev[current]);
   if (next[current] >= 0) around.Insert(next[current]);
   array<int> linked;
   if (links.Find(current, linked)) foreach (int l : linked) around.Insert(l);
   foreach (int n : around)
   {
    if (closed[n]) continue;
    float tentative = cost[current] + vector.DistanceXZ(pos[current], pos[n]);
    if (tentative >= cost[n]) continue;
    cost[n] = tentative;
    parent[n] = current;
    key[n] = tentative + vector.DistanceXZ(pos[n], pos[goal]);
    BBB_Terrain.HeapPush(heap, key, n);
   }
  }
  millis = System.GetTickCount() - started;
  if (!found) return false;
  // A long way round is no time saver: the game's driver goes direct instead.
  s_fLastLength = cost[goal] + vector.DistanceXZ(a, pos[start]) + vector.DistanceXZ(pos[goal], b);
  float direct = vector.DistanceXZ(a, b);
  if (s_fLastLength > direct * MAX_DETOUR + 250)
  {
   PrintFormat("[ACR] ROAD detour too long: %1 m by road for %2 m direct, driving direct", (int)s_fLastLength, (int)direct);
   s_bLastDetour = true;
   return false;
  }

  // Path back from the goal, then one hop every LEG metres (plus the goal).
  array<vector> path = {};
  int walk = goal;
  while (walk >= 0)
  {
   path.InsertAt(pos[walk], 0);
   if (walk == start) break;
   walk = parent[walk];
  }
  float travelled;
  for (int s = 1; s < path.Count(); s++)
  {
   travelled += vector.DistanceXZ(path[s - 1], path[s]);
   if (travelled >= LEG && s < path.Count() - 1)
   {
    legs.Insert(BBB.Ground(path[s]));
    travelled = 0;
   }
  }
  legs.Insert(BBB.Ground(path[path.Count() - 1]));
  millis = System.GetTickCount() - started;
  return true;
 }

 protected static int CellKey(vector p)
 {
  int cx = Math.Floor(p[0] / LINK);
  int cz = Math.Floor(p[2] / LINK);
  return cx * 100003 + cz;
 }

 protected static void AddLink(map<int, ref array<int>> links, int from, int to)
 {
  array<int> list;
  if (!links.Find(from, list))
  {
   list = {};
   links.Insert(from, list);
  }
  if (!list.Contains(to)) list.Insert(to);
 }

 protected static int Nearest(array<vector> pos, vector point)
 {
  int best = 0;
  float bestDistance = float.MAX;
  foreach (int i, vector p : pos)
  {
   float d = vector.DistanceXZ(p, point);
   if (d < bestDistance) { bestDistance = d; best = i; }
  }
  return best;
 }
}
