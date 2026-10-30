// srv:baseworks, srv:baseworksarland (about 6 min): Fortify on the middle of a base (ACR_BaseWorks,
// #105). A squad at the biggest town we can give the West. Checks: the option, the click's reach and
// what is refused, charging nothing; the first site paid alone; a new order refunds the site in hand
// and leaves the rest open; tier I built site by site and on the side's map; tier II with its
// defenders, tier III with more, and nothing past that; a lost site opens again and only it is
// rebuilt; the base falling hands the works over standing. srv:baseworksai: AI commanders with RP
// banked fortify a town of their own.
class ACR_TestBaseWorks
{
 static const ResourceName SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName ENEMY = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const string NAME = "Works Squad";
 static const int STEP_LIMIT = 300; // s a tier may take (its checkpoint can be 400 m out)
 static const int CHECKS = 35;
 protected static int s_iStep;
 protected static int s_iStepAt;
 protected static int s_iRP;
 protected static int s_iQueued;
 protected static ACR_Base s_Base;
 protected static ACR_BaseWork s_Work;
 protected static ACR_BaseSite s_Lost;
 protected static ACR_BaseSite s_Skipped;
 protected static int s_iMen;
 protected static string s_sSide;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (System.IsCLIParam("acrBaseWorksAITest"))
  {
   RunAI(manager, second);
   return;
  }
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 8) Setup(manager);
  if (second < 15 || !s_Base) return;
  ACR_Squad squad = Find(manager);
  if (!squad)
  {
   if (second == 40) ACR_TestRegress.Abort("baseworks", "no squad");
   return;
  }
  ACR_FactionData us = manager.m_Bases.GetFaction(s_sSide);
  int waited = second - s_iStepAt;
  switch (s_iStep)
  {
   case 0: Refusals(squad, us); Next(second); break;
   case 1: First(squad, us); Next(second); break;
   case 2: if (waited >= 3) { Replaced(squad, us); Give(squad); Next(second); } break;
   case 3: if (waited >= 2) { Gave(squad, us); Next(second); } break;
   case 4: if (Tier(1, waited)) { TierOne(squad, us, manager); OrderNext(squad); Next(second); } break;
   case 5: if (Retried(waited)) { OrderNext(squad); Next(second); } break;
   case 6: if (Tier(2, waited)) { TierTwo(squad); OrderNext(squad); Next(second); } break;
   case 7: if (Tier(3, waited)) { TierThree(squad); Lose(); Next(second); } break;
   case 8: if (waited >= 7) { Lost(squad, us); Next(second); } break;
   case 9: if (Rebuilt(waited)) { RebuiltChecks(us); EnemyNear(manager); Next(second); } break;
   case 10: if (Deployed(waited)) { DeployedChecks(); s_Base.m_sOwner = ACR_FactionSelection.East(); Next(second); } break;
   case 11: if (waited >= 7) { Fallen(); s_iStep = 99; } break;
  }
 }

 protected static void Next(int second)
 {
  s_iStep++;
  s_iStepAt = second;
 }

 protected static bool Tier(int tier, int waited)
 {
  if (s_Work && s_Work.m_iTier >= tier) return true;
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", string.Format("tier %1 not done in %2 s: %3 built, %4 open", tier, STEP_LIMIT, ACR_BaseWorks.BuiltSites(s_Work, tier), ACR_BaseWorks.OpenSites(s_Work, tier)));
  return false;
 }

 protected static ACR_Squad Find(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == NAME) return squad;
  return null;
 }

 // The base with the most buildings, ours for the test.
 protected static void Setup(ACR_Manager manager)
 {
  s_sSide = ACR_FactionSelection.West();
  ACR_Fortify.WORK_SCALE = 0.05;
  ACR_BaseSystem.s_bNoIncomeForTest = true; // the RP sums below are the sites' alone
  ACR_BaseWorks.s_iMaxSitesForTest = 1;
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_sOwner == ACR_FactionSelection.East()) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
   if (buildings.Count() <= most) continue;
   most = buildings.Count();
   s_Base = base;
  }
  if (!s_Base)
  {
   ACR_TestRegress.Abort("baseworks", "no base");
   return;
  }
  s_Base.m_sOwner = s_sSide;
  manager.m_Bases.GetFaction(s_sSide).m_iRP = 1000;
  PrintFormat("[ACR] TEST baseworks at %1: %2 buildings", s_Base.m_sName, most);
  vector spot = ACR_Manager.Ground(s_Base.GetOrigin() + Vector(25, 0, 25));
  manager.m_sSpawningFaction = s_sSide;
  manager.SpawnUnit(NAME, SQUAD, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static void Refusals(ACR_Squad squad, ACR_FactionData us)
 {
  s_iRP = us.m_iRP;
  vector center = s_Base.GetOrigin();
  ACR_TestRegress.Check("baseworks: on by default, a click on the middle means the base", ACR_BaseWorks.BaseAt(center + Vector(30, 0, 0), s_sSide) == s_Base, "");
  ACR_TestRegress.Check("baseworks: a click further out is a plain Fortify", ACR_BaseWorks.BaseAt(center + Vector(ACR_BaseWorks.CLICK_RADIUS + 5, 0, 0), s_sSide) != s_Base, "");
  ACR_Options.Set("acrBaseWorks", "0");
  ACR_TestRegress.Check("baseworks: -acrBaseWorks 0 turns it off", !ACR_BaseWorks.Enabled() && !ACR_BaseWorks.BaseAt(center, s_sSide), "");
  ACR_Options.Unset("acrBaseWorks");
  ACR_Base enemy;
  foreach (ACR_Base other : ACR_Base.s_All)
   if (other && !other.m_bRetired && other.m_sOwner == ACR_FactionSelection.East()) enemy = other;
  string why;
  bool refused = true;
  if (enemy) refused = !ACR_BaseWorks.Order(squad, enemy, why);
  ACR_TestRegress.Check("baseworks: an enemy base is refused", refused && us.m_iRP == s_iRP, why);
  s_Base.m_bBuilt = true;
  ACR_TestRegress.Check("baseworks: a FOB is pointed to its own panel", ACR_BaseWorks.BaseAt(center, s_sSide) == s_Base && !ACR_BaseWorks.Order(squad, s_Base, why) && why.Contains("FOB") && us.m_iRP == s_iRP, why);
  s_Base.m_bBuilt = false;
 }

 protected static void First(ACR_Squad squad, ACR_FactionData us)
 {
  string why;
  bool ok = ACR_BaseWorks.Order(squad, s_Base, why);
  s_Work = ACR_BaseWorks.Find(s_Base);
  ACR_TestRegress.Check("baseworks: the order takes", ok && s_Work != null, why);
  if (!ok || !s_Work)
  {
   ACR_TestRegress.Abort("baseworks", "no works: " + why);
   return;
  }
  int t1 = ACR_BaseWorks.OpenSites(s_Work, 1);
  int t2 = ACR_BaseWorks.OpenSites(s_Work, 2);
  int t3 = ACR_BaseWorks.OpenSites(s_Work, 3);
  ACR_TestRegress.Check("baseworks: sites planned on all three tiers", t1 > 0 && t2 > 0 && t3 > 0, string.Format("%1 / %2 / %3 (one of each kind in the test)", t1, t2, t3));
  ACR_TestRegress.Check("baseworks: planning stays under 2 s", ACR_BaseWorks.s_iLastPlanMs < 2000, string.Format("%1 ms", ACR_BaseWorks.s_iLastPlanMs));
  ACR_TestRegress.Check("baseworks: only the first site is paid", squad.m_FortifyPlan && us.m_iRP == s_iRP - squad.m_FortifyPlan.m_iCost, string.Format("%1 -> %2", s_iRP, us.m_iRP));
  s_iQueued = ACR_Navmesh.s_iQueued;
 }

 protected static void Replaced(ACR_Squad squad, ACR_FactionData us)
 {
  int open = ACR_BaseWorks.OpenSites(s_Work, 1);
  ACR_OrderSnapshot aside = squad.SnapshotOrder();
  squad.MoveTo(ACR_Manager.Ground(s_Base.GetOrigin() + Vector(20, 0, 0)));
  ACR_TestRegress.Check("baseworks: a new order refunds the site in hand", !squad.m_FortifyPlan && us.m_iRP == s_iRP, string.Format("%1 of %2", us.m_iRP, s_iRP));
  ACR_TestRegress.Check("baseworks: ... and the sites stay open", ACR_BaseWorks.OpenSites(s_Work, 1) == open && ACR_BaseWorks.BuiltSites(s_Work) == 0, "");
  // A Fortify order called off while it was set aside isn't put back (a regroup, a player's turn).
  squad.RestoreOrder(aside);
  ACR_TestRegress.Check("baseworks: a called-off order isn't restored", squad.OrderState() != ACR_EOrderState.FORTIFY && !squad.m_FortifyPlan, squad.OrderState().ToString());
  string why;
  bool again = ACR_BaseWorks.Order(squad, s_Base, why);
  ACR_TestRegress.Check("baseworks: ordered again it goes on", again, why);
 }

 // Three minutes with no progress on this site, as with a truck parked on it.
 protected static void Give(ACR_Squad squad)
 {
  if (!squad.m_FortifyPlan) return;
  s_Skipped = squad.m_FortifyPlan.m_Site;
  squad.m_FortifyPlan.m_bArrived = true;
  squad.m_FortifyPlan.m_fLastProgress = GetGame().GetWorld().GetWorldTime() - ACR_Squad.FOB_GIVE_UP_MS - 1000;
 }

 protected static void Gave(ACR_Squad squad, ACR_FactionData us)
 {
  bool onward = squad.m_FortifyPlan && s_Skipped && squad.m_FortifyPlan.m_Site != s_Skipped && s_Skipped.m_bSkipped;
  int paid;
  if (squad.m_FortifyPlan) paid = squad.m_FortifyPlan.m_iCost;
  ACR_TestRegress.Check("baseworks: a site it can't finish is refunded and it goes on", onward && us.m_iRP == s_iRP - paid, string.Format("%1 -> %2", s_iRP, us.m_iRP));
 }

 protected static bool Retried(int waited)
 {
  if (s_Skipped && s_Skipped.m_bBuilt)
  {
   ACR_TestRegress.Check("baseworks: the next order tries the skipped site again", true, "");
   return true;
  }
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", "the skipped site wasn't tried again");
  return false;
 }

 protected static void TierOne(ACR_Squad squad, ACR_FactionData us, ACR_Manager manager)
 {
  int built = ACR_BaseWorks.BuiltSites(s_Work, 1);
  bool standing = true;
  bool damage = false;
  int paid;
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt) continue;
   paid += ACR_BaseWorks.Cost(site);
   if (site.m_aWalls.IsEmpty()) standing = false;
   foreach (IEntity piece : site.m_aWalls)
   {
    if (!piece) standing = false;
    else if (piece.FindComponent(DamageManagerComponent)) damage = true;
   }
  }
  ACR_TestRegress.Check("baseworks: tier I built, every site standing", built > 0 && standing && ACR_BaseWorks.OpenSites(s_Work, 1) == 0, string.Format("%1 sites", built));
  ACR_TestRegress.Check("baseworks: paid per site", us.m_iRP == s_iRP - paid, string.Format("%1 -> %2, %3 for the sites", s_iRP, us.m_iRP, paid));
  ACR_TestRegress.Check("baseworks: the navmesh is told", ACR_Navmesh.s_iQueued > s_iQueued, string.Format("%1 more", ACR_Navmesh.s_iQueued - s_iQueued));
  ACR_TestRegress.Check("baseworks: the sandbags have a damage manager", damage, "");
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Fortify.Zones(s_sSide, positions, radii, labels);
  string label = "";
  foreach (string line : labels) if (line.Contains("Defences I")) label = line;
  ACR_TestRegress.Check("baseworks: on the side's map", !label.IsEmpty(), label);
  ACR_TestRegress.Check("baseworks: the squad holds the base after the tier", !squad.m_FortifyPlan && squad.OrderState() == ACR_EOrderState.DEFEND, squad.OrderState().ToString());
 }

 protected static void OrderNext(ACR_Squad squad)
 {
  string why;
  if (!ACR_BaseWorks.Order(squad, s_Base, why)) ACR_TestRegress.Abort("baseworks", "next tier refused: " + why);
 }

 protected static void TierTwo(ACR_Squad squad)
 {
  bool real = true;
  bool nest = false;
  string kinds = "";
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 2 || !site.m_bBuilt) continue;
   kinds += ACR_BaseWorks.SiteWords(site) + ", ";
   if (site.m_eKind == ACR_EBaseSite.NEST) nest = true;
   if (site.m_eKind == ACR_EBaseSite.NEST && !site.m_aWalls[0].FindComponent(SCR_CampaignBuildingCompositionComponent)) real = false;
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK && !site.m_Roadblock) real = false;
  }
  ACR_TestRegress.Check("baseworks: tier II built", ACR_BaseWorks.BuiltSites(s_Work, 2) > 0 && real, kinds);
  ACR_TestRegress.Check("baseworks: tier II has an MG nest", nest, kinds);
  int men = Men();
  bool crewOut = s_Work.m_Defenders && !s_Work.m_Defenders.m_aCrewGuns.IsEmpty();
  ACR_TestRegress.Check("baseworks: tier II has defenders in reserve", men == ACR_BaseWorks.Defenders(2), string.Format("%1 men", men));
  ACR_TestRegress.Check("baseworks: the reserve is told to man the nest's gun", crewOut, "");
 }

 // The works' men: in reserve, or on the guns.
 protected static int Men()
 {
  if (!s_Work.m_Defenders) return 0;
  int men = s_Work.m_Defenders.m_iReserve;
  if (s_Work.m_Defenders.m_GunCrew)
  {
   array<AIAgent> crew = {};
   s_Work.m_Defenders.m_GunCrew.GetAgents(crew);
   men += crew.Count();
  }
  return men;
 }

 protected static void EnemyNear(ACR_Manager manager)
 {
  vector spot = ACR_Manager.Ground(s_Work.m_Defenders.m_vPos + Vector(250, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit("Works Enemy", ENEMY, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static bool Deployed(int waited)
 {
  if (s_Work.m_Defenders && s_Work.m_Defenders.m_Group) return true;
  if (waited > 40) ACR_TestRegress.Abort("baseworks", "no defenders deployed with an enemy near");
  return false;
 }

 protected static void DeployedChecks()
 {
  SCR_AIGroup group = s_Work.m_Defenders.m_Group;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  ACR_TestRegress.Check("baseworks: an enemy near deploys the reserve", agents.Count() > 0, string.Format("%1 men", agents.Count()));
  ACR_TestRegress.Check("baseworks: ... into the buildings round the post or the base's middle", ACR_Garrison.HasPosts(group), string.Format("at %1", s_Work.m_Defenders.m_vPos));
 }

 protected static void TierThree(ACR_Squad squad)
 {
  int men = Men();
  ACR_TestRegress.Check("baseworks: tier III built, more defenders", ACR_BaseWorks.BuiltSites(s_Work, 3) > 0 && men == ACR_BaseWorks.Defenders(3), string.Format("%1 men", men));
  string why;
  ACR_TestRegress.Check("baseworks: nothing past tier III", !ACR_BaseWorks.Order(squad, s_Base, why), why);
 }

 protected static void Lose()
 {
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt) continue;
   s_Lost = site;
   foreach (IEntity piece : site.m_aWalls) if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
   return;
  }
 }

 protected static void Lost(ACR_Squad squad, ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: a site with no walls left opens again", s_Lost && !s_Lost.m_bBuilt && ACR_BaseWorks.OpenTier(s_Work) == 1 && s_Lost.m_aDressing.IsEmpty(), "");
  ACR_TestRegress.Check("baseworks: the tiers still stand", s_Work.m_iTier == 3, "");
  s_iMen = Men();
  s_iRP = us.m_iRP;
  OrderNext(squad);
 }

 protected static bool Rebuilt(int waited)
 {
  if (s_Lost && s_Lost.m_bBuilt) return true;
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", "the lost site wasn't rebuilt");
  return false;
 }

 protected static void RebuiltChecks(ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: only the lost site is rebuilt and paid", us.m_iRP == s_iRP - ACR_BaseWorks.Cost(s_Lost) && ACR_BaseWorks.OpenTier(s_Work) == 0, string.Format("%1 -> %2", s_iRP, us.m_iRP));
  ACR_TestRegress.Check("baseworks: a rebuild adds no men", Men() == s_iMen, string.Format("%1 then %2", s_iMen, Men()));
 }

 protected static void Fallen()
 {
  string east = ACR_FactionSelection.East();
  ACR_TestRegress.Check("baseworks: the works pass to whoever takes the base", s_Work.m_sFaction == east && ACR_BaseWorks.BuiltSites(s_Work) > 0 && s_Work.m_iTier == 3, s_Work.m_sFaction);
  ACR_TestRegress.Check("baseworks: the men in reserve don't change hands", !s_Work.m_Defenders, "");
  ACR_TestRegress.Complete("baseworks", CHECKS);
 }

 // ---------------------------------------------------------------- the AI

 protected static void RunAI(ACR_Manager manager, int second)
 {
  if (second == 5) ACR_Options.Set("acrFortifyAI", "1");
  if (second >= 5 && second % 30 == 5)
  {
   array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
   foreach (string side : sides)
   {
    ACR_FactionData data = manager.m_Bases.GetFaction(side);
    if (data) data.m_iRP = 1000;
   }
  }
  if (second < 60 || second % 10 != 0) return;
  int built, tier;
  string where = "";
  foreach (ACR_BaseWork work : ACR_BaseWorks.s_aWorks)
  {
   built += ACR_BaseWorks.BuiltSites(work);
   tier = Math.Max(tier, work.m_iTier);
   where += string.Format(" %1 %2:%3", work.m_sFaction, work.m_Base.m_sName, ACR_BaseWorks.BuiltSites(work));
  }
  if (tier < 1 && second < 500) return;
  ACR_TestRegress.Check("baseworksai: an AI commander fortifies a town", tier >= 1, string.Format("%1 sites built, by second %2:%3", built, second, where));
  // Past the first site it pays from the build budget too.
  ACR_AICommander ai = manager.AICommanderFor(ACR_FactionSelection.East());
  bool stops = false;
  if (ai)
  {
   ACR_Options.Set("acrAIBuildShare", "15");
   ai.m_fBuildBudget = 25;
   bool enough = ACR_BaseWorks.AIMayPay(ai, 20);
   ai.m_fBuildBudget = 15;
   stops = enough && !ACR_BaseWorks.AIMayPay(ai, 20);
   ACR_Options.Unset("acrAIBuildShare");
  }
  ACR_TestRegress.Check("baseworksai: the next site waits for the build budget", stops, "");
  ACR_TestRegress.Complete("baseworksai", 2);
 }
}
