// Give a freshly visible, selected infantry target a firing opportunity before
// ordinary combat repositioning. Native perception, weapon selection and friendly
// fire checks stay in charge. No targets are inferred from world/map positions.
class BBB_Engagement
{
	static bool s_bRead;
	static bool s_bOn = true;
	static bool s_bTargetPriority = true;
	static bool s_bTestMetrics;
	static int s_iFiltered;
	static int s_iResolved;
	static const float RANGE_M = 100;
	static const float OPENING_FIRE_S = 2;

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;
		string value;
		if (System.GetCLIParam("bbbCombatEngagement", value)) s_bOn = value == "1";
		if (System.GetCLIParam("bbbCombatTargetPriority", value)) s_bTargetPriority = value == "1";
		PrintFormat("[BBB] ENGAGEMENT movement=%1 visibleTargetPriority=%2", s_bOn, s_bTargetPriority);
	}

	static bool Eligible(SCR_AIUtilityComponent utility, SCR_AICombatComponent combat, BaseTarget target)
	{
		Read();
		if (!s_bOn) return false;
		int started;
		if (BBB_Perf.s_bOn) started = System.GetTickCount();
		bool eligible = CanEngage(utility, combat, target);
		if (BBB_Perf.s_bOn) BBB_Perf.Add("ev.engagement", System.GetTickCount() - started);
		return eligible;
	}

	protected static bool CanEngage(SCR_AIUtilityComponent utility, SCR_AICombatComponent combat, BaseTarget target)
	{
		if (!utility || !combat || !target || !utility.m_AIInfo) return false;
		if (target.GetTargetCategory() != ETargetCategory.ENEMY) return false;
		if (target != combat.GetCurrentTarget() || target.GetUnitType() != EAIUnitType.UnitType_Infantry) return false;
		if (target.IsDisarmed() || !combat.IsTargetVisible(target) || target.GetDistance() > RANGE_M) return false;
		// Fresh reports can outlive personal line of fire. Let vanilla reposition
		// when the native target trace is obstructed instead of holding him there.
		if (target.GetTraceFraction() < 0.99) return false;
		if (target.GetDistance() < combat.GetSelectedWeaponMinDist() + 5) return false;
		return Ready(utility, combat);
	}

	static bool Ready(SCR_AIUtilityComponent utility, SCR_AICombatComponent combat)
	{
		if (!utility || !combat || !utility.m_AIInfo) return false;
		if (combat.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) return false;
		if (utility.m_AIInfo.HasUnitState(EUnitState.IN_VEHICLE) || utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS)) return false;
		if (!BBB.IsAlive(utility.m_OwnerEntity) || BBB.PlayerOf(utility.m_OwnerEntity)) return false;
		BaseWeaponComponent selected = combat.BBBSelectedWeapon();
		if (!selected || selected != combat.GetCurrentWeapon()) return false;
		EWeaponType kind = selected.GetWeaponType();
		if (kind != EWeaponType.WT_RIFLE && kind != EWeaponType.WT_MACHINEGUN) return false;
		BaseMagazineComponent magazine = selected.GetCurrentMagazine();
		if (!magazine || magazine.GetAmmoCount() <= 0) return false;
		// Preserve explicit fast/forced movement and direct player/GM activities.
		return !BBB_OrderIntent.Blocks(utility);
	}
}

modded class SCR_AICombatComponent
{
	// Diagnostic association only; never used to nominate a production target.
	BaseTarget m_BBBPriorityTestTarget;
	protected bool BBBPreferVisibleContact()
	{
		BBB_Engagement.Read();
		if (!BBB_Engagement.s_bTargetPriority || !m_Perception) return false;
		BaseTarget previous = GetCurrentTarget();
		if (!previous || previous.GetUnitType() != EAIUnitType.UnitType_Infantry || previous.GetTimeSinceSeen() <= 2) return false;
		if (!BBB_Engagement.Ready(m_Utility, this)) return false;
		array<BaseTarget> enemies = {};
		m_Perception.GetTargetsList(enemies, ETargetCategory.ENEMY);
		foreach (BaseTarget candidate : enemies)
		{
			if (candidate == previous || candidate.GetUnitType() != EAIUnitType.UnitType_Infantry || candidate.IsDisarmed()) continue;
			if (candidate.GetDistance() > BBB_Engagement.RANGE_M || candidate.GetDistance() < GetSelectedWeaponMinDist() + 5) continue;
			if (IsTargetVisible(candidate) && candidate.GetTraceFraction() >= 0.99) return true;
		}
		return false;
	}

	override void EvaluateWeaponAndTarget(out bool outWeaponEvent, out bool outSelectedTargetChanged,
		out BaseTarget outPrevTarget, out BaseTarget outCurrentTarget,
		out bool outRetreatTargetChanged, out bool outCompartmentChanged)
	{
		// Change only a due native selection, with a real close visible alternative.
		// Temporarily exclude remembered positions; the native selector still chooses
		// both target and matching weapon. Restore its normal memory windows afterward.
		bool filtered;
		bool previousClose;
		int started;
		if (BBB_Perf.s_bOn) started = System.GetTickCount();
		if (GetGame().GetWorld().GetWorldTime() >= m_fNextWeaponTargetEvaluation_ms && BBBPreferVisibleContact())
		{
			filtered = true;
			previousClose = GetCurrentTarget().GetDistance() < CLOSE_RANGE_COMBAT_DISTANCE;
			float freshMax = Math.Max(TARGET_MAX_LAST_SEEN_VISIBLE, m_Perception.GetUpdateInterval()) + 0.02;
			m_WeaponTargetSelector.SetSelectionProperties(freshMax, freshMax, freshMax, TARGET_MIN_INDIRECT_TRACE_FRACTION_MIN,
				TARGET_MAX_DISTANCE_INFANTRY, TARGET_MAX_DISTANCE_VEHICLE, TARGET_MAX_TIME_SINCE_ENDANGERED, TARGET_MAX_DISTANCE_DISARMED);
		}
		// Profile added checks, excluding the native selector's own work.
		if (BBB_Perf.s_bOn) BBB_Perf.Add("ev.targetPriority", System.GetTickCount() - started);
		super.EvaluateWeaponAndTarget(outWeaponEvent, outSelectedTargetChanged, outPrevTarget, outCurrentTarget, outRetreatTargetChanged, outCompartmentChanged);
		if (filtered)
		{
			if (BBB_Perf.s_bOn) started = System.GetTickCount();
			SetTargetSelectionProperties(previousClose);
			if (BBB_Engagement.s_bTestMetrics)
			{
				BBB_Engagement.s_iFiltered++;
				BaseTarget chosen = GetCurrentTarget();
				if (chosen && IsTargetVisible(chosen))
				{
					BBB_Engagement.s_iResolved++;
					m_BBBPriorityTestTarget = chosen;
				}
			}
			if (BBB_Perf.s_bOn) BBB_Perf.Add("ev.targetPriority", System.GetTickCount() - started);
		}
	}
}

modded class SCR_AICombatMoveLogic_Attack
{
	protected override bool MoveToNextPosCondition()
	{
		// Only delays ordinary repositioning. The base node evaluates danger,
		// withdrawal, unusable cover, reloading and friendly-fire avoidance first.
		if (BBB_Engagement.Eligible(m_Utility, m_CombatComp, m_Target) && !m_State.IsExecutingRequest() && !m_State.m_bInCover && m_State.m_fTimerStopped_s < BBB_Engagement.OPENING_FIRE_S)
			return false;
		return super.MoveToNextPosCondition();
	}

	protected override bool ResolveFailMoveIfNoCover()
	{
		// With a visible shootable threat, failure to find cover is not a reason
		// to advance into the open. Search again later; losing sight restores pursuit.
		if (BBB_Engagement.Eligible(m_Utility, m_CombatComp, m_Target)) return true;
		return super.ResolveFailMoveIfNoCover();
	}

	protected override void PushRequestMove()
	{
		super.PushRequestMove();
		if (!BBB_Engagement.Eligible(m_Utility, m_CombatComp, m_Target)) return;
		SCR_AICombatMoveRequest_Move request = SCR_AICombatMoveRequest_Move.Cast(m_State.GetRequest());
		if (!request || request.m_eReason != SCR_EAICombatMoveReason.STANDARD) return;
		// Keep native cover choice, but allow rifle/MG aiming during a short bound.
		if (request.m_eMovementType == EMovementType.SPRINT) request.m_eMovementType = EMovementType.RUN;
		request.m_bAimAtTarget = SCR_AICombatMoveUtils.IsAimingAndMovementPossible(request.m_eStanceMoving, request.m_eMovementType);
	}
}
