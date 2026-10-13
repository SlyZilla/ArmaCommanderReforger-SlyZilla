// Inventory replacement during real native movement, without any enemy or firing cue.
// The final case replaces kit while the controller is processing a holster animation.
class ACR_TestUAHands : BBB_AimTest
{
	static ref ACR_TestUAHands s_Hands;
	protected float m_SwapAt;
	protected float m_CheckAt;
	protected bool m_Swapped;
	protected bool m_Checked;
	protected int m_Passed;
	protected int m_Failed;
	protected bool m_Missing;
	protected bool m_East;
	protected AIWaypoint m_HoldWaypoint;

	void ACR_TestUAHands() { m_aCheckDistances = {25, 60, 100}; }

	static void MaybeStartHands()
	{
		if (s_Hands || !Replication.IsServer()) return;
		if (!System.IsCLIParam("acrUAHandsTest") && !System.IsCLIParam("acrWeaponReadyEastTest")) return;
		s_Hands = new ACR_TestUAHands();
		GetGame().GetCallqueue().CallLater(s_Hands.BeginHands, 10000);
	}

	protected void BeginHands()
	{
		m_East = System.IsCLIParam("acrWeaponReadyEastTest");
		if (!ACR_UALoadout.Active() || !FindSite()) { m_Missing = true; FinishHands(); return; }
		LoadLane();
		for (int i = 0; i < 5; i++)
		{
			BBB_AimLane lane = new BBB_AimLane();
			lane.m_vDir = m_aSiteDir[0];
			lane.m_vFrom = m_aSiteFrom[0] + Vector(lane.m_vDir[2], 0, -lane.m_vDir[0]) * (i * 15);
			ResourceName team = TEAM_US;
			if (m_East) team = TEAM_USSR;
			lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(team, lane.m_vFrom, lane.m_vDir));
			if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
			else m_Missing = true;
			m_aLanes.Insert(lane);
		}
		m_SwapAt = Now() + 5;
		GetGame().GetCallqueue().CallLater(TickHands, 500, true);
	}

	protected void TickHands()
	{
		if (!m_Swapped && Now() >= m_SwapAt)
		{
			m_Swapped = true;
			foreach (int caseId, BBB_AimLane lane : m_aLanes)
			{
				Hook(lane.m_A, false);
				if (lane.m_A.m_aMen.Count() != 4) m_Missing = true;
				if (caseId > 0)
				{
					SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", lane.m_vFrom + lane.m_vDir * 200, lane.m_vDir));
					if (waypoint)
					{
						if (caseId == 2) waypoint.SetPriorityLevel(SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
						lane.m_aSpawned.Insert(waypoint);
						lane.m_A.m_Group.AddWaypoint(waypoint);
					}
					else m_Missing = true;
				}
			}
			// Allow movement to start before the swap rather than assigning it afterward.
			GetGame().GetCallqueue().CallLater(SwapHands, 2500);
			return;
		}
		if (m_CheckAt <= 0 || Now() < m_CheckAt) return;
		if (!m_Checked)
		{
			m_Checked = true;
			CheckHands("settled");
			m_CheckAt = Now() + 5;
			return;
		}
		CheckHands("stable");
		FinishHands();
	}

	protected void SwapHands()
	{
		foreach (int caseId, BBB_AimLane lane : m_aLanes)
		{
			foreach (IEntity man : lane.m_A.m_aMen)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(man);
				if (!character || (!m_East && !ACR_UALoadout.IsUA(man))) { m_Missing = true; continue; }
				CharacterControllerComponent controller = character.GetCharacterController();
				if (caseId == 3 && !m_East) controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedContextual, false);
				if (!m_East) ACR_UALoadout.Check(man);
				PrintFormat("[ACR] UA HANDS swapped case=%1 changing=%2 hands=%3", caseId, controller.IsChangingItem(), ACR_UALoadout.Leaf(controller.GetCurrentItemInHands()));
			}
		}
		m_CheckAt = Now() + 12;
		GetGame().GetCallqueue().CallLater(LateHolster, 7000);
	}

	protected void LateHolster()
	{
		if (m_aLanes.Count() < 5) return;
		foreach (IEntity man : m_aLanes[4].m_A.m_aMen)
		{
			CharacterControllerComponent controller = ChimeraCharacter.Cast(man).GetCharacterController();
			bool accepted = controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedContextual, false);
			PrintFormat("[ACR] UA HANDS late native holster accepted=%1 hands=%2", accepted, ACR_UALoadout.Leaf(controller.GetCurrentItemInHands()));
		}
		SCR_AIGroup group = m_aLanes[4].m_A.m_Group;
		group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
		group.GetGroupUtilityComponent().EvaluateCombatMode();
		m_HoldWaypoint = group.GetCurrentWaypoint();
	}

	protected void CheckHands(string phase)
	{
		foreach (int caseId, BBB_AimLane lane : m_aLanes)
		{
			int armed;
			int empty;
			int busy;
			int contacts;
			foreach (IEntity man : lane.m_A.m_aMen)
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(man);
				if (!character || Down(man)) { m_Missing = true; continue; }
				CharacterControllerComponent controller = character.GetCharacterController();
				IEntity held = controller.GetCurrentItemInHands();
				BaseWeaponComponent weapon;
				if (held) weapon = BaseWeaponComponent.Cast(held.FindComponent(BaseWeaponComponent));
				if (weapon) armed++;
				if (!held) empty++;
				if (controller.IsChangingItem()) busy++;
				AIControlComponent control = AIControlComponent.Cast(man.FindComponent(AIControlComponent));
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(control.GetAIAgent().FindComponent(SCR_AIUtilityComponent));
				if (utility.m_PerceptionComponent.GetTargetCount(ETargetCategory.ENEMY) > 0) contacts++;
				string action = "none";
				if (utility.GetCurrentBehavior()) action = utility.GetCurrentBehavior().Type().ToString();
				PrintFormat("[ACR] UA HANDS %1 case=%2 hands=%3 changing=%4 action=%5", phase, caseId, ACR_UALoadout.Leaf(held), controller.IsChangingItem(), action);
			}
			bool ok = armed == 4 && empty == 0 && busy == 0 && contacts == 0;
			if (caseId == 4) ok = ok && m_HoldWaypoint != null && lane.m_A.m_Group.GetCurrentWaypoint() == m_HoldWaypoint && lane.m_A.m_Group.GetGroupUtilityComponent().GetCombatModeActual() == EAIGroupCombatMode.HOLD_FIRE;
			if (ok) m_Passed++;
			else m_Failed++;
			string verdict = "FAIL";
			if (ok) verdict = "PASS";
			PrintFormat("[ACR] UA HANDS CHECK %1 %2 case=%3 equipped=%4 empty=%5 changing=%6 contacts=%7", verdict, phase, caseId, armed, empty, busy, contacts);
		}
	}

	protected void FinishHands()
	{
		GetGame().GetCallqueue().Remove(TickHands);
		GetGame().GetCallqueue().Remove(SwapHands);
		GetGame().GetCallqueue().Remove(LateHolster);
		foreach (BBB_AimLane lane : m_aLanes) Clear(lane);
		m_aLanes.Clear();
		string verdict = "PASS";
		if (m_Passed != 10 || m_Failed > 0 || m_Missing) verdict = "FAIL";
		PrintFormat("[ACR] VERDICT %1 test=uahands passed=%2 failed=%3 expected=10 missing=%4", verdict, m_Passed, m_Failed, m_Missing);
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		ACR_TestUAHands.MaybeStartHands();
	}
}
