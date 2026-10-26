// Where the AI commander puts a roadblock (playtest 25 Oct: of the AI's roadblocks, one stood
// 18 m from a T-crossing and one 25 m from a road's dead end). ManageRoadblocks took the first
// road point 250-380 m out towards the enemy that ValidateSite accepted, and ValidateSite snaps to
// the nearest piece of road, its end included: road pieces end at junctions and dead ends.
// Now a fan of candidates towards the enemy (ACR_SiteScore.Fan) is snapped by ValidateSite and
// scored (Score):
// - sight down the road towards the enemy from a gunner's height, and a straight stretch ahead;
// - the road pointing at the enemy, a wider road, 250-400 m out;
// - chokepoints: a bridge, a cutting or embankment, woods on both sides; inside a town less;
// - a junction, dead end or side road joining within JUNCTION_CLEAR m costs score, and the site
//   is also tried that far along the road the other way (nudged);
// - a road that already has one of our roadblocks on it costs score (a second road into a base
//   first);
// - on the enemy's road route to the base (BBB_Roads.Route) scores more, and that route's points
//   200-450 m out are candidates too.
// Better siting never means fewer roadblocks (the user: "they are already kinda rare"): scores
// never refuse a site; below FLOOR the best valid one is still taken, and with no candidate at all
// the old straight-line first-fit is tried. Scored candidates are kept per base for CACHE_S.
class ACR_RoadSite : Managed
{
 vector m_vPos;          // on the road (ValidateSite's snap)
 float m_fYaw;           // facing the enemy
 ref ACR_RoadInfo m_Road;
 float m_fScore;
 float m_fSight;         // m of road seen towards the enemy
 float m_fFacing;        // cos of the road's heading against the way to the enemy
 float m_fOut;           // m from the base
 float m_fBreak;         // m along the road to the nearest junction, dead end or side road
 string m_sBreak;        // which of those ("" none within the walk)
 string m_sChoke;
 bool m_bNudged;         // moved along the road, away from a junction
 bool m_bBlocked;        // one of our roadblocks is on its road (set by Best)
 bool m_bOnRoute;        // on the enemy's road route to the base
 string m_sHow;          // how Best picked it: "scored", "below the floor", "first-fit fallback"
}

class ACR_RoadSiteCache : Managed
{
 float m_fAt;
 ref array<ref ACR_RoadSite> m_aSites = {};
 string m_sNone;         // why no candidate came up ("" some did)
 ref array<vector> m_aRoute = {}; // the enemy's road route to the base (empty: none)
 int m_iRouteMs;
}

class ACR_RoadblockSiting
{
 static const float JUNCTION_CLEAR = 30;  // m along the road from a junction, dead end or side road
 static const float EYE = 1.6;            // a gunner's eye over the road
 static const float SIGHT_MAX = 250;
 static const float SIGHT_STEP = 10;
 static const float SAME_ROAD = 20;       // a roadblock this close to the road walked is on it
 static const float CACHE_S = 600;
 static const float NEAR_OUT = 200;
 static const float FAR_OUT = 450;
 static const float FLOOR = 3;            // below this the best is still taken, logged so
 static const float ON_ROUTE = 2;         // the bonus for a site on the enemy's road route
 static const float ROUTE_NEAR = 15;      // m from the route that counts as on it
 static float s_fFloor = FLOOR;           // tests raise it
 static bool s_bNoFanForTest;             // tests: the fan finds nothing (the first-fit fallback)
 static int s_iEvaluated;                 // tests and the log
 protected static ref map<string, ref ACR_RoadSiteCache> s_mCache = new map<string, ref ACR_RoadSiteCache>();

 // ---------------------------------------------------------------- scoring

 // Score a road site facing 'yaw' (also used to look at roadblocks already standing).
 static ACR_RoadSite Describe(vector pos, float yaw, vector basePos, vector enemyPos)
 {
  s_iEvaluated++;
  ACR_RoadSite site = new ACR_RoadSite();
  site.m_vPos = pos;
  site.m_fYaw = yaw;
  site.m_Road = ACR_SiteScore.Road(pos, yaw);
  site.m_fOut = vector.DistanceXZ(pos, basePos);
  site.m_sChoke = ACR_SiteScore.Choke(pos, yaw);
  site.m_fBreak = ACR_SiteScore.WALK;
  if (site.m_Road)
  {
   site.m_fBreak = site.m_Road.NearestBreak(site.m_sBreak);
   site.m_fSight = ACR_SiteScore.SightAlongPath(pos, EYE, site.m_Road.m_aAhead, SIGHT_MAX, SIGHT_STEP);
   vector toEnemy = enemyPos - pos;
   toEnemy[1] = 0;
   float rad = site.m_Road.m_fYaw * Math.DEG2RAD;
   vector heading = Vector(Math.Sin(rad), 0, Math.Cos(rad));
   if (toEnemy.Length() > 1) site.m_fFacing = vector.Dot(heading, toEnemy.Normalized());
  }
  site.m_fScore = Score(site);
  return site;
 }

 static float Score(ACR_RoadSite site)
 {
  float score = Math.Min(site.m_fSight, SIGHT_MAX) / 50;            // up to 5
  if (site.m_Road)
  {
   score += Math.Min(site.m_Road.m_fStraight, 150) / 50;            // up to 3
   score += Math.Min(site.m_Road.m_fWidth, 8) / 4;                  // up to 2
  }
  score += 2 * Math.Max(site.m_fFacing, 0);                         // up to 2
  if (site.m_fOut >= 250 && site.m_fOut <= 400) score += 1;
  if (site.m_sChoke.Contains("bridge")) score += 2;
  if (site.m_sChoke.Contains("cutting") || site.m_sChoke.Contains("embankment")) score += 1.5;
  if (site.m_sChoke.Contains("woods")) score += 1;
  if (site.m_sChoke.Contains("town")) score -= 2;
  if (site.m_fBreak < JUNCTION_CLEAR) score -= 4 * (1 - site.m_fBreak / JUNCTION_CLEAR); // up to -4
  if (site.m_bOnRoute) score += ON_ROUTE;
  return score;
 }

 // One of our roadblocks stands on the road this site is on (within SAME_ROAD of the road
 // walked each way).
 static bool RoadBlocked(ACR_RoadSite site, string faction)
 {
  if (!site.m_Road) return false;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || rb.m_sFaction != faction) continue;
   if (ACR_SiteScore.OnPath(rb.m_vPos, site.m_Road.m_aAhead) <= SAME_ROAD || ACR_SiteScore.OnPath(rb.m_vPos, site.m_Road.m_aBehind) <= SAME_ROAD) return true;
  }
  return false;
 }

 // ---------------------------------------------------------------- candidates

 // The scored candidates round a base towards an enemy (cached CACHE_S per base and enemy).
 // 'none' says why there are none.
 static void Candidates(vector basePos, vector enemyPos, string faction, notnull array<ref ACR_RoadSite> sites, out string none)
 {
  none = "";
  string key = string.Format("%1|%2|%3", faction, Snap(basePos), Snap(enemyPos));
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_RoadSiteCache cache;
  if (!s_bNoFanForTest && s_mCache.Find(key, cache) && now - cache.m_fAt < CACHE_S * 1000)
  {
   foreach (ACR_RoadSite cached : cache.m_aSites) sites.Insert(cached);
   none = cache.m_sNone;
   return;
  }
  cache = new ACR_RoadSiteCache();
  cache.m_fAt = now;
  array<vector> samples = {};
  if (!s_bNoFanForTest)
  {
   ACR_SiteScore.Fan(basePos, enemyPos, NEAR_OUT, FAR_OUT, 50, 45, 15, samples);
   // The way the enemy's vehicles would come: its road route to the base, sampled 200-450 m out.
   int nodes;
   if (BBB_Roads.Route(enemyPos, basePos, cache.m_aRoute, nodes, cache.m_iRouteMs)) RouteSamples(cache.m_aRoute, basePos, samples);
   else cache.m_aRoute.Clear();
  }
  map<string, int> refused = new map<string, int>();
  foreach (vector sample : samples)
  {
   vector pos;
   float yaw;
   string refusal;
   if (!ACR_Roadblocks.ValidateSite(sample, pos, yaw, refusal, faction, enemyPos))
   {
    refused.Set(refusal, refused.Get(refusal) + 1);
    continue;
   }
   if (Seen(cache, pos)) continue;
   ACR_RoadSite site = Describe(pos, yaw, basePos, enemyPos);
   MarkRoute(site, cache.m_aRoute);
   cache.m_aSites.Insert(site);
   // Near a junction: also the spot that far along the road the other way.
   if (site.m_Road && site.m_fBreak < JUNCTION_CLEAR)
   {
    float metres = JUNCTION_CLEAR + 5 - site.m_fBreak;
    if (site.m_sBreak == site.m_Road.m_sAhead && site.m_fBreak == site.m_Road.m_fAhead) metres = -metres;
    vector moved;
    vector movedPos;
    float movedYaw;
    string movedWhy;
    if (site.m_Road.Along(metres, moved) && ACR_Roadblocks.ValidateSite(moved, movedPos, movedYaw, movedWhy, faction, enemyPos) && !Seen(cache, movedPos))
    {
     ACR_RoadSite nudged = Describe(movedPos, movedYaw, basePos, enemyPos);
     nudged.m_bNudged = true;
     MarkRoute(nudged, cache.m_aRoute);
     cache.m_aSites.Insert(nudged);
    }
   }
  }
  if (cache.m_aSites.IsEmpty())
  {
   if (samples.IsEmpty()) cache.m_sNone = "no candidates";
   else if (refused.Count() == 1 && refused.Contains("target position must be on or near a road")) cache.m_sNone = "no road within reach";
   else
   {
    cache.m_sNone = "every road site refused:";
    foreach (string why, int count : refused) cache.m_sNone += string.Format(" %1 x%2;", why, count);
   }
  }
  none = cache.m_sNone;
  if (!s_bNoFanForTest) s_mCache.Set(key, cache);
  foreach (ACR_RoadSite fresh : cache.m_aSites) sites.Insert(fresh);
 }

 // Points every 25 m along the route (legs run from the enemy to the base) 200-450 m from the base.
 static void RouteSamples(notnull array<vector> route, vector basePos, notnull array<vector> samples)
 {
  for (int i = 1; i < route.Count(); i++)
  {
   vector a = route[i - 1];
   vector b = route[i];
   float len = vector.DistanceXZ(a, b);
   for (float s = 0; s < len; s += 25)
   {
    vector p = a + (b - a) * (s / Math.Max(len, 0.01));
    float fromBase = vector.DistanceXZ(p, basePos);
    if (fromBase >= NEAR_OUT && fromBase <= FAR_OUT) samples.Insert(p);
   }
  }
 }

 static void MarkRoute(ACR_RoadSite site, notnull array<vector> route)
 {
  if (route.Count() < 2) return;
  site.m_bOnRoute = ACR_SiteScore.OnPath(site.m_vPos, route) <= ROUTE_NEAR;
  site.m_fScore = Score(site);
 }

 // The enemy's road route cached for a base (false when none): its length and what it cost to find.
 static bool RouteFor(vector basePos, vector enemyPos, string faction, out int length, out int ms)
 {
  string key = string.Format("%1|%2|%3", faction, Snap(basePos), Snap(enemyPos));
  ACR_RoadSiteCache cache;
  length = 0;
  ms = 0;
  if (!s_mCache.Find(key, cache) || cache.m_aRoute.Count() < 2) return false;
  for (int i = 1; i < cache.m_aRoute.Count(); i++) length += vector.DistanceXZ(cache.m_aRoute[i - 1], cache.m_aRoute[i]);
  ms = cache.m_iRouteMs;
  return true;
 }

 protected static bool Seen(ACR_RoadSiteCache cache, vector pos)
 {
  foreach (ACR_RoadSite other : cache.m_aSites)
  {
   if (vector.DistanceXZ(other.m_vPos, pos) < 25) return true;
  }
  return false;
 }

 protected static string Snap(vector p)
 {
  return string.Format("%1,%2", Math.Round(p[0] / 10), Math.Round(p[2] / 10));
 }

 // ---------------------------------------------------------------- picking

 // The site round a base, by the ladder: the best scored one still valid now (other roadblocks
 // may have gone up since it was scored), also below FLOOR; with no candidate, the old
 // straight-line first-fit. 'freeRoad': only roads without one of our roadblocks (a second road
 // into a base); else such roads just score less. null + 'none' when nothing at all.
 static ACR_RoadSite Best(vector basePos, vector enemyPos, string faction, bool freeRoad, out int considered, out string none)
 {
  array<ref ACR_RoadSite> sites = {};
  Candidates(basePos, enemyPos, faction, sites, none);
  considered = sites.Count();
  ACR_RoadSite best;
  float bestScore;
  foreach (ACR_RoadSite site : sites)
  {
   vector pos;
   float yaw;
   string refusal;
   if (!ACR_Roadblocks.ValidateSite(site.m_vPos, pos, yaw, refusal, faction, enemyPos)) continue;
   site.m_bBlocked = RoadBlocked(site, faction);
   if (freeRoad && site.m_bBlocked) continue;
   float score = site.m_fScore;
   if (site.m_bBlocked) score -= 4;
   if (best && score <= bestScore) continue;
   best = site;
   bestScore = score;
  }
  if (best)
  {
   best.m_sHow = "scored";
   if (bestScore < s_fFloor) best.m_sHow = "below the floor";
   none = "";
   return best;
  }
  // A second road wanted, none free: no second roadblock there (as before this change).
  if (freeRoad && considered > 0)
  {
   none = "every road there has a roadblock of ours";
   return null;
  }
  ACR_RoadSite old = FirstFit(basePos, enemyPos, faction);
  if (old)
  {
   old.m_sHow = "first-fit fallback";
   none = "";
  }
  return old;
 }

 // The old pick: the first road point 250-380 m out on the straight line to the enemy that
 // ValidateSite takes.
 static ACR_RoadSite FirstFit(vector basePos, vector enemyPos, string faction)
 {
  vector toEnemy = enemyPos - basePos;
  toEnemy[1] = 0;
  toEnemy.Normalize();
  for (float d = 250; d <= 380; d += 30)
  {
   vector sample = basePos + toEnemy * d;
   sample[1] = GetGame().GetWorld().GetSurfaceY(sample[0], sample[2]);
   vector pos;
   float yaw;
   string refusal;
   if (ACR_Roadblocks.ValidateSite(sample, pos, yaw, refusal, faction, enemyPos)) return Describe(pos, yaw, basePos, enemyPos);
  }
  return null;
 }

 static string Breakdown(ACR_RoadSite site)
 {
  if (!site) return "none";
  string choke = site.m_sChoke;
  if (choke.IsEmpty()) choke = "-";
  string road = "none near";
  if (site.m_fBreak < ACR_SiteScore.WALK) road = string.Format("%1 at %2 m", site.m_sBreak, (int)site.m_fBreak);
  float straight, width;
  if (site.m_Road)
  {
   straight = site.m_Road.m_fStraight;
   width = site.m_Road.m_fWidth;
  }
  string extra = "";
  if (site.m_bNudged) extra += ", nudged off a junction";
  if (site.m_bBlocked) extra += ", its road already blocked";
  if (site.m_bOnRoute) extra += ", on the enemy's road route";
  if (!site.m_sHow.IsEmpty()) extra += ", " + site.m_sHow;
  return string.Format("score %1 (sight %2 m, straight %3 m, faces enemy %4, width %5 m, %6 m out, nearest break %7, choke %8%9)", Math.Round(site.m_fScore * 10) / 10, (int)site.m_fSight, (int)straight, Math.Round(site.m_fFacing * 100) / 100, Math.Round(width * 10) / 10, (int)site.m_fOut, road, choke, extra);
 }

 static void Reset()
 {
  s_mCache.Clear();
  s_iEvaluated = 0;
  s_fFloor = FLOOR;
  s_bNoFanForTest = false;
 }
}
