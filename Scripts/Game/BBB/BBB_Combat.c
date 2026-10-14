// Combat response for vanilla groups. This stays inside each group's real awareness:
// proximity can briefly wake simulation, but never creates a target. Contact reports
// contain only the position last observed by that group, with no target entity pointer.
class BBB_CombatContact : Managed
{
	vector m_vPosition;
	float m_fReportedAt;
	string m_sFaction;
}

class BBB_Combat
{
	static ref BBB_Combat s_Combat;
	static bool s_bRead;
	static bool s_bDetail = true;
	static bool s_bRadio = true;
	static bool s_bResponse = true;
	static bool s_bManeuver;
	static int s_iWakeups;
	static int s_iReports;
	static int s_iResponses;
	static int s_iIncomingFireCues;
	static int s_iCuesWithTarget;
	static int s_iCuesWithoutTarget;
	static bool s_bTestMetrics;

	static const float NEAR_ENEMY_M = 450;
	static const float CONTACT_HOLD_S = 12;
	static const float CUE_HOLD_S = 25;
	static const float LOD_PREVENT_S = 4;
	static const float RADIO_TTL_S = 15;
	static const int RADIO_MAX_CONTACTS = 8;
	static const float RESPONSE_COOLDOWN_S = 20;
	static const float RESPONSE_DURATION_S = 6;
	static const float MIN_RESPONSE_M = 25;
	static const float MAX_RESPONSE_M = 900;
	static const float AIRBORNE_M = 12;
	static const float RADIO_CONE_DEG = 35;
	static const float RESPONSE_PRIORITY = 69.5;
	static const float BOUND_PAUSE_S = 2.5;

	protected ref map<SCR_AIGroup, float> m_ActiveUntil = new map<SCR_AIGroup, float>();
	protected ref map<SCR_AIGroup, ref array<ref BBB_CombatContact>> m_Reports = new map<SCR_AIGroup, ref array<ref BBB_CombatContact>>();

	static void Read()
	{
		if (s_bRead) return;
		s_bRead = true;
		string value;
		if (System.GetCLIParam("bbbCombatDetail", value) && value == "0") s_bDetail = false;
		if (System.GetCLIParam("bbbCombatRadio", value) && value == "0") s_bRadio = false;
		if (System.GetCLIParam("bbbCombatResponse", value) && value == "0") s_bResponse = false;
		if (System.GetCLIParam("bbbCombatManeuver", value) && value == "1") s_bManeuver = true;
		PrintFormat("[BBB] COMBAT detail=%1 radio=%2 response=%3 maneuver=%4", s_bDetail, s_bRadio, s_bResponse, s_bManeuver);
	}

	static bool ManeuverOn()
	{
		Read();
		return s_bManeuver;
	}

	void Tick()
	{
		Read();
		if (!s_bDetail && !s_bRadio) return;
		int started = System.GetTickCount();
		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(groups);
		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		if (s_bDetail) DetailTick(groups, now);
		if (s_bRadio) PruneReports(groups, now);
	}

	static void SetTestMode(bool detail, bool response)
	{
		if (!s_Combat) return;
		s_bDetail = detail;
		s_bRadio = false;
		s_bResponse = response;
		s_bTestMetrics = true;
		s_Combat.m_ActiveUntil.Clear();
	}

	protected void DetailTick(array<SCR_AIGroup> groups, float now)
	{
		// Give both sides enough active time to acquire and act on their own contacts.
		// This uses faction hostility only to wake simulation, never to nominate a target.
		for (int i = 0; i < groups.Count(); i++)
		{
			SCR_AIGroup a = groups[i];
			if (!a || !BBB.AnchorOf(a)) continue;
			if (HasOwnCombatCue(a)) KeepAwake(a, now, CUE_HOLD_S);
			Faction factionA = a.GetFaction();
			if (!factionA) continue;
			vector posA = BBB.AnchorOf(a).GetOrigin();
			for (int j = i + 1; j < groups.Count(); j++)
			{
				SCR_AIGroup b = groups[j];
				if (!b || !BBB.AnchorOf(b)) continue;
				Faction factionB = b.GetFaction();
				if (!factionB || !IsHostile(factionA, factionB)) continue;
				if (vector.DistanceXZ(posA, BBB.AnchorOf(b).GetOrigin()) > NEAR_ENEMY_M) continue;
				KeepAwake(a, now, CONTACT_HOLD_S);
				KeepAwake(b, now, CONTACT_HOLD_S);
			}
		}

		for (int index = m_ActiveUntil.Count() - 1; index >= 0; index--)
		{
			SCR_AIGroup group = m_ActiveUntil.GetKey(index);
			if (!group || !groups.Contains(group))
			{
				m_ActiveUntil.RemoveElement(index);
				continue;
			}
			float until = m_ActiveUntil.GetElement(index);
			if (until < now) continue;
			array<AIAgent> agents = {};
			BBB.s_Hooks.GetMembers(group, agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent || !agent.GetControlledEntity() || BBB.PlayerOf(agent.GetControlledEntity())) continue;
				if (agent.GetLOD() != 0) agent.SetLOD(0);
				agent.PreventMaxLOD(LOD_PREVENT_S);
				BBB_Perf.Count("combat.awakeAgent");
			}
			BBB_Perf.Count("combat.awakeGroup");
		}
	}

	protected bool HasOwnCombatCue(SCR_AIGroup group)
	{
		SCR_AIGroupUtilityComponent groupUtility = group.GetGroupUtilityComponent();
		if (groupUtility && groupUtility.GetThreatMeasure() >= 0.33) return true;
		array<AIAgent> agents = {};
		BBB.s_Hooks.GetMembers(group, agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent || !agent.GetControlledEntity()) continue;
			if (agent.GetDangerEventsCount() > 0) return true;
			// Utility/perception live on the agent, not its controlled character.
			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (utility && utility.m_CombatComponent && utility.m_CombatComponent.GetCurrentTarget()) return true;
		}
		return false;
	}

	protected void KeepAwake(SCR_AIGroup group, float now, float seconds)
	{
		if (!group) return;
		float until = now + seconds;
		float old;
		if (!m_ActiveUntil.Find(group, old) || old < until)
		{
			if (old < now) s_iWakeups++;
			m_ActiveUntil.Set(group, until);
		}
	}

	static bool IsHostile(Faction a, Faction b)
	{
		if (!a || !b || a == b) return false;
		return a.IsFactionEnemy(b) || b.IsFactionEnemy(a);
	}

	static void OnEnemyDetectedFiltered(SCR_AIGroup group, SCR_AITargetInfo target, AIAgent reporter)
	{
		Read();
		if (!s_bRadio || !s_Combat || !group || !target) return;
		if (target.m_vWorldPos == vector.Zero) return;
		s_Combat.RecordContact(group, target);
	}

	protected void RecordContact(SCR_AIGroup group, SCR_AITargetInfo target)
	{
		ref array<ref BBB_CombatContact> contacts;
		if (!m_Reports.Find(group, contacts) || !contacts)
		{
			contacts = new array<ref BBB_CombatContact>();
			m_Reports.Set(group, contacts);
		}
		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		string faction = "";
		if (target.m_Faction) faction = target.m_Faction.GetFactionKey();
		foreach (BBB_CombatContact existing : contacts)
		{
			if (existing.m_sFaction != faction || vector.DistanceXZ(existing.m_vPosition, target.m_vWorldPos) > 30) continue;
			existing.m_vPosition = target.m_vWorldPos;
			existing.m_fReportedAt = now;
			s_iReports++;
			BBB_Perf.Count("combat.radioUpdate");
			return;
		}
		if (contacts.Count() >= RADIO_MAX_CONTACTS) contacts.RemoveOrdered(0);
		BBB_CombatContact contact = new BBB_CombatContact();
		contact.m_vPosition = target.m_vWorldPos;
		contact.m_fReportedAt = now;
		contact.m_sFaction = faction;
		contacts.Insert(contact);
		s_iReports++;
		BBB_Perf.Count("combat.radioReport");
	}

	protected void PruneReports(array<SCR_AIGroup> groups, float now)
	{
		for (int index = m_Reports.Count() - 1; index >= 0; index--)
		{
			SCR_AIGroup group = m_Reports.GetKey(index);
			if (!group || !groups.Contains(group))
			{
				m_Reports.RemoveElement(index);
				continue;
			}
			ref array<ref BBB_CombatContact> contacts = m_Reports.GetElement(index);
			for (int j = contacts.Count() - 1; j >= 0; j--)
				if (now - contacts[j].m_fReportedAt > RADIO_TTL_S) contacts.RemoveOrdered(j);
			if (contacts.IsEmpty()) m_Reports.RemoveElement(index);
		}
	}

	static bool RecentContact(float ageSeconds)
	{
		return ageSeconds >= 0 && ageSeconds <= RADIO_TTL_S;
	}

	static bool ContactNearBearing(SCR_AIGroup group, vector from, vector heard, out vector position, out float age)
	{
		Read();
		if (!s_bRadio || !s_Combat || !group) return false;
		ref array<ref BBB_CombatContact> contacts;
		if (!s_Combat.m_Reports.Find(group, contacts) || !contacts) return false;
		vector direction = heard - from;
		direction[1] = 0;
		if (direction.LengthSq() <= 0.001) return false;
		direction.Normalize();
		float minDot = Math.Cos(RADIO_CONE_DEG * Math.DEG2RAD);
		float bestDot = minDot;
		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		foreach (BBB_CombatContact contact : contacts)
		{
			float since = now - contact.m_fReportedAt;
			if (!RecentContact(since)) continue;
			vector to = contact.m_vPosition - from;
			to[1] = 0;
			float distance = to.Length();
			if (distance < MIN_RESPONSE_M || distance > MAX_RESPONSE_M) continue;
			to.Normalize();
			float dot = vector.Dot(direction, to);
			if (dot <= bestDot || !OnGround(contact.m_vPosition)) continue;
			bestDot = dot;
			position = contact.m_vPosition;
			age = since;
		}
		return bestDot > minDot;
	}

	static bool ValidSector(int sectorId)
	{
		return sectorId >= 0 && sectorId < SCR_AISectorThreatFilter.SECTOR_COUNT;
	}

	static void OnThreatSector(SCR_AIUtilityComponent utility, int sectorId)
	{
		Read();
		if (!s_bResponse && !s_bTestMetrics) return;
		// Major-sector callbacks can report -1 when no sector is valid. The native
		// threat-filter lookups below require a real sector index.
		if (!ValidSector(sectorId) || !utility || !utility.m_SectorThreatFilter || !utility.m_AIInfo) return;
		if (utility.m_AIInfo.HasUnitState(EUnitState.IN_TURRET) || utility.m_AIInfo.HasUnitState(EUnitState.PILOT) || utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS)) return;
		IEntity soldier = utility.m_OwnerEntity;
		if (!soldier || BBB.PlayerOf(soldier)) return;
		SCR_AICombatComponent combat = utility.m_CombatComponent;
		if (!combat) return;
		int flags = utility.m_SectorThreatFilter.GetSectorFlags(sectorId);
		if (!(flags & SCR_EAIThreatSectorFlags.DIRECTED_AT_ME) && !(flags & SCR_EAIThreatSectorFlags.CAUSED_DAMAGE)) return;
		bool hasTarget;
		if (s_bResponse || s_bTestMetrics) hasTarget = combat.GetCurrentTarget() != null;
		if (s_bTestMetrics)
		{
			s_iIncomingFireCues++;
			if (hasTarget) s_iCuesWithTarget++;
			else s_iCuesWithoutTarget++;
		}
		if (!s_bResponse || hasTarget || combat.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) return;
		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		if (now < utility.m_fBBBContactFireReady) return;
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
		BaseWeaponComponent weapon = null;
		if (weapons) weapon = weapons.GetCurrentWeapon();
		if (!weapon || !weapon.GetCurrentMagazine() || weapon.GetCurrentMagazine().GetAmmoCount() <= 0) return;
		vector heard = utility.m_SectorThreatFilter.GetSectorPos(sectorId);
		if (!OnGround(heard)) return;
		vector from = soldier.GetOrigin();
		float distance = vector.DistanceXZ(from, heard);
		if (distance < MIN_RESPONSE_M || distance > MAX_RESPONSE_M) return;
		AIAgent agent = utility.GetAIAgent();
		SCR_AIGroup group;
		if (agent) group = SCR_AIGroup.Cast(agent.GetParentGroup());
		vector aim = BBB_Gunner.Guess(from, heard, distance);
		float age = 0;
		vector shared;
		if (group && ContactNearBearing(group, from, heard, shared, age)) aim = shared;
		aim = BBB.Ground(aim);
		float half = Math.Clamp(distance * 0.04 + age * 1.2, 4, 35);
		vector bbMin = Vector(aim[0] - half, aim[1] - 1, aim[2] - half);
		vector bbMax = Vector(aim[0] + half, aim[1] + 2.5, aim[2] + half);
		SCR_AISuppressionVolumeBox volume = new SCR_AISuppressionVolumeBox(bbMin, bbMax);
		BBB_AIContactFireBehavior response = new BBB_AIContactFireBehavior(utility, null, volume, RESPONSE_DURATION_S, 1.1);
		utility.AddAction(response);
		utility.m_fBBBContactFireReady = now + RESPONSE_DURATION_S + RESPONSE_COOLDOWN_S;
		s_iResponses++;
		BBB_Perf.Count("combat.returnFire");
		int groupId = -1;
		if (group) groupId = group.GetGroupID();
		PrintFormat("[BBB] COMBAT return fire: group %1, %2 m toward ground contact %3 (%4 s old)", groupId, Math.Round(distance), aim, Math.Round(age));
	}

	static bool OnGround(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world) return false;
		float ground = world.GetSurfaceY(position[0], position[2]);
		return Math.AbsFloat(position[1] - ground) <= AIRBORNE_M;
	}
}

class BBB_AIContactFireBehavior : SCR_AISuppressBehavior
{
	protected float m_fEndMs;

	void BBB_AIContactFireBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, SCR_AISuppressionVolumeBase suppressionVolume, float suppressionDuration, float fireRate, float priorityLevel = PRIORITY_LEVEL_NORMAL)
	{
		m_fEndMs = GetGame().GetWorld().GetWorldTime() + suppressionDuration * 1000;
		SetIsUniqueInActionQueue(true);
	}

	override float CustomEvaluate()
	{
		if (GetGame().GetWorld().GetWorldTime() > m_fEndMs) { Complete(); return 0; }
		if (!m_Utility || !m_Utility.m_CombatComponent) { Complete(); return 0; }
		if (!BBB_Combat.s_bResponse || !BBB.IsAlive(m_Utility.m_OwnerEntity) || BBB.PlayerOf(m_Utility.m_OwnerEntity)) { Complete(); return 0; }
		if (m_Utility.m_CombatComponent.GetCurrentTarget()) { Complete(); return 0; }
		if (m_Utility.m_CombatComponent.GetCombatMode() == EAIGroupCombatMode.HOLD_FIRE) { Complete(); return 0; }
		if (!m_Utility.m_AIInfo || m_Utility.m_AIInfo.HasUnitState(EUnitState.IN_TURRET) || m_Utility.m_AIInfo.HasUnitState(EUnitState.PILOT) || m_Utility.m_AIInfo.HasUnitState(EUnitState.UNCONSCIOUS)) { Complete(); return 0; }
		return BBB_Combat.RESPONSE_PRIORITY;
	}
}

modded class SCR_AIUtilityComponent
{
	float m_fBBBContactFireReady;
}

modded class SCR_AIGroupUtilityComponent
{
	override void OnEnemyDetectedFiltered(SCR_AIGroup group, SCR_AITargetInfo target, AIAgent reporter)
	{
		super.OnEnemyDetectedFiltered(group, target, reporter);
		BBB_Combat.OnEnemyDetectedFiltered(group, target, reporter);
	}
}

// Optional short bound for recognized threats: keep vanilla's attack movement and cover
// selection, but reduce the pinned soldier's dwell in selected cover. Disabled by default
// until a clean firefight comparison confirms a survival and shooting tradeoff.
modded class SCR_AICombatMoveLogic_Attack
{
	override protected float ResolveStoppedWaitTime(bool inCover, EAIThreatState threat, EWeaponType weaponType)
	{
		float wait = super.ResolveStoppedWaitTime(inCover, threat, weaponType);
		if (!BBB_Combat.ManeuverOn() || !inCover || !m_Target || threat != EAIThreatState.THREATENED) return wait;
		return Math.Min(wait, BBB_Combat.BOUND_PAUSE_S);
	}
}
