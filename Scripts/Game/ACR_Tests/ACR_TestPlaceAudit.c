// -acrPlaceAuditTest 1 (server-capable, about 1 min; any map): how good each town's plan is, not only whether
// it breaks a rule. Every long piece is looked at from behind the bags: how far it sees out, whether it stands
// boxed in a yard, whether it faces the threat, whether it stands on hard ground or hugs a wall. Every way into
// the town is checked for a line across it and a gun on it. One AUDIT row per town and one AUDIT TOTAL row, the
// worst pieces as AUDITBAD rows and every town as PLANMAP rows for a drawing. It fails on nothing by itself: it
// has floors (FLOOR_*) that go up as the planner gets better.
class ACR_TestPlaceAudit
{
 static const float LOOK = 80;          // m looked out from a piece
 static const float SHORT = 25;         // m: a piece that sees less than this watches a fence, not an approach
 static const float BOX = 25;           // ... boxed: front and back both
 static const float GATE_NEAR = 30;     // m from a way in to a piece for it to be held
 static const float GUN_ON_GATE = 150;  // m a gun covers a way in from, within its fan
 static const float HUG = 3;            // m from a wall, running along it: pointless beside something as good
 static const float ALMOST = 14;        // m between two lines' ends that nearly meet: a hole, not a gap on purpose
 // What the planner has to reach, in percent (of long pieces, or of ways in).
 static const float FLOOR_SEEING = 60;
 static const float FLOOR_BOXED = 15;
 static const float FLOOR_AWAY = 15;
 static const float FLOOR_GATES = 50;
 static const float FLOOR_GUN_GATES = 30;

 protected static int s_iNext;
 protected static int s_iTowns;
 protected static int s_iPieces, s_iSeeing, s_iShort, s_iBoxed, s_iAway, s_iHard, s_iHug;
 protected static int s_iGates, s_iGatesHeld, s_iGatesGun, s_iNests, s_iNestsOnGate, s_iAlmost;
 protected static float s_fLookSum;
 protected static ref array<ACR_Base> s_aBases = {};
 protected static string s_sSide;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
  if (second < 5) return;
  if (s_aBases.IsEmpty() && s_iNext == 0)
  {
   s_sSide = ACR_FactionSelection.West();
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && !base.m_bRetired && !base.m_bHidden && !base.m_bBuilt) s_aBases.Insert(base);
  }
  for (int i = 0; i < 2 && s_iNext < s_aBases.Count(); i++)
  {
   Town(s_aBases[s_iNext]);
   s_iNext++;
  }
  if (s_iNext == s_aBases.Count() && s_iTowns > 0)
  {
   Totals();
   s_iTowns = 0;
  }
 }

 protected static int Percent(int part, int whole)
 {
  if (whole <= 0) return 100;
  return part * 100 / whole;
 }

 protected static void Town(ACR_Base base)
 {
  string faction = base.m_sOwner;
  if (faction.IsEmpty()) faction = s_sSide;
  ACR_BaseWorks.Reset();
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(base, faction);
  ACR_BaseSurvey survey = ACR_BaseWorks.s_LastSurvey;
  if (!work || !survey) return;
  s_iTowns++;
  array<vector> dirs = {};
  array<float> weights = {};
  ACR_BaseDefences.Threats(base, faction, dirs, weights);
  int pieces, seeing, shortView, boxed, away, hard, hug;
  float lookSum;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    if (!piece.m_bLong) continue;
    pieces++;
    float bearing = ACR_BaseDefences.Bearing(piece.m_vFacing);
    vector eye = piece.m_vPos - piece.m_vFacing * 0.4;
    eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_BAGS;
    float look = Math.Max(ACR_BaseSurvey.Reach(eye, bearing, LOOK), Math.Max(ACR_BaseSurvey.Reach(eye, bearing - 18, LOOK), ACR_BaseSurvey.Reach(eye, bearing + 18, LOOK)));
    lookSum += look;
    vector rear = piece.m_vPos + piece.m_vFacing * 0.6;
    rear[1] = ACR_BaseLiner.Ground(rear) + 1.0;
    float back = ACR_BaseSurvey.Reach(rear, bearing + 180, LOOK);
    string bad = "";
    if (look >= SHORT) seeing++;
    else
    {
     shortView++;
     bad += " short";
    }
    if (look < BOX && back < BOX)
    {
     boxed++;
     bad += " boxed";
    }
    if (ACR_BaseDefences.ThreatDot(dirs, weights, piece.m_vFacing) < -0.2 && work.m_Style.m_iKind != ACR_BaseStyle.RING)
    {
     away++;
     bad += " away";
    }
    if (ACR_Fortify.HardSurface(piece.m_vPos))
    {
     hard++;
     bad += " hard";
    }
    if (Hugs(survey, piece))
    {
     hug++;
     bad += " hug";
    }
    if (!bad.IsEmpty()) PrintFormat("AUDITBAD|%1|%2|%3|%4|%5", base.m_sName, piece.m_vPos[0], piece.m_vPos[2], (int)look, bad);
   }
  }
  // The ways in.
  int gates, held, gunned;
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (survey.m_aThreat[gate.m_iSector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
   gates++;
   bool isHeld = false;
   foreach (ACR_BaseLine gated : work.m_aLines)
   {
    foreach (vector lane : gated.m_aLanes) if (vector.DistanceXZ(lane, gate.m_vPos) < GATE_NEAR) isHeld = true;
    foreach (ACR_LinePiece near : gated.m_aPieces) if (vector.DistanceXZ(near.m_vPos, gate.m_vPos) < GATE_NEAR) isHeld = true;
   }
   bool isGunned = false;
   foreach (ACR_BaseSite site : work.m_aSites)
   {
    if (site.m_eKind != ACR_EBaseSite.NEST && site.m_eKind != ACR_EBaseSite.BUNKER) continue;
    vector to = gate.m_vPos - site.m_vPos;
    to[1] = 0;
    float apart = to.Length();
    if (apart > GUN_ON_GATE || apart < 1) continue;
    to.Normalize();
    if (vector.Dot(to, ACR_BaseSurvey.Dir(site.m_fYaw)) > 0.7) isGunned = true;
   }
   if (isHeld) held++;
   if (isGunned) gunned++;
   if (!isHeld) PrintFormat("AUDITGATE|%1|%2|%3|open", base.m_sName, gate.m_vPos[0], gate.m_vPos[2]);
  }
  // Lines' ends that nearly meet.
  int almost;
  foreach (int a, ACR_BaseLine one : work.m_aLines)
  {
   if (one.m_aPieces.IsEmpty()) continue;
   for (int b = a + 1; b < work.m_aLines.Count(); b++)
   {
    ACR_BaseLine two = work.m_aLines[b];
    if (two.m_aPieces.IsEmpty()) continue;
    array<vector> ends = {one.m_aPieces[0].m_vFrom, one.m_aPieces[one.m_aPieces.Count() - 1].m_vTo};
    array<vector> others = {two.m_aPieces[0].m_vFrom, two.m_aPieces[two.m_aPieces.Count() - 1].m_vTo};
    foreach (vector e : ends)
     foreach (vector o : others)
      if (vector.DistanceXZ(e, o) < ALMOST) almost++;
   }
  }
  int nests;
  foreach (ACR_BaseSite gun : work.m_aSites) if (gun.m_eKind == ACR_EBaseSite.NEST) nests++;
  s_iPieces += pieces;
  s_iSeeing += seeing;
  s_iShort += shortView;
  s_iBoxed += boxed;
  s_iAway += away;
  s_iHard += hard;
  s_iHug += hug;
  s_iGates += gates;
  s_iGatesHeld += held;
  s_iGatesGun += gunned;
  s_iNests += nests;
  s_iAlmost += almost;
  s_fLookSum += lookSum;
  int mean = 0;
  if (pieces > 0) mean = lookSum / pieces;
  PrintFormat("AUDIT|%1|%2|pieces %3|lines %4|nests %5", base.m_sName, ACR_BaseStyle.Word(work.m_Style.m_iKind), pieces, work.m_aLines.Count(), nests);
  PrintFormat("AUDIT2|%1|sees %2%%|mean %3 m|short %4|boxed %5|away %6|hard %7|hug %8", base.m_sName, Percent(seeing, pieces), mean, shortView, boxed, away, hard, hug);
  PrintFormat("AUDIT3|%1|gates %2|held %3|gunned %4|almost %5|ms %6", base.m_sName, gates, held, gunned, almost, ACR_BaseWorks.s_iLastPlanMs);
 }

 // Runs along a wall or fence, close to it.
 protected static bool Hugs(ACR_BaseSurvey survey, ACR_LinePiece piece)
 {
  vector along = ACR_BaseDefences.FlatDir(piece.m_vFrom, piece.m_vTo);
  int count = survey.m_aWallEnds.Count();
  for (int i = 0; i + 1 < count; i += 2)
  {
   vector a = survey.m_aWallEnds[i];
   if (vector.DistanceXZ(a, piece.m_vPos) > HUG + 15) continue;
   ACR_BaseDefenceSeg seg = new ACR_BaseDefenceSeg();
   seg.m_vA = a;
   seg.m_vB = survey.m_aWallEnds[i + 1];
   vector q;
   if (ACR_BaseDefences.SegDistance(seg, piece.m_vPos, q) >= HUG) continue;
   if (Math.AbsFloat(ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(seg.m_vA, seg.m_vB), along)) > 0.85) return true;
  }
  return false;
 }

 protected static void Totals()
 {
  int mean = 0;
  if (s_iPieces > 0) mean = s_fLookSum / s_iPieces;
  PrintFormat("AUDIT TOTAL pieces %1, sees %2%%, mean %3 m, short %4, boxed %5, away %6, hard %7, hug %8", s_iPieces, Percent(s_iSeeing, s_iPieces), mean, s_iShort, s_iBoxed, s_iAway, s_iHard, s_iHug);
  PrintFormat("AUDIT TOTAL gates %1, held %2%%, gunned %3%%, nests %4, almost meeting %5", s_iGates, Percent(s_iGatesHeld, s_iGates), Percent(s_iGatesGun, s_iGates), s_iNests, s_iAlmost);
  ACR_TestRegress.Check("placeaudit: most pieces see out", Percent(s_iSeeing, s_iPieces) >= FLOOR_SEEING, string.Format("%1%%", Percent(s_iSeeing, s_iPieces)));
  ACR_TestRegress.Check("placeaudit: few pieces boxed in", Percent(s_iBoxed, s_iPieces) <= FLOOR_BOXED, string.Format("%1%%", Percent(s_iBoxed, s_iPieces)));
  ACR_TestRegress.Check("placeaudit: few pieces face away from the threat", Percent(s_iAway, s_iPieces) <= FLOOR_AWAY, string.Format("%1%%", Percent(s_iAway, s_iPieces)));
  ACR_TestRegress.Check("placeaudit: the ways in are held", Percent(s_iGatesHeld, s_iGates) >= FLOOR_GATES, string.Format("%1%%", Percent(s_iGatesHeld, s_iGates)));
  ACR_TestRegress.Check("placeaudit: guns on the ways in", Percent(s_iGatesGun, s_iGates) >= FLOOR_GUN_GATES, string.Format("%1%%", Percent(s_iGatesGun, s_iGates)));
  ACR_TestRegress.Complete("placeaudit", 5);
 }
}
