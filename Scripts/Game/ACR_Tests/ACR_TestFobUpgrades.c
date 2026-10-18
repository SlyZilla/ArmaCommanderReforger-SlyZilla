// ACR_TestFobUpgrades: Regression tests for Stage 4 AI FOB upgrade and supply management.
// Checks:
//  1. BestUpgradeSlotFor returns SLOT_RELAY (5) initially on fresh FOB
//  2. OrderFobUpgrade refuses when FOB supplies < cost
//  3. OrderFobUpgrade succeeds when FOB supplies >= cost
//  4. ManageFobUpgrades respects single-upgrade lock while building
//  5. AssistFobConstruction dispatches nearby engineer squad
//  6. After Relay built, next priority is Barracks (tier 0)
//  7. After Barracks built, next priority is Repair (tier 0)
//  8. After Repair built, next priority is Fuel
//  9. After Fuel built, next priority is Hospital
// 10. After base tiers built, next priority is Barracks tier 1 (Large)
// 11. OrderFobSupplies refuses when RP < 20 + 30 (reserve gate)
// 12. OrderFobSupplies succeeds when RP >= 50 and stock < 500
// 13. OrderFobSupplies refuses when delivery already pending
// 14. OrderFobSupplies refuses when FOB stock >= 500
// 15. Contest suppresses both supply orders and upgrade progression

class ACR_TestFobUpgrades
{
 static const int CHECKS = 15;

 static void Run(ACR_Manager manager, ACR_Base fob, ACR_Squad alpha, IEntity truck)
 {
  if (!manager || !fob || !alpha || !truck)
  {
   ACR_TestRegress.Abort("structures", "FOB Upgrades test fixture missing");
   return;
  }

  ACR_AICommander ai = manager.AICommanderFor("US");
  if (!ai) ai = new ACR_AICommander(manager, "US");

  if (!fob.m_aFobSlots) ACR_FobSlots.InitSlots(fob);

  // 1. BestUpgradeSlotFor returns SLOT_RELAY (5) initially on fresh FOB
  int targetTier = 0;
  int pick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: BestUpgradeSlotFor returns Relay initially", pick == ACR_FobSlots.SLOT_RELAY && targetTier == 0, string.Format("slot=%1 tier=%2", pick, targetTier));

  // 2. OrderFobUpgrade refuses when FOB supplies < cost
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  if (store) store.SetResourceValue(100);
  bool orderRefused = !ai.OrderFobUpgrade(fob, 100);
  ACR_TestRegress.Check("fobupgrades: OrderFobUpgrade refuses when supplies < cost", orderRefused, "");

  // 3. OrderFobUpgrade succeeds when FOB supplies >= cost
  if (store) store.SetResourceValue(300);
  bool orderOk = ai.OrderFobUpgrade(fob, 300);
  ACR_FobSlot relaySlot = fob.m_aFobSlots[ACR_FobSlots.SLOT_RELAY];
  bool buildingOk = (orderOk && relaySlot != null && relaySlot.m_eState == ACR_EFobSlotState.BUILDING && store.GetResourceValue() == 50);
  ACR_TestRegress.Check("fobupgrades: OrderFobUpgrade succeeds when supplies >= cost", buildingOk, "");

  // 4. ManageFobUpgrades respects single-upgrade lock while building
  ai.SetFobUpgradeEvalTimeForTest(0);
  ai.ManageFobUpgrades();
  int buildingCount = 0;
  foreach (ACR_FobSlot s : fob.m_aFobSlots)
  {
   if (s && s.m_eState == ACR_EFobSlotState.BUILDING) buildingCount++;
  }
  ACR_TestRegress.Check("fobupgrades: ManageFobUpgrades respects single-upgrade lock while building", buildingCount == 1, string.Format("building=%1", buildingCount));

  // 5. AssistFobConstruction dispatches nearby engineer squad
  vector truckPos = truck.GetOrigin();
  vector alphaPos = alpha.GetAnchor().GetOrigin();
  truck.SetOrigin(fob.GetOrigin() + Vector(200, 0, 0));
  alpha.GetAnchor().SetOrigin(fob.GetOrigin() + Vector(200, 0, 0));
  alpha.Halt("Holding position");
  // Any engineer or truck squad within 500 m may be the nearest one picked.
  ACR_Squad assistant = ai.AssistFobConstruction(fob);
  bool assisted = assistant && assistant.OrderState() == ACR_EOrderState.MOVE;
  ACR_TestRegress.Check("fobupgrades: AssistFobConstruction dispatches nearby engineer squad", assisted, "");
  alpha.Halt("Holding position");

  // 6. After Relay built, next priority is Barracks (tier 0)
  ACR_FobSlots.Advance(fob, 35.0);
  manager.m_Structures.ACR_TestForceTick();
  bool relayBuilt = (relaySlot.m_eState == ACR_EFobSlotState.BUILT);
  int nextPick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: after Relay built, next priority is Barracks (tier 0)", relayBuilt && nextPick == ACR_FobSlots.SLOT_BARRACKS && targetTier == 0, string.Format("built=%1 pick=%2", relayBuilt, nextPick));

  // 7. After Barracks built, next priority is Repair (tier 0)
  if (store) store.SetResourceValue(500);
  ai.OrderFobUpgrade(fob, 500);
  ACR_FobSlots.Advance(fob, 40.0);
  manager.m_Structures.ACR_TestForceTick();
  ACR_FobSlot barracksSlot = fob.m_aFobSlots[ACR_FobSlots.SLOT_BARRACKS];
  bool barracksBuilt = (barracksSlot.m_eState == ACR_EFobSlotState.BUILT && barracksSlot.m_iTier == 0);
  nextPick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: after Barracks built, next priority is Repair (tier 0)", barracksBuilt && nextPick == ACR_FobSlots.SLOT_REPAIR && targetTier == 0, string.Format("built=%1 pick=%2", barracksBuilt, nextPick));

  // 8. After Repair built, next priority is Fuel
  if (store) store.SetResourceValue(500);
  ai.OrderFobUpgrade(fob, 500);
  ACR_FobSlots.Advance(fob, 35.0);
  manager.m_Structures.ACR_TestForceTick();
  ACR_FobSlot repairSlot = fob.m_aFobSlots[ACR_FobSlots.SLOT_REPAIR];
  bool repairBuilt = (repairSlot.m_eState == ACR_EFobSlotState.BUILT && repairSlot.m_iTier == 0);
  nextPick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: after Repair built, next priority is Fuel", repairBuilt && nextPick == ACR_FobSlots.SLOT_FUEL && targetTier == 0, string.Format("built=%1 pick=%2", repairBuilt, nextPick));

  // 9. After Fuel built, next priority is Hospital
  if (store) store.SetResourceValue(500);
  ai.OrderFobUpgrade(fob, 500);
  ACR_FobSlots.Advance(fob, 35.0);
  manager.m_Structures.ACR_TestForceTick();
  ACR_FobSlot fuelSlot = fob.m_aFobSlots[ACR_FobSlots.SLOT_FUEL];
  bool fuelBuilt = (fuelSlot.m_eState == ACR_EFobSlotState.BUILT);
  nextPick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: after Fuel built, next priority is Hospital", fuelBuilt && nextPick == ACR_FobSlots.SLOT_HOSPITAL && targetTier == 0, string.Format("built=%1 pick=%2", fuelBuilt, nextPick));

  // 10. After base tiers built, next priority is Barracks tier 1 (Large)
  fob.m_aFobSlots[ACR_FobSlots.SLOT_HOSPITAL].m_eState = ACR_EFobSlotState.BUILT;
  fob.m_aFobSlots[ACR_FobSlots.SLOT_DEPOT].m_eState = ACR_EFobSlotState.BUILT;
  fob.m_aFobSlots[ACR_FobSlots.SLOT_AMMO].m_eState = ACR_EFobSlotState.BUILT;
  nextPick = ai.BestUpgradeSlotFor(fob, targetTier);
  ACR_TestRegress.Check("fobupgrades: after base tiers built, next priority is Barracks tier 1 (Large)", nextPick == ACR_FobSlots.SLOT_BARRACKS && targetTier == 1, string.Format("pick=%1 tier=%2", nextPick, targetTier));

  // 11. OrderFobSupplies refuses when RP < 20 + 30 (reserve gate)
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  int prevRP = data.m_iRP;
  data.m_iRP = 49;
  if (store) store.SetResourceValue(100);
  fob.m_iSupplyDelivery = 0;
  bool supplyRefused = !ai.OrderFobSupplies(fob, data);
  ACR_TestRegress.Check("fobupgrades: OrderFobSupplies refuses when RP < 20 + 30 (reserve gate)", supplyRefused, "");

  // 12. OrderFobSupplies succeeds when RP >= 50 and stock < 500
  data.m_iRP = 60;
  bool supplyOk = ai.OrderFobSupplies(fob, data);
  bool deliveryPending = (supplyOk && fob.m_iSupplyDelivery == 500 && data.m_iRP == 40);
  ACR_TestRegress.Check("fobupgrades: OrderFobSupplies succeeds when RP >= 50 and stock < 500", deliveryPending, string.Format("ok=%1 delivery=%2 rp=%3", supplyOk, fob.m_iSupplyDelivery, data.m_iRP));
  data.m_iRP = prevRP;

  // 13. OrderFobSupplies refuses when delivery already pending
  bool duplicateRefused = !ai.OrderFobSupplies(fob, data);
  ACR_TestRegress.Check("fobupgrades: OrderFobSupplies refuses when delivery already pending", duplicateRefused, "");
  fob.m_iSupplyDelivery = 0;

  // 14. OrderFobSupplies refuses when FOB stock >= 500
  if (store) store.SetResourceValue(600);
  bool highStockRefused = !ai.OrderFobSupplies(fob, data);
  ACR_TestRegress.Check("fobupgrades: OrderFobSupplies refuses when FOB stock >= 500", highStockRefused, "");

  // 15. Contest suppresses both supply orders and upgrade progression
  if (store) store.SetResourceValue(200);
  fob.m_bContested = true;
  bool contestSupply = !ai.OrderFobSupplies(fob, data);
  bool contestUpgrade = !ai.OrderFobUpgrade(fob, 200);
  fob.m_bContested = false;
  ACR_TestRegress.Check("fobupgrades: contest suppresses both supply orders and upgrade progression", contestSupply && contestUpgrade, "");

  // Cleanup slots and restore positions
  ACR_FobSlots.Cleanup(fob);
  truck.SetOrigin(truckPos);
  alpha.GetAnchor().SetOrigin(alphaPos);
 }
}
