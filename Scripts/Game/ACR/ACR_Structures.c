// Player-built structures (4 Oct; Jacob: built HQ structures should count in the mode).
// Compositions built with a construction truck (the game's free-roam building) register here
// once finished; what they do depends on the composition:
//  - Field Hospital: soldiers of its side nearby are healed (after a short stay).
//  - Vehicle Maintenance: vehicles nearby are slowly repaired.
//  - Ammo Storage: soldiers nearby get magazines/launchers, vehicles' guns are refilled.
//  - Fuel Storage: vehicles nearby are slowly refuelled.
//    (These use local FOB supplies by default; the free-services option restores unlimited
//    field points. AI squads in need also walk to points with available stock.)
//  - Helipad: a pad for support helicopters, and where bought helicopters appear.
//  - Headquarters / Player Hub: a forward base ("FOB n") where purchases and reinforcements
//    deploy, unless it stands inside an existing base. Hidden from the enemy until found.
// Built anywhere; owned by the builder's side. Removed when the structure is gone.
// Supply depots inside a friendly FOB expand its storage once, without adding supplies.
enum ACR_EStructure
{
 NONE,
 HOSPITAL,
 REPAIR,
 AMMO,
 FUEL,
 HELIPAD,
 HQ,
 DEPOT,
 RELAY
}

modded class SCR_CampaignBuildingCompositionComponent
{
 [RplProp()]
 protected RplId m_ACRFobProvider = RplId.Invalid();

 void ACR_SetFobProvider(IEntity provider)
 {
  RplComponent component = RplComponent.Cast(provider.FindComponent(RplComponent));
  if (component) m_ACRFobProvider = component.Id();
  Replication.BumpMe();
 }

 IEntity ACR_FobProvider()
 {
  RplComponent component = RplComponent.Cast(Replication.FindItem(m_ACRFobProvider));
  if (component) return component.GetEntity();
  return null;
 }

	protected bool m_bACRBuildCharged;
	protected bool m_bACRBuildRejected;

	// Charge paid FOB stock when the blueprint is placed, before another player can
	// spend it. Vanilla's plain game mode never charges the campaign budget itself.
	void ACR_ChargeFobBuild()
	{
		IEntity newOwner = GetProviderEntity();
		if (!Replication.IsServer() || m_bACRBuildCharged || m_bACRBuildRejected || !newOwner) return;
		if (!ACR_FobProviderComponent.Cast(newOwner.FindComponent(ACR_FobProviderComponent))) return;
		if (ACR_FobSupply.SpendBuild(newOwner, GetCompositionCost())) m_bACRBuildCharged = true;
		else
		{
			m_bACRBuildRejected = true;
			GetGame().GetCallqueue().CallLater(ACR_DeleteRejectedBuild, 0, false);
		}
	}

	void ACR_DeleteRejectedBuild()
	{
		if (GetOwner()) SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}

	bool ACR_BuildRejected() { return m_bACRBuildRejected; }

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (GetGame().InPlayMode() && Replication.IsServer()) GetOnCompositionSpawned().Insert(ACR_OnCompositionSpawned);
		if (GetGame().InPlayMode() && ACR_Structures.KindOf(owner) != ACR_EStructure.NONE) ACR_Structures.s_Registered.Insert(this);
	}

	void ACR_OnCompositionSpawned(bool spawned)
	{
		if (!spawned || m_bACRBuildCharged || m_bACRBuildRejected || !Replication.IsServer()) return;
		if (GetProviderEntity() && ACR_FobProviderComponent.Cast(GetProviderEntity().FindComponent(ACR_FobProviderComponent)))
		{
			ACR_ChargeFobBuild();
			return;
		}
		if (ACR_Structures.ChargeBuild(this, GetProviderEntity())) m_bACRBuildCharged = true;
	}

	int ACR_CompositionCost()
	{
		return GetCompositionCost();
	}

	void ACR_SetProviderForTest(IEntity provider)
	{
		SetProviderEntity(provider);
	}
}

// Entering building mode also assigns providers to existing nearby compositions.
// Charge new placements here so opening the menu cannot bill or delete existing buildings.
modded class SCR_CampaignBuildingPlacingEditorComponent
{
 override protected void SetProviderAndBuilder(notnull SCR_CampaignBuildingCompositionComponent compositionComponent)
 {
  super.SetProviderAndBuilder(compositionComponent);
  compositionComponent.ACR_ChargeFobBuild();
 }
}

class ACR_Structure : Managed
{
 SCR_CampaignBuildingCompositionComponent m_Composition;
 ACR_EStructure m_eKind;
 string m_sFaction;
 vector m_vPos;
 ACR_Helipad m_Pad;
 ACR_Base m_Fob;
 IEntity m_Entity;
}

class ACR_Structures : Managed
{
 static ref array<SCR_CampaignBuildingCompositionComponent> s_Registered = {};
 static const float FOB_RADIUS = 40;
 static const int TICK_MS = 2000;
 protected static int s_iFobs;

 protected ACR_Manager m_Manager;
 protected ref array<ref ACR_Structure> m_Active = {};
 protected float m_fNextTick;

 void ACR_Structures(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 static ACR_EStructure KindOfName(string prefab)
 {
  string name = prefab;
  name.ToLower();
  if (name.Contains("fieldhospital")) return ACR_EStructure.HOSPITAL;
  if (name.Contains("vehiclemaintenance")) return ACR_EStructure.REPAIR;
  if (name.Contains("ammostorage")) return ACR_EStructure.AMMO;
  if (name.Contains("fuelstorage")) return ACR_EStructure.FUEL;
  if (name.Contains("helipad_l")) return ACR_EStructure.HELIPAD;
  if (name.Contains("headquarters") || name.Contains("playerhub")) return ACR_EStructure.HQ;
  if (name.Contains("supplycache_") || name.Contains("supplystorage_")) return ACR_EStructure.DEPOT;
  if (name.Contains("antenna_s_")) return ACR_EStructure.RELAY;
  return ACR_EStructure.NONE;
 }

 static ACR_EStructure KindOf(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return ACR_EStructure.NONE;
  return KindOfName(entity.GetPrefabData().GetPrefabName());
 }

 static string KindName(ACR_EStructure kind)
 {
  switch (kind)
  {
   case ACR_EStructure.HOSPITAL: return "field hospital";
   case ACR_EStructure.REPAIR: return "vehicle maintenance";
   case ACR_EStructure.AMMO: return "ammo storage";
   case ACR_EStructure.FUEL: return "fuel storage";
   case ACR_EStructure.HELIPAD: return "helipad";
   case ACR_EStructure.HQ: return "headquarters";
   case ACR_EStructure.DEPOT: return "supply depot";
   case ACR_EStructure.RELAY: return "radio relay";
  }
  return "structure";
 }

 // ACR's worlds use the plain game mode, which does not deduct construction supplies.
 // Charge the vanilla editable budget once the composition reports that it was built.
 static bool ChargeBuild(SCR_CampaignBuildingCompositionComponent composition, IEntity provider)
 {
  if (!composition || !provider) return false;
  IEntity truck = provider;
  for (int i = 0; truck && i < 8; i++)
  {
   if ((ACR_Logistics.ServiceOf(truck) & ACR_EService.CONSTRUCTION) != 0) break;
   truck = truck.GetParent();
  }
  if (!truck || (ACR_Logistics.ServiceOf(truck) & ACR_EService.CONSTRUCTION) == 0) return false;

  IEntity owner = composition.GetOwner();
  if (!owner || !owner.GetPrefabData()) return false;
  int supplyCost = composition.ACR_CompositionCost();
  if (supplyCost <= 0) return false;

  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
  float loads = supplyCost / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
  if (!manager.m_Logistics.Use(truck, loads))
  {
   PrintFormat("[ACR] BUILD %1 needs %2 supplies; construction truck has %3", FilePath.StripPath(owner.GetPrefabData().GetPrefabName()), supplyCost, manager.m_Logistics.StockOf(truck) * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION);
   return false;
  }
  PrintFormat("[ACR] BUILD %1 charged %2 supplies (%3 loads)", FilePath.StripPath(owner.GetPrefabData().GetPrefabName()), supplyCost, loads);
  return true;
 }

 // Logistics service flags of a kind (0 = not a service point).
 static int ServiceOf(ACR_EStructure kind)
 {
  switch (kind)
  {
   case ACR_EStructure.HOSPITAL: return ACR_EService.MEDICAL;
   case ACR_EStructure.REPAIR: return ACR_EService.REPAIR;
   case ACR_EStructure.AMMO: return ACR_EService.AMMO;
   case ACR_EStructure.FUEL: return ACR_EService.FUEL;
  }
  return 0;
 }

 // The builder's side, else the side in the composition's name (..._US_..., ..._USSR_...).
 static string FactionOf(SCR_CampaignBuildingCompositionComponent composition)
 {
  int builder = composition.GetBuilderId();
  if (builder > 0)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(builder);
   if (faction) return faction.GetFactionKey();
  }
  IEntity owner = composition.GetOwner();
  if (!owner || !owner.GetPrefabData()) return "";
  string name = owner.GetPrefabData().GetPrefabName();
  name = FilePath.StripExtension(FilePath.StripPath(name));
  array<string> parts = {};
  name.Split("_", parts, true);
  FactionManager factions = GetGame().GetFactionManager();
  foreach (string part : parts)
   if (factions && factions.GetFactionByKey(part)) return part;
  return "";
 }

 bool IsActive(SCR_CampaignBuildingCompositionComponent composition)
 {
  foreach (ACR_Structure s : m_Active) if (s.m_Composition == composition) return true;
  return false;
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextTick) return;
  m_fNextTick = now + TICK_MS;
  // Gone (dismantled, destroyed): undo.
  for (int i = m_Active.Count() - 1; i >= 0; i--)
  {
   ACR_Structure s = m_Active[i];
   if (s.m_Fob) ACR_FobSupply.Tick(s.m_Fob);
   if (s.m_Composition && s.m_Entity) continue;
   Deactivate(s);
   m_Active.RemoveOrdered(i);
  }
  for (int r = s_Registered.Count() - 1; r >= 0; r--)
  {
   SCR_CampaignBuildingCompositionComponent composition = s_Registered[r];
   if (!composition || !composition.GetOwner()) { s_Registered.Remove(r); continue; }
   if (composition.ACR_BuildRejected() || !composition.IsCompositionSpawned() || IsActive(composition)) continue;
   Activate(composition);
  }
  foreach (ACR_Structure active : m_Active)
   if (active.m_Fob) UpdateCapacity(active.m_Fob);
 }

 void UpdateCapacity(ACR_Base base)
 {
  bool depot;
  foreach (ACR_Structure structure : m_Active)
  {
   if (!structure.m_Entity || !structure.m_Composition || structure.m_eKind != ACR_EStructure.DEPOT || structure.m_sFaction != base.m_sOwner) continue;
   if (vector.DistanceXZ(structure.m_vPos, base.GetOrigin()) <= base.m_fRadius) depot = true;
  }
  int capacity = ACR_FobSupply.CAPACITY;
  if (depot) capacity += ACR_LogisticsRules.DepotBonus();
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!store) return;
  if (store.GetMaxResourceValue() != capacity) store.SetMaxResourceValue(capacity);
  if (store.GetResourceValue() > capacity) store.SetResourceValue(capacity);
 }

 // A completed relay composition must still contain its working antenna.
 // Read live entities so removal/movement closes the gate before the next tick.
 static IEntity RelayAntenna(IEntity entity)
 {
  if (!entity || ACR_Logistics.Health(entity) <= 0) return null;
  if (entity.FindComponent(ScriptedRadioComponent)) return entity;
  for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
  {
   IEntity antenna = RelayAntenna(child);
   if (antenna) return antenna;
  }
  return null;
 }

 bool HasRelay(ACR_Base base)
 {
  if (!base || base.m_bRetired) return false;
  foreach (ACR_Structure structure : m_Active)
  {
   if (structure.m_eKind != ACR_EStructure.RELAY || structure.m_sFaction != base.m_sOwner) continue;
   if (!structure.m_Entity || !structure.m_Composition || structure.m_Composition.ACR_BuildRejected() || !structure.m_Composition.IsCompositionSpawned()) continue;
   if (vector.DistanceXZ(structure.m_Entity.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
   if (RelayAntenna(structure.m_Entity)) return true;
  }
  return false;
 }

 protected void Activate(SCR_CampaignBuildingCompositionComponent composition)
 {
  ACR_Structure s = new ACR_Structure();
  s.m_Composition = composition;
  s.m_Entity = composition.GetOwner();
  s.m_eKind = KindOf(s.m_Entity);
  s.m_sFaction = FactionOf(composition);
  s.m_vPos = s.m_Entity.GetOrigin();
  m_Active.Insert(s);
  if (s.m_sFaction.IsEmpty())
  {
   PrintFormat("[ACR] STRUCTURE %1 at %2: no side, ignored", KindName(s.m_eKind), s.m_vPos);
   return;
  }
  int service = ServiceOf(s.m_eKind);
  if (service != 0)
  {
   if (!m_Manager.m_Logistics) m_Manager.m_Logistics = new ACR_Logistics(m_Manager);
   m_Manager.m_Logistics.AddPoint(s.m_Entity, service, s.m_sFaction);
  }
  if (s.m_eKind == ACR_EStructure.HELIPAD)
  {
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   s.m_Entity.GetWorldTransform(parameters.Transform);
   s.m_Pad = ACR_Helipad.Cast(GetGame().SpawnEntity(ACR_Helipad, GetGame().GetWorld(), parameters));
   if (s.m_Pad) s.m_Pad.m_sFaction = s.m_sFaction;
  }
  if (s.m_eKind == ACR_EStructure.HQ) s.m_Fob = MakeFob(s);
  string where = "";
  if (s.m_Fob) where = " = " + s.m_Fob.m_sName;
  else if (s.m_Pad && s.m_Pad.Base()) where = " (pad of " + s.m_Pad.Base().m_sName + ")";
  PrintFormat("[ACR] STRUCTURE %1 %2 built at %3%4", s.m_sFaction, KindName(s.m_eKind), s.m_vPos, where);
 }

 protected ACR_Base MakeFob(ACR_Structure s)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (vector.DistanceXZ(base.GetOrigin(), s.m_vPos) <= base.m_fRadius + FOB_RADIUS) return null; // inside a base already
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = s.m_vPos;
  ACR_Base fob = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), parameters));
  if (!fob) return null;
  s_iFobs++;
  fob.m_sName = string.Format("FOB %1", s_iFobs);
  fob.m_sOwner = s.m_sFaction;
  fob.m_fScore = 100;
  fob.m_iGarrison = 0;
  fob.m_iGarrisonLeft = 0;
  fob.m_iValue = 0;
  fob.m_bIncome = false;
  fob.m_bHidden = true;
  fob.m_bAllowSpawn = true;
  fob.m_bAllowReinforce = true;
  fob.m_fRadius = FOB_RADIUS;
  fob.m_bBuilt = true;
  ACR_FobSupply.Create(fob, s.m_Entity);
  return fob;
 }

 protected void Deactivate(ACR_Structure s)
 {
  if (m_Manager.m_Logistics && s.m_Entity) m_Manager.m_Logistics.RemovePoint(s.m_Entity);
  if (m_Manager.m_Logistics) m_Manager.m_Logistics.PrunePoints();
  if (s.m_Pad) s.m_Pad.m_bGone = true;
  if (s.m_Fob) s.m_Fob.Retire();
  PrintFormat("[ACR] STRUCTURE %1 %2 at %3 is gone", s.m_sFaction, KindName(s.m_eKind), s.m_vPos);
 }

 int CountActive(ACR_EStructure kind, string faction)
 {
  int count;
  foreach (ACR_Structure s : m_Active) if (s.m_eKind == kind && s.m_sFaction == faction && s.m_Entity) count++;
  return count;
 }

 ACR_Structure Find(IEntity entity)
 {
  foreach (ACR_Structure s : m_Active) if (s.m_Entity == entity) return s;
  return null;
 }
}

// Built ammo/vehicle compositions carry vanilla self-resupply actions that log a script error on
// a dedicated server when they find no storage manager (vanilla then just skips the check). Skip
// it quietly instead: same behaviour, no false script error in logs and test runs.
modded class SCR_ResupplySelfSupportStationAction
{
 override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
 {
  if (m_bCheckIfItemInStorage && pOwnerEntity)
  {
   bool found = pOwnerEntity.FindComponent(InventoryStorageManagerComponent) != null;
   if (!found && m_bAllowGetStorageFromParent && pOwnerEntity.GetParent())
    found = pOwnerEntity.GetParent().FindComponent(InventoryStorageManagerComponent) != null;
   if (!found) m_bCheckIfItemInStorage = false;
  }
  super.Init(pOwnerEntity, pManagerComponent);
	}
}

// Vanilla resupply stations log an error when their game-mode catalog manager is not
// ready at their first delayed init. They can still use their configured fallback cost.
modded class SCR_BaseItemSupportStationComponent
{
	protected override bool InitValidSetup()
	{
		m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		return super.InitValidSetup();
	}

	protected override int GetSupplyAmountAction(IEntity actionOwner, IEntity actionUser, SCR_BaseUseSupportStationAction action)
	{
		if (!AreSuppliesEnabled()) return 0;
		SCR_BaseItemHolderSupportStationAction itemHolder = SCR_BaseItemHolderSupportStationAction.Cast(action);
		if (!itemHolder) return 0;

		if (!m_EntityCatalogManager) m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!m_EntityCatalogManager)
			return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);

		SCR_EntityCatalogEntry catalogEntry = m_EntityCatalogManager.GetEntryWithPrefabFromAnyCatalog(EEntityCatalogType.ITEM, itemHolder.GetItemPrefab(), GetFaction());
		if (!catalogEntry) return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);
		SCR_ArsenalItem arsenalData = SCR_ArsenalItem.Cast(catalogEntry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenalData) return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);

		return Math.ClampInt(arsenalData.GetSupplyCost(m_eSupplyCostType) + m_iBaseSupplyCostOnUse, 0, int.MAX);
	}
}

// Preserve the resupply component's own range validation while allowing its catalog
// manager to become available after entity initialization.
modded class SCR_ResupplySupportStationComponent
{
	protected override bool InitValidSetup()
	{
		if (UsesRange())
		{
			Print("'SCR_ResupplySupportStationComponent' does not support range. Make sure m_fRange is set to -1", LogLevel.ERROR);
			return false;
		}

		if (!m_EntityCatalogManager) m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!m_EntityCatalogManager) return true;
		return super.InitValidSetup();
	}
}
