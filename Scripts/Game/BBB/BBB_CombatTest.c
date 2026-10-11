// Observer-free A/B firefight for combat-detail scheduling and infantry return fire.
// Two repeats of each detail/response combination alternate faction order. The test logs
// whether real incoming-fire cues arrived with or without a recognized target; if no
// no-target cues occur, the lane does not establish return-fire behavior.
class BBB_CombatTestLane : BBB_AimLane
{
	float m_fFirstA = -1;
	float m_fFirstB = -1;
	int m_iStartAwake;
	int m_iStartCues;
	int m_iStartTargetCues;
	int m_iStartUnknownCues;
	int m_iStartResponses;
	bool m_bDetail;
	bool m_bResponse;
	bool m_bSwap;
}

class BBB_CombatTest : BBB_AimTest
{
	static ref BBB_CombatTest s_CombatTest;
	static const int ROUNDS = 8;
	static const float ROUND_S = 35;
	static const float NEAR_GROUP_CLEAR_M = 450;
	static const float VISIBLE_GROUP_CLEAR_M = 700;

	protected ref array<bool> m_aDetail = {};
	protected ref array<bool> m_aResponse = {};
	protected ref array<bool> m_aSwap = {};
	protected ref array<string> m_aCombatKeys = {};
	protected ref map<string, int> m_mRounds = new map<string, int>();
	protected ref map<string, int> m_mShotsA = new map<string, int>();
	protected ref map<string, int> m_mShotsB = new map<string, int>();
	protected ref map<string, int> m_mDownA = new map<string, int>();
	protected ref map<string, int> m_mDownB = new map<string, int>();
	protected ref map<string, float> m_mFirstA = new map<string, float>();
	protected ref map<string, int> m_mFirstACount = new map<string, int>();
	protected ref map<string, float> m_mFirstB = new map<string, float>();
	protected ref map<string, int> m_mFirstBCount = new map<string, int>();
	protected bool m_bOriginalDetail;
	protected bool m_bOriginalRadio;
	protected bool m_bOriginalResponse;
	protected int m_iResponseBase;
	protected int m_iCompletedRounds;
	protected bool m_bMissingTeams;
	protected bool m_bOriginalPerf;
	protected bool m_bOriginalPerfReport;

	void BBB_CombatTest()
	{
		// This comparison is at 100 m. The inherited aim test also demands clear
		// sight at 150/300/500 m, rejecting otherwise usable combat lanes.
		m_aCheckDistances = {100};
	}

	protected override void PinDetail(AIAgent agent)
	{
		// The aim harness pins both sides awake. Here scheduling is one of the
		// measured switches, so let the engine and BBB_Combat choose their LOD.
	}

	static void MaybeStartCombat()
	{
		if (s_CombatTest || !Replication.IsServer()) return;
		if (!System.IsCLIParam("bbbCombatTest") && !System.IsCLIParam("acrBbbCombatTest")) return;
		s_CombatTest = new BBB_CombatTest();
		GetGame().GetCallqueue().CallLater(s_CombatTest.BeginCombat, 10000);
	}

	protected void BeginCombat()
	{
		BBB_Combat.Read();
		m_bOriginalPerf = BBB_Perf.s_bOn;
		m_bOriginalPerfReport = BBB_Perf.s_bOwnReport;
		BBB_Perf.s_bOn = true;
		BBB_Perf.s_bOwnReport = true;
		m_bOriginalDetail = BBB_Combat.s_bDetail;
		m_bOriginalRadio = BBB_Combat.s_bRadio;
		m_bOriginalResponse = BBB_Combat.s_bResponse;
		m_iResponseBase = BBB_Combat.s_iResponses;
		BBB_Combat.s_iIncomingFireCues = 0;
		BBB_Combat.s_iCuesWithTarget = 0;
		BBB_Combat.s_iCuesWithoutTarget = 0;
		if (!FindIsolatedSite())
		{
			Print("[BBB] COMBATTEST no clear lane");
			Print("[ACR] VERDICT FAIL test=bbbcombat reason=no isolated lane; no combat comparison ran");
			Print("[ACR] TEST COMPLETE");
			BBB_Perf.s_bOn = m_bOriginalPerf;
			BBB_Perf.s_bOwnReport = m_bOriginalPerfReport;
			return;
		}
		LoadLane();
		// Deliberately do not insert an ObserversSystem observer here.
		m_aDetail.Insert(false); m_aSwap.Insert(false);
		m_aResponse.Insert(false);
		m_aDetail.Insert(true); m_aSwap.Insert(true);
		m_aResponse.Insert(false);
		m_aDetail.Insert(true); m_aSwap.Insert(false);
		m_aResponse.Insert(true);
		m_aDetail.Insert(false); m_aSwap.Insert(true);
		m_aResponse.Insert(true);
		m_aDetail.Insert(false); m_aSwap.Insert(false);
		m_aResponse.Insert(true);
		m_aDetail.Insert(true); m_aSwap.Insert(true);
		m_aResponse.Insert(true);
		m_aDetail.Insert(true); m_aSwap.Insert(false);
		m_aResponse.Insert(false);
		m_aDetail.Insert(false); m_aSwap.Insert(true);
		m_aResponse.Insert(false);
		m_fNextRound = Now() + 4;
		GetGame().GetCallqueue().CallLater(TickCombat, 500, true);
	}

	// Nearby ACR groups can become accidental observers. Keep them outside the
	// combat-detail proximity radius; farther groups are allowed only when terrain blocks
	// line of sight to both test positions.
	protected bool FindIsolatedSite()
	{
		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		array<vector> dirs = {Vector(0.087, 0, 0.996), Vector(0.996, 0, -0.087), Vector(-0.087, 0, -0.996), Vector(-0.996, 0, 0.087)};
		int tested;
		for (float x = mins[0] + 600; x < maxs[0] - 600; x += 100)
		{
			for (float z = mins[2] + 600; z < maxs[2] - 600; z += 100)
			{
				vector from = Vector(x, 0, z);
				foreach (vector dir : dirs)
				{
					tested++;
					if (!LaneClear(from, dir) || !ClearOfExistingGroups(from, dir)) continue;
					m_aSiteFrom.Insert(from);
					m_aSiteDir.Insert(dir);
					PrintFormat("[BBB] COMBATTEST lane at %1 heading %2 (%3 tried, no nearby or visible groups)", from, dir, tested);
					return true;
				}
			}
		}
		PrintFormat("[BBB] COMBATTEST no isolated lane in %1 tries", tested);
		return false;
	}

	protected bool ClearOfExistingGroups(vector from, vector dir)
	{
		array<SCR_AIGroup> groups = {};
		BBB.s_Hooks.GetGroups(groups);
		vector to = from + dir * 100;
		foreach (SCR_AIGroup group : groups)
		{
			IEntity anchor = BBB.AnchorOf(group);
			if (!anchor) continue;
			vector position = anchor.GetOrigin();
			float near = Math.Min(vector.DistanceXZ(position, from), vector.DistanceXZ(position, to));
			if (near < NEAR_GROUP_CLEAR_M) return false;
			if (near <= VISIBLE_GROUP_CLEAR_M && (Visible(position, from) || Visible(position, to))) return false;
		}
		return true;
	}

	protected bool StartCombatRound(int round)
	{
		if (round >= ROUNDS) return false;
		BBB_Combat.SetTestMode(m_aDetail[round], m_aResponse[round]);
		BBB_CombatTestLane lane = new BBB_CombatTestLane();
		lane.m_bDetail = m_aDetail[round];
		lane.m_bResponse = m_aResponse[round];
		lane.m_bSwap = m_aSwap[round];
		lane.m_vFrom = m_aSiteFrom[0];
		lane.m_vDir = m_aSiteDir[0];
		lane.m_vTo = lane.m_vFrom + lane.m_vDir * 100;
		ResourceName teamA = TEAM_US;
		ResourceName teamB = TEAM_USSR;
		if (lane.m_bSwap)
		{
			teamA = TEAM_USSR;
			teamB = TEAM_US;
		}
		lane.m_A.m_Group = SCR_AIGroup.Cast(Spawn(teamA, lane.m_vFrom, lane.m_vDir));
		lane.m_B.m_Group = SCR_AIGroup.Cast(Spawn(teamB, lane.m_vTo, lane.m_vDir * -1));
		if (lane.m_A.m_Group) lane.m_aSpawned.Insert(lane.m_A.m_Group);
		if (lane.m_B.m_Group) lane.m_aSpawned.Insert(lane.m_B.m_Group);
		lane.m_fStart = Now();
		lane.m_fHookAt = Now() + 3;
		lane.m_iStartAwake = BBB_Combat.s_iWakeups;
		lane.m_iStartCues = BBB_Combat.s_iIncomingFireCues;
		lane.m_iStartTargetCues = BBB_Combat.s_iCuesWithTarget;
		lane.m_iStartUnknownCues = BBB_Combat.s_iCuesWithoutTarget;
		lane.m_iStartResponses = BBB_Combat.s_iResponses;
		m_aLanes.Insert(lane);
		m_fRoundEnd = Now() + ROUND_S;
		string detail = "off";
		string response = "off";
		if (lane.m_bDetail) detail = "on";
		if (lane.m_bResponse) response = "on";
		PrintFormat("[BBB] COMBATTEST round %1/%2: detail %3, response %4, swapped %5, two rifle squads at 100 m, no observer", round + 1, ROUNDS, detail, response, lane.m_bSwap);
		return true;
	}

	protected void TickCombat()
	{
		float now = Now();
		if (m_aLanes.IsEmpty())
		{
			if (now < m_fNextRound) return;
			m_iRound++;
			if (!StartCombatRound(m_iRound)) FinishCombat();
			return;
		}
		foreach (BBB_AimLane baseLane : m_aLanes)
		{
			BBB_CombatTestLane lane = BBB_CombatTestLane.Cast(baseLane);
			if (!lane) continue;
			if (!lane.m_bHooked && now >= lane.m_fHookAt)
			{
				lane.m_bHooked = true;
				Hook(lane.m_A, false);
				Hook(lane.m_B, false);
			}
			int beforeA = lane.m_A.m_iShots;
			int beforeB = lane.m_B.m_iShots;
			lane.m_A.SampleAmmo();
			lane.m_B.SampleAmmo();
			if (lane.m_fFirstA < 0 && lane.m_A.m_iShots > beforeA) lane.m_fFirstA = now - lane.m_fStart;
			if (lane.m_fFirstB < 0 && lane.m_B.m_iShots > beforeB) lane.m_fFirstB = now - lane.m_fStart;
		}
		if (now >= m_fRoundEnd) EndCombatRound();
	}

	protected void EndCombatRound()
	{
		foreach (BBB_AimLane baseLane : m_aLanes)
		{
			BBB_CombatTestLane lane = BBB_CombatTestLane.Cast(baseLane);
			if (!lane) continue;
			string detail = "off";
			string response = "off";
			if (lane.m_bDetail) detail = "on";
			if (lane.m_bResponse) response = "on";
			string key = "detail " + detail + " response " + response;
			int downA = Downed(lane.m_A);
			int downB = Downed(lane.m_B);
			m_iCompletedRounds++;
			if (lane.m_A.m_aMen.IsEmpty() || lane.m_B.m_aMen.IsEmpty()) m_bMissingTeams = true;
			PrintFormat("[BBB] COMBATTEST %1 swapped %2: shots %3/%4, first shot %5/%6 s, down %7/%8, wake events %9", key, lane.m_bSwap, lane.m_A.m_iShots, lane.m_B.m_iShots, lane.m_fFirstA, lane.m_fFirstB, downA, downB, BBB_Combat.s_iWakeups - lane.m_iStartAwake);
			PrintFormat("[BBB] COMBATTEST cues: incoming %1, target %2, no target %3, response actions %4", BBB_Combat.s_iIncomingFireCues - lane.m_iStartCues, BBB_Combat.s_iCuesWithTarget - lane.m_iStartTargetCues, BBB_Combat.s_iCuesWithoutTarget - lane.m_iStartUnknownCues, BBB_Combat.s_iResponses - lane.m_iStartResponses);
			if (!m_aCombatKeys.Contains(key)) m_aCombatKeys.Insert(key);
			m_mRounds.Set(key, m_mRounds.Get(key) + 1);
			m_mShotsA.Set(key, m_mShotsA.Get(key) + lane.m_A.m_iShots);
			m_mShotsB.Set(key, m_mShotsB.Get(key) + lane.m_B.m_iShots);
			m_mDownA.Set(key, m_mDownA.Get(key) + downA);
			m_mDownB.Set(key, m_mDownB.Get(key) + downB);
			if (lane.m_fFirstA >= 0) { m_mFirstA.Set(key, m_mFirstA.Get(key) + lane.m_fFirstA); m_mFirstACount.Set(key, m_mFirstACount.Get(key) + 1); }
			if (lane.m_fFirstB >= 0) { m_mFirstB.Set(key, m_mFirstB.Get(key) + lane.m_fFirstB); m_mFirstBCount.Set(key, m_mFirstBCount.Get(key) + 1); }
			Clear(lane);
		}
		m_aLanes.Clear();
		m_fNextRound = Now() + 3;
	}

	protected void FinishCombat()
	{
		GetGame().GetCallqueue().Remove(TickCombat);
		BBB_Combat.s_bDetail = m_bOriginalDetail;
		BBB_Combat.s_bRadio = m_bOriginalRadio;
		BBB_Combat.s_bResponse = m_bOriginalResponse;
		BBB_Combat.s_bTestMetrics = false;
		BBB_Perf.s_bOn = m_bOriginalPerf;
		BBB_Perf.s_bOwnReport = m_bOriginalPerfReport;
		foreach (string key : m_aCombatKeys)
		{
			float firstA = -1;
			float firstB = -1;
			if (m_mFirstACount.Get(key) > 0) firstA = m_mFirstA.Get(key) / m_mFirstACount.Get(key);
			if (m_mFirstBCount.Get(key) > 0) firstB = m_mFirstB.Get(key) / m_mFirstBCount.Get(key);
			PrintFormat("[BBB] COMBATTEST TOTAL %1 rounds %2: shots %3/%4, mean first shot %5/%6 s, down %7/%8", key, m_mRounds.Get(key), m_mShotsA.Get(key), m_mShotsB.Get(key), firstA, firstB, m_mDownA.Get(key), m_mDownB.Get(key));
		}
		PrintFormat("[BBB] COMBATTEST cue totals: incoming %1 (target %2, no target %3), responses %4", BBB_Combat.s_iIncomingFireCues, BBB_Combat.s_iCuesWithTarget, BBB_Combat.s_iCuesWithoutTarget, BBB_Combat.s_iResponses - m_iResponseBase);
		Print("[BBB] COMBATTEST done");
		string verdict = "PASS";
		if (m_iCompletedRounds != ROUNDS || m_bMissingTeams || BBB_Combat.s_iIncomingFireCues == 0) verdict = "FAIL";
		PrintFormat("[ACR] VERDICT %1 test=bbbcombat rounds=%2 expected=%3 missingTeams=%4 incoming=%5", verdict, m_iCompletedRounds, ROUNDS, m_bMissingTeams, BBB_Combat.s_iIncomingFireCues);
		if (BBB_Combat.s_iCuesWithoutTarget == 0) Print("[BBB] COMBATTEST return-fire comparison inconclusive: no incoming cues without a recognized target");
		Print("[ACR] TEST COMPLETE");
	}
}

modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		BBB_CombatTest.MaybeStartCombat();
	}
}
