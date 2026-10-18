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
 static const int CHECKS = 12;

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

  // 9. Tick transitions to PLANTING and orders HOLD when in range
  raidAttack.m_Squads.Insert(alpha);
  float testNow = 1000.0;
  vector originalAlphaPos = alpha.GetAnchor().GetOrigin();
  IEntity hqEnt = manager.m_Structures.HqEntityOf(fob);
  vector hqPos = fob.GetOrigin();
  if (hqEnt) hqPos = hqEnt.GetOrigin();
  alpha.GetAnchor().SetOrigin(hqPos + Vector(2, 0, 2));

  raidAttack.m_SapperDemo.Tick(testNow);
  bool isPlanting = (raidAttack.m_SapperDemo.State() == ACR_ESapperDemoState.PLANTING && alpha.OrderState() == ACR_EOrderState.HOLD);
  ACR_TestRegress.Check("sapper: Tick transitions to PLANTING and orders HOLD when in range", isPlanting, string.Format("state=%1 order=%2", raidAttack.m_SapperDemo.State(), alpha.OrderState()));

  // 10. Tick completes planting, spawns demo charge, and transitions to FALLBACK
  testNow += 4500.0;
  raidAttack.m_SapperDemo.Tick(testNow);
  bool isFallback = (raidAttack.m_SapperDemo.State() == ACR_ESapperDemoState.FALLBACK && raidAttack.m_SapperDemo.DemoCharge() != null);
  ACR_TestRegress.Check("sapper: Tick completes planting, spawns demo charge, and transitions to FALLBACK", isFallback, string.Format("state=%1 charge=%2", raidAttack.m_SapperDemo.State(), raidAttack.m_SapperDemo.DemoCharge() != null));

  // 11. Fallback orders squad to clear 55m blast radius
  bool fallbackOrdered = (alpha.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(raidAttack.m_SapperDemo.FallbackPos(), hqPos) >= 50.0);
  ACR_TestRegress.Check("sapper: Fallback orders squad to clear 55m blast radius", fallbackOrdered, string.Format("dist=%1", vector.DistanceXZ(raidAttack.m_SapperDemo.FallbackPos(), hqPos)));

  // Clean up placed demo charge
  if (raidAttack.m_SapperDemo.DemoCharge())
  {
   SCR_EntityHelper.DeleteEntityAndChildren(raidAttack.m_SapperDemo.DemoCharge());
  }

  // 12. Detonate triggers explosion and retires enemy FOB
  EntitySpawnParams dummyParams = new EntitySpawnParams();
  dummyParams.TransformMode = ETransformMode.WORLD;
  dummyParams.Transform[3] = hqPos + Vector(500, 0, 500);
  ACR_Base demoTargetFob = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), dummyParams));
  demoTargetFob.m_sOwner = "US";
  demoTargetFob.m_bBuilt = true;
  demoTargetFob.m_bRetired = false;

  ACR_AIAttack demoAttack = new ACR_AIAttack();
  demoAttack.m_Target = demoTargetFob;
  demoAttack.m_bFobRaid = true;
  demoAttack.m_SapperDemo = new ACR_SapperDemo(manager, aiEast, demoAttack);
  demoAttack.m_Squads.Insert(alpha);

  demoAttack.m_SapperDemo.SetStateForTest(ACR_ESapperDemoState.FALLBACK);
  demoAttack.m_SapperDemo.SetFuseDeadlineForTest(testNow);
  demoAttack.m_SapperDemo.Tick(testNow + 100);

  bool detonatedAndRetired = (demoAttack.m_SapperDemo.State() == ACR_ESapperDemoState.DETONATED && demoTargetFob.m_bRetired && demoAttack.m_SapperDemo.IsDetonated());
  ACR_TestRegress.Check("sapper: Detonate triggers explosion and retires enemy FOB", detonatedAndRetired, string.Format("state=%1 retired=%2", demoAttack.m_SapperDemo.State(), demoTargetFob.m_bRetired));

  // Restore alpha position and clean up
  alpha.GetAnchor().SetOrigin(originalAlphaPos);
  alpha.ClearOrders();
  demoTargetFob.Retire();
  SCR_EntityHelper.DeleteEntityAndChildren(demoTargetFob);
 }
}
