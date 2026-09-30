// Road routes for AI drivers. The game's AI driver is at its worst on long
// trips: it cuts across country, gets stuck, gives up and the crew walks.
// Instead, a vehicle's trip is planned once along the road network (A* over
// the road points in the area) and handed to the driver as a chain of short
// hops of about LEG metres, each on the road. The game still does the actual
// driving, so there is no per-frame cost: only this one search per order.
class ACR_Roads
{
 static const float MARGIN = 700;     // search area around the two ends
 static const float SPACING = 12;     // keep road points about this far apart
 static const float LINK = 14;        // points of different roads this close are connected (junctions)
 static const float LEG = 160;        // distance between the hops given to the driver
 static const float MAX_OFFROAD = 300;// ends farther than this from any road: no road route
 static const int MAX_NODES = 40000;

 // Legs along the roads from a to b (last one the road point nearest b).
 // Returns false when there is no sensible road route (then drive direct).
 static bool Route(vector a, vector b, notnull array<vector> legs, out int nodeCount, out int millis)
 {
  legs.Clear();
  nodeCount = 0;
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
   foreach (int p, vector point : points)
   {
    bool isEnd = p == 0 || p == points.Count() - 1;
    if (last >= 0 && !isEnd && vector.DistanceXZ(point, lastPos) < SPACING) continue;
    int id = pos.Count();
    pos.Insert(point);
    prev.Insert(last);
    next.Insert(-1);
    roadOf.Insert(r);
    if (last >= 0) next[last] = id;
    last = id;
    lastPos = point;
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
  ACR_Terrain.HeapPush(heap, key, start);
  bool found;
  array<int> around = {};
  while (!heap.IsEmpty())
  {
   int current = ACR_Terrain.HeapPop(heap, key);
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
    ACR_Terrain.HeapPush(heap, key, n);
   }
  }
  millis = System.GetTickCount() - started;
  if (!found) return false;

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
    legs.Insert(ACR_Manager.Ground(path[s]));
    travelled = 0;
   }
  }
  legs.Insert(ACR_Manager.Ground(path[path.Count() - 1]));
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
