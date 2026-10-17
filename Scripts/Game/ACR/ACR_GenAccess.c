// Startup-only land connectivity for generated objectives. Unlike a straight-line distance,
// this detects peninsulas whose only approach passes through the opposing HQ's perimeter.
// The coarse terrain graph is cached per survey; it is not tactical AI/navmesh pathfinding.
class ACR_GenAccess
{
 static const float CELL = 100;
 static const float HQ_CLEARANCE = 350;
 protected float m_fX, m_fZ, m_fCell;
 protected int m_iW, m_iH;
 protected ref array<int> m_Land = {};
 protected ref array<int> m_Links = {};
 protected ref array<int> m_DX = {1, -1, 0, 0, 1, 1, -1, -1};
 protected ref array<int> m_DZ = {0, 0, 1, -1, 1, -1, 1, -1};

 void Build(array<ref ACR_GenSite> sites)
 {
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
 }

 protected vector Position(int index)
 {
  int x = index % m_iW;
  int z = index / m_iW;
  return Vector(m_fX + m_fCell * x, 0, m_fZ + m_fCell * z);
 }

 protected bool DryLeg(vector a, vector b)
 {
  for (int s = 1; s < 4; s++)
  {
   vector p = a + (b - a) * (0.25 * s);
   if (BBB_Terrain.IsWaterAt(p)) return false;
  }
  return true;
 }

 protected int Nearby(vector at, array<int> reached = null)
 {
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
   float d = vector.DistanceXZ(at, point);
   if (d < distance && DryLeg(at, point)) { distance = d; best = index; }
  }
  return best;
 }

 protected int Links(int index)
 {
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
  reached.Clear(); reached.Resize(m_Land.Count());
  int start = Nearby(from);
  if (start < 0) return;
  ref array<int> queue = {start};
  reached[start] = 1;
  for (int head = 0; head < queue.Count(); head++)
  {
   int current = queue[head];
   int x = current % m_iW, z = current / m_iW;
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
  int index = Nearby(at, reached);
  return index >= 0 && OutsideHQ(at, Position(index), opposingHQ, clearance);
 }
}
