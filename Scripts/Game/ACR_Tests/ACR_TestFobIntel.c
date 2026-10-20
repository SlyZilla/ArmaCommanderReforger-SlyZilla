// ACR_TestFobIntel: FOBs in the intel feed and the after-action report (20 Oct).
// An enemy FOB found once (map, feed, report), own FOB events (established, HQ lost) in the
// owner's feed counters and timeline. Called by srv:structures (tick 50, after the FOB defences
// fixture has been built and has lost its HQ).
class ACR_TestFobIntel
{
	static const int CHECKS = 4; // (one per Check below)

	static void Run(ACR_Manager manager, ACR_Base fob)
	{
		if (!manager || !fob || fob.m_sOwner != "US")
		{
			ACR_TestRegress.Abort("structures", "FOB intel test fixture missing");
			return;
		}
		ACR_Intel east = manager.IntelFor("USSR");
		fob.m_aDetectedBy.RemoveItem("USSR");
		bool hidden = !fob.IsVisibleTo("USSR");
		int feedBefore = east.m_Feed.Count();
		east.FobFound(fob, "Test Squad", GetGame().GetWorld().GetWorldTime());
		bool fed = !east.m_Feed.IsEmpty() && east.m_Feed[0].Contains(fob.m_sName) && east.m_Feed[0].Contains("Test Squad");
		ACR_TestRegress.Check("FOB intel: a found enemy FOB goes on the map and into the feed", hidden && fob.IsVisibleTo("USSR") && fed, string.Format("hidden %1 feed %2", hidden, east.m_Feed.Count()));
		int feedAfter = east.m_Feed.Count();
		east.FobFound(fob, "Test Squad", GetGame().GetWorld().GetWorldTime());
		ACR_TestRegress.Check("FOB intel: an FOB already known is not reported again", east.m_Feed.Count() == feedAfter && (feedAfter > feedBefore || feedAfter == ACR_Intel.FEED_LINES), string.Format("%1 -> %2 -> %3", feedBefore, feedAfter, east.m_Feed.Count()));
		ACR_SideStats us = ACR_Report.Of("US");
		ACR_TestRegress.Check("FOB intel: the report counts FOBs built and HQs lost", us.m_iFobsBuilt >= 2 && us.m_iFobsLost >= 1 && ACR_Report.Text("US").Contains("FOBs built"), string.Format("built %1 lost %2", us.m_iFobsBuilt, us.m_iFobsLost));
		// The timeline keeps 12 lines: the defences fixture lost its HQ just before this test.
		bool hqLost, found;
		foreach (string line : us.m_aTimeline)
			if (line.Contains("HQ lost")) hqLost = true;
		foreach (string eastLine : ACR_Report.Of("USSR").m_aTimeline)
			if (eastLine.Contains("found enemy " + fob.m_sName)) found = true;
		ACR_TestRegress.Check("FOB intel: FOB events in both sides' report timelines", hqLost && found, string.Format("HQ lost %1 found %2", hqLost, found));
	}
}
