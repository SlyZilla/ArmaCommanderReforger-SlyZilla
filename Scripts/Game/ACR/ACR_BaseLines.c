// A base's works as defensive lines (#105, the owner on photos of 30 Oct: "they look like randomly placed
// objects ... it does not make sense to have one singular defense and then nothing"). The line is the
// unit, not the position: sandbag pieces chained end to end across an approach the survey found weak
// (ACR_BaseSurvey), from cover to cover where it can.
//   A line runs from a seed both ways (ACR_BaseLiner.Walk), along the town's edge at some offset, or
//   straight across a gap in a wall. Each piece must fit and have a view on its firing side; one that
//   doesn't turns a little, or the line goes round with a round piece.
//   It ends on an anchor (a building, a wall or fence, a slope too steep, the road, another work); with
//   none it curves back, a round piece and an end piece, so it doesn't just stop in the open.
//   A road: the line stops at its edge, goes on across it, and a lane stays open for the checkpoint.
//   A walk-through gap every 25-35 m of wall, for men on foot, ours included.
//   Nests are built into a line (the sandbags where one goes come out, ACR_BaseWorks.Build), bunkers and
//   towers stand at its ends or behind it, the command post behind its middle, wire in front. Nothing
//   stands alone.
// A line is built in stretches of 20-28 m, one site each: the squad works along it and pays per stretch,
// and a lost stretch can be built again. The worst approach first, then the next while the budget of
// pieces lasts, so a line that convinces rather than a thin ring. Which approach, the offset, where
// it bends and where the guns go are weighted among the near-best and seeded (ACR_Variety).
class ACR_LinePiece : Managed
{
 vector m_vPos;        // its middle, on the ground
 float m_fYaw;
 ResourceName m_sPrefab;
 vector m_vFrom;       // its ends, in the order along the line
 vector m_vTo;
 vector m_vFacing;     // the line's firing side here
 int m_iBreak;         // what follows it: 0 the next piece, 1 a walk-through gap, 2 a road
 bool m_bCurve;        // part of an end curving back
 bool m_bLong;
}

class ACR_BaseLine : Managed
{
 int m_iTier;
 int m_iSector;
 ref array<ref ACR_LinePiece> m_aPieces = {};  // in order, once it's chosen
 ref array<ref ACR_LinePiece> m_aLeft = {};    // the two ways from the seed, before that
 ref array<ref ACR_LinePiece> m_aRight = {};
 string m_sEndA;                               // what each end stands on: building, wall, slope, road, work, gap, curve
 string m_sEndB;
 ref array<vector> m_aLanes = {};              // where a road goes through it, and which way is out along it
 ref array<vector> m_aLaneOut = {};
 vector m_vMiddle;
 vector m_vFacing;
 float m_fLength;
 float m_fView;
 float m_fScore;
 bool m_bGap;                                  // closes a gap in a wall
 int m_iSites;

 int Count() { return m_aLeft.Count() + m_aRight.Count(); }
 int Anchors()
 {
  int anchors;
  if (!m_sEndA.IsEmpty()) anchors++;
  if (!m_sEndB.IsEmpty()) anchors++;
  return anchors;
 }
}

// Walks one candidate line.
class ACR_BaseLiner : Managed
{
 static const int FIT_OK = 0;
 static const int FIT_ROAD = 1;
 static const int FIT_BUILDING = 2;
 static const int FIT_SOLID = 3;
 static const int FIT_SLOPE = 4;
 static const int FIT_DOOR = 5;
 static const int FIT_VIEW = 6;
 static const int FIT_WATER = 7;
 static const float VIEW = 18;              // m clear on a piece's firing side
 static const float MAX_TURN = 11;          // degrees at one joint: a wider one opens the outside of the joint
 static const float MAX_RISE = 0.75;        // m along a long piece (its ground tilt tops at 12 degrees)
 static const float MAX_LEAN = 0.5;         // m across it
 static const float ROAD_KEEP = 0.6;        // m from a road's edge to a piece

 ACR_BaseSurvey m_Survey;
 ref array<ref ACR_BaseDefenceSeg> m_aRoads = {};
 ref array<IEntity> m_aHouses = {};
 float m_fOffset;      // m past the town's edge
 float m_fWobble;      // m the offset swings by along the line
 float m_fPhase;
 float m_fSeed;        // the bearing it started from
 float m_fSpan;        // degrees it may run from there
 int m_iGapEvery;      // long pieces between walk-through gaps
 int m_iSeedRun;       // long pieces already unbroken at the seed
 bool m_bTarget;       // straight for a point (the far side of a gap in a wall)
 vector m_vTarget;
 protected bool m_bStrict;

 static vector FacingOf(vector heading, int sign)
 {
  if (sign > 0) return Vector(-heading[2], 0, heading[0]);
  return Vector(heading[2], 0, -heading[0]);
 }

 static vector Turned(vector heading, float degrees)
 {
  float rad = degrees * Math.DEG2RAD;
  float c = Math.Cos(rad);
  float s = Math.Sin(rad);
  return Vector(heading[0] * c + heading[2] * s, 0, heading[2] * c - heading[0] * s);
 }

 // Away from the firing side by 'degrees'.
 static vector TurnedBack(vector heading, int sign, float degrees)
 {
  vector back = FacingOf(heading, sign) * -1;
  float rad = degrees * Math.DEG2RAD;
  return heading * Math.Cos(rad) + back * Math.Sin(rad);
 }

 static float Ground(vector p)
 {
  return GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
 }

 void ACR_BaseLiner(ACR_BaseSurvey survey, float seedBearing)
 {
  m_Survey = survey;
  m_fSeed = seedBearing;
  m_fSpan = 60;
  vector way = ACR_BaseSurvey.Dir(seedBearing);
  foreach (ACR_BaseDefenceSeg seg : survey.m_aSegs)
  {
   vector mid = (seg.m_vA + seg.m_vB) * 0.5 - survey.m_vCenter;
   mid[1] = 0;
   float apart = mid.Length();
   if (apart > 60 && ACR_BaseDefences.DotXZ(way, mid * (1.0 / apart)) < 0.2) continue;
   m_aRoads.Insert(seg);
  }
  foreach (IEntity house : survey.m_aBuildings)
  {
   if (!house) continue;
   vector toHouse = house.GetOrigin() - survey.m_vCenter;
   toHouse[1] = 0;
   float away = toHouse.Length();
   if (away > 60 && ACR_BaseDefences.DotXZ(way, toHouse * (1.0 / away)) < 0.2) continue;
   m_aHouses.Insert(house);
  }
 }

 // Whether a piece of this length fits going 'heading' from 'tip': why not, else FIT_OK.
 // loose: an end piece or a piece that closes on an anchor (no view asked, nearer to things).
 int Fit(vector tip, vector heading, float length, int sign, bool loose)
 {
  vector to = tip + heading * length;
  vector mid = tip + heading * (length * 0.5);
  float tipY = Ground(tip);
  float toY = Ground(to);
  mid[1] = Ground(mid);
  if (BBB_Terrain.IsWaterAt(mid) || BBB_Terrain.IsWaterAt(to)) return FIT_WATER;
  float keep = ROAD_KEEP;
  if (loose) keep = 0.3;
  if (ACR_BaseDefences.RoadEdgeGap(m_aRoads, mid) < keep || ACR_BaseDefences.RoadEdgeGap(m_aRoads, to) < keep) return FIT_ROAD;
  if (Math.AbsFloat(toY - tipY) > MAX_RISE * length / ACR_BaseLines.LONG_STEP) return FIT_SLOPE;
  vector facing = FacingOf(heading, sign);
  vector front = mid + facing * 0.6;
  vector rear = mid - facing * 0.6;
  if (Math.AbsFloat(Ground(front) - Ground(rear)) > MAX_LEAN) return FIT_SLOPE;
  float clear = 1;
  if (loose) clear = 0.3;
  if (ACR_BaseDefences.BuildingGap(m_aHouses, mid, clear + 1) < clear || ACR_BaseDefences.BuildingGap(m_aHouses, to, clear + 1) < clear) return FIT_BUILDING;
  if (m_Survey.DoorGap(mid) < ACR_BaseSurvey.WALL_DOOR) return FIT_DOOR;
  if (!ACR_BaseDefences.WorldClear(mid, ACR_BaseDefences.Bearing(facing), Vector(length * 0.5 + 0.05, 0, 0.61))) return FIT_SOLID;
  if (loose) return FIT_OK;
  vector muzzle = mid + facing * 0.8;
  muzzle[1] = Ground(muzzle) + ACR_BaseSurvey.UP_BAGS;
  if (ACR_BaseSurvey.Reach(muzzle, ACR_BaseDefences.Bearing(facing), VIEW) < VIEW - 0.5) return FIT_VIEW;
  return FIT_OK;
 }

 // Where the line wants to go next: on along the town's edge at its offset, or at its target.
 protected vector Wanted(vector tip, vector heading, int sign)
 {
  if (m_bTarget) return ACR_BaseDefences.FlatDir(tip, m_vTarget);
  vector center = m_Survey.m_vCenter;
  float bearing = ACR_BaseDefences.Bearing(tip - center);
  float radius = vector.DistanceXZ(tip, center);
  float step = (ACR_BaseLines.LONG_STEP * 1.5 / Math.Max(radius, 15)) * Math.RAD2DEG * sign;
  float ahead = bearing + step;
  float reach = m_Survey.EdgeSmooth(ahead) + m_fOffset + m_fWobble * Math.Sin(ahead * 0.11 + m_fPhase);
  vector target = center + ACR_BaseSurvey.Dir(ahead) * reach;
  return ACR_BaseDefences.FlatDir(tip, target);
 }

 protected float TurnBetween(vector from, vector to)
 {
  return ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(to) - ACR_BaseDefences.Bearing(from) + 180) - 180;
 }

 // The next long piece: the turn towards where it wants to go, then wider either way. FIT_OK with
 // 'next' set, or why it can't go straight on.
 protected int NextHeading(vector tip, vector heading, int sign, out vector next)
 {
  // Gently while nothing's in the way: a line that keeps turning has no straight run for a nest.
  float wanted = Math.Clamp(TurnBetween(heading, Wanted(tip, heading, sign)), -MAX_TURN * 0.5, MAX_TURN * 0.5);
  array<float> turns = {wanted};
  array<float> more = {-MAX_TURN, MAX_TURN, -MAX_TURN * 0.5, MAX_TURN * 0.5, 0};
  foreach (float turn : more)
   if (Math.AbsFloat(turn - wanted) > 2) turns.Insert(turn);
  int first = FIT_OK;
  foreach (int i, float tried : turns)
  {
   vector way = Turned(heading, tried);
   int fit = Fit(tip, way, ACR_BaseLines.LONG_STEP, sign, false);
   if (fit == FIT_OK)
   {
    next = way;
    return FIT_OK;
   }
   // A road or a house straight ahead is the end of it, not something to wriggle along.
   if (i == 0) first = fit;
   if (fit == FIT_ROAD && Math.AbsFloat(tried) < 6) return fit;
  }
  return first;
 }

 protected ACR_LinePiece Piece(vector from, vector to, vector middle, float yaw, ResourceName prefab, vector facing)
 {
  ACR_LinePiece piece = new ACR_LinePiece();
  piece.m_vFrom = from;
  piece.m_vTo = to;
  piece.m_vPos = middle;
  piece.m_vPos[1] = Ground(middle);
  piece.m_fYaw = yaw;
  piece.m_sPrefab = prefab;
  piece.m_vFacing = facing;
  piece.m_bLong = ACR_BaseDefences.IsLong(prefab);
  return piece;
 }

 // One way from 'tip', up to 'most' long pieces. How it ended, "" if in the open.
 string Walk(vector tip, vector heading, int sign, int most, notnull array<ref ACR_LinePiece> run, notnull array<vector> lanes, notnull array<vector> laneOut, ACR_BaseDefenceKit kit)
 {
  // (The walk the other way from the seed counts towards the first gap too.)
  int sinceGap = m_iSeedRun;
  bool rounded;
  int made;
  while (made < most)
  {
   if (m_bTarget && vector.DistanceXZ(tip, m_vTarget) < ACR_BaseLines.LONG_STEP) return Closed(tip, heading, sign, run, kit, "gap");
   if (!m_bTarget && OffSeed(tip) > m_fSpan) return "";
   vector next;
   int why = NextHeading(tip, heading, sign, next);
   if (why == FIT_OK)
   {
    vector to = tip + next * ACR_BaseLines.LONG_STEP;
    vector facing = FacingOf(next, sign);
    run.Insert(Piece(tip, to, tip + next * (ACR_BaseLines.LONG_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sLong, facing));
    tip = to;
    heading = next;
    made++;
    sinceGap++;
    if (sinceGap >= m_iGapEvery)
    {
     vector over = tip + heading * ACR_BaseLines.WALK_GAP;
     // (Overdue: a gap anyway, whatever the ground in it is like.)
     if (sinceGap >= m_iGapEvery + 1 || Fit(tip, heading, ACR_BaseLines.WALK_GAP, sign, true) == FIT_OK)
     {
      run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_WALK;
      tip = over;
      sinceGap = 0;
     }
    }
    continue;
   }
   if (why == FIT_ROAD && !run.IsEmpty())
   {
    vector across;
    vector lane;
    if (Across(tip, heading, across, lane))
    {
     AddLane(lane, lanes, laneOut);
     if (made < most && Fit(across, heading, ACR_BaseLines.LONG_STEP, sign, false) == FIT_OK)
     {
      run[run.Count() - 1].m_iBreak = ACR_BaseLines.BREAK_ROAD;
      tip = across;
      sinceGap = 0;
      continue;
     }
    }
    else AddLane(tip + heading * 3, lanes, laneOut);
    return Closed(tip, heading, sign, run, kit, "road");
   }
   // Round what's in the way, once a way: a round piece bends the line back.
   if (!rounded && why != FIT_SLOPE && why != FIT_ROAD && !run.IsEmpty())
   {
    rounded = true;
    vector after;
    if (RoundPiece(tip, heading, sign, run, kit, after))
    {
     tip = run[run.Count() - 1].m_vTo;
     heading = after;
     made++;
     continue;
    }
   }
   return Ended(tip, heading, sign, why, run, kit);
  }
  return "";
 }

 protected float OffSeed(vector tip)
 {
  return Math.AbsFloat(ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(tip - m_Survey.m_vCenter) - m_fSeed + 180) - 180);
 }

 // Over the road ahead: the first spot on its far side, and the lane's middle. False if it runs along.
 protected bool Across(vector tip, vector heading, out vector across, out vector lane)
 {
  bool onRoad;
  float entered;
  for (float d = 1; d <= 30; d += 1)
  {
   vector p = tip + heading * d;
   float gap = ACR_BaseDefences.RoadEdgeGap(m_aRoads, p);
   if (!onRoad)
   {
    if (gap < 0.3)
    {
     onRoad = true;
     entered = d;
    }
    else if (d > 8) return false;
    continue;
   }
   if (gap >= 0.8)
   {
    across = p;
    lane = tip + heading * ((entered + d) * 0.5);
    return true;
   }
  }
  return false;
 }

 protected void AddLane(vector lane, notnull array<vector> lanes, notnull array<vector> laneOut)
 {
  foreach (vector have : lanes) if (vector.DistanceXZ(have, lane) < 12) return;
  vector onRoad;
  float dist;
  int seg = ACR_BaseDefences.NearestSeg(m_aRoads, lane, onRoad, dist);
  vector out_way = ACR_BaseDefences.FlatDir(m_Survey.m_vCenter, lane);
  if (seg >= 0)
  {
   ACR_BaseDefenceSeg road = m_aRoads[seg];
   vector along = ACR_BaseDefences.FlatDir(road.m_vA, road.m_vB);
   if (ACR_BaseDefences.DotXZ(along, out_way) < 0) along = along * -1;
   out_way = along;
   lane = onRoad;
  }
  lanes.Insert(lane);
  laneOut.Insert(out_way);
 }

 // A round piece at the tip: its chord turned back 20 degrees, its far tip 30 more. Its tips stand
 // ROUND_HALF either side of its middle and half a metre behind it. The heading it leaves on.
 protected bool RoundPiece(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit, out vector after)
 {
  vector chord = TurnedBack(heading, sign, 20);
  if (Fit(tip, chord, ACR_BaseDefences.ROUND_HALF * 2, sign, true) != FIT_OK) return false;
  vector facing = FacingOf(chord, sign);
  vector to = tip + chord * (ACR_BaseDefences.ROUND_HALF * 2);
  vector middle = tip + chord * ACR_BaseDefences.ROUND_HALF + facing * 0.5;
  ACR_LinePiece piece = Piece(tip, to, middle, ACR_BaseDefences.Bearing(facing), kit.m_sRound, facing);
  run.Insert(piece);
  after = TurnedBack(chord, sign, 30);
  return true;
 }

 // Stopped by something: is it something to end on? Closes up to it if so; "" if it's open ground.
 protected string Ended(vector tip, vector heading, int sign, int why, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit)
 {
  string word = m_Survey.AnchorNear(tip + heading * 1.5, 4.5);
  if (word.IsEmpty() && why == FIT_SLOPE) word = "slope";
  if (word.IsEmpty()) return "";
  return Closed(tip, heading, sign, run, kit, word);
 }

 // The last short piece up to what it ends on, if there's room for one.
 protected string Closed(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit, string word)
 {
  vector facing = FacingOf(heading, sign);
  if (Fit(tip, heading, ACR_BaseLines.SHORT_STEP, sign, true) == FIT_OK)
  {
   vector to = tip + heading * ACR_BaseLines.SHORT_STEP;
   run.Insert(Piece(tip, to, tip + heading * (ACR_BaseLines.SHORT_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sShort, facing));
  }
  return word;
 }

 // The open end of a line curves back: a round piece and an end piece with its taper trailing, or just
 // the end piece. Pieces come off the end until that fits. With no pieces this way at all, from the seed.
 // False if none of it will.
 bool CurveBack(notnull array<ref ACR_LinePiece> run, int sign, ACR_BaseDefenceKit kit, vector seedTip, vector seedHeading)
 {
  while (true)
  {
   // (A piece or two left past a break by what came off isn't worth curving.)
   ACR_BaseLines.LoneTail(run);
   vector tip = seedTip;
   vector heading = seedHeading;
   if (!run.IsEmpty())
   {
    ACR_LinePiece last = run[run.Count() - 1];
    last.m_iBreak = 0;
    tip = last.m_vTo;
    heading = ACR_BaseDefences.FlatDir(last.m_vFrom, last.m_vTo);
   }
   vector after;
   if (RoundPiece(tip, heading, sign, run, kit, after))
   {
    ACR_LinePiece bend = run[run.Count() - 1];
    bend.m_bCurve = true;
    if (AddEnd(bend.m_vTo, after, sign, run, kit)) return true;
    run.Remove(run.Count() - 1);
   }
   vector sharp = TurnedBack(heading, sign, 50);
   if (AddEnd(tip, sharp, sign, run, kit)) return true;
   if (run.IsEmpty()) return false;
   run.Remove(run.Count() - 1);
  }
  return false;
 }

 protected bool AddEnd(vector tip, vector heading, int sign, notnull array<ref ACR_LinePiece> run, ACR_BaseDefenceKit kit)
 {
  if (Fit(tip, heading, ACR_BaseLines.END_STEP, sign, true) != FIT_OK) return false;
  vector to = tip + heading * ACR_BaseLines.END_STEP;
  // The taper is at the end piece's local +X (ACR_BaseDefences.EndFacing): it trails away from the line.
  vector facing = ACR_BaseDefences.EndFacing(heading);
  ACR_LinePiece piece = Piece(tip, to, tip + heading * (ACR_BaseLines.END_STEP * 0.5), ACR_BaseDefences.Bearing(facing), kit.m_sEnd, FacingOf(heading, sign));
  piece.m_bCurve = true;
  run.Insert(piece);
  return true;
 }
}

class ACR_BaseLines
{
 // Long high piece: 3.43 m over its ends (x -1.69 .. 1.74); these leave a few cm of overlap at a joint.
 static const float LONG_STEP = 3.38;
 static const float END_STEP = 2.1;       // end high piece: 2.13 m
 static const float SHORT_STEP = 1.68;    // short high piece: 1.73 m
 static const float WALK_GAP = 2.4;       // m: men in file get through, a vehicle doesn't
 static const int BREAK_WALK = 1;
 static const int BREAK_ROAD = 2;
 static const int MIN_PIECES = 5;         // a shorter line isn't a line, unless it closes a gap in a wall
 static const int MIN_GAP_PIECES = 2;
 static const int MOST_ONE_WAY = 18;      // long pieces from a seed each way
 // Cap on one base's sandbag pieces: replicated props, each a streaming node and a navmesh rebuild. For
 // scale, one vanilla sandbag position is 26-40 pieces, and a medium checkpoint about 28.
 static const int MOST_PIECES = 100;
 static const int MOST_WIRE = 10;         // wire segments in front of one line
 static const float CLASH = 22;           // m between lines
 static const float NEST_HALF = 3.4;      // m: a nest's depth either side of the line (6-6.2 m; FIA's 9.8 m stands proud of it)
 static const float CHECKPOINT_OUT = 10;  // m along the road outside the line: its gun and wire clear the line's end
 static ref array<float> s_aOffsets = {5, 10, 16, 24, 34};

 // ---------------------------------------------------------------- budgets

 static int Budget(int tier, int town)
 {
  switch (tier)
  {
   case 1: return Math.ClampInt(24 + town / 2, 30, 60);
   case 2: return Math.ClampInt(10 + town / 4, 10, 25);
   case 3: return 15;
  }
  return 0;
 }

 // ---------------------------------------------------------------- the plan

 static void Plan(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(work.m_sFaction);
  array<ref ACR_BaseLine> pool = {};
  int started = System.GetTickCount();
  Candidates(work, survey, kit, pool);
  int walked = pool.Count();
  int total = 0;
  for (int tier = 1; tier <= 2; tier++) total += Fill(work, survey, kit, pool, tier, Budget(tier, survey.m_iTown));
  array<ref ACR_BaseLine> behind = {};
  Fallback(work, survey, kit, behind);
  total += Fill(work, survey, kit, behind, 3, Math.Min(Budget(3, survey.m_iTown), MOST_PIECES - total));
  survey.m_iLineMs = System.GetTickCount() - started;
  foreach (int i, ACR_BaseLine line : work.m_aLines)
   PrintFormat("[ACR] BASEWORKS %1 line %2, tier %3: %4 pieces, %5 m in %6 stretches, facing %7, ends %8 and %9", work.m_Base.m_sName, i, line.m_iTier, line.m_aPieces.Count(), (int)line.m_fLength, line.m_iSites, (int)ACR_BaseDefences.Bearing(line.m_vFacing), line.m_sEndA, line.m_sEndB);
  PrintFormat("[ACR] BASEWORKS %1: %2 lines, %3 sandbag pieces (at most %4), from %5 walked, %6 ms", work.m_Base.m_sName, work.m_aLines.Count(), total, MOST_PIECES, walked, survey.m_iLineMs);
  if (work.m_aLines.IsEmpty()) PrintFormat("[ACR] BASEWORKS %1: no line fits: %2", work.m_Base.m_sName, survey.FitWords());
 }

 // Lines worth having round the worst approaches: at a few offsets and shifts past the town's edge, and
 // across the gaps in its walls.
 protected static void Candidates(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool)
 {
  array<int> sectors = {};
  survey.WeakSectors(sectors, 4);
  string town = work.m_Base.m_sName;
  foreach (int order, int s : sectors)
  {
   RandomGenerator rng = ACR_Variety.For(string.Format("base line %1 %2", town, s));
   float bearing = ACR_BaseSurvey.SectorBearing(s);
   foreach (float offset : s_aOffsets)
   {
    float shift = rng.RandFloat01() * 16 - 8;
    float past = offset + rng.RandFloat01() * 3;
    float seedAt = bearing + shift;
    vector seed = survey.m_vCenter + ACR_BaseSurvey.Dir(seedAt) * (survey.EdgeSmooth(seedAt) + past);
    ACR_BaseLine line = WalkFrom(survey, kit, seed, seedAt, past, rng, s, false, vector.Zero);
    if (line) pool.Insert(line);
   }
  }
  GapLines(work, survey, kit, pool);
 }

 // A line from a seed, both ways.
 protected static ACR_BaseLine WalkFrom(ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, vector seed, float seedAt, float past, RandomGenerator rng, int sector, bool gap, vector target)
 {
  seed[1] = ACR_BaseLiner.Ground(seed);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, seedAt);
  liner.m_fOffset = past;
  liner.m_fWobble = rng.RandFloat01() * 2.5;
  liner.m_fPhase = rng.RandFloat01() * 6.28;
  liner.m_iGapEvery = 8 + (int)(rng.RandFloat01() * 3);
  vector facing = ACR_BaseSurvey.Dir(seedAt);
  vector heading = Vector(facing[2], 0, -facing[0]);
  ACR_BaseLine line = new ACR_BaseLine();
  line.m_iSector = sector;
  line.m_vFacing = facing;
  if (gap)
  {
   liner.m_bTarget = true;
   liner.m_vTarget = target;
   heading = ACR_BaseDefences.FlatDir(seed, target);
   int side = 1;
   if (ACR_BaseDefences.DotXZ(ACR_BaseLiner.FacingOf(heading, 1), facing) < 0) side = -1;
   if (liner.Fit(seed, heading, LONG_STEP, side, false) != ACR_BaseLiner.FIT_OK) return null;
   line.m_bGap = true;
   line.m_sEndA = "wall";
   line.m_sEndB = liner.Walk(seed, heading, side, MOST_ONE_WAY, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
   if (side < 0)
   {
    // The chain reads from its right end: the walk went the other way.
    foreach (ACR_LinePiece gapWalked : line.m_aRight) line.m_aLeft.Insert(gapWalked);
    line.m_aRight.Clear();
    string word = line.m_sEndA;
    line.m_sEndA = line.m_sEndB;
    line.m_sEndB = word;
   }
  }
  else
  {
   if (liner.Fit(seed, heading, LONG_STEP, 1, false) != ACR_BaseLiner.FIT_OK && liner.Fit(seed, heading * -1, LONG_STEP, -1, false) != ACR_BaseLiner.FIT_OK) return null;
   line.m_sEndB = liner.Walk(seed, heading, 1, MOST_ONE_WAY, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
   foreach (ACR_LinePiece walked : line.m_aRight)
   {
    liner.m_iSeedRun++;
    if (walked.m_iBreak != 0) break;
   }
   line.m_sEndA = liner.Walk(seed, heading * -1, -1, MOST_ONE_WAY, line.m_aLeft, line.m_aLanes, line.m_aLaneOut, kit);
  }
  int least = MIN_PIECES;
  if (gap) least = MIN_GAP_PIECES;
  if (line.Count() < least) return null;
  Measure(line, survey);
  return line;
 }

 // Gaps in walls and fences on the threatened side: free wall ends facing each other across open
 // ground. Closing one is the best kind of line.
 protected static void GapLines(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool)
 {
  array<vector> ends = {};
  survey.FreeWallEnds(ends);
  RandomGenerator rng = ACR_Variety.For("base line gaps " + work.m_Base.m_sName);
  int tried;
  foreach (int i, vector a : ends)
  {
   for (int k = i + 1; k < ends.Count() && tried < 10; k++)
   {
    vector b = ends[k];
    float apart = vector.DistanceXZ(a, b);
    if (apart < 2 * LONG_STEP || apart > 40) continue;
    vector middle = (a + b) * 0.5;
    float bearing = ACR_BaseDefences.Bearing(middle - survey.m_vCenter);
    int sector = ACR_BaseSurvey.Sector(bearing);
    if (survey.m_aThreat[sector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
    // Roughly across the way out, not along it.
    vector across = ACR_BaseDefences.FlatDir(a, b);
    if (Math.AbsFloat(ACR_BaseDefences.DotXZ(across, ACR_BaseSurvey.Dir(bearing))) > 0.6) continue;
    tried++;
    vector start = a + across * 0.6;
    ACR_BaseLine line = WalkFrom(survey, kit, start, bearing, 0, rng, sector, true, b);
    if (line) pool.Insert(line);
   }
  }
 }

 protected static void Measure(ACR_BaseLine line, ACR_BaseSurvey survey)
 {
  float length = 0;
  vector middleAt = vector.Zero;
  int n = 0;
  float weak = 0;
  foreach (ACR_LinePiece piece : line.m_aLeft)
  {
   length += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(piece.m_vFacing)));
   n++;
  }
  foreach (ACR_LinePiece other : line.m_aRight)
  {
   length += vector.DistanceXZ(other.m_vFrom, other.m_vTo);
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(other.m_vFacing)));
   n++;
  }
  ACR_LinePiece mid;
  if (!line.m_aRight.IsEmpty()) mid = line.m_aRight[0];
  else mid = line.m_aLeft[0];
  line.m_vMiddle = mid.m_vPos;
  line.m_vFacing = mid.m_vFacing;
  line.m_fLength = length;
  // How much of it sees well out: every third piece, at a gun's height (a nest wants this).
  int seen = 0;
  int looked = 0;
  LookFrom(line.m_aLeft, seen, looked);
  LookFrom(line.m_aRight, seen, looked);
  line.m_fView = seen * 1.0 / Math.Max(looked, 1);
 }

 protected static void LookFrom(array<ref ACR_LinePiece> run, inout int seen, inout int looked)
 {
  for (int i = 1; i < run.Count(); i += 3)
  {
   ACR_LinePiece piece = run[i];
   vector muzzle = piece.m_vPos + piece.m_vFacing * 0.8;
   muzzle[1] = ACR_BaseLiner.Ground(muzzle) + ACR_BaseSurvey.UP_NEST;
   looked++;
   if (ACR_BaseSurvey.Reach(muzzle, ACR_BaseDefences.Bearing(piece.m_vFacing), 45) >= 44) seen++;
  }
 }

 // Up to 'budget' sandbag pieces of this tier: the best line for the worst approach, then the next.
 // What's left of the pool goes on to the next tier. The pieces used.
 protected static int Fill(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool, int tier, int budget)
 {
  int used = 0;
  int round = 0;
  while (budget - used >= MIN_GAP_PIECES)
  {
   array<ACR_BaseLine> open = {};
   array<float> scores = {};
   for (int i = pool.Count() - 1; i >= 0; i--)
   {
    ACR_BaseLine line = pool[i];
    if (Clashes(work, line))
    {
     pool.Remove(i);
     continue;
    }
    // Too long for what's left is cut to fit; too short for what it is doesn't go.
    open.Insert(line);
    scores.Insert(Score(line, survey));
   }
   if (open.IsEmpty()) break;
   int picked = ACR_Variety.Pick(scores, string.Format("base line pick %1 %2 %3", work.m_Base.m_sName, tier, round), 0.15);
   ACR_BaseLine chosen = open[picked];
   round++;
   // A strong ref until it's on the work.
   int at = pool.Find(chosen);
   ACR_BaseLine keep = chosen;
   work.m_aLines.Insert(keep);
   pool.Remove(at);
   int count = Realize(keep, survey, kit, budget - used);
   if (count == 0)
   {
    work.m_aLines.RemoveItem(keep);
    continue;
   }
   keep.m_iTier = tier;
   used += count;
   Stretches(work, keep, survey, kit, tier);
   foreach (ACR_LinePiece piece : keep.m_aPieces) survey.AddLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(piece.m_vFacing)), 0.03);
  }
  return used;
 }

 protected static float Score(ACR_BaseLine line, ACR_BaseSurvey survey)
 {
  float weak = 0;
  int n = 0;
  foreach (ACR_LinePiece a : line.m_aLeft)
  {
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(a.m_vFacing)));
   n++;
  }
  foreach (ACR_LinePiece b : line.m_aRight)
  {
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(b.m_vFacing)));
   n++;
  }
  float score = weak / Math.Max(n, 1) + Math.Min(line.m_fLength, 80) / 80 + 0.3 * line.Anchors() + 0.6 * line.m_fView + 0.15 * line.m_aLanes.Count();
  if (line.m_bGap) score += 0.4;
  return score;
 }

 // A piece near one of a line already chosen: this one crosses or doubles it (rows of sandbags 15 m
 // apart aren't two lines; the fallback line stands further back than this).
 protected static bool Clashes(ACR_BaseWork work, ACR_BaseLine line)
 {
  foreach (ACR_BaseLine other : work.m_aLines)
  {
   if (vector.DistanceXZ(other.m_vMiddle, line.m_vMiddle) > (other.m_fLength + line.m_fLength) * 0.5 + CLASH + 2) continue;
   foreach (ACR_LinePiece piece : other.m_aPieces)
   {
    if (NearRun(line.m_aLeft, piece.m_vPos, CLASH) || NearRun(line.m_aRight, piece.m_vPos, CLASH)) return true;
   }
  }
  return false;
 }

 protected static bool NearRun(array<ref ACR_LinePiece> run, vector at, float within)
 {
  foreach (ACR_LinePiece piece : run) if (vector.DistanceXZ(piece.m_vPos, at) < within) return true;
  return false;
 }

 // The chosen line's pieces in order, cut to 'most', its open ends curved back. The pieces it has, 0
 // if nothing fits.
 protected static int Realize(ACR_BaseLine line, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int most)
 {
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, ACR_BaseDefences.Bearing(line.m_vFacing));
  // Curves take two pieces each.
  int room = most;
  if (line.m_sEndA.IsEmpty()) room -= 2;
  if (line.m_sEndB.IsEmpty()) room -= 2;
  while (line.Count() > room && line.Count() > 0)
  {
   // Keep the middle: off the longer way, whose end is then open.
   if (line.m_aLeft.Count() > line.m_aRight.Count())
   {
    if (!line.m_sEndA.IsEmpty()) room -= 2;
    line.m_sEndA = "";
    line.m_aLeft.Remove(line.m_aLeft.Count() - 1);
   }
   else
   {
    if (!line.m_sEndB.IsEmpty()) room -= 2;
    line.m_sEndB = "";
    line.m_aRight.Remove(line.m_aRight.Count() - 1);
   }
  }
  if (line.m_bGap && line.Count() < MIN_GAP_PIECES) return 0;
  if (!line.m_bGap && line.Count() < MIN_PIECES) return 0;
  if (line.m_aLeft.IsEmpty() && line.m_aRight.IsEmpty()) return 0;
  SeedStub(line.m_aRight, line.m_aLeft, line);
  SeedStub(line.m_aLeft, line.m_aRight, line);
  // Which end has lost its anchor to a cut.
  if (LoneTail(line.m_aRight)) line.m_sEndB = "";
  if (LoneTail(line.m_aLeft)) line.m_sEndA = "";
  vector seedRight, seedLeft, headingRight, headingLeft;
  if (!line.m_aLeft.IsEmpty())
  {
   seedRight = line.m_aLeft[0].m_vFrom;
   headingRight = ACR_BaseDefences.FlatDir(line.m_aLeft[0].m_vTo, line.m_aLeft[0].m_vFrom);
  }
  if (!line.m_aRight.IsEmpty())
  {
   seedLeft = line.m_aRight[0].m_vFrom;
   headingLeft = ACR_BaseDefences.FlatDir(line.m_aRight[0].m_vTo, line.m_aRight[0].m_vFrom);
  }
  if (line.m_sEndB.IsEmpty() && liner.CurveBack(line.m_aRight, 1, kit, seedRight, headingRight)) line.m_sEndB = "curve";
  if (line.m_sEndA.IsEmpty() && liner.CurveBack(line.m_aLeft, -1, kit, seedLeft, headingLeft)) line.m_sEndA = "curve";
  // An end with nothing to stand on and no room to curve isn't a line we build.
  if (line.m_sEndA.IsEmpty() || line.m_sEndB.IsEmpty()) return 0;
  // Left reversed, then right: the seed in the middle.
  line.m_aPieces.Clear();
  for (int i = line.m_aLeft.Count() - 1; i >= 0; i--)
  {
   ACR_LinePiece piece = line.m_aLeft[i];
   vector from = piece.m_vFrom;
   piece.m_vFrom = piece.m_vTo;
   piece.m_vTo = from;
   // What follows it now is what came before it on the walk.
   if (i > 0) piece.m_iBreak = line.m_aLeft[i - 1].m_iBreak;
   else piece.m_iBreak = 0;
   line.m_aPieces.Insert(piece);
  }
  foreach (ACR_LinePiece right : line.m_aRight) line.m_aPieces.Insert(right);
  DropLoneRuns(line);
  float length = 0;
  foreach (ACR_LinePiece counted : line.m_aPieces) length += vector.DistanceXZ(counted.m_vFrom, counted.m_vTo);
  line.m_fLength = length;
  if (!line.m_aPieces.IsEmpty())
  {
   ACR_LinePiece mid = line.m_aPieces[line.m_aPieces.Count() / 2];
   line.m_vMiddle = mid.m_vPos;
   line.m_vFacing = mid.m_vFacing;
   line.m_iSector = ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(mid.m_vFacing));
  }
  // The lanes it kept.
  for (int l = line.m_aLanes.Count() - 1; l >= 0; l--)
  {
   bool near;
   foreach (ACR_LinePiece kept : line.m_aPieces) if (vector.DistanceXZ(kept.m_vPos, line.m_aLanes[l]) < 25) near = true;
   if (near) continue;
   line.m_aLanes.Remove(l);
   line.m_aLaneOut.Remove(l);
  }
  return line.m_aPieces.Count();
 }

 // What's left round the seed when a break falls right after it: too short to stand by itself, it comes off
 // and the line starts from the far side of the break.
 protected static void SeedStub(array<ref ACR_LinePiece> run, array<ref ACR_LinePiece> other, ACR_BaseLine line)
 {
  foreach (ACR_LinePiece near : other) if (near.m_iBreak != 0) return;
  float length = 0;
  foreach (ACR_LinePiece behind : other) length += vector.DistanceXZ(behind.m_vFrom, behind.m_vTo);
  int cut = -1;
  foreach (int i, ACR_LinePiece piece : run)
  {
   length += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
   if (piece.m_iBreak == 0) continue;
   cut = i;
   break;
  }
  if (cut < 0 || length >= 2 * LONG_STEP - 0.2 || cut + 1 >= run.Count()) return;
  string word = "";
  if (run[cut].m_iBreak == BREAK_ROAD) word = "road";
  if (run == line.m_aRight) line.m_sEndA = word;
  else line.m_sEndB = word;
  other.Clear();
  for (int k = 0; k <= cut; k++) run.RemoveOrdered(0);
 }

 // A single piece between two breaks (round the seed, where a road and a gap fell close) comes off.
 protected static void DropLoneRuns(ACR_BaseLine line)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  for (int i = pieces.Count() - 2; i > 0; i--)
  {
   if (pieces[i].m_iBreak != 0 && pieces[i - 1].m_iBreak != 0) pieces.RemoveOrdered(i);
  }
 }

 // Fewer than two long pieces beyond a gap or a road aren't worth having: they come off. True if any did.
 static bool LoneTail(array<ref ACR_LinePiece> run)
 {
  bool cut = false;
  while (true)
  {
   int n = run.Count();
   int longs = 0;
   int k = n - 1;
   while (k >= 0)
   {
    if (run[k].m_bLong) longs++;
    if (k > 0 && run[k - 1].m_iBreak != 0) break;
    k--;
   }
   if (k <= 0 || longs >= 2) return cut;
   while (run.Count() > k) run.Remove(run.Count() - 1);
   run[k - 1].m_iBreak = 0;
   cut = true;
  }
  return cut;
 }

 // Sites: stretches of 20-28 m along the line, a new one at a gap or a road. Each dressed behind.
 protected static void Stretches(ACR_BaseWork work, ACR_BaseLine line, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int tier)
 {
  RandomGenerator rng = ACR_Variety.For(string.Format("base stretch %1 %2", work.m_Base.m_sName, work.m_aLines.Count()));
  float target = 20 + rng.RandFloat01() * 8;
  array<ACR_LinePiece> pieces = {};
  float length = 0;
  int order = 0;
  float left = line.m_fLength;
  foreach (int i, ACR_LinePiece piece : line.m_aPieces)
  {
   pieces.Insert(piece);
   float span = vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
   length += span;
   left -= span;
   bool cut = piece.m_iBreak != 0 || i == line.m_aPieces.Count() - 1;
   // A short tail goes on the stretch before it.
   if (length >= target && left >= 10) cut = true;
   if (!cut) continue;
   AddStretch(work, line, pieces, survey, kit, tier, order);
   order++;
   pieces.Clear();
   length = 0;
  }
  line.m_iSites = order;
 }

 protected static void AddStretch(ACR_BaseWork work, ACR_BaseLine line, array<ACR_LinePiece> pieces, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int tier, int order)
 {
  ACR_LinePiece mid = pieces[pieces.Count() / 2];
  ACR_BaseDefencePos pos = new ACR_BaseDefencePos();
  pos.m_eKind = ACR_EBaseDefenceKind.LINE;
  pos.m_vPos = mid.m_vPos;
  pos.m_vFacing = mid.m_vFacing;
  foreach (ACR_LinePiece piece : pieces)
  {
   pos.m_aPiecePos.Insert(piece.m_vPos);
   pos.m_aPieceYaw.Insert(piece.m_fYaw);
   pos.m_aPiecePrefab.Insert(piece.m_sPrefab);
   pos.m_aPieceWall.Insert(true);
  }
  // A few things behind the bags, so people seem to live here.
  ACR_BaseDefences.Seed(mid.m_vPos);
  vector first = pieces[0].m_vPos;
  vector along = ACR_BaseDefences.FlatDir(first, pieces[pieces.Count() - 1].m_vPos);
  float span = vector.DistanceXZ(first, pieces[pieces.Count() - 1].m_vPos);
  ACR_BaseDefences.Dress(pos, kit, first, mid.m_vFacing, along, span, 1.5, 1.2, 1 + ACR_BaseDefences.RandIndex(2), survey.m_aSegs, survey.m_aBuildings, true);
  ACR_BaseDefences.Light(pos, kit, mid.m_vPos - mid.m_vFacing * 0.9, survey.m_aBuildings);
  ACR_BaseSite site = new ACR_BaseSite();
  site.m_eKind = ACR_EBaseSite.PLANNED;
  site.m_Plan = pos;
  site.m_vPos = mid.m_vPos;
  site.m_fYaw = ACR_BaseDefences.Bearing(mid.m_vFacing);
  site.m_iSector = ACR_BaseSurvey.Sector(site.m_fYaw);
  site.m_iTier = tier;
  site.m_iLine = work.m_aLines.Find(line);
  site.m_iOrder = order;
  site.m_fScore = survey.WeakLight(site.m_iSector);
  work.m_aSites.Insert(site);
 }

 // A line behind the first, if the town has the depth: well inside its edge on the worst approach.
 protected static void Fallback(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> behind)
 {
  if (work.m_aLines.IsEmpty()) return;
  ACR_BaseLine front = work.m_aLines[0];
  float bearing = ACR_BaseDefences.Bearing(front.m_vFacing);
  RandomGenerator rng = ACR_Variety.For("base line back " + work.m_Base.m_sName);
  array<float> backs = {30, 42, 54};
  foreach (float back : backs)
  {
   vector seed = front.m_vMiddle - front.m_vFacing * back;
   float at = ACR_BaseDefences.Bearing(seed - survey.m_vCenter);
   // It must still be the same side of the town.
   if (vector.DistanceXZ(seed, survey.m_vCenter) < 25 || ACR_BaseDefences.DotXZ(ACR_BaseSurvey.Dir(at), front.m_vFacing) < 0.5) continue;
   float radius = vector.DistanceXZ(seed, survey.m_vCenter);
   float past = radius - survey.EdgeSmooth(at);
   ACR_BaseLine line = WalkFrom(survey, kit, seed, at, past, rng, ACR_BaseSurvey.Sector(bearing), false, vector.Zero);
   if (line) behind.Insert(line);
  }
 }

 // ---------------------------------------------------------------- what stands on the lines

 // A nest's width on the line: the pieces that come out for it (4: 13.5 m; FIA's wider one 5: 16.9 m).
 static int NestPieces(string faction)
 {
  if (faction == "FIA") return 5;
  return 4;
 }

 // Up to 'most' nests, built into the lines: on a straight run with the fan clear. Worst sector
 // first, away from the ends, a weighted pick; fewer if the lines have no such place.
 static void Nests(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  array<float> yaws = {};
  array<vector> places = {};
  array<ACR_BaseLine> onLines = {};
  array<int> firsts = {};
  int width = NestPieces(work.m_sFaction);
  vector half = Vector(width * LONG_STEP * 0.5, 0, NEST_HALF);
  int noRun, noRoad, noHouse, noDoor, noRoom, noFire;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_iTier > 2) continue;
   int count = line.m_aPieces.Count();
   for (int i = 1; i + width < count; i++)
   {
    ACR_LinePiece a = line.m_aPieces[i];
    ACR_LinePiece b = line.m_aPieces[i + width - 1];
    bool run = true;
    for (int k = i; k < i + width; k++)
    {
     ACR_LinePiece piece = line.m_aPieces[k];
     if (!piece.m_bLong || (k < i + width - 1 && piece.m_iBreak != 0)) run = false;
    }
    if (!run || ACR_BaseDefences.DotXZ(a.m_vFacing, b.m_vFacing) < 0.85)
    {
     noRun++;
     continue;
    }
    vector place = (a.m_vFrom + b.m_vTo) * 0.5;
    place[1] = ACR_BaseLiner.Ground(place);
    if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, place) < ACR_BaseSurvey.GUN_ROAD)
    {
     noRoad++;
     continue;
    }
    if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, place, ACR_BaseSurvey.BUILDING_CLEAR) < ACR_BaseSurvey.BUILDING_CLEAR)
    {
     noHouse++;
     continue;
    }
    if (survey.DoorGap(place) < ACR_BaseSurvey.DOOR_CLEAR)
    {
     noDoor++;
     continue;
    }
    vector facing = (a.m_vFacing + b.m_vFacing) * 0.5;
    float yaw = ACR_BaseDefences.Bearing(facing);
    if (!ACR_BaseDefences.WorldClear(place, yaw, half, null, true))
    {
     noRoom++;
     continue;
    }
    // The gun may look a little aside of the line's own facing.
    if (!ACR_BaseSurvey.NestFan(place, yaw))
    {
     if (ACR_BaseSurvey.NestFan(place, yaw + 15)) yaw += 15;
     else if (ACR_BaseSurvey.NestFan(place, yaw - 15)) yaw -= 15;
     else
     {
      noFire++;
      continue;
     }
    }
    // Not with another line's bags close in front of the gun.
    if (BagsAhead(work, place, yaw))
    {
     noFire++;
     continue;
    }
    places.Insert(place);
    yaws.Insert(yaw);
    onLines.Insert(line);
    firsts.Insert(i);
    i += 1;
   }
  }
  int made;
  for (made = 0; made < most; made++)
  {
   array<float> scores = {};
   array<int> open = {};
   foreach (int c, vector at : places)
   {
    if (ACR_BaseWorks.GunsNear(work, at, yaws[c])) continue;
    int sector = ACR_BaseSurvey.Sector(yaws[c]);
    open.Insert(c);
    scores.Insert(survey.Weak(sector));
   }
   if (open.IsEmpty()) break;
   int picked = open[ACR_Variety.Pick(scores, string.Format("base nest %1 %2", work.m_Base.m_sName, made))];
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.NEST;
   site.m_iTier = 2;
   site.m_vPos = places[picked];
   site.m_fYaw = ACR_FobDefences.BearingYaw(yaws[picked]);
   site.m_iSector = ACR_BaseSurvey.Sector(yaws[picked]);
   site.m_fScore = survey.Weak(site.m_iSector);
   site.m_iSpot = ACR_FobDefences.POST_COUNT;
   site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, work.m_sFaction, 1);
   site.m_iLine = work.m_aLines.Find(onLines[picked]);
   for (int cut = firsts[picked]; cut < firsts[picked] + width; cut++) site.m_aCuts.Insert(onLines[picked].m_aPieces[cut].m_vPos);
   work.m_aSites.Insert(site);
   survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_NEST);
  }
  // No straight run with a view on the lines: at a line's end, where the fan is clear.
  if (made < most)
  {
   array<ref ACR_BaseSpot> ends = {};
   foreach (ACR_BaseLine atEnd : work.m_aLines)
   {
    if (atEnd.m_iTier > 2 || atEnd.m_aPieces.IsEmpty()) continue;
    EndSpots(work, survey, atEnd, atEnd.m_aPieces[0], -1, ends);
    EndSpots(work, survey, atEnd, atEnd.m_aPieces[atEnd.m_aPieces.Count() - 1], 1, ends);
   }
   for (; made < most; made++)
   {
    array<ACR_BaseSpot> open2 = {};
    array<float> scores2 = {};
    foreach (ACR_BaseSpot spot : ends)
    {
     if (!spot.m_bNest || ACR_BaseWorks.GunsNear(work, spot.m_vPos, spot.m_fFacing)) continue;
     open2.Insert(spot);
     scores2.Insert(survey.Weak(spot.m_iSector));
    }
    if (open2.IsEmpty()) break;
    ACR_BaseSpot chosen = open2[ACR_Variety.Pick(scores2, string.Format("base nest end %1 %2", work.m_Base.m_sName, made))];
    ACR_BaseSite beside = new ACR_BaseSite();
    beside.m_eKind = ACR_EBaseSite.NEST;
    beside.m_iTier = 2;
    beside.m_vPos = chosen.m_vPos;
    beside.m_fYaw = ACR_FobDefences.BearingYaw(chosen.m_fFacing);
    beside.m_iSector = chosen.m_iSector;
    beside.m_fScore = survey.Weak(beside.m_iSector);
    beside.m_iSpot = ACR_FobDefences.POST_COUNT;
    beside.m_sPrefab = ACR_FobDefences.PrefabFor(beside.m_iSpot, work.m_sFaction, 1);
    work.m_aSites.Insert(beside);
    survey.AddCover(beside.m_iSector, ACR_BaseWorks.COVER_NEST);
   }
  }
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: %2 of %3 MG nests: %4 places on its lines, none or too close for more (not straight %5, road %6, building %7, door %8, no room %9", work.m_Base.m_sName, made, most, places.Count(), noRun, noRoad, noHouse, noDoor, noRoom);
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: nest places with no line of fire: %2", work.m_Base.m_sName, noFire);
 }

 // Up to 'most' bunkers and towers tied to the lines: past a line's end looking across its front,
 // a tower behind it (a bunker's ports wouldn't see over the bags).
 static void Posts(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  array<ref ACR_BaseSpot> spots = {};
  bool towers = work.m_sFaction != "FIA";
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_iTier > 2 || line.m_aPieces.IsEmpty()) continue;
   EndSpots(work, survey, line, line.m_aPieces[0], -1, spots);
   EndSpots(work, survey, line, line.m_aPieces[line.m_aPieces.Count() - 1], 1, spots);
   if (towers) BehindSpots(work, survey, line, spots);
  }
  int towersMade, bunkersMade;
  int made;
  for (made = 0; made < most; made++)
  {
   array<ACR_BaseSpot> open = {};
   array<float> scores = {};
   foreach (ACR_BaseSpot spot : spots)
   {
    // A tower looks the same way as the nest in front of it, on purpose.
    if (ACR_BaseWorks.GunWithin(work, spot.m_vPos, 30)) continue;
    open.Insert(spot);
    float score = survey.Weak(spot.m_iSector) + 0.2 * spot.m_fReach / ACR_BaseSurvey.OPEN_RANGE;
    scores.Insert(score);
   }
   if (open.IsEmpty()) break;
   ACR_BaseSpot chosen = open[ACR_Variety.Pick(scores, string.Format("base post %1 %2", work.m_Base.m_sName, made))];
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.BUNKER;
   site.m_iTier = 3;
   site.m_vPos = chosen.m_vPos;
   site.m_fYaw = ACR_FobDefences.BearingYaw(chosen.m_fFacing);
   site.m_iSector = chosen.m_iSector;
   site.m_fReach = chosen.m_fReach;
   site.m_fScore = survey.Weak(chosen.m_iSector);
   site.m_iSpot = 0;
   if (towers && ACR_BaseWorks.TowerThere(work, chosen, towersMade, bunkersMade, made)) site.m_iSpot = 1;
   if (site.m_iSpot == 1) towersMade++;
   else bunkersMade++;
   site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, work.m_sFaction, 2);
   work.m_aSites.Insert(site);
   survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_BUNKER);
  }
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: %2 of %3 bunkers and towers (%4 places by its lines)", work.m_Base.m_sName, made, most, spots.Count());
 }

 protected static void EndSpots(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseLine line, ACR_LinePiece end, int way, notnull array<ref ACR_BaseSpot> spots)
 {
  // Out from the last long piece (a curved end trails behind it).
  ACR_LinePiece last = end;
  int index = line.m_aPieces.Find(end);
  while (!last.m_bLong && index > 0 && index < line.m_aPieces.Count() - 1)
  {
   index -= way;
   last = line.m_aPieces[index];
  }
  vector out_way = ACR_BaseDefences.FlatDir(last.m_vFrom, last.m_vTo);
  vector tip = last.m_vTo;
  if (way < 0)
  {
   out_way = out_way * -1;
   tip = last.m_vFrom;
  }
  vector facing = last.m_vFacing;
  array<vector> places = {tip + out_way * 10, tip + out_way * 14, tip + out_way * 8 - facing * 8};
  array<float> turns = {0, 25, 45};
  foreach (vector place : places)
  {
   foreach (float turn : turns)
   {
    // Turned out, across the front of the line's end.
    vector look = facing * Math.Cos(turn * Math.DEG2RAD) + out_way * Math.Sin(turn * Math.DEG2RAD);
    ACR_BaseSpot spot = MakeSpot(work, survey, place, ACR_BaseDefences.Bearing(look), false);
    if (!spot) continue;
    spots.Insert(spot);
    break;
   }
  }
 }

 protected static void BehindSpots(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseLine line, notnull array<ref ACR_BaseSpot> spots)
 {
  int count = line.m_aPieces.Count();
  array<float> shares = {0.2, 0.35, 0.5, 0.65, 0.8};
  array<float> backs = {12, 17};
  foreach (float share : shares)
  {
   ACR_LinePiece piece = line.m_aPieces[(int)(count * share)];
   foreach (float back : backs)
   {
    ACR_BaseSpot spot = MakeSpot(work, survey, piece.m_vPos - piece.m_vFacing * back, ACR_BaseDefences.Bearing(piece.m_vFacing), true);
    if (!spot) continue;
    spots.Insert(spot);
    break;
   }
  }
 }

 protected static ACR_BaseSpot MakeSpot(ACR_BaseWork work, ACR_BaseSurvey survey, vector place, float facing, bool towerOnly)
 {
  place[1] = ACR_BaseLiner.Ground(place);
  if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, place) < ACR_BaseSurvey.GUN_ROAD) return null;
  if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, place, ACR_BaseSurvey.BUILDING_CLEAR) < ACR_BaseSurvey.BUILDING_CLEAR) return null;
  if (survey.DoorGap(place) < ACR_BaseSurvey.DOOR_CLEAR) return null;
  if (PieceWithin(work, place, ACR_BaseSurvey.FOOT_HALF + 1)) return null;
  string why;
  if (!ACR_FobBuild.FootprintClear(place, ACR_FobDefences.PIECE_RADIUS, null, why, true, ACR_FobDefences.MAX_RISE)) return null;
  if (!ACR_BaseDefences.WorldClear(place, facing, Vector(ACR_BaseSurvey.FOOT_HALF, 0, ACR_BaseSurvey.FOOT_HALF), null, true)) return null;
  bool tower = ACR_BaseSurvey.TowerFan(place, facing);
  bool bunker = !towerOnly && ACR_BaseSurvey.BunkerFan(place, facing) && !BagsAhead(work, place, facing);
  if (!tower && !bunker) return null;
  if (!bunker && work.m_sFaction == "FIA") return null;
  ACR_BaseSpot spot = new ACR_BaseSpot();
  spot.m_vPos = place;
  spot.m_fFacing = facing;
  spot.m_iSector = ACR_BaseSurvey.Sector(facing);
  spot.m_bBunker = bunker;
  spot.m_bTower = tower;
  spot.m_bNest = !towerOnly && ACR_BaseSurvey.NestFan(place, facing) && !BagsAhead(work, place, facing);
  vector muzzle = place;
  muzzle[1] = place[1] + ACR_BaseSurvey.UP_BUNKER;
  spot.m_fReach = Math.Max(ACR_BaseSurvey.Reach(muzzle, facing, ACR_BaseSurvey.OPEN_RANGE), 50);
  return spot;
 }

 static bool PieceWithin(ACR_BaseWork work, vector at, float within)
 {
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (vector.DistanceXZ(line.m_vMiddle, at) > line.m_fLength * 0.5 + within + 4) continue;
   foreach (ACR_LinePiece piece : line.m_aPieces) if (vector.DistanceXZ(piece.m_vPos, at) < within) return true;
  }
  return false;
 }

 // Our bags in a bunker's fan, close enough to stop it (they're as high as its ports).
 protected static bool BagsAhead(ACR_BaseWork work, vector at, float facing)
 {
  vector look = ACR_BaseSurvey.Dir(facing);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    float apart = vector.DistanceXZ(piece.m_vPos, at);
    if (apart > 30) continue;
    if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(at, piece.m_vPos), look) > 0.9) return true;
   }
  }
  return false;
 }

 // ---------------------------------------------------------------- wire and the command post

 // Wire in front of the lines (tier II): one site per line's middle part, a line of segments 7 m out,
 // where they fit.
 static void Wire(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  float length = ACR_Fortify.WireLength(survey.m_vCenter);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_iTier > 2 || line.m_fLength < 25) continue;
   ACR_BaseDefencePos pos = new ACR_BaseDefencePos();
   pos.m_eKind = ACR_EBaseDefenceKind.WIRE;
   pos.m_vPos = line.m_vMiddle;
   pos.m_vFacing = line.m_vFacing;
   int count = line.m_aPieces.Count();
   int step = Math.Max((int)Math.Ceil(length / LONG_STEP), 1);
   int mid = count / 2;
   int reach = Math.Min(MOST_WIRE / 2 * step, count / 2 - 1);
   for (int i = mid - reach; i <= mid + reach; i += step)
   {
    if (i < 0 || i >= count) continue;
    ACR_LinePiece piece = line.m_aPieces[i];
    if (!piece.m_bLong) continue;
    vector at = piece.m_vPos + piece.m_vFacing * 7;
    at[1] = ACR_BaseLiner.Ground(at);
    vector along = ACR_BaseDefences.FlatDir(piece.m_vFrom, piece.m_vTo);
    float yaw = ACR_BaseDefences.Bearing(along);
    if (ACR_Fortify.WireAlongX()) yaw -= 90;
    if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < 1.5 || ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, 4) < 3 || BBB_Terrain.IsWaterAt(at)) continue;
    if (!ACR_BaseDefences.WorldClear(at, ACR_BaseDefences.Bearing(piece.m_vFacing), Vector(length * 0.5, 0, 0.5))) continue;
    pos.m_aPiecePos.Insert(at);
    pos.m_aPieceYaw.Insert(yaw);
    pos.m_aPiecePrefab.Insert(ACR_Fortify.WIRE);
    pos.m_aPieceWall.Insert(false);
   }
   if (pos.m_aPiecePos.Count() < 2) continue;
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.PLANNED;
   site.m_Plan = pos;
   site.m_vPos = line.m_vMiddle;
   site.m_fYaw = ACR_BaseDefences.Bearing(line.m_vFacing);
   site.m_iSector = line.m_iSector;
   site.m_iTier = 2;
   site.m_iLine = work.m_aLines.Find(line);
   site.m_fScore = survey.WeakLight(site.m_iSector);
   work.m_aSites.Insert(site);
  }
 }

 // The command post: in a building behind the worst line's middle if one has a layout, else as near
 // the town's middle as before.
 static void CommandPost(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  array<ref ACR_BaseDefencePos> posts = {};
  ACR_BaseDefences.Posts(work.m_Base, ACR_BaseDefences.Kit(work.m_sFaction), survey.m_aDirs, survey.m_aWeights, survey.m_aBuildings, posts);
  if (posts.IsEmpty()) return;
  array<float> scores = {};
  foreach (ACR_BaseDefencePos post : posts)
  {
   survey.TrimPost(post);
   float score = post.m_fScore;
   if (!work.m_aLines.IsEmpty())
   {
    ACR_BaseLine front = work.m_aLines[0];
    float apart = vector.DistanceXZ(post.m_vPos, front.m_vMiddle);
    if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(post.m_vPos, front.m_vMiddle), front.m_vFacing) > 0.5) score += 20 - Math.Min(apart, 100) * 0.1;
   }
   scores.Insert(score);
  }
  int picked = ACR_Variety.Pick(scores, "base post " + work.m_Base.m_sName);
  ACR_BaseDefencePos chosen = posts[picked];
  ACR_BaseSite site = new ACR_BaseSite();
  site.m_eKind = ACR_EBaseSite.PLANNED;
  site.m_Plan = chosen;
  site.m_vPos = chosen.m_vPos;
  site.m_fYaw = ACR_BaseDefences.Bearing(chosen.m_vFacing);
  site.m_iSector = ACR_BaseSurvey.Sector(site.m_fYaw);
  site.m_iTier = 3;
  site.m_fScore = 1;
  work.m_aSites.Insert(site);
 }

 // ---------------------------------------------------------------- checkpoints

 // A checkpoint in a line's lane, on the road from each of the two nearest enemy bases: road,
 // checkpoint and line read as one thing. Worked last in its tier (NextPlan).
 static void Checkpoints(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  vector center = work.m_Base.GetOrigin();
  array<ACR_Base> enemies = {};
  array<float> dists = {};
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (!other || other.m_bRetired || other.m_sOwner.IsEmpty() || other.m_sOwner == work.m_sFaction) continue;
   enemies.Insert(other);
   dists.Insert(vector.DistanceXZ(center, other.GetOrigin()));
  }
  int made = 0;
  for (int round = 0; round < most && !enemies.IsEmpty(); round++)
  {
   int nearest = 0;
   foreach (int i, float d : dists) if (d < dists[nearest]) nearest = i;
   ACR_Base enemy = enemies[nearest];
   enemies.Remove(nearest);
   dists.Remove(nearest);
   vector toEnemy = ACR_BaseDefences.FlatDir(center, enemy.GetOrigin());
   array<vector> spots = {};
   array<float> yaws = {};
   array<float> scores = {};
   array<int> sectors = {};
   foreach (ACR_BaseLine line : work.m_aLines)
   {
    foreach (int l, vector lane : line.m_aLanes)
    {
     if (ACR_BaseDefences.DotXZ(line.m_aLaneOut[l], toEnemy) < -0.1) continue;
     vector sample = lane + line.m_aLaneOut[l] * CHECKPOINT_OUT;
     vector pos;
     float yaw;
     string refusal;
     if (!ACR_Roadblocks.ValidateSite(sample, pos, yaw, refusal, work.m_sFaction, enemy.GetOrigin(), work.m_Base)) continue;
     if (ACR_BaseWorks.CheckpointNear(work, pos, 60)) continue;
     spots.Insert(pos);
     yaws.Insert(yaw);
     sectors.Insert(line.m_iSector);
     scores.Insert(survey.Weak(line.m_iSector) + ACR_BaseDefences.DotXZ(line.m_aLaneOut[l], toEnemy));
    }
   }
   if (spots.IsEmpty())
   {
    PrintFormat("[ACR] BASEWORKS %1: no checkpoint for the road from %2: no line crosses a road that way", work.m_Base.m_sName, enemy.m_sName);
    continue;
   }
   int picked = ACR_Variety.Pick(scores, string.Format("base works road %1 %2", work.m_Base.m_sName, round));
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.ROADBLOCK;
   site.m_iTier = 2;
   site.m_vPos = spots[picked];
   site.m_fYaw = yaws[picked];
   site.m_iSector = sectors[picked];
   site.m_fScore = survey.Weak(site.m_iSector);
   work.m_aSites.Insert(site);
   survey.AddLight(site.m_iSector, ACR_BaseWorks.COVER_CHECKPOINT);
   made++;
  }
 }
}
