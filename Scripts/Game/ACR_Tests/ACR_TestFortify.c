// -acrFortifyTest 1 (server-capable, ~4 min): the commander's Fortify order. Prefabs per side,
// options, setup rows, refusals that charge nothing and the facing rule; Alpha digs in (a regroup
// on the way pausing it without a refund; tier I: pieces placed and facing, its side's map ring),
// upgrades (tier II: the MG nest, manned by native Defend; an upgrade with a blocked spot refused
// whole), a cancelled upgrade refunded in full, a Fortify order paid partly by the refund of the
// mine order it replaces,
// an enemy team pausing the work, a truck on a spot holding the finished work until it leaves,
// tier III, the position and its wire dropping off the map once its pieces are gone, and the AI
// commander fortifying only with -acrFortifyAI, never with a squad holding a position. A new
// position with every spot blocked is refused (no wire-only position); ten minutes with the order
// set aside don't abandon it.
class ACR_TestFortify
{
 static const int CHECKS = 32;
 static const ResourceName BOULDER = "{1F0AA3EA77753B5D}Prefabs/Rocks/Granite/Granite_Boulder_02.et";
 static const ResourceName SANDBAG_WALL = "{A3A7C365E845B1B6}PrefabsEditable/Auto/Props/Military/Sandbags/E_Sandbag_01_long_high_burlap.et";
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRP;
 static vector s_vSite;
 static ACR_Fortification s_Fort;
 static ref ACR_OrderSnapshot s_Regroup;
 static ref array<IEntity> s_aWire = {};
 static ref ACR_Fortification s_Held;
 static IEntity s_Wall;
 static vector s_vNestFirst;
 static string s_sRegroup;
 static IEntity s_Enemy;
 static float s_fProgress;
 static vector s_vTruckHome;
 static int s_iVehicleWaits;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("fortify", "no Alpha squad");
   return;
  }
  if (tick > 380) ACR_TestRegress.Abort("fortify", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  int waited = tick - s_iStepAt;
  string why;

  if (s_iStep == 0)
  {
   ACR_Fortify.WORK_SCALE = 0.15;
   us.m_iRP = 500;
   CheckStatic(alpha, us);
   if (!FindSite(alpha, s_vSite))
   {
    ACR_TestRegress.Abort("fortify", "no site for the position near Alpha");
    return;
   }
   s_iRP = us.m_iRP;
   bool ordered = ACR_Fortify.Order(alpha, s_vSite, why);
   ACR_TestRegress.Check("fortify: order charges 10 RP and starts digging in", ordered && us.m_iRP == s_iRP - 10 && alpha.OrderState() == ACR_EOrderState.FORTIFY && alpha.m_Order.StartsWith("Fortifying: hasty"), string.Format("%1 rp %2 -> %3 '%4' at %5", why, s_iRP, us.m_iRP, alpha.m_Order, s_vSite));
   // At once, before the walk: a regroup sets the order aside (as ACR_Squad's regroup does)
   // and restores it a few seconds later.
   s_Regroup = alpha.SnapshotOrder();
   alpha.ClearOrders();
   alpha.SetOrder("Regrouping", ACR_EOrderState.REGROUP);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (waited < 3) return;
   s_sRegroup = string.Format("while regrouping: plan %1, rp %2, state %3", alpha.m_FortifyPlan != null, us.m_iRP, alpha.OrderState());
   if (!alpha.m_FortifyPlan || us.m_iRP != s_iRP - 10 || alpha.OrderState() != ACR_EOrderState.REGROUP) s_sRegroup = "lost " + s_sRegroup;
   alpha.RestoreOrder(s_Regroup);
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (alpha.m_FortifyPlan && waited < 150) return;
   s_Fort = ACR_Fortify.NearestOwn("US", s_vSite, 5);
   int standing;
   if (s_Fort) standing = ACR_Fortify.Standing(s_Fort);
   ACR_TestRegress.Check("fortify: tier I built and held", s_Fort && s_Fort.m_iTier == 1 && standing >= 1 && alpha.m_Fortification == s_Fort && alpha.OrderState() == ACR_EOrderState.DEFEND, string.Format("after %1 s: %2 pieces, order '%3'", waited, standing, alpha.m_Order));
   ACR_TestRegress.Check("fortify: a regroup on the way pauses the paid order; restored, it is built with no refund", !s_sRegroup.StartsWith("lost") && s_Fort && s_Fort.m_iTier == 1 && us.m_iRP == s_iRP - 10, string.Format("%1; after: rp %2 (paid %3)", s_sRegroup, us.m_iRP, s_iRP - 10));
   if (!s_Fort)
   {
    ACR_TestRegress.Abort("fortify", "no position was built");
    return;
   }
   ACR_TestRegress.Check("fortify: pieces are the side's and face the position's way", PiecesFace(s_Fort), string.Format("position yaw %1", s_Fort.m_fYaw));
   string wireDetail;
   ACR_TestRegress.Check("fortify: a joined wire line in front of the position", WireJoined(s_Fort, wireDetail), wireDetail);
   array<vector> positions = {};
   array<float> radii = {};
   array<string> labels = {};
   ACR_Fortify.Zones("US", positions, radii, labels);
   string label;
   if (!labels.IsEmpty()) label = labels[0];
   int own = labels.Count();
   labels.Clear();
   ACR_Fortify.Zones("USSR", positions, radii, labels);
   ACR_TestRegress.Check("fortify: a pale ring on its own side's map only", own == 1 && label == "~Fortified position I" && labels.IsEmpty(), string.Format("US %1 '%2', USSR %3", own, label, labels.Count()));
   // Fortify again a few metres off: the upgrade.
   s_iRP = us.m_iRP;
   // A high sandbag wall (a prop, not a composition) 8 m in front of the MG nest's first place,
   // across its line of fire.
   string nestWhy;
   if (ACR_Fortify.FindSpot(s_Fort.m_vPos, s_Fort.m_fYaw, 2, s_Fort, s_vNestFirst, nestWhy))
   {
    float wallRad = (s_Fort.m_fYaw + ACR_Fortify.SpotTurn(2)) * Math.DEG2RAD;
    s_Wall = ACR_Fortify.SpawnPiece(SANDBAG_WALL, ACR_Manager.Ground(s_vNestFirst + Vector(Math.Sin(wallRad), 0, Math.Cos(wallRad)) * 8), s_Fort.m_fYaw + ACR_Fortify.SpotTurn(2), "US");
   }
   bool upgrading = ACR_Fortify.Order(alpha, s_vSite + Vector(6, 0, 0), why);
   ACR_TestRegress.Check("fortify: ordering it again on the position upgrades it (15 RP)", upgrading && us.m_iRP == s_iRP - 15 && alpha.m_FortifyPlan && alpha.m_FortifyPlan.m_Target == s_Fort && alpha.m_FortifyPlan.m_iTier == 2 && !alpha.m_Fortification, why);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (alpha.m_FortifyPlan && waited < 150) return;
   IEntity nest = s_Fort.m_aPieces[2];
   string nestPrefab;
   if (nest) nestPrefab = nest.GetPrefabData().GetPrefabName();
   ACR_TestRegress.Check("fortify: tier II adds the MG nest", s_Fort.m_iTier == 2 && nest && nestPrefab == ACR_FobDefences.NEST_US && alpha.m_Fortification == s_Fort, string.Format("tier %1 nest %2 net %3", s_Fort.m_iTier, nest != null, s_Fort.m_aPieces[4] != null));
   // The net has a structure under it (a bare net over bare ground looked wrong, playtest).
   string covered;
   if (s_Fort.m_aPieces[4]) covered = s_Fort.m_aPieces[4].GetPrefabData().GetPrefabName();
   ACR_TestRegress.Check("fortify: tier II's covered post is a sandbag bunker under its camo net", covered == ACR_Fortify.NET_US && covered.Contains("Sandbag_Bunker_Camouflaged"), FilePath.StripPath(covered));
   string nestFireWhy;
   float nestMoved;
   if (nest) nestMoved = vector.DistanceXZ(nest.GetOrigin(), s_vNestFirst);
   bool nestFires = nest && ACR_Fortify.LineOfFire(nest, nestFireWhy);
   ACR_TestRegress.Check("fortify: an MG nest whose place faces a wall moves to one with a clear line of fire", s_Wall && nest && nestMoved > 1 && nestFires, string.Format("wall %1, nest %2 m from its first place, line of fire %3 %4", s_Wall != null, nestMoved, nestFires, nestFireWhy));
   if (s_Wall) SCR_EntityHelper.DeleteEntityAndChildren(s_Wall);
   // Tier III with its spots blocked: nothing goes in, the tier stays (CompleteFortify refunds),
   // and it can be ordered again.
   ACR_FortifyPlan next = ACR_Fortify.Plan(alpha, s_vSite, why);
   string blockedWhy;
   ACR_Fortification blocked;
   ACR_Fortify.s_bBlockSpotsForTest = true;
   if (next) blocked = ACR_Fortify.Build(next, blockedWhy);
   ACR_Fortify.s_bBlockSpotsForTest = false;
   ACR_FortifyPlan again = ACR_Fortify.Plan(alpha, s_vSite, why);
   ACR_TestRegress.Check("fortify: an upgrade with a blocked spot is refused whole, the tier kept, so it can be ordered again", next && !blocked && s_Fort.m_iTier == 2 && s_Fort.m_aPieces[2] == nest && again && again.m_iTier == 3, string.Format("'%1', tier %2, again %3", blockedWhy, s_Fort.m_iTier, again != null));
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   bool manned = Manned(s_Fort);
   if (!manned && waited < 60) return;
   ACR_TestRegress.Check("fortify: a gunner mans the position's MG", manned, string.Format("after %1 s", waited));
   // A third order called off before the pieces go in: all of it back, guns left.
   s_iRP = us.m_iRP;
   bool ordered = ACR_Fortify.Order(alpha, s_vSite, why);
   int charged = us.m_iRP;
   alpha.Halt("Holding");
   ACR_TestRegress.Check("fortify: a cancelled upgrade is refunded in full", ordered && charged == s_iRP - 25 && us.m_iRP == s_iRP && !alpha.m_FortifyPlan && !alpha.m_Fortification && s_Fort.m_iTier == 2, string.Format("%1 rp %2 -> %3 -> %4", why, s_iRP, charged, us.m_iRP));
   // Alpha starts laying mines elsewhere; the tier III Fortify order replaces that order and
   // refunds it, like any new order. Then an enemy team turns up in front of the position.
   bool laying = false;
   for (int a = 0; a < 8 && !laying; a++)
   {
    float angle = Math.PI2 * a / 8;
    laying = ACR_Minefields.Order(alpha, ACR_EMineKind.AP, ACR_Manager.Ground(s_vSite + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * 70), why);
   }
   // 12 RP left: with the mine order's 15 back, the 25 RP tier III is affordable.
   us.m_iRP = 12;
   int beforeFortify = us.m_iRP;
   ordered = laying && ACR_Fortify.Order(alpha, s_vSite, why);
   ACR_TestRegress.Check("fortify: a Fortify order replaces a mine-laying order, its refund counting towards the price", ordered && !alpha.m_MinePlan && alpha.m_FortifyPlan && alpha.m_FortifyPlan.m_iTier == 3 && us.m_iRP == beforeFortify + 15 - 25, string.Format("%1 laying %2, rp %3 -> %4", why, laying, beforeFortify, us.m_iRP));
   us.m_iRP = 450;
   ResourceName team = "{A2F75E45C66B1C0A}Prefabs/Groups/OPFOR/Group_USSR_MachineGunTeam.et";
   s_Enemy = ACR_Manager.SpawnPrefab(team, ACR_Fortify.ToWorld(s_vSite, s_Fort.m_fYaw, Vector(0, 0, 35)));
   PrintFormat("[ACR] FORTIFY test: tier III ordered %1 (%2), enemy team %3", ordered, why, s_Enemy);
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   // A 2 s window: Alpha may soon shoot the team.
   if (waited == 1 && alpha.m_FortifyPlan) s_fProgress = alpha.m_FortifyPlan.m_fProgress;
   if (waited < 3) return;
   float now;
   if (alpha.m_FortifyPlan) now = alpha.m_FortifyPlan.m_fProgress;
   ACR_TestRegress.Check("fortify: an enemy team at the site pauses the work", alpha.m_FortifyPlan && Math.AbsFloat(now - s_fProgress) < 0.01 && alpha.m_Order.Contains("contact"), string.Format("progress %1 -> %2, order '%3'", s_fProgress, now, alpha.m_Order));
   RemoveEnemy(h);
   // A truck parks on the bunker's spot.
   IEntity truck = h.m_Range.m_Truck;
   if (truck)
   {
    s_vTruckHome = truck.GetOrigin();
    truck.SetOrigin(ACR_Fortify.ToWorld(s_vSite, s_Fort.m_fYaw, ACR_Fortify.SpotOffset(4)) + Vector(0, 1, 0));
    truck.Update();
   }
   NextStep(tick);
   return;
  }

  if (s_iStep == 6)
  {
   if (alpha.m_FortifyPlan && alpha.m_Order.Contains("vehicles")) s_iVehicleWaits++;
   if (s_iVehicleWaits < 3 && alpha.m_FortifyPlan && waited < 120) return;
   ACR_TestRegress.Check("fortify: finished work waits for the truck to leave", s_iVehicleWaits >= 3 && alpha.m_FortifyPlan && s_Fort.m_iTier == 2, string.Format("waited %1 s while the truck stood there, tier %2, order '%3'", s_iVehicleWaits, s_Fort.m_iTier, alpha.m_Order));
   IEntity truck = h.m_Range.m_Truck;
   if (truck)
   {
    truck.SetOrigin(s_vTruckHome);
    truck.Update();
   }
   NextStep(tick);
   return;
  }

  if (s_iStep == 7)
  {
   if (alpha.m_FortifyPlan && waited < 60) return;
   string bunker, hmg;
   if (s_Fort.m_aPieces[4]) bunker = s_Fort.m_aPieces[4].GetPrefabData().GetPrefabName();
   if (s_Fort.m_aPieces[3]) hmg = s_Fort.m_aPieces[3].GetPrefabData().GetPrefabName();
   ACR_TestRegress.Check("fortify: tier III: bunker and heavy MG", s_Fort.m_iTier == 3 && (bunker == ACR_FobDefences.BUNKER_US || hmg == ACR_Fortify.HMG_US), string.Format("tier %1, bunker '%2', hmg '%3'", s_Fort.m_iTier, bunker, hmg));
   // Both guns can shoot: nothing but wire within 15 m in front (the user saw guns facing a
   // sandbag wall, playtest).
   string nestFire, hmgFire;
   bool nestClear = s_Fort.m_aPieces[2] && ACR_Fortify.LineOfFire(s_Fort.m_aPieces[2], nestFire);
   bool hmgClear = s_Fort.m_aPieces[3] && ACR_Fortify.LineOfFire(s_Fort.m_aPieces[3], hmgFire);
   ACR_TestRegress.Check("fortify: every gun has a clear line of fire", nestClear && hmgClear, string.Format("MG nest %1 %2, heavy MG %3 %4", nestClear, nestFire, hmgClear, hmgFire));
   // Every piece destroyed or dismantled: the position goes, and its wire with it.
   s_aWire.Copy(s_Fort.m_aWire);
   foreach (IEntity piece : s_Fort.m_aPieces) if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
   NextStep(tick);
   return;
  }

  if (s_iStep == 8)
  {
   if (waited < 7) return;
   array<vector> gonePositions = {};
   array<float> goneRadii = {};
   array<string> goneLabels = {};
   ACR_Fortify.Zones("US", gonePositions, goneRadii, goneLabels);
   int wireLeft;
   foreach (IEntity segment : s_aWire) if (segment) wireLeft++;
   ACR_TestRegress.Check("fortify: a position with no pieces left drops off the map, its wire too", ACR_Fortify.CountPositions("US") == 0 && goneLabels.IsEmpty() && !alpha.m_Fortification && s_aWire.Count() > 0 && wireLeft == 0, string.Format("%1 positions, wire %2 of %3 left", ACR_Fortify.CountPositions("US"), wireLeft, s_aWire.Count()));

   // AI commander: off by default, on with -acrFortifyAI 1.
   us.m_iRP = 500;
   ACR_AICommander ai = new ACR_AICommander(manager, "US");
   ai.StartTickForTest();
   ai.ManageFortify();
   ACR_TestRegress.Check("fortify: the AI commander does not fortify without -acrFortifyAI", !DiggingFor(manager, "US"));
   ACR_Options.Set("acrFortifyAI", "1");
   // Alpha holds a position (a stand-in): the AI picks another squad for its new one.
   s_Held = new ACR_Fortification();
   alpha.m_Fortification = s_Held;
   ai.StartTickForTest();
   ai.ManageFortify();
   ACR_Squad digger = DiggingFor(manager, "US");
   ACR_TestRegress.Check("fortify: the AI does not take a squad off a position it holds to dig a new one", digger && digger != alpha && alpha.m_Fortification, string.Format("digger %1", digger));
   alpha.m_Fortification = null;
   string where = "none";
   if (digger) where = string.Format("%1 at %2", digger.GetName(), digger.m_FortifyPlan.m_vSite);
   ACR_TestRegress.Check("fortify: with -acrFortifyAI 1 it digs a squad in on an approach", digger && digger.m_iAITask == ACR_EAITask.FORTIFY && digger.OrderState() == ACR_EOrderState.FORTIFY, where);
   if (digger) digger.Halt("Holding");
   ACR_Options.Unset("acrFortifyAI");
   ACR_TestRegress.Complete("fortify", CHECKS);
   s_iStep = 9;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 // Prefabs per side, options, setup rows, refusals and facing (nothing spawned or charged).
 protected static void CheckStatic(ACR_Squad alpha, ACR_FactionData us)
 {
  ACR_TestRegress.Check("fortify: US / USSR / RHS / FIA pieces", ACR_Fortify.PrefabFor(0, "US", 1) == ACR_FobDefences.SANDBAGS_US && ACR_Fortify.PrefabFor(0, "RHS_AFRF", 1) == ACR_FobDefences.SANDBAGS_USSR && ACR_Fortify.PrefabFor(2, "FIA", 2) == ACR_FobDefences.NEST_FIA && ACR_Fortify.PrefabFor(4, "US", 2) == ACR_Fortify.NET_US && ACR_Fortify.PrefabFor(4, "US", 3) == ACR_FobDefences.BUNKER_US && ACR_Fortify.PrefabFor(3, "USSR", 3) == ACR_Fortify.HMG_USSR);
  ACR_TestRegress.Check("fortify: defaults are orders on, AI commanders off", ACR_Fortify.Enabled() && !ACR_Fortify.AIUses() && ACR_Fortify.CanFortify(alpha));
  map<string, string> header = new map<string, string>();
  header.Set("acrFortify", "0");
  ACR_Options.UseHeaderForTest(header);
  bool headerOff = !ACR_Fortify.Enabled() && ACR_Options.Source("acrFortify") == "header";
  ACR_Options.UseHeaderForTest(null);
  ACR_TestRegress.Check("fortify: a mission header can turn the orders off", headerOff && ACR_Fortify.Enabled());
  string rows = ACR_Setup.Rows();
  ACR_TestRegress.Check("fortify: setup screen rows with their defaults", rows.Contains("fortify\tFortify orders: on") && rows.Contains("fortifyai\tAI commanders fortify: off") && ACR_Setup.Option("fortifyai") == "acrFortifyAI");

  vector probe = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(60, 0, 60));
  string why, poorWhy, baseWhy;
  ACR_Options.Set("acrFortify", "0");
  bool off = !ACR_Fortify.Plan(alpha, probe, why) && why.Contains("turned off");
  ACR_Options.Unset("acrFortify");
  us.m_iRP = 5;
  bool poor = !ACR_Fortify.Plan(alpha, probe, poorWhy) && poorWhy.Contains("RP") && us.m_iRP == 5;
  us.m_iRP = 500;
  bool atBase = false;
  if (!ACR_Base.s_All.IsEmpty()) atBase = !ACR_Fortify.Plan(alpha, ACR_Base.s_All[0].GetOrigin() + Vector(5, 0, 0), baseWhy) && baseWhy.Contains("base");
  ACR_TestRegress.Check("fortify: refused when off, short of RP or at a base, nothing charged", off && poor && atBase && us.m_iRP == 500, why + " | " + poorWhy + " | " + baseWhy);

  // Facing, with no enemy known near the spot: the nearest enemy base, even when the squad comes
  // from that side (a spot on its far side from the base).
  vector enemyBase = NearestEnemyBase("US", alpha.GetAnchor().GetOrigin());
  vector site;
  string detail = "no site away from the enemy base";
  bool baseFacing = false;
  bool contactKnown = false;
  if (enemyBase != vector.Zero && FindSite(alpha, site, alpha.GetAnchor().GetOrigin() - enemyBase))
  {
   vector siteBase = NearestEnemyBase("US", site);
   contactKnown = ACR_Intel.ThreatPoint("US", site) != siteBase;
   ACR_FortifyPlan plain = ACR_Fortify.Plan(alpha, site, why);
   vector away = site - alpha.GetAnchor().GetOrigin();
   vector toBase = siteBase - site;
   float awayYaw = Math.Atan2(away[0], away[2]) * Math.RAD2DEG;
   float baseYaw = Math.Atan2(toBase[0], toBase[2]) * Math.RAD2DEG;
   baseFacing = plain && YawGap(plain.m_fYaw, baseYaw) < 1 && YawGap(plain.m_fYaw, awayYaw) > 90;
   if (plain) detail = string.Format("facing %1, enemy base %2, away from the squad %3", plain.m_fYaw, baseYaw, awayYaw);
  }
  if (contactKnown) ACR_TestRegress.Skip("fortify: with no known enemy near, faces the nearest enemy base, even ordered from that side", "our side already knows of an enemy near the spot");
  else ACR_TestRegress.Check("fortify: with no known enemy near, faces the nearest enemy base, even ordered from that side", baseFacing, detail);

  // A known enemy within 600 m of the spot wins; a point passed in (the AI's) wins over both.
  ACR_Intel intel = ACR_Manager.GetInstance().IntelFor("US");
  ACR_Contact contact = new ACR_Contact();
  contact.m_Pos = ACR_Manager.Ground(site + Vector(150, 0, 0));
  bool contactFacing = false;
  string contactDetail = "no intel";
  if (intel)
  {
   intel.m_Contacts.Insert(contact);
   ACR_FortifyPlan seen = ACR_Fortify.Plan(alpha, site, why);
   vector target = site + Vector(-100, 0, 30);
   ACR_FortifyPlan aimed = ACR_Fortify.Plan(alpha, site, why, target);
   intel.m_Contacts.RemoveItem(contact);
   vector at = target - site;
   float atYaw = Math.Atan2(at[0], at[2]) * Math.RAD2DEG;
   contactFacing = seen && aimed && YawGap(seen.m_fYaw, 90) < 1 && YawGap(aimed.m_fYaw, atYaw) < 1;
   if (seen && aimed) contactDetail = string.Format("contact %1 vs 90, given point %2 vs %3", seen.m_fYaw, aimed.m_fYaw, atYaw);
  }
  ACR_TestRegress.Check("fortify: faces a known enemy near the spot, or the point it is given", contactFacing, contactDetail);

  // Every spot blocked: no position, so no wire-only position the upkeep would drop at once.
  ACR_FortifyPlan blockedPlan = ACR_Fortify.Plan(alpha, site, why);
  string blockedWhy;
  ACR_Fortification blockedFort;
  ACR_Fortify.s_bBlockSpotsForTest = true;
  if (blockedPlan) blockedFort = ACR_Fortify.Build(blockedPlan, blockedWhy);
  ACR_Fortify.s_bBlockSpotsForTest = false;
  ACR_TestRegress.Check("fortify: a new position with every spot blocked is refused, wire and all", blockedPlan && !blockedFort && ACR_Fortify.CountPositions("US") == 0, blockedWhy);

  // Ten minutes with the order set aside (a player holding the squad) count for neither the walk
  // limit nor the give-up timer: the first tick back doesn't abandon it. Called off after.
  bool kept = false;
  string keptDetail = "no order";
  string pauseWhy;
  vector pauseSite;
  if (FindSite(alpha, pauseSite) && ACR_Fortify.Order(alpha, pauseSite, pauseWhy) && alpha.m_FortifyPlan)
  {
   ACR_FortifyPlan pausePlan = alpha.m_FortifyPlan;
   float deadline = pausePlan.m_fDeadline;
   alpha.TickFortify(pausePlan.m_fLastTick + 600000);
   kept = alpha.m_FortifyPlan == pausePlan && alpha.OrderState() == ACR_EOrderState.FORTIFY && Math.AbsFloat(pausePlan.m_fDeadline - deadline - 599000) < 1;
   keptDetail = string.Format("'%1', deadline moved %2 s", alpha.m_Order, (pausePlan.m_fDeadline - deadline) / 1000);
   alpha.Halt("Holding");
  }
  ACR_TestRegress.Check("fortify: ten minutes set aside count for neither the walk limit nor the give-up timer", kept && us.m_iRP == 500, keptDetail);

  // A boulder where a post would go: the post moves off it.
  bool rockMoved = false;
  string rockDetail = "no site";
  vector rockSite;
  if (FindSite(alpha, rockSite))
  {
   vector postAt = ACR_Fortify.ToWorld(rockSite, 0, ACR_Fortify.SpotOffset(0));
   IEntity boulder = ACR_Manager.SpawnPrefab(BOULDER, postAt);
   bool rockSeen = ACR_Fortify.RockIn(postAt, ACR_Fortify.SpotRadius(0));
   vector moved;
   string rockWhy;
   bool placed = ACR_Fortify.FindSpot(rockSite, 0, 0, null, moved, rockWhy);
   bool offIt = !placed || (vector.DistanceXZ(moved, postAt) > 0.5 && !ACR_Fortify.RockIn(moved, ACR_Fortify.SpotRadius(0)));
   rockMoved = boulder && rockSeen && offIt;
   if (placed) rockDetail = string.Format("boulder %1, seen %2, post moved %3 m off it", boulder != null, rockSeen, vector.DistanceXZ(moved, postAt));
   else rockDetail = string.Format("boulder %1, seen %2, post skipped: every place has the rock (%3)", boulder != null, rockSeen, rockWhy);
   if (boulder) SCR_EntityHelper.DeleteEntityAndChildren(boulder);
  }
  ACR_TestRegress.Check("fortify: a piece is never put on a big rock", rockMoved, rockDetail);
 }

 // The nearest base an enemy side holds that 'faction' can see.
 protected static vector NearestEnemyBase(string faction, vector from)
 {
  vector best;
  float nearest = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (!candidate || candidate.m_bRetired || candidate.m_sOwner.IsEmpty() || candidate.m_sOwner == faction || !candidate.IsVisibleTo(faction)) continue;
   float d = vector.DistanceXZ(candidate.GetOrigin(), from);
   if (d >= nearest) continue;
   nearest = d;
   best = candidate.GetOrigin();
  }
  return best;
 }

 protected static float YawGap(float a, float b)
 {
  float gap = Math.AbsFloat(a - b);
  while (gap > 180) gap = Math.AbsFloat(gap - 360);
  return gap;
 }

 // Every standing piece is the US one for its spot and turned the position's way.
 protected static bool PiecesFace(ACR_Fortification fort)
 {
  bool any = false;
  foreach (int spot, IEntity piece : fort.m_aPieces)
  {
   if (!piece) continue;
   any = true;
   string prefab = piece.GetPrefabData().GetPrefabName();
   if (prefab != ACR_Fortify.PrefabFor(spot, "US", fort.m_iTier)) return false;
   vector angles = piece.GetYawPitchRoll();
   if (YawGap(angles[0], fort.m_fYaw + ACR_Fortify.SpotTurn(spot)) > 3) return false;
  }
  return any;
 }

 // The wire: at least three segments, all in front of the posts, each one within a segment's
 // length of the next (end to end, no gaps).
 protected static bool WireJoined(ACR_Fortification fort, out string detail)
 {
  float length = ACR_Fortify.WireLength(fort.m_vPos);
  float rad = fort.m_fYaw * Math.DEG2RAD;
  vector forward = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  float widest;
  float nearest = float.MAX;
  bool front = true;
  foreach (int i, IEntity segment : fort.m_aWire)
  {
   if (!segment) return false;
   vector offset = segment.GetOrigin() - fort.m_vPos;
   float ahead = offset[0] * forward[0] + offset[2] * forward[2];
   nearest = Math.Min(nearest, ahead);
   if (ahead < 6) front = false;
   if (i > 0) widest = Math.Max(widest, vector.DistanceXZ(segment.GetOrigin(), fort.m_aWire[i - 1].GetOrigin()));
  }
  detail = string.Format("%1 segments of %2 m, widest step %3 m, nearest %4 m ahead", fort.m_aWire.Count(), length, widest, nearest);
  return fort.m_aWire.Count() >= 3 && front && widest <= length + 0.05;
 }

 protected static bool Manned(ACR_Fortification fort)
 {
  array<IEntity> turrets = {};
  ACR_Fortify.Turrets(fort, turrets);
  foreach (IEntity turret : turrets)
   if (ACR_Roadblocks.IsTurretOccupied(turret)) return true;
  return false;
 }

 // Killed, not deleted: AI still reporting contact on a deleted body is a script error.
 protected static void RemoveEnemy(ACR_TestHarness h)
 {
  SCR_AIGroup group = SCR_AIGroup.Cast(s_Enemy);
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent && agent.GetControlledEntity()) h.Kill(agent.GetControlledEntity());
 }

 // A spot near the squad where the order plans (away from the bases, clear for its pieces);
 // towards: only spots in that direction from the squad (zero: any).
 protected static bool FindSite(ACR_Squad squad, out vector site, vector towards = vector.Zero)
 {
  towards[1] = 0;
  if (towards != vector.Zero) towards.Normalize();
  vector from = squad.GetAnchor().GetOrigin();
  for (int r = 0; r < 4; r++)
  {
   for (int a = 0; a < 12; a++)
   {
    float angle = Math.PI2 * a / 12;
    vector dir = Vector(Math.Cos(angle), 0, Math.Sin(angle));
    if (towards != vector.Zero && vector.Dot(dir, towards) < 0.7) continue;
    vector point = ACR_Manager.Ground(from + dir * (50 + r * 20));
    string why;
    if (!ACR_Fortify.Plan(squad, point, why)) continue;
    site = point;
    return true;
   }
  }
  return false;
 }

 protected static ACR_Squad DiggingFor(ACR_Manager manager, string faction)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.m_FortifyPlan && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) return squad;
  return null;
 }
}
