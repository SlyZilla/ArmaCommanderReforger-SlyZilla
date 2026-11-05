// The screen (defence step 2, -acrDefenceForecast 2; Jacob, 4 Nov: "the forecast is blind without eyes
// forward"): a small element posted outside a threatened base, where it sees the way the attack is
// expected to come, early. It doesn't fight for the spot: its sightings go to the side's intel like
// any squad's (so the forecast gets the contacts and headings it needs), and it pulls back to the base
// once the enemy is close, joining the defence there. The AI commander runs it (ManageScreens); this
// class sites it.
class ACR_ScreenPost : Managed
{
 ACR_Squad m_Squad;
 ACR_Base m_Base;
 vector m_vSpot;
 float m_fBearing;     // degrees from the base toward the way it watches
 float m_fSince;       // world time it was posted
 float m_fQuietSince = -1; // world time the base stopped being watched (-1: still watched)
 int m_iSees;          // sample points it sees out along the way (of SAMPLES)
}

class ACR_DefenceScreen
{
 // Out near the threat ring (THREAT_DISTANCE past the radius): the AI notices walking men a few hundred metres off,
 // so a screen at 160 m saw an attack no earlier than the base (forecastdefend, 4 Nov).
 static const float OUT_LEAST = 250;  // m past the base's radius: the nearest spot tried
 static const float OUT_MOST = 550;   // ... and the farthest
 static const float OUT_STEP = 75;
 static const float TURN_MOST = 30;   // degrees either side of the way
 static const float TURN_STEP = 15;
 static const float LOOK_NEAR = 100;  // m past the spot: the first point it should see
 static const float LOOK_FAR = 700;   // ... and the last
 static const float LOOK_STEP = 100;
 static const float LOOK_FAN = 15;    // degrees: two more rays either side
 static const int SAMPLES = 21;       // 7 points x 3 rays
 static const float EYE = 1.7;        // m: a man kneeling on a crest
 static const float BODY = 1.0;       // m: a man walking, half hidden
 static const float HEIGHT_WEIGHT = 0.05; // a sample's worth per metre above the base
 static const float ENEMY_GAP = 250;  // m a spot keeps from an enemy base's ring
 static const float FAR_WEIGHT = 0.006; // a sample's worth per metre further out (seeing him earlier)

 static vector Toward(float bearing)
 {
  float r = bearing * Math.DEG2RAD;
  return Vector(Math.Sin(r), 0, Math.Cos(r));
 }

 // The spot outside 'base' that sees most of the way out along 'bearing' (ties to the higher one).
 // False: nothing dry and clear of enemy bases sees anything.
 static bool Spot(ACR_Base base, float bearing, notnull array<ACR_Base> enemyBases, out vector spot, out int sees)
 {
  BaseWorld world = GetGame().GetWorld();
  vector center = base.GetOrigin();
  float best = 0;
  sees = 0;
  for (float gap = OUT_LEAST; gap <= OUT_MOST; gap += OUT_STEP)
  {
   for (float turn = -TURN_MOST; turn <= TURN_MOST; turn += TURN_STEP)
   {
    float b = bearing + turn;
    vector candidate = ACR_Manager.Ground(center + Toward(b) * (base.m_fRadius + gap));
    if (BBB_Terrain.IsWaterAt(candidate) || NearEnemy(candidate, enemyBases)) continue;
    int seen = Sees(world, candidate, bearing);
    float score = seen + Math.Max(0, candidate[1] - center[1]) * HEIGHT_WEIGHT + gap * FAR_WEIGHT;
    if (seen == 0 || score <= best) continue;
    best = score;
    spot = candidate;
    sees = seen;
   }
  }
  return best > 0;
 }

 // Points out along the way (three rays round 'bearing', LOOK_NEAR..LOOK_FAR past 'eye') it sees.
 static int Sees(BaseWorld world, vector eye, float bearing)
 {
  vector from = eye + Vector(0, EYE, 0);
  int seen;
  for (float fan = -LOOK_FAN; fan <= LOOK_FAN; fan += LOOK_FAN)
  {
   vector dir = Toward(bearing + fan);
   for (float d = LOOK_NEAR; d <= LOOK_FAR; d += LOOK_STEP)
   {
    vector body = ACR_Manager.Ground(eye + dir * d) + Vector(0, BODY, 0);
    if (ACR_AttackGround.Sees(world, from, body)) seen++;
   }
  }
  return seen;
 }

 protected static bool NearEnemy(vector at, notnull array<ACR_Base> enemyBases)
 {
  foreach (ACR_Base enemy : enemyBases)
   if (enemy && vector.DistanceXZ(at, enemy.GetOrigin()) < enemy.m_fRadius + ENEMY_GAP) return true;
  return false;
 }
}
