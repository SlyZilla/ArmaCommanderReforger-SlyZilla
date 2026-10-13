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
  CheckUA(tick);
  CheckDMRReplacements(tick);
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
  if (uaPair)
  {
   SpawnUATeams();
   s_iUAExpected = expected + 16;
   return;
  }
  ACR_TestRegress.Complete("factions", expected);
 }

 // UA (11 Oct): every soldier of the US key is re-equipped in UA kit. A few extra teams
 // cover the roles (MG, LAT, GL, sniper) beside the starting squads.
 protected static int s_iUAExpected;
 protected static string s_OtherDMRName = "RU DMR test";
 protected static int s_OtherDMRSize = 6;
 protected static void SpawnUATeams()
 {
  ACR_Manager acr = ACR_Manager.GetInstance();
  ACR_Base home;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!home && base.m_sOwner == "US") home = base;
  if (!home) return;
  array<ResourceName> teams = {
   "{958039B857396B7B}Prefabs/Groups/BLUFOR/Group_US_MachineGunTeam.et",
   "{FAEA8B9E1252F56E}Prefabs/Groups/BLUFOR/Group_US_Team_LAT.et",
   "{DE747BC9217D383C}Prefabs/Groups/BLUFOR/Group_US_Team_GL.et",
   "{D807C7047E818488}Prefabs/Groups/BLUFOR/Group_US_SniperTeam.et"
  };
  acr.m_sSpawningFaction = "US";
  foreach (int i, ResourceName team : teams)
  {
   vector at = acr.ClearSpotNear(home, 20, 80, 4);
   acr.SpawnUnit("UA test " + i, team, "", false, "", at, at);
  }
  // The UA mod's BTR-25 (BTR-70 hull, its own turret part): Jacob 11 Oct, Take over gunner
  // didn't find its gunner.
  vector btrAt = acr.ClearSpotNear(home, 30, 90, 6);
  acr.SpawnUnit("UA BTR test", "", "{A3C0E731AEB7C2BD}Prefabs/Vehicles/Wheeled/BTR70/BTR-25_UA.et", true, "", btrAt, btrAt);
  acr.m_sSpawningFaction = "";
  // Full squads, so the DMR policy is exercised independently of the specialist teams.
  ResourceName uaFull = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
  acr.m_sSpawningFaction = "US";
  vector uaAt = acr.ClearSpotNear(home, 30, 90, 4);
  acr.SpawnUnit("UA DMR test", uaFull, "", false, "", uaAt, uaAt);
  string russian = ACR_FactionSelection.West();
  if (russian == "US") russian = ACR_FactionSelection.East();
  if (russian == "RHS_USAF") { s_OtherDMRName = "USMC DMR test"; s_OtherDMRSize = 9; }
  ACR_Base ruHome;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (!ruHome && candidate.m_sOwner == russian) ruHome = candidate;
  if (ruHome)
  {
   acr.m_sSpawningFaction = russian;
   ResourceName ruFull = ACR_FactionCatalog.ResolvePrefab(russian, "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et", EEntityCatalogType.GROUP);
   vector ruAt = acr.ClearSpotNear(ruHome, 30, 90, 4);
   acr.SpawnUnit(s_OtherDMRName, ruFull, "", false, "", ruAt, ruAt);
  }
  acr.m_sSpawningFaction = "";
 }

 static void CheckUA(int tick)
 {
  if (s_iUAExpected == 0 || tick != 45) return;
  int soldiers, equipped, dressed, armed, supplied, east, west, oldKit, launchers, aviators, kitted, emptyHands, onFootArmed;
  string kitShort;
  string tally;
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (!ChimeraCharacter.Cast(man) || !ACR_UALoadout.IsUA(man)) continue;
   CharacterControllerComponent controller = ChimeraCharacter.Cast(man).GetCharacterController();
   if (!controller || controller.IsDead()) continue;
   soldiers++;
   if (ACR_UALoadout.WasEquipped(man)) equipped++;
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
   SCR_CharacterInventoryStorageComponent loadout = inventory.GetCharacterStorage();
   string leaf = ACR_UALoadout.Leaf(man);
   IEntity jacket = loadout.GetClothFromArea(LoadoutJacketArea);
   IEntity pants = loadout.GetClothFromArea(LoadoutPantsArea);
   string helmetLeaf = ACR_UALoadout.Leaf(loadout.GetClothFromArea(LoadoutHeadCoverArea));
   helmetLeaf.ToLower();
   if (!leaf.Contains("Pilot") && (helmetLeaf.Contains("gentex") || helmetLeaf.Contains("hgu") || helmetLeaf.Contains("sph4") || helmetLeaf.Contains("zsh5"))) aviators++;
   if (leaf.Contains("HeliPilot") || (jacket && pants && jacket.GetPrefabData().GetPrefabName() == ACR_UALoadout.JACKET && pants.GetPrefabData().GetPrefabName() == ACR_UALoadout.PANTS)) dressed++;
   else PrintFormat("[ACR] FACTIONTEST UA undressed %1 jacket=%2 pants=%3", leaf, ACR_UALoadout.Leaf(jacket), ACR_UALoadout.Leaf(pants));
   string shortHere;
   foreach (string kitLine : ACR_UALoadout.KIT)
   {
    array<string> kitParts = {};
    kitLine.Split("|", kitParts, false);
    if (kitParts.Count() == 3 && ACR_UALoadout.CountKind(man, inventory, kitParts[0]) < kitParts[2].ToInt()) shortHere += " " + kitParts[0];
   }
   if (shortHere.IsEmpty() || leaf.Contains("HeliPilot")) kitted++;
   else kitShort += " " + leaf + ":" + shortHere;
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
   array<IEntity> list = {};
   if (weapons) weapons.GetWeaponsList(list);
   IEntity primary;
   foreach (IEntity weaponEntity : list)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
    if (!weapon) continue;
    string name = ACR_UALoadout.Leaf(weaponEntity);
    if (name.Contains("M60")) oldKit++;
    if (weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) launchers++;
    else if (!primary && weapon.GetWeaponType() != EWeaponType.WT_HANDGUN && weapon.GetWeaponType() != EWeaponType.WT_FRAGGRENADE && weapon.GetWeaponType() != EWeaponType.WT_SMOKEGRENADE) primary = weaponEntity;
   }
   if (!primary) continue;
   armed++;
   if (!controller.IsPlayerControlled() && !controller.IsUnconscious() && !ChimeraCharacter.Cast(man).IsInVehicle())
   {
    onFootArmed++;
    if (!controller.GetCurrentItemInHands() && !controller.IsChangingItem())
    {
     emptyHands++;
     PrintFormat("[ACR] FACTIONTEST UA EMPTY HANDS %1 carried=%2 current=%3", leaf, ACR_UALoadout.Leaf(primary), weapons.GetCurrentWeapon() != null);
    }
   }
   string primaryLeaf = ACR_UALoadout.Leaf(primary);
   tally += " " + primaryLeaf;
   if (primaryLeaf.Contains("AK") || primaryLeaf.Contains("PK") || primaryLeaf.Contains("RPK") || primaryLeaf.Contains("SVD")) east++;
   else west++;
   BaseWeaponComponent primaryWeapon = BaseWeaponComponent.Cast(primary.FindComponent(BaseWeaponComponent));
   array<BaseMuzzleComponent> muzzles = {};
   primaryWeapon.GetMuzzlesList(muzzles);
   if (!muzzles.IsEmpty() && ACR_UALoadout.CountPrefab(inventory, muzzles[0].GetDefaultMagazineOrProjectileName()) >= 2) supplied++;
   else PrintFormat("[ACR] FACTIONTEST UA short of magazines %1 %2", leaf, primaryLeaf);
  }
  PrintFormat("[ACR] FACTIONTEST UA weapons east=%1 west=%2:%3", east, west, tally);
  // BTR-25 gunner: what Take over gunner would find.
  string btrReport = "no BTR-25";
  ChimeraCharacter btrGunner;
  array<IEntity> allVehicles = {};
  ACR_Vehicles.All(allVehicles);
  foreach (IEntity btr : allVehicles)
  {
   if (!btr || !btr.GetPrefabData() || !ACR_UALoadout.Leaf(btr).Contains("BTR-25")) continue;
   btrReport = ACR_GunnerDrive.DescribeTurrets(btr);
   foreach (ACR_Squad crewSquad : ACR_Manager.GetInstance().m_Squads)
   {
    array<IEntity> crewVehicles = {};
    crewSquad.GetVehicles(crewVehicles);
    foreach (IEntity crewVehicle : crewVehicles)
     if (crewVehicle == btr || crewVehicle.GetRootParent() == btr)
     {
      btrReport += " | squad " + crewSquad.GetName() + " vehicle " + ACR_UALoadout.Leaf(crewVehicle);
      ChimeraCharacter found = ACR_GunnerDrive.SquadGunner(crewSquad);
      if (found) btrGunner = found;
     }
   }
  }
  PrintFormat("[ACR] FACTIONTEST UA BTR-25: %1", btrReport);
  ACR_TestRegress.Check("factions: every UA soldier was re-equipped without a failure", soldiers >= 10 && equipped == soldiers && ACR_UALoadout.s_iFailed == 0, string.Format("%1/%2, %3 failed", equipped, soldiers, ACR_UALoadout.s_iFailed));
  ACR_TestRegress.Check("factions: UA soldiers wear the UA jacket and pants", dressed == soldiers, string.Format("%1/%2", dressed, soldiers));
  ACR_TestRegress.Check("factions: UA rifles mostly Soviet pattern, no M60s left", armed > 0 && east * 2 > armed && oldKit == 0, string.Format("east %1 west %2 of %3, M60 %4", east, west, armed, oldKit));
  ACR_TestRegress.Check("factions: UA soldiers carry spare magazines for their weapon", supplied == armed, string.Format("%1/%2", supplied, armed));
  ACR_TestRegress.Check("factions: no UA infantry wear aviator (Gentex) helmets", aviators == 0, string.Format("%1 aviator helmets", aviators));
  ACR_TestRegress.Check("factions: UA soldiers carry map, compass, watch, radio, meds, shovel and grenades", kitted == soldiers && ACR_UALoadout.s_iKitFailed == 0, string.Format("%1/%2, %3 short%4", kitted, soldiers, ACR_UALoadout.s_iKitFailed, kitShort));
  ACR_TestRegress.Check("factions: Take over gunner finds the UA BTR-25's gunner", btrGunner != null, btrReport);
  ACR_TestRegress.Check("factions: the UA LAT team carries launchers", launchers >= 1, string.Format("%1 launchers", launchers));
  ACR_TestRegress.Check("factions: armed UA infantry are not left empty-handed after the kit swap", onFootArmed > 0 && emptyHands == 0, string.Format("%1 empty of %2 on-foot armed soldiers", emptyHands, onFootArmed));
  CheckFullDMR("UA DMR test", 9);
  CheckFullDMR(s_OtherDMRName, s_OtherDMRSize);
  ACR_TestRegress.Check("factions: both full rifle squads keep machine guns, grenadiers and launchers", FullSupports("UA DMR test", 2, 2, 2) && FullSupports(s_OtherDMRName, 1, 1, 1), "DMR replaces a rifle, not a support weapon");
  BeginDMRReplacement("UA DMR test");
  BeginDMRReplacement(s_OtherDMRName);
 }

 protected static ref array<string> s_DMRNames = {};
 protected static ref array<ResourceName> s_DMRMissing = {};
 protected static ref array<vector> s_DMRLocations = {};

 static void BeginDMRReplacement(string name)
 {
  ACR_Squad squad = FullSquad(name);
  IEntity target;
  ResourceName original;
  array<AIAgent> members = {};
  if (squad) squad.GetAgents(members);
  foreach (AIAgent agent : members)
  {
   IEntity man = agent.GetControlledEntity();
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
   array<IEntity> carried = {};
   if (weapons) weapons.GetWeaponsList(carried);
   foreach (IEntity item : carried)
    if (ACR_SquadMarksmen.IsDMR(item)) target = man;
   if (target) break;
  }
  if (target)
  {
   original = target.GetPrefabData().GetPrefabName();
   s_DMRLocations.Insert(target.GetOrigin() + Vector(2, 0, 0));
   SCR_EntityHelper.DeleteEntityAndChildren(target);
  }
  else s_DMRLocations.Insert(vector.Zero);
  s_DMRNames.Insert(name);
  s_DMRMissing.Insert(original);
 }

 static void CheckDMRReplacements(int tick)
 {
  if (s_DMRNames.Count() != 2) return;
  if (tick == 48)
  {
   foreach (int i, string name : s_DMRNames)
   {
    ACR_Squad squad = FullSquad(name);
    array<ResourceName> missing = {};
    if (squad) squad.GetMissing(missing);
    bool correct = missing.Count() == 1 && missing[0] == s_DMRMissing[i] && !s_DMRMissing[i].IsEmpty();
    ACR_TestRegress.Check("factions: " + name + " retains its original reinforcement recipe", correct, string.Format("%1 missing", missing.Count()));
    if (!correct) continue;
    Resource resource = Resource.Load(missing[0]);
    EntitySpawnParams parameters = new EntitySpawnParams();
    parameters.TransformMode = ETransformMode.WORLD;
    parameters.Transform[3] = ACR_Manager.Ground(s_DMRLocations[i]);
    IEntity replacement = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
    if (replacement) squad.m_Group.AddAIEntityToGroup(replacement);
   }
  }
  if (tick == 65)
  {
   CheckFullDMR("UA DMR test", 9);
   CheckFullDMR(s_OtherDMRName, s_OtherDMRSize);
   ACR_TestRegress.Complete("factions", s_iUAExpected);
  }
 }

 static ACR_Squad FullSquad(string name)
 {
  foreach (ACR_Squad squad : ACR_Manager.GetInstance().m_Squads)
   if (squad.GetName() == name) return squad;
  return null;
 }

 static void CheckFullDMR(string name, int size)
 {
  ACR_Squad squad = FullSquad(name);
  int marksmen;
  bool magazines;
  string detail;
  if (squad)
  {
   array<AIAgent> members = {};
   squad.GetAgents(members);
   foreach (AIAgent agent : members)
   {
    IEntity man = agent.GetControlledEntity();
    BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
    array<IEntity> carried = {};
    weapons.GetWeaponsList(carried);
    foreach (IEntity gun : carried)
    {
     if (!ACR_SquadMarksmen.IsDMR(gun)) continue;
     marksmen++;
     detail += " " + ACR_UALoadout.Leaf(gun);
     BaseWeaponComponent weapon = BaseWeaponComponent.Cast(gun.FindComponent(BaseWeaponComponent));
     array<BaseMuzzleComponent> muzzles = {};
     weapon.GetMuzzlesList(muzzles);
     SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
     if (!muzzles.IsEmpty() && weapon.GetCurrentMagazine() && weapon.GetCurrentMagazine().GetAmmoCount() > 0 && ACR_UALoadout.CountPrefab(inventory, muzzles[0].GetDefaultMagazineOrProjectileName()) >= 2) magazines = true;
    }
   }
   detail += string.Format(" size=%1 authored=%2", squad.CountAlive(), squad.m_Group.m_aUnitPrefabSlots.Count());
  }
  ACR_TestRegress.Check("factions: " + name + " has one loaded DMR and unchanged strength", squad && squad.CountAlive() == size && marksmen == 1 && magazines, string.Format("DMRs=%1 supplied=%2%3", marksmen, magazines, detail));
 }

 static bool FullSupports(string name, int mgNeed, int glNeed, int atNeed)
 {
  ACR_Squad squad = FullSquad(name);
  if (!squad) return false;
  int mg, gl, at;
  array<AIAgent> members = {};
  squad.GetAgents(members);
  foreach (AIAgent agent : members)
  {
   IEntity man = agent.GetControlledEntity();
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
   array<IEntity> carried = {};
   weapons.GetWeaponsList(carried);
   foreach (IEntity item : carried)
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
    if (!weapon) continue;
    if (weapon.GetWeaponType() == EWeaponType.WT_MACHINEGUN) mg++;
    if (weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) at++;
    array<BaseMuzzleComponent> muzzles = {};
    weapon.GetMuzzlesList(muzzles);
    foreach (BaseMuzzleComponent muzzle : muzzles)
     if (muzzle.GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) gl++;
   }
  }
  PrintFormat("[ACR] FACTIONTEST DMR SUPPORT %1 MG=%2 GL=%3 AT=%4", name, mg, gl, at);
  return mg >= mgNeed && gl >= glNeed && at >= atNeed;
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
