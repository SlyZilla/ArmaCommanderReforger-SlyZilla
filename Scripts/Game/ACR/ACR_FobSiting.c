// Where the AI commander puts a FOB (the user, 26 Oct: the roadblock-style scoring for FOB sites).
// FindFobSite took the first of fixed offsets back from the front base towards our nearest deploy
// base (450, 550, 350, 650 and 750 m, each at 0, +-25, +-50 and +-75 deg) that
// ACR_FobBuild.ValidateSite accepted (an HQ and at least MIN_CLEAR_SLOTS of the 8 upgrade slots),
// 600 m from enemy bases and not contested. Now every one of those offsets that passes is scored
// (Score), and the best is taken:
// - room: the HQ, all 8 upgrade slots and all 7 defence spots clear;
// - concealment: hidden from the enemy base and from points on its approach to the front base
//   (a reverse slope, woods or buildings between);
// - a road close by for the construction truck and its loads;
// - the distance back from the front base, 450-550 m best;
// - cover: our roadblocks and Fortify positions within COVER_REACH;
// - off the enemy's road route to the front base (BBB_Roads.Route, as the roadblock siting);
// - inside a town costs score.
// Where the old offsets give fewer than ENOUGH sites with every spot clear, a wider fan is tried
// too (300-900 m back, out to 120 deg either side): the playtest of 26 Oct had "no clear FOB site"
// at two bases, five times. Never fewer FOBs: the score only orders the sites that pass, and the
// old offsets are all validated first, so the old first fit is always one of them. Validating a site is the costly part (an HQ and
// 15 spots each), so a step validates at most VALIDATE_PER_STEP (ACR_AICommander.ManageFobBuild
// spreads a search over a few ticks), and a front base's scored sites are kept for CACHE_S.
class ACR_FobSite : Managed
{
 vector m_vPos;
 int m_iOrder;          // its place in the old order (0: what the first fit tried first)
 int m_iSlots;          // clear upgrade slots of ACR_FobSlots.SLOT_COUNT
 int m_iDefences;       // clear defence spots of ACR_FobDefences.SPOT_COUNT
 int m_iHidden;         // of m_iEyes enemy eyes, how many can't see it
 int m_iEyes;
 float m_fRoad;         // m to the nearest road (-1: none found)
 float m_fBack;         // m back from the front base
 int m_iCover;          // our roadblocks and Fortify positions within COVER_REACH
 bool m_bOnRoute;       // within ROUTE_NEAR of the enemy's road route to the front base
 bool m_bTown;
 float m_fScore;
}

class ACR_FobSiteCache : Managed
{
 ref array<vector> m_aTodo = {};           // offsets still to validate, in the old order
 int m_iNext;
 int m_iOld;                               // the old offsets come first, this many
 bool m_bDone;                             // the old offsets gave ENOUGH: no wider look
 ref array<ref ACR_FobSite> m_aSites = {};
 ref array<vector> m_aRoute = {};          // the enemy's road route to the front base
 vector m_vEnemy;
 float m_fAt;
 int m_iRefused;
 string m_sLastRefusal;
}

class ACR_FobSiting
{
 static const int VALIDATE_PER_STEP = 6;
 static const int ENOUGH = 3;           // sites with every spot clear from the old offsets: no wider fan
 static const float CACHE_S = 600;
 static const float EYE = 2;            // m: an enemy's eye over the ground
 static const float HQ_TOP = 3;         // m: the top of the HQ, what an enemy would see of it
 static const float SIGHT_MAX = 1500;   // m: an eye further off doesn't count
 static const float COVER_REACH = 400;
 static const float ROUTE_NEAR = 100;
 static const float ROAD_NEAR = 40;
 static const float ROAD_FAR = 120;
 static const float ENEMY_CLEAR = 600;  // m from an enemy or neutral base, as before
 protected static ref map<string, ref ACR_FobSiteCache> s_mCache = new map<string, ref ACR_FobSiteCache>();

 // The search for a FOB behind 'front' ('back': the way to our nearest deploy base; 'enemy': the
 // nearest enemy base, vector.Zero for none). Validates at most 'most' more offsets (-1: all that
 // are left) and fills 'sites' with those scored so far; true when the search is complete.
 static bool Step(string faction, ACR_Base front, vector back, vector enemy, int most, notnull array<ref ACR_FobSite> sites, out string none)
 {
  none = "";
  string key = faction + "|" + front.m_sName;
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_FobSiteCache cache;
  if (!s_mCache.Find(key, cache) || now - cache.m_fAt > CACHE_S * 1000 || vector.DistanceXZ(cache.m_vEnemy, enemy) > 50)
  {
   cache = Start(front, back, enemy);
   s_mCache.Set(key, cache);
  }
  int done = 0;
  while (!cache.m_bDone && cache.m_iNext < cache.m_aTodo.Count() && (most < 0 || done < most))
  {
   if (cache.m_iNext == cache.m_iOld && Full(cache) >= ENOUGH)
   {
    cache.m_bDone = true;
    break;
   }
   int order = cache.m_iNext;
   cache.m_iNext++;
   done++;
   vector spot;
   string refusal;
   if (!Validate(cache.m_aTodo[order], faction, spot, refusal))
   {
    cache.m_iRefused++;
    cache.m_sLastRefusal = refusal;
    continue;
   }
   if (Seen(cache, spot)) continue;
   ACR_FobSite site = Describe(spot, order, front, faction, cache);
   cache.m_aSites.Insert(site);
  }
  foreach (ACR_FobSite scored : cache.m_aSites) sites.Insert(scored);
  bool complete = cache.m_bDone || cache.m_iNext >= cache.m_aTodo.Count();
  if (complete && sites.IsEmpty()) none = string.Format("%1 offsets, none clear (last: %2)", cache.m_aTodo.Count(), cache.m_sLastRefusal);
  return complete;
 }

 // The old offsets in their order, the enemy's road route.
 protected static ACR_FobSiteCache Start(ACR_Base front, vector back, vector enemy)
 {
  ACR_FobSiteCache cache = new ACR_FobSiteCache();
  cache.m_fAt = GetGame().GetWorld().GetWorldTime();
  cache.m_vEnemy = enemy;
  array<float> dists = {450.0, 550.0, 350.0, 650.0, 750.0};
  array<float> angles = {0.0, 25.0, -25.0, 50.0, -50.0, 75.0, -75.0};
  foreach (float d : dists)
  {
   foreach (float a : angles)
   {
    float rad = a * Math.DEG2RAD;
    float cosA = Math.Cos(rad);
    float sinA = Math.Sin(rad);
    vector rotated = Vector(back[0] * cosA - back[2] * sinA, 0, back[0] * sinA + back[2] * cosA);
    vector candidate = front.GetOrigin() + rotated * d;
    candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
    cache.m_aTodo.Insert(candidate);
   }
  }
  cache.m_iOld = cache.m_aTodo.Count();
  // The wider fan, nearest the old band first.
  array<float> wideDists = {500.0, 400.0, 600.0, 300.0, 700.0, 800.0, 900.0};
  array<float> wideAngles = {0.0, 15.0, -15.0, 35.0, -35.0, 60.0, -60.0, 90.0, -90.0, 120.0, -120.0};
  foreach (float wd : wideDists)
  {
   foreach (float wa : wideAngles)
   {
    float wrad = wa * Math.DEG2RAD;
    vector wrotated = Vector(back[0] * Math.Cos(wrad) - back[2] * Math.Sin(wrad), 0, back[0] * Math.Sin(wrad) + back[2] * Math.Cos(wrad));
    vector wide = front.GetOrigin() + wrotated * wd;
    wide[1] = GetGame().GetWorld().GetSurfaceY(wide[0], wide[2]);
    cache.m_aTodo.Insert(wide);
   }
  }
  if (enemy != vector.Zero)
  {
   int nodes, ms;
   if (!BBB_Roads.Route(enemy, front.GetOrigin(), cache.m_aRoute, nodes, ms)) cache.m_aRoute.Clear();
  }
  return cache;
 }

 // As FindFobSite checked an offset: ValidateSite's spot, ENEMY_CLEAR from enemy and neutral
 // bases, not contested.
 protected static bool Validate(vector candidate, string faction, out vector spot, out string refusal)
 {
  if (!ACR_FobBuild.ValidateSite(candidate, spot, refusal, "", 20)) return false;
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (b.m_sOwner == faction || b.m_sOwner.IsEmpty() || b.m_bRetired) continue;
   if (vector.DistanceXZ(b.GetOrigin(), spot) < ENEMY_CLEAR)
   {
    refusal = "within 600 m of " + b.m_sName;
    return false;
   }
  }
  if (ACR_FobBuild.IsContested(spot, faction))
  {
   refusal = "contested";
   return false;
  }
  return true;
 }

 protected static ACR_FobSite Describe(vector spot, int order, ACR_Base front, string faction, ACR_FobSiteCache cache)
 {
  ACR_FobSite site = new ACR_FobSite();
  site.m_vPos = spot;
  site.m_iOrder = order;
  string why;
  int slots, defences;
  ACR_FobBuild.CheckSpotCount(spot, why, slots, null, defences);
  site.m_iSlots = slots;
  site.m_iDefences = defences;
  site.m_fBack = vector.DistanceXZ(spot, front.GetOrigin());
  site.m_fRoad = RoadDistance(spot);
  site.m_bTown = ACR_SiteScore.Buildings(spot, 40) >= 4;
  if (cache.m_aRoute.Count() >= 2) site.m_bOnRoute = ACR_SiteScore.OnPath(spot, cache.m_aRoute) <= ROUTE_NEAR;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == faction && vector.DistanceXZ(rb.m_vPos, spot) <= COVER_REACH) site.m_iCover++;
  }
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction == faction && vector.DistanceXZ(fort.m_vPos, spot) <= COVER_REACH) site.m_iCover++;
  }
  // The enemy's eyes: its base, and three points on the line from it to the front base.
  if (cache.m_vEnemy != vector.Zero)
  {
   for (int e = 0; e < 4; e++)
   {
    vector eye = cache.m_vEnemy + (front.GetOrigin() - cache.m_vEnemy) * (e * 0.25);
    if (vector.DistanceXZ(eye, spot) > SIGHT_MAX) continue;
    site.m_iEyes++;
    if (!Sees(eye, spot)) site.m_iHidden++;
   }
  }
  site.m_fScore = Score(site);
  return site;
 }

 static float Score(ACR_FobSite s)
 {
  float score = 0;
  // Room: an HQ that can't take all its upgrades stays weak.
  if (s.m_iSlots >= ACR_FobSlots.SLOT_COUNT && s.m_iDefences >= ACR_FobDefences.SPOT_COUNT) score += 3;
  else score += 2.0 * s.m_iSlots / ACR_FobSlots.SLOT_COUNT + 0.5 * s.m_iDefences / ACR_FobDefences.SPOT_COUNT;
  // Hidden from the enemy's side (no eye within SIGHT_MAX: as good as hidden).
  if (s.m_iEyes > 0) score += 3.0 * s.m_iHidden / s.m_iEyes;
  else score += 3;
  // A road for the trucks.
  if (s.m_fRoad >= 0 && s.m_fRoad <= ROAD_NEAR) score += 2;
  else if (s.m_fRoad >= 0 && s.m_fRoad <= ROAD_FAR) score += 1;
  // 450-550 m back from the front base best.
  score += Math.Clamp(1 - Math.AbsFloat(s.m_fBack - 500) / 250, 0, 1);
  score += Math.Min(s.m_iCover * 0.5, 1.5);
  if (s.m_bOnRoute) score -= 2;
  if (s.m_bTown) score -= 2;
  return score;
 }

 static string Breakdown(ACR_FobSite s)
 {
  string room = string.Format("%1 of %2 upgrade slots, %3 of %4 defence spots clear", s.m_iSlots, ACR_FobSlots.SLOT_COUNT, s.m_iDefences, ACR_FobDefences.SPOT_COUNT);
  string hidden = "no enemy eye within 1.5 km";
  if (s.m_iEyes > 0) hidden = string.Format("hidden from %1 of %2 enemy eyes", s.m_iHidden, s.m_iEyes);
  string road = "no road found";
  if (s.m_fRoad >= 0) road = string.Format("road at %1 m", (int)s.m_fRoad);
  string text = string.Format("score %1 (%2; %3; %4; %5 m back", s.m_fScore, room, hidden, road, (int)s.m_fBack);
  if (s.m_iCover > 0) text += string.Format("; covered by %1 of ours", s.m_iCover);
  if (s.m_bOnRoute) text += "; on the enemy's road route";
  if (s.m_bTown) text += "; in a town";
  text += string.Format("; the old order's #%1)", s.m_iOrder + 1);
  return text;
 }

 // The best scored site not avoided (ACR_AICommander.Avoided decides; 'avoid' lists them), null with none.
 // The site to take (none avoided within 150 m): the best, or with variety on (ACR_Variety) one of
 // those within 12% of its score, weighted towards the best, seeded by 'key' (the side, the front,
 // the sites it gave up on), so two FOBs behind one front can differ between matches. Equal scores
 // go to the earlier of the old offsets, as before.
 static ACR_FobSite Best(notnull array<ref ACR_FobSite> sites, notnull array<vector> avoid, string key = "")
 {
  array<ACR_FobSite> open = {};
  foreach (ACR_FobSite site : sites)
  {
   bool skipped = false;
   foreach (vector a : avoid)
   {
    if (vector.DistanceXZ(a, site.m_vPos) < 150) skipped = true;
   }
   if (skipped) continue;
   // Kept in the old offsets' order, so the first of equal scores is the earlier one.
   int at = open.Count();
   while (at > 0 && open[at - 1].m_iOrder > site.m_iOrder) at--;
   open.InsertAt(site, at);
  }
  array<float> scores = {};
  foreach (ACR_FobSite candidate : open) scores.Insert(candidate.m_fScore);
  int pick = ACR_Variety.Pick(scores, "fob site " + key);
  if (pick < 0) return null;
  return open[pick];
 }

 static void Reset() { s_mCache.Clear(); }

 protected static int Full(ACR_FobSiteCache cache)
 {
  int full;
  foreach (ACR_FobSite site : cache.m_aSites)
  {
   if (site.m_iSlots >= ACR_FobSlots.SLOT_COUNT && site.m_iDefences >= ACR_FobDefences.SPOT_COUNT) full++;
  }
  return full;
 }

 protected static bool Seen(ACR_FobSiteCache cache, vector pos)
 {
  foreach (ACR_FobSite other : cache.m_aSites)
  {
   if (vector.DistanceXZ(other.m_vPos, pos) < 15) return true;
  }
  return false;
 }

 protected static float RoadDistance(vector pos)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return -1;
  BaseRoad road;
  float distance;
  aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, distance);
  if (!road) return -1;
  return distance;
 }

 // Whether an enemy's eye at 'from' (EYE over the ground) sees the HQ's top at 'site'.
 protected static bool Sees(vector from, vector site)
 {
  BaseWorld world = GetGame().GetWorld();
  vector eye = from;
  eye[1] = world.GetSurfaceY(from[0], from[2]) + EYE;
  vector top = site;
  top[1] = world.GetSurfaceY(site[0], site[2]) + HQ_TOP;
  TraceParam trace = new TraceParam();
  trace.Start = eye;
  trace.End = top;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return world.TraceMove(trace, SightFilter) >= 1;
 }

 protected static bool SightFilter(notnull IEntity e)
 {
  return !ChimeraCharacter.Cast(e) && !Vehicle.Cast(e);
 }
}
