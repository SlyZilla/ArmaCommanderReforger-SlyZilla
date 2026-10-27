// -acrFobSitingTest 1 (server-capable, ~1 min): where the AI commander puts a FOB (ACR_FobSiting).
// For every US base, the old first fit (FindFobSite's offsets in their order, the first that
// passes, as it was) against the scored search:
// - never fewer FOBs: wherever the old first fit found a site, the scored search does too;
// - the pick has all 8 upgrade slots and 7 defence spots clear wherever any site there does;
// - the old first fit is among the scored sites;
// - a search in steps of VALIDATE_PER_STEP ends (the wider fan included) and picks what the whole one picks;
// - the score's terms move as they should;
// - an abandoned site is skipped and the next best taken.
// Each base's two sites and the pick's breakdown are logged as "[ACR] FOBSITING ...".
class ACR_TestFobSiting
{
 static const int CHECKS = 6;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  // The checks are on the best site: no variety (ACR_Variety).
  ACR_Variety.s_bOff = true;
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ACR_FobSiting.Reset();

  int oldFound = 0;
  int newFound = 0;
  int fewer = 0;
  int fullAvailable = 0;
  int fullPicked = 0;
  int oldMissing = 0;
  ACR_Base stepBase;
  ACR_Base twoSites;
  string detail = "";
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != "US") continue;
   vector oldSite;
   bool old = OldFirstFit(ai, base, oldSite);
   vector newSite;
   bool found = ai.FindFobSite(base, newSite);
   array<ref ACR_FobSite> sites = {};
   string none;
   ACR_FobSiting.Step("US", base, Back(ai, base), Enemy(base), -1, sites, none);
   if (old) oldFound++;
   if (found) newFound++;
   if (old && !found) fewer++;
   bool anyFull = false;
   foreach (ACR_FobSite s : sites)
   {
    if (s.m_iSlots >= ACR_FobSlots.SLOT_COUNT && s.m_iDefences >= ACR_FobDefences.SPOT_COUNT) anyFull = true;
   }
   array<vector> noAvoid = {};
   ACR_FobSite best = ACR_FobSiting.Best(sites, noAvoid);
   if (anyFull)
   {
    fullAvailable++;
    if (best && best.m_iSlots >= ACR_FobSlots.SLOT_COUNT && best.m_iDefences >= ACR_FobDefences.SPOT_COUNT) fullPicked++;
   }
   if (old)
   {
    bool among = false;
    foreach (ACR_FobSite t : sites)
    {
     if (vector.DistanceXZ(t.m_vPos, oldSite) < 15) among = true;
    }
    if (!among) oldMissing++;
   }
   if (!stepBase && found) stepBase = base;
   // A base with another clear site 150 m or more from the best (an abandoned site is avoided that far).
   if (!twoSites && best)
   {
    foreach (ACR_FobSite other : sites)
    {
     if (vector.DistanceXZ(other.m_vPos, best.m_vPos) >= 150) twoSites = base;
    }
   }
   string pick = "none";
   if (best) pick = ACR_FobSiting.Breakdown(best);
   PrintFormat("[ACR] FOBSITING %1: old first fit %2 at %3; scored %4 at %5 of %6 clear sites: %7", base.m_sName, old, oldSite, found, newSite, sites.Count(), pick);
  }
  detail = string.Format("old first fit found %1, scored %2, fewer %3", oldFound, newFound, fewer);
  ACR_TestRegress.Check("fobsiting: wherever the old first fit found a FOB site, the scored search does too", oldFound > 0 && fewer == 0, detail);
  ACR_TestRegress.Check("fobsiting: the pick has every upgrade slot and defence spot clear wherever a site there does", fullPicked == fullAvailable, string.Format("%1 of %2 bases", fullPicked, fullAvailable));
  ACR_TestRegress.Check("fobsiting: the old first fit is among the scored sites", oldFound > 0 && oldMissing == 0, string.Format("missing at %1 of %2 bases", oldMissing, oldFound));

  // In steps, as ManageFobBuild runs it.
  bool stepped = false;
  string stepDetail = "no base with a site";
  if (stepBase)
  {
   vector whole;
   ai.FindFobSite(stepBase, whole);
   ACR_FobSiting.Reset();
   int steps = 0;
   bool pending = true;
   vector part;
   bool got = false;
   while (pending && steps < 40)
   {
    steps++;
    got = ai.FindFobSiteStep(stepBase, part, ACR_FobSiting.VALIDATE_PER_STEP, pending);
   }
   stepped = got && !pending && vector.DistanceXZ(part, whole) < 1;
   stepDetail = string.Format("%1: %2 steps, found %3, %4 m from the whole search's pick", stepBase.m_sName, steps, got, (int)vector.DistanceXZ(part, whole));
  }
  ACR_TestRegress.Check("fobsiting: a search in steps ends and picks what the whole search picks", stepped, stepDetail);

  // The terms.
  ACR_FobSite plain = Synthetic();
  float baseScore = ACR_FobSiting.Score(plain);
  ACR_FobSite hidden = Synthetic();
  hidden.m_iHidden = 4;
  ACR_FobSite road = Synthetic();
  road.m_fRoad = 20;
  ACR_FobSite full = Synthetic();
  full.m_iSlots = ACR_FobSlots.SLOT_COUNT;
  full.m_iDefences = ACR_FobDefences.SPOT_COUNT;
  ACR_FobSite farBack = Synthetic();
  farBack.m_fBack = 750;
  ACR_FobSite route = Synthetic();
  route.m_bOnRoute = true;
  ACR_FobSite town = Synthetic();
  town.m_bTown = true;
  ACR_FobSite covered = Synthetic();
  covered.m_iCover = 2;
  bool terms = ACR_FobSiting.Score(hidden) > baseScore && ACR_FobSiting.Score(road) > baseScore && ACR_FobSiting.Score(full) > baseScore && ACR_FobSiting.Score(farBack) < baseScore && ACR_FobSiting.Score(route) < baseScore && ACR_FobSiting.Score(town) < baseScore && ACR_FobSiting.Score(covered) > baseScore;
  ACR_TestRegress.Check("fobsiting: the score's terms move as they should", terms, string.Format("plain %1, hidden %2, road %3, all spots %4, 750 m back %5, on route %6, town %7, covered %8", baseScore, ACR_FobSiting.Score(hidden), ACR_FobSiting.Score(road), ACR_FobSiting.Score(full), ACR_FobSiting.Score(farBack), ACR_FobSiting.Score(route), ACR_FobSiting.Score(town), ACR_FobSiting.Score(covered)));

  // An abandoned site is skipped.
  bool skipped = false;
  string skipDetail = "no base with two clear sites 150 m apart";
  if (twoSites)
  {
   vector first;
   vector second;
   ai.FindFobSite(twoSites, first);
   ai.AvoidSite(first);
   bool again = ai.FindFobSite(twoSites, second);
   skipped = again && vector.DistanceXZ(first, second) >= 150;
   skipDetail = string.Format("%1: first %2, then %3 (%4 m apart)", twoSites.m_sName, first, second, (int)vector.DistanceXZ(first, second));
  }
  if (twoSites) ACR_TestRegress.Check("fobsiting: an abandoned site is skipped and the next best taken", skipped, skipDetail);
  else ACR_TestRegress.Skip("fobsiting: an abandoned site is skipped and the next best taken", skipDetail);
  ACR_FobSiting.Reset();
  ACR_TestRegress.Complete("fobsiting", CHECKS);
 }

 // What FindFobSite did before: the offsets in order, the first that passes.
 protected static bool OldFirstFit(ACR_AICommander ai, ACR_Base front, out vector site)
 {
  site = vector.Zero;
  vector dir = Back(ai, front);
  array<float> dists = {450.0, 550.0, 350.0, 650.0, 750.0};
  array<float> angles = {0.0, 25.0, -25.0, 50.0, -50.0, 75.0, -75.0};
  foreach (float d : dists)
  {
   foreach (float a : angles)
   {
    float rad = a * Math.DEG2RAD;
    vector rotated = Vector(dir[0] * Math.Cos(rad) - dir[2] * Math.Sin(rad), 0, dir[0] * Math.Sin(rad) + dir[2] * Math.Cos(rad));
    vector candidate = front.GetOrigin() + rotated * d;
    candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
    vector spot;
    string refusal;
    if (!ACR_FobBuild.ValidateSite(candidate, spot, refusal, "", 20)) continue;
    bool safe = true;
    foreach (ACR_Base b : ACR_Base.s_All)
    {
     if (b.m_sOwner != "US" && !b.m_sOwner.IsEmpty() && !b.m_bRetired && vector.DistanceXZ(b.GetOrigin(), spot) < 600.0) safe = false;
    }
    if (!safe || ACR_FobBuild.IsContested(spot, "US")) continue;
    site = spot;
    return true;
   }
  }
  return false;
 }

 protected static vector Back(ACR_AICommander ai, ACR_Base front)
 {
  ACR_Base deploy = ai.NearestDeployBase(front);
  if (!deploy) return Vector(0, 0, -1);
  vector dir = deploy.GetOrigin() - front.GetOrigin();
  dir[1] = 0;
  if (dir.LengthSq() <= 0.001) return Vector(0, 0, -1);
  dir.Normalize();
  return dir;
 }

 protected static vector Enemy(ACR_Base from)
 {
  vector best = vector.Zero;
  float bestDistance = float.MAX;
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (!b || b.m_bRetired || b.m_sOwner.IsEmpty() || b.m_sOwner == "US") continue;
   float d = vector.DistanceXZ(b.GetOrigin(), from.GetOrigin());
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = b.GetOrigin();
  }
  return best;
 }

 protected static ACR_FobSite Synthetic()
 {
  ACR_FobSite s = new ACR_FobSite();
  s.m_iSlots = 6;
  s.m_iDefences = 5;
  s.m_iEyes = 4;
  s.m_iHidden = 1;
  s.m_fRoad = 200;
  s.m_fBack = 500;
  return s;
 }
}
