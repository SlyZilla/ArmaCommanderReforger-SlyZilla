// UA soldiers (Jacob, 11 Oct live: "a lot of 80s US equipment, and no pants").
// Ukraine Armed Forces RHS re-skins the vanilla US key, but only its base soldier: every
// role prefab keeps its 80s US kit, and the base's pants prefab (Pants_UA_M14_ground) is not
// shipped, so they spawn without trousers. Each UA soldier (AI and command unit) is
// re-equipped once on the server, within a second of appearing: UA M14 jacket and pants,
// a UA helmet, TV102 vests, Faradey boots, and a weapon by role. Weapons: mostly Soviet
// pattern, about one in four Western (Jacob, 11 Oct: "mostly Soviet, some West"). Magazines
// come from each new weapon's own default magazine, so they always fit.
// Pilots keep their kit; crews keep their helmets. Inactive unless the UA mod is loaded.
enum ACR_EUARole
{
 RIFLEMAN,
 LEADER,
 MG,
 AR,
 GL,
 LAT,
 MARKSMAN,
 CREW,
 PILOT,
 UNARMED
}

class ACR_UALoadout
{
 static bool s_bEnabled = true;
 static float s_fWestShare = 0.25;
 static int s_iEquipped;
 static int s_iFailed;
 protected static ref set<EntityID> s_Done = new set<EntityID>();
 protected static ref map<string, ref array<ResourceName>> s_Pools;

 static const ResourceName JACKET = "{A9D4DA189D2FBC65}Prefabs/Characters/Uniforms/Jacket_UA_M14_ground.et";
 static const ResourceName PANTS = "{846B61FF83278E38}Prefabs/Characters/Uniforms/Pants_AU_M14_piso.et";
 static const ResourceName BOOTS = "{AB9E3D5524876346}Prefabs/Characters/Footwear/Boots_Faradey_1085/Footware_Faradey_1085.et";
 static const ResourceName PLATE = "{4825CC3E487EEB3A}Prefabs/Characters/Vests/Vest_TV102/Vest_TV102_OD_UA.et";
 static const ResourceName VEST_MG = "{86D26DDE3CA3694D}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_MG_OD_UA.et";
 static const ResourceName VEST_LEADER = "{47E3F46C6EEEAD7B}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_Comando_UA.et";
 // Belt boxes and RPG-7 rockets don't fit the TV102 pouches: a pack for those who have none.
 static const ResourceName PACK = "{16C7AD0508F53A9B}Prefabs/Items/Equipment/Backpacks/backpack_Wartech_BB102/Backpack_Wartech_BB102_OD.et";
 static const ResourceName PACK_RPG = "{0D39750E5695B9D8}Prefabs/Items/Equipment/Backpacks/Backpack_RPG_Gunner.et";

 protected static void Add(string pool, ResourceName prefab, int weight)
 {
  array<ResourceName> list;
  if (!s_Pools.Find(pool, list))
  {
   list = {};
   s_Pools.Set(pool, list);
  }
  for (int i = 0; i < weight; i++) list.Insert(prefab);
 }

 // Weighted pools (an entry appears as often as its weight). RHS Status Quo is loaded with
 // the UA mod (its dependency); vanilla entries are the donated/older stock.
 protected static void BuildPools()
 {
  if (s_Pools) return;
  s_Pools = new map<string, ref array<ResourceName>>();
  ResourceName ak74 = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
  ResourceName ak74rhs = "{6408F2C84AE5DBD0}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_RHSMag.et";
  ResourceName ak74n = "{6733EB7DB00E7067}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_RHSMag.et";
  ResourceName ak74m = "{FA8BE6075AFFBCA8}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M.et";
  ResourceName ak74mDot = "{1B22CB7855379872}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_1P63.et";
  ResourceName ak74mRail = "{46D829D1995F62D4}Prefabs/Weapons/Rifles/AK74M/variants/Rifle_AK74M_b10m_b19n_npz_rail_perst_aimpoint_high.et";
  ResourceName aks74u = "{BFEA719491610A45}Prefabs/Weapons/Rifles/AKS74U/Rifle_AKS74U.et";
  ResourceName aks74un = "{3FA38ECE65392FC3}Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN.et";
  ResourceName aks74unDot = "{2259092E586DD78B}Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN_b18_t1.et";
  ResourceName m4 = "{14BAB2D9F6F782D8}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_II.et";
  ResourceName m4Dot = "{90D8238D1D8D2DC5}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_RAS_Aimpoint.et";
  ResourceName m16a4 = "{7E9CD221A58D3ECB}Prefabs/Weapons/Rifles/M16/Rifle_M16A4.et";
  ResourceName m16a2 = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
  ResourceName hk416 = "{353D39126E677470}Prefabs/Weapons/Rifles/HK416A5/Rifle_HK416A5_Var1.et";

  Add("rifle_east", ak74, 3); Add("rifle_east", ak74rhs, 2); Add("rifle_east", ak74n, 1);
  Add("rifle_east", ak74m, 3); Add("rifle_east", ak74mRail, 1); Add("rifle_east", aks74u, 1);
  Add("rifle_west", m4, 3); Add("rifle_west", m16a2, 2); Add("rifle_west", m16a4, 2); Add("rifle_west", hk416, 1);

  Add("leader_east", ak74mDot, 2); Add("leader_east", ak74mRail, 2); Add("leader_east", ak74m, 1);
  Add("leader_west", m4Dot, 2); Add("leader_west", hk416, 1);

  Add("gl_east", "{63E8322E2ADD4AA7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_GP25.et", 2);
  Add("gl_east", "{F9DF0AA717A9A9A1}Prefabs/Weapons/Rifles/AK74M/Rifle_AK74M_GP25.et", 2);
  Add("gl_west", "{00D6DCF68C664480}Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_M203_BLOCK_II.et", 1);
  Add("gl_west", "{5A987A8A13763769}Prefabs/Weapons/Rifles/M16/Rifle_M16A2_M203.et", 1);

  Add("mg_east", "{A89BC9D55FFB4CD8}Prefabs/Weapons/MachineGuns/PKM/MG_PKM.et", 3);
  Add("mg_east", "{5D6226C0B4631749}Prefabs/Weapons/MachineGuns/PKM/MG_PKM_B51.et", 1);
  Add("mg_east", "{559290E2A7D633FE}Prefabs/Weapons/MachineGuns/PKM/MG_PKP_B51.et", 1);
  Add("mg_west", "{A87B0A22462E20B1}Prefabs/Weapons/MachineGuns/M240/MG_M240.et", 1);

  Add("ar_east", "{A7AF84C6C58BA3E8}Prefabs/Weapons/MachineGuns/RPK74/MG_RPK74.et", 2);
  Add("ar_east", "{4EF6890D756306D1}Prefabs/Weapons/MachineGuns/RPK74M/MG_RPK74M.et", 2);
  Add("ar_east", "{591519C23638AFD0}Prefabs/Weapons/MachineGuns/RPK74N/MG_RPK74N.et", 1);
  Add("ar_west", "{D2B48DEBEF38D7D7}Prefabs/Weapons/MachineGuns/M249/MG_M249.et", 1);

  Add("marksman_east", "{6415B7923DE28C1B}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO.et", 2);
  Add("marksman_east", "{C828F6D563E1A9BE}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO_7N14.et", 1);
  Add("marksman_west", "{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et", 1);

  Add("crew_east", aks74u, 2); Add("crew_east", aks74un, 2); Add("crew_east", aks74unDot, 1);
  Add("crew_west", m4, 1);

  // Launchers: RPG-7 alongside donated disposables (M72, RPG-75) and RPG-22.
  Add("lat_east", "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et", 2);
  Add("lat_east", "{D6BC90F8AE0AB8EF}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_pgo7_rhs.et", 1);
  Add("lat_east", "{722CE6FEC39EE896}Prefabs/Weapons/Launchers/RPG22/Launcher_RPG22.et", 1);
  Add("lat_west", "{9C5C20FB0E01E64F}Prefabs/Weapons/Launchers/M72/Launcher_M72A3.et", 2);
  Add("lat_west", "{7C45EC94C698246B}Prefabs/Weapons/Launchers/RPG75/Launcher_RPG75.et", 1);

  Add("helmet", "{EED31918A22454F9}Prefabs/Characters/HeadGear/Helmet_ECH/variants/ECH_M14_ua.et", 4);
  // Headgear_exfil_ua_m14 never fits the head slot (11 Oct test): left out.
  Add("helmet", "{0368162B6B9C319D}Prefabs/Characters/HeadGear/Helmet_Gentex_56/ua_gentex_rhs.et", 1);
  Add("helmet", "{87782D2671BAED67}Prefabs/Characters/HeadGear/Helmet_Gentex_56/ua_gentex_rhs_tipo2.et", 1);
  Add("helmet", "{62E461DF7D5F7A0C}Prefabs/Characters/HeadGear/Helmet_Gentex_56/ua_gentex_rhs_tipo3.et", 1);
  Add("helmet", "{5FD044E70833F33B}Prefabs/Characters/HeadGear/Helmet_Gentex_56/ua_gentex_rhs_tipo4.et", 1);

  Add("vest", "{74F6035104400897}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_rifleman_OD_UA.et", 2);
  Add("vest", "{E383053302D67DCA}Prefabs/Characters/Vests/Vest_TV102/variants/Vest_TV102_Basico_UA.et", 1);
 }

 static ResourceName Pick(string pool)
 {
  BuildPools();
  array<ResourceName> list;
  if (!s_Pools.Find(pool, list) || list.IsEmpty()) return "";
  return list[Math.RandomInt(0, list.Count())];
 }

 // East or west pool for a role ("rifle" -> rifle_east / rifle_west).
 static ResourceName PickSide(string role)
 {
  if (Math.RandomFloat01() < s_fWestShare) return Pick(role + "_west");
  return Pick(role + "_east");
 }

 static bool Active()
 {
  return s_bEnabled && Replication.IsServer() && ACR_FactionSelection.UkraineModLoaded();
 }

 static bool IsUA(IEntity soldier)
 {
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(soldier.FindComponent(FactionAffiliationComponent));
  return affiliation && affiliation.GetAffiliatedFaction() && affiliation.GetAffiliatedFaction().GetFactionKey() == "US";
 }

 // Called every second for every AI soldier and command unit: equips each UA soldier once.
 static void Check(IEntity soldier)
 {
  if (!soldier || !Active()) return;
  if (s_Done.Contains(soldier.GetID())) return;
  ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
  if (!character || !IsUA(soldier)) return;
  CharacterControllerComponent controller = character.GetCharacterController();
  if (!controller || controller.IsDead()) return;
  s_Done.Insert(soldier.GetID());
  if (Equip(soldier)) s_iEquipped++;
  else s_iFailed++;
 }

 static bool WasEquipped(IEntity soldier) { return soldier && s_Done.Contains(soldier.GetID()); }

 static string Leaf(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return "";
  return FilePath.StripExtension(FilePath.StripPath(entity.GetPrefabData().GetPrefabName()));
 }

 static ACR_EUARole RoleOf(string leaf, IEntity primary, IEntity launcher)
 {
  if (leaf.Contains("HeliPilot")) return ACR_EUARole.PILOT;
  if (leaf.EndsWith("_Crew") || leaf.Contains("HeliCrew")) return ACR_EUARole.CREW;
  if (leaf.Contains("Unarmed")) return ACR_EUARole.UNARMED;
  if (leaf.EndsWith("_SL") || leaf.EndsWith("_TL") || leaf.EndsWith("_PL") || leaf.EndsWith("_CC") || leaf.Contains("Officer") || leaf.Contains("Sergeant")) return ACR_EUARole.LEADER;
  if (launcher) return ACR_EUARole.LAT;
  if (!primary) return ACR_EUARole.RIFLEMAN;
  string weapon = Leaf(primary);
  BaseWeaponComponent component = BaseWeaponComponent.Cast(primary.FindComponent(BaseWeaponComponent));
  if (component && component.GetWeaponType() == EWeaponType.WT_MACHINEGUN)
  {
   if (weapon.Contains("M249")) return ACR_EUARole.AR;
   return ACR_EUARole.MG;
  }
  if ((component && component.GetWeaponType() == EWeaponType.WT_SNIPERRIFLE) || weapon.Contains("M21") || leaf.Contains("Sniper")) return ACR_EUARole.MARKSMAN;
  if (component)
  {
   array<BaseMuzzleComponent> muzzles = {};
   component.GetMuzzlesList(muzzles);
   if (muzzles.Count() > 1) return ACR_EUARole.GL;
  }
  return ACR_EUARole.RIFLEMAN;
 }

 static ResourceName PrimaryFor(ACR_EUARole role)
 {
  if (role == ACR_EUARole.LEADER) return PickSide("leader");
  if (role == ACR_EUARole.MG) return PickSide("mg");
  if (role == ACR_EUARole.AR) return PickSide("ar");
  if (role == ACR_EUARole.GL) return PickSide("gl");
  if (role == ACR_EUARole.MARKSMAN) return PickSide("marksman");
  if (role == ACR_EUARole.CREW) return PickSide("crew");
  if (role == ACR_EUARole.PILOT || role == ACR_EUARole.UNARMED) return "";
  return PickSide("rifle");
 }

 // Magazines carried for the main muzzle (one more is in the weapon).
 static int SpareMagazines(ACR_EUARole role, string weapon)
 {
  if (role == ACR_EUARole.MG) return 2;
  if (role == ACR_EUARole.AR)
  {
   if (weapon.Contains("M249")) return 2;
   return 5;
  }
  if (role == ACR_EUARole.CREW || role == ACR_EUARole.PILOT) return 3;
  if (role == ACR_EUARole.MARKSMAN) return 5;
  return 6;
 }

 protected static void FindWeapons(IEntity soldier, out IEntity primary, out IEntity launcher, out IEntity sidearm)
 {
  primary = null;
  launcher = null;
  sidearm = null;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weaponEntity : list)
  {
   if (!weaponEntity) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   EWeaponType type = weapon.GetWeaponType();
   if (type == EWeaponType.WT_ROCKETLAUNCHER) { if (!launcher) launcher = weaponEntity; }
   else if (type == EWeaponType.WT_HANDGUN) { if (!sidearm) sidearm = weaponEntity; }
   else if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_SNIPERRIFLE || type == EWeaponType.WT_GRENADELAUNCHER)
   {
    if (!primary) primary = weaponEntity;
   }
  }
 }

 // Take the worn item off one loadout area (its contents stay inside it) and put the new one on.
 protected static bool Swap(SCR_InventoryStorageManagerComponent inventory, SCR_CharacterInventoryStorageComponent loadout, typename area, ResourceName prefab, notnull array<IEntity> removed)
 {
  if (prefab.IsEmpty()) return false;
  IEntity worn = loadout.GetClothFromArea(area);
  if (worn)
  {
   if (worn.GetPrefabData() && worn.GetPrefabData().GetPrefabName() == prefab) return true;
   if (!inventory.TryRemoveItemFromStorage(worn, loadout)) return false;
  }
  if (inventory.TrySpawnPrefabToStorage(prefab, loadout))
  {
   if (worn) removed.Insert(worn);
   return true;
  }
  // The new item would not go on: put the old one back rather than leave the slot bare.
  if (worn && !inventory.TryInsertItemInStorage(worn, loadout)) removed.Insert(worn);
  return false;
 }

 // Into any storage that takes it (belt boxes may only fit a pack).
 static bool SpawnCarried(SCR_InventoryStorageManagerComponent inventory, ResourceName prefab)
 {
  if (inventory.TrySpawnPrefabToStorage(prefab)) return true;
  array<BaseInventoryStorageComponent> storages = {};
  inventory.GetStorages(storages);
  foreach (BaseInventoryStorageComponent storage : storages)
   if (storage && inventory.TrySpawnPrefabToStorage(prefab, storage)) return true;
  return false;
 }

 static bool IsAmmo(IEntity item)
 {
  if (!item) return false;
  if (item.FindComponent(BaseMagazineComponent)) return true;
  if (!item.GetPrefabData()) return false;
  string path = item.GetPrefabData().GetPrefabName();
  return path.Contains("/Weapons/Ammo/") || path.Contains("/Weapons/Magazines/") || path.Contains("/Weapons/Warheads/");
 }

 // Default magazines (or projectiles) of every reloadable muzzle the soldier now carries.
 protected static void Ammunition(IEntity soldier, notnull array<ResourceName> magazines, notnull array<BaseMuzzleComponent> muzzlesOut)
 {
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  if (!weapons) return;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  foreach (IEntity weaponEntity : list)
  {
   if (!weaponEntity) continue;
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   array<BaseMuzzleComponent> muzzles = {};
   weapon.GetMuzzlesList(muzzles);
   foreach (BaseMuzzleComponent muzzle : muzzles)
   {
    SCR_MuzzleInMagComponent inMag = SCR_MuzzleInMagComponent.Cast(muzzle);
    if (inMag && !inMag.CanBeReloaded()) continue;
    ResourceName magazine = muzzle.GetDefaultMagazineOrProjectileName();
    if (magazine.IsEmpty()) continue;
    magazines.Insert(magazine);
    muzzlesOut.Insert(muzzle);
   }
  }
 }

 static int CountPrefab(SCR_InventoryStorageManagerComponent inventory, ResourceName prefab)
 {
  array<IEntity> items = {};
  inventory.GetItems(items);
  int count;
  foreach (IEntity item : items)
   if (item && item.GetPrefabData() && item.GetPrefabData().GetPrefabName() == prefab) count++;
  return count;
 }

 static bool Equip(IEntity soldier)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  SCR_CharacterInventoryStorageComponent loadout = inventory.GetCharacterStorage();
  if (!loadout) return false;
  string leaf = Leaf(soldier);
  IEntity primary;
  IEntity launcher;
  IEntity sidearm;
  FindWeapons(soldier, primary, launcher, sidearm);
  ACR_EUARole role = RoleOf(leaf, primary, launcher);

  // Clothes. Items in the old clothes are moved over afterwards (old ammo is dropped).
  array<IEntity> removed = {};
  string clothFails;
  if (role != ACR_EUARole.PILOT)
  {
   if (!Swap(inventory, loadout, LoadoutJacketArea, JACKET, removed)) clothFails += " jacket";
   if (!Swap(inventory, loadout, LoadoutPantsArea, PANTS, removed)) clothFails += " pants";
   if (!Swap(inventory, loadout, LoadoutBootsArea, BOOTS, removed)) clothFails += " boots";
   ResourceName helmet = Pick("helmet");
   if (role != ACR_EUARole.CREW && !Swap(inventory, loadout, LoadoutHeadCoverArea, helmet, removed)) clothFails += " helmet:" + FilePath.StripPath(helmet);
   if (!Swap(inventory, loadout, LoadoutArmoredVestSlotArea, PLATE, removed)) clothFails += " plate";
   ResourceName vest = Pick("vest");
   if (role == ACR_EUARole.MG || role == ACR_EUARole.AR) vest = VEST_MG;
   else if (role == ACR_EUARole.LEADER) vest = VEST_LEADER;
   if (!Swap(inventory, loadout, LoadoutVestArea, vest, removed)) clothFails += " vest:" + FilePath.StripPath(vest);
  }

  // Weapons.
  ResourceName newPrimary = PrimaryFor(role);
  bool primaryOk = true;
  if (!newPrimary.IsEmpty())
  {
   if (primary) inventory.TryDeleteItem(primary);
   primaryOk = inventory.TrySpawnPrefabToStorage(newPrimary, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY);
  }
  ResourceName newLauncher;
  if (role == ACR_EUARole.LAT)
  {
   newLauncher = PickSide("lat");
   if (launcher) inventory.TryDeleteItem(launcher);
   if (!inventory.TrySpawnPrefabToStorage(newLauncher, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY)) primaryOk = false;
  }

  // A pack for belt boxes and rockets, only where the back is empty (medic/radio packs stay).
  if (!loadout.GetClothFromArea(LoadoutBackpackArea))
  {
   ResourceName pack;
   if (role == ACR_EUARole.MG || role == ACR_EUARole.AR) pack = PACK;
   else if (newLauncher.Contains("RPG7")) pack = PACK_RPG;
   if (!pack.IsEmpty() && !inventory.TrySpawnPrefabToStorage(pack, loadout)) clothFails += " pack";
  }

  // Ammunition that fits what is carried now; everything else that was in the old clothes moves over.
  array<ResourceName> keep = {};
  array<BaseMuzzleComponent> muzzles = {};
  Ammunition(soldier, keep, muzzles);
  int moved;
  foreach (IEntity old : removed)
  {
   BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(old.FindComponent(BaseInventoryStorageComponent));
   if (!storage) continue;
   array<IEntity> inside = {};
   storage.GetAll(inside);
   foreach (IEntity item : inside)
   {
    if (!item || (IsAmmo(item) && (!item.GetPrefabData() || !keep.Contains(item.GetPrefabData().GetPrefabName())))) continue;
    BaseInventoryStorageComponent target = inventory.FindStorageForItem(item);
    if (target && inventory.TryMoveItemToStorage(item, target)) moved++;
   }
  }
  foreach (IEntity old : removed) SCR_EntityHelper.DeleteEntityAndChildren(old);
  // Loose old ammunition still in the inventory (backpacks, character storage).
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
   if (IsAmmo(item) && item.GetPrefabData() && !keep.Contains(item.GetPrefabData().GetPrefabName())) inventory.TryDeleteItem(item);

  IEntity nowPrimary;
  IEntity nowLauncher;
  IEntity nowSidearm;
  FindWeapons(soldier, nowPrimary, nowLauncher, nowSidearm);
  string weaponLeaf = Leaf(nowPrimary);
  int spawned;
  foreach (int i, ResourceName magazine : keep)
  {
   // Spares on top of the loaded one: UGL grenades 6, RPG-7 rockets 1, sidearm 2.
   int wanted = 2;
   IEntity owner = muzzles[i].GetOwner();
   if (muzzles[i].GetMuzzleType() == EMuzzleType.MT_UGLMuzzle) wanted = 6;
   else if (owner == nowPrimary) wanted = SpareMagazines(role, weaponLeaf);
   else if (owner == nowLauncher) wanted = 1;
   int have = CountPrefab(inventory, magazine);
   for (int n = have; n < wanted; n++)
   {
    if (!SpawnCarried(inventory, magazine))
    {
     clothFails += " ammo:" + FilePath.StripPath(magazine);
     break;
    }
    spawned++;
   }
  }

  CharacterControllerComponent controller = ChimeraCharacter.Cast(soldier).GetCharacterController();
  if (controller && nowPrimary) controller.TryEquipRightHandItem(nowPrimary, EEquipItemType.EEquipTypeWeapon, false);

  PrintFormat("[ACR] UA LOADOUT %1 role=%2 weapon=%3 launcher=%4 mags+%5 moved=%6 failed:%7", leaf, typename.EnumToString(ACR_EUARole, role), weaponLeaf, Leaf(nowLauncher), spawned, moved, clothFails);
  return clothFails.IsEmpty() && primaryOk;
 }
}
