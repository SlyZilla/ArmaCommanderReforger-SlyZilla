// Lower a held weapon, never holster it. Only ordinary travel may relax;
// native combat, investigation and specialist tasks keep their own posture.
class BBB_RelaxedMarch
{
	static bool s_bRead;
	static bool s_bOn = true;
	static const float QUIET_MS = 15000;
	protected static ref BBB_VanillaHooks s_WorldGroups = new BBB_VanillaHooks();
	protected static ref array<SCR_AIGroup> s_aHostGroups = {};

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;
		string value;
		if (System.GetCLIParam("bbbRelaxedMarch", value) && value == "0") s_bOn = false;
	}

	static bool Travel(SCR_AIBehaviorBase action)
	{
		if (!action || action.EvaluatePriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_GAMEMASTER) return false;
		bool formation = action.Type() == SCR_AIMoveInFormationBehavior;
		bool individual = SCR_AIMoveIndividuallyBehavior.Cast(action) != null;
		if (!formation && !individual) return false;
		SCR_AIActivityBase activity = action.GetGroupActivityContext();
		if (activity) return activity.Type() == SCR_AIMoveActivity;
		// Custom individual movement must explicitly identify ordinary travel.
		// GROUP_GOAL/COMBAT movement (bounds, clears, assaults) stays ready.
		return formation || (individual && action.GetCause() == SCR_EAIBehaviorCause.SAFE);
	}

	static bool Danger(AIAgent agent, SCR_AIUtilityComponent utility)
	{
		if (!agent || !utility || !utility.m_CombatComponent || !utility.m_ThreatSystem || !utility.m_PerceptionComponent) return true;
		if (agent.GetDangerEventsCount() > 0 || utility.m_CombatComponent.GetCurrentTarget() || utility.m_CombatComponent.GetRetreatTarget()) return true;
		if (utility.m_ThreatSystem.GetThreatMeasure() > 0.001) return true;
		array<BaseTarget> targets = {};
		utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.ENEMY);
		if (!targets.IsEmpty()) return true;
		utility.m_PerceptionComponent.GetTargetsList(targets, ETargetCategory.DETECTED);
		return !targets.IsEmpty();
	}

	static bool Eligible(AIAgent agent, SCR_AIUtilityComponent utility)
	{
		if (!agent || agent.GetLOD() != 0 || !utility || !utility.IsMilitary()) return false;
		ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!soldier || soldier.IsInVehicle()) return false;
		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious() || controller.IsPlayerControlled() || BBB.PlayerOf(soldier)) return false;
		if (controller.IsChangingItem() || controller.IsGadgetInHands() || controller.IsReloading() || controller.GetDisableWeaponControls() || controller.IsClimbing() || controller.IsSwimming()) return false;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group) return false;
		SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(group.GetCurrentWaypoint());
		if (SCR_SearchAndDestroyWaypoint.Cast(waypoint)) return false;
		if (waypoint && waypoint.GetPriorityLevel() >= SCR_AIActionBase.PRIORITY_LEVEL_GAMEMASTER) return false;
		bool allowed = !s_aHostGroups.Contains(group) || BBB.s_Hooks.AllowRelaxedMarch(group);
		return allowed && Travel(utility.GetCurrentBehavior());
	}

	static void Tick()
	{
		Read();
		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(s_aHostGroups);
		groups.Copy(s_aHostGroups);
		array<SCR_AIGroup> nativeGroups = {};
		s_WorldGroups.GetGroups(nativeGroups);
		foreach (SCR_AIGroup worldGroup : nativeGroups)
			if (!groups.Contains(worldGroup)) groups.Insert(worldGroup);
		float now = GetGame().GetWorld().GetWorldTime();
		foreach (SCR_AIGroup group : groups)
		{
			if (!group) continue;
			array<AIAgent> agents = {};
			BBB.s_Hooks.GetMembers(group, agents);
			bool danger = false;
			foreach (AIAgent member : agents)
			{
				if (!member || !BBB.IsAlive(member.GetControlledEntity())) continue;
				SCR_AIUtilityComponent info = SCR_AIUtilityComponent.Cast(member.FindComponent(SCR_AIUtilityComponent));
				if (Danger(member, info)) danger = true;
			}
			foreach (AIAgent agent : agents)
			{
				if (!agent) continue;
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
				if (!utility) continue;
				bool eligible = s_bOn && Eligible(agent, utility);
				if (danger || !eligible || utility.m_fBBBMarchQuietSince <= 0) utility.m_fBBBMarchQuietSince = now;
				bool relaxed = eligible && !danger && now - utility.m_fBBBMarchQuietSince >= QUIET_MS;
				ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (character && character.GetCharacterController())
				{
					CharacterControllerComponent controller = character.GetCharacterController();
					if (relaxed) controller.SetWeaponRaised(false);
					else if (utility.m_bBBBMarchLowered && !controller.IsPlayerControlled() && !BBB.PlayerOf(character) && !controller.IsDead() && !controller.IsUnconscious() && !character.IsInVehicle() && !controller.IsGadgetInHands() && !controller.IsChangingItem()) controller.SetWeaponRaised(true);
				}
				utility.m_bBBBMarchLowered = relaxed;
			}
		}
	}

	static void Stop()
	{
		bool enabled = s_bOn;
		s_bOn = false;
		Tick();
		s_WorldGroups = new BBB_VanillaHooks(); // new world's clock may start below this cache's timestamp
		s_aHostGroups.Clear();
		s_bOn = enabled;
	}
}

modded class SCR_AIUtilityComponent
{
	float m_fBBBMarchQuietSince;
	bool m_bBBBMarchLowered;
}

// Movement trees can request a raised weapon repeatedly. Respect their other
// outputs and substitute only the posture of an eligible quiet traveller.
modded class SCR_AISetWeaponRaised
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		ENodeResult result = super.EOnTaskSimulate(owner, dt);
		if (result != ENodeResult.SUCCESS || !owner) return result;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
		if (!utility || !utility.m_bBBBMarchLowered || !BBB_RelaxedMarch.s_bOn) return result;
		if (BBB_RelaxedMarch.Eligible(owner, utility) && !BBB_RelaxedMarch.Danger(owner, utility))
		{
			ChimeraCharacter soldier = ChimeraCharacter.Cast(owner.GetControlledEntity());
			if (soldier) soldier.GetCharacterController().SetWeaponRaised(false);
		}
		return result;
	}
}
