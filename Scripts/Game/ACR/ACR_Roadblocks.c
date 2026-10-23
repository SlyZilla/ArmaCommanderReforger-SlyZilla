// Roadblocks / Observation Posts (aca-f23).
// Infantry squads construct fortified checkpoints across road corridors.
// Includes dragon's teeth / sandbag breastworks, dynamic navmesh updates,
// and a mounted heavy machine gun turret manned by the constructing squad.
// Squads remain mobile (Option 1): redeploying leaves the physical roadblock in place.

class ACR_Roadblock : Managed
{
	vector m_vPos;
	float m_fYaw;
	string m_sFaction;
	IEntity m_Entity;
	IEntity m_Turret;
	string m_sBuilderSquad;
	float m_fSpawnedAt;
	bool m_bAlerted;
}

class ACR_Roadblocks : Managed
{
	static ref array<ref ACR_Roadblock> s_aRoadblocks = {};

	static const ResourceName PREFAB_US = "{55D508179EF5E64A}Prefabs/Compositions/Slotted/SlotRoadMedium/Checkpoint_M_US_01.et";
	static const ResourceName PREFAB_USSR = "{7C85836D444E3797}Prefabs/Compositions/Slotted/SlotRoadMedium/Checkpoint_M_USSR_01.et";

	static const int ROADBLOCK_COST = 20;
	static const int DISMANTLE_REFUND = 10;
	static const float BUILD_DURATION_S = 45.0;
	static const float MAX_ROAD_DIST = 20.0;
	static const float MIN_BASE_DIST = 120.0;
	static const float MIN_ROADBLOCK_DIST = 180.0;
	static const float CP_RADIUS = 15.0;

	static ResourceName PrefabFor(string factionKey)
	{
		if (factionKey == "USSR" || factionKey.Contains("RU") || factionKey.Contains("SLA"))
			return PREFAB_USSR;
		return PREFAB_US;
	}

	static bool CanOrder(ACR_Squad squad, out string refusal)
	{
		if (!squad || squad.CountAlive() <= 0)
		{
			refusal = "squad is unavailable";
			return false;
		}
		if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB)
		{
			refusal = "already building";
			return false;
		}
		if (squad.IsVehicle())
		{
			refusal = "vehicles cannot construct roadblocks";
			return false;
		}
		string factionKey = "";
		if (squad.GetFaction()) factionKey = squad.GetFaction().GetFactionKey();
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager || manager.m_Bases.GetRP(factionKey) < ROADBLOCK_COST)
		{
			refusal = string.Format("not enough RP (%1 needed)", ROADBLOCK_COST);
			return false;
		}
		return true;
	}

	static bool ValidateSite(vector pos, out vector outRoadPos, out float outYaw, out string refusal, string faction = "", vector facing = vector.Zero)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !aiWorld.GetRoadNetworkManager())
		{
			refusal = "road network unavailable";
			return false;
		}

		BaseRoad road;
		float roadDist;
		aiWorld.GetRoadNetworkManager().GetClosestRoad(pos, road, roadDist);
		if (!road || roadDist > MAX_ROAD_DIST)
		{
			refusal = "target position must be on or near a road";
			return false;
		}

		// Calculate spline alignment
		array<vector> points = {};
		road.GetPoints(points);
		if (points.Count() < 2)
		{
			refusal = "road alignment unavailable";
			return false;
		}

		// Find closest segment
		int bestIdx = -1;
		float bestDistSq = float.MAX;
		vector bestProj = pos;
		for (int i = 0; i < points.Count() - 1; i++)
		{
			vector p1 = points[i];
			vector p2 = points[i + 1];
			vector seg = p2 - p1;
			seg[1] = 0;
			float lenSq = seg.LengthSq();
			if (lenSq < 0.01) continue;

			vector v = pos - p1;
			v[1] = 0;
			float t = (v[0] * seg[0] + v[2] * seg[2]) / lenSq;
			t = Math.Clamp(t, 0.0, 1.0);
			vector proj = p1 + seg * t;
			proj[1] = pos[1];
			float distSq = vector.DistanceSqXZ(pos, proj);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestIdx = i;
				bestProj = proj;
			}
		}

		if (bestIdx < 0 || bestDistSq > MAX_ROAD_DIST * MAX_ROAD_DIST)
		{
			refusal = "road alignment unavailable";
			return false;
		}

		vector dir = points[bestIdx + 1] - points[bestIdx];
		dir[1] = 0;
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;

		// Orient facing if direction specified
		if (facing != vector.Zero)
		{
			vector faceDir = facing - bestProj;
			faceDir[1] = 0;
			if (dir[0] * faceDir[0] + dir[2] * faceDir[2] < 0)
			{
				yaw += 180.0;
			}
		}

		outRoadPos = bestProj;
		outRoadPos[1] = GetGame().GetWorld().GetSurfaceY(outRoadPos[0], outRoadPos[2]);
		outYaw = yaw;

		// Validate where the composition will actually spawn, not the map click.
		// Distance check to bases
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (vector.DistanceXZ(outRoadPos, base.GetOrigin()) < MIN_BASE_DIST)
			{
				refusal = "too close to base perimeter";
				return false;
			}
		}

		// Distance check to other roadblocks
		PruneRoadblocks();
		foreach (ACR_Roadblock rb : s_aRoadblocks)
		{
			if (vector.DistanceXZ(outRoadPos, rb.m_vPos) < MIN_ROADBLOCK_DIST)
			{
				refusal = "too close to existing roadblock";
				return false;
			}
		}

		// Check for vehicles on the spot
		if (ACR_FobSlots.VehicleOnSpot(outRoadPos, 8.0))
		{
			refusal = "vehicles blocking road spot";
			return false;
		}

		return true;
	}

	static ACR_Roadblock SpawnRoadblock(string factionKey, vector pos, float yaw, string squadName = "")
	{
		ResourceName prefab = PrefabFor(factionKey);
		if (prefab.IsEmpty()) return null;

		Resource res = Resource.Load(prefab);
		if (!res.IsValid()) return null;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), spawnParams.Transform);
		spawnParams.Transform[3] = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), spawnParams);
		if (!ent) return null;

		ent.SetYawPitchRoll(Vector(yaw, 0, 0));

		SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(ent.FindComponent(SCR_CampaignBuildingCompositionComponent));
		if (comp)
		{
			comp.ACR_SetFaction(factionKey);
			comp.ACR_MarkBuildCharged();
			comp.ACR_SetCompositionSpawned(true);
		}

		ACR_Navmesh.RebuildAfterSpawn(ent);

		ACR_Roadblock rb = new ACR_Roadblock();
		rb.m_vPos = pos;
		rb.m_fYaw = yaw;
		rb.m_sFaction = factionKey;
		rb.m_Entity = ent;
		rb.m_Turret = FindTurretInHierarchy(ent);
		rb.m_sBuilderSquad = squadName;
		rb.m_fSpawnedAt = GetGame().GetWorld().GetWorldTime();

		s_aRoadblocks.Insert(rb);
		PrintFormat("[ACR] ROADBLOCK spawned for %1 at %2 (turret=%3)", factionKey, pos, rb.m_Turret != null);
		return rb;
	}

	static void DismantleRoadblock(ACR_Roadblock rb)
	{
		if (!rb) return;
		if (rb.m_Turret) VacateTurret(rb.m_Turret);
		if (rb.m_Entity) ACR_Navmesh.DeleteStructure(rb.m_Entity);

		ACR_Manager manager = ACR_Manager.GetInstance();
		if (manager) manager.m_Bases.Refund(rb.m_sFaction, DISMANTLE_REFUND);

		s_aRoadblocks.RemoveItem(rb);
		PrintFormat("[ACR] ROADBLOCK dismantled at %1, refunded %2 RP to %3", rb.m_vPos, DISMANTLE_REFUND, rb.m_sFaction);
	}

	static IEntity FindTurretInHierarchy(IEntity root)
	{
		if (!root) return null;
		if (root.FindComponent(TurretControllerComponent)) return root;
		SCR_BaseCompartmentManagerComponent comp = SCR_BaseCompartmentManagerComponent.Cast(root.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (comp)
		{
			array<BaseCompartmentSlot> slots = {};
			comp.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
			if (!slots.IsEmpty()) return root;
		}
		IEntity child = root.GetChildren();
		while (child)
		{
			IEntity found = FindTurretInHierarchy(child);
			if (found) return found;
			child = child.GetSibling();
		}
		return null;
	}

	static bool SeatSoldierInTurret(IEntity turretEnt, ChimeraCharacter soldier)
	{
		if (!turretEnt || !soldier || !ACR_Respawn.IsAlive(soldier)) return false;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return false;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return false;
		BaseCompartmentSlot seat = slots[0];
		if (seat.GetOccupant()) return false;
		CompartmentAccessComponent access = soldier.GetCompartmentAccessComponent();
		if (!access) return false;
		return access.GetInVehicle(turretEnt, seat, true, -1, ECloseDoorAfterActions.CLOSE_DOOR, false);
	}

	static bool IsTurretOccupied(IEntity turretEnt)
	{
		if (!turretEnt) return false;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return false;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return false;
		return slots[0].GetOccupant() != null;
	}

	static void VacateTurret(IEntity turretEnt)
	{
		if (!turretEnt) return;
		SCR_BaseCompartmentManagerComponent compMgr = SCR_BaseCompartmentManagerComponent.Cast(turretEnt.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compMgr) return;
		array<BaseCompartmentSlot> slots = {};
		compMgr.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if (slots.IsEmpty()) return;
		IEntity occupant = slots[0].GetOccupant();
		if (!occupant) return;
		ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
		if (!character) return;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (access) access.GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
	}

	static void PruneRoadblocks()
	{
		for (int i = s_aRoadblocks.Count() - 1; i >= 0; i--)
		{
			ACR_Roadblock rb = s_aRoadblocks[i];
			if (!rb || !rb.m_Entity || rb.m_Entity.IsDeleted())
			{
				s_aRoadblocks.RemoveOrdered(i);
			}
		}
	}

	static ACR_Roadblock FindNearest(vector pos, float maxDist = 50.0)
	{
		PruneRoadblocks();
		ACR_Roadblock best = null;
		float bestDist = maxDist;
		foreach (ACR_Roadblock rb : s_aRoadblocks)
		{
			float d = vector.DistanceXZ(pos, rb.m_vPos);
			if (d < bestDist)
			{
				bestDist = d;
				best = rb;
			}
		}
		return best;
	}
}
