// Structures on sloping ground (Jacob, 26 Oct; 6 Nov). Our structures spawn dead level (yaw only), but
// the site checks accept up to 4 m of rise across a footprint, so on a slope a level piece floats at one
// end and is buried at the other. Every structure follows the grade, each by its own curve per axis
// (Follow): 1:1 for the first few degrees, a share of the grade beyond that, never past a cap. So it
// accounts for the slope and stays usable:
// - LIE: the ground itself, up to 12 deg. Along a wall's length, and both ways for pieces that lie on
//   the ground (sandbag positions, log barricades): cover stays the same height above the ground.
// - LEAN: through a wall's thickness (sandbag lines, wire, road barricades): 1:1 to 3 deg, then 40%, at
//   most 7. A real wall stands upright; over its short depth the gap left is a few cm.
// - FLOOR: tents, buildings, bunkers, gun nests, checkpoints: 1:1 to 2 deg, then 35%, at most 5. Floors
//   stay walkable, a gun's arcs barely move.
// - TALL: guard towers and antennas: 1:1 to 1 deg, then 20%, at most 2.5; a lean shows at the top.
// - PAD: helipads: 1:1 to 2 deg, then half, at most 6; still a landing spot.
// What a floored piece doesn't follow it makes up by sinking into its uphill side, so its downhill
// edge doesn't stand on air (Sink). Called on a piece just spawned level (ACR_Fortify.SpawnPiece,
// ACR_Roadblocks.SpawnRoadblock, ACR_FobSlots.SpawnComposition); a line's walls spawn tilted
// (ACR_BaseDefences.SpawnOnGround, through Transform). Loose dressing (chairs, crates) stays level.
class ACR_GroundTilt
{
 static const int LIE = 0;
 static const int LEAN = 1;
 static const int FLOOR = 2;
 static const int TALL = 3;
 static const int PAD = 4;

 static const float MAX_TILT = 12;     // degrees: the steepest any piece follows (LIE)
 static const float FLOAT_LEFT = 0.05; // m a floored piece's downhill edge may still stand off the ground
 static const float SINK_MAX = 0.5;    // m it sinks at most to get there
 static const float BURY_MAX = 0.8;    // m its uphill edge may go under, the sink included
 static const float NO_TILT = 0.004;   // grade (about 0.25 deg) under which an upright piece stays as built
 static bool s_bLevel;                 // tests: today's level placement, for the before/after
 static bool s_bTrace;                 // tests: log each upright piece's fit

 // -acrGroundTilt 0 (host option): everything level as before, for a before/after in game.
 static bool Enabled() { return !s_bLevel && ACR_LogisticsRules.Choice("acrGroundTilt", "1") != "0"; }

 // Fits 'piece' (spawned level at 'pos', facing 'yaw') to the ground under it, as its kind follows a
 // slope; returns whether it moved.
 static bool Place(IEntity piece, ResourceName prefab, vector pos, float yaw)
 {
  if (!piece || !Enabled()) return false;
  float halfX, halfZ;
  if (!Footprint(prefab, halfX, halfZ)) Bounds(piece, halfX, halfZ);
  int kindX, kindZ;
  Kinds(prefab, halfX, halfZ, kindX, kindZ);
  bool upright = kindX >= FLOOR && kindZ >= FLOOR;
  // The composition's parts first, as they stand level: each keeps its place relative to the root.
  vector level[4];
  piece.GetWorldTransform(level);
  array<IEntity> parts = {};
  array<vector> was = {};
  Parts(piece, parts, was);
  // A composition lies on the ground under its parts (a sandbag position's 26 parts spread well
  // past any footprint guessed for it); a single piece (a wire segment) on its footprint's ends.
  // The plane turns about the middle of the parts, not the root: a guard tower's root stands off to
  // one side of its parts, and turned about it the whole tower stood on air.
  float along, across, height;
  float midX = 0;
  float midZ = 0;
  if (!FitParts(level, was, along, across, height, midX, midZ)) height = Samples(pos, level[0], level[2], halfX, halfZ, along, across);
  float rawAlong = along;
  float rawAcross = across;
  FollowSlope(rawAlong, rawAcross, kindX, kindZ, along, across);
  // Near-flat ground: a building stays as built (and needs no layout record for clients).
  if (upright && Math.AbsFloat(along) < NO_TILT && Math.AbsFloat(across) < NO_TILT) return false;
  vector at = level[3];
  at[1] = height - across * midX - along * midZ;
  vector mat[4];
  Frame(level[0], level[2], along, across, at, mat);
  int n = parts.Count();
  array<vector> moved = {};
  for (int i = 0; i < n; i++)
  {
   for (int k = 0; k < 4; k++) moved.Insert(Remap(was[i * 4 + k], level, mat, k == 3));
  }
  if (upright)
  {
   float sink = Sink(mat, moved, halfX, halfZ);
   if (s_bTrace) PrintFormat("[ACR] GROUNDTILT place %1: %2 parts, grade %3 / %4, followed %5 / %6, sink %7 cm", FilePath.StripPath(prefab), n, rawAlong, rawAcross, along, across, Math.Round(sink * 100));
   mat[3] = mat[3] - vector.Up * sink;
   for (int p = 0; p < n; p++) moved[p * 4 + 3] = moved[p * 4 + 3] - vector.Up * sink;
  }
  piece.SetWorldTransform(mat);
  foreach (int j, IEntity part : parts)
  {
   vector t[4];
   for (int m = 0; m < 4; m++) t[m] = moved[j * 4 + m];
   part.SetWorldTransform(t);
  }
  piece.Update();
  // Every machine lays it out the same way: clients build it level from its prefab.
  ACR_StructureLayout.Record(piece);
  return true;
 }

 // How far a floored piece with transform 'mat' (its parts at 'moved', 4 vectors each) sinks: enough
 // that its downhill edge stands at most FLOAT_LEFT off the ground, at most SINK_MAX, and never so
 // far that its uphill edge goes more than BURY_MAX under.
 protected static float Sink(vector mat[4], array<vector> moved, float halfX, float halfZ)
 {
  float over, under;
  int n = moved.Count() / 4;
  if (n == 0 || !PlaneGaps(mat, moved, over, under)) Gaps(mat, halfX, halfZ, over, under);
  if (s_bTrace) PrintFormat("[ACR] GROUNDTILT sink: on air %1 cm, buried %2 cm before it", Math.Round(over * 100), Math.Round(under * 100));
  float sink = Math.Clamp(over - FLOAT_LEFT, 0, SINK_MAX);
  return Math.Min(sink, Math.Max(0, BURY_MAX - under));
 }

 // The slope a piece follows along its facing and across it, from the ground's ('rawAlong',
 // 'rawAcross', rise per m). A piece that follows both ways alike follows the steepest slope, its cap
 // on its whole lean; a wall each way by its own kind.
 static void FollowSlope(float rawAlong, float rawAcross, int kindX, int kindZ, out float along, out float across)
 {
  along = Follow(rawAlong, kindZ);
  across = Follow(rawAcross, kindX);
  if (kindX != kindZ) return;
  float steepest = Math.Sqrt(rawAlong * rawAlong + rawAcross * rawAcross);
  if (steepest < 0.0001) return;
  float scale = Follow(steepest, kindX) / steepest;
  along = rawAlong * scale;
  across = rawAcross * scale;
 }

 // A slope (rise per m) as a piece of 'kind' follows it: 1:1 up to its 'full' angle, its 'share' of
 // the grade beyond, at most its cap.
 static float Follow(float grade, int kind)
 {
  float full, share, cap;
  FollowCurve(kind, full, share, cap);
  float deg = Math.Atan2(Math.AbsFloat(grade), 1) * Math.RAD2DEG;
  float follow = deg;
  if (deg > full) follow = full + (deg - full) * share;
  follow = Math.Min(follow, cap);
  float t = Math.Tan(follow * Math.DEG2RAD);
  if (grade < 0) return -t;
  return t;
 }

 // Each kind's curve, in degrees: followed 1:1 up to 'full', then 'share' of the rest, at most 'cap'.
 static void FollowCurve(int kind, out float full, out float share, out float cap)
 {
  full = MAX_TILT;
  share = 0;
  cap = MAX_TILT;
  if (kind == LEAN)
  {
   full = 3;
   share = 0.4;
   cap = 7;
  }
  else if (kind == FLOOR)
  {
   full = 2;
   share = 0.35;
   cap = 5;
  }
  else if (kind == TALL)
  {
   full = 1;
   share = 0.2;
   cap = 2.5;
  }
  else if (kind == PAD)
  {
   full = 2;
   share = 0.5;
   cap = 6;
  }
 }

 // How a piece follows the grade across it (local X, 'kindX') and along its facing (local Z, 'kindZ').
 static void Kinds(ResourceName prefab, float halfX, float halfZ, out int kindX, out int kindZ)
 {
  kindX = FLOOR;
  kindZ = FLOOR;
  if (prefab == ACR_FobDefences.TOWER_US || prefab == ACR_FobDefences.TOWER_USSR || prefab == ACR_FobSlots.RELAY_US || prefab == ACR_FobSlots.RELAY_USSR || prefab == ACR_FobSlots.RELAY_FIA)
  {
   kindX = TALL;
   kindZ = TALL;
  }
  else if (prefab == ACR_FobSlots.HELIPAD_US || prefab == ACR_FobSlots.HELIPAD_USSR || prefab == ACR_FobSlots.HELIPAD_FIA)
  {
   kindX = PAD;
   kindZ = PAD;
  }
  else if (prefab == ACR_FobDefences.SANDBAGS_US || prefab == ACR_FobDefences.SANDBAGS_USSR || prefab == ACR_FobDefences.SANDBAGS_FIA || prefab == ACR_Roadblocks.LOGS_1 || prefab == ACR_Roadblocks.LOGS_2 || prefab == ACR_Roadblocks.LOGS_3)
  {
   kindX = LIE;
   kindZ = LIE;
  }
  else if (Footprint(prefab, halfX, halfZ)) Wall(halfX, halfZ, kindX, kindZ); // wire, road barricades
 }

 // A wall lies along its length and leans a little through its depth.
 static void Wall(float halfX, float halfZ, out int kindX, out int kindZ)
 {
  kindX = LIE;
  kindZ = LEAN;
  if (halfZ > halfX)
  {
   kindX = LEAN;
   kindZ = LIE;
  }
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

 // Any other piece's footprint: its model's bounds, at least 0.5 m either way.
 protected static void Bounds(IEntity piece, out float halfX, out float halfZ)
 {
  vector mins, maxs;
  piece.GetBounds(mins, maxs);
  halfX = Math.Max(0.5, (maxs[0] - mins[0]) * 0.5);
  halfZ = Math.Max(0.5, (maxs[2] - mins[2]) * 0.5);
 }

 // A wall's transform at 'pos' facing 'yaw', from the ground at the ends of its footprint: across
 // (+-halfX) and along (+-halfZ); it lies along its length and leans a little through its depth
 // (Wall). The origin sits at the mean of the samples.
 static void Transform(vector pos, float yaw, float halfX, float halfZ, out vector mat[4])
 {
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
  vector right = mat[0];
  vector forward = mat[2];
  float along, across;
  vector at = pos;
  at[1] = Samples(pos, right, forward, halfX, halfZ, along, across);
  int kindX, kindZ;
  Wall(halfX, halfZ, kindX, kindZ);
  Frame(right, forward, Follow(along, kindZ), Follow(across, kindX), at, mat);
 }

 // The ground's slope along 'forward' and across 'right' from four samples at the footprint's ends;
 // returns their mean height.
 protected static float Samples(vector pos, vector right, vector forward, float halfX, float halfZ, out float along, out float across)
 {
  BaseWorld world = GetGame().GetWorld();
  float front = Ground(world, pos + forward * halfZ);
  float back = Ground(world, pos - forward * halfZ);
  float rightY = Ground(world, pos + right * halfX);
  float leftY = Ground(world, pos - right * halfX);
  along = (front - back) / (2 * halfZ);
  across = (rightY - leftY) / (2 * halfX);
  return (front + back + rightY + leftY) * 0.25;
 }

 // The plane through the ground under the composition's parts as they stand level (least squares
 // in the piece's own frame, height = a + b * across + c * along): its slopes, the middle of the
 // parts in that frame ('midX' across, 'midZ' along) and the plane's height there; false with fewer
 // than three parts or parts in a line.
 protected static bool FitParts(vector level[4], array<vector> was, out float along, out float across, out float height, out float midX, out float midZ)
 {
  along = 0;
  across = 0;
  midX = 0;
  midZ = 0;
  height = level[3][1];
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
  along = c;
  across = b;
  midX = sx / n;
  midZ = sz / n;
  height = level[3][1] + a + b * midX + c * midZ;
  return true;
 }

 // The frame whose ground plane rises 'along' (per m) to the front and 'across' (per m) to the
 // right of the level 'right' / 'forward' (already followed as the piece's kind), its origin at 'at'.
 protected static void Frame(vector right, vector forward, float along, float across, vector at, out vector mat[4])
 {
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
   float off = Math.AbsFloat(PlaneY(mat, at[i * 4 + 3]) - Ground(world, at[i * 4 + 3]));
   worst = Math.Max(worst, off);
   mean += off / n;
  }
  return true;
 }

 // A piece's ground plane against the ground under its parts: how far it stands above it at most
 // ('over', a floor on air) and below it ('under', buried); with no parts, at its footprint's corners.
 static void Floats(IEntity piece, ResourceName prefab, out float over, out float under)
 {
  over = 0;
  under = 0;
  if (!piece) return;
  vector mat[4];
  piece.GetWorldTransform(mat);
  array<IEntity> parts = {};
  array<vector> at = {};
  Parts(piece, parts, at);
  if (PlaneGaps(mat, at, over, under)) return;
  float halfX, halfZ;
  if (!Footprint(prefab, halfX, halfZ)) Bounds(piece, halfX, halfZ);
  Gaps(mat, halfX, halfZ, over, under);
 }

 // The plane through 'mat''s origin with normal mat[1] against the ground at each part's origin
 // ('transforms', 4 vectors a part): the most it stands above and below; false with no parts.
 protected static bool PlaneGaps(vector mat[4], array<vector> transforms, out float over, out float under)
 {
  over = 0;
  under = 0;
  int n = transforms.Count() / 4;
  if (n == 0 || Math.AbsFloat(mat[1][1]) < 0.01) return false;
  BaseWorld world = GetGame().GetWorld();
  for (int i = 0; i < n; i++)
  {
   vector p = transforms[i * 4 + 3];
   float gap = PlaneY(mat, p) - Ground(world, p);
   if (gap > over) over = gap;
   if (-gap > under) under = -gap;
  }
  return true;
 }

 // The height of 'mat''s ground plane (through its origin, normal mat[1]) at p's x / z.
 protected static float PlaneY(vector mat[4], vector p)
 {
  return mat[3][1] - (mat[1][0] * (p[0] - mat[3][0]) + mat[1][2] * (p[2] - mat[3][2])) / mat[1][1];
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
