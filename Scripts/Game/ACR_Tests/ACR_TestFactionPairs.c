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

 static void Run(int tick)
 {
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
  ACR_TestRegress.Complete("factions", expected);
 }
}
