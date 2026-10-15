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

	static vector s_vInitHeadAngles;
	static vector s_vInitHeadLocalAngles;
	static int s_iHeadBone = -1;
	static int s_iNeckBone = -1;

	static void GetBoneAngles(IEntity ent, int bone, out vector worldAngles, out vector localAngles)
	{
		worldAngles = vector.Zero;
		localAngles = vector.Zero;
		if (!ent || bone < 0) return;
		Animation anim = ent.GetAnimation();
		if (!anim) return;
		vector mat[4];
		if (anim.GetBoneMatrix(bone, mat))
			worldAngles = Math3D.MatrixToAngles(mat);
		vector locMat[4];
		if (anim.GetBoneLocalMatrix(bone, locMat))
			localAngles = Math3D.MatrixToAngles(locMat);
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
		BBB_HorizonScan.SetExemptSoldier(soldier);
		s_Utility = SCR_AIUtilityComponent.Cast(agents[0].FindComponent(SCR_AIUtilityComponent));
		s_Controller = soldier.GetCharacterController();
		s_HeadAiming = CharacterHeadAimingComponent.Cast(soldier.GetHeadAimingComponent());
		s_Perception = PerceptionComponent.Cast(soldier.FindComponent(PerceptionComponent));

		Check("US soldier components valid", s_Utility != null && s_Controller != null && s_HeadAiming != null && s_Perception != null);

		Animation anim = soldier.GetAnimation();
		if (anim)
		{
			s_iHeadBone = anim.GetBoneIndex("Head");
			if (s_iHeadBone < 0) s_iHeadBone = anim.GetBoneIndex("head");
			s_iNeckBone = anim.GetBoneIndex("Neck1");
		}

		GetBoneAngles(soldier, s_iHeadBone, s_vInitHeadAngles, s_vInitHeadLocalAngles);

		vector ypr = soldier.GetYawPitchRoll();
		vector aimRotModel = soldier.GetAimRotationModel();
		vector headAimRot = s_HeadAiming.GetAimingRotation();
		vector lookAngles = s_HeadAiming.GetLookAngles();
		vector eyePos = soldier.EyePosition();

		PrintFormat("[BBB] LOOKTEST Initial: YPR=%1 AimRotModel=%2 HeadAimRot=%3 LookAngles=%4 HeadBone=%5 HeadW=%6 HeadL=%7 NeckBone=%8 EyePos=%9",
			ypr, aimRotModel, headAimRot, lookAngles, s_iHeadBone, s_vInitHeadAngles, s_vInitHeadLocalAngles, s_iNeckBone, eyePos);

		GetGame().GetCallqueue().CallLater(Step2_TestStationaryHeadTurn, 800);
	}

	static void Step2_TestStationaryHeadTurn()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		Print("[BBB] LOOKTEST Step 2: Testing stationary head turn (forced freelook + LookX=90)");

		BBB_HorizonScan.ApplyScan(soldier, 90.0, 0.0);

		GetGame().GetCallqueue().CallLater(Step2_CheckStationaryHeadTurn, 1000);
	}

	static void Step2_CheckStationaryHeadTurn()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector ypr = soldier.GetYawPitchRoll();
		vector aimRotModel = soldier.GetAimRotationModel();
		vector headAimRot = s_HeadAiming.GetAimingRotation();
		AimingComponent weaponAim = soldier.GetWeaponAimingComponent();
		vector weaponDirWorld = vector.Zero;
		if (weaponAim) weaponDirWorld = weaponAim.GetAimingDirectionWorld();
		vector headW, headL;
		GetBoneAngles(soldier, s_iHeadBone, headW, headL);

		PrintFormat("[BBB] LOOKTEST Step 2 Result: ForcedFreeLook=%1 YPR=%2 AimRotModel=%3 HeadAimRot=%4 HeadW=%5 HeadL=%6 WeaponDirW=%7",
			s_Controller.IsFreeLookForced(), ypr, aimRotModel, headAimRot, headW, headL, weaponDirWorld);

		bool bodyStatic = Math.AbsFloat(ypr[0]) < 2.0;
		bool weaponForward = weaponDirWorld[2] > 0.9;
		bool headTurned = headAimRot[0] > 30.0 || Math.AbsFloat(headW[0] - s_vInitHeadAngles[0]) > 10.0;
		Check("Stationary ForcedFreeLook turns head without rotating body or weapon",
			bodyStatic && weaponForward && headTurned,
			string.Format("bodyYaw=%1 headYaw=%2 weaponDir=%3", ypr[0], headAimRot[0], weaponDirWorld));

		BBB_HorizonScan.ResetScan(soldier, s_Utility);

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

		// Direct head look during march with anim graph shared variables and forced freelook
		Print("[BBB] LOOKTEST Commanding HeadAiming 75 deg East with ForcedFreeLook during North march");
		BBB_HorizonScan.ApplyScan(soldier, 75.0, 0.0);

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
		vector headW, headL;
		GetBoneAngles(soldier, s_iHeadBone, headW, headL);

		PrintFormat("[BBB] LOOKTEST Marching FreeLook Result: vel=%1 speed=%2 bodyYPR=%3 aimRotModel=%4 headAimRot=%5 aimDirW=%6 weaponDirW=%7 headW=%8",
			vel, vel.Length(), ypr, aimRotModel, headAimRot, aimDirWorld, weaponDirWorld, headW);

		// Check if soldier continued moving North while looking East with head only
		bool movingNorth = (vel[2] > 0.3);
		bool bodyFacingNorth = (Math.AbsFloat(ypr[0]) < 25.0);
		bool headScanningEast = (headAimRot[0] > 20.0 || aimDirWorld[0] > 0.3 || Math.AbsFloat(headW[0] - s_vInitHeadAngles[0]) > 10.0);

		Check("Marching soldier turns head to scan flank while body marches forward",
			movingNorth && bodyFacingNorth && headScanningEast,
			string.Format("vel=%1 bodyYPR=%2 headAimRot=%3 headW=%4", vel, ypr, headAimRot, headW));

		BBB_HorizonScan.ResetScan(soldier, s_Utility);
		GetGame().GetCallqueue().CallLater(Step4_TestFlankPerception, 2000);
	}

	static void Step4_TestFlankPerception()
	{
		ChimeraCharacter soldier = ChimeraCharacter.Cast(s_SoldierUS);
		vector origin = soldier.GetOrigin();

		// Spawn enemy 60 degrees to the right (flank) at 45 meters
		vector enemyPos = origin + Vector(39, 0, 22.5);
		s_GroupUSSR = SCR_AIGroup.Cast(Spawn("{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et", enemyPos, Vector(-1, 0, 0)));

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
