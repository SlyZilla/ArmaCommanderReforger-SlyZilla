// -acrFobAITest 1 (server-capable, ~1 min): hand a FOB to the AI. No AI commander on either side
// (as when players command both). A US HQ placed 450 m out makes a FOB: in the player's hands
// the AI leaves it alone; handed to the AI it is flagged for its own side, orders supplies with
// the side's RP when the store is empty and the next upgrade in its build order once stocked; a
// captured FOB isn't the captor's; handed back, the AI leaves it alone again.
class ACR_TestFobAI
{
 static const int CHECKS = 6;
 static int s_iStep;
 static int s_iStepAt;
 static IEntity s_HQ;
 static ACR_Base s_Fob;
 static int s_iRP;
 static int s_iExpect;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("fobai", "no Alpha squad");
   return;
  }
  if (tick > 150) ACR_TestRegress.Abort("fobai", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  int waited = tick - s_iStepAt;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");

  if (s_iStep == 0)
  {
   // A headquarters away from every base.
   vector home = alpha.GetAnchor().GetOrigin();
   vector spot;
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450));
    if (!NearBase(candidate) && !BBB_Terrain.IsWaterAt(candidate)) { spot = candidate; break; }
   }
   s_HQ = ACR_Manager.SpawnPrefab(ACR_TestStructures.HQ, spot);
   PrintFormat("[ACR] FOBAI TEST headquarters at %1: %2", spot, s_HQ != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    if (base && base.m_bBuilt && !base.m_bRetired && base.m_sOwner == "US" && base.m_FobSupply) s_Fob = base;
   if (!s_Fob)
   {
    if (waited > 30) ACR_TestRegress.Abort("fobai", "the headquarters made no FOB");
    return;
   }
   if (!s_Fob.m_aFobSlots) ACR_FobSlots.InitSlots(s_Fob);
   SetStock(s_Fob, 600);
   us.m_iRP = 1000;
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (waited < 4) return;
   bool idle = !ACR_FobSlots.AnyBuilding(s_Fob) && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 600 && s_Fob.m_iSupplyDelivery == 0;
   ACR_TestRegress.Check("fobai: a new FOB is in the player's hands: the AI leaves it alone", idle && !ACR_FobAI.Handed(s_Fob) && ACR_FobAI.FeedFlag(s_Fob) == 0, string.Format("%1: building %2, stock %3, delivery %4", s_Fob.m_sName, ACR_FobSlots.AnyBuilding(s_Fob), ACR_FobSupply.Stock(s_Fob.m_FobSupply), s_Fob.m_iSupplyDelivery));
   ACR_FobAI.Set(s_Fob, true);
   ACR_TestRegress.Check("fobai: handed to the AI, flagged on its own side's panel", ACR_FobAI.Handed(s_Fob) && ACR_FobAI.FeedFlag(s_Fob) == 2 && ACR_FobAI.AnyFor("US") && !ACR_FobAI.AnyFor("USSR"), "");
   // An empty store: it orders supplies.
   SetStock(s_Fob, 0);
   s_iRP = us.m_iRP;
   manager.FobAICommanderFor("US").SetFobUpgradeEvalTimeForTest(0);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (s_Fob.m_iSupplyDelivery == 0 && waited < 5) return;
   ACR_TestRegress.Check("fobai: with an empty store it orders supplies with the side's RP", s_Fob.m_iSupplyDelivery > 0 && us.m_iRP == s_iRP - ACR_FobSupply.DELIVERY_COST, string.Format("delivery %1, RP %2 -> %3", s_Fob.m_iSupplyDelivery, s_iRP, us.m_iRP));
   // Stocked: the next upgrade in its build order.
   SetStock(s_Fob, 600);
   int tier;
   s_iExpect = manager.FobAICommanderFor("US").BestUpgradeSlotFor(s_Fob, tier);
   manager.FobAICommanderFor("US").SetFobUpgradeEvalTimeForTest(0);
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   ACR_FobSlot next = ACR_FobSlots.SlotAt(s_Fob, s_iExpect);
   bool building = next && next.m_eState == ACR_EFobSlotState.BUILDING;
   if (!building && waited < 5) return;
   int stock = ACR_FobSupply.Stock(s_Fob.m_FobSupply);
   ACR_TestRegress.Check("fobai: once stocked it orders the next upgrade in its build order", s_iExpect >= 0 && building && stock < 600, string.Format("%1 building %2, stock %3", ACR_FobSlots.SlotName(s_iExpect), building, stock));
   // Captured: not the captor's.
   s_Fob.m_sOwner = "USSR";
   bool captured = !ACR_FobAI.Handed(s_Fob) && !ACR_FobAI.AnyFor("USSR") && ACR_FobAI.FeedFlag(s_Fob) == 0;
   s_Fob.m_sOwner = "US";
   ACR_TestRegress.Check("fobai: a captured FOB isn't handed to the captor's AI", captured && ACR_FobAI.Handed(s_Fob), "");
   // Handed back: the AI leaves it alone again (no supply order for an empty store).
   ACR_FobAI.Set(s_Fob, false);
   s_Fob.m_iSupplyDelivery = 0;
   SetStock(s_Fob, 0);
   s_iRP = us.m_iRP;
   ACR_AICommander ai = manager.FobAICommanderFor("US");
   ai.SetFobUpgradeEvalTimeForTest(0);
   ai.ManageHandedFobs();
   ACR_TestRegress.Check("fobai: handed back, the AI leaves it alone again", !ACR_FobAI.Handed(s_Fob) && !ACR_FobAI.AnyFor("US") && s_Fob.m_iSupplyDelivery == 0 && us.m_iRP == s_iRP, string.Format("delivery %1, RP %2 -> %3", s_Fob.m_iSupplyDelivery, s_iRP, us.m_iRP));
   ACR_TestRegress.Complete("fobai", CHECKS);
   s_iStep = 5;
  }
 }

 protected static void SetStock(ACR_Base fob, int value)
 {
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  if (store) store.SetResourceValue(value);
 }

 protected static bool NearBase(vector p)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && vector.DistanceXZ(p, base.GetOrigin()) < base.m_fRadius + 150) return true;
  return false;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
