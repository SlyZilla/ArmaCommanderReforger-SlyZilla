// Navmesh rebuild round FOB structures (server). The soldier and vehicle navmeshes are baked
// with the terrain, so a script-spawned HQ, upgrade or defence piece is invisible to pathing
// (AI walk into it) and a deleted one leaves a hole AI route round. Vanilla rebuilds only for
// editor placement and shovel dismantling; our spawns and deletes go through here instead.
class ACR_Navmesh
{
	static const int SPAWN_DELAY_MS = 1000;   // let the composition's children and physics settle
	static const int DELETE_DELAY_MS = 1000;  // vanilla's delay: the entity must be fully gone

	static int s_iQueued;   // rebuilds scheduled (tests)
	static int s_iRequests; // areas requested (tests)

	// After a structure is spawned or finished building.
	static void RebuildAfterSpawn(IEntity entity)
	{
		if (!entity || !Replication.IsServer()) return;
		GetGame().GetCallqueue().CallLater(RebuildEntity, SPAWN_DELAY_MS, false, entity);
		s_iQueued++;
	}

	protected static void RebuildEntity(IEntity entity)
	{
		if (!entity || entity.IsDeleted()) return;
		array<ref Tuple2<vector, vector>> areas = {};
		array<bool> redoRoads = {};
		if (!Collect(entity, areas, redoRoads)) return;
		Request(areas, redoRoads);
	}

	// Deletes a structure and rebuilds the navmesh where it stood (areas taken before deletion).
	static void DeleteStructure(IEntity entity)
	{
		if (!entity) return;
		array<ref Tuple2<vector, vector>> areas = {};
		array<bool> redoRoads = {};
		bool collected = Replication.IsServer() && Collect(entity, areas, redoRoads);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		if (!collected) return;
		GetGame().GetCallqueue().CallLater(Request, DELETE_DELAY_MS, false, areas, redoRoads);
		s_iQueued++;
	}

	// The navmesh-layer bounds of the entity and its children; false = nothing to rebuild.
	static bool Collect(IEntity entity, notnull array<ref Tuple2<vector, vector>> areas, notnull array<bool> redoRoads)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld) return false;
		aiWorld.GetNavmeshRebuildAreas(entity, areas, redoRoads);
		return !areas.IsEmpty();
	}

	protected static void Request(array<ref Tuple2<vector, vector>> areas, array<bool> redoRoads)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld || !areas || !redoRoads) return;
		aiWorld.RequestNavmeshRebuildAreas(areas, redoRoads);
		s_iRequests += areas.Count();
	}
}
