class BBB_OrderIntentTestHold : SCR_AIBehaviorBase
{
 void BBB_OrderIntentTestHold(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity)
 {
  m_sBehaviorTree = "AI/BehaviorTrees/Chimera/Soldier/Wait.bt";
  SetPriorityLevel(PRIORITY_LEVEL_PLAYER);
 }
 override float CustomEvaluate() { return 0; }
}

// Close-contact diagnosis. Targets come only from native perception; test geometry
// is used for measurement, never supplied to production target selection.
class BBB_EngagementTest : BBB_CombatTest
{
	static ref BBB_EngagementTest s_Engagement;
	static const ResourceName WP_MOVE = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	protected ref BBB_AimLane m_Lane;
	protected ref map<BaseWeaponComponent, int> m_Ammo = new map<BaseWeaponComponent, int>();
	protected ref map<IEntity, vector> m_Positions = new map<IEntity, vector>();
	protected ref map<BaseWeaponComponent, BaseMagazineComponent> m_Magazines = new map<BaseWeaponComponent, BaseMagazineComponent>();
	protected ref map<IEntity, float> m_FirstContact = new map<IEntity, float>();
	protected ref array<IEntity> m_ReturnedFire = {};
	protected float m_End;
	protected float m_LogAt;
	protected int m_Round;
	protected int m_Completed;
	protected int m_SelectedSamples;
	protected int m_Shots;
	protected bool m_Missing;
	protected bool m_SavedEngagement;
	protected int m_Fresh;
	protected int m_BlockedAim;
	protected int m_FreshShots;
	protected int m_TurnedBack;
	protected int m_FreshMoving;
	protected float m_ResponseTime;
	protected int m_Responses;
	protected bool m_CoreGuards;
	protected bool m_WaypointGuard;
	protected int m_GuardPasses;
	protected int m_GuardFailures;
	protected bool m_TargetComparison;
	protected bool m_SavedPriority;
	protected bool m_SavedMetrics;
	protected int m_StaleOverFresh;
	protected int m_FilterBase;
	protected int m_ResolvedBase;
	protected int m_HandoffShots;
	protected int m_TotalHandoffShots;

	void BBB_EngagementTest() { m_aCheckDistances = {25, 40, 60, 90, 100}; }

	protected override bool LaneClear(vector from, vector dir)
	{
		// The inherited lane checked only 100 m and could select a steep hillside.
		// Close contact needs unobstructed intermediate distances and a level strip.
		float low = float.MAX;
		float high = -float.MAX;
		vector across = Vector(dir[2], 0, -dir[0]);
		for (int step = 0; step <= 20; step++)
		{
			for (int edge = -1; edge <= 1; edge++)
			{
				vector point = from + dir * (step * 5) + across * (edge * 5);
				float y = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
				low = Math.Min(low, y);
				high = Math.Max(high, y);
				if (high - low > 2.5) return false;
			}
		}
		return super.LaneClear(from, dir);
	}

	static void MaybeStartEngagement()
	{
		if (s_Engagement || !Replication.IsServer()) return;
		if (!System.IsCLIParam("acrBbbEngagementTest") && !System.IsCLIParam("acrBbbTargetPriorityTest")) return;
		s_Engagement = new BBB_EngagementTest();
		GetGame().GetCallqueue().CallLater(s_Engagement.BeginEngagement, 10000);
	}

	protected void BeginEngagement()
	{
		BBB_Engagement.Read();
		m_SavedEngagement = BBB_Engagement.s_bOn;
		m_SavedPriority = BBB_Engagement.s_bTargetPriority;
		m_SavedMetrics = BBB_Engagement.s_bTestMetrics;
		m_TargetComparison = System.IsCLIParam("acrBbbTargetPriorityTest");
		BBB_Engagement.s_bTestMetrics = true;
		m_bOriginalPerf = BBB_Perf.s_bOn;
		m_bOriginalPerfReport = BBB_Perf.s_bOwnReport;
		BBB_Perf.s_bOn = true;
		BBB_Perf.s_bOwnReport = true;
		if (!FindIsolatedSite()) { FinishEngagement(); return; }
		LoadLane();
		Print("[BBB] ENGAGE measurement: 8 x 20 s, stationary/move-through-contact, engagement off/on, factions swapped, native targets only");
		GetGame().GetCallqueue().CallLater(TickEngagement, 500, true);
		m_End = Now() + 4;
	}

	protected void StartEngagement()
	{
		m_Lane = new BBB_AimLane();
		m_Lane.m_vFrom = m_aSiteFrom[0];
		m_Lane.m_vDir = m_aSiteDir[0];
		m_Lane.m_vTo = m_Lane.m_vFrom + m_Lane.m_vDir * 60;
		ResourceName a = TEAM_US;
		ResourceName b = TEAM_USSR;
		if (m_Round % 2 == 1) { a = TEAM_USSR; b = TEAM_US; }
		m_Lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(a, m_Lane.m_vFrom, m_Lane.m_vDir));
		m_Lane.m_B.m_Group = SCR_AIGroup.Cast(Spawn(b, m_Lane.m_vTo, m_Lane.m_vDir * -1));
		if (m_Lane.m_A.m_Group) m_Lane.m_aSpawned.Insert(m_Lane.m_A.m_Group);
		if (m_Lane.m_B.m_Group) m_Lane.m_aSpawned.Insert(m_Lane.m_B.m_Group);
		m_Lane.m_fStart = Now();
		m_Lane.m_fHookAt = Now() + 3;
		m_End = Now() + 20;
		BBB_Engagement.s_bOn = m_Round % 4 >= 2;
		BBB_Engagement.s_bTargetPriority = false;
		if (m_TargetComparison)
		{
			BBB_Engagement.s_bOn = true;
			BBB_Engagement.s_bTargetPriority = m_Round % 4 >= 2;
		}
		m_StaleOverFresh = 0;
		m_HandoffShots = 0;
		m_FilterBase = BBB_Engagement.s_iFiltered;
		m_ResolvedBase = BBB_Engagement.s_iResolved;
		m_Ammo.Clear();
		m_Positions.Clear();
		m_Magazines.Clear();
		m_FirstContact.Clear();
		m_ReturnedFire.Clear();
		m_Fresh = 0; m_BlockedAim = 0; m_FreshShots = 0; m_TurnedBack = 0; m_FreshMoving = 0; m_ResponseTime = 0; m_Responses = 0;
		PrintFormat("[BBB] ENGAGE round=%1 move=%2 swap=%3 engagement=%4", m_Round + 1, m_Round >= 4, m_Round % 2 == 1, BBB_Engagement.s_bOn);
		PrintFormat("[BBB] ENGAGE targetPriority=%1 (comparison=%2)", BBB_Engagement.s_bTargetPriority, m_TargetComparison);
	}

	protected void TickEngagement()
	{
		float now = Now();
		if (!m_Lane)
		{
			if (now < m_End) return;
			if (m_Round >= 8) { FinishEngagement(); return; }
			StartEngagement();
			return;
		}
		if (!m_Lane.m_bHooked && now >= m_Lane.m_fHookAt)
		{
			m_Lane.m_bHooked = true;
			Hook(m_Lane.m_A, false);
			Hook(m_Lane.m_B, false);
			if (m_Lane.m_A.m_aMen.IsEmpty() || m_Lane.m_B.m_aMen.IsEmpty()) m_Missing = true;
			if (m_Round >= 4 && m_Lane.m_A.m_Group)
			{
				AIWaypoint wp = AIWaypoint.Cast(Spawn(WP_MOVE, m_Lane.m_vTo + m_Lane.m_vDir * 30, m_Lane.m_vDir));
				if (wp) { m_Lane.m_aSpawned.Insert(wp); m_Lane.m_A.m_Group.AddWaypoint(wp); }
				else m_Missing = true;
			}
		}
		if (m_Lane.m_bHooked)
		{
			bool log = now >= m_LogAt;
			SampleSide(m_Lane.m_A, 0, log);
			SampleSide(m_Lane.m_B, 1, log);
			if (log) m_LogAt = now + 2;
		}
		if (now < m_End) return;
		PrintFormat("[BBB] ENGAGE summary r=%1 move=%2 engagement=%3 fresh=%4 aimBlocked=%5 freshShots=%6 movingFresh=%7 backTurned=%8", m_Round + 1, m_Round >= 4, BBB_Engagement.s_bOn, m_Fresh, m_BlockedAim, m_FreshShots, m_FreshMoving, m_TurnedBack);
		PrintFormat("[BBB] ENGAGE latency r=%1 responders=%2 totalSeconds=%3 alive=%4/%5", m_Round + 1, m_Responses, m_ResponseTime, m_Lane.m_A.m_aMen.Count() - Downed(m_Lane.m_A), m_Lane.m_B.m_aMen.Count() - Downed(m_Lane.m_B));
		PrintFormat("[BBB] ENGAGE priority r=%1 on=%2 staleOverFresh=%3 filtered=%4 resolved=%5 handoffShots=%6", m_Round + 1, BBB_Engagement.s_bTargetPriority, m_StaleOverFresh, BBB_Engagement.s_iFiltered - m_FilterBase, BBB_Engagement.s_iResolved - m_ResolvedBase, m_HandoffShots);
		m_Completed++;
		Clear(m_Lane);
		m_Lane = null;
		m_Ammo.Clear();
		m_Positions.Clear();
		m_Magazines.Clear();
		m_FirstContact.Clear();
		m_ReturnedFire.Clear();
		m_Round++;
		m_End = now + 3;
	}

	protected void SampleSide(BBB_AimSide side, int sideId, bool log)
	{
		foreach (int index, IEntity man : side.m_aMen)
		{
			if (Down(man)) continue;
			AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
			AIAgent agent;
			if (control) agent = control.GetAIAgent();
			SCR_AIUtilityComponent utility;
			if (agent) utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility || !utility.m_CombatComponent || !utility.m_PerceptionComponent) { m_Missing = true; continue; }
			SCR_AICombatComponent combat = utility.m_CombatComponent;
			BaseTarget selected = combat.GetCurrentTarget();
			if (BBB_Engagement.Eligible(utility, combat, selected)) TestGuards(utility, combat, selected);
			if (selected) m_SelectedSamples++;
			BaseWeaponComponent current = combat.GetCurrentWeapon();
			int ammo = -1;
			int fired;
			if (current && current.GetCurrentMagazine())
			{
				ammo = current.GetCurrentMagazine().GetAmmoCount();
				if (m_Ammo.Contains(current) && m_Magazines.Get(current) == current.GetCurrentMagazine() && m_Ammo.Get(current) > ammo) fired = m_Ammo.Get(current) - ammo;
				m_Ammo.Set(current, ammo);
				m_Magazines.Set(current, current.GetCurrentMagazine());
			}
			m_Shots += fired;
			vector position = man.GetOrigin();
			float speed;
			if (m_Positions.Contains(man)) speed = vector.DistanceXZ(position, m_Positions.Get(man)) * 2;
			m_Positions.Set(man, position);
			array<BaseTarget> enemies = {};
			array<BaseTarget> detected = {};
			utility.m_PerceptionComponent.GetTargetsList(enemies, ETargetCategory.ENEMY);
			utility.m_PerceptionComponent.GetTargetsList(detected, ETargetCategory.DETECTED);
			if (selected && selected.GetTimeSinceSeen() > 2)
			{
				foreach (BaseTarget freshEnemy : enemies)
				{
					if (freshEnemy.IsDisarmed() || freshEnemy.GetUnitType() != EAIUnitType.UnitType_Infantry || freshEnemy.GetDistance() > BBB_Engagement.RANGE_M) continue;
					if (!combat.IsTargetVisible(freshEnemy) || freshEnemy.GetTraceFraction() < 0.99) continue;
					m_StaleOverFresh++;
					break;
				}
			}
			float range = -1;
			float seen = -1;
			float facing;
			if (selected)
			{
				range = selected.GetDistance();
				seen = selected.GetTimeSinceSeen();
				vector toward = selected.GetLastSeenPosition() - position;
				toward[1] = 0;
				toward.Normalize();
				vector matrix[4];
				man.GetWorldTransform(matrix);
				facing = vector.Dot(matrix[2], toward);
			}
			if (selected && combat.IsTargetVisible(selected) && ammo > 0)
			{
				m_Fresh++;
				m_FreshShots += fired;
				if (fired > 0 && selected == combat.m_BBBPriorityTestTarget)
				{
					m_HandoffShots += fired;
					m_TotalHandoffShots += fired;
				}
				if (utility.m_CombatMoveState && !utility.m_CombatMoveState.m_bAimAtTarget) m_BlockedAim++;
				if (speed > 1) m_FreshMoving++;
				if (facing < 0) m_TurnedBack++;
				if (!m_FirstContact.Contains(man)) m_FirstContact.Set(man, Now());
				if (fired > 0 && !m_ReturnedFire.Contains(man))
				{
					m_ReturnedFire.Insert(man);
					m_ResponseTime += Now() - m_FirstContact.Get(man);
					m_Responses++;
				}
			}
			if (!log && fired == 0) continue;
			string action = "none";
			SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
			if (behavior) action = behavior.Type().ToString();
			PrintFormat("[BBB] ENGAGE r=%1 side=%2 unit=%3 enemies=%4 detected=%5 selected=%6 seen=%7 dist=%8 action=%9", m_Round + 1, sideId, index, enemies.Count(), detected.Count(), selected != null, seen, range, action);
			PrintFormat("[BBB] ENGAGE motion r=%1 side=%2 unit=%3 speed=%4 facing=%5 threat=%6 ammo=%7 fired=%8 mode=%9", m_Round + 1, sideId, index, speed, facing, utility.m_ThreatSystem.GetThreatMeasure(), ammo, fired, combat.GetCombatMode());
			if (log)
			{
				SCR_AICombatMoveState state = utility.m_CombatMoveState;
				if (state) PrintFormat("[BBB] ENGAGE move r=%1 side=%2 unit=%3 executing=%4 aim=%5 cover=%6 stopped=%7", m_Round + 1, sideId, index, state.IsExecutingRequest(), state.m_bAimAtTarget, state.m_bInCover, state.m_fTimerStopped_s);
				foreach (BaseTarget enemy : enemies)
					PrintFormat("[BBB] ENGAGE known r=%1 side=%2 unit=%3 dist=%4 seen=%5 trace=%6 score=%7 disarmed=%8", m_Round + 1, sideId, index, enemy.GetDistance(), enemy.GetTimeSinceSeen(), enemy.GetTraceFraction(), combat.m_WeaponTargetSelector.CalculateTargetScore(enemy), enemy.IsDisarmed());
			}
		}
	}

	protected void GuardCheck(string name, bool ok)
	{
		string result = "FAIL";
		if (ok) { result = "PASS"; m_GuardPasses++; }
		else m_GuardFailures++;
		PrintFormat("[BBB] ENGAGE guard %1 %2", result, name);
	}

	protected void TestGuards(SCR_AIUtilityComponent utility, SCR_AICombatComponent combat, BaseTarget target)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
		if (!group || !group.GetGroupUtilityComponent()) return;
		if (!m_CoreGuards)
		{
			m_CoreGuards = true;
			BBB_Engagement.s_bOn = false;
			GuardCheck("off switch with a real selected visible target", !BBB_Engagement.Eligible(utility, combat, target));
			BBB_Engagement.s_bOn = true;
			SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
			EAIGroupCombatMode previousMode = groupUtility.GetCombatModeExternal();
			groupUtility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
			groupUtility.EvaluateCombatMode();
			GuardCheck("actual hold-fire mode", !BBB_Engagement.Eligible(utility, combat, target));
			groupUtility.SetCombatMode(previousMode);
			groupUtility.EvaluateCombatMode();
			SCR_AIBehaviorBase behavior = utility.GetCurrentBehavior();
			if (behavior)
			{
				int previousLevel = behavior.EvaluatePriorityLevel();
				behavior.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
				GuardCheck("direct behavior priority", !BBB_Engagement.Eligible(utility, combat, target));
				behavior.SetPriorityLevel(previousLevel);
			}
			BBB_OrderIntentTestHold pending = new BBB_OrderIntentTestHold(utility, null);
			utility.AddAction(pending);
			GuardCheck("queued player order during a combat reaction", BBB_OrderIntent.Blocks(utility) && !BBB_Engagement.Eligible(utility, combat, target));
			pending.SetRemoveAction(true);
			pending.Fail();
			GuardCheck("cancelled queued order releases the guard", !BBB_OrderIntent.Blocks(utility));

		}
		SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(group.GetCurrentWaypoint());
		if (!m_WaypointGuard && waypoint)
		{
			m_WaypointGuard = true;
			float previousPriority = waypoint.GetPriorityLevel();
			waypoint.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
			GuardCheck("forced movement waypoint", !BBB_Engagement.Eligible(utility, combat, target));
			waypoint.SetPriorityLevel(previousPriority);
		}
	}

	protected void FinishEngagement()
	{
		GetGame().GetCallqueue().Remove(TickEngagement);
		if (m_Lane) Clear(m_Lane);
		m_Lane = null;
		m_Ammo.Clear();
		m_Positions.Clear();
		m_Magazines.Clear();
		m_FirstContact.Clear();
		m_ReturnedFire.Clear();
		BBB_Engagement.s_bOn = m_SavedEngagement;
		BBB_Engagement.s_bTargetPriority = m_SavedPriority;
		BBB_Engagement.s_bTestMetrics = m_SavedMetrics;
		BBB_Perf.s_bOn = m_bOriginalPerf;
		BBB_Perf.s_bOwnReport = m_bOriginalPerfReport;
		string verdict = "PASS";
		if (m_Completed != 8 || m_Missing || m_SelectedSamples == 0 || m_Shots == 0 || m_GuardPasses != 6 || m_GuardFailures > 0) verdict = "FAIL";
		if (m_TargetComparison && (BBB_Engagement.s_iFiltered == 0 || BBB_Engagement.s_iResolved == 0 || m_TotalHandoffShots == 0)) verdict = "FAIL";
		PrintFormat("[BBB] ENGAGE guard totals passed=%1 failed=%2 expected=6", m_GuardPasses, m_GuardFailures);
		string testName = "bbbengagement";
		if (m_TargetComparison) testName = "bbbtargetpriority";
		PrintFormat("[ACR] VERDICT %1 test=%2 rounds=%3 expected=8 missing=%4 targetSamples=%5 shots=%6 (fixture coverage and order guards, not a behavior acceptance verdict)", verdict, testName, m_Completed, m_Missing, m_SelectedSamples, m_Shots);
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		BBB_EngagementTest.MaybeStartEngagement();
	}
}
