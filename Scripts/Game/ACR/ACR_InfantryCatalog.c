// Faction-specific defaults extend the existing catalog/purchase transaction. No
// group roster assets: the native group's slots remain the reinforcement recipe.
enum ACR_EFormation { NONE, LINE, MANEUVER, MG, AT, SCOUT, SNIPER, SF, ENGINEER, MEDIC }

modded class SCR_AIGroup
{
 ACR_EFormation m_eACRFormation;
}

class ACR_InfantryCatalog
{
 static bool Active(string faction)
 {
  return faction == "RHS_AFRF" || faction == "RHS_USAF" || (faction == "US" && ACR_FactionSelection.UkraineModLoaded());
 }

 static bool UseDefaults(string faction, ACR_FactionSettings settings)
 {
  if (!Active(faction)) return false;
  if (!settings) return true;
  return settings.m_bAutoFill && (!settings.m_aRequisitions || settings.m_aRequisitions.IsEmpty());
 }

 static ACR_RequisitionEntry Add(array<ref ACR_RequisitionEntry> entries, string name, int cost, ACR_EFormation formation, ACR_EInfantryKit kit, array<int> roles)
 {
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  ResourceName group = ACR_Mortars.GROUP_WEST;
  if (kit == ACR_EInfantryKit.RU || kit == ACR_EInfantryKit.RU_SF) group = ACR_Mortars.GROUP_EAST;
  entry.Set(name, group, "", cost, 4);
  entry.m_eFormation = formation;
  entry.m_aSoldiers = {};
  foreach (int role : roles) entry.m_aSoldiers.Insert(ACR_InfantryTemplates.Character(kit, role));
  if (formation == ACR_EFormation.SF) { entry.m_bRare = true; entry.m_iStock = 2; }
  entries.Insert(entry);
  return entry;
 }

 static void Build(string faction, array<ref ACR_RequisitionEntry> entries)
 {
  if (faction == "RHS_USAF") { BuildUSMC(entries); return; }
  ACR_EInfantryKit kit = ACR_EInfantryKit.UA;
  bool ru = faction == "RHS_AFRF";
  if (ru) kit = ACR_EInfantryKit.RU;
  if (ru) Add(entries, "Motor Rifle Squad", 24, ACR_EFormation.LINE, kit, {1,4,2,10,5,6});
  else Add(entries, "Rifle Squad", 30, ACR_EFormation.LINE, kit, {1,0,0,4,3,2,10,5,6});
  Add(entries, "Maneuver Team", 14, ACR_EFormation.MANEUVER, kit, {1,4,3,14});
  Add(entries, "Machine Gun Team", 12, ACR_EFormation.MG, kit, {1,2,10});
  Add(entries, "Anti-Tank Team", 14, ACR_EFormation.AT, kit, {1,5,11});
  if (!ru) Add(entries, "Light AT Team", 12, ACR_EFormation.AT, kit, {1,14,14});
  Add(entries, "Scout Team", 18, ACR_EFormation.SCOUT, kit, {1,0,0,6});
  Add(entries, "Sniper Pair", 16, ACR_EFormation.SNIPER, kit, {6,1});
  if (!ru)
  {
   Add(entries, "Western Rifle Squad", 34, ACR_EFormation.LINE, ACR_EInfantryKit.WESTERN, {1,0,14,4,3,2,10,6});
   Add(entries, "Western Fire Team", 18, ACR_EFormation.MANEUVER, ACR_EInfantryKit.WESTERN, {1,4,3,14});
  }
  if (ru) Add(entries, "Special Forces Team", 48, ACR_EFormation.SF, ACR_EInfantryKit.RU_SF, {1,0,12,14,4,6});
  else Add(entries, "Special Forces Team", 48, ACR_EFormation.SF, ACR_EInfantryKit.UA_SF, {1,0,12,14,3,6});
  Add(entries, "Engineer Team", 16, ACR_EFormation.ENGINEER, kit, {1,13,13,3});
  Add(entries, "Medical Team", 10, ACR_EFormation.MEDIC, kit, {12,12,0});
 }

 static void BuildUSMC(array<ref ACR_RequisitionEntry> entries)
 {
  ACR_EInfantryKit kit = ACR_EInfantryKit.USMC;
  Add(entries, "Marine Rifle Squad", 34, ACR_EFormation.LINE, kit, {1,1,6,3,3,4,4,14,14});
  Add(entries, "Marine Fire Team", 18, ACR_EFormation.MANEUVER, kit, {1,3,4,14});
  Add(entries, "Light Infantry Team", 14, ACR_EFormation.MANEUVER, kit, {1,0,0,0});
  Add(entries, "Machine Gun Team", 12, ACR_EFormation.MG, kit, {1,2,10});
  Add(entries, "Grenadier Team", 18, ACR_EFormation.MANEUVER, kit, {1,3,4,4});
  Add(entries, "Light AT Team", 16, ACR_EFormation.AT, kit, {1,3,14,14});
  Add(entries, "Scout Team", 20, ACR_EFormation.SCOUT, kit, {1,0,0,6});
  Add(entries, "Sniper Pair", 16, ACR_EFormation.SNIPER, kit, {15,16});
  Add(entries, "Engineer Team", 16, ACR_EFormation.ENGINEER, kit, {1,13,13,3});
  Add(entries, "Medical Team", 10, ACR_EFormation.MEDIC, kit, {12,12,0});
  Add(entries, "MARSOC Team", 48, ACR_EFormation.SF, ACR_EInfantryKit.MARSOC, {1,0,12,14,3,6});
 }

 static ACR_RequisitionEntry Starting(string faction, ResourceName group)
 {
  if (!UseDefaults(faction, ACR_FactionSettings.Find(faction)) || !group.Contains("RifleSquad")) return null;
  array<ref ACR_RequisitionEntry> entries = {};
  Build(faction, entries);
  return entries[0];
 }

 static bool Specialist(ACR_EFormation formation)
 {
  return formation == ACR_EFormation.SCOUT || formation == ACR_EFormation.SNIPER || formation == ACR_EFormation.ENGINEER || formation == ACR_EFormation.MEDIC;
 }

 // Deliberately independent of RP. A price increase must never make SF more attractive.
 static float AIWeight(ACR_RequisitionEntry entry)
 {
  if (entry.m_eFormation == ACR_EFormation.SF) return 6.0;
  if (entry.m_iBaseCost > 0) return entry.m_iBaseCost;
  return entry.m_iCost;
 }

 static void VehiclePrices(string faction, array<ref ACR_RequisitionEntry> entries)
 {
  if (!Active(faction)) return;
  foreach (ACR_RequisitionEntry entry : entries)
  {
   if (entry.m_sVehicle.IsEmpty() || entry.m_bAircraft || entry.m_iSupportHeli > 0) continue;
   string path = entry.m_sVehicle;
   int service = ACR_Logistics.ServiceOfName(path);
   if ((service & ACR_EService.CONSTRUCTION) != 0) entry.m_iCost = 24;
   else if ((service & (ACR_EService.AMMO | ACR_EService.REPAIR | ACR_EService.FUEL)) != 0) entry.m_iCost = 22;
   else if (service != 0) continue; // ambulance prices were not part of the approved truck revision
   else if (path.Contains("BRDM2")) entry.m_iCost = 52;
   else if (path.Contains("BTR70") && faction == "RHS_AFRF") entry.m_iCost = 72;
   else if (path.Contains("K4386"))
   {
    if (entry.m_bArmed) entry.m_iCost = 52;
    else entry.m_iCost = 38;
   }
   else if (entry.m_bTransport) entry.m_iCost = 22;
   else if (path.Contains("UAZ469"))
   {
    if (entry.m_bArmed) entry.m_iCost = 26;
    else entry.m_iCost = 16;
   }
   else if (path.Contains("M998") || path.Contains("M1025"))
   {
    if (entry.m_bArmed) entry.m_iCost = 32;
    else entry.m_iCost = 20;
   }
  }
 }
}
