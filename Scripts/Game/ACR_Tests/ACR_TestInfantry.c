modded class ACR_AICommander
{
 bool InfantryTestHasSF() { return HasSpecialForces(); }
}

// Native purchase, inventory and casualty-replacement acceptance for RU/UA defaults.
class ACR_TestInfantry
{
 static ref array<ref ACR_PendingPurchase> s_Units = {};
 static ref array<ref ACR_RequisitionEntry> s_Entries = {};
 static ref array<ref ACR_PendingPurchase> s_Replacements = {};
 static int s_Checks;
 static bool s_USMC;
 static ref array<IEntity> s_NativeMarines = {};
 static ref array<string> s_NativeRoles = {"Rifleman", "SL", "LAT", "AMG", "TL_Marksman"};

 static void Check(string label, bool ok, string detail = "")
 {
  s_Checks++;
  ACR_TestRegress.Check("infantry: " + label, ok, detail);
 }

 static bool Weapons(IEntity man, ACR_InfantryKitComponent kit)
 {
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(man.FindComponent(BaseWeaponManagerComponent));
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!weapons || !inventory || !kit || !ACR_UALoadout.Settled(man)) { PrintFormat("[ACR] KITTEST early %1 weapons=%2 inventory=%3 kit=%4 settled=%5", ACR_UALoadout.Leaf(man), weapons != null, inventory != null, kit != null, ACR_UALoadout.Settled(man)); if (kit) PrintFormat("[ACR] KITTRIES %1 %2", ACR_UALoadout.Leaf(man), kit.m_iSupplyAttempts); return false; }
  foreach (string kitLine : ACR_UALoadout.KIT)
  {
   array<string> kitParts = {};
   kitLine.Split("|", kitParts, false);
   if (kitParts.Count() != 3 || ACR_UALoadout.CountKind(man, inventory, kitParts[0]) < kitParts[2].ToInt()) return false;
  }
  SCR_CharacterInventoryStorageComponent worn = inventory.GetCharacterStorage();
  if (kit.IsUA() && (ACR_UALoadout.Leaf(worn.GetClothFromArea(LoadoutJacketArea)) != "Jacket_UA_M14_ground" || ACR_UALoadout.Leaf(worn.GetClothFromArea(LoadoutPantsArea)) != "Pants_AU_M14_piso")) return false;
  if (kit.IsSF())
  {
   if (ACR_UALoadout.CountPrefab(inventory, ACR_InfantryEquipment.NVG) != 1) return false;
   IEntity armor = worn.GetClothFromArea(LoadoutArmoredVestSlotArea);
   if (!armor || armor.GetPrefabData().GetPrefabName() != ACR_InfantryEquipment.ARMOR) return false;
   array<IEntity> sfItems = {};
   inventory.GetItems(sfItems);
   bool mounted;
   foreach (IEntity sfItem : sfItems)
   {
    if (!sfItem || !sfItem.GetPrefabData() || sfItem.GetPrefabData().GetPrefabName() != ACR_InfantryEquipment.NVG) continue;
    InventoryItemComponent nvgItem = InventoryItemComponent.Cast(sfItem.FindComponent(InventoryItemComponent));
    if (nvgItem && nvgItem.GetParentSlot() && nvgItem.GetParentSlot().GetStorage().GetOwner() == worn.GetClothFromArea(LoadoutHeadCoverArea)) mounted = true;
   }
   if (!mounted) return false;
  }
  if (kit.m_eRole == ACR_EUARole.MEDIC && ACR_UALoadout.CountKind(man, inventory, "FieldDressing") < 8) return false;
  if (kit.m_eRole == ACR_EUARole.MG_ASSISTANT || kit.m_eRole == ACR_EUARole.AT_ASSISTANT || kit.m_eRole == ACR_EUARole.MARKSMAN)
  {
   AIControlComponent supportControl = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
   AIAgent supportAgent = supportControl.GetControlAIAgent();
   SCR_AIGroup supportGroup = SCR_AIGroup.Cast(supportAgent.GetParentGroup());
   ACR_EUARole supportRole = ACR_EUARole.LAT;
   if (kit.m_eRole == ACR_EUARole.MG_ASSISTANT) supportRole = ACR_EUARole.MG;
   bool needsSupportAmmo = kit.m_eRole != ACR_EUARole.MARKSMAN || (kit.m_eKit == ACR_EInfantryKit.RU && supportGroup.m_eACRFormation == ACR_EFormation.LINE);
   if (needsSupportAmmo)
   {
    bool hasSupportAmmo;
    array<AIAgent> supportMembers = {};
    supportGroup.GetAgents(supportMembers);
    foreach (AIAgent supportMember : supportMembers)
    {
     IEntity supportBody = supportMember.GetControlledEntity();
     ACR_InfantryKitComponent supportKit = ACR_InfantryKitComponent.Of(supportBody);
     if (!supportKit || supportKit.m_eRole != supportRole) continue;
     BaseWeaponManagerComponent supportWeapons = BaseWeaponManagerComponent.Cast(supportBody.FindComponent(BaseWeaponManagerComponent));
     array<IEntity> supportGuns = {};
     supportWeapons.GetWeaponsList(supportGuns);
     foreach (IEntity supportGun : supportGuns)
     {
      BaseWeaponComponent supportWeapon = BaseWeaponComponent.Cast(supportGun.FindComponent(BaseWeaponComponent));
      if (!supportWeapon) continue;
      if (supportRole == ACR_EUARole.LAT && supportWeapon.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER) continue;
      if (supportRole == ACR_EUARole.MG && supportWeapon.GetWeaponType() != EWeaponType.WT_MACHINEGUN) continue;
      array<BaseMuzzleComponent> supportMuzzles = {};
      supportWeapon.GetMuzzlesList(supportMuzzles);
      if (!supportMuzzles.IsEmpty() && ACR_UALoadout.CountPrefab(inventory, supportMuzzles[0].GetDefaultMagazineOrProjectileName()) >= 2) hasSupportAmmo = true;
     }
    }
    if (!hasSupportAmmo) return false;
   }
  }
  array<IEntity> carried = {};
  weapons.GetWeaponsList(carried);
  int primaries;
  bool launcher = kit.m_eRole != ACR_EUARole.LAT && kit.m_eRole != ACR_EUARole.LIGHT_AT;
  bool supplied = true;
  foreach (IEntity gun : carried)
  {
   BaseWeaponComponent component = BaseWeaponComponent.Cast(gun.FindComponent(BaseWeaponComponent));
   if (!component) continue;
   if (component.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER)
   {
    launcher = true;
    if (kit.m_eRole == ACR_EUARole.LAT && !ACR_UALoadout.Leaf(gun).Contains("RPG7")) return false;
    continue;
   }
   if (component.GetWeaponType() != EWeaponType.WT_RIFLE && component.GetWeaponType() != EWeaponType.WT_MACHINEGUN && component.GetWeaponType() != EWeaponType.WT_SNIPERRIFLE && component.GetWeaponType() != EWeaponType.WT_GRENADELAUNCHER) continue;
   primaries++;
   array<BaseMuzzleComponent> muzzles = {};
   component.GetMuzzlesList(muzzles);
   if (muzzles.IsEmpty() || !component.GetCurrentMagazine() || component.GetCurrentMagazine().GetAmmoCount() <= 0) supplied = false;
   else if (ACR_UALoadout.CountPrefab(inventory, muzzles[0].GetDefaultMagazineOrProjectileName()) < 2) supplied = false;
   if ((kit.m_eRole == ACR_EUARole.GL || kit.m_eRole == ACR_EUARole.SPOTTER) && muzzles.Count() < 2) return false;
   if (kit.m_eRole == ACR_EUARole.MG && component.GetWeaponType() != EWeaponType.WT_MACHINEGUN) return false;
   string name = ACR_UALoadout.Leaf(gun);
   if (kit.m_eRole == ACR_EUARole.MARKSMAN && !ACR_SquadMarksmen.IsDMR(gun)) return false;
   if (kit.IsUSMC())
   {
    ResourceName expectedWeapon = ACR_InfantryEquipment.Primary(kit);
    if (gun.GetPrefabData().GetPrefabName() != expectedWeapon) return false;
    array<IEntity> marineItems = {};
    inventory.GetItems(marineItems);
    bool rco;
    foreach (IEntity marineItem : marineItems)
    {
     string marinePath = marineItem.GetPrefabData().GetPrefabName();
     if (marinePath.Contains("PAS13") || marinePath.Contains("/Thermal")) return false;
     if (marinePath.Contains("optic_TA31RCO")) rco = true;
    }
    if (expectedWeapon == ACR_InfantryEquipment.MARINE && !rco) return false;
    if (kit.m_eRole == ACR_EUARole.SNIPER && !name.Contains("M40A5")) return false;
   }
   if (kit.IsSF())
   {
    string path = gun.GetPrefabData().GetPrefabName();
    if (path != ACR_InfantryEquipment.AKSF && path != ACR_InfantryEquipment.GPSF && path != ACR_InfantryEquipment.SVDSF && path != ACR_InfantryEquipment.HK && path != ACR_InfantryEquipment.M27 && path != ACR_InfantryEquipment.M38) return false;
    // Check the actual attached muzzle device, not just the weapon prefab name.
    array<IEntity> children = {};
    inventory.GetItems(children);
    bool suppressor;
    foreach (IEntity child : children)
    {
     if (!child || !child.GetPrefabData()) continue;
     string itemPath = child.GetPrefabData().GetPrefabName();
     if (itemPath.Contains("/Thermal") || itemPath.Contains("PAS13")) return false;
     IEntity ancestor = child.GetParent();
     for (int depth = 0; ancestor && depth < 12; depth++)
     {
      if (ancestor == gun && (itemPath.Contains("Supressor_TGP") || itemPath.Contains("SF_RC3") || itemPath.Contains("KAC_NT4_QDSS"))) suppressor = true;
      ancestor = ancestor.GetParent();
     }
    }
    if (!suppressor) return false;
   }
  }
  if (primaries != 1 || !supplied || !launcher) PrintFormat("[ACR] KITTEST %1 primaries=%2 supplied=%3 launcher=%4", ACR_UALoadout.Leaf(man), primaries, supplied, launcher);
  return primaries == 1 && supplied && launcher;
 }

 static void Run(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 5)
  {
   s_USMC = System.IsCLIParam("acrUSMCInfantryTest");
   if ((!s_USMC && !ACR_FactionSelection.UkraineModLoaded()) || !GetGame().GetFactionManager().GetFactionByKey("RHS_AFRF"))
   { ACR_TestRegress.Abort("infantry", "requires RHS + UA pair"); return; }
   if (s_USMC)
   {
    foreach (int nativeIndex, string nativeRole : s_NativeRoles)
    {
     ResourceName nativePrefab = "Prefabs/Characters/Factions/BLUFOR/RHS_USAF/RHS_USAF_USMC_MEF/Character_RHS_USAF_USMC_" + nativeRole + ".et";
     IEntity nativeMarine = ACR_Requisition.SpawnForPurchase(nativePrefab, ACR_Manager.Ground(Vector(1350 + nativeIndex * 5,0,2700)));
     s_NativeMarines.Insert(nativeMarine);
    }
   }
   array<string> sides = {"RHS_AFRF", "US"};
   if (s_USMC) sides = {"RHS_AFRF", "RHS_USAF"};
   foreach (int sideIndex, string side : sides)
   {
    array<ref ACR_RequisitionEntry> entries = {};
    ACR_InfantryCatalog.Build(side, entries);
    int count = 9;
    if (side == "US") count = 12;
    array<int> prices = {24,14,12,14,18,16,48,16,10};
    array<int> sizes = {6,4,3,3,4,2,6,4,3};
    if (side == "US")
    {
     prices = {30,14,12,14,12,18,16,34,18,48,16,10};
     sizes = {9,4,3,3,3,4,2,8,4,6,4,3};
    }
    if (side == "RHS_USAF")
    {
     count = 11;
     prices = {34,18,14,12,18,16,20,16,16,10,48};
     sizes = {9,4,4,3,4,4,4,2,4,3,6};
    }
    bool correct = entries.Count() == count;
    foreach (int priceIndex, ACR_RequisitionEntry priced : entries)
     if (priceIndex >= prices.Count() || priced.m_iCost != prices[priceIndex] || priced.m_aSoldiers.Count() != sizes[priceIndex]) correct = false;
    Check(side + " approved prices and formation sizes", correct);
    ACR_FactionData data = manager.m_Bases.GetFaction(side);
    ACR_Base home;
    foreach (ACR_Base candidate : ACR_Base.s_All)
     if (candidate.m_sOwner == side && ACR_LogisticsRules.CanDeploy(candidate)) { home = candidate; break; }
    int sfIndex = -1;
    foreach (int offerIndex, ACR_RequisitionEntry offer : data.m_aRequisitions)
     if (offer.m_eFormation == ACR_EFormation.SF) sfIndex = offerIndex;
    for (int failure = 0; failure < 2; failure++)
    {
     data.m_iRP = 500;
     int beforeStock;
     if (sfIndex >= 0) beforeStock = data.m_aRequisitions[sfIndex].m_iRemaining;
     ACR_Requisition.s_iTestPassSpawns = failure;
     ACR_Requisition.s_iTestFailSpawns = 1;
     string reply = manager.m_Requisition.PurchaseAt(side, sfIndex, home);
     ACR_Requisition.s_iTestFailSpawns = 0;
     ACR_Requisition.s_iTestPassSpawns = 0;
     bool refunded = sfIndex >= 0 && data.m_iRP == 500 && data.m_aRequisitions[sfIndex].m_iRemaining == beforeStock && reply.Contains("refunded");
     Check(side + " failed SF deployment refunds money and rare stock", refunded, reply);
    }
    foreach (int i, ACR_RequisitionEntry entry : entries)
    {
     s_Entries.Insert(entry);
     ACR_PendingPurchase purchase = new ACR_PendingPurchase();
     purchase.m_Entry = entry;
     purchase.m_sFaction = side;
     purchase.m_sName = "Kit " + side + " " + entry.m_sName;
     vector position = Vector(1400 + 40 * i, 0, 2750 + 400 * sideIndex);
     position = ACR_Manager.Ground(position);
     bool spawned = manager.m_Requisition.SpawnParts(purchase, entry.m_sGroup, "", false, "", position, position);
     s_Units.Insert(purchase);
     Check(purchase.m_sName + " spawned", spawned && purchase.m_Group != null);
     if (purchase.m_Group)
     {
      Faction friendly = GetGame().GetFactionManager().GetFactionByKey("US");
      purchase.m_Group.SetFaction(friendly);
      array<AIAgent> friendlyAgents = {};
      purchase.m_Group.GetAgents(friendlyAgents);
      foreach (AIAgent friendlyAgent : friendlyAgents)
      {
       IEntity friendlyBody = friendlyAgent.GetControlledEntity();
       SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(friendlyBody.FindComponent(SCR_FactionAffiliationComponent));
       if (affiliation) affiliation.SetAffiliatedFaction(friendly);
      }
     }
     if (entry.m_eFormation == ACR_EFormation.SF)
     {
      ACR_Pricing.Setup(entry, true);
      array<int> sfStock = {1,2,4};
      bool finiteStock = !entry.m_bStepped && entry.m_iRemaining == sfStock[ACR_Manager.ThreatOption()];
      float weight = ACR_InfantryCatalog.AIWeight(entry);
      entry.m_iBaseCost = 240;
      entry.m_iCost = 240;
      Check(side + " SF price-independent weight and scarcity", weight == ACR_InfantryCatalog.AIWeight(entry) && entry.m_bRare && entry.m_iStock == 2 && finiteStock);
      entry.m_iCost = 48;
      entry.m_iBaseCost = 48;
     }
    }
   }
  }
  if (tick == 30)
  {
   foreach (ACR_PendingPurchase sfUnit : s_Units)
   {
    if (!sfUnit.m_Group || sfUnit.m_Group.m_eACRFormation != ACR_EFormation.SF) continue;
    ACR_Squad sfSquad = manager.FindSquadByGroup(sfUnit.m_Group);
    if (sfSquad) sfSquad.Rename("Ordinary Formation");
   }
   string west = "US";
   if (s_USMC) west = "RHS_USAF";
   ACR_AICommander uaAI = new ACR_AICommander(manager, west);
   Check("AI recognizes active SF independently of price or display name", uaAI.InfantryTestHasSF());
   if (s_USMC)
   {
    foreach (int inspectedIndex, IEntity nativeBody : s_NativeMarines)
    {
     ACR_FactionCatalog.TrimUSMCRiflemanOptic(nativeBody);
     SCR_InventoryStorageManagerComponent nativeInventory;
     if (nativeBody) nativeInventory = SCR_InventoryStorageManagerComponent.Cast(nativeBody.FindComponent(SCR_InventoryStorageManagerComponent));
     array<IEntity> nativeItems = {};
     if (nativeInventory) nativeInventory.GetItems(nativeItems);
     bool expectedSight;
     bool oldSight;
     foreach (IEntity nativeItem : nativeItems)
     {
      string nativePath = nativeItem.GetPrefabData().GetPrefabName();
      if (nativePath.Contains("optic_vc18dsco")) oldSight = true;
      if (s_NativeRoles[inspectedIndex] == "TL_Marksman")
      { if (nativePath.Contains("optic_ts30a2")) expectedSight = true; }
      else if (nativePath.Contains("Optic_TA31RCO")) expectedSight = true;
     }
     Check("native Marine " + s_NativeRoles[inspectedIndex] + " wears the correct combat or marksman optic", expectedSight && !oldSight);
    }
    ACR_FactionData marineData = manager.m_Bases.GetFaction("RHS_USAF");
    bool vehiclePrices = true;
    int sharedVehicles;
    foreach (ACR_RequisitionEntry vehicleEntry : marineData.m_aRequisitions)
    {
     string vehiclePath = vehicleEntry.m_sVehicle;
     if (vehiclePath.IsEmpty() || vehicleEntry.m_bAircraft || vehicleEntry.m_iSupportHeli > 0) continue;
     int wantedPrice;
     int service = ACR_Logistics.ServiceOfName(vehiclePath);
     if ((service & ACR_EService.CONSTRUCTION) != 0) wantedPrice = 24;
     else if ((service & (ACR_EService.AMMO | ACR_EService.REPAIR | ACR_EService.FUEL)) != 0) wantedPrice = 22;
     else if (service != 0) continue;
     else if (vehicleEntry.m_bTransport) wantedPrice = 22;
     else if (vehiclePath.Contains("M998") || vehiclePath.Contains("M1025"))
     {
      wantedPrice = 20;
      if (vehicleEntry.m_bArmed) wantedPrice = 32;
     }
     if (wantedPrice == 0) continue;
     sharedVehicles++;
     if (vehicleEntry.m_iBaseCost != wantedPrice) vehiclePrices = false;
    }
    Check("US shared vehicles use approved base prices", vehiclePrices && sharedVehicles >= 7, string.Format("%1 vehicles", sharedVehicles));
   }
   foreach (int index, ACR_PendingPurchase inspected : s_Units)
   {
    ACR_RequisitionEntry spec = s_Entries[index];
    array<AIAgent> members = {};
    if (inspected.m_Group) inspected.m_Group.GetAgents(members);
    bool ok = members.Count() == spec.m_aSoldiers.Count();
    string detail;
    foreach (AIAgent member : members)
    {
     IEntity soldier = member.GetControlledEntity();
     ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
     bool equipped = soldier && Weapons(soldier, kit);
     if (!equipped) detail += " " + ACR_UALoadout.Leaf(soldier);
     ok = equipped && ok;
    }
    Check(inspected.m_sName + " physical strength, roles and loaded ammunition", ok, detail);
    if (spec.m_eFormation == ACR_EFormation.SF && inspected.m_Group && !members.IsEmpty())
    {
     IEntity casualty = members[0].GetControlledEntity();
     ResourceName original = casualty.GetPrefabData().GetPrefabName();
     vector at = casualty.GetOrigin();
     SCR_EntityHelper.DeleteEntityAndChildren(casualty);
     ACR_Squad squad = manager.FindSquadByGroup(inspected.m_Group);
     array<ResourceName> missing = {};
     if (squad) squad.GetMissing(missing);
     Check("SF casualty exposes the same native replacement template", missing.Count() == 1 && missing[0] == original);
     ACR_PendingPurchase replacement = new ACR_PendingPurchase();
     replacement.m_Group = inspected.m_Group;
     replacement.m_sName = original;
     replacement.m_Vehicle = ACR_Requisition.SpawnForPurchase(original, at);
     if (replacement.m_Vehicle) inspected.m_Group.AddAIEntityToGroup(replacement.m_Vehicle);
     s_Replacements.Insert(replacement);
    }
   }
  }
  if (tick == 55)
  {
   foreach (ACR_PendingPurchase restored : s_Replacements)
    Check("SF casualty replacement retains suppressed kit", restored.m_Vehicle && Weapons(restored.m_Vehicle, ACR_InfantryKitComponent.Of(restored.m_Vehicle)));
   Check("both factions replaced SF casualties", s_Replacements.Count() == 2);
   int expected = 56;
   if (s_USMC) expected = 60;
   ACR_TestRegress.Complete("infantry", expected);
  }
 }
}
