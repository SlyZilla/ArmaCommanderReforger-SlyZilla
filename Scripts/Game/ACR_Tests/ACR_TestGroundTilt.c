// -acrGroundTiltTest 1 (server-capable, ~1 min): pieces on a slope, level as before and fitted to the
// ground (ACR_GroundTilt), for Jacob's before/after. On a 15-30% slope near the test range that the
// site checks accept, each piece that lies on the ground is spawned through its own spawn path,
// level and then tilted, facing up the slope and across it, and its footprint corners are measured
// against the ground. An MG nest and a guard tower lean only part of the grade (the tower less) and
// sink so their downhill side stands on less air than level ones; a tilted composition's parts tilt
// with it; on flat ground nothing changes. Each piece's numbers are logged as "[ACR] GROUNDTILT ...".
class ACR_TestGroundTilt
{
 static const int CHECKS = 7;
 protected static bool s_bBags, s_bWire, s_bBarricade;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   ACR_TestRegress.Abort("groundtilt", "no Alpha squad");
   return;
  }
  vector home = alpha.GetAnchor().GetOrigin();
  float uphill;
  vector slope = FindGround(home, 0.15, 0.3, uphill);
  float flatYaw;
  vector flat = FindGround(home, 0, 0.01, flatYaw);
  if (slope == vector.Zero || flat == vector.Zero)
  {
   ACR_TestRegress.Abort("groundtilt", string.Format("no test ground (slope %1, flat %2)", slope, flat));
   return;
  }
  PrintFormat("[ACR] GROUNDTILT slope at %1, uphill %2 deg; flat ground at %3", slope, (int)uphill, flat);
  ACR_Fortify.WireLength(slope);

  string bags = Compare(ACR_FobDefences.SANDBAGS_US, slope, uphill, "sandbags");
  string wire = Compare(ACR_Fortify.WIRE, slope, uphill, "wire");
  string barricade = Compare(ACR_Roadblocks.PrefabFor(ACR_ERoadblockType.BARRICADE, "US", ACR_Roadblocks.RoadWidthAt(slope)), slope, uphill, "barricade");
  ACR_TestRegress.Check("groundtilt: a sandbag position lies on the ground on a slope, where level it floats and sinks", s_bBags, bags);
  ACR_TestRegress.Check("groundtilt: a barbed-wire segment lies on the ground on a slope", s_bWire, wire);
  ACR_TestRegress.Check("groundtilt: a barricade lies on the ground on a slope (the roadblock's own spawn)", s_bBarricade, barricade);

  // Upright pieces lean part of the grade, a tower less than a nest, and sink to stand on less air.
  float grade = Grade(slope, uphill);
  ACR_GroundTilt.s_bTrace = true;
  float nestTilt, nestOver, nestUnder, nestLevelOver;
  bool nestMade = Upright(ACR_FobDefences.NEST_US, slope, uphill, nestTilt, nestOver, nestUnder, nestLevelOver);
  float towerTilt, towerOver, towerUnder, towerLevelOver;
  bool towerMade = Upright(ACR_FobDefences.TOWER_US, slope, uphill, towerTilt, towerOver, towerUnder, towerLevelOver);
  ACR_GroundTilt.s_bTrace = false;
  string leanWhy = string.Format("slope %1 deg; nest %2 deg, tower %3 deg", grade, nestTilt, towerTilt);
  PrintFormat("[ACR] GROUNDTILT upright: %1", leanWhy);
  bool leans = nestMade && towerMade && nestTilt > 0.5 && nestTilt <= 5.1 && nestTilt < grade && towerTilt > 0.5 && towerTilt <= 2.6;
  ACR_TestRegress.Check("groundtilt: an MG nest leans part of the grade (at most 5 deg), a guard tower at most 2.5", leans, leanWhy);
  string floatWhy = string.Format("nest on air %1 cm (level %2), buried %3 cm; tower on air %4 cm (level %5), buried %6 cm", Cm(nestOver), Cm(nestLevelOver), Cm(nestUnder), Cm(towerOver), Cm(towerLevelOver), Cm(towerUnder));
  PrintFormat("[ACR] GROUNDTILT upright: %1", floatWhy);
  float buryOk = ACR_GroundTilt.BURY_MAX + 0.05;
  bool sits = nestMade && towerMade && nestOver < nestLevelOver * 0.5 && towerOver < towerLevelOver * 0.5 && buryOk > nestUnder && buryOk > towerUnder;
  ACR_TestRegress.Check("groundtilt: on the slope a nest and a tower stand on under half the air level ones do, buried at most 0.8 m", sits, floatWhy);

  // A tilted composition's parts tilt with it: each keeps the tilt it had to the root when level
  // (some parts are turned or tipped on purpose).
  ACR_GroundTilt.s_bLevel = true;
  IEntity flatBags = ACR_Fortify.SpawnPiece(ACR_FobDefences.SANDBAGS_US, ACR_Manager.Ground(slope), uphill, "US");
  array<float> levelDots = {};
  RelativeUps(flatBags, levelDots);
  Remove(flatBags);
  ACR_GroundTilt.s_bLevel = false;
  IEntity sandbags = ACR_Fortify.SpawnPiece(ACR_FobDefences.SANDBAGS_US, ACR_Manager.Ground(slope), uphill, "US");
  float rootTilt = Tilt(sandbags);
  array<float> tiltedDots = {};
  RelativeUps(sandbags, tiltedDots);
  Remove(sandbags);
  int parts = tiltedDots.Count();
  int off = 0;
  if (levelDots.Count() != parts) off = parts;
  else
  {
   foreach (int d, float dot : tiltedDots)
   {
    if (Math.AbsFloat(dot - levelDots[d]) > 0.0002) off++; // about 1 deg either way
   }
  }
  ACR_TestRegress.Check("groundtilt: a tilted composition's parts tilt with it", rootTilt > 3 && parts > 0 && off == 0, string.Format("root tilt %1 deg, %2 parts, %3 not with it", rootTilt, parts, off));

  // On flat ground nothing changes.
  IEntity level = ACR_Fortify.SpawnPiece(ACR_FobDefences.SANDBAGS_US, ACR_Manager.Ground(flat), flatYaw, "US");
  bool levelMade = level != null;
  float flatTilt = Tilt(level);
  Remove(level);
  ACR_TestRegress.Check("groundtilt: on flat ground a sandbag position stays level", levelMade && flatTilt < 1, string.Format("tilt %1 deg", flatTilt));
  ACR_TestRegress.Complete("groundtilt", CHECKS);
 }

 // A piece spawned level and tilted, facing up the slope and across it, and whether tilting helps:
 // a composition's ground plane against the ground under each of its parts (the worst and the
 // mean), a single piece's footprint corners. Helps: the tilted worst at most half the level one
 // and the mean lower (the tilt is capped at 12 deg, so on a steeper slope some gap is left).
 protected static string Compare(ResourceName prefab, vector at, float uphill, string name)
 {
  float levelWorst = 0;
  float tiltedWorst = 0;
  float levelMean = 0;
  float tiltedMean = 0;
  string detail = "";
  for (int turn = 0; turn < 2; turn++)
  {
   float yaw = uphill + turn * 90;
   ACR_GroundTilt.s_bLevel = true;
   IEntity levelPiece = Spawn(prefab, at, yaw);
   float levelGap, levelAvg;
   bool composition = ACR_GroundTilt.PlaneOff(levelPiece, levelGap, levelAvg);
   if (!composition) levelGap = Worst(levelPiece, prefab);
   else Remove(levelPiece);
   ACR_GroundTilt.s_bLevel = false;
   IEntity tiltedPiece = Spawn(prefab, at, yaw);
   float tiltedGap, tiltedAvg;
   if (!ACR_GroundTilt.PlaneOff(tiltedPiece, tiltedGap, tiltedAvg)) tiltedGap = Worst(tiltedPiece, prefab);
   else Remove(tiltedPiece);
   levelWorst = Math.Max(levelWorst, levelGap);
   tiltedWorst = Math.Max(tiltedWorst, tiltedGap);
   levelMean += levelAvg / 2;
   tiltedMean += tiltedAvg / 2;
   string facing = "up the slope";
   if (turn == 1) facing = "across it";
   string line = string.Format("%1 facing %2: level %3 cm off the ground at worst, tilted %4 cm", name, facing, Cm(levelGap), Cm(tiltedGap));
   if (composition) line += string.Format(" (its plane under its parts; mean %1 / %2 cm)", Cm(levelAvg), Cm(tiltedAvg));
   else line += " (its footprint's corners)";
   PrintFormat("[ACR] GROUNDTILT %1", line);
   if (detail != "") detail += "; ";
   detail += line;
  }
  bool better = tiltedWorst <= levelWorst * 0.5 && tiltedMean <= levelMean;
  if (name == "sandbags") s_bBags = better;
  else if (name == "wire") s_bWire = better;
  else s_bBarricade = better;
  return detail;
 }

 protected static IEntity Spawn(ResourceName prefab, vector at, float yaw)
 {
  vector ground = ACR_Manager.Ground(at);
  if (prefab == ACR_FobDefences.SANDBAGS_US || prefab == ACR_Fortify.WIRE) return ACR_Fortify.SpawnPiece(prefab, ground, yaw, "US");
  // No lane: it moves pieces sideways, and the tilt is measured on the layout as the prefab has it.
  ACR_Roadblock rb = ACR_Roadblocks.SpawnRoadblock("US", ground, yaw, "", ACR_ERoadblockType.BARRICADE, false);
  if (!rb) return null;
  ACR_Roadblocks.s_aRoadblocks.RemoveItem(rb);
  return rb.m_Entity;
 }

 // The worst gap between the piece's footprint corners and the ground (m); removes the piece.
 protected static float Worst(IEntity piece, ResourceName prefab)
 {
  if (!piece) return 99;
  float halfX, halfZ;
  ACR_GroundTilt.Footprint(prefab, halfX, halfZ);
  vector mat[4];
  piece.GetWorldTransform(mat);
  float over, under;
  ACR_GroundTilt.Gaps(mat, halfX, halfZ, over, under);
  Remove(piece);
  return Math.Max(over, under);
 }

 protected static float Tilt(IEntity piece)
 {
  if (!piece) return 99;
  vector mat[4];
  piece.GetWorldTransform(mat);
  return Math.Acos(Math.Clamp(vector.Dot(mat[1], vector.Up), -1, 1)) * Math.RAD2DEG;
 }

 // For each part, the dot of its up with the root's (its tilt to the root).
 protected static void RelativeUps(IEntity piece, notnull array<float> dots)
 {
  if (!piece) return;
  vector root[4];
  piece.GetWorldTransform(root);
  IEntity child = piece.GetChildren();
  while (child)
  {
   vector mat[4];
   child.GetWorldTransform(mat);
   dots.Insert(vector.Dot(mat[1], root[1]));
   child = child.GetSibling();
  }
 }

 // The ground's grade at 'at' up 'uphill' over 6 m, in degrees.
 protected static float Grade(vector at, float uphill)
 {
  vector dir = Vector(Math.Sin(uphill * Math.DEG2RAD), 0, Math.Cos(uphill * Math.DEG2RAD));
  BaseWorld world = GetGame().GetWorld();
  float rise = world.GetSurfaceY(at[0] + dir[0] * 3, at[2] + dir[2] * 3) - world.GetSurfaceY(at[0] - dir[0] * 3, at[2] - dir[2] * 3);
  return Math.Atan2(Math.AbsFloat(rise), 6) * Math.RAD2DEG;
 }

 // An upright piece (Fortify's spawn) on the slope facing up it: its tilt, how far its ground plane
 // stands on air and is buried at worst, and how far a level one stands on air.
 protected static bool Upright(ResourceName prefab, vector at, float uphill, out float tilt, out float over, out float under, out float levelOver)
 {
  float levelUnder;
  ACR_GroundTilt.s_bLevel = true;
  IEntity level = ACR_Fortify.SpawnPiece(prefab, ACR_Manager.Ground(at), uphill, "US");
  bool made = level != null;
  ACR_GroundTilt.Floats(level, prefab, levelOver, levelUnder);
  Remove(level);
  ACR_GroundTilt.s_bLevel = false;
  IEntity piece = ACR_Fortify.SpawnPiece(prefab, ACR_Manager.Ground(at), uphill, "US");
  made = made && piece != null;
  tilt = Tilt(piece);
  ACR_GroundTilt.Floats(piece, prefab, over, under);
  Remove(piece);
  return made;
 }

 protected static int Cm(float m)
 {
  return (int)Math.Round(m * 100);
 }

 protected static void Remove(IEntity piece)
 {
  if (piece) ACR_Navmesh.DeleteStructure(piece);
 }

 // A spot 40-800 m from 'center' whose grade over 6 m in some direction is between minGrade and
 // maxGrade, dry and clear for a 4 m footprint (as a Fortify spot's check); 'uphill' is the yaw
 // up that grade.
 protected static vector FindGround(vector center, float minGrade, float maxGrade, out float uphill)
 {
  uphill = 0;
  BaseWorld world = GetGame().GetWorld();
  for (float r = 40; r <= 800; r += 40)
  {
   for (int k = 0; k < 16; k++)
   {
    float angle = k * 22.5 * Math.DEG2RAD;
    vector p = center + Vector(Math.Sin(angle) * r, 0, Math.Cos(angle) * r);
    if (!ACR_Manager.OnMap(p) || BBB_Terrain.IsWaterAt(p)) continue;
    for (int d = 0; d < 8; d++)
    {
     float yaw = d * 22.5;
     vector dir = Vector(Math.Sin(yaw * Math.DEG2RAD), 0, Math.Cos(yaw * Math.DEG2RAD));
     float rise = world.GetSurfaceY(p[0] + dir[0] * 3, p[2] + dir[2] * 3) - world.GetSurfaceY(p[0] - dir[0] * 3, p[2] - dir[2] * 3);
     float grade = Math.AbsFloat(rise) / 6;
     if (grade < minGrade || grade > maxGrade) continue;
     string why;
     if (!ACR_FobBuild.FootprintClear(p, 4, null, why, true, ACR_Fortify.MAX_RISE)) continue;
     uphill = yaw;
     if (rise < 0) uphill = yaw + 180;
     return ACR_Manager.Ground(p);
    }
   }
  }
  return vector.Zero;
 }
}
