// ACR_TestFobPatrol: the standing FOB patrol (Jacob, 20 Oct). Size by barracks, taken from the
// reserve, back to the reserve when the barracks go. Called by srv:structures (tick 50).
class ACR_TestFobPatrol
{
	static const int CHECKS = 4; // (one per Check below)

	static void Run(ACR_Manager manager, ACR_Base fob)
	{
		if (!manager || !fob || !fob.FobOnline())
		{
			ACR_TestRegress.Abort("structures", "FOB patrol test fixture missing");
			return;
		}
		int savedHousing = fob.m_iHousingGarrison;
		int savedGarrison = fob.m_iGarrison;
		int savedLeft = fob.m_iGarrisonLeft;
		ACR_BaseSystem.s_bNoFobPatrols = false;

		fob.m_iHousingGarrison = 0;
		int none = ACR_BaseSystem.PatrolSize(fob);
		fob.m_iHousingGarrison = 11;
		int small = ACR_BaseSystem.PatrolSize(fob);
		fob.m_iHousingGarrison = 19;
		int large = ACR_BaseSystem.PatrolSize(fob);
		ACR_TestRegress.Check("FOB patrol: none without barracks, 2 for small, 4 for large", none == 0 && small == 2 && large == 4, string.Format("%1 %2 %3", none, small, large));

		fob.m_iGarrison = 19;
		fob.m_iGarrisonLeft = 19;
		manager.m_Bases.UpdatePatrol(fob);
		int patrolled = 0;
		bool walking = false;
		if (fob.m_PatrolGroup)
		{
			patrolled = fob.m_PatrolGroup.GetAgentsCount();
			walking = fob.m_PatrolGroup.GetCurrentWaypoint() != null;
		}
		ACR_TestRegress.Check("FOB patrol: 4 soldiers leave the reserve and walk the perimeter", patrolled == 4 && fob.m_iGarrisonLeft == 15 && walking, string.Format("patrol %1 reserve %2 waypoint %3", patrolled, fob.m_iGarrisonLeft, walking));

		manager.m_Bases.UpdatePatrol(fob);
		int again = 0;
		if (fob.m_PatrolGroup) again = fob.m_PatrolGroup.GetAgentsCount();
		ACR_TestRegress.Check("FOB patrol: a full patrol takes no more from the reserve", again == 4 && fob.m_iGarrisonLeft == 15, string.Format("patrol %1 reserve %2", again, fob.m_iGarrisonLeft));

		fob.m_iHousingGarrison = 0;
		manager.m_Bases.UpdatePatrol(fob);
		ACR_TestRegress.Check("FOB patrol: stands down into the reserve when the barracks go", fob.m_PatrolGroup == null && fob.m_iGarrisonLeft == 19, string.Format("reserve %1", fob.m_iGarrisonLeft));

		ACR_BaseSystem.s_bNoFobPatrols = true;
		fob.m_iHousingGarrison = savedHousing;
		fob.m_iGarrison = savedGarrison;
		fob.m_iGarrisonLeft = savedLeft;
	}
}
