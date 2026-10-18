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
	static const ResourceName HOSPITAL_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_USSR_01.et";
	static const ResourceName HOSPITAL_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_FIA_01.et";

	static const ResourceName REPAIR_US = "{7AD06D9CF2672AC5}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et";
	static const ResourceName REPAIR_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_USSR_01.et";
	static const ResourceName REPAIR_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_FIA_01.et";

	static const ResourceName AMMO_US = "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et";
	static const ResourceName AMMO_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_USSR_01.et";
	static const ResourceName AMMO_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_FIA_01.et";

	static const ResourceName FUEL_US = "{6D2A35DA74224780}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_US_01.et";
	static const ResourceName FUEL_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_USSR_01.et";
	static const ResourceName FUEL_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_FIA_01.et";

	static const ResourceName HELIPAD_US = "{D3DDB76548C16BDB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_US_01.et";
	static const ResourceName HELIPAD_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_USSR_01.et";
	static const ResourceName HELIPAD_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_FIA_01.et";

	static const ResourceName RELAY_US = "{F7DC8BB193BCAF44}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_US_01.et";
	static const ResourceName RELAY_USSR = "{0E9BAF79B9D47993}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_USSR_01.et";
	static const ResourceName RELAY_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Antenna_S_FIA_01.et";

	static const ResourceName BARRACKS_US = "{DB7F01C68EACBEC0}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_US_01.et";
	static const ResourceName BARRACKS_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_USSR_01.et";
	static const ResourceName BARRACKS_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_FIA_01.et";

	static const ResourceName DEPOT_US = "{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et";
	static const ResourceName DEPOT_USSR = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_USSR_01.et";
	static const ResourceName DEPOT_FIA = "PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_FIA_01.et";

	static void InitSlots(ACR_Base fob)
	{
		if (!fob) return;
		fob.m_aFobSlots = {};
		fob.m_aFobSlots.Insert(CreateSlot(0, ACR_EStructure.HOSPITAL, "Field Hospital", 250, 40.0, Vector(0, 0, 18), 0));
		fob.m_aFobSlots.Insert(CreateSlot(1, ACR_EStructure.REPAIR, "Vehicle Repair", 350, 45.0, Vector(15, 0, 15), 45));
		fob.m_aFobSlots.Insert(CreateSlot(2, ACR_EStructure.AMMO, "Ammo Storage", 200, 30.0, Vector(18, 0, 0), 90));
		fob.m_aFobSlots.Insert(CreateSlot(3, ACR_EStructure.FUEL, "Fuel Storage", 200, 30.0, Vector(15, 0, -15), 135));
		fob.m_aFobSlots.Insert(CreateSlot(4, ACR_EStructure.HELIPAD, "Helipad", 300, 45.0, Vector(0, 0, -22), 180));
		fob.m_aFobSlots.Insert(CreateSlot(5, ACR_EStructure.RELAY, "Radio Relay", 250, 30.0, Vector(-15, 0, -15), 225));
		fob.m_aFobSlots.Insert(CreateSlot(6, ACR_EStructure.BARRACKS, "Barracks", 275, 40.0, Vector(-18, 0, 0), 270));
		fob.m_aFobSlots.Insert(CreateSlot(7, ACR_EStructure.DEPOT, "Supply Depot", 200, 30.0, Vector(-15, 0, 15), 315));
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
		return slot;
	}

	static ResourceName PrefabFor(ACR_EStructure kind, string factionKey)
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
				if (isEast) return REPAIR_USSR;
				if (isFia) return REPAIR_FIA;
				return REPAIR_US;
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
				if (isEast) return BARRACKS_USSR;
				if (isFia) return BARRACKS_FIA;
				return BARRACKS_US;
			case ACR_EStructure.DEPOT:
				if (isEast) return DEPOT_USSR;
				if (isFia) return DEPOT_FIA;
				return DEPOT_US;
		}
		return "";
	}

	static string SlotName(int index)
	{
		switch (index)
		{
			case 0: return "Field Hospital";
			case 1: return "Vehicle Repair";
			case 2: return "Ammo Storage";
			case 3: return "Fuel Storage";
			case 4: return "Helipad";
			case 5: return "Radio Relay";
			case 6: return "Barracks";
			case 7: return "Supply Depot";
		}
		return "Slot";
	}

	static int SlotCost(int index)
	{
		switch (index)
		{
			case 0: return 250;
			case 1: return 350;
			case 2: return 200;
			case 3: return 200;
			case 4: return 300;
			case 5: return 250;
			case 6: return 275;
			case 7: return 200;
		}
		return 200;
	}

	static float SlotDuration(int index)
	{
		switch (index)
		{
			case 0: return 40.0;
			case 1: return 45.0;
			case 2: return 30.0;
			case 3: return 30.0;
			case 4: return 45.0;
			case 5: return 30.0;
			case 6: return 40.0;
			case 7: return 30.0;
		}
		return 30.0;
	}

	static bool HasEngineer(ACR_Base base)
	{
		if (!base) return false;
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return false;
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
			if (truck)
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
		if (slot.m_eState == ACR_EFobSlotState.BUILT)
		{
			refusal = string.Format("%1 is already built", slot.m_sName);
			return "";
		}

		int stock = ACR_FobSupply.Stock(base.m_FobSupply);
		if (stock < slot.m_iCost)
		{
			refusal = string.Format("Needs %1 supplies (FOB has %2)", slot.m_iCost, stock);
			return "";
		}

		if (!ACR_FobSupply.SpendBuild(base.m_FobSupply, slot.m_iCost))
		{
			refusal = "Failed to deduct supplies";
			return "";
		}

		slot.m_eState = ACR_EFobSlotState.BUILDING;
		slot.m_fProgress = 0;
		slot.m_Entity = null;
		PrintFormat("[ACR] FOB %1: started upgrade %2 (cost=%3, duration=%4 s)", base.m_sName, slot.m_sName, slot.m_iCost, (int)slot.m_fDuration);
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
			if (slot.m_eState == ACR_EFobSlotState.BUILT)
			{
				if (!slot.m_Entity || ACR_Logistics.Health(slot.m_Entity) <= 0)
				{
					PrintFormat("[ACR] FOB %1: structure %2 destroyed, slot reset", base.m_sName, slot.m_sName);
					slot.m_eState = ACR_EFobSlotState.EMPTY;
					slot.m_Entity = null;
					slot.m_fProgress = 0;
				}
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
		slot.m_fProgress = slot.m_fDuration;

		vector pos = base.GetOrigin() + slot.m_vOffset;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);

		ResourceName prefab = PrefabFor(slot.m_eKind, base.m_sOwner);
		IEntity ent = ACR_Manager.SpawnPrefab(prefab, pos);
		if (ent)
		{
			vector angles = Vector(slot.m_fAngleDeg, 0, 0);
			ent.SetAngles(angles);
			slot.m_Entity = ent;
			SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(ent.FindComponent(SCR_CampaignBuildingCompositionComponent));
			if (comp)
			{
				comp.ACR_SetFobProvider(base.m_FobSupply);
				comp.ACR_MarkBuildCharged();
				comp.ACR_SetCompositionSpawned(true);
			}
		}
		PrintFormat("[ACR] FOB %1: completed upgrade %2 at %3 (entity=%4)", base.m_sName, slot.m_sName, pos, ent != null);
	}

	static string Describe(ACR_Base base)
	{
		if (!base || !base.m_aFobSlots) return "";
		string text = "";
		foreach (ACR_FobSlot slot : base.m_aFobSlots)
		{
			if (slot.m_eState == ACR_EFobSlotState.BUILT)
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
				SCR_EntityHelper.DeleteEntityAndChildren(slot.m_Entity);
				slot.m_Entity = null;
			}
			slot.m_eState = ACR_EFobSlotState.EMPTY;
			slot.m_fProgress = 0;
		}
	}
}
