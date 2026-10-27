// Pieces on sloping ground (Jacob, 26 Oct). Our structures spawn dead level (yaw only), but the
// site checks accept up to 4 m of rise across a footprint, so on a slope that passes one end of a
// piece floats while the other is buried. A piece that should lie on the ground (sandbag positions,
// barbed wire, barricades and log barricades) is tilted to the plane through four ground samples at
// the ends of its footprint, at most MAX_TILT; tents, buildings, bunkers, towers, checkpoints and
// gun nests stay upright (a floor or a gun's arcs want level). Called on a piece just spawned level
// (ACR_Fortify.SpawnPiece, ACR_Roadblocks.SpawnRoadblock, ACR_FobSlots.SpawnComposition).
class ACR_GroundTilt
{
 static const float MAX_TILT = 12; // degrees off level, along and across
 static bool s_bLevel;             // tests: today's level placement, for the before/after

 // -acrGroundTilt 0 (host option): everything level as before, for a before/after in game.
 static bool Enabled() { return !s_bLevel && ACR_LogisticsRules.Choice("acrGroundTilt", "1") != "0"; }

 // Tilts 'piece' (spawned level at 'pos', facing 'yaw') to the ground under it if its prefab is
 // one that lies on the ground; returns whether it did.
 static bool Place(IEntity piece, ResourceName prefab, vector pos, float yaw)
 {
  float halfX, halfZ;
  if (!piece || !Enabled() || !Footprint(prefab, halfX, halfZ)) return false;
  // The composition's parts first, as they stand level: each keeps its place relative to the root.
  vector level[4];
  piece.GetWorldTransform(level);
  array<IEntity> parts = {};
  array<vector> was = {};
  Parts(piece, parts, was);
  // A composition lies on the ground under its parts (a sandbag position's 26 parts spread well
  // past any footprint guessed for it); a single piece (a wire segment) on its footprint's ends.
  vector mat[4];
  if (!FitParts(level, was, mat)) Transform(pos, yaw, halfX, halfZ, mat);
  piece.SetWorldTransform(mat);
  foreach (int i, IEntity part : parts)
  {
   vector moved[4];
   for (int k = 0; k < 4; k++) moved[k] = Remap(was[i * 4 + k], level, mat, k == 3);
   part.SetWorldTransform(moved);
  }
  piece.Update();
  return true;
 }

 // Every part under 'piece' (children, their children), with its world transform (4 vectors each).
 protected static void Parts(IEntity piece, notnull array<IEntity> parts, notnull array<vector> transforms)
 {
  IEntity child = piece.GetChildren();
  while (child)
  {
   vector t[4];
   child.GetWorldTransform(t);
   parts.Insert(child);
   for (int k = 0; k < 4; k++) transforms.Insert(t[k]);
   Parts(child, parts, transforms);
   child = child.GetSibling();
  }
 }

 // A part's axis (or, with 'point', its origin) in the level root's frame, put back in the tilted one's.
 protected static vector Remap(vector v, vector from[4], vector to[4], bool point)
 {
  vector d = v;
  if (point) d = v - from[3];
  float x = vector.Dot(d, from[0]);
  float y = vector.Dot(d, from[1]);
  float z = vector.Dot(d, from[2]);
  vector r = to[0] * x + to[1] * y + to[2] * z;
  if (point) r = r + to[3];
  return r;
 }

 // The pieces that lie on the ground, and the half sizes of their footprint across (local X) and
 // along their facing (local Z).
 static bool Footprint(ResourceName prefab, out float halfX, out float halfZ)
 {
  halfX = 0;
  halfZ = 0;
  if (prefab == ACR_FobDefences.SANDBAGS_US || prefab == ACR_FobDefences.SANDBAGS_USSR || prefab == ACR_FobDefences.SANDBAGS_FIA)
  {
   halfX = 1.6;
   halfZ = 1;
   return true;
  }
  if (prefab == ACR_Fortify.WIRE)
  {
   float half = ACR_Fortify.WireLength(vector.Zero) * 0.5;
   halfX = 0.5;
   halfZ = half;
   if (ACR_Fortify.WireAlongX())
   {
    halfX = half;
    halfZ = 0.5;
   }
   return true;
  }
  if (prefab == ACR_Roadblocks.BARRICADE_S_US || prefab == ACR_Roadblocks.BARRICADE_S_USSR) halfX = 3;
  else if (prefab == ACR_Roadblocks.BARRICADE_M_US || prefab == ACR_Roadblocks.BARRICADE_M_USSR) halfX = 4.5;
  else if (prefab == ACR_Roadblocks.BARRICADE_L_US || prefab == ACR_Roadblocks.BARRICADE_L_USSR) halfX = 6;
  else if (prefab == ACR_Roadblocks.LOGS_1) halfX = 2.5;
  else if (prefab == ACR_Roadblocks.LOGS_2) halfX = 4;
  else if (prefab == ACR_Roadblocks.LOGS_3) halfX = 5.5;
  else return false;
  halfZ = 1;
  return true;
 }

 // The transform at 'pos' facing 'yaw' whose ground plane passes through the surface at the ends
 // of the footprint: across (+-halfX) and along (+-halfZ). Each slope is capped at MAX_TILT; the
 // origin sits at the mean of the samples.
 static void Transform(vector pos, float yaw, float halfX, float halfZ, out vector mat[4])
 {
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
  BaseWorld world = GetGame().GetWorld();
  vector right = mat[0];
  vector forward = mat[2];
  float front = Ground(world, pos + forward * halfZ);
  float back = Ground(world, pos - forward * halfZ);
  float rightY = Ground(world, pos + right * halfX);
  float leftY = Ground(world, pos - right * halfX);
  vector at = pos;
  at[1] = (front + back + rightY + leftY) * 0.25;
  Frame(right, forward, (front - back) / (2 * halfZ), (rightY - leftY) / (2 * halfX), at, mat);
 }

 // The plane through the ground under the composition's parts as they stand level (least squares
 // in the piece's own frame, height = a + b * across + c * along), and the transform on it; false
 // with fewer than three parts or parts in a line.
 protected static bool FitParts(vector level[4], array<vector> was, out vector mat[4])
 {
  int n = was.Count() / 4;
  if (n < 3) return false;
  BaseWorld world = GetGame().GetWorld();
  float sx = 0;
  float sz = 0;
  float sh = 0;
  float sxx = 0;
  float szz = 0;
  float sxz = 0;
  float sxh = 0;
  float szh = 0;
  for (int i = 0; i < n; i++)
  {
   vector p = was[i * 4 + 3];
   vector d = p - level[3];
   float x = vector.Dot(d, level[0]);
   float z = vector.Dot(d, level[2]);
   float h = world.GetSurfaceY(p[0], p[2]) - level[3][1];
   sx += x;
   sz += z;
   sh += h;
   sxx += x * x;
   szz += z * z;
   sxz += x * z;
   sxh += x * h;
   szh += z * h;
  }
  float det = n * (sxx * szz - sxz * sxz) - sx * (sx * szz - sxz * sz) + sz * (sx * sxz - sxx * sz);
  if (Math.AbsFloat(det) < 0.001) return false;
  float a = (sh * (sxx * szz - sxz * sxz) - sx * (sxh * szz - sxz * szh) + sz * (sxh * sxz - sxx * szh)) / det;
  float b = (n * (sxh * szz - sxz * szh) - sh * (sx * szz - sxz * sz) + sz * (sx * szh - sxh * sz)) / det;
  float c = (n * (sxx * szh - sxh * sxz) - sx * (sx * szh - sxh * sz) + sh * (sx * sxz - sxx * sz)) / det;
  vector at = level[3];
  at[1] = level[3][1] + a;
  Frame(level[0], level[2], c, b, at, mat);
  return true;
 }

 // The frame whose ground plane rises 'along' (per m) to the front and 'across' (per m) to the
 // right of the level 'right' / 'forward', each capped at MAX_TILT, its origin at 'at'.
 protected static void Frame(vector right, vector forward, float along, float across, vector at, out vector mat[4])
 {
  float cap = Math.Tan(MAX_TILT * Math.DEG2RAD);
  along = Math.Clamp(along, -cap, cap);
  across = Math.Clamp(across, -cap, cap);
  vector tiltedForward = forward + vector.Up * along;
  vector tiltedRight = right + vector.Up * across;
  vector up = Cross(tiltedForward, tiltedRight);
  up.Normalize();
  vector newRight = Cross(up, tiltedForward);
  newRight.Normalize();
  vector newForward = Cross(newRight, up);
  newForward.Normalize();
  mat[0] = newRight;
  mat[1] = up;
  mat[2] = newForward;
  mat[3] = at;
 }

 // How far the ground plane of a piece with transform 'mat' is from the ground under each of its
 // parts (m): the worst and the mean; false with no parts.
 static bool PlaneOff(IEntity piece, out float worst, out float mean)
 {
  worst = 0;
  mean = 0;
  if (!piece) return false;
  vector mat[4];
  piece.GetWorldTransform(mat);
  array<IEntity> parts = {};
  array<vector> at = {};
  Parts(piece, parts, at);
  int n = parts.Count();
  if (n == 0 || Math.AbsFloat(mat[1][1]) < 0.01) return false;
  BaseWorld world = GetGame().GetWorld();
  for (int i = 0; i < n; i++)
  {
   vector p = at[i * 4 + 3];
   // The plane through the origin with normal mat[1], at this part's x / z.
   float planeY = mat[3][1] - (mat[1][0] * (p[0] - mat[3][0]) + mat[1][2] * (p[2] - mat[3][2])) / mat[1][1];
   float off = Math.AbsFloat(planeY - world.GetSurfaceY(p[0], p[2]));
   worst = Math.Max(worst, off);
   mean += off / n;
  }
  return true;
 }

 // How far the piece's footprint corners are off the ground under them (floating above it: 'over';
 // buried: 'under'), for a piece with transform 'mat' and the given half sizes.
 static void Gaps(vector mat[4], float halfX, float halfZ, out float over, out float under)
 {
  over = 0;
  under = 0;
  BaseWorld world = GetGame().GetWorld();
  for (int i = 0; i < 4; i++)
  {
   float sx = -1;
   if (i == 1 || i == 2) sx = 1;
   float sz = -1;
   if (i >= 2) sz = 1;
   vector corner = mat[3] + mat[0] * (halfX * sx) + mat[2] * (halfZ * sz);
   float gap = corner[1] - Ground(world, corner);
   if (gap > over) over = gap;
   if (-gap > under) under = -gap;
  }
 }

 protected static float Ground(BaseWorld world, vector at)
 {
  return world.GetSurfaceY(at[0], at[2]);
 }

 protected static vector Cross(vector a, vector b)
 {
  return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
 }
}
