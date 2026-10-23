// Verified installed weapon families; ordinary UA variety is applied only where
// it cannot change a purchased team's promised MG/AT/UGL capability.
class ACR_InfantryEquipment
{
 // Use explicit backpack cargo and verify the item on a subsequent retry.
 static bool SpawnSupply(SCR_InventoryStorageManagerComponent inventory, ResourceName item)
 {
  SCR_CharacterInventoryStorageComponent loadout = inventory.GetCharacterStorage();
  if (item == NVG && loadout)
  {
   IEntity helmet = loadout.GetClothFromArea(LoadoutHeadCoverArea);
   ClothNodeStorageComponent mount;
   if (helmet) mount = ClothNodeStorageComponent.Cast(helmet.FindComponent(ClothNodeStorageComponent));
   if (!mount) return false;
   return inventory.TrySpawnPrefabToStorage(item, mount);
  }
  IEntity pack;
  if (loadout) pack = loadout.GetClothFromArea(LoadoutBackpackArea);
  if (!pack) return false;
  SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(pack.FindComponent(SCR_UniversalInventoryStorageComponent));
  if (!cargo) return false;
  return inventory.TrySpawnPrefabToStorage(item, cargo, -1, EStoragePurpose.PURPOSE_DEPOSIT);
 }

 // Inventory-based top-ups run after the UA clothing transfer has settled.
 static bool Supply(SCR_InventoryStorageManagerComponent inventory, ResourceName item, int wanted)
 {
  if (ACR_UALoadout.CountPrefab(inventory, item) >= wanted) return true;
  // One confirmed item per retry. Inventory inserts complete asynchronously;
  // accepting several inserts into the same free slot is not proof they arrived.
  SpawnSupply(inventory, item);
  return false;
 }

 static bool Finish(IEntity soldier)
 {
  ACR_InfantryKitComponent kit = ACR_InfantryKitComponent.Of(soldier);
  if (!kit) return true;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  foreach (string line : ACR_UALoadout.KIT)
  {
   array<string> parts = {};
   line.Split("|", parts, false);
   if (parts.Count() != 3 || ACR_UALoadout.CountKind(soldier, inventory, parts[0]) >= parts[2].ToInt()) continue;
   ResourceName fieldItem = parts[1];
   if (kit.IsUSMC())
   {
    if (parts[0] == "/Compass/") fieldItem = "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et";
    else if (parts[0] == "/Watches/") fieldItem = "{78ED4FEF62BBA728}Prefabs/Items/Equipment/Watches/Watch_SandY184A.et";
    else if (parts[0] == "/Flashlights/") fieldItem = "{3A421547BC29F679}Prefabs/Items/Equipment/Flashlights/Flashlight_MX991/Flashlight_MX991.et";
    else if (parts[0] == "ETool_") fieldItem = "{6E35D94130954509}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE_FreeRoamBuilding_Gadget.et";
    else if (parts[0] == "/Grenades/Grenade_") fieldItem = "{E8F00BF730225B00}Prefabs/Weapons/Grenades/Grenade_M67.et";
    else if (parts[0] == "/Grenades/Smoke_") fieldItem = "{9DB69176CEF0EE97}Prefabs/Weapons/Grenades/Smoke_ANM8HC.et";
   }
   SpawnSupply(inventory, fieldItem);
   return false;
  }
  array<ResourceName> ammunition = {};
  array<BaseMuzzleComponent> muzzles = {};
  ACR_UALoadout.Ammunition(soldier, ammunition, muzzles);
  foreach (int i, ResourceName magazine : ammunition)
  {
   int wanted = 1 + ACR_UALoadout.SpareMagazines(kit.m_eRole, ACR_UALoadout.Leaf(muzzles[i].GetOwner()));
   if (muzzles[i].GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) wanted = 7;
   else
   {
    BaseWeaponComponent weapon = BaseWeaponComponent.Cast(muzzles[i].GetOwner().FindComponent(BaseWeaponComponent));
    if (weapon && weapon.GetWeaponType() == EWeaponType.WT_ROCKETLAUNCHER) wanted = 2;
    else if (weapon && weapon.GetWeaponType() == EWeaponType.WT_HANDGUN) wanted = 3;
   }
   if (!Supply(inventory, magazine, wanted)) return false;
  }
  if (kit.IsSF())
  {
   if (!Supply(inventory, NVG, 1)) return false;
   ResourceName smoke = "{77EAE5E07DC4678A}Prefabs/Weapons/Grenades/Smoke_RDG2.et";
   if (kit.IsUSMC()) smoke = "{9DB69176CEF0EE97}Prefabs/Weapons/Grenades/Smoke_ANM8HC.et";
   if (!Supply(inventory, smoke, 3)) return false;
  }
  if (kit.m_eRole == ACR_EUARole.MEDIC || kit.IsSF())
  {
   int dressings = 4;
   if (kit.m_eRole == ACR_EUARole.MEDIC) dressings = 8;
   if (!Supply(inventory, "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et", dressings)) return false;
   if (!Supply(inventory, "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et", 4)) return false;
   if (!Supply(inventory, "{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et", 4)) return false;
  }
  AIControlComponent control = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
  AIAgent agent;
  if (control) agent = control.GetControlAIAgent();
  SCR_AIGroup group;
  if (agent) group = SCR_AIGroup.Cast(agent.GetParentGroup());
  // A possessed leader's agent can be in the hidden command group. Supply the
  // logical formation's support ammunition rather than losing that association.
  if (group && group.m_eACRFormation == ACR_EFormation.NONE)
  {
   ACR_Squad logical = ACR_Manager.GetInstance().FindSquadBySoldier(soldier);
   if (logical && logical.m_Group) group = logical.m_Group;
  }
  if (group && (group.m_eACRFormation == ACR_EFormation.SCOUT || group.m_eACRFormation == ACR_EFormation.SNIPER || kit.m_eRole == ACR_EUARole.LEADER))
  {
   ResourceName binoculars = BINO;
   if (kit.IsUSMC()) binoculars = "{FAE8B9F7CEA49696}Prefabs/Items/Equipment/Binoculars/Rangefinder_Vector21_lc.et";
   if (!Supply(inventory, binoculars, 1)) return false;
  }
  if (group && (kit.m_eRole == ACR_EUARole.MG_ASSISTANT || kit.m_eRole == ACR_EUARole.AT_ASSISTANT || (kit.m_eRole == ACR_EUARole.MARKSMAN && kit.m_eKit == ACR_EInfantryKit.RU)))
  {
   ACR_EUARole gunner = ACR_EUARole.LAT;
   if (kit.m_eRole == ACR_EUARole.MG_ASSISTANT) gunner = ACR_EUARole.MG;
   array<AIAgent> members = {};
   group.GetAgents(members);
   foreach (AIAgent member : members)
   {
    IEntity body = member.GetControlledEntity();
    ACR_InfantryKitComponent other = ACR_InfantryKitComponent.Of(body);
    if (!other || other.m_eRole != gunner) continue;
    BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
    array<IEntity> carried = {};
    if (weapons) weapons.GetWeaponsList(carried);
    foreach (IEntity supportWeapon : carried)
    {
     BaseWeaponComponent component = BaseWeaponComponent.Cast(supportWeapon.FindComponent(BaseWeaponComponent));
     if (!component) continue;
     if (gunner == ACR_EUARole.MG && component.GetWeaponType() != EWeaponType.WT_MACHINEGUN) continue;
     if (gunner == ACR_EUARole.LAT && component.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER) continue;
     array<BaseMuzzleComponent> supportMuzzles = {};
     component.GetMuzzlesList(supportMuzzles);
     if (!supportMuzzles.IsEmpty()) if (!Supply(inventory, supportMuzzles[0].GetDefaultMagazineOrProjectileName(), 2)) return false;
    }
   }
  }
  return true;
 }
 static const ResourceName AK = "{FA8BE6075AFFBCA8}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M.et";
 static const ResourceName AKDOT = "{1B22CB7855379872}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_1P63.et";
 static const ResourceName GP = "{F9DF0AA717A9A9A1}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_GP25.et";
 static const ResourceName PKM = "{A89BC9D55FFB4CD8}Prefabs/Weapons/MachineGuns/PKM/MG_PKM.et";
 static const ResourceName RPK = "{4EF6890D756306D1}Prefabs/Weapons/MachineGuns/RPK74M/MG_RPK74M.et";
 static const ResourceName SVD = "{6415B7923DE28C1B}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO.et";
 static const ResourceName M4 = "{90D8238D1D8D2DC5}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_RAS_Aimpoint.et";
 static const ResourceName M203 = "{00D6DCF68C664480}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_M203_BLOCK_II.et";
 static const ResourceName M240 = "{A87B0A22462E20B1}Prefabs/Weapons/MachineGuns/M240/MG_M240.et";
 static const ResourceName M249 = "{D2B48DEBEF38D7D7}Prefabs/Weapons/MachineGuns/M249/MG_M249.et";
 static const ResourceName M21 = "{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et";
 static const ResourceName AKSF = "{BCEF9B6B525600EC}Prefabs/Weapons/Rifles/AK74M/variants/Rifle_AK74M_npz_b10m_b19n_tgpa_SSO.et";
 static const ResourceName GPSF = "Prefabs/Weapons/Rifles/AK74M/variants/Rifle_AK74M_npz_b10m_b19n_tgpa_GP25_SSO.et";
 static const ResourceName SVDSF = "{896DDA7B352187EF}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_1P21_TGPV.et";
 static const ResourceName HK = "{7C571F6F5A20A124}Prefabs/Weapons/Rifles/HK416A5/Rifle_HK416A5_Suppressed.et";
 static const ResourceName M27 = "{31F7F8522CF135B1}Prefabs/Weapons/Rifles/M27IAR/Rifle_M27IAR_VCOG_NT4.et";
 static const ResourceName RPG7 = "{D6BC90F8AE0AB8EF}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_pgo7_rhs.et";
 static const ResourceName RPG22 = "{722CE6FEC39EE896}Prefabs/Weapons/Launchers/RPG22/Launcher_RPG22.et";
 static const ResourceName M72 = "{9C5C20FB0E01E64F}Prefabs/Weapons/Launchers/M72/Launcher_M72A3.et";
 static const ResourceName ARMOR = "{4748E96320E4B517}Prefabs/Characters/Vests/Vest_AACPC/Vest_AACPC_NeckProtection_Groin_FG.et";
 static const ResourceName VEST = "{B9F1A4544C4D12E9}Prefabs/Characters/Vests/Vest_AACPC/variants/Vest_AACPC_arsarmaHSGI_FG_2.et";
 static const ResourceName RUHELMET = "{7600E06C18AC1B53}Prefabs/Characters/HeadGear/Helmet_LShZ/variants/SSO/Helmet_LShZ_new_OD_HC_Peltor_SSO.et";
 static const ResourceName UAHELMET = "{A36613EAD646961D}Prefabs/Characters/HeadGear/Helmet_OPSCORE/Helmet_OPSCORE_XP_MC_AMP.et";
 static const ResourceName NVG = "{B385064A383A384B}Prefabs/Items/Equipment/Nightvision/PVS14/NVG_PVS14Dual_WP.et";
 static const ResourceName BINO = "{243948B23D90BECB}Prefabs/Items/Equipment/Binoculars/Binoculars_B8/Binoculars_B8.et";

 static const ResourceName MARINE = "{0761CE8644BBB28E}Prefabs/Weapons/Rifles/M27IAR/Rifle_M27IAR_TA31RCO.et";
 static const ResourceName MARINE_GL = "{A692E521D21D8ECA}Prefabs/Weapons/Rifles/M4A1/Rifle_M4_M203_RAS_RCO_PEQ.et";
 static const ResourceName MARINE_MG = "{BA87F78D0E7E3A90}Prefabs/Weapons/MachineGuns/M240/MG_M240_Light_MDO.et";
 static const ResourceName M38 = "{D0F213711B925CF5}Prefabs/Weapons/Rifles/M27IAR/Rifle_M38SDMR.et";
 static const ResourceName M40 = "{56CE827F9AD5880E}Prefabs/Weapons/Rifles/M40/Rifle_M40A5_Optic_PEQ.et";

 static ResourceName Primary(ACR_InfantryKitComponent kit)
 {
  ACR_EUARole role = kit.m_eRole;
  if (kit.m_eKit == ACR_EInfantryKit.MARSOC)
  {
   if (role == ACR_EUARole.AR) return M27;
   if (role == ACR_EUARole.MARKSMAN) return M38;
   return HK;
  }
  if (kit.m_eKit == ACR_EInfantryKit.USMC)
  {
   if (role == ACR_EUARole.GL || role == ACR_EUARole.SPOTTER) return MARINE_GL;
   if (role == ACR_EUARole.MG) return MARINE_MG;
   if (role == ACR_EUARole.AR) return M249;
   if (role == ACR_EUARole.MARKSMAN) return M38;
   if (role == ACR_EUARole.SNIPER) return M40;
   return MARINE;
  }
  if (kit.m_eKit == ACR_EInfantryKit.RU_SF)
  {
   if (role == ACR_EUARole.GL) return GPSF;
   if (role == ACR_EUARole.MARKSMAN) return SVDSF;
   return AKSF;
  }
  if (kit.m_eKit == ACR_EInfantryKit.UA_SF)
  {
   if (role == ACR_EUARole.AR) return M27;
   if (role == ACR_EUARole.MARKSMAN) return SVDSF;
   return HK;
  }
  if (kit.m_eKit == ACR_EInfantryKit.WESTERN)
  {
   if (role == ACR_EUARole.GL) return M203;
   if (role == ACR_EUARole.MG) return M240;
   if (role == ACR_EUARole.AR) return M249;
   if (role == ACR_EUARole.MARKSMAN) return M21;
   return M4;
  }
  if (role == ACR_EUARole.GL) return GP;
  if (role == ACR_EUARole.MG) return PKM;
  if (role == ACR_EUARole.AR) return RPK;
  if (kit.m_eKit == ACR_EInfantryKit.UA)
  {
   if (role == ACR_EUARole.MARKSMAN) return ACR_UALoadout.PrimaryFor(ACR_EUARole.MARKSMAN);
   if (role == ACR_EUARole.LEADER) return ACR_UALoadout.PrimaryFor(ACR_EUARole.LEADER);
   return ACR_UALoadout.PrimaryFor(ACR_EUARole.RIFLEMAN);
  }
  if (role == ACR_EUARole.MARKSMAN) return SVD;
  if (role == ACR_EUARole.LEADER) return AKDOT;
  return AK;
 }

 static ResourceName Launcher(ACR_InfantryKitComponent kit)
 {
  if (kit.m_eRole == ACR_EUARole.LAT) return RPG7;
  if (kit.m_eRole != ACR_EUARole.LIGHT_AT) return "";
  if (kit.IsUSMC() || kit.m_eKit == ACR_EInfantryKit.WESTERN || kit.m_eKit == ACR_EInfantryKit.UA_SF) return M72;
  if (kit.m_eKit == ACR_EInfantryKit.UA && Math.RandomFloat01() < 0.25) return M72;
  return RPG22;
 }
}
