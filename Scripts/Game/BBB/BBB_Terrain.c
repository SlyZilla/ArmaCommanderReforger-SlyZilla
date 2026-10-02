// Terrain sense for orders: the game's pathfinding happily sends infantry
// swimming across a bay when that is the shortest line. Before an AI order goes
// out we check the straight line for water; if it crosses more than a little,
// a coarse land-only route (A* over 50 m cells) is found and the squad walks it
// leg by leg. Also used to keep attack staging points on dry ground.
class BBB_Terrain
{
 static const float SEA_LEVEL = 0.4;      // surface below this is sea (Arland/Everon sea level is 0)
 static const float WET_ALLOWED = 40;     // m of water on a straight leg that is still fine (streams, beaches)
 static const float CELL = 50;
 static const float MARGIN = 1500;        // search area around the two ends
 static const int MAX_CELLS = 60000;

 // The nearest point on a road within maxDistance (false if none). The game's AI
 // drivers can't reach many off-road spots: the move fails and the crew gets
 // out and walks. Sending vehicles to road points avoids that.
 static bool RoadPointNear(vector pos, float maxDistance, out vector point)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld) return false;
  RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
  if (!roads) return false;
  BaseRoad road;
  float distance;
  roads.GetClosestRoad(pos, road, distance);
  if (!road || distance > maxDistance) return false;
  array<vector> points = {};
  road.GetPoints(points);
  float best = float.MAX;
  foreach (vector p : points)
  {
   float d = vector.DistanceXZ(p, pos);
   if (d < best) { best = d; point = p; }
  }
  if (best > maxDistance) return false;
  point = BBB.Ground(point);
  return true;
 }

 static bool IsWater(float x, float z)
 {
  return GetGame().GetWorld().GetSurfaceY(x, z) < SEA_LEVEL;
 }

 static bool IsWaterAt(vector p)
 {
  return IsWater(p[0], p[2]);
 }

 // Metres of the straight line from a to b that run over water.
 static float WaterAlong(vector a, vector b, float step = 20)
 {
  float length = vector.DistanceXZ(a, b);
  int samples = Math.Max(1, (int)(length / step));
  float wet;
  for (int i = 1; i < samples; i++)
  {
   float t = i / (float)samples;
   if (IsWater(a[0] + (b[0] - a[0]) * t, a[2] + (b[2] - a[2]) * t)) wet += length / samples;
  }
  return wet;
 }

 // A point on the ring around target in roughly this direction that is dry and
 // has a dry run in to the target. Returns false if none is found.
 static bool DryApproach(vector target, vector dir, float ring, out vector point)
 {
  array<float> turns = {0, 20, -20, 40, -40, 60, -60};
  array<float> scales = {1, 0.75, 1.3};
  foreach (float scale : scales)
  {
   foreach (float turn : turns)
   {
    float angle = turn * Math.DEG2RAD;
    vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
    vector candidate = target + rotated * ring * scale;
    if (IsWaterAt(candidate) || WaterAlong(candidate, target) > WET_ALLOWED / 2) continue;
    point = BBB.Ground(candidate);
    return true;
   }
  }
  return false;
 }

 // Land route from a to b as a list of waypoints ending with b. Returns false when
 // the straight line is dry enough (or no land route exists): then go direct.
 static bool LandRoute(vector a, vector b, notnull array<vector> route)
 {
  route.Clear();
  if (WaterAlong(a, b) <= WET_ALLOWED) return false;
  float minX = Math.Min(a[0], b[0]) - MARGIN;
  float minZ = Math.Min(a[2], b[2]) - MARGIN;
  float spanX = Math.AbsFloat(a[0] - b[0]) + 2 * MARGIN;
  float spanZ = Math.AbsFloat(a[2] - b[2]) + 2 * MARGIN;
  float cell = CELL;
  if ((spanX / cell) * (spanZ / cell) > MAX_CELLS) cell = Math.Sqrt(spanX * spanZ / MAX_CELLS) + 1;
  int w = (int)(spanX / cell) + 1;
  int h = (int)(spanZ / cell) + 1;
  int count = w * h;
  array<float> cost = {};
  array<int> parent = {};
  array<int> state = {};   // 0 unknown, 1 land, 2 water, +10 closed
  cost.Resize(count);
  parent.Resize(count);
  state.Resize(count);
  for (int c = 0; c < count; c++)
  {
   cost[c] = float.MAX;
   parent[c] = -1;
   state[c] = 0;
  }
  int start = CellOf(a, minX, minZ, cell, w, h);
  int goal = CellOf(b, minX, minZ, cell, w, h);
  array<float> key = {};
  key.Resize(count);
  array<int> heap = {};
  cost[start] = 0;
  key[start] = Heuristic(start, goal, w, cell);
  HeapPush(heap, key, start);
  bool found;
  array<int> dxs = {1, -1, 0, 0, 1, 1, -1, -1};
  array<int> dzs = {0, 0, 1, -1, 1, -1, 1, -1};
  while (!heap.IsEmpty())
  {
   int current = HeapPop(heap, key);
   if (state[current] >= 10) continue;
   state[current] = state[current] + 10;
   if (current == goal) { found = true; break; }
   int cx = current % w;
   int cz = current / w;
   for (int n = 0; n < 8; n++)
   {
    int nx = cx + dxs[n];
    int nz = cz + dzs[n];
    if (nx < 0 || nz < 0 || nx >= w || nz >= h) continue;
    int next = nz * w + nx;
    if (state[next] >= 10) continue;
    if (state[next] == 0)
    {
     state[next] = 1;
     if (next != goal && IsWater(minX + (nx + 0.5) * cell, minZ + (nz + 0.5) * cell)) state[next] = 2;
    }
    if (state[next] == 2) continue;
    float step = cell;
    if (n >= 4) step = cell * 1.4142;
    float tentative = cost[current] + step;
    if (tentative >= cost[next]) continue;
    cost[next] = tentative;
    parent[next] = current;
    key[next] = tentative + Heuristic(next, goal, w, cell);
    HeapPush(heap, key, next);
   }
  }
  if (!found) return false;
  // Cell chain from start to goal.
  array<vector> chain = {};
  int walk = goal;
  while (walk >= 0)
  {
   int wx = walk % w;
   int wz = walk / w;
   chain.InsertAt(Vector(minX + (wx + 0.5) * cell, 0, minZ + (wz + 0.5) * cell), 0);
   if (walk == start) break;
   walk = parent[walk];
  }
  chain[0] = a;
  chain[chain.Count() - 1] = b;
  // Keep only the corners: from each point, jump to the farthest one still reachable over dry ground.
  int at = 0;
  while (at < chain.Count() - 1)
  {
   int reach = at + 1;
   for (int j = chain.Count() - 1; j > at + 1; j--)
   {
    if (WaterAlong(chain[at], chain[j]) <= WET_ALLOWED / 2) { reach = j; break; }
   }
   route.Insert(BBB.Ground(chain[reach]));
   at = reach;
  }
  return route.Count() > 1;
 }

 protected static int CellOf(vector p, float minX, float minZ, float cell, int w, int h)
 {
  int x = Math.Clamp((int)((p[0] - minX) / cell), 0, w - 1);
  int z = Math.Clamp((int)((p[2] - minZ) / cell), 0, h - 1);
  return z * w + x;
 }

 protected static float Heuristic(int from, int to, int w, float cell)
 {
  int fromX = from % w;
  int toX = to % w;
  int fromZ = from / w;
  int toZ = to / w;
  float dx = fromX - toX;
  float dz = fromZ - toZ;
  return Math.Sqrt(dx * dx + dz * dz) * cell;
 }

 static void HeapPush(array<int> heap, array<float> key, int node)
 {
  heap.Insert(node);
  int i = heap.Count() - 1;
  while (i > 0)
  {
   int up = (i - 1) / 2;
   if (key[heap[up]] <= key[heap[i]]) break;
   int swap = heap[up];
   heap[up] = heap[i];
   heap[i] = swap;
   i = up;
  }
 }

 // Keys are lowered in place without re-sorting, so pops can come slightly out
 // of order: the route may be a little longer than the shortest, which is fine
 // for 50 m cells. Popped duplicates are skipped as already closed.
 static int HeapPop(array<int> heap, array<float> key)
 {
  int top = heap[0];
  int last = heap[heap.Count() - 1];
  heap.RemoveOrdered(heap.Count() - 1);
  if (heap.IsEmpty()) return top;
  heap[0] = last;
  int i = 0;
  int size = heap.Count();
  while (true)
  {
   int left = 2 * i + 1;
   int right = left + 1;
   int smallest = i;
   if (left < size && key[heap[left]] < key[heap[smallest]]) smallest = left;
   if (right < size && key[heap[right]] < key[heap[smallest]]) smallest = right;
   if (smallest == i) break;
   int swap = heap[smallest];
   heap[smallest] = heap[i];
   heap[i] = swap;
   i = smallest;
  }
  return top;
 }
}
