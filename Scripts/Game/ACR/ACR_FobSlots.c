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
	bool m_bAdopted;          // a free-form composition inside the FOB counted as this slot
	bool m_bBlocked;          // cached spot check: empty slot, or the next tier of a built one
	string m_sBlockedReason;
	float m_fNextSpotCheck;
	bool m_bAwaitingClear;    // finished, waiting for vehicles to leave the spot
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

	// Preset layout round the command post (x east, z north, metres). Footprints at the
	// largest tier (helipad 21 m, large living area 22 m, heavy bay 17 m) do not overlap
	// each other or the HQ (10 m); every centre stays inside the 40 m FOB circle.
	static vector SlotOffset(int index)
	{
		switch (index)
		{
			case SLOT_HOSPITAL: return Vector(36, 0, -9);
			case SLOT_REPAIR: return Vector(13, 0, -35);
			case SLOT_AMMO: return Vector(-13, 0, -16);
			case SLOT_FUEL: return Vector(-31, 0, -20);
			case SLOT_HELIPAD: return Vector(4, 0, 35);
			case SLOT_RELAY: return Vector(-15, 0, -35);
			case SLOT_BARRACKS: return Vector(-36, 0, 13);
			case SLOT_DEPOT: return Vector(33, 0, 17);
		}
		return vector.Zero;
	}

	// Each composition faces the command post.
	static float SlotYaw(int index)
	{
		vector offset = SlotOffset(index);
		float yaw = Math.Atan2(-offset[0], -offset[2]) * Math.RAD2DEG;
		if (yaw > 180) yaw -= 360;
		if (yaw < -180) yaw += 360;
		return yaw;
	}

	static ACR_EStructure KindAt(int index)
	{
		switch (index)
		{
			case SLOT_HOSPITAL: return ACR_EStructure.HOSPITAL;
			case SLOT_REPAIR: return ACR_EStructure.REPAIR;
			case SLOT_AMMO: return ACR_EStructure.AMMO;
			case SLOT_FUEL: return ACR_EStructure.FUEL;
			case SLOT_HELIPAD: return ACR_EStructure.HELIPAD;
			case SLOT_RELAY: return ACR_EStructure.RELAY;
			case SLOT_BARRACKS: return ACR_EStructure.BARRACKS;
			case SLOT_DEPOT: return ACR_EStructure.DEPOT;
		}
		return ACR_EStructure.NONE;
	}

	static float SlotRadius(int index, int tier = 0)
	{
		return ACR_Structures.KindRadius(KindAt(index), tier);
	}

	static vector SlotPosition(vector center, int index)
	{
		vector pos = center + SlotOffset(index);
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		return pos;
	}

	static void InitSlots(ACR_Base fob)
	{
		if (!fob) return;
		fob.m_aFobSlots = {};
		for (int i = 0; i < SLOT_COUNT; i++)
			fob.m_aFobSlots.Insert(CreateSlot(i, KindAt(i), SlotName(i, 0, fob.m_sOwner), SlotCost(i, 0), SlotDuration(i, 0), SlotOffset(i), SlotYaw(i)));
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

	// FIA has no large living area or medium maintenance bay: tier 1 keeps the small model.
	static bool SameModel(int index, int tier, string factionKey)
	{
		if (tier <= 0 || factionKey != "FIA") return false;
		return index == SLOT_REPAIR || index == SLOT_BARRACKS;
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

	static string SlotName(int index, int tier = 0, string factionKey = "")
	{
		string name = BaseSlotName(index, tier);
		if (SameModel(index, tier, factionKey)) name += " (small model)";
		return name;
	}

	protected static string BaseSlotName(int index, int tier)
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

	static bool s_bSkipSpotChecksForTest; // legacy fixtures on unprepared test terrain

	// What a spot check ignores: the FOB's own HQ and the slot's current building.
	static void IgnoreFor(ACR_Base base, ACR_FobSlot slot, notnull array<IEntity> ignore)
	{
		ignore.Clear();
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager && manager.m_Structures)
		{
			IEntity hq = manager.m_Structures.HqEntityOf(base);
			if (hq) ignore.Insert(hq);
		}
		if (slot && slot.m_Entity) ignore.Insert(slot.m_Entity);
	}

	static bool SlotSpotClear(vector center, int index, int tier, array<IEntity> ignore, out string reason)
	{
		reason = "";
		if (s_bSkipSpotChecksForTest) return true;
		return ACR_FobBuild.FootprintClear(SlotPosition(center, index), SlotRadius(index, tier), ignore, reason);
	}

	static bool CheckSlot(ACR_Base base, ACR_FobSlot slot, int tier, out string reason)
	{
		array<IEntity> ignore = {};
		IgnoreFor(base, slot, ignore);
		return SlotSpotClear(base.GetOrigin(), slot.m_iSlotIndex, tier, ignore, reason);
	}

	static bool UsedBySlot(ACR_Base base, IEntity entity)
	{
		if (!base || !base.m_aFobSlots || !entity) return false;
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
			if (slot.m_Entity == entity) return true;
		return false;
	}

	static bool AnyBuilding(ACR_Base base)
	{
		if (!base || !base.m_aFobSlots) return false;
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
			if (slot.m_eState == ACR_EFobSlotState.BUILDING) return true;
		return false;
	}

	protected static void ResetSlot(ACR_FobSlot slot)
	{
		slot.m_eState = ACR_EFobSlotState.EMPTY;
		slot.m_Entity = null;
		slot.m_sOwner = "";
		slot.m_fProgress = 0;
		slot.m_iTier = 0;
		slot.m_iTargetTier = 0;
		slot.m_bAdopted = false;
		slot.m_bAwaitingClear = false;
		slot.m_fNextSpotCheck = 0;
		slot.m_iCost = SlotCost(slot.m_iSlotIndex, 0);
		slot.m_sName = SlotName(slot.m_iSlotIndex, 0);
		slot.m_fDuration = SlotDuration(slot.m_iSlotIndex, 0);
	}

	// Smart slots: a service composition already standing inside the FOB counts as built.
	static void Reconcile(ACR_Base base)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!base || !base.m_aFobSlots || !manager || !manager.m_Structures) return;
		array<IEntity> found = {};
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (slot.m_eState != ACR_EFobSlotState.EMPTY) continue;
			manager.m_Structures.StructuresAt(base, slot.m_eKind, base.m_sOwner, found);
			foreach (IEntity candidate : found)
			{
				if (UsedBySlot(base, candidate)) continue;
				slot.m_eState = ACR_EFobSlotState.BUILT;
				slot.m_Entity = candidate;
				slot.m_sOwner = base.m_sOwner;
				slot.m_iTier = ACR_Structures.TierOf(candidate);
				if (slot.m_iTier > MaxTier(slot.m_iSlotIndex)) slot.m_iTier = MaxTier(slot.m_iSlotIndex);
				slot.m_iTargetTier = slot.m_iTier;
				slot.m_bAdopted = true;
				slot.m_fProgress = SlotDuration(slot.m_iSlotIndex, slot.m_iTier);
				slot.m_sName = SlotName(slot.m_iSlotIndex, slot.m_iTier, base.m_sOwner);
				PrintFormat("[ACR] FOB %1: existing %2 at %3 counts as its %4 slot", base.m_sName, ACR_Structures.KindName(slot.m_eKind), candidate.GetOrigin(), slot.m_sName);
				break;
			}
		}
	}

	static void TransferOwner(ACR_Base base, string faction)
	{
		if (!base || !base.m_aFobSlots) return;
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (slot.m_eState != ACR_EFobSlotState.BUILT || !slot.m_Entity) continue;
			slot.m_sOwner = faction;
			slot.m_sName = SlotName(slot.m_iSlotIndex, slot.m_iTier, faction);
		}
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
		if (base.m_bHqLost)
		{
			refusal = "FOB HQ destroyed: rebuild it with an Establish FOB order";
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

		string spotWhy;
		if (!CheckSlot(base, slot, targetTier, spotWhy))
		{
			slot.m_bBlocked = true;
			slot.m_sBlockedReason = spotWhy;
			refusal = string.Format("%1 spot blocked: %2", SlotName(slotIndex, targetTier, key), spotWhy);
			return "";
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
		slot.m_sName = SlotName(slotIndex, targetTier, key);
		slot.m_bAwaitingClear = false;
		slot.m_bBlocked = false;
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
		if (dt > 5.0) dt = 2.0;
		if (dt > 0) Advance(base, dt);
		RefreshSpots(base, now);
	}

	// Cached spot checks for the panel (every 10 s per slot).
	static void RefreshSpots(ACR_Base base, float now)
	{
		if (!base || !base.m_aFobSlots || base.m_bRetired) return;
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (now < slot.m_fNextSpotCheck) continue;
			slot.m_fNextSpotCheck = now + 10000;
			int tier = -1;
			if (slot.m_eState == ACR_EFobSlotState.EMPTY) tier = 0;
			else if (IsBuiltForOwner(slot, base.m_sOwner) && slot.m_iTier < MaxTier(slot.m_iSlotIndex)) tier = slot.m_iTier + 1;
			if (tier < 0)
			{
				slot.m_bBlocked = false;
				slot.m_sBlockedReason = "";
				continue;
			}
			string why;
			slot.m_bBlocked = !CheckSlot(base, slot, tier, why);
			slot.m_sBlockedReason = why;
		}
	}

	static void Advance(ACR_Base base, float dt)
	{
		if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_aFobSlots) return;

		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			bool relayLost = slot.m_eKind == ACR_EStructure.RELAY && slot.m_eState == ACR_EFobSlotState.BUILT && !ACR_Structures.RelayAntenna(slot.m_Entity);
			bool builtGone = slot.m_eState == ACR_EFobSlotState.BUILT && !slot.m_Entity;
			bool baseGone = slot.m_eState == ACR_EFobSlotState.BUILDING && slot.m_iTargetTier > 0 && !slot.m_Entity;
			if (relayLost || builtGone || baseGone || (slot.m_Entity && ACR_Logistics.Health(slot.m_Entity) <= 0))
			{
				PrintFormat("[ACR] FOB %1: structure %2 destroyed, slot reset", base.m_sName, slot.m_sName);
				if (slot.m_Entity)
				{
					SCR_CampaignBuildingCompositionComponent lostComp = SCR_CampaignBuildingCompositionComponent.Cast(slot.m_Entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
					if (lostComp) ACR_Structures.s_Registered.RemoveItem(lostComp);
					SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
				}
				ResetSlot(slot);
				continue;
			}
			if (slot.m_eState == ACR_EFobSlotState.BUILDING && !slot.m_sOwner.IsEmpty() && slot.m_sOwner != base.m_sOwner)
			{
				// Captured mid-build: the work is lost, a finished lower tier stays.
				if (slot.m_Entity && ACR_Logistics.Health(slot.m_Entity) > 0)
				{
					slot.m_eState = ACR_EFobSlotState.BUILT;
					slot.m_iTargetTier = slot.m_iTier;
					slot.m_sOwner = base.m_sOwner;
					slot.m_sName = SlotName(slot.m_iSlotIndex, slot.m_iTier, base.m_sOwner);
					slot.m_fProgress = 0;
				}
				else ResetSlot(slot);
			}
		}

		Reconcile(base);

		bool contested = base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), base.m_sOwner);
		if (contested || base.m_bHqLost) return;

		bool hasEngineer = HasEngineer(base);
		float speedMult = 1.0;
		if (hasEngineer) speedMult = 2.0;

		foreach (ACR_FobSlot s : base.m_aFobSlots)
		{
			if (s.m_eState != ACR_EFobSlotState.BUILDING) continue;
			if (s.m_fProgress < s.m_fDuration) s.m_fProgress += dt * speedMult;
			if (s.m_fProgress >= s.m_fDuration)
			{
				s.m_fProgress = s.m_fDuration;
				TryComplete(base, s);
			}
		}
	}

	// Vehicles parked on a finished spot hold the composition back until they move.
	static bool VehicleOnSpot(vector position, float radius)
	{
		array<IEntity> vehicles = {};
		ACR_Vehicles.All(vehicles);
		foreach (IEntity vehicle : vehicles)
			if (vehicle && ACR_Logistics.Health(vehicle) > 0 && vector.DistanceXZ(vehicle.GetOrigin(), position) < radius) return true;
		return false;
	}

	static void TryComplete(ACR_Base base, ACR_FobSlot slot)
	{
		vector pos = SlotPosition(base.GetOrigin(), slot.m_iSlotIndex);
		float radius = SlotRadius(slot.m_iSlotIndex, slot.m_iTargetTier);
		if (VehicleOnSpot(pos, radius))
		{
			if (!slot.m_bAwaitingClear) PrintFormat("[ACR] FOB %1: %2 finished, waiting for vehicles to clear its spot", base.m_sName, slot.m_sName);
			slot.m_bAwaitingClear = true;
			return;
		}
		slot.m_bAwaitingClear = false;
		string why;
		if (!CheckSlot(base, slot, slot.m_iTargetTier, why))
		{
			// Something was built on the spot meanwhile: refund and step back.
			SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(base.m_FobSupply);
			if (store) store.SetResourceValue(Math.Min(store.GetMaxResourceValue(), ACR_FobSupply.Stock(base.m_FobSupply) + slot.m_iCost));
			PrintFormat("[ACR] FOB %1: %2 spot blocked at completion (%3), %4 supplies refunded", base.m_sName, slot.m_sName, why, slot.m_iCost);
			slot.m_bBlocked = true;
			slot.m_sBlockedReason = why;
			if (slot.m_Entity)
			{
				slot.m_eState = ACR_EFobSlotState.BUILT;
				slot.m_iTargetTier = slot.m_iTier;
				slot.m_sName = SlotName(slot.m_iSlotIndex, slot.m_iTier, base.m_sOwner);
				slot.m_fProgress = SlotDuration(slot.m_iSlotIndex, slot.m_iTier);
			}
			else ResetSlot(slot);
			return;
		}
		CompleteUpgrade(base, slot);
	}

	static void CompleteUpgrade(ACR_Base base, ACR_FobSlot slot)
	{
		slot.m_eState = ACR_EFobSlotState.BUILT;
		slot.m_sOwner = base.m_sOwner;
		slot.m_fProgress = slot.m_fDuration;
		slot.m_iTier = slot.m_iTargetTier;
		slot.m_bAdopted = false;
		slot.m_bAwaitingClear = false;

		if (slot.m_Entity)
		{
			SCR_CampaignBuildingCompositionComponent oldComp = SCR_CampaignBuildingCompositionComponent.Cast(slot.m_Entity.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (oldComp) ACR_Structures.s_Registered.RemoveItem(oldComp);
			SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
			slot.m_Entity = null;
		}

		vector pos = SlotPosition(base.GetOrigin(), slot.m_iSlotIndex);
		ACR_FobBuild.FellTrees(pos, SlotRadius(slot.m_iSlotIndex, slot.m_iTier));

		ResourceName prefab = PrefabFor(slot.m_eKind, base.m_sOwner, slot.m_iTier);
		IEntity ent = null;
		Resource resource = Resource.Load(prefab);
		if (resource.IsValid())
		{
			EntitySpawnParams spawnParams = new EntitySpawnParams();
			spawnParams.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(slot.m_fAngleDeg, 0, 0), spawnParams.Transform);
			spawnParams.Transform[3] = pos;
			ent = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
		}
		if (ent)
		{
			ent.SetYawPitchRoll(Vector(slot.m_fAngleDeg, 0, 0));
			slot.m_Entity = ent;
			SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(ent.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (comp)
			{
				comp.ACR_SetFaction(base.m_sOwner);
				comp.ACR_SetTier(slot.m_iTier);
				comp.ACR_SetFobProvider(base.m_FobSupply);
				comp.ACR_MarkBuildCharged();
				comp.ACR_SetCompositionSpawned(true);
			}
		}
		PrintFormat("[ACR] FOB %1: completed upgrade %2 (tier %3) at %4 (entity=%5)", base.m_sName, slot.m_sName, slot.m_iTier, pos, ent != null);
	}

	// Panel encoding (one int per slot): -1 none; 0 empty; 1 empty, spot blocked;
	// 10..109 building tier 0 (10 + %); 110..209 building tier 1; 300 built tier 0
	// (301 = next tier's spot blocked); 400 built tier 1.
	static int FeedState(ACR_Base base, int index)
	{
		if (!base || !base.m_aFobSlots || index < 0 || index >= base.m_aFobSlots.Count()) return -1;
		ACR_FobSlot slot = base.m_aFobSlots[index];
		if (IsBuiltForOwner(slot, base.m_sOwner))
		{
			if (slot.m_iTier > 0) return 400;
			if (slot.m_bBlocked && MaxTier(index) > 0) return 301;
			return 300;
		}
		if (slot.m_eState == ACR_EFobSlotState.BUILDING && (slot.m_sOwner.IsEmpty() || slot.m_sOwner == base.m_sOwner))
		{
			int pct = Math.ClampInt((int)((slot.m_fProgress / slot.m_fDuration) * 100), 0, 99);
			if (slot.m_iTargetTier == 0) return 10 + pct;
			return 110 + pct;
		}
		if (slot.m_bBlocked) return 1;
		return 0;
	}

	static string Describe(ACR_Base base)
	{
		if (!base || !base.m_aFobSlots) return "";
		string text = "";
		bool paused = base.m_bContested || base.m_bHqLost || ACR_FobBuild.IsContested(base.GetOrigin(), base.m_sOwner);
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
				if (paused) cont = " (paused)";
				else if (slot.m_bAwaitingClear) cont = " (move vehicles off its spot)";
				text += string.Format("\n  %1 (%2%%%3)", slot.m_sName, pct, cont);
			}
			else if (slot.m_bBlocked)
			{
				text += string.Format("\n  %1: spot blocked (%2)", slot.m_sName, slot.m_sBlockedReason);
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
			ResetSlot(slot);
		}
	}
}
