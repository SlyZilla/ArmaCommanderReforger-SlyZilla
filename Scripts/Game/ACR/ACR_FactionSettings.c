// One buyable unit. Mission makers add these in the faction settings entity's
// Requisition list (with the + button and the prefab browser), or leave the
// list empty and let it fill from the faction's Game Master catalog.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_RequisitionEntry
{
 [Attribute("", UIWidgets.EditBox, "Name shown in the buy list and on the map (numbered per purchase)")]
 string m_sName;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Infantry group to spawn", "et")]
 ResourceName m_sGroup;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle to spawn (optional)", "et")]
 ResourceName m_sVehicle;
 [Attribute("1", UIWidgets.CheckBox, "Spawn the vehicle with its driver and gunners")]
 bool m_bCrewed;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Group that rides in the vehicle (optional)", "et")]
 ResourceName m_sPassengers;
 [Attribute("20", UIWidgets.EditBox, "Cost in requisition points")]
 int m_iCost;
 [Attribute("2", UIWidgets.EditBox, "How many can be bought per match")]
 int m_iStock;
 bool m_bAircraft;   // set automatically; the AI commander never buys these (its AI cannot fly)
 bool m_bTransport;  // set automatically: an unarmed truck, which the AI commander buys to ferry squads
 bool m_bArmed = true; // set automatically for vehicles: has a weapon (the AI doesn't buy unarmed cars to fight with)

 // runtime
 int m_iRemaining;
 int m_iBought;

 void Set(string name, ResourceName group, ResourceName vehicle, int cost, int stock)
 {
  m_sName = name;
  m_sGroup = group;
  m_sVehicle = vehicle;
  m_bCrewed = true;
  m_iCost = cost;
  m_iStock = stock;
 }
}

// Per-faction Arma Commander setup, placed in World Editor: starting RP, what
// the commander can buy, and which soldiers garrison bases.
class ACR_FactionSettingsClass : GenericEntityClass {}
class ACR_FactionSettings : GenericEntity
{
 [Attribute("US", UIWidgets.EditBox, "Faction key this applies to (e.g. US, USSR, FIA)")]
 string m_sFaction;
 [Attribute("100", UIWidgets.EditBox, "Requisition points at the start")]
 int m_iStartingRP;
 [Attribute("1", UIWidgets.CheckBox, "An AI commander leads this faction whenever no player is in command")]
 bool m_bAICommander;
 [Attribute("8", UIWidgets.EditBox, "AI commander: most squads it keeps in the field")]
 int m_iAIUnitCap;
 [Attribute("1", UIWidgets.EditBox, "AI commander: how many base attacks it runs at once")]
 int m_iAIAttacks;
 [Attribute("1", UIWidgets.CheckBox, "Add every group and vehicle from the faction's Game Master catalog with automatic prices. Entries below override or add to it.")]
 bool m_bAutoFill;
 [Attribute(desc: "Hand-picked units. With auto fill on, an entry using the same prefab replaces the automatic one.")]
 ref array<ref ACR_RequisitionEntry> m_aRequisitions;
 [Attribute(desc: "Automatic entries to leave out (group or vehicle prefabs)", params: "et")]
 ref array<ResourceName> m_aExclude;
 [Attribute(desc: "Soldiers used for base garrisons. Empty = the faction's riflemen, gunners and AT soldiers from its catalog.", params: "et")]
 ref array<ResourceName> m_aGarrisonSoldiers;

 [Attribute("1", UIWidgets.CheckBox, "The commander can call off-map supports (artillery, air, helicopters)")]
 bool m_bSupports;
 [Attribute(desc: "Supports on offer. Empty = all of them with default prices and cooldowns.")]
 ref array<ref ACR_SupportEntry> m_aSupports;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Gunship for helicopter supports (empty = UH-1H for western factions, Mi-8 otherwise)", "et")]
 ResourceName m_sSupportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Transport helicopter for the door-gunner support (empty = armed UH-1H for western factions, armed Mi-8 otherwise)", "et")]
 ResourceName m_sSupportTransportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Rocket for rocket strikes and gunships (empty = Hydra 70 / S-5)", "et")]
 ResourceName m_sSupportRocket;

 static ref array<ACR_FactionSettings> s_All = {};

 void ACR_FactionSettings(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }
 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_All.Insert(this);
 }
 void ~ACR_FactionSettings()
 {
  if (s_All) s_All.RemoveItem(this);
 }

 static ACR_FactionSettings Find(string factionKey)
 {
  foreach (ACR_FactionSettings settings : s_All)
   if (settings.m_sFaction == factionKey) return settings;
  return null;
 }
}

// Builds the runtime buy list and garrison pool for a faction.
class ACR_FactionCatalog
{
 static void BuildRequisitions(string factionKey, ACR_FactionSettings settings, array<ref ACR_RequisitionEntry> result)
 {
  result.Clear();
  array<ResourceName> used = {};
  if (settings && settings.m_aRequisitions)
  {
   foreach (ACR_RequisitionEntry manual : settings.m_aRequisitions)
   {
    if (manual.m_sGroup.IsEmpty() && manual.m_sVehicle.IsEmpty()) continue;
    ACR_RequisitionEntry copy = new ACR_RequisitionEntry();
    copy.Set(manual.m_sName, manual.m_sGroup, manual.m_sVehicle, manual.m_iCost, manual.m_iStock);
    copy.m_bCrewed = manual.m_bCrewed;
    copy.m_sPassengers = manual.m_sPassengers;
    if (copy.m_sName.IsEmpty()) copy.m_sName = "Unit";
    result.Insert(copy);
    if (!manual.m_sGroup.IsEmpty()) used.Insert(manual.m_sGroup);
    if (!manual.m_sVehicle.IsEmpty()) used.Insert(manual.m_sVehicle);
   }
  }
  if (!settings || settings.m_bAutoFill)
  {
   array<ResourceName> exclude = {};
   if (settings && settings.m_aExclude) exclude.Copy(settings.m_aExclude);
   AddCatalog(factionKey, EEntityCatalogType.GROUP, used, exclude, result);
   AddCatalog(factionKey, EEntityCatalogType.VEHICLE, used, exclude, result);
  }
  foreach (ACR_RequisitionEntry entry : result) entry.m_iRemaining = entry.m_iStock;
  PrintFormat("[ACR] Requisition list for %1: %2 entries", factionKey, result.Count());
 }

 protected static SCR_EntityCatalog GetCatalog(string factionKey, EEntityCatalogType type)
 {
  SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(factionKey));
  if (!faction) return null;
  return faction.GetFactionEntityCatalogOfType(type, false);
 }

 protected static void AddCatalog(string factionKey, EEntityCatalogType type, array<ResourceName> used, array<ResourceName> exclude, array<ref ACR_RequisitionEntry> result)
 {
  SCR_EntityCatalog catalog = GetCatalog(factionKey, type);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   ResourceName prefab = entry.GetPrefab();
   if (prefab.IsEmpty() || used.Contains(prefab) || exclude.Contains(prefab)) continue;
   // Guards can't be hired in vanilla either; static guns become supports later.
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GUARD) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_GUARD)) continue;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TURRET) || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE)) continue;
   string name = WidgetManager.Translate(entry.GetEntityName());
   if (name.IsEmpty()) name = FilePath.StripExtension(FilePath.StripPath(prefab));
   // The catalog lists camouflage variants under the same name; offer each unit once.
   if (HasName(result, name)) continue;
   ACR_RequisitionEntry item = new ACR_RequisitionEntry();
   if (type == EEntityCatalogType.GROUP)
   {
    int cost = GroupCost(entry);
    item.Set(name, prefab, "", cost, StockFor(cost, 60, 6));
   }
   else
   {
    int cost = VehicleCost(entry);
    item.Set(name, "", prefab, cost, StockFor(cost, 90, 4));
    item.m_bAircraft = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER) || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE);
    item.m_bArmed = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED);
    item.m_bTransport = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK) && !entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED) && !item.m_bAircraft;
   }
   used.Insert(prefab);
   result.Insert(item);
  }
 }

 protected static bool HasName(array<ref ACR_RequisitionEntry> list, string name)
 {
  foreach (ACR_RequisitionEntry item : list)
   if (item.m_sName == name) return true;
  return false;
 }

 // 3 RP per soldier (Game Master's AI budget is one per soldier).
 protected static int GroupCost(SCR_EntityCatalogEntry entry)
 {
  int soldiers;
  SCR_EditableGroupUIInfo info = SCR_EditableGroupUIInfo.Cast(entry.GetEntityUiInfo());
  if (info)
  {
   array<ref SCR_EntityBudgetValue> budgets = {};
   info.GetGroupBudgetCost(budgets);
   foreach (SCR_EntityBudgetValue budget : budgets)
    if (budget.GetBudgetType() == EEditableEntityBudget.AI) soldiers += budget.GetBudgetValue();
  }
  if (soldiers <= 0)
  {
   soldiers = 4;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.GROUPSIZE_SMALL)) soldiers = 2;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.GROUPSIZE_LARGE)) soldiers = 8;
  }
  return Math.Max(6, soldiers * 3);
 }

 protected static int VehicleCost(SCR_EntityCatalogEntry entry)
 {
  int cost = 15;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR)) cost = 12;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK)) cost = 15;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC)) cost = 45;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER)) cost = 60;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED)) cost += 10;
  if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR)) cost += 15;
  return cost + 6; // crew
 }

 // Cheaper units are more plentiful.
 protected static int StockFor(int cost, int budget, int maximum)
 {
  return Math.Clamp(Math.Round(budget / cost), 1, maximum);
 }

 static void BuildGarrisonPool(string factionKey, ACR_FactionSettings settings, array<ResourceName> result)
 {
  result.Clear();
  if (settings && settings.m_aGarrisonSoldiers && !settings.m_aGarrisonSoldiers.IsEmpty())
  {
   result.Copy(settings.m_aGarrisonSoldiers);
   return;
  }
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.CHARACTER);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RIFLEMAN))
   {
    // riflemen weighted double
    result.Insert(entry.GetPrefab());
    result.Insert(entry.GetPrefab());
   }
   else if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MACHINEGUNNER) || entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_ANTITANK) || entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GRENADIER))
    result.Insert(entry.GetPrefab());
  }
 }
}
