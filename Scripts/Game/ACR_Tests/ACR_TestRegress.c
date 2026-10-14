// -acrRegressTest 1 (server-capable): regression checks for bugs fixed after
// the 1 Oct review. Each check prints "[ACR] REGRESS PASS|FAIL <name>".
// Every test that uses Check ends with Complete(): one "[ACR] VERDICT" line (passed,
// failed, skipped, missing against the expected count, invariant errors) before the
// legacy "TEST COMPLETE" marker. TEST COMPLETE alone only means the run ended;
// tools/verdict.sh reads the VERDICT line (R11, 5 Oct review).
class ACR_TestRegress
{
 static ref ACR_Squad s_Heli;
 static int s_iReportLost;
 static int s_iPassed, s_iFailed, s_iSkipped;
 static bool s_bCompleted;
 static void Check(string name, bool ok, string detail = "")
 {
  string verdict = "FAIL";
  if (ok) { verdict = "PASS"; s_iPassed++; }
  else s_iFailed++;
  PrintFormat("[ACR] REGRESS %1 %2 %3", verdict, name, detail);
 }

 // An optional check that could not run here (e.g. no Soviet squads to reveal), with
 // the reason; it counts towards the expected total. A required check never calls
 // this: when it doesn't run it shows as missing and the run fails.
 static void Skip(string name, string reason)
 {
  s_iSkipped++;
  PrintFormat("[ACR] REGRESS SKIP %1 (%2)", name, reason);
 }

 // End of a test run: prints the verdict once, then the legacy completion marker.
 // expected = the number of checks a full run makes (-1: not declared yet, the
 // verdict then says so). Passing needs no failures, no missing or skipped checks,
 // at least one check, and no order-state invariant errors.
 static void Complete(string test = "test", int expected = -1)
 {
  if (s_bCompleted) return;
  s_bCompleted = true;
  int ran = s_iPassed + s_iFailed;
  int missing = 0;
  if (expected >= 0) missing = Math.Max(0, expected - ran - s_iSkipped);
  string role = "server";
  if (GetGame().GetPlayerController()) role = "client";
  string result = "PASS";
  if (s_iFailed > 0 || missing > 0 || ACR_Perf.s_iInvariantErrors > 0 || ran == 0) result = "FAIL";
  else if (expected < 0) result = "PASS-UNDECLARED";
  string expectedText = "?";
  if (expected >= 0) expectedText = expected.ToString();
  PrintFormat("[ACR] VERDICT %1 test=%2 role=%3 passed=%4 failed=%5 skipped=%6 missing=%7 expected=%8 invariant=%9", result, test, role, s_iPassed, s_iFailed, s_iSkipped, missing, expectedText, ACR_Perf.s_iInvariantErrors);
  Print("[ACR] TEST COMPLETE");
 }

 // A run that cannot be valid (wrong world, missing prerequisite): always a failure.
 static void Abort(string test, string reason)
 {
  if (s_bCompleted) return;
  s_bCompleted = true;
  PrintFormat("[ACR] VERDICT FAIL test=%1 aborted=%2", test, reason);
  Print("[ACR] TEST COMPLETE");
 }

 static string Verdict(bool ok)
 {
  if (ok) return "PASS";
  return "FAIL";
 }

 // Checks a full run makes (Skip counts as made). Raise it with every new check.
 static const int EXPECTED = 75; // 69 + ACR_TestOptions.CHECKS (5) + reserve base
 static SCR_AIGroup s_RecoveryGroup;
 static SCR_AIGroup s_BoardingGroup;
 static SCR_AIGroup s_CancelBoardingGroup;
 static IEntity s_ArsenalTruck;

 static ACR_Squad s_Defender;
 static SCR_AIGroup s_Stale;
 static ref ACR_Squad s_StaleSquad;
 static RplId s_StaleId;
 static const ResourceName STALE_GROUP = "{3BF36BDEEB33AEC9}Prefabs/Groups/BLUFOR/Group_US_SentryTeam.et";
 static IEntity s_Layout;
 static const ResourceName LAYOUT_BASE = "{BB1B3769EB8C290D}Prefabs/Compositions/Misc/FreeRoamBuilding/CompositionLayoutBase.et";

 static void Run(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !harness.m_Range) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  ACR_Squad bravo = manager.FindSquadByGroup(harness.m_Range.m_Bravo);
  if (!bravo) return;
  IEntity truck = harness.m_Range.m_Truck;

  // R07 (5 Oct review): frontline bases don't depend on the order bases come in.
  if (tick == 4)
  {
   ACR_TestOptions.Run();
   // AI reserve (Jacob 14 Oct): a secure non-HQ base behind the front when one exists, else the HQ.
   array<string> reserveSides = {"US", "USSR"};
   foreach (string reserveSide : reserveSides)
   {
    ACR_AICommander reserveAI = new ACR_AICommander(manager, reserveSide);
    ACR_Base reserveAt = reserveAI.ReserveBase();
    int reserveFitCount;
    string reserveFitting;
    foreach (ACR_Base candidate : ACR_Base.s_All)
     if (reserveAI.ReserveFits(candidate)) { reserveFitCount++; reserveFitting += candidate.m_sName + " "; }
    bool reserveOk = reserveAt != null && (reserveFitCount == 0 || (!reserveAI.IsHQ(reserveAt) && reserveAI.ReserveFits(reserveAt)));
    string reserveText = "none";
    if (reserveAt) reserveText = string.Format("%1 (HQ %2, %3 m from the front)", reserveAt.m_sName, reserveAI.IsHQ(reserveAt), Math.Round(reserveAI.FrontDistance(reserveAt)));
    if (reserveSide == "US") ACR_TestRegress.Check("AI reserve waits at a secure base behind the front, not the HQ when one fits", reserveOk, string.Format("US: %1; fitting: %2", reserveText, reserveFitting));
    else PrintFormat("[ACR] REGRESS info USSR reserve: %1; fitting: %2", reserveText, reserveFitting);
   }
   Check("BBB rejects both negative and out-of-range threat sectors", !BBB_Combat.ValidSector(-1) && !BBB_Combat.ValidSector(SCR_AISectorThreatFilter.SECTOR_COUNT) && !BBB_Combat.ValidSector(100000) && BBB_Combat.ValidSector(0), "native lookup bounds");
   bool standardGunship;
   ACR_FactionData usOffers = manager.m_Bases.GetFaction("US");
   if (usOffers)
    foreach (ACR_RequisitionEntry offer : usOffers.m_aRequisitions)
     if (offer.m_sVehicle.Contains("UH1H_armed_gunship") && offer.m_bCrewed && offer.m_bAircraft && offer.m_bRare) standardGunship = true;
   Check("US gunship Huey remains in standard requisition", standardGunship, "armed UH-1H is a crewed rare aircraft");
   array<int> picked = {};
   array<string> sets = {};
   array<ref array<float>> orders = {{1000, 1100, 900}, {900, 1000, 1100}, {1100, 900, 1000}};
   foreach (array<float> gaps : orders)
   {
    ACR_AICommander.FrontlinePick(gaps, picked);
    array<float> got = {};
    foreach (int i : picked) got.Insert(gaps[i]);
    got.Sort();
    string text = "";
    foreach (float g : got) text += ((int)g).ToString() + " ";
    sets.Insert(text);
   }
   ACR_AICommander.FrontlinePick({900, 1050, 1050.5}, picked);
   int edge = picked.Count();
   Check("frontline bases the same whatever the order", sets[0] == "900 1000 " && sets[1] == sets[0] && sets[2] == sets[0] && edge == 2, string.Format("'%1' '%2' '%3', boundary %4", sets[0], sets[1], sets[2], edge));
   Check("first-player host is opt-in, fixed to the first connection, and ends on disconnect", ACR_Manager.TrustedFirstPlayerHost(7, 7, true, false) && !ACR_Manager.TrustedFirstPlayerHost(8, 7, true, false) && !ACR_Manager.TrustedFirstPlayerHost(7, 7, false, false) && !ACR_Manager.TrustedFirstPlayerHost(7, 7, true, true), "denied by default, no transfer or reconnect reuse");
   float radius = 100;
   Check("base-proximity intel reports without observation through radius + 60 m", ACR_Intel.InsideBaseProximityReport(radius, radius) && ACR_Intel.InsideBaseProximityReport(radius + 59, radius) && !ACR_Intel.InsideBaseProximityReport(radius + 60, radius), "automatic reporting is intentional; line of sight is not required");
   Check("AI intel only refreshes targets detected in the last 10 s", ACR_Intel.RecentDetection(10) && !ACR_Intel.RecentDetection(10.1), "old targets stay at their last detected position");
   Check("native command wheel is refused for aircraft crews", ACR_Squad.NativeCommandAllowed(false) && !ACR_Squad.NativeCommandAllowed(true), "client hides the menu and server rejects execution");
  }
  // R04: a group that will be removed before an order for it arrives.
  if (tick == 3) s_Stale = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(STALE_GROUP, harness.Ground("1300 0 2700")));
  // R03 (5 Oct review): a refused order leaves credit alone, whatever its wording.
  if (tick == 3)
  {
   ACR_Requests requests = manager.m_Requests;
   int before = bravo.m_iOrderedBy;
   float beforeAt = bravo.m_fOrderedAt;
   int serial = bravo.m_iOrderSerial;
   ACR_OrderOutcome refused = new ACR_OrderOutcome();
   refused.Refuse("That squad belongs to another faction");
   bool creditedRefusal = requests.Credit(4242, bravo, ACR_ERequest.QUEUE_MOVE, harness.Ground("1400 0 2832"), refused, serial, before, beforeAt);
   bool untouched = bravo.m_iOrderedBy == before && bravo.m_fOrderedAt == beforeAt;
   ACR_OrderOutcome accepted = new ACR_OrderOutcome();
   accepted.Accept("anything at all");
   bool creditedQueue = requests.Credit(4242, bravo, ACR_ERequest.QUEUE_MOVE, harness.Ground("1400 0 2832"), accepted, serial, before, beforeAt);
   bool credited = bravo.m_iOrderedBy == 4242;
   bool creditedNoop = requests.Credit(4243, bravo, ACR_ERequest.MODE_FAST, vector.Zero, accepted, serial, 4242, bravo.m_fOrderedAt);
   bravo.m_iOrderedBy = before;
   bravo.m_fOrderedAt = beforeAt;
   Check("refused queued order credits nobody; accepted one credits once", !creditedRefusal && untouched && creditedQueue && credited && !creditedNoop, string.Format("refused %1 untouched %2 queued %3 credited %4 noop %5", creditedRefusal, untouched, creditedQueue, credited, creditedNoop));
  }
  if (tick == 7)
  {
   s_StaleSquad = manager.FindSquadByGroup(s_Stale);
   s_StaleId = SCR_EntityHelper.EntityToRplId(s_Stale);
   if (s_Stale)
   {
    array<AIAgent> agents = {};
    s_Stale.GetAgents(agents);
    foreach (AIAgent agent : agents) if (agent.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
    SCR_EntityHelper.DeleteEntityAndChildren(s_Stale);
   }
  }
  if (tick == 9)
  {
   string why;
   ACR_Squad resolved = manager.m_Requests.ResolveOrderSquad(null, s_StaleId, why);
   string whyTruck;
   ACR_Squad notSquad;
   if (truck) notSquad = manager.m_Requests.ResolveOrderSquad(null, SCR_EntityHelper.EntityToRplId(truck), whyTruck);
   string whyLive;
   ACR_Squad live = manager.m_Requests.ResolveOrderSquad(null, SCR_EntityHelper.EntityToRplId(harness.m_Range.m_Bravo), whyLive);
   Check("order for a removed squad is refused, never sent to another", s_StaleSquad != null && s_StaleId.IsValid() && resolved == null && why.Contains("gone") && truck && notSquad == null && live == bravo, string.Format("was a squad %1, resolved %2 '%3', truck %4, live %5", s_StaleSquad != null, resolved != null, why, notSquad != null, live == bravo));
  }

  // R01/R05 (5 Oct review): buying and reinforcing are transactions. Failures are
  // injected with ACR_Requisition.s_iTestFailSpawns (the next N spawns return nothing),
  // s_bTestNoCrew (a crew that never turns up) and a short pending timeout.
  if (tick == 11) BuyTransactions(manager, harness);
  if (tick == 21) ReinforcePart(manager);
  if (tick == 19) PendingResults(manager);
  if (tick == 13 && s_sCarrier != "")
  {
   int after = VehiclesNear(s_CarrierAt, 6);
   // Control: the good truck bought the same way is found by the same query.
   int control = 0;
   if (s_Depot) control = VehiclesNear(ACR_Manager.Ground(s_Depot.GetOrigin() + Vector(18, 0, 18)), 6);
   Check("purchase: vehicle with failed passengers is removed and refunded", s_bCarrierOk && after == s_iCarrierBefore && control >= 1, string.Format("%1, vehicles there now %2, control truck found %3", s_sCarrier, after, control));
  }

  // 1. Two dismount hand-overs in a row: the resumed order must stand.
  if (tick == 5 && truck)
  {
   int placed = bravo.BoardCargo(truck, 200, true);
   PrintFormat("[ACR] REGRESS info: Bravo placed in the truck: %1", placed);
  }
  if (tick == 9)
  {
   Check("bravo mounted for the dismount check", bravo.IsMounted(), "");
   vector target = harness.Ground("1460 0 2900");
   bravo.DismountAndContinue(ACR_Squad.WP_MOVE, target, -1, "Moving", ACR_EOrderKind.MOVE);
  }
  if (tick == 10) bravo.DismountAndContinue(ACR_Squad.WP_MOVE, harness.Ground("1460 0 2900"), -1, "Moving", ACR_EOrderKind.MOVE);
  if (tick == 20)
  {
   array<AIWaypoint> waypoints = {};
   bravo.m_Group.GetWaypoints(waypoints);
   Check("second dismount keeps the resumed order", bravo.m_Order == "Moving" && !waypoints.IsEmpty() && !bravo.IsMounted(), string.Format("order '%1' waypoints %2 mounted %3", bravo.m_Order, waypoints.Count(), bravo.IsMounted()));
  }

  // 2. Helicopter type per side, and a helicopter that can't launch refunds.
  if (tick == 22)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   Check("USSR gets the Mi-8", manager.m_Supports.HelicopterFor("USSR") == ACR_Supports.HELI_EAST, manager.m_Supports.HelicopterFor("USSR"));
   Check("door-gunner helicopters exist", Resource.Load(ACR_Supports.HELI_TRANSPORT_WEST).IsValid() && Resource.Load(ACR_Supports.HELI_TRANSPORT_EAST).IsValid(), "");
   // Engine active-AI limit fits both sides' soldier caps (4 Oct: 3/4 of squads evicted at 128).
   int aiLimit = GetGame().GetAIWorld().GetLimitOfActiveAIs();
   Check("active AI limit fits both soldier caps", aiLimit >= 2 * manager.m_iSoldierCap, string.Format("limit %1, cap %2", aiLimit, manager.m_iSoldierCap));
   Check("US gets the UH-1H", manager.m_Supports.HelicopterFor("US") == ACR_Supports.HELI_WEST, manager.m_Supports.HelicopterFor("US"));
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   string saved = soviets.m_sSupportHelicopter;
   soviets.m_sSupportHelicopter = "{0000000000000000}Prefabs/ACR/NoSuchHelicopter.et";
   ACR_FactionData data = manager.m_Bases.GetFaction("USSR");
   data.m_iRP = 500;
   int index = harness.SupportIndex(manager, ACR_ESupportType.HELI_LOITER);
   string result = manager.m_Supports.Call("USSR", index, harness.Ground("1400 0 2832"), true);
   ACR_SupportEntry entry = data.m_aSupports[index];
   Check("failed helicopter launch refunds RP and cooldown", data.m_iRP == 500 && entry.m_fReadyAt <= GetGame().GetWorld().GetWorldTime(), result + string.Format(" rp %1", data.m_iRP));
   soviets.m_sSupportHelicopter = saved;
  }

  // 3. World bounds for client-supplied points.
  if (tick == 23)
  {
   Check("point on the map", ACR_Manager.OnMap(harness.Ground("1400 0 2832")), "");
   Check("point off the map", !ACR_Manager.OnMap(Vector(9000000, 0, 9000000)), "");
  }
  // 5. (1 Oct MP) The AI commander waits while players load in, outside test runs.
  if (tick == 24)
  {
   float savedStart = manager.m_fAIHoldStart;
   manager.m_fAIHoldStart = GetGame().GetWorld().GetWorldTime();
   ACR_Perf.s_bChecks = false;
   bool held = manager.HoldAIFor("US");
   ACR_Perf.s_bChecks = true;
   bool free = !manager.HoldAIFor("US");
   manager.m_fAIHoldStart = savedStart;
   Check("AI commander held at scenario start, not in test runs", held && free, string.Format("held %1 free in tests %2", held, free));
  }
  // 6. (1 Oct MP) Rides leave once most riders are aboard.
  if (tick == 25)
   Check("ride leaves with 7 of 9 aboard, not 6", ACR_Rides.MostlyAboard(7, 9) && !ACR_Rides.MostlyAboard(6, 9), "");
  // 7. (1 Oct MP) Our full-screen map config (denser contours) is the one in use and loads.
  if (tick == 27)
  {
   SCR_MapConfigComponent mapConfig = SCR_MapConfigComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_MapConfigComponent));
   ResourceName used;
   if (mapConfig) used = mapConfig.GetGadgetMapConfig();
   Check("map uses the dense-contour config", used.Contains("ACR_MapFullscreen") && Resource.Load(used).IsValid(), used);
   // Text chat needs a chat entity in the world (4 Oct MP: nobody could type).
   Check("world has a text chat entity", GetGame().GetChat() != null, "");
   // Garrison order (3 Oct): hand-made prefabs load; its label is a defend state.
   Check("garrison waypoint and post prefabs load", Resource.Load(ACR_Squad.WP_GARRISON).IsValid() && Resource.Load(ACR_Garrison.POST_CROUCH).IsValid() && Resource.Load(ACR_Garrison.POST_STAND).IsValid());
   Check("garrisoning counts as defending", ACR_Squad.StateOfLabel("Garrisoning") == ACR_EOrderState.DEFEND);
  }
  // 8. (1 Oct MP) A construction truck's crew gets the building shovel.
  if (tick == 28)
  {
   foreach (ACR_RequisitionEntry entry : manager.m_Bases.GetFaction("US").m_aRequisitions)
   {
    if ((ACR_Logistics.ServiceOfName(entry.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
    manager.SpawnUnit("Shovel Test", "", entry.m_sVehicle, true, "", harness.Ground("1420 0 2790"), harness.Ground("1425 0 2790"));
    PrintFormat("[ACR] REGRESS info: construction truck %1", entry.m_sVehicle);
    break;
   }
  }
  if (tick == 33)
  {
   bool shovel = false;
   string truckFaction;
   bool affiliationChanges;
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Shovel Test") continue;
    IEntity shovelTruck = ACR_Transport.CrewedVehicle(squad);
    ChimeraCharacter driver = ACR_Squad.DriverOf(shovelTruck);
    shovel = driver && ACR_Manager.HasShovel(driver);
    ACR_Vehicles.SetOwner(shovelTruck, "USSR");
    FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(shovelTruck.FindComponent(FactionAffiliationComponent));
    if (affiliation && affiliation.GetAffiliatedFaction()) truckFaction = affiliation.GetAffiliatedFaction().GetFactionKey();
    affiliationChanges = truckFaction == "USSR";
    ACR_Vehicles.SetOwner(shovelTruck, "US");
   }
   Check("construction truck driver has a shovel", shovel, ACR_Requisition.s_sLastShovel);
   Check("vehicle affiliation follows its owning faction", affiliationChanges, truckFaction);
  }
  // 9. (1 Oct) Hosting with players on USSR: Arland has only a US player spawn point and
  //    the auto-spawn names US, so the game never finished loading.
  if (tick == 30)
  {
   ACR_PlayerSide.Force("USSR");
   ACR_PlayerSide.s_bSpawnChecked = false;
   ACR_PlayerSide.EnsureSpawnPoint();
   bool point = SCR_SpawnPoint.GetSpawnPointCountForFaction("USSR") > 0;
   SCR_AutoSpawnLogic autoSpawn = new SCR_AutoSpawnLogic();
   Faction forced;
   if (autoSpawn) forced = autoSpawn.ACR_ForcedFaction();
   bool faction = forced && forced.GetFactionKey() == "USSR";
   Check("players forced onto USSR get a spawn point and spawn as USSR", point && faction, string.Format("spawn point %1, auto-spawn %2, logic %3", point, faction, autoSpawn != null));
   foreach (SCR_SpawnPoint added : SCR_SpawnPoint.GetSpawnPointsForFaction("USSR")) SCR_EntityHelper.DeleteEntityAndChildren(added);
   ACR_PlayerSide.Force("");
  }
  // 4. An AI commander leaves the other side's defenders alone (with both
  //    sides on AI it used to set them free every 15 s).
  if (tick == 26)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != "USSR") continue;
    foreach (ACR_Base base : ACR_Base.s_All)
     if (base.m_sOwner == "USSR") { squad.m_iAITask = ACR_EAITask.DEFEND; squad.m_AITarget = base; s_Defender = squad; break; }
    if (s_Defender) break;
   }
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = true;
   PrintFormat("[ACR] REGRESS info: Soviet defender %1, US AI on", s_Defender != null);
  }
  // Construction silhouette is removed even when the player's main entity isn't known
  // on this machine (a dedicated server has none, like a client possessing far away).
  if (tick == 34)
  {
   SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
   s_Layout = ACR_Manager.SpawnPrefab(LAYOUT_BASE, harness.Ground("1440 0 2880"));
   SCR_CampaignBuildingLayoutComponent layout;
   if (s_Layout) layout = SCR_CampaignBuildingLayoutComponent.Cast(s_Layout.FindComponent(SCR_CampaignBuildingLayoutComponent));
   int prefabId = -1;
   if (building && !building.GetCompositionResourceName(0).IsEmpty()) prefabId = 0;
   if (layout && prefabId >= 0)
   {
    layout.SetPrefabId(prefabId);
    layout.SpawnPreview();
    bool shown = layout.HasBuildingPreview();
    layout.DeletePreview();
    Check("construction silhouette is removed without a known main entity", shown && !layout.HasBuildingPreview(), string.Format("shown %1", shown));
   }
   else PrintFormat("[ACR] REGRESS info: silhouette check skipped (layout %1, composition id %2)", layout != null, prefabId);
  }
  if (tick == 35 && s_Layout) SCR_EntityHelper.DeleteEntityAndChildren(s_Layout);
  // Reinforce restocks ammo only inside a friendly base that can deploy units.
  if (tick == 36 || tick == 38)
  {
   ACR_Base depot;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == "US" && base.m_bAllowSpawn && !base.m_bContested) { depot = base; break; }
   array<AIAgent> agents = {};
   bravo.GetAgents(agents);
   IEntity soldier;
   if (!agents.IsEmpty()) soldier = agents[0].GetControlledEntity();
   if (!depot || !soldier) PrintFormat("[ACR] REGRESS info: restock check skipped (base %1, soldier %2)", depot != null, soldier != null);
   else if (tick == 36)
   {
    soldier.SetOrigin(ACR_Manager.Ground(depot.GetOrigin() + Vector(3, 0, 3)));
    InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(soldier.FindComponent(InventoryStorageManagerComponent));
    array<IEntity> magazines = {};
    if (inventory) inventory.FindItemsWithComponents(magazines, {MagazineComponent});
    foreach (IEntity magazine : magazines) inventory.TryDeleteItem(magazine);
    PrintFormat("[ACR] REGRESS info: %1 magazines removed", magazines.Count());
   }
   else
   {
    soldier.SetOrigin(ACR_Manager.Ground(depot.GetOrigin() + Vector(3, 0, 3)));
    float before = ACR_Logistics.SoldierMagazines(soldier);
    Check("no deploy base outside a base circle", manager.DeployBaseAt("US", depot.GetOrigin() + Vector(depot.m_fRadius + 50, 0, 0)) == null, "");
    Check("deploy base found inside its circle", manager.DeployBaseAt("US", soldier.GetOrigin()) == depot, depot.m_sName);
    int restocked = manager.RestockAmmo(bravo, depot);
    float after = ACR_Logistics.SoldierMagazines(soldier);
    Check("reinforce restocks ammo inside a deploy base", restocked >= 1 && after > before, string.Format("magazines %1 -> %2, restocked %3", before, after, restocked));
   }
  }
  // One saved-order type (refactor 2 Oct): a snapshot puts the same order back.
  if (tick == 42)
  {
   bravo.Defend(harness.Ground("1450 0 2890"));
   ACR_OrderSnapshot saved = bravo.SnapshotOrder();
   bravo.Halt("Halted");
   bool halted = bravo.OrderState() == ACR_EOrderState.HOLD;
   bravo.RestoreOrder(saved);
   bool back = bravo.m_Order == saved.m_sLabel && bravo.OrderState() == ACR_EOrderState.DEFEND && bravo.m_OrderKind == ACR_EOrderKind.DEFEND && bravo.m_sOrderPrefab == ACR_Squad.WP_DEFEND && vector.Distance(bravo.m_MoveTarget, saved.m_Target) < 0.1;
   Check("a saved order comes back unchanged", halted && back, string.Format("label '%1' state %2 kind %3", bravo.m_Order, typename.EnumToString(ACR_EOrderState, bravo.OrderState()), bravo.m_OrderKind));
  }
  // Debug menu actions (2 Oct).
  if (tick == 44)
  {
   ACR_Debug dbg = manager.m_Debug;
   ACR_FactionData usData = manager.m_Bases.GetFaction("US");
   usData.m_iRP = 100;
   dbg.Run(manager, ACR_EDebug.RP_OWN, "US", "");
   Check("debug: +500 RP to own side", usData.m_iRP == 100 + ACR_Debug.RP_STEP, usData.m_iRP.ToString());
   foreach (ACR_SupportEntry sup : usData.m_aSupports) sup.m_fReadyAt = GetGame().GetWorld().GetWorldTime() + 999000;
   dbg.Run(manager, ACR_EDebug.RESET_SUPPORTS, "US", "");
   bool ready = true;
   foreach (ACR_SupportEntry sup2 : usData.m_aSupports) if (sup2.m_fReadyAt > GetGame().GetWorld().GetWorldTime()) ready = false;
   Check("debug: supports ready again", ready && !usData.m_aSupports.IsEmpty(), "");
   ACR_Base sovietBase;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner == "USSR" && (!sovietBase || base.m_bIncome)) sovietBase = base; // a label with [value] if possible
   if (sovietBase)
   {
    dbg.Run(manager, ACR_EDebug.CAPTURE_OWN, "US", sovietBase.GetLabel());
    bool ours = sovietBase.m_sOwner == "US";
    dbg.Run(manager, ACR_EDebug.CAPTURE_ENEMY, "US", sovietBase.GetLabel());
    Check("debug: capture a base by its map label and hand it back", ours && sovietBase.m_sOwner == "USSR", sovietBase.GetLabel());
   }
   dbg.Run(manager, ACR_EDebug.PAUSE_ENEMY, "USSR", "");
   bool paused = dbg.IsPaused("US") && dbg.Status(manager, "USSR").Contains("PAUSED");
   dbg.Run(manager, ACR_EDebug.PAUSE_ENEMY, "USSR", "");
   Check("debug: pause and resume the enemy AI", paused && !dbg.IsPaused("US"), dbg.Status(manager, "USSR"));
   string attack = dbg.Run(manager, ACR_EDebug.ATTACK_NOW, "USSR", "");
   if (attack.Contains("no free squads")) Skip("debug: forced AI attack", attack);
   else Check("debug: forced AI attack", attack.Contains("attacking"), attack);
   dbg.Run(manager, ACR_EDebug.REVEAL, "US", "");
  }
  if (tick == 46)
  {
   int sovietsSeen;
   foreach (IEntity seen, vector seenAt : manager.IntelFor("US").m_SeenAt) if (ACR_Intel.FactionKeyOf(seen) == "USSR") sovietsSeen++;
   int sovietSoldiers;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR") sovietSoldiers += squad.Strength();
   manager.m_Debug.Run(manager, ACR_EDebug.REVEAL, "US", "");
   if (sovietSoldiers == 0) Skip("debug: reveal shows enemy soldiers", "no Soviet squads");
   else Check("debug: reveal shows enemy soldiers", sovietsSeen > 0 && !manager.m_Debug.Reveals("US"), string.Format("%1 seen of about %2", sovietsSeen, sovietSoldiers));
  }
  if (tick == 45) MountedRecoveryCheck(manager, truck);
  // Vehicle crews carry their unit's name even when they register before they're seated
  // (2 Oct live: Truck 1 and Humvee 1 showed as "Squad 1" and "Squad 2").
  if (tick == 40)
  {
   s_RecoveryGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(STALE_GROUP, harness.Ground("1430 0 2850")));
   s_BoardingGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(STALE_GROUP, harness.Ground("1506 0 2956")));
   s_CancelBoardingGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(STALE_GROUP, harness.Ground("1508 0 2958")));
   s_ArsenalTruck = ACR_Manager.SpawnPrefab("{1FF144F8A209239D}Prefabs/Vehicles/Wheeled/M923A1/M923A1_arsenal.et", harness.Ground("1450 0 2850"));
   ACR_Vehicles.SetOwner(s_ArsenalTruck, "USSR");
   manager.m_sSpawningFaction = "US";
   manager.SpawnUnit("Regress Truck", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", harness.Ground("1500 0 2950"), harness.Ground("1506 0 2956"));
   // A crewed helicopter outside the helipads (Game Master / bought): AI can't fly it, so its
   // crew must never get a move (4 Oct MP: both pilots bailed out and walked).
   manager.SpawnUnit("Regress Heli", "", ACR_Supports.HELI_TRANSPORT_WEST, true, "", harness.Ground("1490 0 2870"), harness.Ground("1496 0 2876"));
  }
  if (tick == 44)
  {
   RecoveryChecks(manager, truck);
   StartBoardingCheck(manager);
   SCR_ArsenalComponent arsenal = ACR_Vehicles.ArsenalOf(s_ArsenalTruck);
   bool arsenalReady;
   if (arsenal)
   {
    FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(s_ArsenalTruck.FindComponent(FactionAffiliationComponent));
    if (affiliation) affiliation.SetAffiliatedFaction(null);
    array<SCR_ArsenalItem> items = {};
    arsenalReady = arsenal.GetAssignedFaction() && arsenal.GetAssignedFaction().GetFactionKey() == "USSR" && arsenal.GetFilteredArsenalItems(items) && !items.IsEmpty();
   }
   Check("empty purchased arsenal keeps its purchasing faction and item catalog", arsenalReady, "US prefab bought by USSR, current vehicle faction cleared");
   // Keep the truck until server shutdown: nearby AI may still hold a contact
   // report for it, so immediate deletion would invalidate that radio request.
   s_Heli = null;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Regress Heli") s_Heli = squad;
   bool moved;
   if (s_Heli) moved = s_Heli.MoveTo(harness.Ground("1600 0 2700"));
   Check("helicopter crew is known as one and refuses a move", s_Heli != null && s_Heli.Aircraft() != null && !moved, string.Format("found %1, moved %2", s_Heli != null, moved));
   AircraftArmourCheck(manager);
  }
  if (tick == 48)
  {
   ACR_Squad crew;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Regress Truck") crew = squad;
   Check("vehicle crew keeps its unit name", crew != null && crew.m_OwnVehicle != null, string.Format("found %1", crew != null));
   if (crew) Check("idle truck crew can give rides", manager.m_Rides && manager.m_Rides.WhyNotTaxi(crew, "US").IsEmpty(), manager.m_Rides.WhyNotTaxi(crew, "US"));
  }
  // After-action report (5 Oct): kills/losses, bases, RP tallied per side.
  if (tick == 45)
  {
   ACR_SideStats ussr = ACR_Report.Of("USSR");
   s_iReportLost = ussr.m_iLost;
   IEntity victim;
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != "USSR") continue;
    array<AIAgent> agents = {};
    squad.GetAgents(agents);
    if (!agents.IsEmpty()) { victim = agents[0].GetControlledEntity(); break; }
   }
   if (victim) harness.Kill(victim);
   else Print("[ACR] REGRESS info: report kill skipped (no Soviet soldier)");
  }
  if (tick == 47)
  {
   ACR_SideStats us = ACR_Report.Of("US");
   ACR_SideStats ussr2 = ACR_Report.Of("USSR");
   string text = ACR_Report.Text("US");
   Check("report: bases won and lost tallied", us.m_iBasesWon >= 1 && ussr2.m_iBasesLost >= 1, string.Format("US won %1, USSR lost %2", us.m_iBasesWon, ussr2.m_iBasesLost));
   Check("report: a dead soldier counts as lost", ussr2.m_iLost > s_iReportLost, string.Format("%1 -> %2", s_iReportLost, ussr2.m_iLost));
   Check("report: RP spent tallied and panel text built", us.m_iRPSpent > 0 && text.Contains("Bases won"), string.Format("spent %1 | %2", us.m_iRPSpent, text.Length()));
   ACR_Report.LogAll();
  }
  // Stepped pricing (5 Oct, Jacob's threat = price climb): ordinary units unlimited, each
  // purchase raises that unit's price by the threat step; rare units keep a fixed stock.
  if (tick == 47)
  {
   ACR_FactionData usData = manager.m_Bases.GetFaction("US");
   ACR_Base depot;
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base.m_sOwner == "US" && base.m_bAllowSpawn && !base.m_bContested) { depot = base; break; }
   int pick = -1;
   int rare = -1;
   foreach (int i, ACR_RequisitionEntry entry : usData.m_aRequisitions)
   {
    if (pick < 0 && entry.m_sVehicle.IsEmpty() && !entry.m_sGroup.IsEmpty() && !entry.m_bMortarTeam) pick = i;
    if (rare < 0 && entry.m_bMortarTeam) rare = i;
   }
   if (pick < 0 || !depot) PrintFormat("[ACR] REGRESS info: pricing check skipped (entry %1, base %2)", pick, depot != null);
   else
   {
    ACR_RequisitionEntry unit = usData.m_aRequisitions[pick];
    int basePrice = unit.m_iBaseCost;
    int boughtBefore = unit.m_iBought;
    int firstPrice = unit.m_iCost;
    usData.m_iRP = 1000;
    string first = manager.m_Requisition.PurchaseAt("US", pick, depot);
    int afterFirst = usData.m_iRP;
    int secondPrice = unit.m_iCost;
    string second = manager.m_Requisition.PurchaseAt("US", pick, depot);
    int paidFirst = 1000 - afterFirst;
    int paidSecond = afterFirst - usData.m_iRP;
    bool climbs = paidFirst == firstPrice && paidSecond == secondPrice && secondPrice == ACR_Pricing.PriceAfter(basePrice, boughtBefore + 1) && secondPrice > firstPrice && unit.m_iCost == ACR_Pricing.PriceAfter(basePrice, boughtBefore + 2);
    Check("stepped pricing: each purchase raises the unit's price", unit.m_bStepped && climbs && ACR_Pricing.Unlimited(unit.m_iRemaining), string.Format("%1: paid %2 then %3, now %4 (base %5) | '%6' / '%7'", unit.m_sName, paidFirst, paidSecond, unit.m_iCost, basePrice, first, second));
    Check("stepped pricing: normal threat step is 15 percent", ACR_Pricing.PriceAfter(100, 1) == 115 && ACR_Pricing.PriceAfter(20, 3) == 29, string.Format("%1 %2", ACR_Pricing.PriceAfter(100, 1), ACR_Pricing.PriceAfter(20, 3)));
    if (rare >= 0)
    {
     ACR_RequisitionEntry mortar = usData.m_aRequisitions[rare];
     Check("stepped pricing: rare units keep a fixed stock and price", mortar.m_bRare && !mortar.m_bStepped && mortar.m_iRemaining + mortar.m_iBought == mortar.m_iStock && mortar.m_iCost == mortar.m_iBaseCost, string.Format("%1 stock %2 left %3", mortar.m_sName, mortar.m_iStock, mortar.m_iRemaining));
    }
    usData.m_iRP = 100;
   }
  }
  // No Reinforce with enemy troops close by (Jacob, 4 Oct; 300 m in play). The radius is moved
  // either side of the nearest enemy, so nobody has to be teleported into a fight.
  if (tick == 49 && bravo && bravo.GetAnchor())
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   IEntity nearest = manager.m_Supports.FindEnemyNear("US", bravo.GetAnchor().GetOrigin(), 100000);
   if (!nearest) Print("[ACR] REGRESS info: reinforce contact check skipped (no enemy soldier)");
   else
   {
    int gap = (int)vector.DistanceXZ(nearest.GetOrigin(), bravo.GetAnchor().GetOrigin());
    int keep = ACR_Requisition.REINFORCE_CLEAR_RADIUS;
    ACR_Requisition.REINFORCE_CLEAR_RADIUS = gap + 20;
    string refused = manager.m_Requisition.InContact(bravo);
    ACR_Requisition.REINFORCE_CLEAR_RADIUS = Math.Max(1, gap - 20);
    string allowed = manager.m_Requisition.InContact(bravo);
    ACR_Requisition.REINFORCE_CLEAR_RADIUS = keep;
    Check("reinforce refused with enemy troops close, explained", refused.Contains("enemy troops within") && allowed == "", string.Format("nearest %1 m: '%2' / '%3'", gap, refused, allowed));
   }
  }
  if (tick == 50)
  {
   bool kept = s_Defender && s_Defender.m_iAITask == ACR_EAITask.DEFEND;
   Check("AI commander leaves the other side's defenders alone", kept, "");
   int seated, crewmen;
   if (s_Heli)
   {
    array<AIAgent> agents = {};
    s_Heli.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     crewmen++;
     if (ACR_Transport.VehicleOf(agent.GetControlledEntity())) seated++;
    }
   }
   Check("helicopter crew stays seated after a refused move", crewmen > 0 && seated == crewmen, string.Format("%1 of %2 seated", seated, crewmen));
  }
  if (tick == 60)
  {
   BoardingCheck(manager);
   PrintFormat("[ACR] REGRESS invariant errors: %1", ACR_Perf.s_iInvariantErrors);
   Complete("regress", EXPECTED);
  }
 }

 static void RecoveryChecks(ACR_Manager manager, IEntity truck)
 {
  ACR_Squad squad = manager.FindSquadByGroup(s_RecoveryGroup);
  bool queuedKept, restored, halted;
  if (squad && squad.GetAnchor())
  {
   vector first = ACR_Manager.Ground(squad.GetAnchor().GetOrigin() + Vector(600, 0, 0));
   vector second = ACR_Manager.Ground(first + Vector(0, 0, 200));
   squad.MoveTo(first);
   squad.Queue(second, false);
   squad.m_iOrderedBy = 4242;
   int serial = squad.m_iOrderSerial;
   AIWaypoint dropped = squad.m_Group.GetCurrentWaypoint();
   if (dropped) { squad.m_Group.RemoveWaypoint(dropped); SCR_EntityHelper.DeleteEntityAndChildren(dropped); }
   squad.Tactics(50, GetGame().GetWorld().GetWorldTime());
   array<vector> points = {};
   array<int> kinds = {};
   squad.GetPlannedPoints(points, kinds);
   queuedKept = squad.m_MoveTarget == first && points.Count() == 1 && points[0] == second;
   restored = squad.m_Group.GetCurrentWaypoint() != null && squad.m_iOrderSerial == serial && squad.m_iOrderedBy == 4242;
   squad.Halt("Holding");
   squad.Tactics(50, GetGame().GetWorld().GetWorldTime() + 11000);
   halted = !squad.m_Group.GetCurrentWaypoint() && squad.OrderState() == ACR_EOrderState.HOLD;
   if (truck) squad.BoardCargo(truck, 10000, true, true);
  }
  Check("dropped waypoint keeps the queued destination behind the current move", queuedKept, "no silent skip");
  Check("dropped move is reissued without changing issuer or order serial", restored, "recovery preserves attribution");
  Check("Halt never recovers an old move", halted, "explicit cancellation");
  ACR_FeedFrame frame = ACR_MapFeed.Build(manager, GetGame().GetFactionManager().GetFactionByKey("US"));
  int count = frame.m_Leaders.Count();
  Check("friendly map feed keeps all parallel marker arrays aligned", frame.m_LeaderPos.Count() == count && frame.m_GroupNames.Count() == count && frame.m_Symbols.Count() == count * 4 && frame.m_Status.Count() == count * 6 && frame.m_GroupIds.Count() == count && frame.m_Commandable.Count() == count && frame.m_LeaderStates.Count() == count && frame.m_Details.Count() == count, count.ToString());
 }

 static void MountedRecoveryCheck(ACR_Manager manager, IEntity truck)
 {
  ACR_Squad squad = manager.FindSquadByGroup(s_RecoveryGroup);
  bool mounted;
  if (squad && truck)
  {
   squad.MoveTo(ACR_Manager.Ground(truck.GetOrigin() + Vector(600, 0, 0)));
   AIWaypoint dropped = squad.m_Group.GetCurrentWaypoint();
   if (dropped) { squad.m_Group.RemoveWaypoint(dropped); SCR_EntityHelper.DeleteEntityAndChildren(dropped); }
   squad.Tactics(50, GetGame().GetWorld().GetWorldTime());
   mounted = squad.IsMounted() && squad.m_Group.GetCurrentWaypoint() != null;
   squad.Halt("Holding");
   // The server shuts down after this test. Keep the crew alive until then:
   // deleting them here invalidates vanilla radio requests still in flight.
  }
  Check("final mounted move recovers a dropped waypoint", mounted, "boarding settled before the check");
 }

 static void StartBoardingCheck(ACR_Manager manager)
 {
  ACR_Squad passengers = manager.FindSquadByGroup(s_BoardingGroup);
  ACR_Squad crew;
  foreach (ACR_Squad candidate : manager.m_Squads) if (candidate.GetName() == "Regress Truck") crew = candidate;
  if (passengers && crew && crew.m_OwnVehicle) passengers.GetIn(crew.m_OwnVehicle.GetOrigin());
  ACR_Squad cancelled = manager.FindSquadByGroup(s_CancelBoardingGroup);
  if (cancelled && crew && crew.m_OwnVehicle)
  {
   cancelled.GetIn(crew.m_OwnVehicle.GetOrigin());
   cancelled.Halt("Holding");
  }
 }

 static void BoardingCheck(ACR_Manager manager)
 {
  ACR_Squad squad = manager.FindSquadByGroup(s_BoardingGroup);
  ACR_Squad crew;
  foreach (ACR_Squad candidate : manager.m_Squads) if (candidate.GetName() == "Regress Truck") crew = candidate;
  int seated;
  if (squad && crew && crew.m_OwnVehicle)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) if (ACR_Transport.VehicleOf(agent.GetControlledEntity()) == crew.m_OwnVehicle) seated++;
  }
  Check("Get In boards passengers in a truck crewed by another squad", crew && ACR_Squad.DriverOf(crew.m_OwnVehicle) && seated > 0, string.Format("%1 passengers", seated));
  ACR_Squad cancelled = manager.FindSquadByGroup(s_CancelBoardingGroup);
  int cancelledSeated;
  if (cancelled)
  {
   array<AIAgent> cancelledAgents = {};
   cancelled.GetAgents(cancelledAgents);
   foreach (AIAgent cancelledAgent : cancelledAgents) if (ACR_Transport.VehicleOf(cancelledAgent.GetControlledEntity())) cancelledSeated++;
  }
  Check("Halt cancels delayed Get In seating", cancelled && cancelled.OrderState() == ACR_EOrderState.HOLD && cancelledSeated == 0, string.Format("%1 passengers", cancelledSeated));
 }

 // ---------------------------------------------------------------- R01/R05 helpers
 static ACR_Base s_Depot;
 static int s_iTruckEntry = -1;
 static int s_iTruckBought, s_iTruckNamed, s_iRPAfterBuys, s_iTruckPaid;
 static string s_sGoodTruck, s_sNoCrewTruck;
 static void AircraftArmourCheck(ACR_Manager manager)
 {
  IEntity helicopter;
  if (s_Heli) helicopter = s_Heli.Aircraft();
  array<AIAgent> crewAgents = {};
  if (s_Heli) s_Heli.GetAgents(crewAgents);
  IEntity occupant;
  BaseMagazineComponent magazine;
  foreach (AIAgent crewAgent : crewAgents)
  {
   IEntity man = crewAgent.GetControlledEntity();
   if (!man || BBB_Armour.VehicleOf(man) != helicopter) continue;
   occupant = man;
   break;
  }
  // Pilots can keep their personal weapon holstered. Use a carried magazine
  // from any fixture soldier rather than requiring the pilot to draw a gun.
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!soldier) continue;
    BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
    if (!weapons) continue;
    array<IEntity> carried = {};
    weapons.GetWeaponsList(carried);
    foreach (IEntity item : carried)
    {
     BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
     if (weapon) magazine = weapon.GetCurrentMagazine();
     if (magazine) break;
    }
    if (magazine) break;
   }
   if (magazine) break;
  }
  if (!helicopter || !occupant || !magazine || !s_ArsenalTruck)
  {
   Check("aircraft hull hits never train armour learning", false, "missing aircraft/crew/magazine/truck fixture");
   Check("aircraft and exposed crew ignore an existing useless-round verdict", false, "missing fixture");
   Check("ground vehicles retain armour learning and fire holds", false, "missing fixture");
   Check("aircraft and seated crew cannot create group gunshot suppression contacts", false, "missing fixture");
   Check("gunner drill rejects airborne sectors and keeps ground sectors", false, "missing fixture");
   return;
  }
  ref map<string, ref BBB_ArmourStat> savedStats = BBB_Armour.s_Stats;
  bool savedLearn = BBB_Armour.s_bLearn;
  BBB_Armour.s_Stats = new map<string, ref BBB_ArmourStat>();
  BBB_Armour.s_bLearn = true;
  BaseDamageContext hit = new BaseDamageContext();
  hit.damageType = EDamageType.KINETIC;
  hit.damageSource = s_ArsenalTruck; // Synthetic callback only; no damage applied.
  for (int n = 0; n < BBB_Armour.s_iMinHits; n++) BBB_Armour.OnVehicleHit(helicopter, hit);
  BBB_Armour.OnOccupantHit(occupant, hit);
  Check("aircraft hull hits never train armour learning", BBB_Armour.s_Stats.IsEmpty());
  BBB_ArmourStat learned = new BBB_ArmourStat();
  learned.m_bUseless = true;
  string ammo = BBB_Armour.Round(BBB_Armour.Key(magazine.GetAmmoType(0)));
  BBB_Armour.s_Stats.Set(BBB_Armour.PrefabKey(helicopter) + "|" + ammo, learned);
  Check("aircraft and exposed crew ignore an existing useless-round verdict", !BBB_Armour.UselessEntity(helicopter, magazine) && !BBB_Armour.UselessEntity(occupant, magazine));
  for (int k = 0; k < BBB_Armour.s_iMinHits; k++) BBB_Armour.OnVehicleHit(s_ArsenalTruck, hit);
  BBB_Armour.s_Stats.Set(BBB_Armour.PrefabKey(s_ArsenalTruck) + "|" + ammo, learned);
  Check("ground vehicles retain armour learning and fire holds", BBB_Armour.HitsOn(BBB_Armour.PrefabKey(s_ArsenalTruck)) >= BBB_Armour.s_iMinHits && BBB_Armour.UselessEntity(s_ArsenalTruck, magazine));
  BBB_Armour.s_Stats = savedStats;
  BBB_Armour.s_bLearn = savedLearn;
  Check("aircraft and seated crew cannot create group gunshot suppression contacts", BBB_SuppressionSafety.AircraftSource(helicopter) && BBB_SuppressionSafety.AircraftSource(occupant) && !BBB_SuppressionSafety.AircraftSource(s_ArsenalTruck));
  vector aircraftSector = helicopter.GetOrigin() + Vector(0, 40, 0);
  vector groundSector = BBB.Ground(helicopter.GetOrigin()) + Vector(0, 1.5, 0);
  Check("gunner drill rejects airborne sectors and keeps ground sectors", !BBB_SuppressionSafety.GroundSector(aircraftSector) && BBB_SuppressionSafety.GroundSector(groundSector));
 }

 static IEntity s_NoCrewVehicle;
 static SCR_AIGroup s_ReinforceGroup;
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";

 static ref array<IEntity> s_Found = {};
 static bool CollectVehicle(IEntity e)
 {
  if (Vehicle.Cast(e)) s_Found.Insert(e);
  return true;
 }
 // Vehicles in the world near a point (a world query, not our registry).
 static int VehiclesNear(vector at, float radius)
 {
  s_Found.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(at, radius, CollectVehicle);
  return s_Found.Count();
 }
 static bool s_bCarrierOk;
 static string s_sCarrier;
 static vector s_CarrierAt;
 static int s_iCarrierBefore;

 static void BuyTransactions(ACR_Manager manager, ACR_TestHarness harness)
 {
  ACR_Requisition req = manager.m_Requisition;
  ACR_FactionData usData = manager.m_Bases.GetFaction("US");
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == "US" && base.m_bAllowSpawn && !base.m_bContested) { s_Depot = base; break; }
  if (!s_Depot) { Print("[ACR] REGRESS info: no US deploy base for the purchase checks"); return; }
  vector centre = s_Depot.GetOrigin();
  int infantry = -1, mortar = -1;
  foreach (int i, ACR_RequisitionEntry e : usData.m_aRequisitions)
  {
   if (infantry < 0 && e.m_sVehicle.IsEmpty() && !e.m_sGroup.IsEmpty() && !e.m_bMortarTeam && !e.m_bObserverTeam && e.m_bStepped) infantry = i;
   if (mortar < 0 && e.m_bMortarTeam && e.m_iRemaining > 0) mortar = i;
   if (s_iTruckEntry < 0 && e.m_bTransport && e.m_bCrewed && e.m_sPassengers.IsEmpty() && e.m_sGroup.IsEmpty() && e.m_iSupportHeli == 0) s_iTruckEntry = i;
  }
  // a) infantry that never spawns: nothing charged, no stock or price step taken.
  if (infantry >= 0)
  {
   ACR_RequisitionEntry unit = usData.m_aRequisitions[infantry];
   usData.m_iRP = 1000;
   int left = unit.m_iRemaining, bought = unit.m_iBought, cost = unit.m_iCost;
   ACR_Requisition.s_iTestFailSpawns = 1;
   string said = req.PurchaseAt("US", infantry, s_Depot);
   ACR_Requisition.s_iTestFailSpawns = 0;
   Check("purchase: a unit that fails to spawn costs nothing", usData.m_iRP == 1000 && unit.m_iRemaining == left && unit.m_iBought == bought && unit.m_iCost == cost && said.Contains("could not be deployed"), string.Format("rp %1, left %2/%3, bought %4/%5, cost %6/%7 | %8", usData.m_iRP, unit.m_iRemaining, left, unit.m_iBought, bought, unit.m_iCost, cost, said));
  }
  // b) a vehicle whose passengers fail: the vehicle it made is taken back too.
  if (infantry >= 0)
  {
   ACR_RequisitionEntry carrier = new ACR_RequisitionEntry();
   carrier.Set("Regress Carrier", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", 10, 3);
   carrier.m_sPassengers = usData.m_aRequisitions[infantry].m_sGroup;
   carrier.m_iRemaining = 3;
   carrier.m_iBaseCost = 10;
   int index = usData.m_aRequisitions.Insert(carrier);
   usData.m_iRP = 1000;
   vector at = ACR_Manager.Ground(centre + Vector(-12, 0, 12));
   int before = VehiclesNear(at, 6);
   ACR_Requisition.s_iTestPassSpawns = 1; // the truck
   ACR_Requisition.s_iTestFailSpawns = 1; // its passengers
   string said = req.PurchaseAt("US", index, s_Depot, at, true);
   ACR_Requisition.s_iTestFailSpawns = 0;
   ACR_Requisition.s_iTestPassSpawns = 0;
   usData.m_aRequisitions.RemoveOrdered(index);
   bool crewNamed = false;
   foreach (IEntity v, string crewName : manager.m_PendingCrewNames) if (crewName.Contains("Regress Carrier")) crewNamed = true;
   s_bCarrierOk = usData.m_iRP == 1000 && carrier.m_iRemaining == 3 && !crewNamed && said.Contains("passengers");
   s_sCarrier = string.Format("rp %1, left %2, crew name pending %3, vehicles before %4 | %5", usData.m_iRP, carrier.m_iRemaining, crewNamed, before, said);
   s_CarrierAt = at;
   s_iCarrierBefore = before;
  }
  // c) a mortar team short of a crewman: all or nothing.
  if (mortar >= 0)
  {
   ACR_RequisitionEntry tube = usData.m_aRequisitions[mortar];
   usData.m_iRP = 1000;
   int left = tube.m_iRemaining;
   int teams = manager.Mortars().m_Teams.Count();
   ACR_Requisition.s_iTestPassSpawns = 1;
   ACR_Requisition.s_iTestFailSpawns = 1;
   string said = req.PurchaseAt("US", mortar, s_Depot, ACR_Manager.Ground(centre + Vector(14, 0, -14)), true);
   ACR_Requisition.s_iTestFailSpawns = 0;
   ACR_Requisition.s_iTestPassSpawns = 0;
   Check("purchase: mortar team short of crew is refunded, no team left", usData.m_iRP == 1000 && tube.m_iRemaining == left && manager.Mortars().m_Teams.Count() == teams, string.Format("rp %1, left %2/%3, teams %4/%5 | %6", usData.m_iRP, tube.m_iRemaining, left, manager.Mortars().m_Teams.Count(), teams, said));
  }
  // d+e) two trucks from one pool: one gets its (delayed) crew, the other's never comes.
  if (s_iTruckEntry >= 0)
  {
   ACR_RequisitionEntry truck = usData.m_aRequisitions[s_iTruckEntry];
   usData.m_iRP = 1000;
   s_iTruckBought = truck.m_iBought;
   s_iTruckNamed = truck.m_iNamed;
   s_sGoodTruck = string.Format("%1 %2", truck.m_sName, truck.m_iNamed + 1);
   req.PurchaseAt("US", s_iTruckEntry, s_Depot, ACR_Manager.Ground(centre + Vector(18, 0, 18)), true);
   ACR_Requisition.s_bTestNoCrew = true;
   ACR_Requisition.s_fPendingTimeoutMs = 4000;
   s_sNoCrewTruck = string.Format("%1 %2", truck.m_sName, truck.m_iNamed + 1);
   s_iTruckPaid = truck.m_iCost;
   req.PurchaseAt("US", s_iTruckEntry, s_Depot, ACR_Manager.Ground(centre + Vector(-18, 0, -18)), true);
   ACR_Requisition.s_bTestNoCrew = false;
   s_iRPAfterBuys = usData.m_iRP;
   foreach (ACR_PendingPurchase p : req.m_aPending) if (p.m_sName == s_sNoCrewTruck) s_NoCrewVehicle = p.m_Vehicle;
   PrintFormat("[ACR] REGRESS info: bought %1 and %2 (no crew), pending %3, rp %4", s_sGoodTruck, s_sNoCrewTruck, req.m_aPending.Count(), usData.m_iRP);
  }
  // A fire team to reinforce, at the deploy base.
  s_ReinforceGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(FIRE_TEAM, ACR_Manager.Ground(centre + Vector(6, 0, -6))));
 }

 static void ReinforcePart(ACR_Manager manager)
 {
  ACR_Squad squad = manager.FindSquadByGroup(s_ReinforceGroup);
  if (!squad || !s_Depot) { Print("[ACR] REGRESS info: no fire team to reinforce"); return; }
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int removed;
  for (int i = 1; i < agents.Count(); i++)
  {
   if (agents[i].GetControlledEntity()) { SCR_EntityHelper.DeleteEntityAndChildren(agents[i].GetControlledEntity()); removed++; }
  }
  GetGame().GetCallqueue().CallLater(ReinforceNow, 1500, false, squad, removed);
 }

 static void ReinforceNow(ACR_Squad squad, int removed)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Requisition req = manager.m_Requisition;
  ACR_FactionData usData = manager.m_Bases.GetFaction("US");
  array<ResourceName> missing = {};
  squad.GetMissing(missing);
  int radius = ACR_Requisition.REINFORCE_CLEAR_RADIUS;
  ACR_Requisition.REINFORCE_CLEAR_RADIUS = 0;
  // Soft limit: at the limit, neither buying nor reinforcing adds soldiers.
  int cap = manager.m_iSoldierCap;
  manager.m_iSoldierCap = manager.m_Cleanup.SoldiersOf("US");
  string capReinforce = req.ReinforceSquad(null, squad);
  string capBuy = "no truck entry";
  if (s_iTruckEntry >= 0) capBuy = req.PurchaseAt("US", s_iTruckEntry, s_Depot);
  manager.m_iSoldierCap = cap;
  Check("soldier limit stops buying and reinforcing", capReinforce.Contains("Too many soldiers") && capBuy.Contains("Too many soldiers"), capReinforce + " | " + capBuy);
  usData.m_iRP = 1000;
  int alive = squad.CountAlive();
  ACR_Requisition.s_iTestFailSpawns = 2;
  string said = req.ReinforceSquad(null, squad);
  ACR_Requisition.s_iTestFailSpawns = 0;
  ACR_Requisition.REINFORCE_CLEAR_RADIUS = radius;
  int delivered = missing.Count() - 2;
  int paid = 1000 - usData.m_iRP;
  Check("reinforce charges only for soldiers who arrived", missing.Count() >= 3 && paid == delivered * ACR_Requisition.REINFORCE_COST_PER_SOLDIER && squad.CountAlive() == alive + delivered && said.Contains(string.Format("%1 of %2", delivered, missing.Count())), string.Format("missing %1, paid %2, alive %3 -> %4 | %5", missing.Count(), paid, alive, squad.CountAlive(), said));
 }

 static void PendingResults(ACR_Manager manager)
 {
  ACR_Requisition req = manager.m_Requisition;
  ACR_Requisition.s_fPendingTimeoutMs = 30000;
  if (s_iTruckEntry < 0) return;
  ACR_FactionData usData = manager.m_Bases.GetFaction("US");
  ACR_RequisitionEntry truck = usData.m_aRequisitions[s_iTruckEntry];
  ACR_Squad crew;
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == s_sGoodTruck) crew = squad;
  bool stillPending;
  foreach (ACR_PendingPurchase p : req.m_aPending) if (p.m_sName == s_sGoodTruck || p.m_sName == s_sNoCrewTruck) stillPending = true;
  bool crewNamePending = manager.m_PendingCrewNames.Contains(s_NoCrewVehicle);
  // RP: the delivered truck stays paid, only the failed one's price comes back.
  int refunded = usData.m_iRP - s_iRPAfterBuys;
  Check("pending purchase: delayed crew completes once; a crew that never comes is refunded", refunded == s_iTruckPaid && crew != null && !stillPending && s_NoCrewVehicle == null && !crewNamePending && truck.m_iBought == s_iTruckBought + 1 && truck.m_iNamed == s_iTruckNamed + 2 && truck.m_iCost == ACR_Pricing.PriceAfter(truck.m_iBaseCost, truck.m_iBought), string.Format("refunded %1 of %2 RP, crew %3, pending %4, no-crew truck left %5, bought %6 (was %7), named +%8, cost %9", refunded, s_iTruckPaid, crew != null, stillPending, s_NoCrewVehicle != null, truck.m_iBought, s_iTruckBought, truck.m_iNamed - s_iTruckNamed, truck.m_iCost));
 }
}
