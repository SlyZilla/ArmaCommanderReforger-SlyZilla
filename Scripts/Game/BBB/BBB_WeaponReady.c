// Native formation/individual movement assumes the weapon is already in hand.
// Recover an armed military AI that is walking empty-handed, without waiting for
// incoming damage. No orders, contacts, ammunition or fire inputs are changed.
class BBB_WeaponReady
{
	static bool s_bRead;
	static bool s_bOn = true;
	static int s_iAttempts;
	static int s_iAccepted;
	static const float RETRY_MS = 2000;

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;
		string value;
		if (System.GetCLIParam("bbbWeaponReady", value) && value == "0") s_bOn = false;
	}

	static void Tick()
	{
		Read();
		if (!s_bOn) return;
		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(groups);
		foreach (SCR_AIGroup group : groups)
		{
			array<AIAgent> agents = {};
			BBB.s_Hooks.GetMembers(group, agents);
			foreach (AIAgent agent : agents) Check(agent);
		}
	}

	static void Check(AIAgent agent)
	{
		if (!agent || agent.GetLOD() != 0) return;
		ChimeraCharacter soldier = ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!soldier) return;
		CharacterControllerComponent controller = soldier.GetCharacterController();
		if (!controller || controller.IsDead() || controller.IsUnconscious() || controller.IsPlayerControlled()) return;
		if (soldier.IsInVehicle() || controller.GetCurrentItemInHands() || controller.IsChangingItem() || controller.IsGadgetInHands() || controller.IsReloading() || controller.GetDisableWeaponControls()) return;
		if (controller.IsClimbing() || controller.IsSwimming()) return;
		if (BBB.PlayerOf(soldier)) return;
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
		if (!utility || !utility.IsMilitary() || !utility.m_AIInfo || !utility.m_CombatComponent) return;
		if (utility.m_AIInfo.HasUnitState(EUnitState.IN_VEHICLE) || utility.m_AIInfo.HasUnitState(EUnitState.IN_TURRET) || utility.m_AIInfo.HasUnitState(EUnitState.PILOT)) return;
		SCR_AIBehaviorBase action = utility.GetCurrentBehavior();
		// Whitelist movement and threat observation. In particular, do not draw
		// through healing, casualty carrying, boarding, construction or gestures.
		if (!SCR_AIMoveInFormationBehavior.Cast(action) && !SCR_AIMoveIndividuallyBehavior.Cast(action) && !SCR_AIMoveAndInvestigateBehavior.Cast(action) && !SCR_AIObserveThreatSystemBehavior.Cast(action)) return;
		float now = GetGame().GetWorld().GetWorldTime();
		if (now < utility.m_fBBBWeaponReadyNext) return;
		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		if (!weapons) return;
		array<IEntity> carried = {};
		weapons.GetWeaponsList(carried);
		BaseWeaponComponent chosen;
		// Preserve a real native weapon choice (e.g. a launcher against a vehicle).
		if (utility.m_CombatComponent.GetCurrentTarget())
		{
			BaseMagazineComponent magazine;
			int muzzle;
			utility.m_CombatComponent.GetSelectedWeaponAndMagazine(chosen, muzzle, magazine);
			if (chosen && !carried.Contains(chosen.GetOwner())) chosen = null;
		}
		if (!chosen)
		{
			foreach (IEntity item : carried)
			{
				if (!item) continue;
				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
				if (!weapon) continue;
				EWeaponType kind = weapon.GetWeaponType();
				if (kind != EWeaponType.WT_RIFLE && kind != EWeaponType.WT_MACHINEGUN && kind != EWeaponType.WT_SNIPERRIFLE && kind != EWeaponType.WT_GRENADELAUNCHER) continue;
				chosen = weapon;
				break;
			}
		}
		if (!chosen) return;
		utility.m_fBBBWeaponReadyNext = now + RETRY_MS;
		s_iAttempts++;
		bool accepted = controller.TryEquipRightHandItem(chosen.GetOwner(), EEquipItemType.EEquipTypeWeapon, false);
		if (accepted) s_iAccepted++;
		BBB_Perf.Count("weaponReady.attempt");
		string weaponName = chosen.GetOwner().ClassName();
		if (chosen.GetOwner().GetPrefabData()) weaponName = FilePath.StripPath(chosen.GetOwner().GetPrefabData().GetPrefabName());
		PrintFormat("[BBB] WEAPON READY %1 equips %2 accepted=%3", BBB.s_Hooks.NameOf(SCR_AIGroup.Cast(agent.GetParentGroup())), weaponName, accepted);
	}
}

modded class SCR_AIUtilityComponent
{
	float m_fBBBWeaponReadyNext;
}
