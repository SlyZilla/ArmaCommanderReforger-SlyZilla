// C&C-style forward base building (Stage 1; Roadmap Item 7).
// Validates FOB placement footprints, fells trees, manages construction truck
// supply reservation, build timer progression, and scripts HQ composition spawning.
class ACR_FobBuild
{
	static const float FOOTPRINT_RADIUS = 30.0;
	static const float CP_RADIUS = 40.0;
	static const float SLOPE_MAX_RISE = 3.0;
	static const float MIN_ROAD_DISTANCE = 22.0;
	static const float MIN_BASE_DISTANCE = 40.0;
	static const float SEARCH_RADIUS = 50.0;
	static const float BUILD_DURATION_S = 90.0;
	static const float LOADS_RESERVED = 20.0;
	static const int SUPPLIES_RESERVED = 500;

	static const ResourceName HQ_US = "{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et";
	static const ResourceName HQ_USSR = "{D70BF977E6D6300C}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_USSR_01.et";
	static const ResourceName HQ_FIA = "{16E06E20F4641F8A}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_FIA_01.et";

	static ResourceName HqPrefabFor(string factionKey)
	{
		if (factionKey == "USSR" || factionKey == "RHS_AFRF") return HQ_USSR;
		if (factionKey == "FIA") return HQ_FIA;
		return HQ_US;
	}

	static bool CanOrder(ACR_Squad squad, out string refusal)
	{
		refusal = "";
		if (!squad || squad.CountAlive() <= 0)
		{
			refusal = "Squad is eliminated";
			return false;
		}
		IEntity truck = squad.ConstructionTruck();
		if (!truck)
		{
			refusal = "Needs a construction truck";
			return false;
		}
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager)
		{
			refusal = "Manager unavailable";
			return false;
		}
		if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
		float stock = manager.m_Logistics.StockOf(truck);
		if (stock < LOADS_RESERVED)
		{
			int supplies = stock * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
			refusal = string.Format("Needs 500 supplies (truck has %1)", supplies);
			return false;
		}
		if (squad.OrderState() == ACR_EOrderState.BUILD_FOB)
		{
			refusal = "Already establishing a FOB";
			return false;
		}
		return true;
	}

	static bool CheckSpot(vector spot, out string reason)
	{
		if (!ACR_Manager.OnMap(spot))
		{
			reason = "Out of world bounds";
			return false;
		}

		// Water check: center and footprint perimeter
		if (BBB_Terrain.IsWaterAt(spot))
		{
			reason = "Site is in water";
			return false;
		}
		for (int i = 0; i < 16; i++)
		{
			float angle = i * 22.5 * Math.DEG2RAD;
			vector p = spot + Vector(Math.Cos(angle) * FOOTPRINT_RADIUS, 0, Math.Sin(angle) * FOOTPRINT_RADIUS);
			if (BBB_Terrain.IsWaterAt(p))
			{
				reason = "Site touches water";
				return false;
			}
		}

		// Slope check: elevation rise <= 3.0 m across the 30 m footprint
		float centerY = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		float minY = centerY;
		float maxY = centerY;
		for (int j = 0; j < 16; j++)
		{
			float a = j * 22.5 * Math.DEG2RAD;
			float yOuter = GetGame().GetWorld().GetSurfaceY(spot[0] + Math.Cos(a) * FOOTPRINT_RADIUS, spot[2] + Math.Sin(a) * FOOTPRINT_RADIUS);
			if (yOuter < minY) minY = yOuter;
			if (yOuter > maxY) maxY = yOuter;
			float yMid = GetGame().GetWorld().GetSurfaceY(spot[0] + Math.Cos(a) * (FOOTPRINT_RADIUS * 0.5), spot[2] + Math.Sin(a) * (FOOTPRINT_RADIUS * 0.5));
			if (yMid < minY) minY = yMid;
			if (yMid > maxY) maxY = yMid;
		}
		if ((maxY - minY) > SLOPE_MAX_RISE)
		{
			reason = string.Format("Terrain too steep (slope %1 m > %2 m)", (int)(maxY - minY + 0.5), (int)SLOPE_MAX_RISE);
			return false;
		}

		// Base proximity check: clear of existing bases within radius + 40 m
		foreach (ACR_Base base : ACR_Base.s_All)
		{
			if (!base || base.m_bRetired) continue;
			float dist = vector.DistanceXZ(base.GetOrigin(), spot);
			if (dist <= base.m_fRadius + MIN_BASE_DISTANCE)
			{
				reason = string.Format("Too close to %1", base.m_sName);
				return false;
			}
		}

		// Road network clearance: require road edge clearance >= 22 m
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld && aiWorld.GetRoadNetworkManager())
		{
			BaseRoad road;
			float roadDist;
			aiWorld.GetRoadNetworkManager().GetClosestRoad(spot, road, roadDist);
			if (road)
			{
				float halfWidth = road.GetWidth() * 0.5;
				if (roadDist - halfWidth < MIN_ROAD_DISTANCE)
				{
					reason = "Too close to a road";
					return false;
				}
			}
		}

		// Obstacle check: buildings, walls, and compositions
		s_Obstacles.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(spot, FOOTPRINT_RADIUS, CollectObstacle, null, EQueryEntitiesFlags.STATIC);
		if (!s_Obstacles.IsEmpty())
		{
			reason = "Blocked by existing buildings or walls";
			s_Obstacles.Clear();
			return false;
		}

		return true;
	}

	static bool ValidateSite(vector clicked, out vector validSpot, out string refusal)
	{
		clicked[1] = GetGame().GetWorld().GetSurfaceY(clicked[0], clicked[2]);
		if (CheckSpot(clicked, refusal))
		{
			validSpot = clicked;
			return true;
		}

		// Search outward in concentric rings up to SEARCH_RADIUS (~50 m)
		for (float r = 10.0; r <= SEARCH_RADIUS; r += 10.0)
		{
			int steps = 8 + (int)(r * 0.2);
			for (int i = 0; i < steps; i++)
			{
				float angle = (i * 360.0 / steps) * Math.DEG2RAD;
				vector candidate = clicked + Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
				candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
				string dummy;
				if (CheckSpot(candidate, dummy))
				{
					validSpot = candidate;
					return true;
				}
			}
		}

		return false;
	}

	static void FellTrees(vector center, float radius)
	{
		s_Foliage.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, CollectFoliage, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity e : s_Foliage)
		{
			if (!e) continue;
			SCR_DestructibleEntity dest = SCR_DestructibleEntity.Cast(e);
			if (dest && !SCR_DestructibleBuildingEntity.Cast(e))
			{
				dest.ForceKill_S();
			}
		}
		s_Foliage.Clear();
	}

	static bool IsContested(vector site, string faction)
	{
		ACR_Manager manager = ACR_Manager.GetInstance();
		if (!manager) return false;
		foreach (ACR_Squad squad : manager.m_Squads)
		{
			if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() == faction) continue;
			IEntity anchor = squad.GetAnchor();
			if (anchor && vector.DistanceXZ(anchor.GetOrigin(), site) <= CP_RADIUS + 10) return true;
		}
		return false;
	}

	protected static ref array<IEntity> s_Obstacles = {};
	protected static bool CollectObstacle(IEntity e)
	{
		if (!e || e.GetParent()) return true;
		if (SCR_DestructibleBuildingEntity.Cast(e) || BaseBuilding.Cast(e) || WallGeneratorEntity.Cast(e))
		{
			s_Obstacles.Insert(e);
			return true;
		}
		if (SCR_CampaignBuildingCompositionComponent.Cast(e.FindComponent(SCR_CampaignBuildingCompositionComponent)))
		{
			s_Obstacles.Insert(e);
			return true;
		}
		return true;
	}

	protected static ref array<IEntity> s_Foliage = {};
	protected static bool CollectFoliage(IEntity e)
	{
		if (e) s_Foliage.Insert(e);
		return true;
	}
}
