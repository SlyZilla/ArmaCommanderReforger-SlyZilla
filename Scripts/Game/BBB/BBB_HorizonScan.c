// BBB_HorizonScan: AI soldiers periodically scan horizons and flanks by turning
// their head (forced freelook), preserving weapon pointing and body heading.
// Sweeping the head moves the perception cone (EyesSensor attached to "Head" bone),
// catching flank threats and maintaining realistic situational awareness.
class BBB_HorizonScan
{
	static bool s_bRead;
	static bool s_bOn = true;
	protected static ref BBB_VanillaHooks s_WorldGroups = new BBB_VanillaHooks();
	protected static ref array<SCR_AIGroup> s_aHostGroups = {};
	static IEntity s_ExemptSoldier;

	static void SetExemptSoldier(IEntity ent)
	{
		s_ExemptSoldier = ent;
	}

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;
		string value;
		if (System.GetCLIParam("bbbHorizonScan", value) && value == "0") s_bOn = false;
	}

	static bool Eligible(AIAgent agent, SCR_AIUtilityComponent utility)
	{
		if (!s_bOn || !agent || agent.GetLOD() != 0 || !utility || !utility.IsMilitary()) return false;
		ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!soldier || soldier.IsInVehicle()) return false;
		if (soldier == s_ExemptSoldier) return false;
		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious() || controller.IsPlayerControlled() || BBB.PlayerOf(soldier)) return false;
		if (controller.IsChangingItem() || controller.IsGadgetInHands() || controller.IsReloading() || controller.GetDisableWeaponControls() || controller.IsClimbing() || controller.IsSwimming()) return false;

		// In active combat or immediate threat, soldiers keep their weapon aimed forward
		if (BBB_RelaxedMarch.Danger(agent, utility)) return false;

		return true;
	}

	static void ApplyScan(ChimeraCharacter soldier, float scanYaw, float scanPitch)
	{
		if (!soldier) return;
		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (controller)
		{
			controller.SetForcedFreeLook(true);
			CharacterInputContext inCtx = controller.GetInputContext();
			if (inCtx)
			{
				inCtx.SetFreelook(true);
				inCtx.SetLookAtAngles(Vector(scanYaw * Math.DEG2RAD, scanPitch * Math.DEG2RAD, 0));
			}
		}

		CharacterHeadAimingComponent headAiming = CharacterHeadAimingComponent.Cast(soldier.GetHeadAimingComponent());
		if (headAiming)
		{
			headAiming.SetAimingRotationWanted(Vector(scanYaw, scanPitch, 0));
			headAiming.SetAimingRotation(Vector(scanYaw, scanPitch, 0));
		}

		CharacterAnimationComponent anim = CharacterAnimationComponent.Cast(soldier.FindComponent(CharacterAnimationComponent));
		if (anim)
		{
			TAnimGraphVariable varLook = anim.BindVariableBool("Look");
			TAnimGraphVariable varLookX = anim.BindVariableFloat("LookX");
			TAnimGraphVariable varLookY = anim.BindVariableFloat("LookY");
			anim.SetSharedVariableBool(varLook, true, false);
			anim.SetSharedVariableFloat(varLookX, scanYaw, false);
			anim.SetSharedVariableFloat(varLookY, scanPitch, false);
			anim.SetVariableBool(varLook, true);
			anim.SetVariableFloat(varLookX, scanYaw);
			anim.SetVariableFloat(varLookY, scanPitch);
		}
	}

	static void ResetScan(ChimeraCharacter soldier, SCR_AIUtilityComponent utility)
	{
		if (!soldier) return;
		CharacterAnimationComponent anim = CharacterAnimationComponent.Cast(soldier.FindComponent(CharacterAnimationComponent));
		if (anim)
		{
			TAnimGraphVariable varLook = anim.BindVariableBool("Look");
			TAnimGraphVariable varLookX = anim.BindVariableFloat("LookX");
			TAnimGraphVariable varLookY = anim.BindVariableFloat("LookY");
			anim.SetSharedVariableBool(varLook, false, false);
			anim.SetSharedVariableFloat(varLookX, 0.0, false);
			anim.SetSharedVariableFloat(varLookY, 0.0, false);
			anim.SetVariableBool(varLook, false);
			anim.SetVariableFloat(varLookX, 0.0);
			anim.SetVariableFloat(varLookY, 0.0);
		}

		CharacterHeadAimingComponent headAiming = CharacterHeadAimingComponent.Cast(soldier.GetHeadAimingComponent());
		if (headAiming)
		{
			headAiming.SetAimingRotationWanted(Vector(0, 0, 0));
			headAiming.SetAimingRotation(Vector(0, 0, 0));
		}

		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (controller)
		{
			CharacterInputContext inCtx = controller.GetInputContext();
			if (inCtx)
			{
				inCtx.SetLookAtAngles(Vector(0, 0, 0));
				inCtx.SetFreelook(false);
			}
			controller.SetForcedFreeLook(false);
		}

		if (utility) utility.m_bBBBScanning = false;
	}

	static void Tick()
	{
		Read();
		if (!s_bOn) return;

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

			foreach (AIAgent agent : agents)
			{
				if (!agent) continue;
				ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
				if (!soldier) continue;

				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
				if (!utility) continue;

				CharacterControllerComponent controller = soldier.GetCharacterController();
				if (!controller) continue;

				if (!Eligible(agent, utility))
				{
					if (utility.m_bBBBScanning)
						ResetScan(soldier, utility);
					continue;
				}

				if (utility.m_bBBBScanning)
				{
					// Active scan: check if dwell time has elapsed
					if (now >= utility.m_fBBBScanEndTime)
					{
						ResetScan(soldier, utility);
						utility.m_fBBBNextScanTime = now + Math.RandomFloat(3500, 7000);
					}
				}
				else
				{
					// Idle / traveling: check if time for next scan
					if (utility.m_fBBBNextScanTime <= 0)
					{
						// Initial stagger so squad members don't scan simultaneously
						utility.m_fBBBNextScanTime = now + Math.RandomFloat(1000, 5000);
						continue;
					}

					if (now >= utility.m_fBBBNextScanTime)
					{
						// Initiate head-only horizon scan
						float side = 1.0;
						if (utility.m_iBBBScanSide == 1) side = -1.0;
						utility.m_iBBBScanSide = -utility.m_iBBBScanSide;
						if (utility.m_iBBBScanSide == 0) utility.m_iBBBScanSide = 1;

						float scanYaw = side * Math.RandomFloat(35.0, 75.0);
						float scanPitch = Math.RandomFloat(-5.0, 5.0);

						ApplyScan(soldier, scanYaw, scanPitch);

						utility.m_bBBBScanning = true;
						utility.m_fBBBScanEndTime = now + Math.RandomFloat(1200, 1800);
					}
				}
			}
		}
	}

	static void Stop()
	{
		s_aHostGroups.Clear();
		s_ExemptSoldier = null;
	}
}

modded class SCR_AIUtilityComponent
{
	float m_fBBBNextScanTime;
	float m_fBBBScanEndTime;
	bool m_bBBBScanning;
	int m_iBBBScanSide;
}
