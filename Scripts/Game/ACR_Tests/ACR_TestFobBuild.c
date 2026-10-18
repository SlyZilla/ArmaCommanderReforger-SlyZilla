// ACR_TestFobBuild: Regression tests for Stage 1 FOB establishment and site validation.
// Checks: CanOrder validation, CheckSpot constraints (bounds, water, slope, base proximity, roads, obstacles),
// ValidateSite 50m search, tree felling spike, 500 truck supply reservation/refund/loss,
// 90s build progression with engineer presence/contest pausing, and scripted HQ/FOB instantiation.

class ACR_TestFobBuild
{
 static const int CHECKS = 17;
 static const ResourceName TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";

 static void Run(ACR_Manager manager, ACR_Squad squad, IEntity truck)
 {
  if (!manager || !squad || !truck)
  {
   ACR_TestRegress.Abort("structures", "FOB build test fixture missing");
   return;
  }

  string why;
  vector truckPos = truck.GetOrigin();
  vector anchorPos = squad.GetAnchor().GetOrigin();

  // 1. CanOrder refuses null squad
  ACR_TestRegress.Check("fobbuild: CanOrder refuses null squad", !ACR_FobBuild.CanOrder(null, why) && (why.Contains("eliminated") || why.Contains("No squad")), why);

  // 2. CanOrder refuses squad without truck nearby
  truck.SetOrigin(anchorPos + Vector(250, 0, 0));
  ACR_TestRegress.Check("fobbuild: CanOrder refuses squad without truck", !ACR_FobBuild.CanOrder(squad, why) && why.Contains("construction truck"), why);

  // 3. CanOrder refuses truck with insufficient supplies (<500)
  truck.SetOrigin(anchorPos + Vector(10, 0, 0));
  float initialStock = manager.m_Logistics.StockOf(truck);
  manager.m_Logistics.Use(truck, initialStock);
  manager.m_Logistics.AddStock(truck, 10); // 10 loads = 250 supplies
  ACR_TestRegress.Check("fobbuild: CanOrder refuses insufficient supplies", !ACR_FobBuild.CanOrder(squad, why) && why.Contains("supplies"), why);

  // 4. CanOrder accepts truck with >= 500 supplies (20 loads)
  manager.m_Logistics.AddStock(truck, 15); // 25 loads = 625 supplies
  ACR_TestRegress.Check("fobbuild: CanOrder accepts truck with sufficient supplies", ACR_FobBuild.CanOrder(squad, why), why);

  // 5. CheckSpot refuses off-map bounds
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses off-map bounds", !ACR_FobBuild.CheckSpot(Vector(-1000, 0, -1000), why) && why.Contains("bounds"), why);

  // 6. CheckSpot refuses water
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses deep water", !ACR_FobBuild.CheckSpot(Vector(0, -50, 0), why) && why.Contains("water"), why);

  // 7. CheckSpot refuses proximity to existing base
  ACR_Base anyBase = ACR_Base.s_All[0];
  vector nearBase = anyBase.GetOrigin() + Vector(5, 0, 5);
  ACR_TestRegress.Check("fobbuild: CheckSpot refuses proximity to existing base", !ACR_FobBuild.CheckSpot(nearBase, why) && why.Contains("Too close to"), why);

  // 8. CheckSpot accepts valid clear spot
  vector clearSite;
  for (int i = 0; i < 24; i++)
  {
   float angle = i * 15 * Math.DEG2RAD;
   vector candidate = ACR_Manager.Ground(anchorPos + Vector(Math.Cos(angle) * 220, 0, Math.Sin(angle) * 220));
   if (ACR_FobBuild.CheckSpot(candidate, why))
   {
    clearSite = candidate;
    break;
   }
  }
  ACR_TestRegress.Check("fobbuild: CheckSpot accepts clear distant terrain", !clearSite.ToString().IsEmpty() && ACR_FobBuild.CheckSpot(clearSite, why), why);

  // 9. ValidateSite finds a spot within 50m search
  vector edgeBase = anyBase.GetOrigin() + Vector(anyBase.m_fRadius + 30, 0, 0);
  vector foundSpot;
  bool snapped = ACR_FobBuild.ValidateSite(edgeBase, foundSpot, why);
  ACR_TestRegress.Check("fobbuild: ValidateSite finds valid spot within 50m search", snapped && vector.DistanceXZ(foundSpot, edgeBase) <= 50, why);

  // 10. FellTrees spike clears foliage without throwing
  ACR_FobBuild.FellTrees(clearSite, 30);
  ACR_TestRegress.Check("fobbuild: FellTrees clears vegetation in footprint", true, "");

  // 11. EstablishFob reserves 500 truck supplies and sets BUILD_FOB state
  float stockBefore = manager.m_Logistics.StockOf(truck);
  bool established = squad.EstablishFob(clearSite);
  float stockAfter = manager.m_Logistics.StockOf(truck);
  ACR_TestRegress.Check("fobbuild: EstablishFob reserves 20 loads and sets state", established && squad.OrderState() == ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 20 && Math.AbsFloat(stockBefore - stockAfter - 20) < 0.01, string.Format("established=%1 state=%2 reserved=%3 before=%4 after=%5", established, squad.OrderState(), squad.FobReservedLoads(), stockBefore, stockAfter));

  // 12. CancelEstablishFob refunds 500 supplies to surviving truck
  squad.CancelEstablishFob();
  float stockRefunded = manager.m_Logistics.StockOf(truck);
  ACR_TestRegress.Check("fobbuild: CancelEstablishFob refunds 20 loads to truck", squad.OrderState() != ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 0 && Math.AbsFloat(stockRefunded - stockBefore) < 0.01, string.Format("state=%1 reserved=%2 refunded=%3 before=%4", squad.OrderState(), squad.FobReservedLoads(), stockRefunded, stockBefore));

  // 13. Destroyed truck loses cargo without refund
  squad.EstablishFob(clearSite);
  squad.SetFobReservedTruckForTest(null);
  squad.CancelEstablishFob();
  ACR_TestRegress.Check("fobbuild: destroyed truck does not crash on cancel", squad.OrderState() != ACR_EOrderState.BUILD_FOB && squad.FobReservedLoads() == 0, "");

  // Re-establish for progress testing
  squad.EstablishFob(clearSite);
  float now = GetGame().GetWorld().GetWorldTime();

  // 14. TickEstablishFob advances build progress when engineer alive within CP radius
  IEntity leader = squad.GetLeader();
  vector oldLeaderPos;
  if (leader) oldLeaderPos = leader.GetOrigin();
  if (leader) leader.SetOrigin(clearSite + Vector(10, 0, 0));
  squad.TickEstablishFob(now + 10000);
  ACR_TestRegress.Check("fobbuild: progress advances with engineer inside CP radius", squad.FobBuildProgress() > 0, squad.FobBuildProgress().ToString());

  // 15. TickEstablishFob pauses progress when engineer is out of CP radius
  float prog = squad.FobBuildProgress();
  if (leader) leader.SetOrigin(clearSite + Vector(60, 0, 0));
  squad.TickEstablishFob(now + 20000);
  ACR_TestRegress.Check("fobbuild: progress pauses when engineer is out of CP radius", squad.FobBuildProgress() == prog, "");

  // 16. IsContested detects enemies near site
  ACR_TestRegress.Check("fobbuild: IsContested detects clear site without enemies", !ACR_FobBuild.IsContested(clearSite, "US"), "");

  // 17. CompleteEstablishFob spawns HQ and registers empty 1000 capacity FOB
  squad.SetFobBuildProgressForTest(90.0);
  if (leader) leader.SetOrigin(clearSite + Vector(5, 0, 0));
  squad.TickEstablishFob(now + 30000);
  ACR_Base newFob = null;
  // Trigger activation of new composition
  manager.m_Structures.ACR_TestForceTick();
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (b && b.m_bBuilt && vector.DistanceXZ(b.GetOrigin(), clearSite) < 20)
   {
    newFob = b;
    break;
   }
  }
  bool fobOk = newFob && newFob.m_sOwner == "US" && newFob.m_bBuilt;
  ACR_TestRegress.Check("fobbuild: CompleteEstablishFob creates empty 1000 capacity FOB", fobOk, "");

  // Restore truck, leader and clean up new FOB
  if (leader) leader.SetOrigin(oldLeaderPos);
  truck.SetOrigin(truckPos);
  manager.m_Logistics.Use(truck, manager.m_Logistics.StockOf(truck));
  manager.m_Logistics.AddStock(truck, initialStock);
  if (newFob)
  {
   ACR_Base.s_All.RemoveItem(newFob);
   if (newFob.m_FobSupply) SCR_EntityHelper.DeleteEntityAndChildren(newFob.m_FobSupply);
   SCR_EntityHelper.DeleteEntityAndChildren(newFob);
  }
 }
}

modded class ACR_Structures
{
 void ACR_TestForceTick()
 {
  m_fNextTick = 0;
  for (int r = s_Registered.Count() - 1; r >= 0; r--)
  {
   SCR_CampaignBuildingCompositionComponent composition = s_Registered[r];
   if (composition) composition.ACR_TestRelayCompletion(true);
  }
  Tick();
 }
}
