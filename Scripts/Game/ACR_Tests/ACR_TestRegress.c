// -acrRegressTest 1 (server-capable): regression checks for bugs fixed after
// the 1 Oct review. Each check prints "[ACR] REGRESS PASS|FAIL <name>".
class ACR_TestRegress
{
 static void Check(string name, bool ok, string detail = "")
 {
  string verdict = "FAIL";
  if (ok) verdict = "PASS";
  PrintFormat("[ACR] REGRESS %1 %2 %3", verdict, name, detail);
 }

 static string Verdict(bool ok)
 {
  if (ok) return "PASS";
  return "FAIL";
 }

 static ACR_Squad s_Defender;
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
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() != "Shovel Test") continue;
    IEntity shovelTruck = ACR_Transport.CrewedVehicle(squad);
    ChimeraCharacter driver = ACR_Squad.DriverOf(shovelTruck);
    shovel = driver && ACR_Manager.HasShovel(driver);
   }
   Check("construction truck driver has a shovel", shovel, ACR_Requisition.s_sLastShovel);
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
   if (attack.Contains("no free squads")) PrintFormat("[ACR] REGRESS info: forced attack skipped (%1)", attack);
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
   if (sovietSoldiers == 0) Print("[ACR] REGRESS info: reveal check skipped (no Soviet squads)");
   else Check("debug: reveal shows enemy soldiers", sovietsSeen > 0 && !manager.m_Debug.Reveals("US"), string.Format("%1 seen of about %2", sovietsSeen, sovietSoldiers));
  }
  // Vehicle crews carry their unit's name even when they register before they're seated
  // (2 Oct live: Truck 1 and Humvee 1 showed as "Squad 1" and "Squad 2").
  if (tick == 40)
  {
   manager.m_sSpawningFaction = "US";
   manager.SpawnUnit("Regress Truck", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", harness.Ground("1500 0 2950"), harness.Ground("1506 0 2956"));
  }
  if (tick == 48)
  {
   ACR_Squad crew;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "Regress Truck") crew = squad;
   Check("vehicle crew keeps its unit name", crew != null && crew.m_OwnVehicle != null, string.Format("found %1", crew != null));
   if (crew) Check("idle truck crew can give rides", manager.m_Rides && manager.m_Rides.WhyNotTaxi(crew, "US").IsEmpty(), manager.m_Rides.WhyNotTaxi(crew, "US"));
  }
  if (tick == 50)
  {
   bool kept = s_Defender && s_Defender.m_iAITask == ACR_EAITask.DEFEND;
   Check("AI commander leaves the other side's defenders alone", kept, "");
   PrintFormat("[ACR] REGRESS invariant errors: %1", ACR_Perf.s_iInvariantErrors);
   Print("[ACR] TEST COMPLETE");
  }
 }
}
