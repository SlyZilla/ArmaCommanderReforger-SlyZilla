// Diagnostic test fixture for AI head-turning, horizon scanning, and perception awareness.
class BBB_LookTest
{
	static SCR_AIGroup s_GroupUS;
	static SCR_AIGroup s_GroupUSSR;
	static IEntity s_SoldierUS;
	static IEntity s_SoldierUSSR;
	static CharacterHeadAimingComponent s_HeadAiming;
	static CharacterControllerComponent s_Controller;
	static SCR_AIUtilityComponent s_Utility;
	static PerceptionComponent s_Perception;
	static AIWaypoint s_Waypoint;

	static int s_iPass, s_iFail;

	static void Check(string name, bool ok, string detail = "")
	{
		string verdict = "FAIL";
		if (ok) { verdict = "PASS"; s_iPass++; }
		else s_iFail++;
		PrintFormat("[BBB] LOOKTEST %1 %2: %3", verdict, name, detail);
	}

	static IEntity Spawn(ResourceName prefab, vector pos, vector dir = Vector(0, 0, 1))
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		mat[0] = Vector(dir[2], 0, -dir[0]);
		mat[1] = Vector(0, 1, 0);
		mat[2] = dir;
		mat[3] = BBB.Ground(pos);
		params.Transform = mat;
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
	}

	static void Begin()
	{
		Print("[BBB] LOOKTEST starting on server...");
		vector startPos = "1346 0 2776";
		s_GroupUS = SCR_AIGroup.Cast(Spawn("{FCF7F5DC4F83955C}Prefabs/Groups/BLUFOR/Group_US_LightFireTeam.et", startPos, Vector(0, 0, 1)));
		if (!s_GroupUS)
		{
			Check("spawn US group", false);
			Finish();
			return;
		}

		// Wait for AI agents to spawn into group
		GetGame().GetCallqueue().CallLater(Step1_InspectInitial, 3000);
	}

	static void Step1_InspectInitial()
	{
		array<AIAgent> agents = {};
		s_GroupUS.GetAgents(agents);
		if (agents.IsEmpty() || !agents[0].GetControlledEntity())
		{
			Check("US agents ready", false);
			Finish();
			return;
		}

		foreach (AIAgent a : agents)
		{
			a.SetLOD(0);
			a.PreventMaxLOD(100);
		}

		ChimeraCharacter soldier = ChimeraCharacter.Cast(agents[0].GetControlledEntity());
		s_SoldierUS = soldier;
		s_Utility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
		s_Controller = soldier.GetCharacterController();
		s_HeadAiming = CharacterHeadAimingComponent.Cast(soldier.GetHeadAimingComponent());
		s_Perception = PerceptionComponent.Cast(soldier.FindComponent(PerceptionComponent));

		Check("US soldier components valid", s_Utility != null && s_Controller != null && s_HeadAiming != null && s_Perception != null);

		// Record initial state facing North
		vector ypr = soldier.GetYawPitchRoll();
		vector aimRotModel = soldier.GetAimRotationModel();
		vector headAimRot = s_HeadAiming.GetAimingRotation();
		vector eyePos = soldier.EyePosition();

		PrintFormat("[BBB] LOOKTEST Initial: YPR=%1 AimRotModel=%2 HeadAimRot=%3 EyePos=%5",
			ypr, aimRotModel, headAimRot, eyePos);

		// Test stationary LookAt 45 degrees right (North-East)
		GetGame().GetCallqueue().CallLater(Step2_TestLookAtStationary, 1000);
	}

	static void Step2_TestLookAtStationary()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();

		// Target position 45 deg right (X = +15m, Z = +15m)
		vector lookTarget = origin + Vector(15, 1.5, 15);
		PrintFormat("[BBB] LOOKTEST Step 2: LookAt target=%1 priority=100 duration=3.0", lookTarget);

		s_Utility.m_LookAction.LookAt(lookTarget, 100, 3.0);

		GetGame().GetCallqueue().CallLater(Step2_CheckLookAtStationary, 1200);
	}

	static void Step2_CheckLookAtStationary()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector ypr = soldier.GetYawPitchRoll();
		vector aimRotModel = soldier.GetAimRotationModel();
		vector headAimRot = s_HeadAiming.GetAimingRotation();
		vector eyePos = soldier.EyePosition();

		PrintFormat("[BBB] LOOKTEST Stationary LookAt Result: YPR=%1 AimRotModel=%2 HeadAimRot=%3 EyePos=%4",
			ypr, aimRotModel, headAimRot, eyePos);

		bool rotated = Math.AbsFloat(ypr[0]) > 5.0 || Math.AbsFloat(aimRotModel[0]) > 0.05;
		Check("Stationary LookAction turns soldier", rotated, string.Format("bodyYaw=%1 headYaw=%2", ypr[0], aimRotModel[0]));

		s_Utility.m_LookAction.Cancel();
		GetGame().GetCallqueue().CallLater(Step3_TestMovingLook, 1500);
	}

	static void Step3_TestMovingLook()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();

		// Give Move waypoint straight North (+Z by 150m)
		vector wpPos = origin + Vector(0, 0, 150);
		s_Waypoint = AIWaypoint.Cast(Spawn("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", wpPos));
		if (!s_Waypoint)
		{
			Check("spawn move waypoint", false);
			Finish();
			return;
		}

		s_GroupUS.AddWaypoint(s_Waypoint);
		PrintFormat("[BBB] LOOKTEST Step 3: Added Move waypoint North at %1, waiting for march...", wpPos);

		// Wait 4 seconds for soldier to enter march/walk
		GetGame().GetCallqueue().CallLater(Step3_MarchingLookToFlank, 4000);
	}

	static void Step3_MarchingLookToFlank()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();
		Physics phy = soldier.GetPhysics();
		vector vel = vector.Zero;
		if (phy) vel = phy.GetVelocity();
		vector ypr = soldier.GetYawPitchRoll();

		PrintFormat("[BBB] LOOKTEST Marching: origin=%1 vel=%2 speed=%3 ypr=%4", origin, vel, vel.Length(), ypr);

		// Command soldier to look East (+X by 50m, 90 degrees to march heading)
		vector lookFlank = origin + Vector(50, 1.5, 0);
		PrintFormat("[BBB] LOOKTEST Commanding LookAt East during North march: target=%1", lookFlank);
		s_Utility.m_LookAction.LookAt(lookFlank, 100, 3.0);

		GetGame().GetCallqueue().CallLater(Step3_CheckMarchingLook, 1200);
	}

	static void Step3_CheckMarchingLook()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();
		Physics phy = soldier.GetPhysics();
		vector vel = vector.Zero;
		if (phy) vel = phy.GetVelocity();
		vector ypr = soldier.GetYawPitchRoll();
		vector aimRotModel = soldier.GetAimRotationModel();
		vector headAimRot = s_HeadAiming.GetAimingRotation();
		vector aimDirWorld = s_HeadAiming.GetAimingDirectionWorld();
		AimingComponent weaponAim = soldier.GetWeaponAimingComponent();
		vector weaponDirWorld = vector.Zero;
		if (weaponAim) weaponDirWorld = weaponAim.GetAimingDirectionWorld();

		PrintFormat("[BBB] LOOKTEST Marching Look Result: vel=%1 speed=%2 bodyYPR=%3 aimRotModel=%4 aimDirW=%5 weaponDirW=%6",
			vel, vel.Length(), ypr, aimRotModel, aimDirWorld, weaponDirWorld);

		// Check if soldier continued moving North while looking/aiming East
		bool movingNorth = (vel[2] > 0.3); // positive Z velocity = moving North
		bool lookingEast = (aimDirWorld[0] > 0.4 || weaponDirWorld[0] > 0.4 || aimRotModel[0] > 0.2 || ypr[0] > 20.0);

		Check("Marching soldier preserves travel while scanning flank", movingNorth || vel.Length() > 0.3,
			string.Format("vel=%1 bodyYPR=%2 aimDir=%3", vel, ypr, aimDirWorld));

		s_Utility.m_LookAction.Cancel();
		GetGame().GetCallqueue().CallLater(Step4_TestFlankPerception, 2000);
	}

	static void Step4_TestFlankPerception()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();

		// Spawn enemy 60 degrees to the right (flank) at 45 meters
		vector enemyPos = origin + Vector(39, 0, 22.5);
		s_GroupUSSR = SCR_AIGroup.Cast(Spawn("{E552DABF363BA7C0}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et", enemyPos, Vector(-1, 0, 0)));

		if (!s_GroupUSSR)
		{
			Check("spawn USSR flank group", false);
			Finish();
			return;
		}

		SCR_AIGroupUtilityComponent utilUSSR = SCR_AIGroupUtilityComponent.Cast(s_GroupUSSR.FindComponent(SCR_AIGroupUtilityComponent));
		if (utilUSSR) utilUSSR.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);

		GetGame().GetCallqueue().CallLater(Step4_EvaluateFlankBeforeLook, 3000);
	}

	static void Step4_EvaluateFlankBeforeLook()
	{
		array<AIAgent> enemyAgents = {};
		s_GroupUSSR.GetAgents(enemyAgents);
		if (enemyAgents.IsEmpty() || !enemyAgents[0].GetControlledEntity())
		{
			Check("USSR agents ready", false);
			Finish();
			return;
		}

		s_SoldierUSSR = enemyAgents[0].GetControlledEntity();

		BaseTarget targetObj = s_Perception.FindTargetPerceptionObject(s_SoldierUSSR);
		int detectedCount = s_Perception.GetTargetCount(ETargetCategory.DETECTED);
		int enemyCount = s_Perception.GetTargetCount(ETargetCategory.ENEMY);

		PrintFormat("[BBB] LOOKTEST Flank Perception Before Look: detected=%1 enemy=%2 targetObj=%3",
			detectedCount, enemyCount, targetObj != null);

		// Now command US soldier to look directly at the USSR soldier position
		vector targetPos = s_SoldierUSSR.GetOrigin() + Vector(0, 1.5, 0);
		PrintFormat("[BBB] LOOKTEST Commanding LookAt flank targetPos=%1", targetPos);
		s_Utility.m_LookAction.LookAt(targetPos, 100, 4.0);

		GetGame().GetCallqueue().CallLater(Step4_EvaluateFlankAfterLook, 2500);
	}

	static void Step4_EvaluateFlankAfterLook()
	{
		BaseTarget targetObj = s_Perception.FindTargetPerceptionObject(s_SoldierUSSR);
		int detectedCount = s_Perception.GetTargetCount(ETargetCategory.DETECTED);
		int enemyCount = s_Perception.GetTargetCount(ETargetCategory.ENEMY);

		float timeSinceDetected = -1;
		float timeSinceSeen = -1;
		if (targetObj)
		{
			timeSinceDetected = targetObj.GetTimeSinceDetected();
			timeSinceSeen = targetObj.GetTimeSinceSeen();
		}

		PrintFormat("[BBB] LOOKTEST Flank Perception After Look: detected=%1 enemy=%2 targetObj=%3 timeSeen=%4 timeDetected=%5",
			detectedCount, enemyCount, targetObj != null, timeSinceSeen, timeSinceDetected);

		bool perceived = (targetObj != null || detectedCount > 0 || enemyCount > 0);
		Check("Flank target perceived after look", perceived,
			string.Format("targetObj=%1 detected=%2 enemy=%3 timeSeen=%4", targetObj != null, detectedCount, enemyCount, timeSinceSeen));

		Finish();
	}

	static void Finish()
	{
		PrintFormat("[BBB] LOOKTEST finished. Passes=%1 Fails=%2", s_iPass, s_iFail);
		string verdict = "FAIL";
		if (s_iFail == 0 && s_iPass >= 4) verdict = "PASS";
		PrintFormat("[ACR] VERDICT %1 test=bbblook passed=%2 failed=%3 expected=4", verdict, s_iPass, s_iFail);
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() && System.IsCLIParam("acrBbbLookTest"))
		{
			GetGame().GetCallqueue().CallLater(BBB_LookTest.Begin, 12000);
		}
	}
}
