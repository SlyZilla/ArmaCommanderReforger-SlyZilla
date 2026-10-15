// Provider-independent travel marker, like a player menu's individual movement.
class BBB_MarchPlayerMove : SCR_AIMoveIndividuallyBehavior
{
 bool m_bTactical;
 void BBB_MarchPlayerMove(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, vector pos, float priority = PRIORITY_BEHAVIOR_MOVE_INDIVIDUALLY, float priorityLevel = PRIORITY_LEVEL_PLAYER, IEntity ent = null, float radius = 1.0) {}
 override int GetCause()
 {
  if (m_bTactical) return SCR_EAIBehaviorCause.GROUP_GOAL;
  return SCR_EAIBehaviorCause.SAFE;
 }
}

// Native movement/posture acceptance drill on BBB_TestArland, ~130 seconds.
class BBB_MarchTestHooks : BBB_Hooks
{
	SCR_AIGroup m_Group;
	bool m_bAllow = true;
	override void GetGroups(notnull array<SCR_AIGroup> groups) { groups.Clear(); if (m_Group) groups.Insert(m_Group); }
	override bool AllowRelaxedMarch(SCR_AIGroup group) { return m_bAllow; }
}

class BBB_RelaxedMarchTest
{
	static ref BBB_MarchTestHooks s_Hooks;
	static ref BBB_Hooks s_SavedHooks;
	static AIWaypoint s_Waypoint;
	static SCR_AIGroup s_TestGroup;
	static int s_iPass, s_iFail;
	static vector s_Start;
	static void Check(string name, bool ok)
	{
		string verdict = "FAIL";
		if (ok) { verdict = "PASS"; s_iPass++; } else s_iFail++;
		PrintFormat("[BBB] MARCHTEST %1 %2", verdict, name);
	}
	static IEntity Spawn(ResourceName prefab, vector pos)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = BBB.Ground(pos);
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
	}
	static void Begin()
	{
		s_SavedHooks = BBB.s_Hooks;
		s_Hooks = new BBB_MarchTestHooks();
		s_Hooks.m_Group = SCR_AIGroup.Cast(Spawn("{FCF7F5DC4F83955C}Prefabs/Groups/BLUFOR/Group_US_LightFireTeam.et", "1346 0 2776"));
		if (!s_Hooks.m_Group) { Check("group spawned", false); Finish(); return; }
		s_TestGroup = s_Hooks.m_Group;
		BBB.s_Hooks = s_Hooks;
		GetGame().GetCallqueue().CallLater(StartMarch, 3000);
	}
	static void StartMarch()
	{
		if (!BBB.AnchorOf(s_Hooks.m_Group)) { Check("group has soldiers", false); Finish(); return; }
		s_Start = BBB.AnchorOf(s_Hooks.m_Group).GetOrigin();
		s_Waypoint = AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", "2546 0 2810"));
		s_Hooks.m_Group.AddWaypoint(s_Waypoint);
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		foreach (AIAgent agent : agents) { agent.SetLOD(0); agent.PreventMaxLOD(100); }
		GetGame().GetCallqueue().CallLater(Quiet, 26000);
	}
	static int Lowered()
	{
		int count;
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (utility && utility.m_bBBBMarchLowered && soldier && soldier.GetCharacterController().GetWeaponManagerComponent().GetCurrentWeapon() && !soldier.GetCharacterController().IsWeaponRaised()) count++;
		}
		return count;
	}
	static void Quiet()
	{
		Check("native travel moves", vector.DistanceXZ(s_Start, BBB.AnchorOf(s_Hooks.m_Group).GetOrigin()) > 10);
		Check("quiet travellers lower held weapons", Lowered() > 0);
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
		utility.m_ThreatSystem.ThreatProjectileFlyby(1);
		BBB_RelaxedMarch.Tick();
		GetGame().GetCallqueue().CallLater(DangerCheck, 1500);
	}
	static void DangerCheck()
	{
		Check("one member's danger restores squad readiness", Lowered() == 0);
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		bool raised;
		foreach (AIAgent agent : agents)
		{
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (soldier.GetCharacterController().IsWeaponRaised()) raised = true;
			utility.m_ThreatSystem.SetThreatValues(0, 0, 0, 0);
		}
		Check("danger actually raises a weapon", raised);
		GetGame().GetCallqueue().CallLater(RelaxAgain, 19000);
	}
	static void RelaxAgain()
	{
		Check("relaxes again after quiet interval", Lowered() > 0);
		s_Hooks.m_bAllow = false;
		BBB_RelaxedMarch.Tick();
		Check("host task veto cancels relaxed posture", Lowered() == 0);
		Check("original waypoint retained", s_Hooks.m_Group.GetCurrentWaypoint() == s_Waypoint);
		s_Hooks.m_bAllow = true;
		GetGame().GetCallqueue().CallLater(StopCheck, 19000);
	}
	static void StopCheck()
	{
		Check("quiet travel resumes after task veto", Lowered() > 0);
		BBB_RelaxedMarch.Stop();
		Check("shutdown releases lowered posture", Lowered() == 0);
		// This group is deliberately absent from the host roster, as player slave groups are.
		s_Hooks.m_Group = null;
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
		 SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		 agent.SetPermanentLOD(0);
		 agent.PreventMaxLOD();
		 BBB_MarchPlayerMove move = new BBB_MarchPlayerMove(utility, null, BBB.Ground(agent.GetControlledEntity().GetOrigin() + Vector(900, 0, 0)));
		 utility.AddAction(move);
		}
		GetGame().GetCallqueue().CallLater(PlayerTravel, 35000);
	}
	static void PlayerTravel()
	{
		array<SCR_AIGroup> groups = {};
		BBB_VanillaHooks worldGroups = new BBB_VanillaHooks();
		worldGroups.GetGroups(groups);
		PrintFormat("[BBB] MARCHTEST world groups=%1 fixtureFound=%2", groups.Count(), groups.Contains(s_TestGroup));
		Check("unregistered player travel lowers held weapons", Lowered() > 0);
		array<AIAgent> agents = {};
		s_TestGroup.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
		 SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		 PrintFormat("[BBB] MARCHTEST player LOD=%1 action=%2 eligible=%3 travel=%4 danger=%5 quietMs=%6", agent.GetLOD(), utility.GetCurrentBehavior(), BBB_RelaxedMarch.Eligible(agent, utility), BBB_RelaxedMarch.Travel(utility.GetCurrentBehavior()), BBB_RelaxedMarch.Danger(agent, utility), GetGame().GetWorld().GetWorldTime() - utility.m_fBBBMarchQuietSince);
		 BBB_MarchPlayerMove move = BBB_MarchPlayerMove.Cast(utility.GetCurrentBehavior());
		 if (move) move.m_bTactical = true;
		}
		BBB_RelaxedMarch.Tick();
		Check("tactical individual movement retains readiness", Lowered() == 0);
		Finish();
	}
	static void Finish()
	{
		BBB.s_Hooks = s_SavedHooks;
		string verdict = "FAIL";
		if (s_iFail == 0 && s_iPass == 11) verdict = "PASS";
		PrintFormat("[ACR] VERDICT %1 test=relaxedmarch passed=%2 failed=%3 expected=11", verdict, s_iPass, s_iFail);
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() && System.IsCLIParam("acrBbbRelaxedMarchTest")) GetGame().GetCallqueue().CallLater(BBB_RelaxedMarchTest.Begin, 15000);
	}
}
