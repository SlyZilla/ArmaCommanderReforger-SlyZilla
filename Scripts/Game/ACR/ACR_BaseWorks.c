// Base works (#105): Fortify on the middle of a base we hold fortifies the whole town, one tier per
// order, with FOB defences' tier names. A squad works it site by site, paying as it goes, so it can
// stop at any point and pick up where it left off: its own, or another squad's, next order on the
// base goes on with what is left, a lost site included.
//   Defences I:   the planner's street corners, house corners and perimeter positions (sandbags).
//   Defences II:  the planner's redoubts at junctions, MG nests on the approaches, a checkpoint on
//                 the road from each of the two nearest enemy bases; defenders in reserve, who man
//                 the nests' guns and deploy when an enemy comes near (ACR_Defenders).
//   Defences III: the planner's command post in a building, bunkers and guard towers; more
//                 defenders, who go into the buildings round the post when they deploy.
// Where the sites go starts from a survey of the base at the first order (ACR_BaseSurvey: its buildings,
// walls, roads in, what already stands there, and which approaches from the threat lie open). The
// sites fill the worst weak points first and each counts as cover, so they spread over the sectors;
// a spot that breaks a hard rule (no line of fire, against a building or a door, on a road) isn't
// used at all, and a town can get fewer sites for it. Each pick and which one comes next are weighted
// among the near-best (ACR_Variety), so no two towns go up alike. The works stay when the base falls:
// they are the new owner's, who can go on from there. A site whose walls are all gone can be rebuilt.
// The AI commander (-acrFortifyAI, ACR_AICommander.ManageBaseWorks) uses it at a threatened front
// base once it has the RP banked, and counts it in the base's defence (Credit).

enum ACR_EBaseSite
{
 PLANNED,  // the planner's sandbags and dressing
 NEST,
 BUNKER,   // a bunker or guard tower
 ROADBLOCK
}

class ACR_BaseSite : Managed
{
 ACR_EBaseSite m_eKind;
 int m_iTier;                       // 1-3
 vector m_vPos;
 float m_fYaw;
 float m_fScore;                    // threat and view
 ref ACR_BaseDefencePos m_Plan;     // PLANNED: its pieces
 ResourceName m_sPrefab;            // NEST, BUNKER
 ref array<IEntity> m_aWalls = {};  // what holds it up: gone when all of these are
 ref array<IEntity> m_aDressing = {};
 ACR_Roadblock m_Roadblock;
 bool m_bBuilt;
 bool m_bNoFit;                     // nothing could be placed: not tried again
 bool m_bSkipped;                   // a squad gave up on it (a truck parked there, no way in): the next order tries again
 int m_iSpot;                       // NEST, BUNKER: its spot in ACR_FobDefences.PrefabFor
 int m_iSector;                     // the sector of the survey it looks into
 float m_fReach;                    // NEST, BUNKER: clear m along its facing (the log, tests)
}

class ACR_BaseWork : Managed
{
 ACR_Base m_Base;
 string m_sFaction;
 int m_iTier;                       // the highest tier finished (0: none yet)
 ref array<ref ACR_BaseSite> m_aSites = {};
 ACR_DefenderPost m_Defenders;
 int m_iMannedTier;                 // the tier the reserve has been raised for (under this owner)
 int m_iPicks;                      // sites handed out, for the weighted pick's key
}

class ACR_BaseWorks
{
 static const float CLICK_RADIUS = 60;  // m from a base's centre: Fortify there means the whole base
 static const int MAX_WALL_SITES = 12;  // tier I sites on the biggest town (the piece count stays in check)
 static const int MAX_NESTS = 3;
 static const int MAX_BUNKERS = 3;
 static const int MAX_ROADBLOCKS = 2;
 static const float SITE_RADIUS = 30;   // squad members this close to a site work
 static const float SPOT_APART = 20;    // m between a nest or bunker and a site of another kind
 static const float GUN_APART = 35;     // m between nests and bunkers
 static const float SAME_WAY = 60;      // m between guns looking the same way (within 25 degrees)
 static const float FIRE_LANE = 30;     // m in front of a gun no site of ours may stand
 static const float COVER_BAGS = 0.25;  // how much a site counts in its sector (ACR_BaseSurvey.AddCover)
 static const float COVER_REDOUBT = 0.35;
 static const float COVER_CHECKPOINT = 0.4;
 static const float COVER_NEST = 0.6;
 static const float COVER_BUNKER = 0.5;
 static const float CHECKPOINT_PAST = 70; // m past the edge of the town a checkpoint may stand ...
 static const float CHECKPOINT_LEAST = 180; // ... and this far from the middle whatever the town's size
 static const float POST_AREA = 40;     // m round the command post its defenders garrison
 static const int AI_RP_RESERVE = 30;   // an AI commander keeps this much back (as ManageBaseWorks)
 static ref array<ref ACR_BaseWork> s_aWorks = {};
 static int s_iMaxSitesForTest;         // at most this many sites of each kind (0: no limit)
 static int s_iLastPlanMs;              // the last plan's cost (tests, log)
 static ref ACR_BaseSurvey s_LastSurvey; // the last plan's survey (tests)

 // -acrBaseWorks 0 turns it off; Fortify on a base is then refused, as before.
 static bool Enabled() { return ACR_Fortify.Enabled() && ACR_LogisticsRules.Choice("acrBaseWorks", "1") != "0"; }

 static int Defenders(int tier)
 {
  if (tier < 2) return 0;
  return ACR_Fortify.Defenders(tier) * 2;
 }

 static int Cost(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return 10;
   case ACR_EBaseSite.BUNKER: return 15;
   case ACR_EBaseSite.ROADBLOCK: return ACR_Roadblocks.Cost(ACR_ERoadblockType.MEDIUM);
  }
  if (!site.m_Plan) return 5;
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.PERIMETER: return 10;
   case ACR_EBaseDefenceKind.SQUARE: return 10;
   case ACR_EBaseDefenceKind.POST: return 20;
  }
  return 5;
 }

 // Seconds of work.
 static float Duration(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return 45;
   case ACR_EBaseSite.BUNKER: return 60;
   case ACR_EBaseSite.ROADBLOCK: return 40;
  }
  if (!site.m_Plan) return 15;
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.PERIMETER: return 30;
   case ACR_EBaseDefenceKind.SQUARE: return 40;
   case ACR_EBaseDefenceKind.POST: return 60;
  }
  return 15;
 }

 static string SiteWords(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return "MG nest";
   case ACR_EBaseSite.BUNKER: return "bunker";
   case ACR_EBaseSite.ROADBLOCK: return "checkpoint";
  }
  if (!site.m_Plan) return "sandbags";
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.CORNER: return "street corner";
   case ACR_EBaseDefenceKind.SQUARE: return "redoubt";
   case ACR_EBaseDefenceKind.EDGE: return "house corner";
   case ACR_EBaseDefenceKind.PERIMETER: return "perimeter position";
   case ACR_EBaseDefenceKind.POST: return "command post";
  }
  return "sandbags";
 }

 // ---------------------------------------------------------------- the order

 // The base of ours a Fortify click at 'point' means, if any (a FOB's is refused in Order).
 static ACR_Base BaseAt(vector point, string faction)
 {
  if (!Enabled()) return null;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_sOwner != faction) continue;
   if (vector.DistanceXZ(point, base.GetOrigin()) < CLICK_RADIUS) return base;
  }
  return null;
 }

 // The server side of the order: the next site of the tier that is open. A new order replaces the
 // squad's current job, like any other. false + refusal, nothing charged, when it can't.
 static bool Order(ACR_Squad squad, ACR_Base base, out string refusal)
 {
  refusal = "";
  if (!Enabled())
  {
   refusal = "fortifying a base is turned off for this match";
   return false;
  }
  if (!ACR_Fortify.CanFortify(squad) || !squad.GetFaction())
  {
   refusal = "only infantry can fortify";
   return false;
  }
  string faction = squad.GetFaction().GetFactionKey();
  if (!base || base.m_bRetired || base.m_sOwner != faction)
  {
   refusal = "that base isn't ours";
   return false;
  }
  if (base.m_bBuilt)
  {
   refusal = "a FOB builds its defences from its own panel";
   return false;
  }
  ACR_BaseWork work = WorkFor(base, faction);
  foreach (ACR_BaseSite skipped : work.m_aSites) skipped.m_bSkipped = false;
  int tier = OpenTier(work);
  if (tier == 0)
  {
   if (work.m_aSites.IsEmpty()) refusal = "no place to fortify here";
   else refusal = "this base is fortified as far as it goes";
   return false;
  }
  ACR_FortifyPlan plan = NextPlan(work, squad, tier, refusal);
  if (!plan) return false;
  if (!squad.Fortify(plan))
  {
   refusal = "unable to start the work";
   return false;
  }
  PrintFormat("[ACR] BASEWORKS %1 %2: %3 starts %4 (%5 sites open)", faction, base.m_sName, squad.GetName(), ACR_FobDefences.TierName(tier - 1), OpenSites(work, tier));
  return true;
 }

 static string Accepted(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return "fortifying the base";
  return string.Format("fortifying %1: %2", base.m_sName, ACR_FobDefences.TierName(OpenTier(work) - 1));
 }

 static ACR_BaseWork Find(ACR_Base base)
 {
  foreach (ACR_BaseWork work : s_aWorks) if (work.m_Base == base) return work;
  return null;
 }

 static int TierOf(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return 0;
  return work.m_iTier;
 }

 static ACR_BaseWork WorkFor(ACR_Base base, string faction)
 {
  ACR_BaseWork work = Find(base);
  if (work)
  {
   if (work.m_sFaction != faction) Transfer(work, faction);
   return work;
  }
  work = new ACR_BaseWork();
  work.m_Base = base;
  work.m_sFaction = faction;
  int started = System.GetTickCount();
  PlanSites(work);
  s_iLastPlanMs = System.GetTickCount() - started;
  PrintFormat("[ACR] BASEWORKS %1 %2: planned %3 sites in %4 ms", faction, base.m_sName, work.m_aSites.Count(), s_iLastPlanMs);
  s_aWorks.Insert(work);
  return work;
 }

 // The lowest tier with a site still to build: a lost site comes before the next tier.
 static int OpenTier(ACR_BaseWork work)
 {
  for (int tier = 1; tier <= ACR_Fortify.MAX_TIER; tier++)
   if (OpenSites(work, tier) > 0) return tier;
  return 0;
 }

 static int OpenSites(ACR_BaseWork work, int tier)
 {
  int open;
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_iTier == tier && !site.m_bBuilt && !site.m_bNoFit && !site.m_bSkipped) open++;
  return open;
 }

 static int BuiltSites(ACR_BaseWork work, int tier = 0)
 {
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_bBuilt && (tier == 0 || site.m_iTier == tier)) built++;
  return built;
 }

 // Another squad is at work on it.
 protected static bool Claimed(ACR_BaseSite site, ACR_Squad except)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad != except && squad.m_FortifyPlan && squad.m_FortifyPlan.m_Site == site) return true;
  return false;
 }

 // The squad's next site of this tier, near it and facing the threat.
 protected static ACR_FortifyPlan NextPlan(ACR_BaseWork work, ACR_Squad squad, int tier, out string refusal)
 {
  refusal = "";
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetAnchor()) return null;
  vector from = squad.GetAnchor().GetOrigin();
  array<ACR_BaseSite> open = {};
  array<float> scores = {};
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_iTier != tier || site.m_bBuilt || site.m_bNoFit || site.m_bSkipped || Claimed(site, squad)) continue;
   open.Insert(site);
   float score = site.m_fScore - vector.DistanceXZ(from, site.m_vPos) / 200;
   // A checkpoint is out of the way: everything else in the town first, then the walk.
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK) score -= 5;
   scores.Insert(score);
  }
  if (open.IsEmpty())
  {
   refusal = "another squad is on what is left to build";
   return null;
  }
  work.m_iPicks++;
  int picked = ACR_Variety.Pick(scores, string.Format("base works %1 %2 %3", work.m_Base.m_sName, tier, work.m_iPicks));
  ACR_BaseSite site = open[picked];
  ACR_FortifyPlan plan = new ACR_FortifyPlan();
  plan.m_sFaction = work.m_sFaction;
  plan.m_vSite = site.m_vPos;
  plan.m_fYaw = site.m_fYaw;
  plan.m_iTier = tier;
  plan.m_Work = work;
  plan.m_Site = site;
  plan.m_iCost = Cost(site);
  if (manager.m_Bases.GetRP(work.m_sFaction) + squad.PendingRP() < plan.m_iCost)
  {
   refusal = string.Format("not enough RP (%1 needed)", plan.m_iCost);
   return null;
  }
  // From the base side of the site.
  vector back = work.m_Base.GetOrigin() - site.m_vPos;
  back[1] = 0;
  float length = back.Length();
  if (length > 1) back = back * (Math.Min(8, length) / length);
  plan.m_vStage = ACR_Manager.Ground(site.m_vPos + back);
  if (BBB_Terrain.IsWaterAt(plan.m_vStage)) plan.m_vStage = site.m_vPos;
  plan.m_fRadius = SITE_RADIUS;
  plan.m_fDuration = Duration(site) * ACR_Fortify.WORK_SCALE;
  float now = GetGame().GetWorld().GetWorldTime();
  plan.m_fLastTick = now;
  plan.m_fLastProgress = now;
  plan.m_fDeadline = now + vector.DistanceXZ(from, plan.m_vStage) / ACR_Minefields.TRAVEL_SPEED * 1000 + ACR_Squad.FOB_GIVE_UP_MS;
  return plan;
 }

 // "Bay Town: street corner"
 static string Words(ACR_FortifyPlan plan)
 {
  return string.Format("%1: %2", plan.m_Work.m_Base.m_sName, SiteWords(plan.m_Site));
 }

 static bool VehicleOnSite(ACR_FortifyPlan plan)
 {
  return ACR_FobSlots.VehicleOnSpot(plan.m_vSite, 8);
 }

 // ---------------------------------------------------------------- the squad's work

 // A site's work is done (ACR_Squad.CompleteFortify): its pieces go in, and the squad goes on to
 // the next of the tier, or holds the base when there is none it can do.
 static void SiteDone(ACR_Squad squad, ACR_FortifyPlan plan)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_BaseWork work = plan.m_Work;
  ACR_BaseSite site = plan.m_Site;
  int orderedBy = squad.m_iOrderedBy;
  if (!s_aWorks.Contains(work) || work.m_sFaction != plan.m_sFaction || !Build(work, site))
  {
   if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
   site.m_bNoFit = s_aWorks.Contains(work) && work.m_sFaction == plan.m_sFaction;
   PrintFormat("[ACR] BASEWORKS %1: no %2 at %3, refunded %4 RP", plan.m_sFaction, SiteWords(site), site.m_vPos, plan.m_iCost);
  }
  if (!s_aWorks.Contains(work) || work.m_sFaction != plan.m_sFaction)
  {
   squad.HoldBaseWorks(work);
   return;
  }
  GoOn(squad, plan, orderedBy);
 }

 // The squad gave up on a site (ACR_Squad.TickFortify): refunded, the rest of the tier goes on, and
 // the next order on the base tries it again. One parked truck doesn't cost the tier.
 static void SiteSkipped(ACR_Squad squad, ACR_FortifyPlan plan)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
  plan.m_Site.m_bSkipped = true;
  PrintFormat("[ACR] BASEWORKS %1: %2 gave up on the %3 at %4, refunded %5 RP", plan.m_sFaction, squad.GetName(), SiteWords(plan.m_Site), plan.m_vSite, plan.m_iCost);
  if (!s_aWorks.Contains(plan.m_Work) || plan.m_Work.m_sFaction != plan.m_sFaction)
  {
   squad.HoldBaseWorks(plan.m_Work);
   return;
  }
  GoOn(squad, plan, squad.m_iOrderedBy);
 }

 protected static void GoOn(ACR_Squad squad, ACR_FortifyPlan plan, int orderedBy)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_BaseWork work = plan.m_Work;
  int tier = plan.m_iTier;
  if (OpenSites(work, tier) == 0 && !WorkedOn(work, tier, squad)) TierDone(work, tier);
  string why;
  ACR_FortifyPlan next;
  if (OpenSites(work, tier) > 0) next = NextPlan(work, squad, tier, why);
  // An AI commander's squad goes on only as far as its build budget does, as its first site did
  // (ACR_AICommander.ManageBaseWorks); it can pick the work up again later.
  ACR_AICommander ai;
  if (manager && squad.m_iAITask == ACR_EAITask.FORTIFY) ai = manager.AICommanderFor(plan.m_sFaction);
  if (next && ai && !AIMayPay(ai, next.m_iCost))
  {
   next = null;
   why = "";
  }
  if (next && squad.Fortify(next))
  {
   squad.m_iOrderedBy = orderedBy;
   if (ai) ai.SpendBuildBudget(next.m_iCost);
   return;
  }
  if (!why.IsEmpty() && OpenSites(work, tier) > 0 && manager)
  {
   string paused = string.Format("%1 stopped work on %2: %3", squad.GetName(), work.m_Base.m_sName, why);
   manager.Command().Feed(plan.m_sFaction, paused);
   if (orderedBy != 0) manager.Notify(orderedBy, paused);
  }
  squad.HoldBaseWorks(work);
 }

 static bool AIMayPay(ACR_AICommander ai, int cost)
 {
  return ai.CanBuild(cost, AI_RP_RESERVE);
 }

 // A squad other than 'except' has a site of this tier in hand.
 protected static bool WorkedOn(ACR_BaseWork work, int tier, ACR_Squad except)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_iTier == tier && Claimed(site, except)) return true;
  return false;
 }

 protected static void TierDone(ACR_BaseWork work, int tier)
 {
  int built = BuiltSites(work, tier);
  if (built == 0) return;
  work.m_iTier = Math.Max(work.m_iTier, tier);
  RaiseDefenders(work);
  ManWorks(work);
  string done = string.Format("%1 fortified: %2, %3 sites", work.m_Base.m_sName, ACR_FobDefences.TierName(tier - 1), built);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.Command().Feed(work.m_sFaction, done);
  ACR_Report.Event(work.m_sFaction, done);
  PrintFormat("[ACR] BASEWORKS %1 %2", work.m_sFaction, done);
 }

 protected static void RaiseDefenders(ACR_BaseWork work)
 {
  int more = Defenders(work.m_iTier) - Defenders(work.m_iMannedTier);
  if (more <= 0) return;
  work.m_iMannedTier = work.m_iTier;
  if (!work.m_Defenders) work.m_Defenders = ACR_Defenders.Add(work.m_sFaction, work.m_Base.GetOrigin(), work.m_Base.m_sName + " works", ACR_BaseDefences.SearchRadius(work.m_Base));
  work.m_Defenders.m_iReserve += more;
 }

 // The reserve on the nests' guns, and based at the command post once there is one.
 protected static void ManWorks(ACR_BaseWork work)
 {
  ACR_DefenderPost post = work.m_Defenders;
  if (!post) return;
  // A town with no command post site: the buildings round its middle.
  if (work.m_iTier >= 3)
  {
   post.m_fRadius = POST_AREA;
   post.m_fGarrisonArea = POST_AREA;
  }
  array<IEntity> guns = {};
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_bBuilt) continue;
   if (work.m_iTier >= 3 && site.m_Plan && site.m_Plan.m_eKind == ACR_EBaseDefenceKind.POST) post.m_vPos = site.m_vPos;
   if (site.m_eKind != ACR_EBaseSite.NEST) continue;
   foreach (IEntity piece : site.m_aWalls)
   {
    IEntity gun = ACR_Roadblocks.FindTurretInHierarchy(piece);
    if (gun) guns.Insert(gun);
   }
  }
  if (!guns.IsEmpty()) ACR_Defenders.CrewGuns(post, guns);
 }

 protected static bool Build(ACR_BaseWork work, ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.ROADBLOCK:
    site.m_Roadblock = ACR_Roadblocks.SpawnRoadblock(work.m_sFaction, site.m_vPos, site.m_fYaw, work.m_Base.m_sName + " works");
    if (!site.m_Roadblock) return false;
    site.m_aWalls.Insert(site.m_Roadblock.m_Entity);
    break;
   case ACR_EBaseSite.PLANNED:
    BuildPlanned(site);
    if (site.m_aWalls.IsEmpty())
    {
     foreach (IEntity prop : site.m_aDressing) if (prop) SCR_EntityHelper.DeleteEntityAndChildren(prop);
     site.m_aDressing.Clear();
     return false;
    }
    break;
   default:
    ACR_FobBuild.FellTrees(site.m_vPos, ACR_FobDefences.PIECE_RADIUS);
    IEntity piece = ACR_Fortify.SpawnPiece(site.m_sPrefab, site.m_vPos, site.m_fYaw, work.m_sFaction);
    if (!piece) return false;
    site.m_aWalls.Insert(piece);
  }
  site.m_bBuilt = true;
  PrintFormat("[ACR] BASEWORKS %1 %2: %3 at %4 (%5 pieces)", work.m_sFaction, work.m_Base.m_sName, SiteWords(site), site.m_vPos, site.m_aWalls.Count() + site.m_aDressing.Count());
  return true;
 }

 // The planner's pieces. Only sandbags and such ask for a navmesh rebuild; a chair doesn't. A
 // position with no sandbags (none fit) is held up by whatever it has.
 protected static void BuildPlanned(ACR_BaseSite site)
 {
  ACR_BaseDefencePos pos = site.m_Plan;
  bool anyWall = pos.Walls() > 0;
  foreach (int i, vector at : pos.m_aPiecePos)
  {
   IEntity piece = ACR_BaseDefences.SpawnProp(pos.m_aPiecePrefab[i], at, pos.m_aPieceYaw[i]);
   if (!piece) continue;
   if (pos.m_aPieceWall[i] || !anyWall)
   {
    site.m_aWalls.Insert(piece);
    ACR_Navmesh.RebuildAfterSpawn(piece);
   }
   else site.m_aDressing.Insert(piece);
  }
 }

 // Tier I up at once and free, nobody walking (Quick start's front towns). The sites built.
 static int BuildNow(ACR_Base base, string faction)
 {
  if (!Enabled() || !base || base.m_bBuilt || base.m_sOwner != faction) return 0;
  ACR_BaseWork work = WorkFor(base, faction);
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_iTier != 1 || site.m_bBuilt || site.m_bNoFit) continue;
   if (Build(work, site)) built++;
   else site.m_bNoFit = true;
  }
  if (built > 0) work.m_iTier = Math.Max(work.m_iTier, 1);
  return built;
 }

 // Tests: every tier up at once, for a look at it (ACR_TestWorksShots).
 static int BuildAllForTest(ACR_Base base, string faction)
 {
  ACR_BaseWork work = WorkFor(base, faction);
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_bBuilt || site.m_bNoFit) continue;
   if (Build(work, site)) built++;
   else site.m_bNoFit = true;
  }
  return built;
 }

 // ---------------------------------------------------------------- where the sites go

 protected static void PlanSites(ACR_BaseWork work)
 {
  ACR_Base base = work.m_Base;
  ACR_BaseSurvey survey = ACR_BaseSurvey.Take(base, work.m_sFaction);
  s_LastSurvey = survey;
  PrintFormat("[ACR] BASEWORKS survey %1 (%2 ms)", survey.Words(), survey.m_iMs);
  array<ref ACR_BaseDefencePos> plan = {};
  string summary;
  ACR_BaseDefences.Plan(base, work.m_sFaction, plan, summary, survey);
  PrintFormat("[ACR] BASEWORKS plan %1", summary);
  array<ref ACR_BaseSite> walls = {};
  array<ref ACR_BaseSite> redoubts = {};
  foreach (ACR_BaseDefencePos pos : plan)
  {
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.PLANNED;
   site.m_Plan = pos;
   site.m_vPos = pos.m_vPos;
   site.m_fYaw = ACR_BaseDefences.Bearing(pos.m_vFacing);
   site.m_iSector = ACR_BaseSurvey.Sector(site.m_fYaw);
   site.m_fScore = pos.m_fThreat + Math.Min(pos.m_fSight, 200) / 200;
   site.m_fReach = pos.m_fSight;
   if (pos.m_eKind == ACR_EBaseDefenceKind.POST)
   {
    site.m_iTier = 3;
    work.m_aSites.Insert(site);
   }
   else if (pos.m_eKind == ACR_EBaseDefenceKind.SQUARE)
   {
    site.m_iTier = 2;
    redoubts.Insert(site);
   }
   else
   {
    site.m_iTier = 1;
    walls.Insert(site);
   }
  }
  // Bigger towns get more; what's left of the plan is for the next town to do otherwise.
  WeakFirst(work, survey, walls, Math.ClampInt(4 + survey.m_iTown / 8, 4, MAX_WALL_SITES), COVER_BAGS, "wall");
  WeakFirst(work, survey, redoubts, redoubts.Count(), COVER_REDOUBT, "redoubt");
  Checkpoints(work, survey);
  survey.FindSpots();
  Strong(work, survey, ACR_EBaseSite.NEST, 2, MAX_NESTS);
  Strong(work, survey, ACR_EBaseSite.BUNKER, 3, MAX_BUNKERS);
  string refused = survey.RefusedWords();
  if (!refused.IsEmpty()) PrintFormat("[ACR] BASEWORKS %1: planner positions left out: %2", base.m_sName, refused);
  PrintFormat("[ACR] BASEWORKS %1 guns: %2", base.m_sName, survey.SpotWords());
  foreach (ACR_BaseSite made : work.m_aSites)
   PrintFormat("[ACR] BASEWORKS site %1 %2, tier %3, at %4 %5 facing %6 (%7 m clear), its sector now %8", base.m_sName, SiteWords(made), made.m_iTier, (int)made.m_vPos[0], (int)made.m_vPos[2], (int)made.m_fYaw, (int)made.m_fReach, survey.Weak(made.m_iSector).ToString(-1, 2));
  if (s_iMaxSitesForTest > 0) Trim(work);
 }

 // Up to 'most' of the planner's positions, the weakest sector first: a weighted pick among the
 // near-best, each one covering its sector for the next.
 protected static void WeakFirst(ACR_BaseWork work, ACR_BaseSurvey survey, array<ref ACR_BaseSite> positions, int most, float cover, string what)
 {
  for (int made = 0; made < most && !positions.IsEmpty(); made++)
  {
   array<float> scores = {};
   foreach (ACR_BaseSite candidate : positions) scores.Insert(survey.WeakLight(candidate.m_iSector) + candidate.m_fScore * 0.1);
   int picked = ACR_Variety.Pick(scores, string.Format("base works %1 %2 %3", what, work.m_Base.m_sName, made));
   ACR_BaseSite site = positions[picked];
   site.m_fScore = survey.WeakLight(site.m_iSector);
   work.m_aSites.Insert(site);
   survey.AddLight(site.m_iSector, cover);
   positions.Remove(picked);
  }
 }

 protected static void Trim(ACR_BaseWork work)
 {
  map<string, int> seen = new map<string, int>();
  for (int i = 0; i < work.m_aSites.Count(); i++)
  {
   string kind = SiteWords(work.m_aSites[i]);
   int count;
   seen.Find(kind, count);
   seen.Set(kind, count + 1);
   if (count < s_iMaxSitesForTest) continue;
   work.m_aSites.RemoveOrdered(i);
   i--;
  }
 }

 // Nests or bunkers on the places the survey found: the weakest sector first, a weighted pick among
 // the near-best, each covering its sector for the next. Fewer than 'most' when no good place is left.
 protected static void Strong(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_EBaseSite kind, int tier, int most)
 {
  bool towers = work.m_sFaction != "FIA";
  int towersMade, bunkersMade;
  int made;
  for (made = 0; made < most; made++)
  {
   array<ACR_BaseSpot> open = {};
   array<float> scores = {};
   foreach (ACR_BaseSpot spot : survey.m_aSpots)
   {
    if (kind == ACR_EBaseSite.NEST && !spot.m_bNest) continue;
    if (kind == ACR_EBaseSite.BUNKER && !spot.m_bBunker && !(towers && spot.m_bTower)) continue;
    if (!SpotFree(work, spot)) continue;
    open.Insert(spot);
    float score = 0.5 * (survey.Weak(spot.m_iSector) + survey.Weak(spot.m_iStands)) + 0.3 * spot.m_fReach / ACR_BaseSurvey.OPEN_RANGE;
    if (spot.m_bGate) score += 0.1;
    // Nearer in is a shorter walk, and the men in it nearer the town.
    float past = vector.DistanceXZ(spot.m_vPos, work.m_Base.GetOrigin()) - survey.m_aEdge[ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(spot.m_vPos - work.m_Base.GetOrigin()))];
    score -= Math.Max(past, 0) / 400;
    scores.Insert(score);
   }
   if (open.IsEmpty()) break;
   int picked = ACR_Variety.Pick(scores, string.Format("base works spot %1 %2 %3", work.m_Base.m_sName, kind, made));
   ACR_BaseSpot chosen = open[picked];
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = kind;
   site.m_iTier = tier;
   site.m_vPos = chosen.m_vPos;
   site.m_fYaw = ACR_FobDefences.BearingYaw(chosen.m_fFacing);
   site.m_iSector = chosen.m_iSector;
   site.m_fReach = chosen.m_fReach;
   site.m_fScore = survey.Weak(chosen.m_iSector);
   site.m_iSpot = ACR_FobDefences.POST_COUNT;
   float cover = COVER_NEST;
   if (kind == ACR_EBaseSite.BUNKER)
   {
    cover = COVER_BUNKER;
    site.m_iSpot = 0;
    if (towers && TowerThere(work, chosen, towersMade, bunkersMade, made)) site.m_iSpot = 1;
    if (site.m_iSpot == 1) towersMade++;
    else bunkersMade++;
   }
   site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, work.m_sFaction, tier - 1);
   work.m_aSites.Insert(site);
   // It covers where it looks, and the side of the town it stands on.
   survey.AddCover(site.m_iSector, cover * 0.6);
   survey.AddCover(chosen.m_iStands, cover * 0.4);
  }
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: %2 of %3 %4 (no good place left)", work.m_Base.m_sName, made, most, SpotKindWords(kind));
 }

 protected static string SpotKindWords(ACR_EBaseSite kind)
 {
  if (kind == ACR_EBaseSite.NEST) return "MG nests";
  return "bunkers and towers";
 }

 // What goes up here: a tower where only its height clears the fan, or where there is a long view
 // or a wall to see over; a bunker at a road in or a shorter view. Among those that fit, by turns
 // as near as the place allows (a weighted pick, seeded).
 protected static bool TowerThere(ACR_BaseWork work, ACR_BaseSpot spot, int towersMade, int bunkersMade, int made)
 {
  if (!spot.m_bBunker) return true;
  if (!spot.m_bTower) return false;
  float tower = 0.5 - 0.25 * towersMade;
  float bunker = 0.5 - 0.25 * bunkersMade;
  if (spot.m_fReach >= 100) tower += 0.3;
  else bunker += 0.15;
  if (spot.m_bGate) bunker += 0.25;
  array<float> scores = {bunker, tower};
  return ACR_Variety.Pick(scores, string.Format("base works post %1 %2", work.m_Base.m_sName, made), 0.3) == 1;
 }

 // Well apart from the other guns and from any site, and with no site of ours in its line of fire.
 protected static bool SpotFree(ACR_BaseWork work, ACR_BaseSpot spot)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   bool gun = site.m_eKind == ACR_EBaseSite.NEST || site.m_eKind == ACR_EBaseSite.BUNKER;
   float apart = vector.DistanceXZ(site.m_vPos, spot.m_vPos);
   if (gun && apart < GUN_APART) return false;
   if (gun && apart < SAME_WAY && ACR_BaseDefences.DotXZ(ACR_BaseSurvey.Dir(site.m_fYaw), ACR_BaseSurvey.Dir(spot.m_fFacing)) > 0.906) return false;
   if (!gun && apart < SPOT_APART) return false;
   if (apart > FIRE_LANE + 10) continue;
   if (InFireLane(spot, site.m_vPos)) return false;
   if (!site.m_Plan) continue;
   foreach (vector piece : site.m_Plan.m_aPiecePos) if (InFireLane(spot, piece)) return false;
  }
  return true;
 }

 protected static bool InFireLane(ACR_BaseSpot spot, vector at)
 {
  float apart = vector.DistanceXZ(at, spot.m_vPos);
  if (apart > FIRE_LANE) return false;
  vector way = ACR_BaseDefences.FlatDir(spot.m_vPos, at);
  return ACR_BaseDefences.DotXZ(way, ACR_BaseSurvey.Dir(spot.m_fFacing)) > 0.9;
 }

 protected static bool NearSite(ACR_BaseWork work, vector pos, float apart)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if (vector.DistanceXZ(site.m_vPos, pos) < apart) return true;
  return false;
 }

 // A checkpoint on a road in from each of the two nearest enemy bases, close to the town: where the
 // road leaves it, as far out as ValidateSite's distance from a base asks and no further (playtest
 // 30 Oct: one 305 m out took 7 of tier II's 13 minutes, and the road from the enemy is the same road
 // nearer in). Scored as the roadblock siting does, less the walk, and worked last in its tier
 // (NextPlan). No road in near enough that way: no checkpoint, and the log says so.
 protected static void Checkpoints(ACR_BaseWork work, ACR_BaseSurvey survey)
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
  for (int made = 0; made < MAX_ROADBLOCKS && !enemies.IsEmpty(); made++)
  {
   int nearest = 0;
   foreach (int i, float d : dists) if (d < dists[nearest]) nearest = i;
   ACR_Base enemy = enemies[nearest];
   enemies.Remove(nearest);
   dists.Remove(nearest);
   array<ref ACR_RoadSite> sites = {};
   array<float> scores = {};
   array<int> sectors = {};
   RoadSitesIn(work, survey, enemy.GetOrigin(), sites, scores, sectors);
   if (sites.IsEmpty())
   {
    PrintFormat("[ACR] BASEWORKS %1: no checkpoint for the road from %2: no road in near enough", work.m_Base.m_sName, enemy.m_sName);
    continue;
   }
   int picked = ACR_Variety.Pick(scores, string.Format("base works road %1 %2", work.m_Base.m_sName, made));
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.ROADBLOCK;
   site.m_iTier = 2;
   site.m_vPos = sites[picked].m_vPos;
   site.m_fYaw = sites[picked].m_fYaw;
   site.m_iSector = sectors[picked];
   site.m_fScore = survey.Weak(site.m_iSector);
   work.m_aSites.Insert(site);
   survey.AddLight(site.m_iSector, COVER_CHECKPOINT);
  }
 }

 // Road sites just past the edge of the town on the roads in that lead the enemy's way.
 protected static void RoadSitesIn(ACR_BaseWork work, ACR_BaseSurvey survey, vector enemy, notnull array<ref ACR_RoadSite> sites, notnull array<float> scores, notnull array<int> sectors)
 {
  vector center = work.m_Base.GetOrigin();
  vector toEnemy = ACR_BaseDefences.FlatDir(center, enemy);
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (ACR_BaseDefences.DotXZ(gate.m_vOut, toEnemy) < 0.2) continue;
   float limit = Math.Max(survey.m_aEdge[gate.m_iSector] + CHECKPOINT_PAST, CHECKPOINT_LEAST);
   for (float along = 0; along <= 120; along += 20)
   {
    vector sample = gate.m_vPos + gate.m_vOut * along;
    if (vector.DistanceXZ(sample, center) > limit) break;
    vector pos;
    float yaw;
    string refusal;
    if (!ACR_Roadblocks.ValidateSite(sample, pos, yaw, refusal, work.m_sFaction, enemy)) continue;
    float out_m = vector.DistanceXZ(pos, center);
    if (out_m > limit || NearSite(work, pos, 15) || FarFromCheckpoints(work, pos, 100)) continue;
    bool seen;
    foreach (ACR_RoadSite have : sites) if (vector.DistanceXZ(have.m_vPos, pos) < 25) seen = true;
    if (seen) continue;
    ACR_RoadSite site = ACR_RoadblockSiting.Describe(pos, yaw, center, enemy);
    sites.Insert(site);
    sectors.Insert(gate.m_iSector);
    // Its usual score (out of about 14), the sector's weakness, less the walk.
    scores.Insert(site.m_fScore + 3 * survey.Weak(gate.m_iSector) - out_m / 50);
   }
  }
 }

 protected static bool FarFromCheckpoints(ACR_BaseWork work, vector pos, float apart)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK && vector.DistanceXZ(site.m_vPos, pos) < apart) return true;
  return false;
 }

 // ---------------------------------------------------------------- upkeep

 // From ACR_Fortify.Tick every 5 s: lost sites and a base that changed hands.
 static void Tick()
 {
  for (int w = s_aWorks.Count() - 1; w >= 0; w--)
  {
   ACR_BaseWork work = s_aWorks[w];
   if (!work.m_Base || work.m_Base.m_bRetired)
   {
    CallOff(work, "its base is gone");
    ACR_Defenders.Remove(work.m_Defenders);
    s_aWorks.RemoveOrdered(w);
    continue;
   }
   string owner = work.m_Base.m_sOwner;
   if (!owner.IsEmpty() && owner != work.m_sFaction) Transfer(work, owner);
   foreach (ACR_BaseSite site : work.m_aSites)
    if (site.m_bBuilt) PruneSite(site);
   // The tier stands while any site of it does.
   while (work.m_iTier > 0 && BuiltSites(work, work.m_iTier) == 0)
   {
    PrintFormat("[ACR] BASEWORKS %1 %2: %3 is gone", work.m_sFaction, work.m_Base.m_sName, ACR_FobDefences.TierName(work.m_iTier - 1));
    work.m_iTier--;
   }
  }
 }

 protected static void PruneSite(ACR_BaseSite site)
 {
  for (int i = site.m_aWalls.Count() - 1; i >= 0; i--)
  {
   IEntity piece = site.m_aWalls[i];
   if (piece && ACR_Logistics.Health(piece) > 0) continue;
   site.m_aWalls.Remove(i);
   if (piece) ACR_Navmesh.DeleteStructure(piece);
  }
  if (!site.m_aWalls.IsEmpty()) return;
  // The dressing goes with it; the site can be built again.
  foreach (IEntity prop : site.m_aDressing) if (prop) SCR_EntityHelper.DeleteEntityAndChildren(prop);
  site.m_aDressing.Clear();
  site.m_Roadblock = null;
  site.m_bBuilt = false;
  PrintFormat("[ACR] BASEWORKS: the %1 at %2 is gone", SiteWords(site), site.m_vPos);
 }

 protected static void Transfer(ACR_BaseWork work, string owner)
 {
  CallOff(work, "the base has fallen");
  PrintFormat("[ACR] BASEWORKS %1: %2 passes to %3 (%4 sites standing)", work.m_Base.m_sName, work.m_sFaction, owner, BuiltSites(work));
  work.m_sFaction = owner;
  // The sandbags change hands; the men in reserve don't.
  ACR_Defenders.Remove(work.m_Defenders);
  work.m_Defenders = null;
  work.m_iMannedTier = 0;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   // Nests and bunkers still to build are the new owner's kind.
   if (!site.m_sPrefab.IsEmpty()) site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, owner, site.m_iTier - 1);
   if (site.m_Roadblock)
   {
    site.m_Roadblock.m_sFaction = owner;
    // Its reserve (-acrRoadblockDefenders) doesn't change hands either.
    ACR_Defenders.Remove(site.m_Roadblock.m_Defenders);
    site.m_Roadblock.m_Defenders = null;
   }
   foreach (IEntity piece : site.m_aWalls)
   {
    if (!piece) continue;
    SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
    if (comp) comp.ACR_SetFaction(owner);
   }
  }
 }

 protected static void CallOff(ACR_BaseWork work, string why)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.m_FortifyPlan && squad.m_FortifyPlan.m_Work == work) squad.CancelFortify(why);
 }

 // The owner's defence credit in the AI's sums: dug in all round, its men hold longer than they
 // would in the open (as ACR_AICommander.FortifiedCredit counts a position's).
 static int Credit(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return 0;
  switch (work.m_iTier)
  {
   case 1: return 2;
   case 2:
    if (work.m_Defenders) return 5;
    return 2;
   case 3:
    if (work.m_Defenders) return 10;
    return 3;
  }
  return 0;
 }

 // The owning side's command map: a pale ring round the town ("~": not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_BaseWork work : s_aWorks)
  {
   if (work.m_sFaction != faction || !work.m_Base) continue;
   int built = BuiltSites(work);
   if (built == 0) continue;
   positions.Insert(work.m_Base.GetOrigin());
   radii.Insert(ACR_BaseDefences.SearchRadius(work.m_Base));
   string tier = "Works";
   if (work.m_iTier > 0) tier = ACR_FobDefences.TierName(work.m_iTier - 1);
   labels.Insert(string.Format("~%1: %2 sites", tier, built));
  }
 }

 static void Reset()
 {
  s_aWorks.Clear();
  s_iMaxSitesForTest = 0;
  s_LastSurvey = null;
 }
}
