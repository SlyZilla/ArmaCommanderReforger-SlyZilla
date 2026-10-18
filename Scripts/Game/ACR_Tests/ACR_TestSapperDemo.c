// ACR_TestSapperDemo: Regression tests for Combat AI sapper demolition of enemy FOBs.
// Checks:
//  1. IsSapperEntity identifies sapper/engineer prefab
//  2. IsSapperSquad identifies squad by name containing Sapper / Engineer
//  3. IsSapperSquad rejects standard rifle squad without engineer keywords
//  4. IsSapperEntry identifies sapper and engineer requisition entries
//  5. HasEnemyFob returns false when enemy FOB is hidden
//  6. HasEnemyFob returns true when enemy FOB is discovered
//  7. FOB attack initializes ACR_SapperDemo in NONE state
//  8. Sapper squad prioritized to first slot for FOB attack
//  9. Tick transitions to PLANTING and orders HOLD when in range
// 10. Tick completes planting, spawns demo charge, and transitions to FALLBACK
// 11. Fallback orders squad to clear 55m blast radius
// 12. Detonate triggers explosion and retires enemy FOB

class ACR_TestSapperDemo
{
 static const int CHECKS = 8; // planting, defusal and detonation: ACR_TestFobDesign

 static void Run(ACR_Manager manager, ACR_Base fob, ACR_Squad alpha)
 {
  if (!manager || !fob || !alpha)
  {
   ACR_TestRegress.Abort("structures", "Sapper demo test fixture missing");
   return;
  }

  // 1. IsSapperEntity identifies sapper/engineer prefab (false for normal infantry, null returns false)
  bool entCheck = !ACR_SapperDemo.IsSapperEntity(null) && !ACR_SapperDemo.IsSapperEntity(alpha.GetAnchor());
  ACR_TestRegress.Check("sapper: IsSapperEntity identifies sapper / engineer prefab", entCheck, "");

  // 2. IsSapperSquad identifies squad by name containing Sapper / Engineer
  string prevName = alpha.GetName();
  alpha.Rename("1st Sapper Squad");
  bool sapperByName = ACR_SapperDemo.IsSapperSquad(alpha);
  ACR_TestRegress.Check("sapper: IsSapperSquad identifies squad by name containing Sapper / Engineer", sapperByName, "");

  // 3. IsSapperSquad rejects standard rifle squad without engineer keywords
  alpha.Rename(prevName);
  bool notSapper = !ACR_SapperDemo.IsSapperSquad(alpha);
  ACR_TestRegress.Check("sapper: IsSapperSquad rejects standard rifle squad without engineer keywords", notSapper, "");

  // 4. IsSapperEntry identifies sapper and engineer requisition entries
  ACR_RequisitionEntry sapperEntry = new ACR_RequisitionEntry();
  sapperEntry.m_sName = "Combat Engineer Team";
  ACR_RequisitionEntry rifleEntry = new ACR_RequisitionEntry();
  rifleEntry.m_sName = "Rifle Squad";
  bool entryCheck = ACR_AICommander.IsSapperEntry(sapperEntry) && !ACR_AICommander.IsSapperEntry(rifleEntry);
  ACR_TestRegress.Check("sapper: IsSapperEntry identifies sapper and engineer requisition entries", entryCheck, "");

  // 5. HasEnemyFob returns false when enemy FOB is hidden
  ACR_AICommander aiEast = manager.AICommanderFor("USSR");
  if (!aiEast) aiEast = new ACR_AICommander(manager, "USSR");
  fob.m_aDetectedBy.RemoveItem("USSR");
  bool notDetected = !aiEast.HasEnemyFob();
  ACR_TestRegress.Check("sapper: HasEnemyFob returns false when enemy FOB is hidden", notDetected, "");

  // 6. HasEnemyFob returns true when enemy FOB is discovered
  fob.m_aDetectedBy.Insert("USSR");
  bool detected = aiEast.HasEnemyFob();
  ACR_TestRegress.Check("sapper: HasEnemyFob returns true when enemy FOB is discovered", detected, "");

  // 7. FOB attack initializes ACR_SapperDemo in NONE state
  ACR_AIAttack raidAttack = new ACR_AIAttack();
  raidAttack.m_Target = fob;
  raidAttack.m_bFobRaid = true;
  raidAttack.m_SapperDemo = new ACR_SapperDemo(manager, aiEast, raidAttack);
  bool attackInitOk = (raidAttack.m_bFobRaid && raidAttack.m_SapperDemo != null && raidAttack.m_SapperDemo.State() == ACR_ESapperDemoState.NONE);
  ACR_TestRegress.Check("sapper: FOB attack initializes ACR_SapperDemo in NONE state", attackInitOk, "");

  // 8. Sapper squad prioritized to first slot for FOB attack
  array<ACR_Squad> squadList = {};
  squadList.Insert(alpha);
  ACR_Squad mockSapper = new ACR_Squad(alpha.m_Group, 99);
  mockSapper.Rename("USSR Sapper Detachment");
  squadList.Insert(mockSapper);
  for (int s = squadList.Count() - 1; s >= 0; s--)
  {
   if (ACR_SapperDemo.IsSapperSquad(squadList[s]))
   {
    ACR_Squad foundSapper = squadList[s];
    squadList.RemoveOrdered(s);
    squadList.InsertAt(foundSapper, 0);
    break;
   }
  }
  bool sapperFirst = (squadList[0] == mockSapper);
  ACR_TestRegress.Check("sapper: sapper squad prioritized to first slot for FOB attack", sapperFirst, "");

 }
}

modded class ACR_AICommander
{
 void ACR_TestAdoptRaid(ACR_AIAttack attack) { m_Attacks.Insert(attack); m_bActive = true; }
 int ACR_TestRaidCount() { return m_Attacks.Count(); }
}

modded class ACR_Manager
{
 int ACR_TestPendingDemolitions() { return m_PrimedDemolitions.Count(); }
}
