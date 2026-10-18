// C&C-style forward base building (Stage 2; Roadmap Item 7).
// Manages preset upgrade slots around FOB CP centers, upgrade ordering,
// supply deduction, build progression with engineer speedup, contest pause,
// scripted placement of vanilla compositions, and slot reset on destruction.

enum ACR_EFobSlotState
{
	EMPTY = 0,
	BUILDING = 1,
	BUILT = 2
}

class ACR_FobSlot : Managed
{
	int m_iSlotIndex;
	ACR_EStructure m_eKind;
	string m_sName;
	int m_iCost;
	float m_fDuration;
	vector m_vOffset;
	float m_fAngleDeg;

	ACR_EFobSlotState m_eState;
	float m_fProgress;
	IEntity m_Entity;
	int m_iTier;
	int m_iTargetTier;
	string m_sOwner;
}

class ACR_FobSlots : Managed
{
	static const int SLOT_COUNT = 8;
	static const int SLOT_HOSPITAL = 0;
	static const int SLOT_REPAIR = 1;
	static const int SLOT_AMMO = 2;
	static const int SLOT_FUEL = 3;
	static const int SLOT_HELIPAD = 4;
	static const int SLOT_RELAY = 5;
	static const int SLOT_BARRACKS = 6;
	static const int SLOT_DEPOT = 7;

	static const ResourceName HOSPITAL_US = "{D0281DAF0B131743}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_US_01.et";
	static const ResourceName HOSPITAL_USSR = "{505F78512FE8B697}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_USSR_01.et";
	static const ResourceName HOSPITAL_FIA = "{71FF67164192F291}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_FIA_01.et";

	static const ResourceName REPAIR_S_US = "{A0FEA3E718F5B605}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_Conflict_US_01.et";
	static const ResourceName REPAIR_S_USSR = "{5ACECF41C167567D}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_Conflict_USSR_01.et";
	static const ResourceName REPAIR_S_FIA = "{DA1AF79DD1C79E83}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_FIA_01.et";

	static const ResourceName REPAIR_M_US = "{7AD06D9CF2672AC5}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et";
	static const ResourceName REPAIR_M_USSR = "{6676FBB212B337D6}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_USSR_01.et";
	static const ResourceName REPAIR_M_FIA = "{DA1AF79DD1C79E83}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_FIA_01.et";

	static const ResourceName AMMO_US = "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et";
	static const ResourceName AMMO_USSR = "{CD9004C6229D900B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_USSR_01.et";
	static const ResourceName AMMO_FIA = "{CDE540ACD9415078}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_FIA_01.et";

	static const ResourceName FUEL_US = "{6D2A35DA74224780}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_US_01.et";
	static const ResourceName FUEL_USSR = "{C2D040BB21F15A20}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_USSR_01.et";
	static const ResourceName FUEL_FIA = "{B6CF358D07FFC99F}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_FIA_01.et";

	static const ResourceName HELIPAD_US = "{D3DDB76548C16BDB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_US_01.et";
	static const ResourceName HELIPAD_USSR = "{C4114DAE8FDCE080}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_USSR_01.et";
	static const ResourceName HELIPAD_FIA = "{D3DDB76548C16BDB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_US_01.et";

	static const ResourceName RELAY_US = "{F7DC8BB193BCAF44}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_US_01.et";
	static const ResourceName RELAY_USSR = "{0E9BAF79B9D47993}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_USSR_01.et";
	static const ResourceName RELAY_FIA = "{C731CC083B954825}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_FIA_01.et";

	static const ResourceName BARRACKS_S_US = "{DB7F01C68EACBEC0}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_US_01.et";
	static const ResourceName BARRACKS_S_USSR = "{BE3988DF6E3CD0B2}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_USSR_01.et";
	static const ResourceName BARRACKS_S_FIA = "{7394E0251880F29A}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_FIA_01.et";

	static const ResourceName BARRACKS_L_US = "{74B740B56C82C0CF}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_LivingArea_L_US_01.et";
	static const ResourceName BARRACKS_L_USSR = "{85E7D977EFAC11CB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_LivingArea_L_USSR_01.et";
	static const ResourceName BARRACKS_L_FIA = "{7394E0251880F29A}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_FIA_01.et";

	static const ResourceName DEPOT_US = "{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et";
	static const ResourceName DEPOT_USSR = "{EB63BCC0AA844B17}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_USSR_01.et";
	static const ResourceName DEPOT_FIA = "{E8B6339148792C0E}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_FIA_01.et";

	static void InitSlots(ACR_Base fob)
	{
		if (!fob) return;
		fob.m_aFobSlots = {};
		fob.m_aFobSlots.Insert(CreateSlot(0, ACR_EStructure.HOSPITAL, "Field Hospital", 250, 40.0, Vector(0, 0, 18), 0));
		fob.m_aFobSlots.Insert(CreateSlot(1, ACR_EStructure.REPAIR, "Light Maintenance", 150, 30.0, Vector(15, 0, 15), 45));
		fob.m_aFobSlots.Insert(CreateSlot(2, ACR_EStructure.AMMO, "Ammo Storage", 325, 30.0, Vector(18, 0, 0), 90));
		fob.m_aFobSlots.Insert(CreateSlot(3, ACR_EStructure.FUEL, "Fuel Storage", 50, 30.0, Vector(15, 0, -15), 135));
		fob.m_aFobSlots.Insert(CreateSlot(4, ACR_EStructure.HELIPAD, "Helipad", 750, 45.0, Vector(0, 0, -22), 180));
		fob.m_aFobSlots.Insert(CreateSlot(5, ACR_EStructure.RELAY, "Radio Relay", 250, 30.0, Vector(-15, 0, -15), 225));
		fob.m_aFobSlots.Insert(CreateSlot(6, ACR_EStructure.BARRACKS, "Barracks (Small)", 275, 35.0, Vector(-18, 0, 0), 270));
		fob.m_aFobSlots.Insert(CreateSlot(7, ACR_EStructure.DEPOT, "Supply Depot", 175, 30.0, Vector(-15, 0, 15), 315));
	}

	protected static ACR_FobSlot CreateSlot(int index, ACR_EStructure kind, string name, int cost, float duration, vector offset, float angle)
	{
		ACR_FobSlot slot = new ACR_FobSlot();
		slot.m_iSlotIndex = index;
		slot.m_eKind = kind;
		slot.m_sName = name;
		slot.m_iCost = cost;
		slot.m_fDuration = duration;
		slot.m_vOffset = offset;
		slot.m_fAngleDeg = angle;
		slot.m_eState = ACR_EFobSlotState.EMPTY;
		slot.m_fProgress = 0;
		slot.m_Entity = null;
		slot.m_iTier = 0;
		slot.m_iTargetTier = 0;
		return slot;
	}

	static ResourceName PrefabFor(ACR_EStructure kind, string factionKey, int tier = 0)
	{
		bool isEast = false;
		if (factionKey == "USSR" || factionKey == "RHS_AFRF") isEast = true;
		bool isFia = false;
		if (factionKey == "FIA") isFia = true;
		switch (kind)
		{
			case ACR_EStructure.HOSPITAL:
				if (isEast) return HOSPITAL_USSR;
				if (isFia) return HOSPITAL_FIA;
				return HOSPITAL_US;
			case ACR_EStructure.REPAIR:
				if (tier > 0)
				{
					if (isEast) return REPAIR_M_USSR;
					if (isFia) return REPAIR_M_FIA;
					return REPAIR_M_US;
				}
				if (isEast) return REPAIR_S_USSR;
				if (isFia) return REPAIR_S_FIA;
				return REPAIR_S_US;
			case ACR_EStructure.AMMO:
				if (isEast) return AMMO_USSR;
				if (isFia) return AMMO_FIA;
				return AMMO_US;
			case ACR_EStructure.FUEL:
				if (isEast) return FUEL_USSR;
				if (isFia) return FUEL_FIA;
				return FUEL_US;
			case ACR_EStructure.HELIPAD:
				if (isEast) return HELIPAD_USSR;
				if (isFia) return HELIPAD_FIA;
				return HELIPAD_US;
			case ACR_EStructure.RELAY:
				if (isEast) return RELAY_USSR;
				if (isFia) return RELAY_FIA;
				return RELAY_US;
			case ACR_EStructure.BARRACKS:
				if (tier > 0)
				{
					if (isEast) return BARRACKS_L_USSR;
					if (isFia) return BARRACKS_L_FIA;
					return BARRACKS_L_US;
				}
				if (isEast) return BARRACKS_S_USSR;
				if (isFia) return BARRACKS_S_FIA;
				return BARRACKS_S_US;
			case ACR_EStructure.DEPOT:
				if (isEast) return DEPOT_USSR;
				if (isFia) return DEPOT_FIA;
				return DEPOT_US;
		}
		return "";
	}

	static int MaxTier(int index)
	{
		if (index == SLOT_REPAIR || index == SLOT_BARRACKS) return 1;
		return 0;
	}

	static string SlotName(int index, int tier = 0)
	{
		switch (index)
		{
			case 0: return "Field Hospital";
			case 1:
				if (tier > 0) return "Heavy Maintenance";
				return "Light Maintenance";
			case 2: return "Ammo Storage";
			case 3: return "Fuel Storage";
			case 4: return "Helipad";
			case 5: return "Radio Relay";
			case 6:
				if (tier > 0) return "Barracks (Large)";
				return "Barracks (Small)";
			case 7: return "Supply Depot";
		}
		return "Slot";
	}

	static int SlotCost(int index, int tier = 0)
	{
		switch (index)
		{
			case 0: return 250;
			case 1:
				if (tier > 0) return 150;
				return 150;
			case 2: return 325;
			case 3: return 50;
			case 4: return 750;
			case 5: return 250;
			case 6:
				if (tier > 0) return 200;
				return 275;
			case 7: return 175;
		}
		return 200;
	}

	static float SlotDuration(int index, int tier = 0)
	{
		switch (index)
		{
			case 0: return 40.0;
			case 1:
				if (tier > 0) return 45.0;
				return 30.0;
			case 2: return 30.0;
			case 3: return 30.0;
			case 4: return 45.0;
			case 5: return 30.0;
			case 6:
				if (tier > 0) return 45.0;
				return 35.0;
			case 7: return 30.0;
		}
		return 30.0;
	}

	static bool IsBuiltForOwner(ACR_FobSlot slot, string owner)
	{
		if (!slot || slot.m_eState != ACR_EFobSlotState.BUILT) return false;
		if (!slot.m_sOwner.IsEmpty() && slot.m_sOwner != owner) return false;
		return true;
	}

	static bool HasEngineer(ACR_Base base)
	{
		if (!base) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return false;
		array<IEntity> vehicles = {};
		ACR_Vehicles.All(vehicles);
		foreach (IEntity candidate : vehicles)
		{
			if (!candidate || ACR_Logistics.Health(candidate) <= 0) continue;
			if ((ACR_Logistics.ServiceOf(candidate) & ACR_EService.CONSTRUCTION) == 0) continue;
			if (vector.DistanceXZ(candidate.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
			FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(candidate.FindComponent(FactionAffiliationComponent));
			if (aff && aff.GetAffiliatedFaction() && aff.GetAffiliatedFaction().GetFactionKey() == base.m_sOwner)
			{
				return true;
			}
		}
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!squad || !squad.GetFaction()) continue;
			if (squad.GetFaction().GetFactionKey() != base.m_sOwner) continue;
			IEntity truck = squad.ConstructionTruck();
			if (truck && ACR_Logistics.Health(truck) > 0)
			{
				if (vector.DistanceXZ(truck.GetOrigin(), base.GetOrigin()) <= base.m_fRadius)
				{
					return true;
				}
			}
			if (truck || ACR_SapperDemo.IsSapperSquad(squad))
			{
				array<AIAgent> agents = {};
				squad.GetAgents(agents);
				foreach (AIAgent agent : agents)
				{
					if (!agent) continue;
					IEntity body = agent.GetControlledEntity();
					if (body && ACR_Respawn.IsAlive(body))
					{
						if (vector.DistanceXZ(body.GetOrigin(), base.GetOrigin()) <= base.m_fRadius)
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	static string OrderUpgrade(ACR_Manager manager, SCR_PlayerController pc, string baseLabel, int slotIndex, out string refusal)
	{
		refusal = "";
		if (!Replication.IsServer() || !manager || !pc)
		{
			refusal = "Server unavailable";
			return "";
		}
		if (!ACR_Setup.Started())
		{
			refusal = "Wait for the match to start";
			return "";
		}
		if (!manager.IsCommander(pc.GetPlayerId()))
		{
			refusal = "Take command to manage FOB upgrades";
			return "";
		}
		IEntity body = pc.GetControlledEntity();
		if (!body || ACR_Logistics.Health(body) <= 0)
		{
			refusal = "Wait until you have spawned";
			return "";
		}
		Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
		if (!faction)
		{
			refusal = "You have no faction";
			return "";
		}
		string key = faction.GetFactionKey();
		ACR_Base base = manager.m_Requisition.FindSpawnBase(key, baseLabel);
		if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply)
		{
			refusal = "Select one of your built FOBs";
			return "";
		}
		return OrderUpgradeBase(manager, key, base, slotIndex, refusal);
	}

	static string OrderUpgradeBase(ACR_Manager manager, string key, ACR_Base base, int slotIndex, out string refusal)
	{
		refusal = "";
		if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_FobSupply)
		{
			refusal = "Invalid FOB";
			return "";
		}
		if (base.m_sOwner != key)
		{
			refusal = "FOB belongs to another faction";
			return "";
		}
		if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), key))
		{
			refusal = "FOB is contested";
			return "";
		}
		if (!base.m_aFobSlots) InitSlots(base);
		if (slotIndex < 0 || slotIndex >= base.m_aFobSlots.Count())
		{
			refusal = "Invalid upgrade slot";
			return "";
		}

		foreach (ACR_FobSlot s : base.m_aFobSlots)
		{
			if (s.m_eState == ACR_EFobSlotState.BUILDING)
			{
				refusal = string.Format("Already building %1", s.m_sName);
				return "";
			}
		}

		ACR_FobSlot slot = base.m_aFobSlots[slotIndex];
		int targetTier = 0;
		if (IsBuiltForOwner(slot, key))
		{
			if (slot.m_iTier < MaxTier(slotIndex))
			{
				targetTier = slot.m_iTier + 1;
			}
			else
			{
				refusal = string.Format("%1 is already built", slot.m_sName);
				return "";
			}
		}

		int cost = SlotCost(slotIndex, targetTier);
		int stock = ACR_FobSupply.Stock(base.m_FobSupply);
		if (stock < cost)
		{
			refusal = string.Format("Needs %1 supplies (FOB has %2)", cost, stock);
			return "";
		}

		if (!ACR_FobSupply.SpendBuild(base.m_FobSupply, cost))
		{
			refusal = "Failed to deduct supplies";
			return "";
		}

		slot.m_eState = ACR_EFobSlotState.BUILDING;
		slot.m_sOwner = key;
		slot.m_fProgress = 0;
		slot.m_iTargetTier = targetTier;
		slot.m_iCost = cost;
		slot.m_fDuration = SlotDuration(slotIndex, targetTier);
		slot.m_sName = SlotName(slotIndex, targetTier);
		PrintFormat("[ACR] FOB %1: started upgrade %2 tier %3 (cost=%4, duration=%5 s)", base.m_sName, slot.m_sName, targetTier, slot.m_iCost, (int)slot.m_fDuration);
		return string.Format("Started building %1", slot.m_sName);
	}

	static void Tick(ACR_Base base)
	{
		if (!base || !base.m_bBuilt || base.m_bRetired) return;
		if (!base.m_aFobSlots) InitSlots(base);

		float now = GetGame().GetWorld().GetWorldTime();
		if (base.m_fFobSlotsLastTick <= 0) base.m_fFobSlotsLastTick = now;
		float dt = (now - base.m_fFobSlotsLastTick) / 1000.0;
		base.m_fFobSlotsLastTick = now;
		if (dt <= 0) return;
		if (dt > 5.0) dt = 2.0;

		Advance(base, dt);
	}

	static void Advance(ACR_Base base, float dt)
	{
		if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_aFobSlots) return;

		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if ((slot.m_eState == ACR_EFobSlotState.BUILT && !slot.m_Entity) || (slot.m_Entity && ACR_Logistics.Health(slot.m_Entity) <= 0))
			{
				PrintFormat("[ACR] FOB %1: structure %2 destroyed, slot reset", base.m_sName, slot.m_sName);
				slot.m_eState = ACR_EFobSlotState.EMPTY;
				slot.m_Entity = null;
				slot.m_sOwner = "";
				slot.m_fProgress = 0;
				slot.m_iTier = 0;
				slot.m_iTargetTier = 0;
				slot.m_iCost = SlotCost(slot.m_iSlotIndex, 0);
				slot.m_sName = SlotName(slot.m_iSlotIndex, 0);
				slot.m_fDuration = SlotDuration(slot.m_iSlotIndex, 0);
				continue;
			}
			if (slot.m_eState == ACR_EFobSlotState.BUILDING && !slot.m_sOwner.IsEmpty() && slot.m_sOwner != base.m_sOwner)
			{
				if (slot.m_Entity && ACR_Logistics.Health(slot.m_Entity) > 0)
				{
					slot.m_eState = ACR_EFobSlotState.BUILT;
					slot.m_iTargetTier = slot.m_iTier;
					slot.m_sName = SlotName(slot.m_iSlotIndex, slot.m_iTier);
				}
				else
				{
					slot.m_eState = ACR_EFobSlotState.EMPTY;
					slot.m_iTier = 0;
					slot.m_iTargetTier = 0;
					slot.m_sOwner = "";
					slot.m_iCost = SlotCost(slot.m_iSlotIndex, 0);
					slot.m_sName = SlotName(slot.m_iSlotIndex, 0);
					slot.m_fDuration = SlotDuration(slot.m_iSlotIndex, 0);
				}
				slot.m_fProgress = 0;
			}
		}

		bool contested = base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), base.m_sOwner);
		if (contested) return;

		bool hasEngineer = HasEngineer(base);
		float speedMult = 1.0;
		if (hasEngineer) speedMult = 2.0;

		foreach (ACR_FobSlot s : base.m_aFobSlots)
		{
			if (s.m_eState == ACR_EFobSlotState.BUILDING)
			{
				s.m_fProgress += dt * speedMult;
				if (s.m_fProgress >= s.m_fDuration)
				{
					CompleteUpgrade(base, s);
				}
			}
		}
	}

	static void CompleteUpgrade(ACR_Base base, ACR_FobSlot slot)
	{
		slot.m_eState = ACR_EFobSlotState.BUILT;
		slot.m_sOwner = base.m_sOwner;
		slot.m_fProgress = slot.m_fDuration;
		slot.m_iTier = slot.m_iTargetTier;

		if (slot.m_Entity)
		{
			SCR_CampaignBuildingCompositionComponent oldComp = SCR_CampaignBuildingCompositionComponent.Cast(slot.m_Entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (oldComp) ACR_Structures.s_Registered.RemoveItem(oldComp);
			SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
			slot.m_Entity = null;
		}

		vector pos = base.GetOrigin() + slot.m_vOffset;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		ACR_FobBuild.FellTrees(pos, 14.0);

		ResourceName prefab = PrefabFor(slot.m_eKind, base.m_sOwner, slot.m_iTier);
		IEntity ent = null;
		Resource resource = Resource.Load(prefab);
		if (resource.IsValid())
		{
			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(slot.m_fAngleDeg, 0, 0), spawnParams.Transform);
			spawnParams.Transform[3] = pos;
			ent = GetGame().SpawnEntityPrefabLocal(resource, null, spawnParams);
		}
		if (ent)
		{
			ent.SetYawPitchRoll(Vector(slot.m_fAngleDeg, 0, 0));
			slot.m_Entity = ent;
			SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(ent.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (comp)
			{
				comp.ACR_SetFaction(base.m_sOwner);
				comp.ACR_SetFobProvider(base.m_FobSupply);
				comp.ACR_MarkBuildCharged();
				comp.ACR_SetCompositionSpawned(true);
			}
		}
		PrintFormat("[ACR] FOB %1: completed upgrade %2 (tier %3) at %4 (entity=%5)", base.m_sName, slot.m_sName, slot.m_iTier, pos, ent != null);
	}

	static string Describe(ACR_Base base)
	{
		if (!base || !base.m_aFobSlots) return "";
		string text = "";
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (IsBuiltForOwner(slot, base.m_sOwner))
			{
				text += "\n  " + slot.m_sName + " (built)";
			}
			else if (slot.m_eState == ACR_EFobSlotState.BUILDING)
			{
				int pct = (int)((slot.m_fProgress / slot.m_fDuration) * 100);
				string cont = "";
				if (base.m_bContested) cont = " (paused)";
				text += string.Format("\n  %1 (%2%%%3)", slot.m_sName, pct, cont);
			}
		}
		if (!text.IsEmpty()) text = "\nUpgrades:" + text;
		return text;
	}

	static void Cleanup(ACR_Base base)
	{
		if (!base || !base.m_aFobSlots) return;
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (slot.m_Entity)
			{
				SCR_CampaignBuildingCompositionComponent oldComp = SCR_CampaignBuildingCompositionComponent.Cast(slot.m_Entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
				if (oldComp) ACR_Structures.s_Registered.RemoveItem(oldComp);
				SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
				slot.m_Entity = null;
			}
			slot.m_eState = ACR_EFobSlotState.EMPTY;
			slot.m_sOwner = "";
			slot.m_fProgress = 0;
			slot.m_iTier = 0;
			slot.m_iTargetTier = 0;
			slot.m_iCost = SlotCost(slot.m_iSlotIndex, 0);
			slot.m_sName = SlotName(slot.m_iSlotIndex, 0);
			slot.m_fDuration = SlotDuration(slot.m_iSlotIndex, 0);
		}
	}
}
