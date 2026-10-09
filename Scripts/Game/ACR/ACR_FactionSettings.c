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
 bool m_bMortarTeam;  // the built-in mortar team (ACR_Mortars), added to every faction's list
 bool m_bObserverTeam; // the built-in forward observer team (ACR_FireMissions)
 bool m_bRare;        // set automatically: aircraft, armour, mortar team, support helicopters keep a fixed stock
 int m_iSupportHeli;  // support helicopter for a helipad (ACR_HeliStation): 1 gunship, 2 door gunners

 // runtime
 int m_iRemaining;
 int m_iBought;
 int m_iNamed;        // purchases ever made, for unit names (refunds don't reuse a number)
 int m_iBaseCost;     // the price before any purchase (m_iCost is the current price)
 bool m_bStepped;     // stepped pricing: unlimited, the price climbs with each purchase (ACR_Pricing)

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

// How a faction's buy list limits purchases (ACR_Pricing).
enum ACR_EPricing
{
 STEPPED,     // ordinary units unlimited, price climbs a step per purchase; rare units fixed stock
 FIXED_STOCK  // every unit has a stock per match (scaled by threat level); prices never change
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sFrom")]
class ACR_PrefabMapping
{
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Vanilla prefab to replace", "et")]
 ResourceName m_sFrom;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Faction-specific prefab", "et")]
 ResourceName m_sTo;
}

enum ACR_ESupportUnitRole
{
	MORTAR_TEAM,
	OBSERVER_TEAM,
	GUNSHIP,
	DOOR_GUN_HELI
}

// Editable per-faction requisition support. Empty prefab fields keep the current
// faction defaults; resource pickers let mission makers replace only what they need.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_SupportUnitSettings
{
 [Attribute("0", UIWidgets.ComboBox, "Support unit role", "", ParamEnumArray.FromEnum(ACR_ESupportUnitRole))]
 ACR_ESupportUnitRole m_eRole;
 [Attribute("1", UIWidgets.CheckBox, "Offer this unit for requisition")]
 bool m_bEnabled = true;
 [Attribute("", UIWidgets.EditBox, "Name shown in the requisition list (empty = default)")]
 string m_sName;
 [Attribute("-1", UIWidgets.EditBox, "Requisition cost in RP (-1 = default)")]
 int m_iCost;
 [Attribute("-1", UIWidgets.EditBox, "Quantity available per match (-1 = default)")]
 int m_iStock;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer team base group prefab", "et")]
 ResourceName m_sGroupPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer spotter character prefab", "et")]
 ResourceName m_sPrimaryPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Observer radio operator character prefab", "et")]
 ResourceName m_sSecondaryPrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Support helicopter airframe prefab", "et")]
 ResourceName m_sVehiclePrefab;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Support helicopter crew prefab used for pilots and gunners", "et")]
 ResourceName m_sCrewPrefab;
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
 [Attribute("0", UIWidgets.ComboBox, "Stepped prices: infantry, cars and trucks can be bought without limit but each purchase raises that unit's price (threat level sets the step); aircraft, armour, mortar teams and support helicopters keep their stock. Fixed stock: every unit has a stock per match, scaled by threat level, at a fixed price.", "", ParamEnumArray.FromEnum(ACR_EPricing))]
 ACR_EPricing m_ePricing;
 [Attribute("1", UIWidgets.CheckBox, "Add every group and vehicle from the faction's Game Master catalog with automatic prices. Entries below override or add to it.")]
 bool m_bAutoFill;
 [Attribute(desc: "Hand-picked units. With auto fill on, an entry using the same prefab replaces the automatic one.")]
 ref array<ref ACR_RequisitionEntry> m_aRequisitions;
 [Attribute(desc: "Automatic entries to leave out (group or vehicle prefabs)", params: "et")]
 ref array<ResourceName> m_aExclude;
 [Attribute(desc: "Soldiers used for base garrisons. Empty = the faction's riflemen, gunners and AT soldiers from its catalog.", params: "et")]
 ref array<ResourceName> m_aGarrisonSoldiers;
 [Attribute(desc: "Optional exact prefab replacements used for starting units and built-in support teams.")]
 ref array<ref ACR_PrefabMapping> m_aPrefabMappings;

 [Attribute("1", UIWidgets.CheckBox, "The commander can call off-map supports (artillery, air, helicopters)")]
 bool m_bSupports;
 [Attribute(desc: "Supports on offer. Empty = all of them with default prices and cooldowns.")]
 ref array<ref ACR_SupportEntry> m_aSupports;
 [Attribute(desc: "Optional editable requisition support units. Add a row to override defaults; each row can enable/disable a role and set its price, stock and prefabs.")]
 ref array<ref ACR_SupportUnitSettings> m_aSupportUnits;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Legacy gunship fallback; m_aSupportUnits airframe overrides it (empty = UH-1H west, Mi-8 east)", "et")]
 ResourceName m_sSupportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Legacy door-gunner helicopter fallback; m_aSupportUnits airframe overrides it (empty = armed UH-1H west, Mi-8 east)", "et")]
 ResourceName m_sSupportTransportHelicopter;
 [Attribute("", UIWidgets.ResourcePickerThumbnail, "Rocket for rocket strikes and gunships (empty = Hydra 70 / S-5)", "et")]
 ResourceName m_sSupportRocket;

 ACR_SupportUnitSettings SupportUnit(ACR_ESupportUnitRole role)
 {
  if (!m_aSupportUnits) return null;
  foreach (ACR_SupportUnitSettings unit : m_aSupportUnits)
   if (unit && unit.m_eRole == role) return unit;
  return null;
 }

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
 // Resolve explicit mission-maker mappings first. Otherwise find a faction catalog
 // prefab with the same role/name suffix, which covers RHS group variants.
 static ResourceName ResolvePrefab(string factionKey, ResourceName source, EEntityCatalogType type)
 {
  if (source.IsEmpty()) return source;
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && settings.m_aPrefabMappings)
   foreach (ACR_PrefabMapping mapping : settings.m_aPrefabMappings)
    if (mapping && mapping.m_sFrom == source && !mapping.m_sTo.IsEmpty()) return mapping.m_sTo;
  string sourceLeaf = FilePath.StripExtension(FilePath.StripPath(source));
  string wanted = sourceLeaf;
  if (wanted.StartsWith("Group_USSR_")) wanted.Replace("Group_USSR_", "");
  else if (wanted.StartsWith("Group_US_")) wanted.Replace("Group_US_", "");
  SCR_EntityCatalog catalog = GetCatalog(factionKey, type);
  if (!catalog) return source;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  ResourceName suffixMatch;
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (!entry.IsEnabled()) continue;
   ResourceName candidate = entry.GetPrefab();
   string leaf = FilePath.StripExtension(FilePath.StripPath(candidate));
   if (leaf == sourceLeaf) return candidate;
   if (wanted.Length() > 3 && leaf.Contains(wanted))
   {
    if (leaf.EndsWith(wanted)) return candidate;
    if (suffixMatch.IsEmpty()) suffixMatch = candidate;
   }
  }
  if (!suffixMatch.IsEmpty()) return suffixMatch;
  PrintFormat("[ACR] FACTION fallback faction=%1 entry=%2 source prefab unchanged", factionKey, source);
  return source;
 }

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
    ApplyCatalogFlags(factionKey, copy);
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
  // Mission makers can edit support requisitions per faction; an absent row keeps defaults.
  ACR_RequisitionEntry supportEntry = ACR_Mortars.MakeEntry();
  if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.MORTAR_TEAM)) result.Insert(supportEntry);
  supportEntry = ACR_FireMissions.MakeEntry(settings);
  if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.OBSERVER_TEAM)) result.Insert(supportEntry);
  // Worlds with helipads: support helicopters are bought for a pad (a lost one is bought again).
  if (ACR_HeliStation.UsesPads(factionKey))
  {
   supportEntry = ACR_HeliStation.MakeEntry(true, settings);
   if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.GUNSHIP)) result.Insert(supportEntry);
   supportEntry = ACR_HeliStation.MakeEntry(false, settings);
   if (ApplySupportUnit(supportEntry, settings, ACR_ESupportUnitRole.DOOR_GUN_HELI)) result.Insert(supportEntry);
  }
  bool stepped = ACR_Pricing.Stepped(settings);
  foreach (ACR_RequisitionEntry entry : result)
  {
   if (entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli > 0 || entry.m_bAircraft) entry.m_bRare = true;
   ACR_Pricing.Setup(entry, stepped);
  }
  PrintFormat("[ACR] Requisition list for %1: %2 entries (%3 helipads in the world), pricing %4", factionKey, result.Count(), ACR_Helipad.s_All.Count(), ACR_Pricing.Name(stepped));
 }

 // Missing config rows preserve the built-in offer. A row can override only the values
 // the mission maker cares about, or disable that support requisition entirely.
 protected static bool ApplySupportUnit(ACR_RequisitionEntry entry, ACR_FactionSettings settings, ACR_ESupportUnitRole role)
 {
  if (!entry || !settings) return true;
  ACR_SupportUnitSettings unit = settings.SupportUnit(role);
  if (!unit) return true;
  if (!unit.m_bEnabled) return false;
  if (!unit.m_sName.IsEmpty()) entry.m_sName = unit.m_sName;
  if (unit.m_iCost >= 0) entry.m_iCost = unit.m_iCost;
  if (unit.m_iStock >= 0) entry.m_iStock = unit.m_iStock;
  return true;
 }

 // Hand-added entries get the same automatic flags as catalog ones (aircraft, armed,
 // transport, rare) from the faction's catalog entry for their vehicle, when it has one.
 protected static void ApplyCatalogFlags(string factionKey, ACR_RequisitionEntry item)
 {
  if (item.m_sVehicle.IsEmpty()) return;
  SCR_EntityCatalog catalog = GetCatalog(factionKey, EEntityCatalogType.VEHICLE);
  if (!catalog) return;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  foreach (SCR_EntityCatalogEntry entry : entries)
  {
   if (entry.GetPrefab() != item.m_sVehicle) continue;
   SetVehicleFlags(entry, item);
   return;
  }
 }

 protected static void SetVehicleFlags(SCR_EntityCatalogEntry entry, ACR_RequisitionEntry item)
 {
  item.m_bAircraft = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER) || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_AIRPLANE);
  item.m_bArmed = entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED);
  item.m_bTransport = entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK) && !entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED) && !item.m_bAircraft;
  item.m_bRare = item.m_bAircraft || entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMOR);
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
    SetVehicleFlags(entry, item);
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

// Buy-list pricing (Jacob, 4 Oct: Everon pays more RP than either side can spend before
// first contact). Stepped (default): ordinary units have no stock limit, but each side's
// price for a unit climbs a step with every purchase of it and never comes back down; the
// threat level sets the step (low +25%, normal +15%, high +8% of the base price). Rare units
// (aircraft, armour, mortar teams, support helicopters) keep a fixed stock that threat
// doesn't touch. Fixed stock: the old system, stock per match scaled by threat level.
class ACR_Pricing
{
 static const int UNLIMITED = 9999;

 // -acrPricing stepped|stock overrides the faction settings' Pricing attribute.
 static bool Stepped(ACR_FactionSettings settings)
 {
  string value;
  if (System.GetCLIParam("acrPricing", value))
  {
   value.ToLower();
   if (value == "stock") return false;
   if (value == "stepped") return true;
  }
  if (settings) return settings.m_ePricing == ACR_EPricing.STEPPED;
  return true;
 }

 static string Name(bool stepped)
 {
  if (stepped) return string.Format("stepped, +%1 percent of base per purchase", Math.Round(Step() * 100));
  return "fixed stock";
 }

 // Price step per purchase as a share of the base price: a low-threat war gets dear fastest.
 static float Step()
 {
  int threat = ACR_Manager.ThreatOption();
  if (threat == 0) return 0.25;
  if (threat == 2) return 0.08;
  return 0.15;
 }

 static void Setup(ACR_RequisitionEntry entry, bool stepped)
 {
  entry.m_iBaseCost = entry.m_iCost;
  entry.m_bStepped = stepped && !entry.m_bRare;
  if (entry.m_bStepped) entry.m_iRemaining = UNLIMITED;
  else if (stepped) entry.m_iRemaining = entry.m_iStock;
  else entry.m_iRemaining = ACR_Manager.ThreatStock(entry.m_iStock, !entry.m_sVehicle.IsEmpty());
 }

 static int PriceAfter(int baseCost, int bought)
 {
  float climb = Step() * baseCost * bought;
  return baseCost + (int)Math.Round(climb);
 }

 // After a purchase (or a refund): the current price from the number bought.
 static void Reprice(ACR_RequisitionEntry entry)
 {
  if (!entry.m_bStepped) return;
  entry.m_iCost = PriceAfter(entry.m_iBaseCost, entry.m_iBought);
 }

 static bool Unlimited(int remaining)
 {
  return remaining >= UNLIMITED / 2;
 }
}
