// "Mines from kit" (realistic): the mines a squad's soldiers carry, and the supplies of the
// construction truck with them. Any mine item counts (vanilla or modded): it has a pressure
// trigger; anti-personnel ones also the AP collision handler. Vanilla note: FIA sappers carry
// 5 PMN-4 each; US and USSR sappers carry demolition charges, not mines (they need a truck).
class ACAFE_Kit
{
 static bool IsMine(IEntity item, out bool antiTank)
 {
  antiTank = false;
  if (!item || !item.FindComponent(SCR_PressureTriggerComponent)) return false;
  antiTank = !item.FindComponent(SCR_AntiPersonnelMineCollisionHandlerComponent);
  return true;
 }

 static void Count(ACR_Squad squad, out int ap, out int at)
 {
  ap = 0;
  at = 0;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  array<IEntity> items = {};
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(body.FindComponent(InventoryStorageManagerComponent));
   if (!inventory) continue;
   items.Clear();
   inventory.GetItems(items);
   foreach (IEntity item : items)
   {
    bool antiTank;
    if (!IsMine(item, antiTank)) continue;
    if (antiTank) at++;
    else ap++;
   }
  }
 }

 // Takes one mine of the kind out of a soldier's kit; prefab: the armed mine to put down.
 static bool Take(ACR_Squad squad, bool antiTank, string faction, out ResourceName prefab)
 {
  prefab = "";
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  array<IEntity> items = {};
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(body.FindComponent(InventoryStorageManagerComponent));
   if (!inventory) continue;
   items.Clear();
   inventory.GetItems(items);
   foreach (IEntity item : items)
   {
    bool itemAT;
    if (!IsMine(item, itemAT) || itemAT != antiTank) continue;
    ResourceName armed = ArmedFor(item, antiTank, faction);
    if (!inventory.TryDeleteItem(item)) continue;
    prefab = armed;
    return true;
   }
  }
  return false;
 }

 // The vanilla editable (self-arming) mine matching the item; others: the side's own kind.
 static ResourceName ArmedFor(IEntity item, bool antiTank, string faction)
 {
  string name = "";
  if (item.GetPrefabData()) name = item.GetPrefabData().GetPrefabName();
  name.ToUpper();
  if (name.Contains("M14")) return ACAFE_Orders.MINE_AP_US;
  if (name.Contains("PMN")) return ACAFE_Orders.MINE_AP_USSR;
  if (name.Contains("M15")) return ACAFE_Orders.MINE_AT_US;
  if (name.Contains("TM62") || name.Contains("TM-62")) return ACAFE_Orders.MINE_AT_USSR;
  return ACAFE_Orders.MineFor(faction, antiTank);
 }

 // Supplies on the construction truck with the squad (0 without one).
 static float TruckSupplies(ACR_Squad squad, out IEntity truck)
 {
  truck = squad.ConstructionTruck();
  if (!truck) return 0;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return 0;
  if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
  return manager.m_Logistics.StockOf(truck) * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
 }
}
