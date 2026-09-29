// Development console helpers. These are not player-facing command endpoints.
// No RPC is exposed; movement must be executed on the authoritative machine.
class ACR_FeasibilityProbe
{
	static void Inspect()
	{
		Print("[ACR] Feasibility probe loaded");
		PrintFormat("[ACR] Server: %1", Replication.IsServer());
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
		{
			Print("[ACR] No local player controller (expected on dedicated server)");
			return;
		}
		PrintFormat("[ACR] Local player: %1", controller.GetPlayerId());
		SCR_PossessSpawnRequestComponent request = SCR_PossessSpawnRequestComponent.Cast(controller.FindComponent(SCR_PossessSpawnRequestComponent));
		PrintFormat("[ACR] Possession request component: %1", request != null);
		PrintFormat("[ACR] Possession manager: %1", SCR_PossessingManagerComponent.GetInstance() != null);
	}

	// Use a dedicated test group and a preplaced MOVE waypoint in the test world.
	// Existing waypoints are detached, not deleted: they may be shared resources.
	static bool MoveNamedGroup(string groupName, string waypointName)
	{
		if (!Replication.IsServer())
		{
			Print("[ACR] Move rejected: execute on server");
			return false;
		}
		AIGroup group = AIGroup.Cast(GetGame().GetWorld().FindEntityByName(groupName));
		AIWaypoint waypoint = AIWaypoint.Cast(GetGame().GetWorld().FindEntityByName(waypointName));
		if (!group || !waypoint)
		{
			Print("[ACR] Move rejected: named group or waypoint missing");
			return false;
		}
		array<AIWaypoint> previous = {};
		group.GetWaypoints(previous);
		foreach (AIWaypoint oldWaypoint : previous)
			group.RemoveWaypoint(oldWaypoint);
		group.AddWaypoint(waypoint);
		PrintFormat("[ACR] Waypoint assigned: %1 -> %2; agents: %3", groupName, waypointName, group.GetServerAgentsCount());
		return true;
	}

	// Local developer console only. The game's spawn request pipeline retains
	// authority over whether possession is allowed; no validation is bypassed.
	// A true result means dispatched, NOT successfully possessed.
	static bool RequestPossession(string soldierName)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return false;
		IEntity soldier = GetGame().GetWorld().FindEntityByName(soldierName);
		if (!soldier)
		{
			Print("[ACR] Possession rejected: named soldier not streamed in");
			return false;
		}
		SCR_PossessSpawnRequestComponent request = SCR_PossessSpawnRequestComponent.Cast(controller.FindComponent(SCR_PossessSpawnRequestComponent));
		RplComponent replication = RplComponent.Cast(soldier.FindComponent(RplComponent));
		if (!request || !replication)
		{
			Print("[ACR] Possession rejected: required component missing");
			return false;
		}
		SCR_PossessSpawnData data = SCR_PossessSpawnData.FromRplId(replication.Id());
		bool dispatched = request.RequestRespawn(data);
		PrintFormat("[ACR] Possession request dispatched: %1. Verify control separately.", dispatched);
		return dispatched;
	}

	static void InspectPlayer(int playerId)
	{
		SCR_PossessingManagerComponent manager = SCR_PossessingManagerComponent.GetInstance();
		if (!manager)
		{
			Print("[ACR] No possession manager in this game mode");
			return;
		}
		PrintFormat("[ACR] Player %1 possessing: %2", playerId, manager.IsPossessing(playerId));
		PrintFormat("[ACR] Main entity: %1; possessed entity: %2", manager.GetMainEntity(playerId), manager.GetPossessedEntity(playerId));
	}
}
