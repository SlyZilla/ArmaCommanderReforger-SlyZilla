// Wings: sandbags running out from the ends of a small work, the same pieces and the same walker as a base's
// lines (ACR_BaseLiner), so each piece fits the ground, keeps off roads, doors and buildings and sees out. A
// Fortify position or a checkpoint on its own is a blob to walk round; a wing makes it part of the ground. The
// owner's idea of 31 Oct 2026: works built from parts, fitted to where they stand.
//   TIE     to the nearest building, wall, rock or work within REACH, closing that side.
//   REFUSE  bends well back and ends in a tail, covering the flank.
//   RUN_ON  on along the line, only a little back, then a tail.
// Each wing is the ground's call, weighted among the near-best and seeded (ACR_Variety): what is near to tie
// into, how open the flank is, how far back there's room. Its length and bend are drawn the same way. A wing
// that can't be walked goes for its next best; none at all is fine, the work stands as before.
// Guns placed later take the place of the bags they stand on (Cut). -acrWings 0 turns them off.
class ACR_WingPlan : Managed
{
 ref array<ref ACR_LinePiece> m_aPieces = {};
 string m_sEnd;   // what it ends on (as ACR_BaseLine.m_sEndA), "curve" a free end, "" no wing
 int m_iShape = -1;
 string m_sWhyNot; // the walks that came to nothing, for the log
}

// A piece that made way for a gun, to go back if the gun doesn't stay.
class ACR_WingCut : Managed
{
 ResourceName m_sPrefab;
 vector m_vPos;
 vector m_vAngles;
}

class ACR_Wings
{
 static const int TIE = 0;
 static const int REFUSE = 1;
 static const int RUN_ON = 2;
 static const float NEAR_SCORE = 0.25;  // shapes this far off the best stay in the draw
 static const float RADIUS = 60;        // m looked round a work
 static const float REACH = 30;         // m a wing looks for something to tie into
 static const float REACH_SMALL = 20;   // ... a checkpoint's
 static const int MOST = 9;             // long pieces a wing that ties runs to, about 30 m
 static const int MOST_SMALL = 6;       // ... a checkpoint's
 static const int FEW = 2;              // a free wing with fewer isn't worth building
 static const int GAP_FROM = 7;         // a tied wing this long leaves a gap for men on foot ...
 static const float GAP_AT = 0.6;       // ... this far along
 static const int MEN_LEAST = 3;        // long pieces standing before a reserve goes behind them
 static const float GUN_ROOM = 2.6;     // m round a gun or post its bags make way
 static const float GUN_FIRE_HALF = 1.6; // m either side of a gun's line of fire, out to ACR_Fortify.FIRE_CLEAR
 static int s_iWorstMs;                 // tests: the most a work's wings have cost to plan
 protected static ref ACR_BaseWork s_NoWork;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_NoWork = new ACR_BaseWork();
 }
 protected static int s_iStop;

 static bool On()
 {
  return ACR_LogisticsRules.Choice("acrWings", "1") != "0";
 }

 // A position's reserve out and wings standing: each man to his own place behind the bags, from the position
 // outwards, so the wings are held and not only there to look at. False: too little wing, they do what they did.
 // -acrWingMen 0 leaves them round the position.
 static bool ManWings(ACR_DefenderPost post, int men)
 {
  ACR_Fortification fort = post.m_Fort;
  if (!fort || !post.m_Group || men <= 0 || ACR_LogisticsRules.Choice("acrWingMen", "1") == "0") return false;
  array<ACR_LinePiece> pieces = {};
  foreach (ACR_WingPlan plan : fort.m_aWingPlans)
  {
   foreach (ACR_LinePiece piece : plan.m_aPieces)
    if (piece.m_bLong && PieceAt(fort, piece.m_vPos)) pieces.Insert(piece);
  }
  if (pieces.Count() < MEN_LEAST) return false;
  array<ref ACR_GarrisonSpot> spots = {};
  while (spots.Count() < men && !pieces.IsEmpty())
  {
   int nearest = 0;
   foreach (int p, ACR_LinePiece candidate : pieces)
    if (vector.DistanceXZ(candidate.m_vPos, fort.m_vPos) < vector.DistanceXZ(pieces[nearest].m_vPos, fort.m_vPos)) nearest = p;
   ACR_LinePiece piece = pieces[nearest];
   pieces.Remove(nearest);
   spots.Insert(ACR_BagHold.SpotBehind(piece.m_vPos, piece.m_vFacing, piece.m_sPrefab, PieceAt(fort, piece.m_vPos)));
  }
  float radius;
  if (ACR_Garrison.PostsAt(post.m_Group, spots, fort.m_vPos, radius) <= 0) return false;
  ACR_Squad.FightLikeSquad(post.m_Group);
  ACR_Guerrillas.Order(post.m_Group, ACR_Squad.WP_GARRISON, fort.m_vPos, radius, true);
  post.m_bOnWings = true;
  PrintFormat("[ACR] WINGS %1: %2 men behind the bags (%3 places)", post.m_sName, men, spots.Count());
  return true;
 }

 protected static IEntity PieceAt(ACR_Fortification fort, vector at)
 {
  foreach (IEntity wing : fort.m_aWings)
   if (wing && vector.DistanceXZ(wing.GetOrigin(), at) < 0.5) return wing;
  return null;
 }

 static string ShapeWord(int shape)
 {
  switch (shape)
  {
   case TIE: return "ties in";
   case REFUSE: return "refused";
   case RUN_ON: return "runs on";
  }
  return "none";
 }

 // A wing from 'tip', going 'heading', its firing side given by 'sign' (ACR_BaseLiner.FacingOf). No pieces if
 // none will do. 'small': a checkpoint's, shorter than a position's.
 static ACR_WingPlan Plan(ACR_BaseSurvey survey, string faction, vector tip, vector heading, int sign, string key, bool small = false)
 {
  ACR_WingPlan wing = new ACR_WingPlan();
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(faction);
  RandomGenerator rng = ACR_Variety.For(key);
  if (!FirstPlace(survey, tip, heading, sign))
  {
   wing.m_sWhyNot = "no room for a first piece";
   return wing;
  }
  vector anchor;
  string word;
  float reach = REACH;
  if (small) reach = REACH_SMALL;
  bool near = survey.NearestAnchor(tip, heading, s_NoWork, reach, anchor, word);
  vector eye = tip;
  eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_BAGS;
  float along = ACR_BaseDefences.Bearing(heading);
  float back = ACR_BaseDefences.Bearing(ACR_BaseLiner.TurnedBack(heading, sign, 35));
  array<float> scores = {-1, 0.55 + 0.35 * ACR_BaseSurvey.Reach(eye, back, 20) / 20, 0.45 + 0.45 * ACR_BaseSurvey.Reach(eye, along, 30) / 30};
  if (near) scores[TIE] = 1.2 - 0.4 * vector.DistanceXZ(tip, anchor) / reach;
  for (int tries = 0; tries < 3; tries++)
  {
   int shape = ACR_Variety.Pick(scores, string.Format("%1|shape%2", key, tries), NEAR_SCORE);
   if (shape < 0 || scores[shape] < 0) break;
   scores[shape] = -1;
   if (Walked(survey, kit, tip, heading, sign, shape, anchor, word, rng, small, wing)) break;
   wing.m_sWhyNot += string.Format("%1 %2 pieces %3 ", ShapeWord(shape), wing.m_aPieces.Count(), ACR_BaseLiner.FitWord(s_iStop));
   wing.m_aPieces.Clear();
   wing.m_iShape = -1;
   wing.m_sEnd = "";
  }
  return wing;
 }

 // Off the end of the work, or as near as it goes: a road's verge there pushes the start out.
 protected static bool FirstPlace(ACR_BaseSurvey survey, inout vector tip, vector heading, int sign)
 {
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, s_NoWork, ACR_BaseDefences.Bearing(tip - survey.m_vCenter));
  array<float> steps = {0, 0.5, 1, 2, 3, 4.5, 6};
  foreach (float step : steps)
  {
   vector at = tip + heading * step;
   at[1] = ACR_BaseLiner.Ground(at);
   if (liner.Fit(at, heading, ACR_BaseLines.LONG_STEP, sign, false, true) != ACR_BaseLiner.FIT_OK) continue;
   tip = at;
   return true;
  }
  return false;
 }

 protected static bool Walked(ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, vector tip, vector heading, int sign, int shape, vector anchor, string word, RandomGenerator rng, bool small, ACR_WingPlan wing)
 {
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, s_NoWork, ACR_BaseDefences.Bearing(tip - survey.m_vCenter));
  array<vector> lanes = {};
  array<vector> laneOut = {};
  liner.m_bTarget = true;
  if (shape == TIE)
  {
   liner.m_vTarget = anchor;
   liner.m_sTargetWord = word;
   int most = MOST;
   if (small) most = MOST_SMALL;
   string ended = liner.Walk(tip, heading, sign, most, wing.m_aPieces, lanes, laneOut, kit);
   s_iStop = liner.m_iStop;
   if (ended.IsEmpty() || wing.m_aPieces.IsEmpty()) return false;
   if (StopAtBreak(wing)) ended = "road";
   wing.m_sEnd = ended;
   wing.m_iShape = shape;
   // Both its ends stand on something: men on foot get through.
   if (wing.m_aPieces.Count() >= GAP_FROM) wing.m_aPieces.RemoveOrdered((int)(wing.m_aPieces.Count() * GAP_AT));
   return true;
  }
  float turn = 25 + rng.RandFloat01() * 25;
  int count = 3 + rng.RandInt(0, 3);
  if (shape == RUN_ON)
  {
   turn = 3 + rng.RandFloat01() * 12;
   count = 4 + rng.RandInt(0, 4);
  }
  if (small) count = 2 + rng.RandInt(0, 2);
  // A point well past where it will stop, so that it's the count that stops it.
  liner.m_vTarget = tip + ACR_BaseLiner.TurnedBack(heading, sign, turn) * ((count + 4) * ACR_BaseLines.LONG_STEP);
  liner.m_sTargetWord = "curve";
  string stopped = liner.Walk(tip, heading, sign, count, wing.m_aPieces, lanes, laneOut, kit);
  s_iStop = liner.m_iStop;
  if (StopAtBreak(wing)) stopped = "road";
  if (wing.m_aPieces.Count() < FEW) return false;
  wing.m_iShape = shape;
  wing.m_sEnd = stopped;
  if (!stopped.IsEmpty()) return true;
  wing.m_sEnd = "curve";
  return liner.CurveBack(wing.m_aPieces, sign, kit, tip, heading) && wing.m_aPieces.Count() >= FEW;
 }

 // A wing doesn't go on across a road or a gap. Whether it was cut.
 protected static bool StopAtBreak(ACR_WingPlan wing)
 {
  foreach (int i, ACR_LinePiece piece : wing.m_aPieces)
  {
   if (piece.m_iBreak == 0) continue;
   piece.m_iBreak = 0;
   while (wing.m_aPieces.Count() > i + 1) wing.m_aPieces.Remove(wing.m_aPieces.Count() - 1);
   return true;
  }
  return false;
 }

 static void Spawn(ACR_WingPlan wing, notnull array<IEntity> pieces)
 {
  foreach (ACR_LinePiece piece : wing.m_aPieces)
  {
   IEntity entity = ACR_BaseDefences.SpawnOnGround(piece.m_sPrefab, piece.m_vPos, piece.m_fYaw);
   if (!entity) continue;
   pieces.Insert(entity);
   ACR_Navmesh.RebuildAfterSpawn(entity);
  }
 }

 static void Remove(notnull array<IEntity> pieces)
 {
  foreach (IEntity piece : pieces) if (piece) ACR_Navmesh.DeleteStructure(piece);
  pieces.Clear();
 }

 // Where a wing starts on a side (1 its local +X, -1 -X) of a composition: just off the end of its bags,
 // going straight on along it. False if it has no bags to go by.
 static bool EndOf(IEntity work, int side, out vector tip, out vector heading)
 {
  vector root[4];
  work.GetWorldTransform(root);
  array<IEntity> units = {};
  ACR_RoadblockLane.Units(work, units);
  bool found;
  float end, endZ;
  foreach (IEntity unit : units)
  {
   float x0, x1, z0, z1;
   ACR_RoadblockLane.Footprint(unit, root, x0, x1, z0, z1);
   if (x1 - x0 < 0.05 && z1 - z0 < 0.05) continue;
   if (!Solid(unit)) continue;
   float x = x1;
   if (side < 0) x = -x0;
   if (found && x <= end) continue;
   found = true;
   end = x;
   endZ = 0.5 * (z0 + z1);
  }
  if (!found) return false;
  vector across = Vector(root[0][0], 0, root[0][2]);
  across.Normalize();
  vector along = Vector(root[2][0], 0, root[2][2]);
  along.Normalize();
  float way = side;
  heading = across * way;
  tip = root[3] + across * (way * (end + 0.1)) + along * endZ;
  tip[1] = ACR_BaseLiner.Ground(tip);
  return true;
 }

 // Bags, walls and the like: not the chair, the ammo box or a patrol point beside them.
 protected static bool Solid(IEntity unit)
 {
  string name = ACR_RoadblockLane.PrefabName(unit);
  return name.Contains("Sandbag") || name.Contains("Wall") || name.Contains("Barricade") || name.Contains("Log") || name.Contains("Hedgehog") || name.Contains("Dragon");
 }

 // ---------------------------------------------------------------- Fortify positions

 // A new position's wings, off its two end posts, when its other pieces stand.
 static void ForPosition(ACR_Fortification fort)
 {
  if (!On()) return;
  // Off the outermost post standing on each side: a blocked spot is skipped when a position goes in.
  IEntity left = fort.m_aPieces[0];
  IEntity right = fort.m_aPieces[1];
  IEntity middle = fort.m_aPieces[2];
  if (!left) left = middle;
  if (!right) right = middle;
  if (!left) left = right;
  if (!right) right = left;
  Both(left, right, fort.m_vPos, fort.m_sFaction, false, fort.m_aWings, fort.m_aWingPlans, "position");
 }

 // A checkpoint's, off the ends of its pieces either side of the road. They start past those and go away from
 // the road, so the lane (ACR_RoadblockLane) stays the way through. Called where a squad or the quick start has
 // built one; a base's own checkpoints stand in its lines, an ambush's barricade stands alone.
 static void ForRoadblock(ACR_Roadblock rb)
 {
  if (!On() || !rb || !rb.m_Entity) return;
  Both(rb.m_Entity, rb.m_Entity, rb.m_vPos, rb.m_sFaction, true, rb.m_aWings, rb.m_aWingPlans, "checkpoint");
 }

 protected static void Both(IEntity left, IEntity right, vector at, string faction, bool small, notnull array<IEntity> pieces, notnull array<ref ACR_WingPlan> plans, string what)
 {
  int started = System.GetTickCount();
  ACR_BaseSurvey survey = ACR_BaseSurvey.TakeAround(at, RADIUS);
  array<string> words = {};
  for (int side = -1; side <= 1; side += 2)
  {
   IEntity from = left;
   if (side > 0) from = right;
   vector tip, heading;
   if (!from || !EndOf(from, side, tip, heading)) continue;
   string key = string.Format("wing|%1|%2|%3", (int)at[0], (int)at[2], side);
   // The work's own clutter (a chair, a bush, a dirt cover by its end piece) doesn't stop the bags.
   ACR_BaseDefences.s_aPreview.Insert(from);
   ACR_WingPlan wing = Plan(survey, faction, tip, heading, side, key, small);
   ACR_BaseDefences.s_aPreview.RemoveItem(from);
   Spawn(wing, pieces);
   if (!wing.m_aPieces.IsEmpty()) plans.Insert(wing);
   words.Insert(string.Format("%1 %2 (%3 pieces, %4; %5)", side, ShapeWord(wing.m_iShape), wing.m_aPieces.Count(), wing.m_sEnd, wing.m_sWhyNot));
  }
  int spent = System.GetTickCount() - started;
  s_iWorstMs = Math.Max(s_iWorstMs, spent);
  PrintFormat("[ACR] WINGS %1 at %2: %3, %4 ms", what, at, Words(words), spent);
 }

 protected static string Words(notnull array<string> words)
 {
  string all;
  foreach (int i, string word : words)
  {
   if (i > 0) all += "; ";
   all += word;
  }
  return all;
 }

 // A gun or post going in at 'place' facing 'yaw': the wing's bags on that spot and in its line of fire make
 // way. What came off is in 'cuts', for PutBack.
 static void Cut(notnull array<IEntity> pieces, vector place, float yaw, bool gun, notnull array<ref ACR_WingCut> cuts)
 {
  if (pieces.IsEmpty()) return;
  float rad = yaw * Math.DEG2RAD;
  vector forward = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector right = Vector(forward[2], 0, -forward[0]);
  for (int i = pieces.Count() - 1; i >= 0; i--)
  {
   IEntity piece = pieces[i];
   if (!piece) continue;
   vector at = piece.GetOrigin() - place;
   float x = vector.Dot(at, right);
   float z = vector.Dot(at, forward);
   bool onSpot = x * x + z * z < GUN_ROOM * GUN_ROOM;
   bool inFire = gun && z > 0 && z < ACR_Fortify.FIRE_CLEAR && Math.AbsFloat(x) < GUN_FIRE_HALF;
   if (!onSpot && !inFire) continue;
   ACR_WingCut cut = new ACR_WingCut();
   EntityPrefabData data = piece.GetPrefabData();
   if (data) cut.m_sPrefab = data.GetPrefabName();
   cut.m_vPos = piece.GetOrigin();
   cut.m_vAngles = piece.GetYawPitchRoll();
   cuts.Insert(cut);
   ACR_Navmesh.DeleteStructure(piece);
   pieces.RemoveOrdered(i);
  }
 }

 static void PutBack(notnull array<IEntity> pieces, notnull array<ref ACR_WingCut> cuts)
 {
  foreach (ACR_WingCut cut : cuts)
  {
   IEntity piece = ACR_BaseDefences.SpawnOnGround(cut.m_sPrefab, cut.m_vPos, cut.m_vAngles[0]);
   if (!piece) continue;
   pieces.Insert(piece);
   ACR_Navmesh.RebuildAfterSpawn(piece);
  }
  cuts.Clear();
 }
}
