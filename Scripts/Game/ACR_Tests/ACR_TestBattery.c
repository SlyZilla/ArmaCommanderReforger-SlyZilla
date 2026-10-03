// -acrBatteryTest 1 (server-capable, ~1 min): the howitzer battery. 18 rounds in three salvos,
// each a falling shell, all landing within the battery's radius of the target.
class ACR_TestBattery
{
 static vector s_Target;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   ACR_Supports.BATTERY_DELAY_MIN = 8;
   ACR_Supports.BATTERY_DELAY_MAX = 8;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 4)
  {
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Bases.GetFaction("US").m_iRP = 500;
   s_Target = ACR_Manager.Ground(alpha.m_Home + Vector(-200, 0, 700));
   int index = h.SupportIndex(manager, ACR_ESupportType.ARTILLERY_BATTERY);
   PrintFormat("[ACR] BATTERY call (entry %1): %2", index, manager.m_Supports.Call("US", index, s_Target));
  }
  if (tick == 55)
  {
   float worst, sum;
   foreach (vector at : ACR_Supports.s_aBatteryImpacts)
   {
    float d = vector.DistanceXZ(at, s_Target);
    worst = Math.Max(worst, d);
    sum += d;
   }
   int count = ACR_Supports.s_iBatteryImpacts;
   float mean;
   if (count > 0) mean = sum / count;
   PrintFormat("[ACR] BATTERY %1 impacts, mean %2 m, farthest %3 m from the target", count, mean, worst);
   ACR_TestRegress.Check("battery: 18 rounds landed", count == 18, string.Format("%1", count));
   ACR_TestRegress.Check("battery: spread wide but inside its radius", worst <= ACR_Supports.BATTERY_RADIUS + 10 && mean > 30, string.Format("mean %1 farthest %2", mean, worst));
   Print("[ACR] TEST COMPLETE");
  }
 }
}
