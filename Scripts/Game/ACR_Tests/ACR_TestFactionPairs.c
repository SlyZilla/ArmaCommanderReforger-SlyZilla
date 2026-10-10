// -acrFactionTest 1 (server-capable): verify the selected pair, faction manager,
// bases and faction-specific buy-list resources on a modded test-range boot.
class ACR_TestFactionPairs
{
 static int CheckCatalog(string factionKey)
 {
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  array<ref ACR_RequisitionEntry> entries = {};
  ACR_FactionCatalog.BuildRequisitions(factionKey, settings, entries);
  int checked;
  int invalid;
  foreach (ACR_RequisitionEntry entry : entries)
  {
   ResourceName group = ACR_FactionCatalog.ResolvePrefab(factionKey, entry.m_sGroup, EEntityCatalogType.GROUP);
   ResourceName vehicle = ACR_FactionCatalog.ResolvePrefab(factionKey, entry.m_sVehicle, EEntityCatalogType.VEHICLE);
   if (!group.IsEmpty())
   {
    Resource resource = Resource.Load(group);
    checked++;
    if (!resource || !resource.IsValid()) invalid++;
   }
   if (!vehicle.IsEmpty())
   {
    Resource resource = Resource.Load(vehicle);
    checked++;
    if (!resource || !resource.IsValid()) invalid++;
   }
  }
  PrintFormat("[ACR] FACTIONTEST catalog %1: %2 resources checked, %3 unavailable", factionKey, checked, invalid);
  return invalid;
 }

 protected static int s_iExpected;
 protected static ref array<IEntity> s_ArsenalTrucks = {};

 // RHS sides: one subunit and camouflage per side (Jacob, 8 Oct live test: MARSOC, desert
 // and woodland mixed; officers and vehicle crews in vanilla 80s kit).
 static int CheckStandard(string factionKey)
 {
  string standard = ACR_FactionCatalog.StandardPath(factionKey);
  if (standard.IsEmpty()) return 0;
  ACR_FactionData data = ACR_Manager.GetInstance().m_Bases.GetFaction(factionKey);
  int offPool;
  foreach (ResourceName soldier : data.m_aGarrison)
  {
   string soldierPath = soldier;
   if (!soldierPath.Contains(standard)) { offPool++; PrintFormat("[ACR] FACTIONTEST %1 garrison off-standard: %2", factionKey, soldier); }
  }
  ACR_TestRegress.Check("factions: " + factionKey + " garrisons use only the standard subunit", !data.m_aGarrison.IsEmpty() && offPool == 0, string.Format("%1 of %2 off %3", offPool, data.m_aGarrison.Count(), standard));
  int offList;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   // Support entries have their own known fallbacks (RHS has no mortar).
   if (entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli > 0) continue;
   bool ok = true;
   if (!entry.m_sGroup.IsEmpty() && !ACR_FactionCatalog.IsStandard(factionKey, entry.m_sGroup, EEntityCatalogType.GROUP)) ok = false;
   if (!entry.m_sVehicle.IsEmpty() && !ACR_FactionCatalog.IsStandard(factionKey, entry.m_sVehicle, EEntityCatalogType.VEHICLE)) ok = false;
   PrintFormat("[ACR] FACTIONTEST %1 buy: %2 | %3 | %4%5", factionKey, entry.m_sName, FilePath.StripPath(entry.m_sGroup), FilePath.StripPath(entry.m_sVehicle), ok);
   if (!ok) offList++;
  }
  ACR_TestRegress.Check("factions: " + factionKey + " buy list uses the standard subunit and paint", offList == 0, string.Format("%1 off-standard entries", offList));
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  array<ref SCR_BasePlayerLoadout> loadouts = {};
  GetGame().GetLoadoutManager().GetPlayerLoadoutsByFaction(faction, loadouts);
  string officer;
  if (!loadouts.IsEmpty()) officer = loadouts[0].GetLoadoutResource();
  ACR_TestRegress.Check("factions: " + factionKey + " command unit is its own officer", loadouts.Count() == 1 && officer.Contains(standard), string.Format("%1 loadouts, %2", loadouts.Count(), FilePath.StripPath(officer)));
  return 3;
 }

 static string Leaf(ResourceName prefab) { return FilePath.StripExtension(FilePath.StripPath(prefab)); }

 static void Run(int tick)
 {
  CheckFlyover(tick);
  if (tick == 70) { FinishCrews(); return; }
  if (tick != 10) return;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  FactionManager manager = GetGame().GetFactionManager();
  int playable;
  array<Faction> all = {};
  manager.GetFactionsList(all);
  foreach (Faction baseFaction : all)
  {
   SCR_Faction faction = SCR_Faction.Cast(baseFaction);
   if (faction && faction.IsPlayable()) playable++;
  }
  ACR_TestRegress.Check("factions: two selected sides are playable", playable == 2, string.Format("%1 / %2, playable=%3", west, east, playable));
  ACR_TestRegress.Check("factions: both selected faction keys exist", manager.GetFactionByKey(west) && manager.GetFactionByKey(east) && west != east, string.Format("%1 / %2", west, east));
  int offSide;
  foreach (ACR_Squad squad : ACR_Manager.GetInstance().m_Squads)
   if (squad.GetFaction() && !ACR_FactionSelection.IsSelected(squad.GetFaction().GetFactionKey())) offSide++;
  ACR_TestRegress.Check("factions: spawned squads use only the selected pair", offSide == 0, string.Format("%1 squads on an unselected faction", offSide));
  bool westBase;
  bool eastBase;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == west) westBase = true;
   if (base.m_sOwner == east) eastBase = true;
  }
  ACR_TestRegress.Check("factions: authored base owners map onto both selected sides", westBase && eastBase, string.Format("%1=%2 %3=%4", west, westBase, east, eastBase));
  ACR_FactionData westData = ACR_Manager.GetInstance().m_Bases.GetFaction(west);
  ACR_FactionData eastData = ACR_Manager.GetInstance().m_Bases.GetFaction(east);
  bool supportEntries = westData != null && eastData != null;
  bool westMortar, westObserver, eastMortar, eastObserver;
  if (supportEntries)
  {
   foreach (ACR_RequisitionEntry entry : westData.m_aRequisitions) { westMortar = westMortar || entry.m_bMortarTeam; westObserver = westObserver || entry.m_bObserverTeam; }
   foreach (ACR_RequisitionEntry entry : eastData.m_aRequisitions) { eastMortar = eastMortar || entry.m_bMortarTeam; eastObserver = eastObserver || entry.m_bObserverTeam; }
  }
  ACR_TestRegress.Check("factions: both buy lists include mortar and observer teams", westMortar && westObserver && eastMortar && eastObserver, "mortar and forward observer entries present");
  int invalid = CheckCatalog(west) + CheckCatalog(east);
  ACR_TestRegress.Check("factions: every catalog buy-list prefab resolves", invalid == 0, string.Format("%1 unavailable resources", invalid));
  bool uaPair;
  string selectedPair;
  System.GetCLIParam("acrFactions", selectedPair);
  selectedPair.ToLower();
  uaPair = selectedPair == "ru_ua" || selectedPair == "us_ua";
  if (uaPair) ACR_TestRegress.Check("factions: UAF resource is detected and labeled UA", ACR_FactionSelection.UkraineModLoaded() && ACR_FactionSelection.DisplayName("US") == "UA", ACR_FactionSelection.DisplayName("US"));
  int expected = 6;
  if (uaPair) expected = 7;
  expected += CheckStandard(west) + CheckStandard(east);
  if (selectedPair == "ru_us")
  {
   string swaps = string.Format("%1 %2 %3 %4",
    Leaf(ACR_FactionCatalog.ResolvePrefab(west, "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et", EEntityCatalogType.GROUP)),
    Leaf(ACR_FactionCatalog.ResolvePrefab(east, "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", EEntityCatalogType.GROUP)),
    Leaf(ACR_FactionCatalog.ResolvePrefab(west, "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et", EEntityCatalogType.VEHICLE)),
    Leaf(ACR_FactionCatalog.ResolvePrefab(east, "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et", EEntityCatalogType.VEHICLE)));
   ACR_TestRegress.Check("factions: starting units swap to the standard subunit and paint", swaps == "Group_USAF_USMC_MEF_RifleSquad Group_RHS_RF_MSV_VKPO_S_RifleSquad M1025_USAF UAZ469_Camo", swaps);
   SCR_AutoSpawnLogic autoSpawn = new SCR_AutoSpawnLogic();
   autoSpawn.ACR_SetForcedFaction("US");
   Faction forced = autoSpawn.ACR_ForcedFaction();
   string forcedKey = "none";
   if (forced) forcedKey = forced.GetFactionKey();
   ACR_TestRegress.Check("factions: the scenario's forced US side joins the selected west side", forcedKey == west, forcedKey);
   expected += 2;
   // Crewed vehicles of each side (the RHS catalogs reuse vanilla vehicles with vanilla crews).
   ACR_Manager acr = ACR_Manager.GetInstance();
   ACR_Base homeWest;
   ACR_Base homeEast;
   foreach (ACR_Base home : ACR_Base.s_All)
   {
    if (!homeWest && home.m_sOwner == west) homeWest = home;
    if (!homeEast && home.m_sOwner == east) homeEast = home;
   }
   vector at = acr.ClearSpotNear(homeWest, 15, 60, 4);
   acr.m_sSpawningFaction = west;
   acr.SpawnUnit("Crew test west", "", "{3277F8B1FA06A2BC}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB_USAF.et", true, "", at, at);
   vector at2 = acr.ClearSpotNear(homeEast, 15, 60, 4);
   acr.m_sSpawningFaction = east;
   acr.SpawnUnit("Crew test east", "", "{0B4DEA8078B78A9B}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et", true, "", at2, at2);
   acr.m_sSpawningFaction = "";
   array<string> arsenalBuyers = {west, east};
   foreach (string buyingSide : arsenalBuyers)
   {
    vector arsenalAt = at;
    if (buyingSide == east) arsenalAt = at2;
    IEntity arsenalTruck = ACR_Manager.SpawnPrefab("{1FF144F8A209239D}Prefabs/Vehicles/Wheeled/M923A1/M923A1_arsenal.et", arsenalAt + Vector(0, 0, 30));
    ACR_Vehicles.SetOwner(arsenalTruck, buyingSide);
    s_ArsenalTrucks.Insert(arsenalTruck);
   }
   // Bomb run: both RHS sides fly the FA-18 (Jacob, 9 Oct). Call one on the east side's
   // own base area (far from the crew cars) and fly a local copy here as a client would.
   ACR_FactionData eastData2 = acr.m_Bases.GetFaction(east);
   int bombIndex = -1;
   bool flyovers = true;
   array<ACR_FactionData> sides = {};
   sides.Insert(westData);
   sides.Insert(eastData2);
   foreach (ACR_FactionData sideData : sides)
   {
    foreach (int i, ACR_SupportEntry support : sideData.m_aSupports)
     if (support.m_eType == ACR_ESupportType.BOMB_RUN)
     {
      if (support.m_eBombRunDelivery != ACR_EBombRunDelivery.FLYOVER || support.m_sBombRunPrefab != ACR_Supports.FA18_FLYOVER) flyovers = false;
      if (sideData == westData) bombIndex = i;
     }
   }
   ACR_TestRegress.Check("factions: both RHS sides' bomb run is the FA-18 flyover", flyovers && bombIndex >= 0, string.Format("index %1", bombIndex));
   acr.m_Bases.GetFaction(west).m_iRP += 500;
   s_vBombTarget = ACR_Manager.Ground(homeEast.GetOrigin() + "250 0 250");
   ACR_Supports.s_iExploded = 0;
   if (!acr.m_Supports) acr.m_Supports = new ACR_Supports(acr);
   string reply = "no bomb run";
   if (bombIndex >= 0) reply = acr.m_Supports.Call(west, bombIndex, s_vBombTarget, true);
   PrintFormat("[ACR] FACTIONTEST bomb run: %1", reply);
   vector flyStart = ACR_Manager.Ground(homeWest.GetOrigin()) + "0 300 0";
   ACR_LocalFlyover.Start(ACR_Supports.FA18_FLYOVER, flyStart, "1 0 0", 180, flyStart[1], 6);
   s_vFlyStart = flyStart;
   s_iExpected = expected + 7;
   return;
  }
  ACR_TestRegress.Complete("factions", expected);
 }

 protected static vector s_vBombTarget;
 protected static vector s_vFlyStart;

 static void CheckFlyover(int tick)
 {
  if (s_iExpected == 0 || tick != 12) return;
  ACR_LocalFlyover flyover = ACR_LocalFlyover.Latest();
  float moved = -1;
  if (flyover && flyover.m_Plane) moved = vector.DistanceXZ(flyover.m_Plane.GetOrigin(), s_vFlyStart);
  ACR_TestRegress.Check("factions: a local FA-18 flies its path", moved > 180 && moved < 600, string.Format("%1 m in about 2 s", Math.Round(moved)));
 }

 protected static void FinishCrews()
 {
  if (s_iExpected == 0) return;
  array<string> buyers = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
  foreach (int index, string buyer : buyers)
  {
   IEntity truck;
   if (index < s_ArsenalTrucks.Count()) truck = s_ArsenalTrucks[index];
   SCR_ArsenalComponent arsenal = ACR_Vehicles.ArsenalOf(truck);
   bool correct;
   array<SCR_ArsenalItem> items = {};
   if (arsenal)
   {
    FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(truck.FindComponent(FactionAffiliationComponent));
    if (affiliation) affiliation.SetAffiliatedFaction(null);
    correct = arsenal.GetAssignedFaction() && arsenal.GetAssignedFaction().GetFactionKey() == buyer && arsenal.GetFilteredArsenalItems(items) && !items.IsEmpty();
   }
   ACR_TestRegress.Check("factions: empty purchased arsenal retains " + buyer + " catalog", correct, string.Format("%1 filtered items", items.Count()));
  }
  ACR_TestRegress.Check("factions: the local FA-18 is removed after its flight", ACR_LocalFlyover.Latest() == null, "");
  ACR_TestRegress.Check("factions: the bomb run's four bombs went off", ACR_Supports.s_iExploded == 4, string.Format("%1 bombs at %2", ACR_Supports.s_iExploded, s_vBombTarget));
  int seated, own;
  string report;
  array<IEntity> cars = {};
  cars.Insert(FindCar(ACR_FactionSelection.West(), "M1025_armed_M2HB_USAF"));
  cars.Insert(FindCar(ACR_FactionSelection.East(), "UAZ469_PKM"));
  foreach (IEntity vehicle : cars)
  {
   if (!vehicle) { report += " missing"; continue; }
   string standard = ACR_FactionCatalog.StandardPath(ACR_Vehicles.BoughtBy(vehicle));
   array<IEntity> crew = {};
   ACR_PendingPurchase.Occupants(vehicle, crew);
   foreach (IEntity man : crew)
   {
    seated++;
    string path = man.GetPrefabData().GetPrefabName();
    if (!standard.IsEmpty() && path.Contains(standard)) own++;
    report += " " + Leaf(path);
   }
  }
  ACR_TestRegress.Check("factions: crewed vehicles seat the side's own crewmen", seated >= 4 && own == seated, string.Format("%1/%2:%3", own, seated, report));
  ACR_TestRegress.Complete("factions", s_iExpected);
 }

 protected static IEntity FindCar(string factionKey, string leaf)
 {
  array<IEntity> all = {};
  ACR_Vehicles.All(all);
  foreach (IEntity vehicle : all)
   if (ACR_Vehicles.BoughtBy(vehicle) == factionKey && vehicle.GetPrefabData() && Leaf(vehicle.GetPrefabData().GetPrefabName()) == leaf) return vehicle;
  return null;
 }
}
